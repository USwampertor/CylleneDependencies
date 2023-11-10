__copyright__ = "Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved."
__license__ = """
NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
"""

"""This package is the gateway to all things Carbonite. If you are starting from scratch:

    >>> import carb
    >>> f = carb.get_framework()
    >>> f.startup()

There is however no need to start up the framework if you are working within a Carbonite
based application. This has already been handled.

Attributes:
    logging (carb.pycarb.logging.ILogging): Gives access to the ILogging interface, which
        can be used to set configuration for logging.

    filesystem (carb.pycarb.filesystem.IFileSystem): Gives access to the IFileSystem interface, which is used to
        read and write files in a platform independent manner, using a uniform path format
        that supports utf8 and long paths.
"""
import sys

from ._carb import *

sys.modules["carb.logging"] = logging
sys.modules["carb.filesystem"] = filesystem


def _get_caller_info():
    f = sys._getframe(2)
    if f is None or not hasattr(f, "f_code"):
        return "(unknown file)", 0, "(unknown function)", "(unknown module)"

    return f.f_code.co_filename, f.f_lineno, f.f_code.co_name, f.f_globals.get("__name__", "")


def log_error(msg):
    file, lno, func, mod = _get_caller_info()
    log(mod, logging.LEVEL_ERROR, file, func, lno, f"{msg}")


def log_warn(msg):
    file, lno, func, mod = _get_caller_info()
    log(mod, logging.LEVEL_WARN, file, func, lno, f"{msg}")


def log_info(msg):
    file, lno, func, mod = _get_caller_info()
    log(mod, logging.LEVEL_INFO, file, func, lno, f"{msg}")


def log_verbose(msg):
    file, lno, func, mod = _get_caller_info()
    log(mod, logging.LEVEL_VERBOSE, file, func, lno, f"{msg}")
