from ._omniclient import *

import asyncio
import concurrent.futures
from typing import Tuple, List, Callable


def get_server_info(url: str) -> Tuple[Result, ServerInfo]:
    """Get server info by url (blocking version).
    """

    ret = None

    def on_info_cb(result, info):
        nonlocal ret
        ret = (result, info)

    get_server_info_with_callback(url, on_info_cb).wait()

    return ret


async def get_server_info_async(url: str) -> Tuple[Result, ServerInfo]:
    """Get server info by url (async version).
    """

    f = concurrent.futures.Future()

    def on_info_cb(result, info):
        if not f.done():
            f.set_result((result, info))

    get_server_info_with_callback(url, on_info_cb)

    return await asyncio.wrap_future(f)


async def list_async(url: str) -> Tuple[Result, Tuple[ListEntry]]:
    """Asynchronously list content of a folder. **(Coroutine version.)**

        Args:
            url (str): Url.
        Returns:
            (Tuple[Result, Tuple[omni.client.ListEntry]]]): Result and tuple of entries.
    """

    f = concurrent.futures.Future()

    def on_list_cb(result, entries):
        if not f.done():
            f.set_result((result, entries))

    list_with_callback(url, on_list_cb)

    return await asyncio.wrap_future(f)


def list(url: str) -> Tuple[Result, Tuple[ListEntry]]:
    """Synchronously list content of a folder. **(Blocking version.)**

        Args:
            url (str): Url.
        Returns:
            (Tuple[Result, Tuple[omni.client.ListEntry]]]): Result and tuple of entries.
    """

    ret = None

    def on_list_cb(result, entries):
        nonlocal ret
        ret = (result, entries)

    list_with_callback(url, on_list_cb).wait()

    return ret


async def stat_async(url: str) -> Tuple[Result, ListEntry]:
    """Asynchronously retrieve information about a file or folder. **(Coroutine version.)**

        Args:
            url (str): Url.
        Returns:
            (Tuple[Result, omni.client.ListEntry]]): Result and entry.
    """

    f = concurrent.futures.Future()

    def on_stat_cb(result, entry):
        if not f.done():
            f.set_result((result, entry))

    stat_with_callback(url, on_stat_cb)

    return await asyncio.wrap_future(f)


def stat(url: str) -> Tuple[Result, ListEntry]:
    """Synchronously retrieve information about a file or folder. **(Blocking version.)**

        Args:
            url (str): Url.
        Returns:
            (Tuple[Result, omni.client.ListEntry]]): Result and entry.
    """

    ret = None

    def on_stat_cb(result, entry):
        nonlocal ret
        ret = (result, entry)

    stat_with_callback(url, on_stat_cb).wait()
    return ret


async def delete_async(url: str) -> Result:
    """Asynchronously delete something (file, folder, mount, liveFile, channel etc..). **(Coroutine version.)**

        Args:
            url (str): Url.

        Returns:
            (omni.client.Result): Result.
    """

    f = concurrent.futures.Future()

    def on_delete_cb(result):
        if not f.done():
            f.set_result(result)

    delete_with_callback(url, on_delete_cb)

    return await asyncio.wrap_future(f)


def delete(url: str) -> Result:
    """Synchronously delete something (file, folder, mount, liveFile, channel etc..). **(Blocking version.)**

        Args:
            url (str): Url.

        Returns:
            (omni.client.Result): Result.
    """

    ret = None

    def on_delete_cb(result):
        nonlocal ret
        ret = result

    delete_with_callback(url, on_delete_cb).wait()

    return ret


async def create_folder_async(url: str) -> Result:
    """Asynchronously create a folder. **(Coroutine version.)**

    Args:
        url (str): Url.

    Returns:
        (omni.client.Result): Result.
    """

    f = concurrent.futures.Future()

    def on_create_folder_cb(result):
        if not f.done():
            f.set_result(result)

    create_folder_with_callback(url, on_create_folder_cb)

    return await asyncio.wrap_future(f)


def create_folder(url: str) -> Result:
    """Synchronously create a folder. **(Blocking version.)**

    Args:
        url (str): Url.

    Returns:
        (omni.client.Result): Result.
    """

    ret = None

    def on_create_folder_cb(result):
        nonlocal ret
        ret = result

    create_folder_with_callback(url, on_create_folder_cb).wait()

    return ret


async def copy_async(src_url: str, dst_url: str, behavior: CopyBehavior = CopyBehavior.ERROR_IF_EXISTS) -> Result:
    """Asynchronously copy a thing from ``src_url`` to ``dst_url``. **(Coroutine version.)**

        The thing can be anything that supports copying (files, folders, etc..)
        If both src and dst are on the same server, this is done on the server
        Otherwise the file is first downloaded from 'src' then uploaded to 'dst'
        Destination folders will be created as needed

    Args:
        src_url (str): Source url.
        dst_url (str): Destination url.
        behavior (omni.client.CopyBehavior, default:ERROR_IF_EXISTS): behavior if the destination exists.

    Returns:
        (omni.client.Result): Result.
    """

    f = concurrent.futures.Future()

    def on_copy_cb(result):
        if not f.done():
            f.set_result(result)

    copy_with_callback(src_url, dst_url, on_copy_cb, behavior)

    return await asyncio.wrap_future(f)


def copy(src_url: str, dst_url: str, behavior: CopyBehavior = CopyBehavior.ERROR_IF_EXISTS) -> Result:
    """Synchronously copy a thing from ``src_url`` to ``dst_url``. **(Blocking version.)**

        The thing can be anything that supports copying (files, folders, etc..)
        If both src and dst are on the same server, this is done on the server
        Otherwise the file is first downloaded from 'src' then uploaded to 'dst'
        Destination folders will be created as needed

    Args:
        src_url (str): Source url.
        dst_url (str): Destination url.
        behavior (omni.client.CopyBehavior, default:ERROR_IF_EXISTS): behavior if the destination exists.

    Returns:
        (omni.client.Result): Result.
    """

    ret = None

    def on_copy_cb(result):
        nonlocal ret
        ret = result

    copy_with_callback(src_url, dst_url, on_copy_cb, behavior).wait()

    return ret


async def get_local_file_async(url: str) -> Tuple[Result, str]:
    """
    Asynchronously get local file path for remote file. **(Coroutine version.)**

    Args:
        url (str): Url.
        callback (Callable[omni.client.Result, str (local file path)]): Callback to be called with the result.

    Returns:
        (omni.client.Request): Request object.
    """

    f = concurrent.futures.Future()

    def on_get_local_file_cb(result, local_file_path):
        if not f.done():
            f.set_result((result, local_file_path))

    get_local_file_with_callback(url, on_get_local_file_cb)

    return await asyncio.wrap_future(f)


def get_local_file(url: str) -> Tuple[Result, str]:
    """
    Get local file path for remote file. **(Blocking version.)**

    Args:
        url (str): Url.
        callback (Callable[omni.client.Result, str (local file path)]): Callback to be called with the result.

    Returns:
        (omni.client.Request): Request object.
    """

    ret = None

    def on_get_local_file_cb(result, local_file_path):
        nonlocal ret
        ret = (result, local_file_path)

    get_local_file_with_callback(url, on_get_local_file_cb).wait()

    return ret


async def read_file_async(url: str) -> Tuple[Result, str, Content]:
    """Asynchronously read a file. **(Coroutine version.)**

    Args:
        url (str): Url.

    Returns:
        (Tuple[omni.client.Result, str, omni.client.Content]): Result, version and content. 
    """

    f = concurrent.futures.Future()

    def on_read_file_cb(result, version, content):
        if not f.done():
            f.set_result((result, version, content))

    read_file_with_callback(url, on_read_file_cb)

    return await asyncio.wrap_future(f)


def read_file(url: str) -> Tuple[Result, str, Content]:
    """Read a file. **(Blocking version.)**

    Args:
        url (str): Url.

    Returns:
        (Tuple[omni.client.Result, str, omni.client.Content]): Result, version and content. 
    """

    ret = None

    def on_read_file_cb(result, version, content):
        nonlocal ret
        ret = (result, version, content)

    read_file_with_callback(url, on_read_file_cb).wait()

    return ret


async def write_file_async(url: str, buffer) -> Tuple[Result]:
    """Asynchronously create a new file, overwriting if it already exists. **(Coroutine version.)**

    Args:
        url (str): Url.
        content (buffer): Content

    Returns:
        (omni.client.Result): Result object.
    """

    f = concurrent.futures.Future()

    def on_write_file_cb(result):
        if not f.done():
            f.set_result(result)

    write_file_with_callback(url, buffer, on_write_file_cb)

    return await asyncio.wrap_future(f)


def write_file(url: str, buffer) -> Tuple[Result]:
    """Create a new file, overwriting if it already exists. **(Blocking version.)**

    Args:
        url (str): Url.
        content (buffer): Content

    Returns:
        (omni.client.Result): Result object.
    """

    ret = None

    def on_write_file_cb(result):
        nonlocal ret
        ret = result

    write_file_with_callback(url, buffer, on_write_file_cb).wait()

    return ret


async def get_acls_async(url: str) -> Tuple[Result, List[AclEntry]]:
    """"Get the ACL's (permissions lists) on a folder/file.  **(Coroutine version.)**

    Args:
        url (str): Url.
        callback (Callable[omni.client.Result, List[AclEntry]): Callback to be called with the result and current ACLs.

    Returns:
        (omni.client.Request): Request object.
    """

    f = concurrent.futures.Future()

    def on_get_acl_cb(result, acls):
        if not f.done():
            f.set_result((result, acls))

    get_acls_with_callback(url, on_get_acl_cb)

    return await asyncio.wrap_future(f)


def get_acls(url: str) -> Tuple[Result, List[AclEntry]]:
    """Get the ACL's (permissions lists) on a folder/file. **(Blocking version.)**

    Args:
        url (str): Url.
        callback (Callable[omni.client.Result, List[AclEntry]): Callback to be called with the result and current ACLs.

    Returns:
        (omni.client.Request): Request object.
    """

    ret = None

    def on_get_acl_cb(result, acls):
        nonlocal ret
        ret = (result, acls)

    get_acls_with_callback(url, on_get_acl_cb).wait()

    return ret


async def set_acls_async(url: str, acls: List[AclEntry]) -> Tuple[Result]:
    """Set the ACL's on a folder/file. **(Coroutine version.)**

    Args:
        url (str): Url.
        acls (List[AclEntry]): The complete new set of ACLs to put on this file/folder.
        callback (Callable[omni.client.Result, List[AclEntry]): Callback to be called with the result and current ACLs.

    Returns:
        (omni.client.Request): Request object.
    """

    f = concurrent.futures.Future()

    def on_set_acl_cb(result):
        if not f.done():
            f.set_result((result))

    set_acls_with_callback(url, acls, on_set_acl_cb)

    return await asyncio.wrap_future(f)


def set_acls(url: str, acls: List[AclEntry]) -> Tuple[Result]:
    """Set the ACL's on a folder/file. **(Blocking version.)**

    Args:
        url (str): Url.
        acls (List[AclEntry]): The complete new set of ACLs to put on this file/folder.
        callback (Callable[omni.client.Result, List[AclEntry]): Callback to be called with the result and current ACLs.

    Returns:
        (omni.client.Request): Request object.
    """

    ret = None

    def on_set_acl_cb(result):
        nonlocal ret
        ret = result

    set_acls_with_callback(url, acls, on_set_acl_cb).wait()

    return ret


async def send_message_async(join_request_id: int, buffer) -> Tuple[Result]:
    """Asynchronously send message to channel. **(Coroutine version.)**

    Args:
        join_request_id (int): Join request id you get from omni.client.join_channel_with_callback.
        content (buffer): Content

    Returns:
        (omni.client.Result): Result object.
    """

    f = concurrent.futures.Future()

    def on_send_message_cb(result):
        if not f.done():
            f.set_result(result)

    send_message_with_callback(join_request_id, buffer, on_send_message_cb)

    return await asyncio.wrap_future(f)


async def list_checkpoints_async(url: str) -> Tuple[Result, Tuple[ListEntry]]:
    """Asynchronously list checkpoints of a file. **(Coroutine version.)**

        Args:
            url (str): Url.
        Returns:
            (Tuple[Result, Tuple[omni.client.ListEntry]]]): Result and tuple of entries.
    """

    f = concurrent.futures.Future()

    def on_list_checkpoints_cb(result, entries):
        if not f.done():
            f.set_result((result, entries))

    list_checkpoints_with_callback(url, on_list_checkpoints_cb)

    return await asyncio.wrap_future(f)


def list_checkpoints(url: str) -> Tuple[Result, Tuple[ListEntry]]:
    """Synchronously list checkpoints of a file. **(Blocking version.)**

        Args:
            url (str): Url.
        Returns:
            (Tuple[Result, Tuple[omni.client.ListEntry]]]): Result and tuple of entries.
    """

    ret = None

    def on_list_checkpoints_cb(result, entries):
        nonlocal ret
        ret = (result, entries)

    list_checkpoints_with_callback(url, on_list_checkpoints_cb).wait()

    return ret


async def create_checkpoint_async(url: str, comment: str, force: bool = False) -> Result:
    """Asynchronously create a checkpoint. **(Coroutine version.)**

            Args:
                url (str): Url.
                comment (str): Comment to associate with the checkpoint
                force (bool): Set to true to force creating a checkpoint even if there are no changes

            Returns:
                (Tuple[omni.client.Result, CheckpointQuery]): Result and query that can be used to refer to this checkpoint
    """

    f = concurrent.futures.Future()

    def on_create_checkpoint_cb(result, checkpoint_query):
        if not f.done():
            f.set_result((result, checkpoint_query))

    create_checkpoint_with_callback(url, comment, force, on_create_checkpoint_cb)

    return await asyncio.wrap_future(f)


def create_checkpoint(url: str, comment: str, force: bool = False) -> Result:
    """Synchronously create a checkpoint. **(Blocking version.)**

            Args:
                url (str): Url.
                comment (str): Comment to associate with the checkpoint
                force (bool): Set to true to force creating a checkpoint even if there are no changes

            Returns:
                (Tuple[omni.client.Result, CheckpointQuery]): Result and query that can be used to refer to this checkpoint
    """

    ret = None

    def on_create_checkpoint_cb(result, checkpoint_query):
        nonlocal ret
        ret = (result, checkpoint_query)

    create_checkpoint_with_callback(url, comment, force, on_create_checkpoint_cb).wait()

    return ret
