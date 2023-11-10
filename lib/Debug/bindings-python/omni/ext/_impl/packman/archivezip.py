# Copyright 2019-2020 NVIDIA CORPORATION

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import logging
import os
import platform
import stat
import time
import zipfile
from typing import Iterable

from . import archive, errors, link

SYMLINK = 0xA << 28
SYMLINK_ISDIR = 0x10

logger = logging.getLogger("packman.archivezip")


# We are forced to monkey patch Python's zipfile implementation because it doesn't handle Windows
# file attributes in accordance with the PKZIP standard
def from_file_fixed_for_windows(cls, filename, arcname=None, *, strict_timestamps=True):
    """Construct an appropriate ZipInfo for a file on the filesystem.

    filename should be the path to a file or directory on the filesystem.

    arcname is the name which it will have within the archive (by default,
    this will be the same as filename, but without a drive letter and with
    leading path separators removed).
    """
    if isinstance(filename, os.PathLike):
        filename = os.fspath(filename)
    st = os.stat(filename)
    isdir = stat.S_ISDIR(st.st_mode)
    mtime = time.localtime(st.st_mtime)
    date_time = mtime[0:6]
    if not strict_timestamps and date_time[0] < 1980:
        date_time = (1980, 1, 1, 0, 0, 0)
    elif not strict_timestamps and date_time[0] > 2107:
        date_time = (2107, 12, 31, 23, 59, 59)
    # Create ZipInfo instance to store file information
    if arcname is None:
        arcname = filename
    arcname = os.path.normpath(os.path.splitdrive(arcname)[1])
    while arcname[0] in (os.sep, os.altsep):
        arcname = arcname[1:]
    if isdir:
        arcname += "/"
    zinfo = cls(arcname, date_time)
    zinfo.external_attr = st.st_file_attributes
    # zinfo.external_attr = (st.st_mode & 0xFFFF) << 16  # Unix attributes
    if isdir:
        zinfo.file_size = 0
        # zinfo.external_attr |= 0x10  # MS-DOS dirfectory flag
    else:
        zinfo.file_size = st.st_size

    return zinfo


if platform.system() == "Windows":
    zipfile.ZipInfo.from_file = classmethod(from_file_fixed_for_windows)


def make_archive_from_folder(input_folder: str, archive_path: str, store_links=False) -> str:
    """Creates zip archive at 'archive_path' from all the files and folders inside 'input_folder'.

    input_folder: Path to folder to compress
    archive_path: Path to resulting archive
    store_links: Capture symbolic links as links or convert them to copies of what they point to
    """
    if not os.path.exists(input_folder):
        raise errors.PackmanError(f"Input folder {input_folder} does not exist!")
    # Generate file list:
    file_list = archive.get_directory_items(input_folder, follow_links=not store_links)
    return make_archive_from_file_list(archive_path, file_list, store_links=store_links)


def make_archive_from_file_list(package_path, iterable_file_list, store_links=False):
    """Create a zip file from all the files in 'iterable_file_list' according to the relative paths in
    iterable 'file_list', storing symbolic links or converting to regular files/folders if requested.

    Uses the "zipfile" Python module with some modifications on top to preserve links (if requested)
    and capture other basic file attributes. Returns the name of the output zip file.
    """
    # Build list of files, directories, links and sizes:
    manifest = archive.create_manifest(iterable_file_list, store_links, encode_links=True)
    file_manifest = manifest["files"]
    link_manifest = manifest["links"]
    total_size = manifest["total_size"]

    zip_filename = package_path
    archive_dir = os.path.dirname(package_path)

    if archive_dir and not os.path.exists(archive_dir):
        logger.info(f"Creating '{archive_dir}'...")
        os.makedirs(archive_dir, exist_ok=True)

    logger.info("Creating file '%s'" % zip_filename)

    # allow 64-bit so we can have huge zips
    zip = zipfile.ZipFile(zip_filename, "w", compression=zipfile.ZIP_DEFLATED, allowZip64=True)

    create_system = 0 if platform.system() == "Windows" else 3
    for full_path, rel_path, size in file_manifest:
        zip.write(full_path, rel_path)
    for full_path, rel_path, target, size in link_manifest:
        # Links need special treatment when stored:
        zipinfo = zipfile.ZipInfo(rel_path)
        zipinfo.create_system = create_system
        zipinfo.external_attr = SYMLINK | (0o777 << 16)
        zipinfo.compress_type = zipfile.ZIP_STORED
        zip.writestr(zipinfo, target)
    zip.close()
    archive.report_compression(zip_filename, total_size)
    return zip_filename


def _extract_link(zip: zipfile.ZipFile, file_info: zipfile.ZipInfo, extract_dir: str):
    link_path = os.path.join(extract_dir, file_info.filename)
    target_path = zip.read(file_info.filename)
    try:
        target_path = target_path.decode("utf8")
    except UnicodeDecodeError:
        raise errors.PackmanError(f"Archive contains link '{file_info.filename}' with invalid encoding (must be utf8)")
    else:
        full_path = os.path.join(os.path.dirname(link_path), target_path)
        if not os.path.exists(full_path):
            return False  # target doesn't exist so cannot deduce its type
        is_dir = os.path.isdir(full_path)
        try:
            link.create_link(link_path, target_path, is_dir)
        except RuntimeError as exc:
            raise errors.PackmanError(
                f"Unable to extract link '{file_info.filename}' due to file system restrictions ({str(exc)})"
            )
    return True


def _process_links(zip: zipfile.ZipInfo, links: Iterable[zipfile.ZipInfo], extract_dir: str):
    left_overs = []
    for zip_info in links:
        done = _extract_link(zip, zip_info, extract_dir)
        if not done:
            left_overs.append(zip_info)
    if not left_overs:
        return
    if len(left_overs) == len(links):
        raise errors.PackmanError("One ore more links in archive could not be resolved - archive is invalid.")
    else:
        _process_links(zip, left_overs, extract_dir)


def _extract_file(zip: zipfile.ZipFile, file_info: zipfile.ZipInfo, extract_dir: str):
    """
    Extracts the file in 'file_info' from zip 'zip' to directory 'extract_dir'. Restore
    permissions to the extent that the source captured it and the target OS can support.
    """
    attr = file_info.external_attr
    zip.extract(file_info, path=extract_dir)

    out_path = os.path.join(extract_dir, file_info.filename)
    if file_info.create_system == 0:
        # Zip created on Windows, MS-DOS file attributes are stored in the attribute:
        if attr & 0b00010000:  # item is a directory
            return  # Windows uses the read-only bit on directories for some Explorer hax, so ignore directories
        if attr & 0b1:  # item is read-only
            # This is a read-only file:
            os.chmod(out_path, stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH)
    elif file_info.create_system == 3:
        # Zip created on Linux/Unix/Darwin - the permission bits are stored in the two upper bytes of the attribute:
        perm = (attr >> 16) & 0o777
        if platform.system() == "Windows":
            # if no write bits are set we make the item read-only:
            if (stat.S_IWUSR | stat.S_IWGRP | stat.S_IWOTH) & perm == 0:
                os.chmod(out_path, stat.S_IREAD)  # we are on Windows so we needn't bother with other bits
        elif perm:  # the permission info is sometimes missing from funky zip files so we must check perm
            # We assume that we are on a nix-like platform in all other cases
            os.chmod(out_path, perm)


def extract_archive_to_folder(archive_path, output_folder):
    """
    Extracts the zip archive at 'archive_path' to folder 'output_folder'.
    """
    with zipfile.ZipFile(archive_path, allowZip64=True) as zip_file:
        # display an unzip progress bar for the user
        uncompress_size = sum(file.file_size for file in zip_file.infolist())
        archive.raise_if_insufficient_space_for_decompression(archive_path, uncompress_size)
        links = []
        for file_info in zip_file.infolist():
            if (file_info.external_attr & SYMLINK) == SYMLINK:
                links.append(file_info)
            else:
                _extract_file(zip_file, file_info, output_folder)
        _process_links(zip_file, links, output_folder)
