__copyright__ = "Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved."
__license__ = """
NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
"""

"""This module contains bindings to C++ carb::ITokens interface.
"""

from ._tokens import *


def get_tokens_interface() -> ITokens:
    """Returns cached :class:`carb.tokens.ITokens` interface"""

    if not hasattr(get_tokens_interface, "iface"):
        get_tokens_interface.iface = acquire_tokens_interface()
    return get_tokens_interface.iface
