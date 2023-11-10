# __all__ = ["get_app"]

import asyncio
from functools import lru_cache

import carb.events

from .._app import *


@lru_cache()
def get_app_interface() -> IApp:
    """Returns cached :class:`omni.kit.app.IApp` interface"""
    return acquire_app_interface()


def get_app() -> IApp:
    """Returns cached :class:`omni.kit.app.IApp` interface. (shorthand)"""
    return get_app_interface()


async def _pre_update_async(self) -> float:
    """Wait for next update of Omniverse Kit. Return delta time in seconds"""

    order = UPDATE_ORDER_PYTHON_ASYNC_FUTURE_BEGIN_UPDATE

    f = asyncio.Future()

    def on_event(e: carb.events.IEvent):
        if not f.done():
            f.set_result(e.payload["dt"])

    sub = self.get_update_event_stream().create_subscription_to_pop(
        on_event, name="omni.kit.app._pre_update_async", order=order
    )
    return await f


async def _next_update_async(self) -> float:
    """Wait for next update of Omniverse Kit. Return delta time in seconds"""

    # maintain legacy behavior that _next_update_async runs after app usdUpdate + rendering
    order = UPDATE_ORDER_PYTHON_ASYNC_FUTURE_END_UPDATE

    f = asyncio.Future()

    def on_event(e: carb.events.IEvent):
        if not f.done():
            f.set_result(e.payload["dt"])

    sub = self.get_update_event_stream().create_subscription_to_pop(
        on_event, name="omni.kit.app._next_update_async", order=order
    )
    return await f


async def _post_update_async(self) -> float:
    order = POST_UPDATE_ORDER_PYTHON_ASYNC_FUTURE

    f = asyncio.Future()

    def on_event(e: carb.events.IEvent):
        if not f.done():
            f.set_result(e.payload["dt"])

    sub = self.get_post_update_event_stream().create_subscription_to_pop(
        on_event, name="omni.kit.app._post_update_async", order=order
    )
    return await f


IApp.next_update_async = _next_update_async
IApp.post_update_async = _post_update_async
IApp.pre_update_async = _pre_update_async
