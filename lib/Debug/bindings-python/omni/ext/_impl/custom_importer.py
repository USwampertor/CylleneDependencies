import importlib
import importlib.util
import os
import sys
import traceback

import carb
import carb.profiler

from .ext_settings import _is_custom_importer_enabled


def reload_module(name: str):
    carb.log_info(f"reloading python module: {name}")
    try:
        if name not in sys.modules:
            return import_module(name)
        return importlib.reload(sys.modules[name])
    except Exception as e:
        exc = traceback.format_exc()
        carb.log_error(f"Failed to reload python module {name}. Error: {e}. Traceback:\n{exc}")
        return None


@carb.profiler.profile()
def _find_spec(finder, absolute_name, path):
    return finder.find_spec(absolute_name, path)


@carb.profiler.profile()
def _module_from_spec(name, spec):
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    return module


@carb.profiler.profile()
def _exec_module(spec, module):
    spec.loader.exec_module(module)


@carb.profiler.profile()
def custom_import(name, package=None):
    """An approximate impl of real import to profile parts of it."""
    absolute_name = importlib.util.resolve_name(name, package)
    try:
        return sys.modules[absolute_name]
    except KeyError:
        pass

    path = None
    if "." in absolute_name:
        parent_name, _, child_name = absolute_name.rpartition(".")
        parent_module = import_module(parent_name)
        path = parent_module.__spec__.submodule_search_locations
    for finder in sys.meta_path:
        spec = _find_spec(finder, absolute_name, path)
        if spec is not None:
            break
    else:
        msg = f"No module named {absolute_name!r}"
        raise ModuleNotFoundError(msg, name=absolute_name)
    module = _module_from_spec(absolute_name, spec)
    _exec_module(spec, module)
    if path is not None:
        setattr(parent_module, child_name, module)
    return module


@carb.profiler.profile(add_args=True)
def import_module(name: str):
    try:
        if _is_custom_importer_enabled():
            return custom_import(name)
        else:
            return importlib.import_module(name)
    except ImportError as e:
        exc = traceback.format_exc()
        carb.log_error(f"Failed to import python module {name}. Error: {e}. Traceback:\n{exc}")
        return None


@carb.profiler.profile(add_args=True)
def import_module_private(folder, module_name: str):
    path_head = os.path.join(folder, module_name.replace(".", "/"))
    if os.path.exists(f"{path_head}.py"):
        spec = importlib.util.spec_from_file_location(module_name, f"{path_head}.py")
    elif os.path.exists(f"{path_head}/__init__.py"):
        spec = importlib.util.spec_from_file_location(module_name, f"{path_head}/__init__.py")
    else:
        carb.log_error(f"Failed to find module {module_name} in {folder}")
        return None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def enable_profile_import_time():
    import builtins

    original_import = builtins.__import__
    mask = 1

    # replace builtin import function with our wrapper. It calls the same function but wrapped into profiler.begin/end

    def _import(name, globals=None, locals=None, fromlist=(), level=0):
        if level == 0:
            active_zone_name = f"import {name}"
        else:
            level_ = "." * level
            name_ = name if name else ",".join(fromlist)
            active_zone_name = f"import {level_}{name_}"

        carb.profiler.begin(mask, active_zone_name)
        try:
            return original_import(name, globals, locals, fromlist, level)
        finally:
            carb.profiler.end(mask)

    builtins.__import__ = _import
