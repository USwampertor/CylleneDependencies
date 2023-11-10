// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
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

#include "../core/IObject.h"


namespace omni
{
/** Platform and operating system info namespace. */
namespace platforminfo
{

/** Forward declaration of the IOSInfo API object. */
class IOsInfo;

/** Names for the supported operating systems. */
enum class OMNI_ATTR("prefix=e") Os
{
    eUnknown, ///< The OS is unknown or could not be determined.
    eWindows, ///< Microsoft Windows.
    eLinux, ///< Any flavor of Linux.
    eMacOs, ///< Mac OS.
};

/** Names for the processor architecture for the system. */
enum class OMNI_ATTR("prefix=e") Architecture
{
    eUnknown, ///< The architecture is unknown or could not be determined.
    eX86_64, ///< Intel X86 64 bit.
    eAarch64, ///< ARM 64-bit.
};

/** A three-part operating system version number.  This includes the major, minor, and
 *  build number.  This is often expressed as "<major>.<minor>.<buildNumber>" when printed.
 */
struct OsVersion
{
    uint32_t major; ///< Major version.
    uint32_t minor; ///< Minor version.
    uint32_t buildNumber; ///< OS specific build number.
};

/** Information about the active compositor on the system. */
struct CompositorInfo
{
    const char* name; ///< The name of the active compositor.  This must not be modified.
    const char* vendor; ///< The vendor of the active compositor.  This must not be modified.
    int32_t releaseVersion; ///< The release version number of the active compositor.
};

/** Interface to collect and retrieve information about the operating system. */
class IOsInfo_abi : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.platforminfo.IOsInfo")>
{
protected:
    /** Retrieves the processor architecture for this platform.
     *
     *  @returns An architecture name.  This will never be
     *           @ref omni::platforminfo::Architecture::eUnknown.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual Architecture getArchitecture_abi() noexcept = 0;

    /** Retrieves an identifier for the current platform.
     *
     *  @returns An operating system name.  This will never be
     *           @ref omni::platforminfo::Os::eUnknown.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual Os getOs_abi() noexcept = 0;

    /** Retrieves the OS version information.
     *
     *  @returns The operating system version numbers.  These will be retrieved from the system
     *           as directly as possible.  If possible, these will not be parsed from a string
     *           version of the operating system's name.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual OsVersion getOsVersion_abi() noexcept = 0;

    /** Retrieves the name and version information for the system compositor.
     *
     *  @returns An object describing the active compositor.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual CompositorInfo getCompositorInfo_abi() noexcept = 0;

    /** Retrieves the friendly printable name of the operating system.
     *
     *  @returns A string describing the operating system.  This is retrieved from the system and
     *           does not necessarily follow any specific formatting.  This may or may not contain
     *           specific version information.  This string is intended for display to users.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual const char* getPrettyName_abi() noexcept = 0;

    /** Retrieves the name of the operating system.
     *
     *  @returns A string describing the operating system.  This is retrieved from the system if
     *           possible and does not necessarily follow any specific formatting.  This may
     *           include different information than the 'pretty' name (though still identifying
     *           the same operating system version).  This string is more intended for logging
     *           or parsing purposes than display to the user.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual const char* getName_abi() noexcept = 0;

    /** Retrieves the operating system distribution name.
     *
     *  @returns The operating system distribution name.  For Windows 10 and up, this often
     *           contains the build's version name (ie: v1909).  For Linux, this contains the
     *           distro name (ie: "Ubuntu", "Gentoo", etc).
     *
     *  @thread_safety This call is thread safe.
     */
    virtual const char* getDistroName_abi() noexcept = 0;

    /** Retrieves the operating system's build code name.
     *
     *  @returns The code name of the operating system's current version.  For Windows 10 and up,
     *           this is the Microsoft internal code name for each release (ie: "RedStone 5",
     *           "21H2", etc).  If possible it will be retrieved from the system.  If not
     *           available, a best guess will be made based on the build version number.  For
     *           Linux, this will be the build name of the current installed version (ie:
     *           "Bionic", "Xenial", etc).
     *
     *  @thread_safety This call is thread safe.
     */
    virtual const char* getCodeName_abi() noexcept = 0;

    /** Retrieves the operating system's kernel version as a string.
     *
     *  @returns A string containing the OS's kernel version information.  There is no standard
     *           layout for a kernel version across platforms so this isn't split up into a
     *           struct of numeric values.  For example, Linux kernel versions often contain
     *           major-minor-hotfix-build_number-string components whereas Mac OS is typically
     *           just major-minor-hotfix.  Windows kernel versions are also often four values.
     *           This is strictly for informational purposes.  Splitting this up into numerical
     *           components is left as an exercise for the caller if needed.
     */
    virtual const char* getKernelVersion_abi() noexcept = 0;
};

} // namespace platforminfo
} // namespace omni

#define OMNI_BIND_INCLUDE_INTERFACE_DECL
#include "IOsInfo.gen.h"

/** @copydoc omni::platforminfo::IOsInfo_abi */
class omni::platforminfo::IOsInfo : public omni::core::Generated<omni::platforminfo::IOsInfo_abi>
{
};

#define OMNI_BIND_INCLUDE_INTERFACE_IMPL
#include "IOsInfo.gen.h"
