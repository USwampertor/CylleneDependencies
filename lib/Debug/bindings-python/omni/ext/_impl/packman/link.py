# Copyright 2019 NVIDIA CORPORATION

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#     http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import subprocess

from . import errors, utils


def _call_command(args):
    with utils.Utf8ConsoleEncoding():
        p = subprocess.Popen(
            args,
            bufsize=0,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            stdin=subprocess.DEVNULL,
            shell=True,
        )
        out, err = p.communicate()
    # we need to return out and err as str, not bytes
    return p.returncode, out.decode("utf8"), err.decode("utf8")


def _sanitize_path(path):
    # Adjust paths so they match native format
    ret = path.replace("/", os.path.sep)  # posix fix
    ret = ret.rstrip(os.path.sep)  # chop of any trailing slashes
    return ret


def _create_junction_link(link_folder_path, target_folder_path):
    # target folder path can be relative but mklink interprets it as relative to CWD rather than relative to link
    # - this forces us to always make junction links absolute
    path = target_folder_path
    path = os.path.join(os.path.dirname(link_folder_path), path)
    path = os.path.normpath(path)

    args = ("mklink", "/j", link_folder_path, path)
    ret_code, out, err = _call_command(args)
    if ret_code:
        if "file already exists" in err:
            # check to see if we have a link already
            try:
                target = get_link_target(link_folder_path)
                if os.path.normcase(target) == os.path.normcase(path):
                    return  # we are already set up
            except RuntimeError:
                pass
        msg = err.strip() + " (%s ==> %s)" % (link_folder_path, path)
        raise RuntimeError(msg)


def _create_symbolic_link(link_path: str, target_path: str, target_is_dir: bool):
    try:
        os.symlink(target_path, link_path, target_is_directory=target_is_dir)
    except FileExistsError as exc:
        # Another process might have beat us to it, only fail if it's not a link or
        # doesn't match
        try:
            target = get_link_target(link_path)
        except RuntimeError:
            # raise original exception
            raise exc
        if os.path.normcase(target) != os.path.normcase(target_path):
            raise exc


def _destroy_link_win(link_folder_path):
    args = ("rmdir", link_folder_path)
    ret_code, out, err = _call_command(args)
    if ret_code:
        msg = err.strip() + " (%s)" % link_folder_path
        raise RuntimeError(msg)


def get_link_target(link_folder_path: str) -> str:
    """Returns the target of the link at `link_folder_path`. If the provided path does not exist
    the call will raise PackmanErrorFileNotFound exception. If the provided path already exists and is not
    a link the call will raise PackmanErrorFileNotLink exception. In all other cases it will return the link target.
    Note that the link target can be an absolute or relative path, depending on how the link was
    created. If relative then it's relative to the directory containing `link_folder_path`.
    """
    # with python 3.8 and beyond os.readlink now supports junctions (reparse points)
    # the only pitfall is that for reparse points it will return the path with the extended path prefix
    # that is: r"\\?\" - we need to chop this off to not cause complete chaos for users of this function.
    link_folder_path = _sanitize_path(link_folder_path)
    try:
        path = os.readlink(link_folder_path)
    except FileNotFoundError as exc:
        raise errors.PackmanErrorFileNotFound(str(exc))
    except OSError:
        raise errors.PackmanErrorFileNotLink(f"Path '{link_folder_path}' exists but is not a link.")

    if os.name == "nt":
        if path.startswith("\\\\?\\"):
            path = path[4:]

    return path


def is_link(path: str) -> bool:
    try:
        get_link_target(path)
    except errors.PackmanErrorFileNotLink:
        return False
    else:
        return True


def create_link(link_path: str, target_path: str, target_is_dir=True):
    """
    Creates a symbolic link from 'link_path' to 'target_path'

    link_path: Absolute or relative path to link to create
    target_path: Absolute or relative path to target; if relative then it is relative to 'link_path'
    target_is_dir: This boolean is only used on Windows to specify if the target is a directory or file.
    """
    link_path = _sanitize_path(link_path)
    target_path = _sanitize_path(target_path)
    # Before we do anything we check to see if the link is already correctly set up:
    try:
        target_now = get_link_target(link_path)
    except errors.PackmanErrorFileNotFound:
        pass  # this is just fine
    except errors.PackmanErrorFileNotLink:
        raise errors.PackmanErrorFileExists(
            f"Path '{link_path}' cannot be created because it already exists as a different type of file."
        )
    else:
        if os.path.normcase(target_now) == os.path.normcase(target_path):
            return  # nothing to do here, already set up
        else:
            # we know we have a link so lets remove the incorrect link
            os.remove(link_path)

    # We always try first using symlink, this can fail on Windows if admin privileges are not
    # present - we then fall back to junction point.
    try:
        _create_symbolic_link(link_path, target_path, target_is_dir=target_is_dir)
    except OSError as exc:
        message = str(exc)
        if (
            target_is_dir
            and os.name == "nt"
            and "1314" in message  # This is the windows error code, it's the same for all languages of Windows
        ):
            _create_junction_link(link_path, target_path)
        else:
            raise RuntimeError(message)


def destroy_link(link_folder_path):
    """
    Destroys an existing file system link
    :param link_folder_path: Path to linked folder to destroy.
    :return: None
    """
    link_folder_path = _sanitize_path(link_folder_path)
    if os.name == "nt":
        _destroy_link_win(link_folder_path)
    else:
        try:
            os.unlink(link_folder_path)
        except OSError as exc:
            raise RuntimeError(str(exc))
