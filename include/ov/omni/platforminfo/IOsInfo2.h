// Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Helper interface to retrieve operating system info.
 */
#pragma once

#include "IOsInfo.h"


namespace omni
{
/** Platform and operating system info namespace. */
namespace platforminfo
{

/** Forward declaration of the IOSInfo2 API object. */
class IOsInfo2;


/** Extended interface to collect and retrieve more information about the operating system.
 *  This inherits from omni::platforminfo::IOsInfo and also provides all of its functionality.
 */
class IOsInfo2_abi : public omni::core::Inherits<omni::platforminfo::IOsInfo, OMNI_TYPE_ID("omni.platforminfo.IOsInfo2")>
{
protected:
    /** [Windows] Tests whether this process is running under compatibility mode.
     *
     *  @returns `true` if this process is running in compatibility mode for an older version
     *           of Windows.  Returns `false` otherwise.  Returns `false` on all non-Windows
     *           platforms.
     *
     *  @remarks Windows 10 and up allow a way for apps to run in 'compatibility mode'.  This
     *           causes many of the OS version functions to return values that represent an
     *           older version of windows (ie: Win8.1 and earlier) instead of the actual version
     *           information.  This can allow some apps that don't fully support Win10 and up to
     *           start properly.
     */
    virtual bool isCompatibilityModeEnabled_abi() noexcept = 0;
};

} // namespace platforminfo
} // namespace omni

#define OMNI_BIND_INCLUDE_INTERFACE_DECL
#include "IOsInfo2.gen.h"

/** @copydoc omni::platforminfo::IOsInfo2_abi */
class omni::platforminfo::IOsInfo2 : public omni::core::Generated<omni::platforminfo::IOsInfo2_abi>
{
};

#define OMNI_BIND_INCLUDE_INTERFACE_IMPL
#include "IOsInfo2.gen.h"
