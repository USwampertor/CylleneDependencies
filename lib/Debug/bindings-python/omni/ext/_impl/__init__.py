"""This module contains bindings to C++ omni::ext::IExtensions interface, core C++ part of Omniverse Kit.

All the function are in omni.ext.IExtensions class, to get it use get_extensions_interface method, which caches
acquire interface call:

    >>> import omni.ext
    >>> a = omni.ext.get_extensions_interface()
"""

__all__ = [
    "EXTENSION_EVENT_SCRIPT_CHANGED",
    "EXTENSION_EVENT_FOLDER_CHANGED",
    "EVENT_REGISTRY_REFRESH_BEGIN",
    "EVENT_REGISTRY_REFRESH_END_SUCCESS",
    "EVENT_REGISTRY_REFRESH_END_FAILURE",
    "EVENT_EXTENSION_PULL_BEGIN",
    "EVENT_EXTENSION_PULL_END_SUCCESS",
    "EVENT_EXTENSION_PULL_END_FAILURE",
    "EXTENSION_SUMMARY_FLAG_NONE",
    "EXTENSION_SUMMARY_FLAG_ANY_ENABLED",
    "EXTENSION_SUMMARY_FLAG_BUILTIN",
    "EXTENSION_SUMMARY_FLAG_INSTALLED",
    "ExtensionPathType",
    "DownloadState",
    "ExtensionManager",
    "ExtensionStateChangeType",
    "IHookHolder",
    "IExtensionManagerHooks",
    "IExtensions",
    "acquire_extensions_interface",
    "ICppExt",
    "acquire_ext_interface",
    "release_ext_interface",
    "IExt",
    "IRegistryProvider",
    "get_extensions_interface",
    "get_extension_name",
    "pack_extension",
    "unpack_extension",
    "create_link",
    "is_link",
    "destroy_link",
    "get_dangling_references",
]

from .._extensions import *
from . import _internal
from .leak_detection import get_dangling_references


def get_extensions_interface() -> IExtensions:
    """Returns cached :class:`omni.ext.IExtensions` interface"""

    if not hasattr(get_extensions_interface, "extensions"):
        get_extensions_interface.extensions = acquire_extensions_interface()
    return get_extensions_interface.extensions


def _get_enabled_extension_module_names(self):
    """Get all python modules of enabled extensions"""

    module_names = []
    for ext_info in self.get_extensions():
        if ext_info["enabled"]:
            ext_dict = self.get_extension_dict(ext_info["id"])
            python_dict = ext_dict["python"]
            if python_dict:
                # support both naming for now:
                for md in python_dict.get("modules", python_dict.get("module", {})):
                    if md.get("name") is not None:
                        module_names.append(md.get("name"))
    return module_names


def _get_extension_id_by_module(self, module: str) -> str:
    """Get enabled extension id that contains this python module."""

    # Module can be deep, like omni.foo.tests._impl. Right cut '.' until we find module that we know of.
    while module:
        ext_id = _internal.get_ext_id_from_module(module)
        if ext_id:
            return ext_id
        module = module.rsplit(".", 1)[0] if "." in module else None


def _get_extension_path_by_module(self, module: str) -> str:
    """Get enabled extension path that contains this python module."""
    ext_id = self.get_extension_id_by_module(module)
    return self.get_extension_path(ext_id) if ext_id else None


def get_extension_name(ext_id: str) -> str:
    """Convert 'omni.foo-tag-1.2.3' to 'omni.foo-tag'"""
    a, b, *_ = ext_id.split("-") + [""]
    if b and not b[0:1].isdigit():
        return f"{a}-{b}"
    return a


def pack_extension(package_id: str, ext_path: str, output_folder: str) -> str:
    """
    Pack extension into archive.

    Supports both single file extensions (kit files) and folders.

    Args:
        package_id (str): Extension package id. Resulting archive will be [package_id].zip
        ext_path (str): Path to extension folder or file.
        output_folder: Folder to output archive into.

    Returns:
        str: Path to archived extension.
    """
    import os
    import platform
    import shutil
    import tempfile

    import carb
    import carb.tokens

    from .packman import archivezip

    # if only 1 file it is a config file, just zip it renaming it into a .kit file.
    if os.path.isfile(ext_path):
        with tempfile.TemporaryDirectory() as tmp_dir:
            folder = f"{tmp_dir}/{package_id}"
            os.makedirs(folder)
            shutil.copyfile(ext_path, f"{folder}/{package_id}.kit")
            return pack_extension(package_id, folder, output_folder)

    # Otherwise zip whole folder as is.
    return archivezip.make_archive_from_folder(ext_path, f"{output_folder}/{package_id}.zip")


def _pack_extension(self, ext_id: str, output_folder: str) -> str:
    """
    Pack extension into archive.

    Supports both single file extensions (kit files) and folders.

    Args:
        ext_id (str): Local extension id.
        output_folder: Folder to output archive into.

    Returns:
        str: Path to archived extension.
    """

    ext_dict = self.get_extension_dict(ext_id)
    if not ext_dict:
        raise Exception(f"Can't find extension with id: {ext_id} to pack")

    package_id = ext_dict["package/packageId"]
    ext_path = ext_dict["path"]

    return pack_extension(package_id, ext_path, output_folder)


def unpack_extension(archive_path: str, output_folder: str, ext_id: str = None, archive_subdir: str = None):
    """
    Unpack extenson making it ready to use if output folder is in extension search paths.

    Supports both single file extensions (kit files) and folders.

    Args:
        archive_path (str): Path to archive.
        output_folder: Folder to unpack extension into.
        ext_id (str): Extension id to use to build folder name. By default acrhive filename is used.
        archive_subdir (str): Subdir in the archive to unpack, by default archive root is used.
    """

    import os
    import shutil
    import tempfile
    from pathlib import Path
    from zipfile import ZipFile

    import carb

    from .packman import archivezip

    if not ext_id:
        ext_id = Path(archive_path).stem

    # If more 1 file or the only file is a generic config: extract into separate folder
    extract_to = f"{output_folder}/{ext_id}"
    with ZipFile(archive_path) as z:
        # If zip contains only 1 kit file -> extract it direct into ext folder
        if len(z.infolist()) == 1 or os.path.splitext(z.infolist()[0].filename)[1] == ".kit":
            extract_to = f"{output_folder}"

    # Unpack archive. If subdir is specified extract first to temp folder and then copy a subfolder.
    if archive_subdir:
        with tempfile.TemporaryDirectory() as tmp_dir:
            carb.log_info(f"[omni.ext] unpack_archive: '{archive_path}' to '{tmp_dir}'")
            archivezip.extract_archive_to_folder(archive_path, tmp_dir)
            subdir = os.path.join(tmp_dir, archive_subdir)
            carb.log_info(f"[omni.ext] copy: '{subdir}' to '{extract_to}'")
            shutil.copytree(subdir, extract_to)
    else:
        carb.log_info(f"[omni.ext] unpack_archive: '{archive_path}' to '{extract_to}'")
        archivezip.extract_archive_to_folder(archive_path, extract_to)


def create_link(link_path: str, target_path: str, target_is_dir=True):
    """
    Creates a symbolic link from 'link_path' to 'target_path'

    Args:
        link_path: Absolute or relative path to link to create
        target_path: Absolute or relative path to target; if relative then it is relative to 'link_path'
        target_is_dir: This boolean is only used on Windows to specify if the target is a directory or file.
    """

    from .packman import link

    link.create_link(link_path, target_path, target_is_dir)


def is_link(path: str) -> bool:
    from .packman import link

    return link.is_link(path)


def destroy_link(link_folder_path):
    """
    Destroys an existing file system link

    Args:
        link_folder_path: Path to linked folder to destroy.
    """
    from .packman import link

    return link.destroy_link(link_folder_path)


ExtensionManager.get_enabled_extension_module_names = _get_enabled_extension_module_names
ExtensionManager.get_extension_id_by_module = _get_extension_id_by_module
ExtensionManager.get_extension_path_by_module = _get_extension_path_by_module
ExtensionManager.pack_extension = _pack_extension
