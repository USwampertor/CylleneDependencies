"""Fast importer for Python files.

The main idea is to scan all python files ahead of time and return results quickly without searching.

It can also persist scan results between runs making it even faster (no scanning at all). But that makes it less developer
friendly, when python files are added/removed.

Overall, python's import system is quite good at caching, it usually doesn't search twice. So theoretically we should
not need this at all. However, in practice, it still does extra OS filesystem calls and a lot of path operations. 
This is especially true for the way OV extensions are designed, where there are 100+ "sys.path" with a lot of virtual 
namespaces. Our "_os_stat_cached" patch made it much better, but still, there is a lot of overhead.

Also, there is this known issue: https://github.com/python/cpython/issues/91519
"""

import glob
import os
import pickle
import sys
from dataclasses import dataclass
from functools import lru_cache
from importlib import metadata
from pathlib import Path

import _frozen_importlib as _bootstrap
import _frozen_importlib_external as _bootstrap_external
import carb.profiler
import carb.tokens

from .utils import log_exec_time

_sys_paths = {}

CACHE_VERSION = 1
MODULES_EXTS = (".py", *_bootstrap_external.EXTENSION_SUFFIXES)  # e.g.: (".py", ".pyd", ".cp37-win_amd64.pyd")
IS_WINDOWS = sys.platform == "win32"


@dataclass(frozen=True)
class FastImporterSettings:
    """Settings for fast importer."""

    enabled: bool
    file_cache: bool
    search_in_top_namespaces_only: bool


_settings = None


class _Cache:
    """Cache for fast importer. Everything can be stored (pickled) in a file."""

    def __init__(self):
        self.version = CACHE_VERSION
        self.known_paths = set()
        self.modules = {}
        self.namespaces = {}
        self.metadata = {}

    @staticmethod
    def _get_cache_path():
        tokens = carb.tokens.get_tokens_interface()
        path = tokens.resolve("${cache}/fast_importer.pickle")
        return path

    @staticmethod
    def load():
        cache = None
        try:
            with open(_Cache._get_cache_path(), "rb") as f:
                cache = pickle.load(f)
        except FileNotFoundError:
            pass

        if not cache or cache.version != CACHE_VERSION:
            cache = _Cache()

        return cache

    def save(self):
        self.abc = 5
        with open(_Cache._get_cache_path(), "wb") as f:
            pickle.dump(self, f, pickle.HIGHEST_PROTOCOL)


_cache = _Cache()
_cache_update_number = -1


def add_sys_path(path: str, module: str):
    """Add sys.path to be handled by the fast importer.

    The module is the name of the module that is expected to be imported from the path. It speeds up a search of python files. Can
    be None or empty to search for all files in that path.

    Returns True if the path was not added before.
    """
    modules = _sys_paths.setdefault(path, [])
    is_new = not modules
    modules.append(module)
    _reset_cache_update_number()
    return is_new


def remove_sys_path(path: str):
    """Remove sys.path from the fast importer."""
    del _sys_paths[path]
    _reset_cache_update_number()


@lru_cache()
def initialize(settings: FastImporterSettings):
    """Enable the fast importer. Only the first call has an effect."""
    global _settings
    _settings = settings
    sys.meta_path.insert(0, FastFinder)


def _reset_cache_update_number():
    """Reset the cache update number. This will force a cache refresh next time."""
    global _cache_update_number
    _cache_update_number = -1


def _check_new_update_number():
    """Check if the app update number has changed since the last check."""
    import omni.kit.app

    number = omni.kit.app.get_app().get_update_number()

    global _cache_update_number
    if _cache_update_number != number:
        _cache_update_number = number
        return True

    return False


def refresh_cache():
    """Try refresh cache if necessary."""
    if not _settings.enabled:
        return

    # Do not update more often than once per app update
    if not _check_new_update_number():
        return

    _refresh_cache()


@log_exec_time()
@carb.profiler.profile()
def _refresh_cache():

    # Cache can be loaded from a file to persist between runs
    global _cache
    _cache = _Cache.load() if _settings.file_cache else _cache

    # If cache was loaded from the file we can compare sys.path it was built with with the current ones.
    # Only new paths need to be scanned. Also, update cache with current paths.
    # If an extension is shutdown path is removed from cache and if enabled again it will be rescanned. That should allow
    # reloading workflow even with fast importer.
    paths = set(_sys_paths.keys())
    new_paths = paths - _cache.known_paths
    _cache.known_paths = paths

    for key in new_paths:

        # Normalize to make all following operations easier (+faster)
        path = key.replace("\\", "/")

        # Top dirs to scan in. If None we scan whole dir.
        # Idea here is to use top namespace to skip scanning a bunch of top dirs. E.g. if you look for `omni.kit.foo`
        # module filtersdirs will be set to `omni` and we will only scan `omni` dir.
        # We can't go deeper that top namespace (e.g. search in only `omni/kit`) as it would require extensions to
        # be very verbose about each module they bring (both public and private). In this example, `foo.py` can do
        # "from .. import _bar" and that would already not work without asking to search in `omni.bar` too.
        # Top namespace seems to be a good compromise. At least each extension can hint importer which folder to start with.
        # If extension has "[[python.module]]" without name specified (module == "") we will just scan everything.
        # omni.kit.pip_archive is a typical example of that. It brings A LOT of modules.
        # In my approx measurements that cuts the cost of building cache in half.
        filterdirs = None
        if _settings.search_in_top_namespaces_only:
            modules_to_import = _sys_paths[key]
            filterdirs = {module.split(".")[0] for module in modules_to_import}
            if filterdirs and "" in filterdirs:
                filterdirs = None

        for fullpath, subpath in _fast_walk(path, filterdirs):
            # Check if python module (or looks like it, .so file can be non-python modules, but we still have to treat
            # them as such. Only when someone actually imports them an error will be raised (not our business).
            for module_ext in MODULES_EXTS:
                if subpath.endswith(module_ext):
                    dirname, basename = _fast_path_split(subpath)

                    # Now we need to build a module name and a namespace path this module is in.
                    # 2 cases: regular module and __init__.py.
                    if basename == "__init__.py":
                        module_name = dirname.replace("/", ".")
                        namespace_path = _fast_path_split(dirname)[0]
                    else:
                        module_name = subpath[: -len(module_ext)].replace("/", ".")
                        namespace_path = dirname

                    # Store found python module
                    _cache.modules[module_name] = fullpath

                    # Walk back namespace path. If our file is `omni/kit/about/foo.py` namespace path will be `omni/kit/about`.
                    # "omni", "omni/kit", "omni/kit/about" are virtual namespaces in that case and we want to store paths to them.
                    parts = namespace_path.split("/") if namespace_path else None
                    while parts:
                        namespace_module = ".".join(parts)
                        _cache.namespaces.setdefault(namespace_module, set()).add(_fast_join(path, "/".join(parts)))
                        parts = parts[:-1]

                    break

    # Store cache in a file if enabled
    if _settings.file_cache:
        _cache.save()


def _fast_join(p0, p1):
    return f"{p0}/{p1}"


def _fast_path_split(path):
    i = path.rfind("/") + 1
    return path[:i].rstrip("/"), path[i:]


def _fast_walk(top, filterdirs: set = None, subpath=""):
    """Like os.walk, but faster and with top dirs filter support."""
    scandir_it = os.scandir(top)
    with scandir_it:
        while True:

            try:
                try:
                    entry = next(scandir_it)
                except StopIteration:
                    break
            except OSError as error:
                return

            name = entry.name

            # On filesystems using CIFS and mfsymlinks, is_dir will return False for a symlink to a directory
            # but os.path.isdir() will return True - see https://github.com/python/cpython/issues/102503
            new_path = _fast_join(top, name)
            try:
                is_dir = entry.is_dir() or (not IS_WINDOWS and os.path.isdir(new_path))
            except OSError:
                is_dir = False

            # For dirs we can filter out some of them
            if is_dir:
                if filterdirs and name not in filterdirs:
                    continue

                if name == "__pycache__":
                    continue

            new_subpath = _fast_join(subpath, name) if subpath else name
            if is_dir:
                yield from _fast_walk(new_path, None, new_subpath)
            else:
                yield new_path, new_subpath


class FastFinder:
    @classmethod
    def find_spec(cls, fullname, path=None, target=None):
        """Find spec for a module.

        This is what is called for every module/namespace even before sys.path hooks. If we know it we can early return
        and save a lot of regular searching in sys.path.
        """

        # Known Module? Easy just return a file spec.
        module_path = _cache.modules.get(fullname, None)
        if module_path:
            return _bootstrap_external.spec_from_file_location(fullname, module_path)

        # Namespace?
        namespace_paths = _cache.namespaces.get(fullname, None)
        if namespace_paths:
            # Build spec for namespace
            spec = _bootstrap.ModuleSpec(fullname, None)
            spec.submodule_search_locations = _bootstrap_external._NamespacePath(
                fullname, list(namespace_paths), cls.find_spec
            )

            # We can't just return namespace. Other parts of it can still be in sys.path.
            # For instance we know about "omni.kit.about", but "omni.kit.app" is still in sys.path. So "omni.kit" has 2
            # paths to search in ("submodule_search_locations"). We need to add them together, so here we fallback to a regular
            # sys.path search. But since sys.path should very small here (we already consumed most of them) it should be fast.
            spec_default = _bootstrap_external.PathFinder.find_spec(fullname, path, target)
            if spec_default:
                for p in spec_default.submodule_search_locations:
                    spec.submodule_search_locations.append(p)

            return spec

        # If we don't know if will fallback to other finders in sys.meta_path
        return None

    @classmethod
    def find_distributions(cls, context):
        module_path = _cache.modules.get(context.name, None)
        if module_path:

            cached_metadata = _cache.metadata.get(context.name, None)
            if cached_metadata:
                return cached_metadata

            path = Path(module_path)
            pip_install_dir = path.parent.parent
            search_pattern = pip_install_dir.__str__() + f"\{context.name}*.dist-info"
            metadata_folders = glob.glob(search_pattern)

            if metadata_folders.__len__() > 0:
                module_metadata = map(metadata.PathDistribution, [Path(p) for p in metadata_folders])
                _cache.metadata[context.name] = list(module_metadata)
                return _cache.metadata[context.name]

        # Fallback to another resolver
        return []
