__copyright__ = "Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved."
__license__ = """
NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
"""

import inspect

import carb
import omni.core
import omni.str

from ._log import *


def log(level: Level, msg: str, channel=None, origin_stack_depth=1):
    """Logs a message at the given level.

    origin_stack_depth -- Describes how many stack frames to move up to grab information about the origin of the log
    message (filename, line number, etc).  A valid of 1 (the default) means use the stack information from the function
    that called this function.  A value greater than 1 can be used when wrapping this function.
    """
    frame = inspect.stack()[origin_stack_depth]
    filename = frame[0].f_code.co_filename
    line_num = frame[0].f_lineno
    func_name = frame.function

    # there may not be a module name (if running in the interactive interpreter)
    module = inspect.getmodule(frame[0])
    if module is not None:
        module_name = module.__name__
    else:
        # the interactive interpreter seems to call itself <module>
        module_name = "<module>"

    _log._write(channel, level, msg, module_name, filename, line_num, func_name)


def verbose(msg: str, channel=None, origin_stack_depth=1):
    """Logs a verbose message."""
    log(Level.VERBOSE, msg, channel, origin_stack_depth + 1)


def info(msg: str, channel=None, origin_stack_depth=1):
    """Logs an informational message."""
    log(Level.INFO, msg, channel, origin_stack_depth + 1)


def warn(msg: str, channel=None, origin_stack_depth=1):
    """Logs a warning."""
    log(Level.WARN, msg, channel, origin_stack_depth + 1)


def error(msg: str, channel=None, origin_stack_depth=1):
    """Logs an error."""
    log(Level.ERROR, msg, channel, origin_stack_depth + 1)


def fatal(msg: str, channel=None, origin_stack_depth=1):
    """Logs a fatal error.  This function only logs.  This function does _not_ terminate the process."""
    log(Level.FATAL, msg, channel, origin_stack_depth + 1)
