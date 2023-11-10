"""This module contains internal helpers for python part of extension manager: omni.ext. It is meant to be used by
extension manager itself only."""

import inspect
import os
import platform
import pprint
import sys
import traceback
import weakref
from collections import defaultdict, namedtuple
from contextlib import suppress
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path
from typing import Dict, List, Tuple, Type

import carb
import carb.profiler

from .._extensions import IExt
from . import fast_importer
from .custom_importer import enable_profile_import_time, import_module, import_module_private, reload_module
from .ext_settings import (
    _get_fast_importer_settings,
    _is_full_leak_check_enabled,
    _is_profile_import_time_enabled,
    _is_reentrant_test_mode,
    _is_startup_disabled,
)
from .leak_detection import find_leaks_in_module, get_dangling_references
from .stat_cache import refresh_stat_cache

_extensions = {}
_module_to_ext_id = {}

# Special hack for omni.ui.scene which imports module from omni.ui folder. TODO: find more generic solution.
EXCLUDE_FROM_NAME_CLASH_CHECK = {"omni.ui.scene"}


@lru_cache()
def is_windows():
    return platform.system().lower() == "windows"


def _is_in_folder(path, parent):
    return os.path.normcase(path).startswith(os.path.normcase(parent))


def _is_subpath(path, parent):
    # This function is the same as `_is_in_folder` above, but doesn't need abs path and normalization.
    return Path(parent) in Path(path).parents


def _is_module_in_folder(module: str, parent: str):
    mod = sys.modules.get(module, None)
    if mod and hasattr(mod, "__file__") and mod.__file__:
        return _is_subpath(mod.__file__, parent)
    return False


def find_modules_in_folder(folder: str):
    modules = []

    for module_name in sys.modules:
        mod = sys.modules[module_name]
        if hasattr(mod, "__file__") and mod.__file__:
            module_folder = os.path.dirname(os.path.abspath(mod.__file__))
            if _is_subpath(module_folder, folder):
                modules.append(module_name)
    return modules


@carb.profiler.profile()
def _import_public(folder, module_name: str, reload_enabled: bool = True):
    def _check_name_clash(module):
        if not module:
            return False
        module_file = getattr(module, "__file__", None)
        if not module_file:
            return False

        module_file = os.path.abspath(module_file)
        if not _is_in_folder(module_file, folder):
            carb.log_warn(
                f"There is already a module '{module.__name__}' in '{module_file}'. Can't import another module with the same name in public space from '{folder}'."
            )
            return True
        return False

    # Import module or if it is already imported -> reload it with all submodules
    if module_name not in sys.modules:
        module = import_module(module_name)
    else:
        if reload_enabled:
            module = reload_module(module_name)
        else:
            module = sys.modules[module_name]

    # Ignore result of that function. It gives false positive on linux, because of folder linking and the way paths is resolved,
    # but having at least a warning can be helpful.
    if is_windows() and module_name not in EXCLUDE_FROM_NAME_CLASH_CHECK:
        _check_name_clash(module)

    return module


def _multi_startup_check(ext_obj, path, module_name):
    # Check that obj was actually declared in a module that is in the path of this extension.
    # Extensions can import obj from other extensions. We don't want to startup twice.
    # Ideally we want to not to startup in that case, but that seems to be problematic to figure out on linux.
    # On linux extension path is canonical and all symlinks are resolved, but module __file__ sometimes is not (I don't know why).
    # If we try to resolve module __file__ path it may resolve up to "source/extensions/" folder, which makes it impossible to compare with extension path.
    # We need to make linux and windows behave the same here first. So for now just produce a warning on windows to help developers:
    if not is_windows():
        return
    mod = sys.modules.get(ext_obj.__module__, None)
    if mod and hasattr(mod, "__file__"):
        mod_file = mod.__file__
        if mod_file and not _is_subpath(mod_file, path):
            carb.log_warn(
                f"Found class `{ext_obj.__name__}` derived from `omni.ext.IExt` in module: '{module_name}', but it was imported from other extension: '{mod_file}', which is not a subpath to: '{path}'. Do not import `IExt` derived class from other extension in your public namespace."
            )


@dataclass
class ExtensionModule:
    path: str
    name: str
    public: bool
    search_ext: bool


class ExtensionModules:
    def __init__(self, ext_id: str):
        self._ext_id = ext_id
        self._started_extensions = []
        self._modules_to_start = []
        self._sys_path_added_folders = set()
        # At first start reload is disabled to avoid accidentally reloading modules we just imported. Later, after first
        # shutdown of this extension - enable reload.
        self._reload_enabled = False

        # Either add to sys.path or delegate to importing "fast importer"
        self._use_fast_importer = _get_fast_importer_settings().enabled

    @carb.profiler.profile()
    def _startup_ext(self, ext_class: Type[IExt], module_name: str, path: str):
        # NOP in startup-disabled mode
        if _is_startup_disabled():
            return

        carb.log_info(
            f"Found class `{ext_class.__name__}` derived from `omni.ext.IExt` in module: '{module_name}' in '{path}'. Calling `on_startup`."
        )

        instance = ext_class()

        if hasattr(instance, "on_startup"):
            # Backward compatible way to pass ext_id only if function expects it:
            sig = inspect.signature(instance.on_startup)
            if len(sig.parameters) > 0:
                instance.on_startup(self._ext_id)
            else:
                instance.on_startup()

        self._started_extensions.append((instance, module_name))

    def _add_sys_path(self, path, module_name):
        is_first = False

        if self._use_fast_importer:
            is_first = fast_importer.add_sys_path(path, module_name)
        else:
            if path not in sys.path:
                carb.log_info(f"Adding {path} to sys.path")
                sys.path.append(path)
                is_first = True

        if is_first:
            self._sys_path_added_folders.add(path)

    def _custom_importer(self, ext_module: ExtensionModule, reload_enabled: bool = True):
        try:
            if ext_module.public:
                return _import_public(ext_module.path, ext_module.name, reload_enabled)
            else:
                return import_module_private(ext_module.path, ext_module.name)
        except Exception as e:
            exc = traceback.format_exc()
            carb.log_error(
                f"Failed to import python module {ext_module.name} from {ext_module.path}. Error: {e}. Traceback:\n{exc}"
            )

    def prestartup_module(self, ext_module: ExtensionModule):
        global _module_to_ext_id

        ext_module.path = os.path.abspath(ext_module.path)

        if ext_module.public:
            self._add_sys_path(ext_module.path, ext_module.name)

        # Do not try to import or search for extensions if module name is empty
        if ext_module.name:
            _module_to_ext_id[ext_module.name] = self._ext_id
            self._modules_to_start.append(ext_module)

    def startup(self):
        for ext_module in self._modules_to_start:
            module = self._custom_importer(ext_module, self._reload_enabled)
            if not module:
                raise Exception(f"Extension python module: '{ext_module.name}' in '{ext_module.path}' failed to load.")
                return
            if ext_module.search_ext:
                carb.log_info(
                    f"Searching for classes derived from 'omni.ext.IExt' in '{ext_module.name}' ('{ext_module.path}')"
                )
                for name in dir(module):
                    obj = getattr(module, name)
                    if isinstance(obj, type) and issubclass(obj, IExt):
                        if ext_module.public:
                            _multi_startup_check(obj, ext_module.path, ext_module.name)
                        self._startup_ext(obj, ext_module.name, ext_module.path)

    def shutdown(self):
        self._reload_enabled = True
        self._modules_to_start = []

        for i in range(len(self._started_extensions) - 1, -1, -1):
            ext_object, module_name = self._started_extensions[i]
            if hasattr(ext_object, "on_shutdown"):
                ext_object.on_shutdown()

            # Create weak ref to extension to check that it was actually GCed
            ext_weak = weakref.ref(ext_object)

            # Explicitly clear extension properties, that helps to release C++ objects
            ext_object.__dict__.clear()

            # Remove last references to extension object which should trigger deletion of it
            del self._started_extensions[i]
            ext_object = None

            # Check if extension was actually destroyed
            if ext_weak() is not None:
                references = get_dangling_references(ext_weak())
                extension_info = f"{self._ext_id} -> {ext_weak().__class__}"
                carb.log_warn(
                    f"{extension_info}: extension object is still alive, something holds a reference on it. References: {references}"
                )

            # Additional leak search, goes over all alive objects and looks for the ones from this module:
            if _is_full_leak_check_enabled():
                find_leaks_in_module(module_name)

        # Cleanup python import system as much as we can:
        for f in self._sys_path_added_folders:
            global _module_to_ext_id

            modules_to_reload = []
            try:
                # Find all modules in that folder and remove from sys.modules. Enabling extension after (reloading) will import
                # them again.
                modules_to_reload = find_modules_in_folder(f)
            except RuntimeError as exc:
                carb.log_verbose(f"Failed to list modules due to sys.modules change during execution: {exc}")

            for name in modules_to_reload:
                del sys.modules[name]
                # Also remove from our module -> ext_id map if exist
                _module_to_ext_id.pop(name, None)

            # Remove from sys.path. That is important if next enabled extension will have the same module names, but in different folder.
            if self._use_fast_importer:
                fast_importer.remove_sys_path(f)
            else:
                sys.path.remove(f)


@lru_cache()
def _initialize_once():
    # Hack for C++ tests to disable profiling, which will crash when unloaded and loaded again.
    if _is_reentrant_test_mode():

        def nop(*args, **kwargs):
            pass

        carb.profiler.begin = nop
        carb.profiler.end = nop

    # Replace global import function with profiled one (a bit slower)
    if _is_profile_import_time_enabled():
        enable_profile_import_time()

    fast_importer.initialize(_get_fast_importer_settings())


def prestartup_all_extensions_in_module(
    ext_id: str, path: str, module_name: str, public: bool = False, search_ext: bool = True
):
    _initialize_once()

    refresh_stat_cache()

    global _extensions
    if ext_id not in _extensions:
        _extensions[ext_id] = ExtensionModules(ext_id)

    _extensions[ext_id].prestartup_module(
        ExtensionModule(path=path, name=module_name, public=public, search_ext=search_ext)
    )


def startup_extension(ext_id: str):
    global _extensions
    if ext_id not in _extensions:
        return

    # Call each time extension starts, in most cases function would return immediately. It does useful work once per
    # update and when "sys.path" changed.
    fast_importer.refresh_cache()

    _extensions[ext_id].startup()


def shutdown_extension(ext_id: str):
    global _extensions
    if ext_id not in _extensions:
        return

    _extensions[ext_id].shutdown()


def get_ext_id_from_module(module_name: str) -> str:
    return _module_to_ext_id.get(module_name, None)
