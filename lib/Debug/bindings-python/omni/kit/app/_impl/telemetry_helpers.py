# __all__ = ["send_telemetry_event"]

from functools import lru_cache

from .app_iface import get_app

_telemetry_hook = None
_telemetry_module = None


@lru_cache()
def _initialize_telemetry():
    global _telemetry_hook
    manager = get_app().get_extension_manager()

    def on_toggle(enabled):
        global _telemetry_module
        if not enabled:
            _telemetry_module = None
            return
        try:
            import omni.kit.telemetry

            _telemetry_module = omni.kit.telemetry
        except:
            pass

    _telemetry_hook = manager.subscribe_to_extension_enable(
        on_enable_fn=lambda _: on_toggle(True),
        on_disable_fn=lambda _: on_toggle(False),
        ext_name="omni.kit.telemetry",
        hook_name="omni.kit.app telemetry",
    )


def send_telemetry_event(
    event_type: str, duration: float = 0, data1: str = "", data2: str = "", value1: float = 0.0, value2: float = 0.0
):
    """
    Send generic telemetry event.

    It is a helper, so that just one liner: `omni.kit.app.send_telemetry_event` can be used anywhere instead of checking
    for telemetry being enabled at each call site.

    If telemetry is not enabled this function does nothing.

    Args:
        event_type (str): A string describing the event that occurred.  There is no restriction on the content or
            formatting of this value. This should neither be `nullptr` nor an empty string.
        duration (float): A generic duration value that can be optionally included with the event.
        data1 (str): A string data value to be sent with the event. The interpretation of this string depends on event_type.
        data2 (str): A string data value to be sent with the event. The interpretation of this string depends on event_type.
        value1 (float): A float data value to be sent with the event. The interpretation of this string depends on event_type.
        value2 (float): A float data value to be sent with the event. The interpretation of this string depends on event_type.
    """

    # This function is called once on first send telemetry call (lru_cache)
    # Instead of checking every time that telemetry extension is enabled we subscibe to event of that extension being enabled.
    # When it happens we set global variable. So for each send telemetry call we only need to check one variable (much faster).
    _initialize_telemetry()

    if _telemetry_module:
        _telemetry_module.ITelemetry().send_custom_event(event_type, duration, data1, data2, value1, value2)
