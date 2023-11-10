__copyright__ = "Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved."
__license__ = """
NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
"""

"""This module contains bindings to C++ carb::datasource interface.
"""

from ._datasource import *


def get_datasource_interface(impl="carb.datasource-file.plugin") -> IDataSource:
    """Returns cached :class:`carb.datasource.IDatasource` interface"""

    if not hasattr(get_datasource_interface, impl):
        setattr(get_datasource_interface, impl, acquire_datasource_interface(impl))
    return getattr(get_datasource_interface, impl)
