// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Helper to get the memory characteristics of a program.
 */
#pragma once

#include "../Defines.h"

#include "../logging/Log.h"

#if CARB_PLATFORM_LINUX
#    include <sys/sysinfo.h>

#    include <stdio.h>
#    include <unistd.h>
#    include <sys/time.h>
#    include <sys/resource.h>
#elif CARB_PLATFORM_MACOS
#    include <mach/task.h>
#    include <mach/mach_init.h>
#    include <mach/mach_host.h>
#    include <os/proc.h>
#    include <sys/resource.h>
#elif CARB_PLATFORM_WINDOWS
#    include "../CarbWindows.h"
#endif

namespace carb
{
namespace extras
{

/** The type of memory to query. */
enum class MemoryQueryType
{
    eAvailable, /**< The available memory on the system. */
    eTotal, /**< The total memory on the system. */
};

size_t getPhysicalMemory(MemoryQueryType type); // forward declare

/** Retrieve the physical memory usage by the current process.
 *  @returns The memory usage for the current process.
 *  @returns 0 if the operation failed.
 */
inline size_t getCurrentProcessMemoryUsage()
{
#if CARB_PLATFORM_LINUX
    unsigned long rss;
    long pageSize;
    FILE* f;
    int result;

    pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize < 0)
    {
        CARB_LOG_ERROR("failed to retrieve the page size");
        return 0;
    }

    f = fopen("/proc/self/statm", "rb");
    if (f == nullptr)
    {
        CARB_LOG_ERROR("failed to open /proc/self/statm");
        return 0;
    }

    result = fscanf(f, "%*u%lu", &rss);
    fclose(f);

    if (result != 1)
    {
        CARB_LOG_ERROR("failed to parse /proc/self/statm");
        return 0;
    }

    return rss * pageSize;
#elif CARB_PLATFORM_WINDOWS
    CARBWIN_PROCESS_MEMORY_COUNTERS mem;
    mem.cb = sizeof(mem);
    if (!K32GetProcessMemoryInfo(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&mem, sizeof(mem)))
    {
        CARB_LOG_ERROR("GetProcessMemoryInfo failed");
        return 0;
    }

    return mem.WorkingSetSize;
#elif CARB_PLATFORM_MACOS
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    task_basic_info info = {};
    kern_return_t r = task_info(mach_task_self(), TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count);
    if (r != KERN_SUCCESS)
    {
        CARB_LOG_ERROR("task_info() failed (%d)", int(r));
        return 0;
    }
    return info.resident_size;
#else
#    warning "getMemoryUsage() has no implementation"
    return 0;
#endif
}

/** Retrieves the peak memory usage information for the calling process.
 *
 *  @returns The maximum process memory usage level in bytes.
 */
inline size_t getPeakProcessMemoryUsage()
{
#if CARB_PLATFORM_WINDOWS
    CARBWIN_PROCESS_MEMORY_COUNTERS mem;
    mem.cb = sizeof(mem);
    if (!K32GetProcessMemoryInfo(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&mem, sizeof(mem)))
    {
        CARB_LOG_ERROR("GetProcessMemoryInfo failed");
        return 0;
    }

    return mem.PeakWorkingSetSize;
#elif CARB_POSIX
    rusage usage_data;
    size_t scale;
    getrusage(RUSAGE_SELF, &usage_data);

#    if CARB_PLATFORM_LINUX
    scale = 1024; // Linux provides this value in kilobytes.
#    elif CARB_PLATFORM_MACOS
    scale = 1; // MacOS provides this value in bytes.
#    else
    CARB_UNSUPPORTED_PLATFORM();
#    endif

    // convert to bytes.
    return usage_data.ru_maxrss * scale;
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/** Stores information about memory in the system.  All values are in bytes. */
struct SystemMemoryInfo
{
    /** The total available physical RAM in bytes.  This may be reported as slightly less than
     *  the expected amount due to some small amount of space being reserved for the OS.  This
     *  is often negligible, but when displaying the memory amount to the user, it is good to
     *  round to the nearest mebibyte or gibibyte.
     */
    uint64_t totalPhysical;

    /** The available physical RAM in bytes.  This is not memory that is necessarily available
     *  to the calling process, but rather the amount of physical RAM that the OS is not
     *  actively using for all running processes at the moment.
     */
    uint64_t availablePhysical;

    /** The total amount of page file space available to the system in bytes.  When physical RAM
     *  is filled, this space will be used to provide a backup space for memory pages of lower
     *  priority, inactive, or background processes so that RAM can be freed up for more active
     *  or demanding processes.
     */
    uint64_t totalPageFile;

    /** The available amount of page file space in bytes.  This is the unused portion of the page
     *  file that the system can still make use of to keep foreground processes from starving.
     */
    uint64_t availablePageFile;

    /** The total amount of virtual memory available to the calling process in bytes.  This is
     *  the maximum amount of addressable space and the maximum memory address that can be used.
     *  On Windows this value should be consistent, but on Linux, this value can change depending
     *  on the owner of the process if the administrator has set differing limits for users.
     */
    uint64_t totalVirtual;

    /** The available amount of virtual memory in bytes still usable by the calling process.  This
     *  is the amount of memory that can still be allocated by the process.  That said however,
     *  the process may not be able to actually use all of that memory if the system's resources
     *  are exhausted.  Since virtual memory space is generally much larger than the amount of
     *  available RAM and swap space on 64 bit processes, the process will still be in an out
     *  of memory situation if all of the system's physical RAM and swap space have been consumed
     *  by the process's demands.  When looking at out of memory situations, it is also important
     *  to consider virtual memory space along with physical and swap space.
     */
    uint64_t availableVirtual;
};

#if CARB_PLATFORM_LINUX
/** Retrieves the memory size multiplier from a value suffix.
 *
 *  @param[in] str  The string containing the memory size suffix.  This may not be `nullptr`.
 *  @returns The multiplier to use to convert the memory value to bytes.  Returns 1 if the
 *           memory suffix is not recognized.  Supported suffixes are 'KB', 'MB', 'GB', 'TB',
 *           'PB', 'EB', and bytes.  All unknown suffixes will be treated as bytes.
 */
inline size_t getMemorySizeMultiplier(const char* str)
{
    size_t multiplier = 1;

    // strip leading whitespace.
    while (str[0] != 0 && (str[0] == ' ' || str[0] == '\t'))
        str++;

    // check the prefix of the multiplier string (ie: "kB", "gB", etc).
    switch (tolower(str[0]))
    {
        case 'e':
            multiplier *= 1024ull; // fall through...
            CARB_FALLTHROUGH;

        case 'p':
            multiplier *= 1024ull; // fall through...
            CARB_FALLTHROUGH;

        case 't':
            multiplier *= 1024ull; // fall through...
            CARB_FALLTHROUGH;

        case 'g':
            multiplier *= 1024ull; // fall through...
            CARB_FALLTHROUGH;

        case 'm':
            multiplier *= 1024ull; // fall through...
            CARB_FALLTHROUGH;

        case 'k':
            multiplier *= 1024ull; // fall through...
            CARB_FALLTHROUGH;

        default:
            break;
    }

    return multiplier;
}

/** Retrieves a memory value by its key name in '/proc/meminfo' or other.
 *
 *  @param[in] filename The name of the pseudo file to parse the information from.  This may be
 *                      `nullptr` to use the default of '/proc/meminfo' instead.
 *  @param[in] name     The name of the key to search for.  This may not be `nullptr`.
 *  @param[in] nameLen  The length of the key name in characters.  If set to 0, the length
 *                      of the string will be calculated.
 *  @returns The value associated with the given key name if found.  Returns 0 otherwise.
 */
inline size_t getMemoryValueByName(const char* filename, const char* name, size_t nameLen = 0)
{
    constexpr const char* kProcMemInfo = "/proc/meminfo";
    FILE* fp;
    size_t bytes = 0;
    char line[256];
    size_t nameLength = nameLen;


    if (filename == nullptr)
        filename = kProcMemInfo;

    if (nameLength == 0)
        nameLength = strlen(name);

    fp = fopen(filename, "r");

    if (fp == nullptr)
        return 0;

    while (fgets(line, CARB_COUNTOF(line), fp) != nullptr)
    {
        // found the key we're looking for => parse its value in kibibytes and succeed.
        if (strncmp(line, name, nameLength) == 0)
        {
            char* endp;
            bytes = strtoull(&line[nameLength], &endp, 10);
            bytes *= getMemorySizeMultiplier(endp);
            break;
        }
    }

    fclose(fp);
    return bytes;
}
#endif

/** Retrieves the memory usage information for the system.
 *
 *  @param[out] out     Receives the memory totals and availability information for the system.
 *  @returns `true` if the memory information is successfully collected.  Returns `false`
 *           otherwise.
 */
inline bool getSystemMemoryInfo(SystemMemoryInfo& out)
{
#if CARB_PLATFORM_LINUX
    struct sysinfo info = {};
    struct rlimit limit = {};
    int result;
    size_t bytes;


    // collect the total memory counts.
    result = sysinfo(&info);

    if (result != 0)
    {
        CARB_LOG_WARN("sysinfo() returned %d", result);

        // retrieve the values from '/proc/meminfo' instead.
        out.totalPhysical = getMemoryValueByName(nullptr, "MemTotal:", sizeof("MemTotal:") - 1);
        out.totalPageFile = getMemoryValueByName(nullptr, "SwapTotal:", sizeof("SwapTotal:") - 1);
    }

    else
    {
        out.totalPhysical = (uint64_t)info.totalram * info.mem_unit;
        out.totalPageFile = (uint64_t)info.totalswap * info.mem_unit;
    }

    // get the virtual memory info.
    if (getrlimit(RLIMIT_AS, &limit) == 0)
    {
        out.totalVirtual = limit.rlim_cur;
        out.availableVirtual = 0;

        // retrieve the total VM usage for the calling process.
        bytes = getMemoryValueByName("/proc/self/status", "VmSize:", sizeof("VmSize:") - 1);

        if (bytes != 0)
        {
            if (bytes > out.totalVirtual)
            {
                CARB_LOG_WARN(
                    "retrieved a larger VM size than total VM space (!?) {bytes = %zu, "
                    "totalVirtual = %" PRIu64 "}",
                    bytes, out.totalVirtual);
            }

            else
                out.availableVirtual = out.totalVirtual - bytes;
        }
    }

    else
    {
        CARB_LOG_WARN("failed to retrieve the total address space {errno = %d/%s}", errno, strerror(errno));
        out.totalVirtual = 0;
        out.availableVirtual = 0;
    }


    // collect the available RAM as best we can.  The values in '/proc/meminfo' are typically
    // more accurate than what sysinfo() gives us due to the 'mem_unit' value.
    bytes = getMemoryValueByName(nullptr, "MemAvailable:", sizeof("MemAvailable:") - 1);

    if (bytes != 0)
        out.availablePhysical = bytes;

    else
        out.availablePhysical = (uint64_t)info.freeram * info.mem_unit;

    // collect the available swap space as best we can.
    bytes = getMemoryValueByName(nullptr, "SwapFree:", sizeof("SwapFree:") - 1);

    if (bytes != 0)
        out.availablePageFile = bytes;

    else
        out.availablePageFile = (uint64_t)info.freeswap * info.mem_unit;

    return true;
#elif CARB_PLATFORM_WINDOWS
    CARBWIN_MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx((LPMEMORYSTATUSEX)&status))
    {
        CARB_LOG_ERROR("GlobalMemoryStatusEx() failed {error = %d}", GetLastError());
        return false;
    }

    out.totalPhysical = (uint64_t)status.ullTotalPhys;
    out.totalPageFile = (uint64_t)status.ullTotalPageFile;
    out.totalVirtual = (uint64_t)status.ullTotalVirtual;
    out.availablePhysical = (uint64_t)status.ullAvailPhys;
    out.availablePageFile = (uint64_t)status.ullAvailPageFile;
    out.availableVirtual = (uint64_t)status.ullAvailVirtual;

    return true;
#elif CARB_PLATFORM_MACOS
    int mib[2];
    size_t length;
    mach_msg_type_number_t count;
    kern_return_t r;
    xsw_usage swap = {};
    task_basic_info info = {};
    struct rlimit limit = {};

    // get the system's swap usage
    mib[0] = CTL_HW, mib[1] = VM_SWAPUSAGE;
    length = sizeof(swap);
    if (sysctl(mib, CARB_COUNTOF(mib), &swap, &length, nullptr, 0) != 0)
    {
        CARB_LOG_ERROR("sysctl() for VM_SWAPUSAGE failed (errno = %d)", errno);
        return false;
    }

    count = TASK_BASIC_INFO_COUNT;
    r = task_info(mach_task_self(), TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count);
    if (r != KERN_SUCCESS)
    {
        CARB_LOG_ERROR("task_info() failed (result = %d, errno = %d)", int(r), errno);
        return false;
    }

    // it's undocumented but RLIMIT_AS is supported
    if (getrlimit(RLIMIT_AS, &limit) != 0)
    {
        CARB_LOG_ERROR("getrlimit(RLIMIT_AS) failed (errno = %d)", errno);
        return false;
    }

    out.totalVirtual = limit.rlim_cur;
    out.availableVirtual = out.totalVirtual - info.virtual_size;
    out.totalPhysical = getPhysicalMemory(MemoryQueryType::eTotal);
    out.availablePhysical = getPhysicalMemory(MemoryQueryType::eAvailable);
    out.totalPageFile = swap.xsu_total;
    out.availablePageFile = swap.xsu_avail;

    return true;
#else
#    warning "getSystemMemoryInfo() has no implementation"
    return 0;
#endif
}

/** Retrieve the physical memory available on the system.
 *  @param[in] type The type of memory to query.
 *  @returns The physical memory available on the system.
 *  @returns 0 if the operation failed.
 *  @note On Linux, disk cache memory doesn't count as available memory, so
 *        allocations may still succeed if available memory is near 0, but it
 *        will contribute to system slowdown if it's using disk cache space.
 */
inline size_t getPhysicalMemory(MemoryQueryType type)
{
#if CARB_PLATFORM_LINUX
    // this is a linux-specific system call
    struct sysinfo info;
    int result;
    const char* search;
    size_t searchLength;
    size_t bytes;

    // attempt to read the available memory from '/proc/meminfo' first.
    if (type == MemoryQueryType::eTotal)
    {
        search = "MemTotal:";
        searchLength = sizeof("MemTotal:") - 1;
    }

    else
    {
        search = "MemAvailable:";
        searchLength = sizeof("MemAvailable:") - 1;
    }

    bytes = getMemoryValueByName(nullptr, search, searchLength);

    if (bytes != 0)
        return bytes;

    // fall back to sysinfo() to get the amount of free RAM if it couldn't be found in
    // '/proc/meminfo'.
    result = sysinfo(&info);
    if (result != 0)
    {
        CARB_LOG_ERROR("sysinfo() returned %d", result);
        return 0;
    }

    if (type == MemoryQueryType::eTotal)
        return info.totalram * info.mem_unit;
    else
        return info.freeram * info.mem_unit;
#elif CARB_PLATFORM_WINDOWS
    CARBWIN_MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);

    if (!GlobalMemoryStatusEx((LPMEMORYSTATUSEX)&status))
    {
        CARB_LOG_ERROR("GlobalMemoryStatusEx failed");
        return 0;
    }

    if (type == MemoryQueryType::eTotal)
        return status.ullTotalPhys;
    else
        return status.ullAvailPhys;
#elif CARB_PLATFORM_MACOS
    int mib[2];
    size_t memSize = 0;
    size_t length;

    if (type == MemoryQueryType::eTotal)
    {
        mib[0] = CTL_HW, mib[1] = HW_MEMSIZE;
        length = sizeof(memSize);
        if (sysctl(mib, CARB_COUNTOF(mib), &memSize, &length, nullptr, 0) != 0)
        {
            CARB_LOG_ERROR("sysctl() for HW_MEMSIZE failed (errno = %d)", errno);
            return false;
        }
    }
    else
    {
        mach_msg_type_number_t count;
        kern_return_t r;
        vm_statistics_data_t vm = {};
        size_t pageSize = getpagesize();

        count = HOST_VM_INFO_COUNT;
        r = host_statistics(mach_host_self(), HOST_VM_INFO, reinterpret_cast<host_info_t>(&vm), &count);
        if (r != KERN_SUCCESS)
        {
            CARB_LOG_ERROR("host_statistics() failed (%d)", int(r));
            return false;
        }

        memSize = (vm.free_count + vm.inactive_count) * pageSize;
    }

    return memSize;
#else
#    warning "getPhysicalMemoryAvailable() has no implementation"
    return 0;
#endif
}

/** Names for the different types of common memory scales.  This covers both binary
 *  (ie: Mebibytes = 1,048,576 bytes) and decimal (ie: Megabytes = 1,000,000 bytes)
 *  size scales.
 */
enum class MemoryScaleType
{
    /** Name for the binary memory size scale.  This is commonly used in operating systems
     *  and software.  All scale names are powers of 1024 bytes and typically use the naming
     *  convention of using the suffix "bibytes" and the first two letters of the prefix of
     *  its decimal counterpart.  For example, "mebibytes" instead of "megabytes".
     */
    eBinaryScale,

    /** Name for the decimal memory size scale.  This is commonly used in hardware size
     *  descriptions.  All scale names are powers of 1000 byes and typically use the Greek
     *  size prefix followed by "Bytes".  For example, "megabytes", "petabytes", "kilobytes",
     *  etc.
     */
    eDecimalScale,
};

/** Retrieves a friendly memory size and scale suffix for a given number of bytes.
 *
 *  @param[in]  bytes   The number of bytes to convert to a friendly memory size.
 *  @param[out] suffix  Receives the name of the suffix to the converted memory size amount.
 *                      This is suitable for displaying to a user.
 *  @param[in]  scale   Whether the conversion should be done using a decimal or binary scale.
 *                      This defaults to @ref MemoryScaleType::eBinaryScale.
 *  @returns The given memory size value @p bytes converted to the next lowest memory scale value
 *           (ie: megabytes, kilobytes, etc).  The appropriate suffix string is returned through
 *           @p suffix.
 */
inline double getFriendlyMemorySize(size_t bytes, const char** suffix, MemoryScaleType scale = MemoryScaleType::eBinaryScale)
{
    constexpr size_t kEib = 1024ull * 1024 * 1024 * 1024 * 1024 * 1024;
    constexpr size_t kPib = 1024ull * 1024 * 1024 * 1024 * 1024;
    constexpr size_t kTib = 1024ull * 1024 * 1024 * 1024;
    constexpr size_t kGib = 1024ull * 1024 * 1024;
    constexpr size_t kMib = 1024ull * 1024;
    constexpr size_t kKib = 1024ull;
    constexpr size_t kEb = 1000ull * 1000 * 1000 * 1000 * 1000 * 1000;
    constexpr size_t kPb = 1000ull * 1000 * 1000 * 1000 * 1000;
    constexpr size_t kTb = 1000ull * 1000 * 1000 * 1000;
    constexpr size_t kGb = 1000ull * 1000 * 1000;
    constexpr size_t kMb = 1000ull * 1000;
    constexpr size_t kKb = 1000ull;
    constexpr size_t limits[2][6] = { { kEib, kPib, kTib, kGib, kMib, kKib }, { kEb, kPb, kTb, kGb, kMb, kKb } };
    constexpr const char* suffixes[2][6] = { { "EiB", "PiB", "TiB", "GiB", "MiB", "KiB" },
                                             { "EB", "PB", "TB", "GB", "MB", "KB" } };

    if (scale != MemoryScaleType::eBinaryScale && scale != MemoryScaleType::eDecimalScale)
    {
        *suffix = "bytes";
        return (double)bytes;
    }

    for (size_t i = 0; i < CARB_COUNTOF(limits[(size_t)scale]); i++)
    {
        size_t limit = limits[(size_t)scale][i];

        if (bytes >= limit)
        {
            *suffix = suffixes[(size_t)scale][i];
            return bytes / (double)limit;
        }
    }

    *suffix = "bytes";
    return (double)bytes;
}

} // namespace extras
} // namespace carb
