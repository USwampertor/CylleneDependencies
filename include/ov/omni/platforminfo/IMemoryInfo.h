// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Helper interface to retrieve memory info.
 */
#pragma once

#include "../core/IObject.h"


namespace omni
{
/** Platform and operating system info namespace. */
namespace platforminfo
{

/** Forward declaration of the IMemoryInfo API object. */
class IMemoryInfo;

/** Interface to collect and retrieve information about memory installed in the system. */
class IMemoryInfo_abi : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.platforminfo.IMemoryInfo")>
{
protected:
    /** Retrieves the total installed physical RAM in the system.
     *
     *  @returns The number of bytes of physical RAM installed in the system.  This value will
     *           not change during the lifetime of the calling process.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual size_t getTotalPhysicalMemory_abi() noexcept = 0;

    /** Retrieves the available physical memory in the system.
     *
     *  @returns The number of bytes of physical RAM that is currently available for use by the
     *           operating system.  Note that this is not a measure of how much memory is
     *           available to the calling process, but rather for the entire system.
     *
     *  @thread_safety This call is thread safe.  However, two consecutive or concurrent calls
     *                 are unlikely to return the same value.
     */
    virtual size_t getAvailablePhysicalMemory_abi() noexcept = 0;

    /** Retrieves the total page file space in the system.
     *
     *  @returns The number of bytes of page file space in the system.  The value will not
     *           change during the lifetime of the calling process.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual size_t getTotalPageFileMemory_abi() noexcept = 0;

    /** Retrieves the available page file space in the system.
     *
     *  @returns The number of bytes of page file space that is currently available for use
     *           by the operating system.
     *
     *  @thread_safety This call is thread safe.  However, two consecutive or concurrent calls
     *                 are unlikely to return the same value.
     */
    virtual size_t getAvailablePageFileMemory_abi() noexcept = 0;

    /** Retrieves the total memory usage for the calling process.
     *
     *  @returns The number of bytes of memory used by the calling process.  This will not
     *           necessarily be the amount of the process's virtual memory space that is
     *           currently in use, but rather the amount of memory that the OS currently
     *           has wired for this process (ie: the process's working set memory).  It is
     *           possible that the process could have a lot more memory allocated, just
     *           inactive as far as the OS is concerned.
     *
     *  @thread_safety This call is thread safe.  However, two consecutive calls are unlikely
     *                 to return the same value.
     */
    virtual size_t getProcessMemoryUsage_abi() noexcept = 0;

    /** Retrieves the peak memory usage of the calling process.
     *
     *  @returns The maximum number of bytes of memory used by the calling process.  This will
     *           not necessarily be the maximum amount of the process's virtual memory space that
     *           was ever allocated, but rather the maximum amount of memory that the OS ever had
     *           wired for the process (ie: the process's working set memory).  It is possible
     *           that the process could have had a lot more memory allocated, just inactive as
     *           far as the OS is concerned.
     */
    virtual size_t getProcessPeakMemoryUsage_abi() noexcept = 0;
};

} // namespace platforminfo
} // namespace omni

#define OMNI_BIND_INCLUDE_INTERFACE_DECL
#include "IMemoryInfo.gen.h"

/** @copydoc omni::platforminfo::IMemoryInfo_abi */
class omni::platforminfo::IMemoryInfo : public omni::core::Generated<omni::platforminfo::IMemoryInfo_abi>
{
};

#define OMNI_BIND_INCLUDE_INTERFACE_IMPL
#include "IMemoryInfo.gen.h"
