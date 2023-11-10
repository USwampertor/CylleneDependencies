from functools import lru_cache

import carb.settings

from .fast_importer import FastImporterSettings

_settings_iface = None


def _get_setting(path, default=None):
    global _settings_iface
    if not _settings_iface:
        _settings_iface = carb.settings.get_settings()
    setting = _settings_iface.get(path)
    return setting if setting is not None else default


@lru_cache()
def _is_full_leak_check_enabled():
    # Full leak check can be disabled, it causes a crash in tests linked with usd. Iterating over gc.get_objects() crashed.
    return _get_setting("/app/extensions/pythonFullLeakCheck", default=True)


@lru_cache()
def _is_startup_disabled():
    return _get_setting("/app/extensions/disableStartup", default=False)


@lru_cache()
def _is_reentrant_test_mode():
    return _get_setting("/app/extensions/~reentrantTestMode", default=False)


@lru_cache()
def _is_path_stat_cache_enabled():
    return not _is_reentrant_test_mode() and _get_setting("/app/extensions/pathStatCacheEnabled", default=True)


@lru_cache()
def _is_custom_importer_enabled():
    return _get_setting("/app/extensions/enableCustomImporter", default=False)


@lru_cache()
def _is_profile_import_time_enabled():
    return _get_setting("/app/extensions/profileImportTime", default=False)


@lru_cache()
def _get_fast_importer_settings() -> FastImporterSettings:
    d = _get_setting("/app/extensions/fastImporter", default={})
    return FastImporterSettings(
        enabled=d.get("enabled", not _is_reentrant_test_mode()),
        file_cache=d.get("fileCache", False),
        search_in_top_namespaces_only=d.get("searchInTopNamespaceOnly", True),
    )
