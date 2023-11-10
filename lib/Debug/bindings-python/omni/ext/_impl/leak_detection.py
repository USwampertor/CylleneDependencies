"""This module contains support for detailed leak detection.
It's primarily used for extension management but can also be used by other extensions for leak debugging.

Exports:
    get_dangling_references: Check a single object
    find_leaks_in_module: Check all objects in a module
"""
import gc
import inspect
import os
import sys
from contextlib import suppress
from typing import Dict, List, Tuple, Type

from carb import log_warn

# ======================================================================
# This environment variable turns on more verbose leak reporting
GC_DEBUG = os.getenv("GC_DEBUG")


# ======================================================================
# Helper to make it easy to filter out functions related to leak detection from the leak list
GLOBAL_MARKER_FOR_THIS_FRAME = True


def __referrer_is_mine(referrer) -> bool:
    """Returns True if the gc referrer passed in comes from this file, which should be ignored"""
    if inspect.isframe(referrer):
        try:
            return "GLOBAL_MARKER_FOR_THIS_FRAME" in referrer.f_globals
        except AttributeError:
            return False
    # Also ignore the frame that calld get_dangling_references
    with suppress(ValueError):
        if sys._getframe(3) == referrer:
            return False
    return False


def __referrer_should_be_ignored(referrer, frames_to_ignore: List) -> bool:
    """Returns True if the referrer is in the current file's frames or within the depth of frames to ignore"""
    if __referrer_is_mine(referrer):
        return True
    for frame_to_ignore in frames_to_ignore:
        if frame_to_ignore == referrer:
            return True
    return False


# ======================================================================
def _get_dangling_references_shallow(who: object, frames_to_ignore: List) -> List[str]:
    """Try to format referrers. Those are usually just dicts. If that is a module, it has __name__

    Args:
        who: Object to check for referrers in the garbage collector
        frames_to_ignore: List of frames for which to prevent printing

    Returns:
        A list of strings describing the references on the object 'who'
    """
    refs = []
    LIMIT = 5
    for index, referrer in enumerate(gc.get_referrers(who)):
        if __referrer_should_be_ignored(referrer, frames_to_ignore):
            continue
        if index >= LIMIT:
            refs.append("[*] ...")
            break
        referrer_as_string = f"[{index}]:"
        if isinstance(referrer, dict):
            if "__name__" in referrer:
                referrer_as_string += "__name__:" + referrer["__name__"]
            else:
                referrer_as_string += f"Dictionary[{len(referrer)} key(s)]"
        else:
            referrer_as_string += f"type: {str(type(referrer))}, id: {id(referrer)}"
        refs.append(referrer_as_string)
    return refs


# ======================================================================
def _get_dangling_references_deep(who, frames_to_ignore: List, full_details: bool = False) -> List[str]:
    """Show the garbage collector referral information for the given object, prepending a title for differentiation

    This is a tool that is useful for tracking down Python object leaks when an extension is unloaded and reloaded,
    to ensure you are properly cleaning up when the extension shuts down. You can insert calls to this in various
    locations of your code where you suspect objects are left dangling.

    Args:
        who: Object to check for referrers in the garbage collector
        frames_to_ignore: List of frames for which to prevent printing
        full_details: If True then add the very verbose _builtins, _globals, and _locals information

    Returns:
        A list of strings describing the references on the object 'who'
    """
    referrer_list = []

    for index, referrer in enumerate(gc.get_referrers(who)):

        # References made within this file are not relevant to leak detection
        if __referrer_should_be_ignored(referrer, frames_to_ignore):
            continue

        # Arbitrarily limit the output to the first 100 referrers, as at that point more information is not helpful
        if index >= 100:
            referrer_list.append("[*] ...")
            break
        if isinstance(referrer, dict):
            if "__name__" in referrer:
                referrer_list.append(f"[{index}]: __name__: '" + referrer["__name__"] + "'")
            else:
                referrer_list.append(f"[{index}]: dictionary")
                for key, value in referrer.items():
                    if isinstance(value, list):
                        referrer_list.append(f"    [{key}] = [")
                        for member in value:
                            referrer_list.append(f"       {member}")
                        referrer_list.append("    ]")
                    else:
                        referrer_list.append(f"    [{key}] = {value}")
        else:
            referrer_list.append(f"[{index}]: type: {type(referrer)}, id: {hex(id(referrer))}")
            # Try a few different known types, using attribute error failure to choose which are applicable

            # Frame types
            with suppress(AttributeError):
                referrer_list.append(f"    f_back: {referrer.f_back}")
                referrer_list.append(f"    f_code: {referrer.f_code}")
                referrer_list.append(f"    f_lasti: {referrer.f_lasti}")
                referrer_list.append(f"    f_lineno: {referrer.f_lineno}")
                referrer_list.append(f"    f_trace: {referrer.f_trace}")
                if full_details:
                    referrer_list.append(f"    f_builtins: {referrer.f_builtins}")
                    referrer_list.append(f"    f_globals: {referrer.f_globals}")
                    referrer_list.append(f"    f_locals: {referrer.f_locals}")
                continue

            # Method types
            with suppress(AttributeError):
                referrer_list.append(f"    {referrer.__func__}")
                continue

            # Cell types
            with suppress(AttributeError):
                # Cell contents just points to the object so it would be redundant to report it
                _ = referrer.cell_contents
                continue

            # Object types
            with suppress(AttributeError, TypeError):
                _ = referrer.items()
                referrer_list.append("    Object")
                for key, value in referrer.items():
                    referrer_list.append(f"      {key} = {str(value)}")
                continue

            # List types
            with suppress(TypeError):
                if isinstance(referrer, list):
                    referrer_list.append(f"    List of {len(referrer)} members")
                    continue

            # Other types, with filtered dir() dumps so that more specific information can be discovered
            with suppress(TypeError):
                contents = {
                    key: str(getattr(referrer, key)) for key in dir(referrer) if len(key) < 2 or key[0:2] != "__"
                }
                referrer_list.append(f"    Dir Members {referrer}")
                for key, value in contents.items():
                    referrer_list.append(f"      {key} = {str(value)}")
                continue

            # Full-on dir() dumps so that more specific information can be discovered
            with suppress(TypeError):
                contents = dir(referrer)
                referrer_list.append(f"    Dir {referrer}")
                for key, value in contents.items():
                    referrer_list.append(f"      {key} = {str(value)}")
                continue

            # Nothing matched, use plain old string conversion
            referrer_list.append(f"    UNKNOWN {referrer}")

    return referrer_list


# ======================================================================
def get_dangling_references(who, frame_depth_to_ignore: int = 0, full_details: bool = False) -> str:
    """Returns a string containing debugging information about dangling references on an object.

    Different levels of detail will be provided based on the environment variable "GC_DEBUG", provided by
    the two methods _get_dangling_references_shallow() and _get_dangling_references_deep().

    This is a tool that is useful for tracking down Python object leaks when an extension is unloaded and reloaded,
    to ensure you are properly cleaning up when the extension shuts down. You can insert calls to this in various
    locations of your code where you suspect objects are left dangling.

    That code is open for improvement to figure out more info on who holds an object. For instance It can go
    recursively up the tree to get more info on who holds what.

    Args:
        who: Object to check for referrers in the garbage collector
        frame_depth_to_ignore: Number of frames above this one deemed irrelevant to the leak detector
            e.g. the caller, a wrapper function invoking the caller, etc.
        full_details: If True then add the very verbose _builtins, _globals, and _locals information

    Returns:
        String containing a multi-line description of the references that remain on the object
    """
    frames_to_ignore = []
    for frame_depth in range(0, frame_depth_to_ignore):
        with suppress(ValueError, AttributeError):
            # Add 1 to start from the caller
            frames_to_ignore.append(sys._getframe(frame_depth + 1))
    if GC_DEBUG:
        return "\n    " + "\n    ".join(_get_dangling_references_deep(who, frames_to_ignore, full_details))
    return str(_get_dangling_references_shallow(who, frames_to_ignore))


# ======================================================================
def _get_alive_objects():
    """Get the list of currently alive objects.
    The gc module returns alive objects, most of them are dicts and lists full of objects. A class will be represented
    by the dict of its attributes. It seems to be enough to go over them element by element to find alive instances.
    """
    for obj in gc.get_objects():
        if isinstance(obj, list):
            yield from obj
        elif isinstance(obj, tuple):
            yield from obj
        elif isinstance(obj, dict):
            yield from obj.values()
        else:
            yield obj


# ======================================================================
def find_leaks_in_module(module_name: str):
    """Examine the contents of the named module for leaks, emitting carbonite warnings if any are found"""
    info: Dict[Type, Tuple[int, str]] = {}  # , count and referer string

    # There is a known issue with enums in pybind11 which causes them to leak memory
    # (see https://github.com/pybind/pybind11/issues/3865)
    #
    # Omit leaky pybind enums from the warning info to reduce spam
    pybind_enum_leak_count = 0

    for live_object in _get_alive_objects():
        if hasattr(live_object, "__class__"):
            # Check it is from this module
            if live_object.__class__.__module__.startswith(module_name):
                if hasattr(type(live_object), "__is_enum"):
                    # pybind enum, probably leaking due to a bug in pybind11, so skip the warning logging
                    pybind_enum_leak_count += 1
                    continue
                if live_object.__class__ not in info:
                    info[live_object.__class__] = [0, get_dangling_references(live_object)]
                info[live_object.__class__][0] += 1

    if pybind_enum_leak_count:
        log_warn(f"Ignored {pybind_enum_leak_count} leaks from pybind enums due to known pybind bug.")
    if info:
        # Promote those to log_error once everything is fixed
        log_warn(f"[Potential Error] Leaks detected in {module_name}:")
    for leak_name in info:
        log_warn(
            f"[Potential Error] Leak: {leak_name}, instances: {info[leak_name][0]}, module: {leak_name.__module__}, references: {info[leak_name][1]}"
        )
