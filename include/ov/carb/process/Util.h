// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Carbonite process utilities.
#pragma once

#include "../Defines.h"

#include "../extras/ScopeExit.h"

#if CARB_PLATFORM_WINDOWS
#    include "../CarbWindows.h"
#elif CARB_POSIX
#    include <unistd.h>
#    include <fcntl.h>
#    if CARB_PLATFORM_MACOS
#        include <sys/errno.h>
#        include <sys/sysctl.h>
#    endif
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

#include <vector>

namespace carb
{

//! Namespace for Carbonite process utilities.
namespace process
{

/** The type for a process ID. */
using ProcessId = uint32_t;

#if CARB_PLATFORM_WINDOWS
static_assert(sizeof(ProcessId) >= sizeof(DWORD), "ProcessId type is too small");
#elif CARB_POSIX
static_assert(sizeof(ProcessId) >= sizeof(pid_t), "ProcessId type is too small");
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

/** The printf format macro to print a process ID. */
#define OMNI_PRIpid PRIu32

/** The printf format macro to print a process ID in hexadecimal. */
#define OMNI_PRIxpid PRIx32

} // namespace process

/**
 * Namespace for utilities that operate on the current thread specifically.
 */
namespace this_process
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail
{
#    if CARB_PLATFORM_WINDOWS
// Returns the process creation time as a Windows FILETIME (number of 100ns units since Jan 1, 1600 GMT).
inline uint64_t getCreationTime()
{
    CARBWIN_FILETIME creationTime{}, exitTime{}, kernelTime{}, userTime{};
    BOOL b = ::GetProcessTimes(::GetCurrentProcess(), (LPFILETIME)&creationTime, (LPFILETIME)&exitTime,
                               (LPFILETIME)&kernelTime, (LPFILETIME)&userTime);
    CARB_ASSERT(b);
    CARB_UNUSED(b);
    return (uint64_t(creationTime.dwHighDateTime) << 32) + creationTime.dwLowDateTime;
}

// Converts a time_t (Unix epoch - seconds since Jan 1, 1970 GMT) to a Windows FILETIME
// (100ns units since Jan 1, 1600 GMT).
inline uint64_t timeTtoFileTime(time_t val)
{
    // Multiply by 10 million to convert to 100ns units, then add a constant that is the number of 100ns units between
    // Jan 1, 1600 GMT and Jan 1, 1970 GMT
    return uint64_t(val) * 10'000'000 + 116444736000000000;
}

// Parses the system startup time from the Windows event log as a Unix time (seconds since Jan 1, 1970 GMT).
// Adapted from https://docs.microsoft.com/en-us/windows/win32/eventlog/querying-for-event-source-messages
// Another possibility would be to use WMI's LastBootupTime, but it is affected by hibernation and clock sync.
inline time_t parseSystemStartupTime()
{
    // Open the system event log
    HANDLE hEventLog = ::OpenEventLogW(NULL, L"System");
    CARB_ASSERT(hEventLog);
    if (!hEventLog)
        return time_t(0);

    // Make sure to close the handle when we're finished
    CARB_SCOPE_EXIT
    {
        CloseEventLog(hEventLog);
    };

    constexpr static size_t kBufferSize = 65536; // Start with a fairly large buffer
    std::vector<uint8_t> bytes(kBufferSize);

    // A lambda that will find the "Event Log Started" record from a buffer
    auto findRecord = [](const uint8_t* bytes, DWORD bytesRead) -> const CARBWIN_EVENTLOGRECORD* {
        constexpr static wchar_t kDesiredSourceName[] = L"EventLog";
        constexpr static DWORD kDesiredEventId = 6005; // Event Log Started
        const uint8_t* const end = bytes + bytesRead;

        while (bytes < end)
        {
            auto record = reinterpret_cast<const CARBWIN_EVENTLOGRECORD*>(bytes);
            // Check the SourceName (first field after the event log record)
            auto SourceName = reinterpret_cast<const WCHAR*>(bytes + sizeof(CARBWIN_EVENTLOGRECORD));
            if (0 == memcmp(SourceName, kDesiredSourceName, sizeof(kDesiredSourceName)))
            {
                if ((record->EventID & 0xFFFF) == kDesiredEventId)
                {
                    // Found it!
                    return record;
                }
            }

            bytes += record->Length;
        }
        return nullptr;
    };

    for (;;)
    {
        DWORD dwBytesRead, dwMinimumBytesNeeded;
        if (!ReadEventLogW(hEventLog, CARBWIN_EVENTLOG_SEQUENTIAL_READ | CARBWIN_EVENTLOG_BACKWARDS_READ, 0,
                           bytes.data(), (DWORD)bytes.size(), &dwBytesRead, &dwMinimumBytesNeeded))
        {
            DWORD err = GetLastError();
            if (err == CARBWIN_ERROR_INSUFFICIENT_BUFFER)
            {
                // Insufficient buffer.
                bytes.resize(dwMinimumBytesNeeded);
            }
            else
            {
                // Error
                return time_t(0);
            }
        }
        else
        {
            if (auto record = findRecord(bytes.data(), dwBytesRead))
            {
                // Found the record!
                return time_t(record->TimeGenerated);
            }
        }
    }
}

// Gets the system startup time as a Unix time (seconds since Jan 1, 1970 GMT).
inline time_t getSystemStartupTime()
{
    static time_t systemStartupTime = parseSystemStartupTime();
    return systemStartupTime;
}
#    endif
} // namespace detail
#endif

/**
 * Returns the ID of the currently executing process.
 * @returns The current ID of the process.
 */
inline process::ProcessId getId()
{
#if CARB_PLATFORM_WINDOWS
    return GetCurrentProcessId();
#elif CARB_POSIX
    return getpid();
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/**
 * Get the ID of the currently executing process.
 * @note Linux: This value is cached, so this can be unsafe if you are using fork() or clone() without calling exec()
 * after. This should be safe if you're only using @ref carb::launcher::ILauncher to launch processes.
 * @returns The current ID of the process.
 */
inline process::ProcessId getIdCached()
{
#if CARB_PLATFORM_WINDOWS
    return GetCurrentProcessId();
#elif CARB_POSIX
    // glibc (since 2.25) does not cache the result of getpid() due to potential
    // edge cases where a fork() syscall was done without the glibc wrapper, so
    // we'll cache it here.
    static pid_t cached = getpid();
    return cached;
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/**
 * Returns an ID uniquely identifying this process at least for the uptime of the machine.
 *
 * Process IDs aren't unique; they can be reused. They are great at identifying a process at a given point in time, but
 * not on a timeline that includes the future and the past. That's what this function seeks to do: give an ultra-high
 * probability that the generated ID has never been in use on this system since the last restart.
 *
 * This function accomplishes this by combining PID with process creation time.
 *
 * On Windows, 30 bits are available for process IDs and the remaining 34 bits are used for the timestamp in 32ms units.
 * For a collision to happen, either 17.4 years would have to pass for rollover or a process would have to start, finish
 * and the process ID be reused by the system within the same 32ms unit.
 *
 * For Linux, up to 22 bits are available for process IDs (but most systems use the default of 15 bits). The remaining
 * 42 bits are used for the timestamp. The timestamp is based on the kernel frequency (ticks per second). A kernel
 * frequency of 100 will result in using 10ms units for the timestamp and the rollover period is 1,394 years. The
 * maximum resolution used for the timer will result in using 1 ms units for the timestamp and a rollover period of
 * 139.4 years. For a collision to happen, either the rollover period would have to pass or a process would have to
 * start, finish and the process ID be reused by the system within the timestamp unit (1 ms - 10 ms).
 *
 * @warning This function is frozen to maintain @rstref{ABI compatibility <abi-compatibility>} over plugins that may be
 * built at different times. Do not change the results of this function, ever! Instead, add a different function.
 *
 * @note The first call to this function within a module may be slow as additional information is obtained from the
 * system. That information is then cached and subsequent calls within the module are very fast.
 *
 * @returns A unique identifier for this process from the last restart until the system is rebooted.
 */
inline uint64_t getUniqueId()
{
#if CARB_PLATFORM_WINDOWS
    // See: https://stackoverflow.com/questions/17868218/what-is-the-maximum-process-id-on-windows
    // Range 00000000 - FFFFFFFC, but aligned to 4 bytes, so 30 significant bits
    const static DWORD pid = GetCurrentProcessId();
    // creationTime is the number of 32ms units since system startup until this process started.
    // 34 bits of 32ms units gives us ~17.4 years of time until rollover. It is highly unlikely that a process ID would
    // be reused by the system within the same 32ms timeframe that the process started.
    const static uint64_t creationTime =
        ((detail::getCreationTime() - detail::timeTtoFileTime(detail::getSystemStartupTime())) / 320'000) &
        0x3ffffffff; // mask
    CARB_ASSERT((pid & 0x3) == 0); // Test assumption
    return (uint64_t(pid) << 32) + creationTime;
#elif CARB_PLATFORM_LINUX
    // We need to retrieve this from /proc. Unfortunately, because of fork(), the PID can change but if the PID changes
    // then the creation time will change too. According to https://man7.org/linux/man-pages/man5/proc.5.html the
    // maximum value for a pid is 1<<22 or ~4 million. That gives us 42 bits for timing information.

    // NOTE: This is not thread-safe static initialization. However, this is okay because every thread in a process will
    // arrive at the same value, so it doesn't matter if multiple threads write the same value.
    static uint64_t cachedValue{};

    // Read the pid every time as it can change if we fork().
    process::ProcessId pid = getId();
    if (CARB_UNLIKELY((cachedValue >> 42) != pid))
    {
        CARB_ASSERT((pid & 0xffc00000) == 0); // Only 22 bits are used for PIDs

        // PID changed (or first time). Read the process start time from /proc
        int fd = open("/proc/self/stat", O_RDONLY);
        CARB_FATAL_UNLESS(fd != -1, "Failed to open /proc/self/stat: {%d/%s}", errno, strerror(errno));

        char buf[4096];
        ssize_t bytes = CARB_RETRY_EINTR(read(fd, buf, CARB_COUNTOF(buf) - 1));
        CARB_FATAL_UNLESS(bytes >= 0, "Failed to read from /proc/self/stat");
        CARB_ASSERT(size_t(bytes) < (CARB_COUNTOF(buf) - 1)); // We should have read everything
        close(fd);

        buf[bytes] = '\0';

        unsigned long long starttime; // time (in clock ticks) since system boot when the process started

        // See https://man7.org/linux/man-pages/man5/proc.5.html
        // the starttime value is the 22nd value so skip all of the other values.

        // Someone evil (read: me when testing) could have a space or parens as part of the binary name. So look for the
        // last close parenthesis and start from there. Hopefully no other fields get added to /proc/[pid]/stat that use
        // parentheses.
        const char* start = strrchr(buf, ')');
        CARB_ASSERT(start);
        int match = sscanf(
            start, ") %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %llu", &starttime);
        CARB_FATAL_UNLESS(match == 1, "Failed to parse process start time from /proc/self/stat");

        static long ticksPerSec = sysconf(_SC_CLK_TCK);
        long divisor;
        if (ticksPerSec <= 0)
            divisor = 1;
        else if (ticksPerSec < 1000)
            divisor = ticksPerSec;
        else
            divisor = ticksPerSec / 1000;

        // Compute the cached value.
        cachedValue = (uint64_t(pid) << 42) + ((starttime / divisor) & 0x3ffffffffff);
    }
    CARB_ASSERT(cachedValue != 0);
    return cachedValue;
#elif CARB_PLATFORM_MACOS
    // MacOS has a maximum process ID of 99998 and a minimum of 100.  This can fit into 17 bits.
    // The remaining 47 bits are used for the process creation timestamp.
    static uint64_t cachedValue{};

    process::ProcessId pid = getId();
    if (CARB_UNLIKELY((cachedValue >> 47) != pid))
    {
        struct kinfo_proc info;
        struct timeval startTime;
        int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)pid };
        size_t length = sizeof(info);
        int result;

        CARB_ASSERT((pid & 0xfffe0000) == 0); // Only 17 bits are used for PIDs.

        // retrieve the process start time.
        memset(&info, 0, sizeof(info));
        result = sysctl(mib, CARB_COUNTOF(mib), &info, &length, nullptr, 0);
        CARB_FATAL_UNLESS(result == 0, "failed to retrieve the process information.");
        startTime = info.kp_proc.p_starttime;

        // create the unique ID by converting the process creation time to a number of 10ms units
        // then adding in the process ID in the high bits.
        cachedValue = (((((uint64_t)startTime.tv_sec * 1'000'000) + startTime.tv_usec) / 10'000) & 0x7fffffffffffull) +
                      (((uint64_t)pid) << 47);
    }

    CARB_ASSERT(cachedValue != 0);
    return cachedValue;
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

} // namespace this_process
} // namespace carb
