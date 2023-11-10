__copyright__ = "Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved."
__license__ = """
NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
"""

"""This module contains bindings to C++ carb::IEvents interface.

Use get_events_interface method, which caches acquire interface call:

.. code-block::

    import carb.events
    events = carb.events.get_events_interface()
    s = events.create_event_stream()
    s.push(0, { "x" : 2 })
    e = s.pop()
    print(e.type)
    print(e.payload)

You also can subscribe to :class:`.IEventStream`

.. code-block::

    import carb.events
    events = carb.events.get_events_interface()
    s = events.create_event_stream()

    def on_event(e):
        print(e.type)
        print(e.payload)

    subscription = s.create_subscription_to_pop(on_event)

    # Some code which pushes events
    # ...

    # unsubscribe
    subscription = None


:class:`.IEventStream` object can often be received from other APIs. Use it to subscribe for events, or push your events.
"""

# This module depends on other modules. VSCode python language server scrapes
# modules in an isolated environment (ignoring PYTHONPATH set). `import` fails and for that we have separate code
# path to explicitly add it's folder to sys.path and import again.
try:
    import carb
    import carb.dictionary
except:
    import os
    import sys

    sys.path.append(os.path.abspath(os.path.join(os.path.dirname(os.path.realpath(__file__)), "..", "..", "..")))
    import carb
    import carb.dictionary

import asyncio
import inspect

from ._events import *


def get_events_interface() -> IEvents:
    """Returns cached :class:`carb.events.IEvents` interface"""

    if not hasattr(get_events_interface, "iface"):
        get_events_interface.iface = acquire_events_interface()
    return get_events_interface.iface


async def _next_event(self: IEventStream, order: int = 0, name: str = ""):
    """Async wait for next event."""

    f = asyncio.Future()

    if len(name) == 0:
        frame = inspect.currentframe().f_back
        name = f"{frame.f_code.co_name} at {frame.f_code.co_filename}:{frame.f_lineno}"

    def on_event(e: IEvent):
        if not f.done():
            f.set_result(e)

    sub = self.create_subscription_to_pop(on_event, order, name)
    res = await f
    return res


async def _next_event_by_type(self: IEventStream, event_type: int, order: int = 0, name: str = ""):
    """Async wait for next event of particular type."""

    f = asyncio.Future()

    if len(name) == 0:
        frame = inspect.currentframe().f_back
        name = f"{frame.f_code.co_name} at {frame.f_code.co_filename}:{frame.f_lineno}"

    def on_event(e: IEvent):
        if not f.done():
            f.set_result(e)

    sub = self.create_subscription_to_pop_by_type(event_type, on_event, order, name)
    res = await f
    return res


IEventStream.next_event = _next_event
IEventStream.next_event_by_type = _next_event_by_type
