# Copyright 2020 NVIDIA CORPORATION

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
import tempfile
from typing import Dict, Iterable, Tuple

import omni.kit.app

from . import errors, link, utils

logger = logging.getLogger("packman.archive")


def get_directory_items(dir_path: str, follow_links: bool = False) -> Tuple[str, str]:
    """Returns a list of files and directories under `dir_path` recursively, as a
    tuple of absolute path and relative path.
    If `follow_links` is true then the search will traverse links to the target
    files and directories. This is useful when the unrolling links.
    """
    # Generate file list:
    file_list = []

    def func(path):
        file_list.append((path, os.path.relpath(path, dir_path)))

    utils.for_each_item_in_folder_tree(dir_path, func, follow_links=follow_links)
    return file_list


def create_manifest(
    file_list: Iterable[Tuple[str, str]], store_links: bool = False, encode_links: bool = False
) -> Dict:
    # Build list of files, directories, links and sizes:
    file_manifest = []
    link_manifest = []
    file_sizes_total = 0
    for full_path, rel_path in file_list:
        # broken file links have to be removed - this is a quick and robust way to detect them
        # and any other directory items that have mysteriously disappeared:
        if not os.path.exists(full_path):
            logger.warning(f"'{full_path}' does not exist or links to an item that does not exist. Skipping ...")
            continue
        file_size = utils.get_size_on_disk(full_path)
        if store_links:
            try:
                target = link.get_link_target(full_path)
            except errors.PackmanErrorFileNotLink:
                pass
            else:
                # Links must be relative to be portable, ensure the link is relative
                if os.path.isabs(target):
                    base = os.path.dirname(full_path)
                    target = os.path.relpath(target, base)
                # Links must point inside the archive, otherwise they cannot be captured as links
                archive_target = os.path.normpath(os.path.join(os.path.dirname(rel_path), target))
                if archive_target.startswith(".."):
                    logger.warning(
                        f"'{full_path}' cannot be stored as link because it targets '{target}', which is outside the archive. Skipping ..."
                    )
                    continue
                if encode_links:
                    target = target.encode("utf8")
                link_manifest.append((full_path, rel_path, target, file_size))
                file_sizes_total += file_size
                continue

        file_manifest.append((full_path, rel_path, file_size))
        file_sizes_total += file_size
    return {"files": file_manifest, "links": link_manifest, "total_size": file_sizes_total}


def report_compression(archive_path: str, input_size: int):
    output_size = os.path.getsize(archive_path)
    if input_size:
        compression_info = "Compressed to %.1f%% of input size" % (float(output_size * 100) / input_size)
    else:
        compression_info = f"Compressed to {output_size} bytes larger than input size - shouldn't call that compression"
    omni.kit.app.get_app().print_and_log(f"Output size: {utils.get_pretty_size(output_size)} ({compression_info})")


def raise_if_insufficient_space_for_decompression(output_path: str, uncompressed_archive_size: int):
    """Raises exception if insufficient space for decompression. `output_path` can be a file or directory path.
    `uncompressed_size` is in bytes.
    """
    # this takes care of the case when temp dir and output dir are on the same device
    utils.raise_if_less_space(output_path, uncompressed_archive_size * 2)
    # we need to check temp separately if it's on a separate device
    utils.raise_if_less_space(tempfile.gettempdir(), uncompressed_archive_size)
