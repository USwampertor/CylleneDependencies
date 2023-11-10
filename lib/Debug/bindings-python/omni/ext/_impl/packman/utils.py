# Copyright 2019-2021 NVIDIA CORPORATION

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import ctypes
import datetime
import functools
import io
import logging
import math
import os
import pathlib
import re
import shutil
import stat
import sys
import tempfile
import threading
import time
from typing import Any, Callable, Iterator, List, Tuple, Union

from . import errors, link

logger = logging.getLogger("packman.utils")


def get_pretty_size(size_bytes):
    # We use 4-character maximum width for number string
    if size_bytes < 1024:
        return "%d bytes" % size_bytes
    size_name = ("bytes", "KiB", "MiB", "GiB", "TiB", "PiB")
    i = int(math.floor(math.log(size_bytes, 1024)))
    p = math.pow(1024, i)
    s = round(size_bytes / p, 2)
    number_str = "%.2f" % s
    num_len = len(number_str)
    max_len = 4
    if num_len > max_len:
        # chop of dangling decimal separator:
        if number_str[max_len - 1] == ".":
            max_len -= 1
        excess = num_len - max_len
        number_str = number_str[: num_len - excess]

    return "%s %s" % (number_str, size_name[i])


def get_pretty_speed(size_bytes, time_seconds):
    try:
        pretty_speed = "%s/s" % get_pretty_size(size_bytes / time_seconds)
    except ZeroDivisionError:
        pretty_speed = "fast"
    return pretty_speed


class NoLock(object):
    def __init__(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        pass


def raise_if_less_space(path: str, num_bytes: int):
    """Logs an error if not enough space available"""
    if path:
        abs_path = os.path.abspath(path)
        if os.path.exists(abs_path):
            if os.path.isfile(abs_path):  # Windows requires a directory path (fixed in Python 3.8)
                abs_path = os.path.dirname(abs_path)
        else:
            while True:
                abs_path = os.path.dirname(abs_path)
                if not abs_path or os.path.exists(abs_path):
                    break
    else:
        abs_path = os.getcwd()
    _, _, free = shutil.disk_usage(abs_path)
    if free < num_bytes:
        raise errors.PackmanError(
            f"Not enough free space available at '{abs_path}' - requested {get_pretty_size(num_bytes)} "
            f" but only {get_pretty_size(free)} bytes available!"
        )


def walk(top: str, follow_links: bool = False) -> Tuple[Iterator[str], Iterator[str], Iterator[str]]:
    """
    The built-in os.walk in Python doesn't handle Windows junctions correctly. By default
    walk should not follow links, so we have to code this up ourselves. Sad face.
    """
    top = os.fspath(top)
    dirs = []
    nondirs = []
    walk_dirs = []

    try:
        scandir_it = os.scandir(top)
    except OSError as error:
        return

    with scandir_it:
        while True:
            try:
                try:
                    entry = next(scandir_it)
                except StopIteration:
                    break
            except OSError as error:
                return

            try:
                is_dir = entry.is_dir()
            except OSError:
                # If is_dir() raises an OSError, classify as not a directory
                is_dir = False

            if is_dir:
                dirs.append(entry.name)
            else:
                nondirs.append(entry.name)

    yield top, dirs, nondirs

    # Recurse into subdirectories
    for dirname in dirs:
        new_path = os.path.join(top, dirname)
        if follow_links or not link.is_link(new_path):
            yield from walk(new_path, follow_links)


def for_each_file_in_folder_tree(folder_path, file_func, follow_links=False):
    for (dirpath, dirnames, filenames) in walk(folder_path, follow_links=follow_links):
        for filename in filenames:
            file_func(os.path.join(dirpath, filename))


def for_each_item_in_folder_tree(folder_path, item_func, follow_links=False):
    for (dirpath, dirnames, filenames) in walk(folder_path, follow_links=follow_links):
        for filename in filenames:
            item_func(os.path.join(dirpath, filename))
        for dirname in dirnames:
            item_func(os.path.join(dirpath, dirname))


def get_size_on_disk(path):
    stat = os.stat(path)
    file_size = stat.st_size
    if not file_size:
        file_size = stat.st_blksize if hasattr(stat, "st_blksize") else 4096
    return file_size


class Utf8ConsoleEncoding:
    def __init__(self):
        if os.name == "nt":
            self.kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        else:
            self.kernel = None

    def __enter__(self):
        if self.kernel:
            self.console_codepage = self.kernel.GetConsoleCP()
            self.output_codepage = self.kernel.GetConsoleOutputCP()
            self.kernel.SetConsoleCP(65001)
            self.kernel.SetConsoleOutputCP(65001)

    def __exit__(self, type, value, traceback):
        if self.kernel:
            self.kernel.SetConsoleCP(self.console_codepage)
            self.kernel.SetConsoleOutputCP(self.output_codepage)
