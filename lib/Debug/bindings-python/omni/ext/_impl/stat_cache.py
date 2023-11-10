import gc
import importlib
import os

from .ext_settings import _is_path_stat_cache_enabled

_stat_cache = {}
_stat_cache_update_number = -1


def _os_stat_cached(path):
    # Cache both return value and exception (do not use @functools.lru_cache)
    global _stat_cache
    r, exc = _stat_cache.get(path, (None, None))
    if r is None and exc is None:
        try:
            r = os.stat(path)
        except Exception as e:
            exc = e
        _stat_cache[path] = (r, exc)
    if exc:
        raise exc
    return r


def refresh_stat_cache():
    if not _is_path_stat_cache_enabled():
        return

    import omni.kit.app

    number = omni.kit.app.get_app().get_update_number()

    global _stat_cache, _stat_cache_update_number
    # Invalidate OS stat call cache every update
    if _stat_cache_update_number != number:
        _stat_cache_update_number = number
        _stat_cache.clear()
        # Speed up: replace python import library stat called with version that has a cache
        importlib._bootstrap_external._path_stat = _os_stat_cached


def reset_stat_cache():
    global _stat_cache_update_number
    _stat_cache_update_number = -1
    refresh_stat_cache()
