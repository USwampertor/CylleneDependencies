// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Carbonite thread utilities.
#pragma once

#include "../Defines.h"

#include "../extras/ScopeExit.h"
#include "../math/Util.h"
#include "../process/Util.h"
#include "../profiler/IProfiler.h"

#if CARB_PLATFORM_WINDOWS
#    include "../CarbWindows.h"
#    include "../extras/Unicode.h"
#elif CARB_POSIX
#    include <sys/syscall.h>

#    include <pthread.h>
#    include <sched.h>
#    include <unistd.h>
#    include <time.h>
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

#if CARB_PLATFORM_MACOS
#    pragma push_macro("min")
#    pragma push_macro("max")
#    undef min
#    undef max
#    include <mach/thread_policy.h>
#    include <mach/thread_act.h>
#    pragma pop_macro("max")
#    pragma pop_macro("min")
#endif

#include <atomic>
#include <thread>

namespace carb
{

namespace thread
{

/** The type for a process ID. */
using ProcessId = process::ProcessId;

/** The type for a thread ID. */
using ThreadId = uint32_t;

/**
 * Each entry in the vector is a bitmask for a set of CPUs.
 *
 * On Windows each entry corresponds to a Processor Group.
 *
 * On Linux the entries are contiguous, like cpu_set_t.
 */
using CpuMaskVector = std::vector<uint64_t>;

/** The number of CPUs represented by an individual cpu mask. */
constexpr uint64_t kCpusPerMask = std::numeric_limits<CpuMaskVector::value_type>::digits;


#if CARB_PLATFORM_WINDOWS
static_assert(sizeof(ThreadId) >= sizeof(DWORD), "ThreadId type is too small");
#elif CARB_POSIX
static_assert(sizeof(ThreadId) >= sizeof(pid_t), "ThreadId type is too small");
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

/** The printf format macro to print a thread ID. */
#define OMNI_PRItid PRIu32

/** The printf format macro to print a thread ID in hexadecimal. */
#define OMNI_PRIxtid PRIx32

#if CARB_PLATFORM_WINDOWS
#    ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail
{
const DWORD MS_VC_EXCEPTION = 0x406D1388;
#        pragma pack(push, 8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType;
    LPCSTR szName;
    DWORD dwThreadID;
    DWORD dwFlags;
} THREADNAME_INFO;
#        pragma pack(pop)

inline void setDebuggerThreadName(DWORD threadId, LPCSTR name)
{
    // Do it the old way, which is only really useful if the debugger is running
    if (::IsDebuggerPresent())
    {
        detail::THREADNAME_INFO info;
        info.dwType = 0x1000;
        info.szName = name;
        info.dwThreadID = threadId;
        info.dwFlags = 0;
#        pragma warning(push)
#        pragma warning(disable : 6320 6322)
        __try
        {
            ::RaiseException(detail::MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
        }
        __except (CARBWIN_EXCEPTION_EXECUTE_HANDLER)
        {
        }
#        pragma warning(pop)
    }
}

} // namespace detail
#    endif

//! The definition of a NativeHandleType. On Windows this is a `HANDLE` and on Linux it is a `pthread_t`.
using NativeHandleType = HANDLE;
#elif CARB_POSIX
//! The definition of a NativeHandleType. On Windows this is a `HANDLE` and on Linux it is a `pthread_t`.
using NativeHandleType = pthread_t;

#    ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail
{

inline int readIntFromFile(const char* file) noexcept
{
    auto fd = ::open(file, O_RDONLY, 0);
    if (fd == -1)
    {
        return -1;
    }

    char buffer[64];
    auto size = CARB_RETRY_EINTR(::read(fd, buffer, sizeof(buffer)));
    ::close(fd);

    if (size <= 0)
    {
        return -1;
    }

    buffer[size] = '\0';
    return std::atoi(buffer);
}

inline int readDockerCpuLimit() noexcept
{
    // See:
    // https://docs.docker.com/config/containers/resource_constraints/#cpu
    // https://engineering.squarespace.com/blog/2017/understanding-linux-container-scheduling
    const auto cfsQuota = readIntFromFile("/sys/fs/cgroup/cpu/cpu.cfs_quota_us");
    const auto cfsPeriod = readIntFromFile("/sys/fs/cgroup/cpu/cpu.cfs_period_us");
    if (cfsQuota > 0 && cfsPeriod > 0)
    {
        // Since we can have fractional CPUs, round up half a CPU to a whole CPU, but make sure we have an even number.
        return ::carb_max(1, (cfsQuota + (cfsPeriod / 2)) / cfsPeriod);
    }
    return -1;
}

} // namespace detail
#    endif
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

/**
 * Sets the name of the given thread.
 *
 * @note The length of the name is limited by the system.
 *
 * @param h The native handle to the thread.
 * @param name The desired name for the thread.
 *
 * @note On Mac OS, it is not possible to name a thread that is not the current
 *       executing thread.
 */
inline void setName(NativeHandleType h, const char* name)
{
#if CARB_PLATFORM_WINDOWS
    // Emulate CARB_NAME_THREAD but don't include Profile.h which would create a circular dependency.
    if (g_carbProfiler)
        g_carbProfiler->nameThreadDynamic(::GetThreadId(h), "%s", name);
    // SetThreadDescription is only available starting with Windows 10 1607
    using PSetThreadDescription = HRESULT(CARBWIN_WINAPI*)(HANDLE, PCWSTR);
    static PSetThreadDescription SetThreadDescription =
        (PSetThreadDescription)::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription");
    if (SetThreadDescription)
    {
        bool b = CARBWIN_SUCCEEDED(SetThreadDescription(h, extras::convertUtf8ToWide(name).c_str()));
        CARB_UNUSED(b);
        CARB_ASSERT(b);
    }
    else
    {
        detail::setDebuggerThreadName(::GetThreadId(h), name);
    }
#elif CARB_PLATFORM_LINUX
    if (h == pthread_self())
    {
        // Emulate CARB_NAME_THREAD but don't include Profile.h which would create a circular dependency.
        if (g_carbProfiler)
            g_carbProfiler->nameThreadDynamic(0, "%s", name);
    }
    if (pthread_setname_np(h, name) != 0)
    {
        // This is limited to 16 characters including NUL according to the man page.
        char buffer[16];
        strncpy(buffer, name, 15);
        buffer[15] = '\0';
        pthread_setname_np(h, buffer);
    }
#elif CARB_PLATFORM_MACOS
    if (h == pthread_self())
    {
        pthread_setname_np(name);
    }

    // not possible to name an external thread on mac
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/**
 * Retrieves the name of the thread previously set with setName().
 *
 * @note The length of the name is limited by the system.
 *
 * @param h The native handle to the thread.
 * @return The name of the thread.
 */
inline std::string getName(NativeHandleType h)
{
#if CARB_PLATFORM_WINDOWS
    // GetThreadDescription is only available starting with Windows 10 1607
    using PGetThreadDescription = HRESULT(CARBWIN_WINAPI*)(HANDLE, PWSTR*);
    static PGetThreadDescription GetThreadDescription =
        (PGetThreadDescription)::GetProcAddress(::GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription");
    if (GetThreadDescription)
    {
        PWSTR threadName;
        if (CARBWIN_SUCCEEDED(GetThreadDescription(h, &threadName)))
        {
            std::string s = extras::convertWideToUtf8(threadName);
            ::LocalFree(threadName);
            return s;
        }
    }
    return std::string();
#elif CARB_PLATFORM_LINUX || CARB_PLATFORM_MACOS
    char buffer[64];
    if (pthread_getname_np(h, buffer, CARB_COUNTOF(buffer)) == 0)
    {
        return std::string(buffer);
    }
    return std::string();
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/**
 * Sets the CPU affinity for the given thread handle
 *
 * Each bit represents a logical CPU; bit 0 for CPU 0, bit 1 for CPU 1, etc.
 *
 * @param h The native handle to the thread
 * @param mask The bitmask representing the desired CPU affinity. Zero (no bits set) is ignored.
 *
 * @note On Mac OS, the CPU affinity works differently than on other systems.
 *       The mask is treated as a unique ID for groups of threads that should run
 *       on the same core, rather than specific CPUs.
 *       For single CPU masks, this will function similarly to other systems
 *       (aside from the fact that the specific core the threads are running on
 *       being different).
 *
 * @note M1 Macs do not support thread affinity so this will do nothing on those systems.
 */
inline void setAffinity(NativeHandleType h, size_t mask)
{
#if CARB_PLATFORM_WINDOWS
    ::SetThreadAffinityMask(h, mask);
#elif CARB_PLATFORM_LINUX
    // From the man page: The cpu_set_t data type is implemented as a bit mask. However, the data structure should be
    // treated as opaque: all manipulation of the CPU sets should be done via the macros described in this page.
    if (!mask)
        return;

    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    static_assert(sizeof(cpuSet) >= sizeof(mask), "Invalid assumption: use CPU_ALLOC");

    do
    {
        int bit = __builtin_ctz(mask);
        CPU_SET(bit, &cpuSet);
        mask &= ~(size_t(1) << bit);
    } while (mask != 0);

    pthread_setaffinity_np(h, sizeof(cpu_set_t), &cpuSet);
#elif CARB_PLATFORM_MACOS
    thread_affinity_policy policy{ static_cast<integer_t>(mask) };
    thread_policy_set(pthread_mach_thread_np(h), THREAD_AFFINITY_POLICY, reinterpret_cast<thread_policy_t>(&policy),
                      THREAD_AFFINITY_POLICY_COUNT);
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/**
 * Sets the CPU Affinity for the thread.
 *
 * On Windows each entry in the CpuMaskVector represents a Processor Group. Each thread can only belong to a single
 * Processor Group, so this function will only set the CPU Affinity to the first non-zero entry in the provided
 * CpuMaskVector. That is to say, if both \c masks[0] and \c masks[1] both have bits sets, only the CPUs in \c masks[0]
 * will be set for the affinity.
 *
 * On Linux, the CpuMaskVector is analogous to a cpu_set_t. There are no restrictions on the number of CPUs that the
 * affinity mask can contain.
 *
 * @param h The thread to set CPU Affinity for.
 * @param masks Affinity masks to set.
 *
 * @return True if the function succeeded, false otherwise. If \c masks is empty, or has no bits set, false will be
 * returned. If the underlying function for setting affinity failed, then \c errno or \c last-error will be set.
 *
 * @note On Mac OS, the CPU affinity works differently than on other systems.
 *       The mask is treated as a unique ID for groups of threads that should run
 *       on cores that share L2 cache, rather than specific CPUs.
 *       For single CPU masks, this will function somewhat similarly to other
 *       systems, but threads won't be pinned to a specific core.
 */
inline bool setAffinity(NativeHandleType h, const CpuMaskVector& masks)
{
    if (masks.empty())
    {
        return false;
    }
#if CARB_PLATFORM_WINDOWS
    // Find the lowest mask with a value set. That is the CPU Group that we'll set the affinity for.
    for (uint64_t i = 0; i < masks.size(); ++i)
    {
        if (masks[i])
        {
            CARBWIN_GROUP_AFFINITY affinity{};
            affinity.Group = (WORD)i;
            affinity.Mask = masks[i];

            return ::SetThreadGroupAffinity(h, (const GROUP_AFFINITY*)&affinity, nullptr);
        }
    }

    // Would only reach here if no affinity mask had a cpu set.
    return false;
#elif CARB_PLATFORM_LINUX
    uint64_t numCpus = kCpusPerMask * masks.size();

    cpu_set_t* cpuSet = CPU_ALLOC(numCpus);
    if (!cpuSet)
    {
        return false;
    }

    CARB_SCOPE_EXIT
    {
        CPU_FREE(cpuSet);
    };

    CPU_ZERO_S(CPU_ALLOC_SIZE(numCpus), cpuSet);

    for (uint64_t i = 0; i < masks.size(); ++i)
    {
        CpuMaskVector::value_type mask = masks[i];
        while (mask != 0)
        {
            int bit = cpp20::countr_zero(mask);
            CPU_SET(bit + (i * kCpusPerMask), cpuSet);
            mask &= ~(CpuMaskVector::value_type(1) << bit);
        }
    }

    if (pthread_setaffinity_np(h, CPU_ALLOC_SIZE(numCpus), cpuSet) != 0)
    {
        return false;
    }
    else
    {
        return true;
    }
#elif CARB_PLATFORM_MACOS
    size_t mask = 0;
    for (uint64_t i = 0; i < masks.size(); ++i)
    {
        mask |= 1ULL << masks[i];
    }

    setAffinity(h, mask);

    return true;
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/**
 * Gets the current CPU Affinity for the thread.
 *
 * On Windows each entry in the CpuMaskVector represents a Processor Group.
 * On Linux, the CpuMaskVector is analogous to a cpu_set_t.
 *
 * @param h The thread to get CPU Affinity for.
 *
 * @return A CpuMaskVector containing the cpu affinities for the thread. If the underlying functions to get thread
 * affinity return an error, the returned CpuMaskVector will be empty and \c errno or \c last-error will be set.
 *
 * @note M1 Macs do not support thread affinity so this will always return an
 *       empty vector.
 */
inline CpuMaskVector getAffinity(NativeHandleType h)
{
    CpuMaskVector results;
#if CARB_PLATFORM_WINDOWS
    CARBWIN_GROUP_AFFINITY affinity;
    if (!::GetThreadGroupAffinity(h, (PGROUP_AFFINITY)&affinity))
    {
        return results;
    }

    results.resize(affinity.Group + 1, 0);
    results.back() = affinity.Mask;

    return results;
#elif CARB_PLATFORM_LINUX
    // Get the current affinity
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    if (pthread_getaffinity_np(h, sizeof(cpu_set_t), &cpuSet) != 0)
    {
        return results;
    }

    // Convert the cpu_set_t to a CpuMaskVector
    results.reserve(sizeof(cpu_set_t) / sizeof(CpuMaskVector::value_type));
    CpuMaskVector::value_type* ptr = reinterpret_cast<CpuMaskVector::value_type*>(&cpuSet);
    for (uint64_t i = 0; i < (sizeof(cpu_set_t) / sizeof(CpuMaskVector::value_type)); i++)
    {
        results.push_back(ptr[i]);
    }

    return results;
#elif CARB_PLATFORM_MACOS
    boolean_t def = false; // if the value retrieved was the default
    mach_msg_type_number_t count = 0; // the length of the returned struct in integer_t
    thread_affinity_policy policy{ 0 };

    int res = thread_policy_get(
        pthread_mach_thread_np(h), THREAD_AFFINITY_POLICY, reinterpret_cast<thread_policy_t>(&policy), &count, &def);

    if (res != 0 || def)
    {
        return results;
    }

    for (uint64_t i = 0; i < (sizeof(policy.affinity_tag) * CHAR_BIT); i++)
    {
        if ((policy.affinity_tag & (1ULL << i)) != 0)
        {
            results.push_back(i);
        }
    }

    return results;
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/**
 * Similar to `std::thread::hardware_concurrency()`, but pays attention to docker cgroup config and CPU limits.
 *
 * Docker container CPU limits are based on the ratio of `/sys/fs/cgroup/cpu/cpu.cfs_quota_us` to
 * `/sys/fs/cgroup/cpu/cpu.cfs_period_us`. Fractional CPUs of a half or larger will round up to a full CPU. It is
 * possible to have an odd number reported by this function.
 * Examples:
 *  * Docker `--cpus="3.75"` will produce `4` (rounds fractional up)
 *  * Docker `--cpus="3.50"` will produce `4` (rounds fractional up)
 *  * Docker `--cpus="3.25"` will produce `3` (rounds fractional down)
 *  * Docker `--cpus="0.25"` will produce `1` (minimum of 1)
 *
 * @returns The number of CPUs available on the current system or within the current container, if applicable.
 */
inline unsigned hardware_concurrency() noexcept
{
#if CARB_PLATFORM_LINUX
    static auto dockerLimit = detail::readDockerCpuLimit();
    if (dockerLimit > 0)
    {
        return unsigned(dockerLimit);
    }
#endif
    return std::thread::hardware_concurrency();
}

} // namespace thread

/**
 * Namespace for utilities that operate on the current thread specifically.
 */
namespace this_thread
{

/**
 * A simple sleep for the current thread that does not include the overhead of `std::chrono`.
 *
 * @param microseconds The number of microseconds to sleep for
 */
inline void sleepForUs(uint32_t microseconds) noexcept
{
#if CARB_PLATFORM_WINDOWS
    ::Sleep(microseconds / 1000);
#elif CARB_POSIX
    uint64_t nanos = uint64_t(microseconds) * 1'000;
    struct timespec rem, req{ time_t(nanos / 1'000'000'000), long(nanos % 1'000'000'000) };
    while (nanosleep(&req, &rem) != 0 && errno == EINTR)
        req = rem; // Complete remaining sleep
#else
    CARB_PLATFORM_UNSUPPORTED()
#endif
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail
{

inline unsigned contentionSpins()
{
    // These must be power-of-two-minus-one so that they function as bitmasks
    constexpr static unsigned kSpinsMax = 128 - 1;
    constexpr static unsigned kSpinsMin = 32 - 1;

    // Use randomness to prevent threads from resonating at the same frequency and permanently contending. Use a
    // simple LCG for randomness.
    static std::atomic_uint _seed; // Use random initialization value as the starting seed
    unsigned int next = _seed.load(std::memory_order_relaxed);
    _seed.store(next * 1103515245 + 12345, std::memory_order_relaxed);
    return ((next >> 24) & kSpinsMax) | kSpinsMin;
}

// This function name breaks naming paradigms so that it shows up prominently in stack traces. As the name implies, this
// function waits until f() returns true.
template <class Func>
void __CONTENDED_WAIT__(Func&& f) noexcept(noexcept(f()))
{
    constexpr static uint32_t kSleepTimeInitialUs(500);
    constexpr static uint32_t kSleepTimeMaxUs(500000);

    uint32_t sleepTimeUs(kSleepTimeInitialUs);
    unsigned spins = contentionSpins();
    for (;;)
    {
        if (CARB_LIKELY(f()))
        {
            return;
        }

        CARB_HARDWARE_PAUSE();
        if (--spins == 0)
        {
            // Serious contention signals the need to back off. Sleep for a bit and refresh spin count.
            sleepForUs(sleepTimeUs);

            spins = contentionSpins();

            // Increase sleep time exponentially
            sleepTimeUs *= 2;
            if (sleepTimeUs > kSleepTimeMaxUs)
            {
                sleepTimeUs = kSleepTimeMaxUs;
            }
        }
    }
}

} // namespace detail
#endif

/**
 * Returns the native handle for the current thread.
 *
 * @note Windows: This handle is not unique to the thread but instead is a pseudo-handle representing "current thread".
 * To obtain a unique handle to the current thread, use the Windows API function `DuplicateHandle()`.
 *
 * @return The native handle for the current thread.
 */
inline thread::NativeHandleType get()
{
#if CARB_PLATFORM_WINDOWS
    return ::GetCurrentThread();
#elif CARB_POSIX
    return pthread_self();
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}


/**
 * Returns the ID of the currently executing process.
 * @returns The current ID of the process.
 */
CARB_DEPRECATED("Use this_process::getId() instead") static inline thread::ProcessId getProcessId()
{
    return this_process::getId();
}

/**
 * Get the ID of the currently executing process.
 * @note Linux: This value is cached, so this can be unsafe if you are using fork() or clone() without calling exec()
 * after. This should be safe if you're only using @ref carb::launcher::ILauncher to launch processes.
 * @returns The current ID of the process.
 */
CARB_DEPRECATED("Use this_process::getIdCached() instead") static inline thread::ProcessId getProcessIdCached()
{
    return this_process::getIdCached();
}

/**
 * Retrieve the thread ID for the current thread.
 * @return The thread ID for the current thread.
 */
inline thread::ThreadId getId()
{
#if CARB_PLATFORM_WINDOWS
    return thread::ThreadId(::GetCurrentThreadId());
#elif CARB_PLATFORM_LINUX
    // This value is stored internally within the pthread_t, but this is opaque and there is no public API for
    // retrieving it. Therefore, we can only do this for the current thread.
    // NOTE: We do not store this in a thread_local because on older versions of glibc (especially 2.17, which is what
    // Centos7 uses), this will require a lock that is also shared with loading shared libraries, which can cause a
    // deadlock.
    thread::ThreadId tid = (thread::ThreadId)(pid_t)syscall(SYS_gettid);
    return tid;
#elif CARB_PLATFORM_MACOS
    return thread::ThreadId(pthread_mach_thread_np(pthread_self()));
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/**
 * Sets the name for the current thread.
 *
 * @note The length of the name is limited by the system and may be truncated.
 *
 * @param name The desired name for the current thread.
 */
inline void setName(const char* name)
{
    thread::setName(get(), name);
}

/**
 * Retrieves the name of the current thread.
 *
 * @return The name of the current thread.
 */
inline std::string getName()
{
    return thread::getName(get());
}

/**
 * Sets the affinity of the current thread.
 *
 * Each bit represents a logical CPU; bit 0 for CPU 0, bit 1 for CPU 1, etc.
 *
 * @note This function is limited to the first 64 CPUs in a system.
 *
 * @param mask The bitmask representing the desired CPU affinity. Zero (no bits set) is ignored.
 */
inline void setAffinity(size_t mask)
{
    thread::setAffinity(get(), mask);
}

/**
 * Sets the CPU Affinity for the current thread.
 *
 * On Windows each entry in the CpuMaskVector represents a Processor Group. Each thread can only belong to a single
 * Processor Group, so this function will only set the CPU Affinity to the first non-zero entry in the provided
 * CpuMaskVector. That is to say, if both \c masks[0] and \c masks[1] have bits sets, only the CPUs in \c masks[0]
 * will be set for the affinity.
 *
 * On Linux, the CpuMaskVector is analogous to a cpu_set_t. There are no restrictions on the number of CPUs that the
 * affinity mask can contain.
 *
 * @param masks Affinity masks to set.
 *
 * @return True if the function succeeded, false otherwise. If \c masks is empty, or has no bits set, false will be
 * returned. If the underlying function for setting affinity failed, then \c errno or \c last-error will be set.
 */
inline bool setAffinity(const thread::CpuMaskVector& masks)
{
    return thread::setAffinity(get(), masks);
}

/**
 * Gets the current CPU Affinity for the current thread.
 *
 * On Windows each entry in the CpuMaskVector represents a Processor Group.
 * On Linux, the CpuMaskVector is analogous to a cpu_set_t.
 *
 * @return A CpuMaskVector containing the cpu affinities for the thread. If the underlying functions to get thread
 * affinity return an error, the returned CpuMaskVector will be empty and \c errno or \c last-error will be set.
 */
inline thread::CpuMaskVector getAffinity()
{
    return thread::getAffinity(get());
}

/**
 * Calls a predicate repeatedly until it returns \c true.
 *
 * This function is recommended only for situations where exactly one thread is waiting on another thread. For multiple
 * threads waiting on a predicate, use \ref spinWaitWithBackoff().
 *
 * @param f The predicate to call repeatedly until it returns `true`.
 */
template <class Func>
void spinWait(Func&& f) noexcept(noexcept(f()))
{
    while (!CARB_LIKELY(f()))
    {
        CARB_HARDWARE_PAUSE();
    }
}

/**
 * Calls a predicate until it returns true with progressively increasing delays between calls.
 *
 * This function is a low-level utility for high-contention cases where multiple threads will be calling @p f
 * simultaneously, @p f needs to succeed (return `true`) before continuing, but @p f will only succeed for one thread at
 * a time. This function does not return until @p f returns `true`, at which point this function returns immediately.
 * High contention is assumed when @p f returns `false` for several calls, at which point the calling thread will
 * progressively sleep between bursts of calls to @p f. This is a back-off mechanism to allow one thread to move forward
 * while other competing threads wait their turn.
 *
 * @param f The predicate to call repeatedly until it returns `true`.
 */
template <class Func>
void spinWaitWithBackoff(Func&& f) noexcept(noexcept(f()))
{
    if (CARB_UNLIKELY(!f()))
    {
        detail::__CONTENDED_WAIT__(std::forward<Func>(f));
    }
}

/**
 * Calls a predicate until it returns true or a random number of attempts have elapsed.
 *
 * This function is a low-level utility for high-contention cases where multiple threads will be calling @p f
 * simultaneously, and @p f needs to succeed (return `true`) before continuing, but @p f will only succeed for one
 * thread at a time. This function picks a pseudo-random maximum number of times to call the function (the randomness is
 * so that multiple threads will not choose the same number and perpetually block each other) and repeatedly calls the
 * function that number of times. If @p f returns `true`, spinTryWait() immediately returns `true`.
 *
 * @param f The predicate to call repeatedly until it returns `true`.
 * @returns `true` immediately when @p f returns `true`. If a random number of attempts to call @p f all return `false`
 * then `false` is returned.
 */
template <class Func>
bool spinTryWait(Func&& f) noexcept(noexcept(f()))
{
    if (CARB_LIKELY(f()))
    {
        return true;
    }
    unsigned spins = detail::contentionSpins();
    while (--spins)
    {
        CARB_HARDWARE_PAUSE();
        if (CARB_LIKELY(f()))
        {
            return true;
        }
    }
    return false;
}


} // namespace this_thread
} // namespace carb
