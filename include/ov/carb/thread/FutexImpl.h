// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"
#include "../math/Util.h"
#include "../thread/Util.h"

#include <atomic>

#if CARB_PLATFORM_WINDOWS
#    pragma comment(lib, "synchronization.lib") // must link with synchronization.lib
#    include "../CarbWindows.h"
#elif CARB_PLATFORM_LINUX
#    include <linux/futex.h>
#    include <sys/syscall.h>
#    include <sys/time.h>

#    include <unistd.h>
#elif CARB_PLATFORM_MACOS
/* nothing for now */
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

namespace carb
{
namespace thread
{
namespace detail
{

template <class T, size_t S = sizeof(T)>
struct to_integral
{
};

template <class T>
struct to_integral<T, 1>
{
    using type = int8_t;
};

template <class T>
struct to_integral<T, 2>
{
    using type = int16_t;
};

template <class T>
struct to_integral<T, 4>
{
    using type = int32_t;
};

template <class T>
struct to_integral<T, 8>
{
    using type = int64_t;
};

template <class T>
using to_integral_t = typename to_integral<T>::type;

template <class As, class T>
CARB_NODISCARD std::enable_if_t<std::is_integral<T>::value && sizeof(As) == sizeof(T), As> reinterpret_as(const T& in) noexcept
{
    static_assert(std::is_integral<As>::value, "Must be integral type");
    return static_cast<As>(in);
}

template <class As, class T>
CARB_NODISCARD std::enable_if_t<std::is_pointer<T>::value && sizeof(As) == sizeof(T), As> reinterpret_as(const T& in) noexcept
{
    static_assert(std::is_integral<As>::value, "Must be integral type");
    return reinterpret_cast<As>(in);
}

template <class As, class T>
CARB_NODISCARD std::enable_if_t<(!std::is_pointer<T>::value && !std::is_integral<T>::value) || sizeof(As) != sizeof(T), As> reinterpret_as(
    const T& in) noexcept
{
    static_assert(std::is_integral<As>::value, "Must be integral type");
    As out{}; // Init to zero
    memcpy(&out, std::addressof(in), sizeof(in));
    return out;
}

template <class Duration>
Duration clampDuration(const Duration& offset)
{
    using namespace std::chrono;
    constexpr static Duration Max = duration_cast<Duration>(milliseconds(0x7fffffff));
    return ::carb_max(Duration(0), ::carb_min(Max, offset));
}


#if CARB_PLATFORM_WINDOWS
// Windows WaitOnAddress() supports 1, 2, 4 or 8 bytes, so it doesn't need to use ParkingLot. For testing ParkingLot
// this can be enabled though.
#    define CARB_USE_PARKINGLOT 0

using hundrednanos = std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>;

template <class T>
inline bool WaitOnAddress(const std::atomic<T>& val, T compare, int64_t* timeout) noexcept
{
    static_assert(sizeof(val) == sizeof(compare), "Invalid assumption about atomic");
    // Use the NTDLL version of this function since we can give it relative or absolute times in 100ns units
    using RtlWaitOnAddressFn = DWORD(__stdcall*)(volatile const void*, void*, size_t, int64_t*);
    static RtlWaitOnAddressFn RtlWaitOnAddress =
        (RtlWaitOnAddressFn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlWaitOnAddress");

    volatile const uint32_t* addr = reinterpret_cast<volatile const uint32_t*>(std::addressof(val));
    switch (DWORD ret = RtlWaitOnAddress(addr, &compare, sizeof(compare), timeout))
    {
        case CARBWIN_STATUS_SUCCESS:
            return true;

        default:
            CARB_FATAL_UNLESS(0, "Unexpected result from RtlWaitOnAddress: %u, GetLastError=%u", ret, ::GetLastError());
            CARB_FALLTHROUGH; // (not really, but the compiler doesn't know that CARB_FATAL_UNLESS doesn't return)
        case CARBWIN_STATUS_TIMEOUT:
            return false;
    }
}

template <class T>
inline void futex_wait(const std::atomic<T>& val, T compare)
{
    WaitOnAddress(val, compare, nullptr);
}

template <class T, class Rep, class Period>
inline bool futex_wait_for(const std::atomic<T>& val, T compare, const std::chrono::duration<Rep, Period>& duration)
{
    // RtlWaitOnAddress treats negative timeouts as positive relative time
    int64_t timeout = -std::chrono::duration_cast<detail::hundrednanos>(duration).count();
    if (timeout >= 0)
    {
        // duration to wait is negative
        return false;
    }

    CARB_ASSERT(timeout < 0);
    return WaitOnAddress(val, compare, &timeout);
}

template <class T, class Clock, class Duration>
inline bool futex_wait_until(const std::atomic<T>& val,
                             T compare,
                             const std::chrono::time_point<Clock, Duration>& time_point)
{
    int64_t absTime;
    auto now = Clock::now();

    // RtlWaitOnAddress is quite slow to return if the time has already elapsed. It's much faster for us to check first.
    if (time_point <= now)
    {
        return false;
    }

    // Constrain the time to something that is well before the heat death of the universe
    auto tp = now + clampDuration(time_point - now);

    // if ((void*)std::addressof(Clock::now) != (void*)std::addressof(std::chrono::system_clock::now))
    if (!std::is_same<Clock, std::chrono::system_clock>::value)
    {
        // If we're not using the system clock, then we need to convert to the system clock
        absTime = std::chrono::duration_cast<detail::hundrednanos>(
                      (tp - now + std::chrono::system_clock::now()).time_since_epoch())
                      .count();
    }
    else
    {
        // Already in terms of system clock
        // According to https://github.com/microsoft/STL/blob/master/stl/inc/chrono, the system_clock appears to
        // use GetSystemTimePreciseAsFileTime minus an epoch value so that it lines up with 1/1/1970 midnight GMT.
        // Unfortunately there's not an easy way to check for it here, but we have a unittest in TestSemaphore.cpp.
        absTime = std::chrono::duration_cast<detail::hundrednanos>(tp.time_since_epoch()).count();
    }

    // Epoch value from https://github.com/microsoft/STL/blob/master/stl/src/xtime.cpp
    // This is the number of 100ns units between 1 January 1601 00:00 GMT and 1 January 1970 00:00 GMT
    constexpr int64_t kFiletimeEpochToUnixEpochIn100nsUnits = 0x19DB1DED53E8000LL;
    absTime += kFiletimeEpochToUnixEpochIn100nsUnits;

    CARB_ASSERT(absTime >= 0);
    return detail::WaitOnAddress(val, compare, &absTime);
}

template <class T>
inline void futex_wake_one(std::atomic<T>& val)
{
    WakeByAddressSingle(std::addressof(val));
}

template <class T>
inline void futex_wake_n(std::atomic<T>& val, size_t n)
{
    while (n--)
        futex_wake_one(val);
}

template <class T>
inline void futex_wake_all(std::atomic<T>& val)
{
    WakeByAddressAll(std::addressof(val));
}

#    if !CARB_USE_PARKINGLOT
template <class T, size_t S = sizeof(T)>
class Futex
{
    static_assert(S == 1 || S == 2 || S == 4 || S == 8, "Unsupported size");

public:
    using AtomicType = typename std::atomic<T>;
    using Type = T;
    static inline void wait(const AtomicType& val, Type compare)
    {
        futex_wait(val, compare);
    }
    template <class Rep, class Period>
    static inline bool wait_for(const AtomicType& val, Type compare, const std::chrono::duration<Rep, Period>& duration)
    {
        return futex_wait_for(val, compare, duration);
    }
    template <class Clock, class Duration>
    static inline bool wait_until(const AtomicType& val,
                                  Type compare,
                                  const std::chrono::time_point<Clock, Duration>& time_point)
    {
        return futex_wait_until(val, compare, time_point);
    }
    static inline void notify_one(AtomicType& a)
    {
        futex_wake_one(a);
    }
    static inline void notify_n(AtomicType& a, size_t n)
    {
        futex_wake_n(a, n);
    }
    static inline void notify_all(AtomicType& a)
    {
        futex_wake_all(a);
    }
};
#    endif

#elif CARB_PLATFORM_LINUX
#    define CARB_USE_PARKINGLOT 1 // Linux only supports 4 byte futex so it must use the ParkingLot

constexpr int64_t kNsPerSec = 1'000'000'000;

inline int futex(const std::atomic_uint32_t& aval,
                 int futex_op,
                 uint32_t val,
                 const struct timespec* timeout,
                 uint32_t* uaddr2,
                 int val3) noexcept
{
    static_assert(sizeof(aval) == sizeof(uint32_t), "Invalid assumption about atomic");
    int ret = syscall(SYS_futex, std::addressof(aval), futex_op, val, timeout, uaddr2, val3);
    return ret >= 0 ? ret : -errno;
}

inline void futex_wait(const std::atomic_uint32_t& val, uint32_t compare)
{
    for (;;)
    {
        int ret = futex(val, FUTEX_WAIT_BITSET_PRIVATE, compare, nullptr, nullptr, FUTEX_BITSET_MATCH_ANY);
        switch (ret)
        {
            case 0:
            case -EAGAIN: // Valid or spurious wakeup
                return;

            case -ETIMEDOUT:
                // Apparently on Windows Subsystem for Linux, calls to the kernel can timeout even when a timeout value
                // was not specified. Fall through.
            case -EINTR: // Interrupted by signal; loop again
                break;

            default:
                CARB_FATAL_UNLESS(0, "Unexpected result from futex(): %d/%s", -ret, strerror(-ret));
        }
    }
}

template <class Rep, class Period>
inline bool futex_wait_for(const std::atomic_uint32_t& val,
                           uint32_t compare,
                           const std::chrono::duration<Rep, Period>& duration)
{
    // Relative time
    int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(clampDuration(duration)).count();
    if (ns <= 0)
    {
        return false;
    }

    struct timespec ts;
    ts.tv_sec = time_t(ns / detail::kNsPerSec);
    ts.tv_nsec = long(ns % detail::kNsPerSec);

    // Since we're using relative time here, we can use FUTEX_WAIT_PRIVATE (see futex() man page)
    int ret = futex(val, FUTEX_WAIT_PRIVATE, compare, &ts, nullptr, 0);
    switch (ret)
    {
        case 0: // Valid wakeup
        case -EAGAIN: // Valid or spurious wakeup
        case -EINTR: // Interrupted by signal; treat as a spurious wakeup
            return true;

        default:
            CARB_FATAL_UNLESS(0, "Unexpected result from futex(): %d/%s", -ret, strerror(-ret));
            CARB_FALLTHROUGH; // (not really but the compiler doesn't know that the above won't return)
        case -ETIMEDOUT:
            return false;
    }
}

template <class Clock, class Duration>
inline bool futex_wait_until(const std::atomic_uint32_t& val,
                             uint32_t compare,
                             const std::chrono::time_point<Clock, Duration>& time_point)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    // Constrain the time to something that is well before the heat death of the universe
    auto now = Clock::now();
    auto tp = now + clampDuration(time_point - now);

    // Get the number of nanoseconds to go
    int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(tp - now).count();
    if (ns <= 0)
    {
        return false;
    }

    ts.tv_sec += time_t(ns / detail::kNsPerSec);
    ts.tv_nsec += long(ns % detail::kNsPerSec);

    // Handle rollover
    if (ts.tv_nsec >= detail::kNsPerSec)
    {
        ++ts.tv_sec;
        ts.tv_nsec -= detail::kNsPerSec;
    }

    for (;;)
    {
        // Since we're using absolute monotonic time, we use FUTEX_WAIT_BITSET_PRIVATE. See the man page for futex for
        // more info.
        int ret = detail::futex(val, FUTEX_WAIT_BITSET_PRIVATE, compare, &ts, nullptr, FUTEX_BITSET_MATCH_ANY);
        switch (ret)
        {
            case 0: // Valid wakeup
            case -EAGAIN: // Valid or spurious wakeup
                return true;

            case -EINTR: // Interrupted by signal; loop again
                break;

            default:
                CARB_FATAL_UNLESS(0, "Unexpected result from futex(): %d/%s", -ret, strerror(-ret));
                CARB_FALLTHROUGH; // (not really but the compiler doesn't know that the above won't return)
            case -ETIMEDOUT:
                return false;
        }
    }
}

inline void futex_wake_n(std::atomic_uint32_t& val, unsigned count)
{
    int ret = futex(val, FUTEX_WAKE_BITSET_PRIVATE, count, nullptr, nullptr, FUTEX_BITSET_MATCH_ANY);
    CARB_ASSERT(ret >= 0, "futex(FUTEX_WAKE) failed with errno=%d/%s", -ret, strerror(-ret));
    CARB_UNUSED(ret);
}

inline void futex_wake_one(std::atomic_uint32_t& val)
{
    futex_wake_n(val, 1);
}

inline void futex_wake_all(std::atomic_uint32_t& val)
{
    futex_wake_n(val, INT_MAX);
}
#elif CARB_PLATFORM_MACOS
#    define CARB_USE_PARKINGLOT 1

#    define UL_COMPARE_AND_WAIT 1
#    define UL_UNFAIR_LOCK 2
#    define UL_COMPARE_AND_WAIT_SHARED 3
#    define UL_UNFAIR_LOCK64_SHARED 4
#    define UL_COMPARE_AND_WAIT64 5
#    define UL_COMPARE_AND_WAIT64_SHARED 6

#    define ULF_WAKE_ALL 0x00000100
#    define ULF_WAKE_THREAD 0x00000200
#    define ULF_NO_ERRNO 0x01000000

/** Undocumented OSX futex-like call.
 *  @param     operation A combination of the UL_* and ULF_* flags.
 *  @param[in] addr      The address to wait on.
 *                       This is a 32 bit value unless UL_COMPARE_AND_WAIT64 is passed.
 *  @param     value     The address's previous value.
 *  @param     timeout   Timeout in microseconds.
 *  @returns    0 or a positive value (representing additional waiters) on success, or a negative value on error. If a
 *              negative value is returned, `errno` will be set, unless ULF_NO_ERRNO is provided, in which case the
 *              return value is the negated error value.
 */
extern "C" int __ulock_wait(uint32_t operation, void* addr, uint64_t value, uint32_t timeout);

/** Undocumented OSX futex-like call.
 *  @param      operation A combination of the UL_* and ULF_* flags.
 *  @param[in]  addr The address to wake, passed to __ulock_wait by other threads.
 *  @param      wake_value An extra value to be interpreted based on \p operation. If `ULF_WAKE_THREAD` is provided,
 *              then this is the mach_port of the specific thread to wake.
 *  @returns    0 or a positive value (representing additional waiters) on success, or a negative value on error. If a
 *              negative value is returned, `errno` will be set, unless ULF_NO_ERRNO is provided, in which case the
 *              return value is the negated error value.
 */
extern "C" int __ulock_wake(uint32_t operation, void* addr, uint64_t wake_value);


inline void futex_wait(const std::atomic_uint32_t& val, uint32_t compare)
{
    for (;;)
    {
        int rc = __ulock_wait(UL_COMPARE_AND_WAIT | ULF_NO_ERRNO,
                              const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(std::addressof(val))), compare, 0);
        if (rc >= 0)
        {
            // According to XNU source, the non-negative return value is the number of remaining waiters.
            // See ulock_wait_cleanup in sys_ulock.c
            return;
        }
        switch (-rc)
        {
            case EINTR: // According to XNU source, EINTR can be returned.
                continue;
            case ETIMEDOUT:
                CARB_FALLTHROUGH;
            case EFAULT:
                CARB_FALLTHROUGH;
            default:
                CARB_FATAL_UNLESS(0, "Unexpected result from __ulock_wait: %d/%s", -rc, strerror(-rc));
        }
    }
}

template <class Rep, class Period>
inline bool futex_wait_for(const std::atomic_uint32_t& val,
                           uint32_t compare,
                           const std::chrono::duration<Rep, Period>& duration)
{
    // Relative time
    int64_t usec = std::chrono::duration_cast<std::chrono::microseconds>(clampDuration(duration)).count();
    if (usec <= 0)
    {
        return false;
    }
    if (usec > UINT32_MAX)
    {
        usec = UINT32_MAX;
    }

    int rc = __ulock_wait(UL_COMPARE_AND_WAIT | ULF_NO_ERRNO,
                          const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(std::addressof(val))), compare, usec);
    if (rc >= 0)
    {
        // According to XNU source, the non-negative return value is the number of remaining waiters.
        // See ulock_wait_cleanup in sys_ulock.c
        return true;
    }
    switch (-rc)
    {
        case EINTR: // Treat signal interrupt as a spurious wakeup
            return true;
        case ETIMEDOUT:
            return false;
        default:
            CARB_FATAL_UNLESS(0, "Unexpected result from __ulock_wait: %d/%s", -rc, strerror(-rc));
    }
}

template <class Clock, class Duration>
inline bool futex_wait_until(const std::atomic_uint32_t& val,
                             uint32_t compare,
                             const std::chrono::time_point<Clock, Duration>& time_point)
{
    // Constrain the time to something that is well before the heat death of the universe
    auto now = Clock::now();
    auto tp = now + clampDuration(time_point - now);

    // Convert to number of microseconds from now
    int64_t usec = std::chrono::duration_cast<std::chrono::microseconds>(tp - now).count();
    if (usec <= 0)
    {
        return false;
    }
    if (usec > UINT32_MAX)
    {
        usec = UINT32_MAX;
    }

    int rc = __ulock_wait(UL_COMPARE_AND_WAIT | ULF_NO_ERRNO,
                          const_cast<uint32_t*>(reinterpret_cast<const uint32_t*>(std::addressof(val))), compare, usec);
    if (rc >= 0)
    {
        // According to XNU source, the non-negative return value is the number of remaining waiters.
        // See ulock_wait_cleanup in sys_ulock.c
        return true;
    }
    switch (-rc)
    {
        case EINTR: // Treat signal interrupt as a spurious wakeup
            return true;
        case ETIMEDOUT:
            return false;
        default:
            CARB_FATAL_UNLESS(0, "Unexpected result from __ulock_wait: %d/%s", -rc, strerror(-rc));
    }
}

inline void futex_wake_n(std::atomic_uint32_t& val, unsigned count)
{
    for (unsigned i = 0; i < count; i++)
    {
        __ulock_wake(UL_COMPARE_AND_WAIT, std::addressof(val), 0);
    }
}

inline void futex_wake_one(std::atomic_uint32_t& val)
{
    __ulock_wake(UL_COMPARE_AND_WAIT, std::addressof(val), 0);
}

inline void futex_wake_all(std::atomic_uint32_t& val)
{
    __ulock_wake(UL_COMPARE_AND_WAIT | ULF_WAKE_ALL, std::addressof(val), 0);
}
#endif

#if CARB_USE_PARKINGLOT
namespace ParkingLot
{

struct WaitEntry
{
    const void* addr;
    WaitEntry* next{ nullptr };
    WaitEntry* prev{ nullptr };
    std::atomic_uint32_t wakeup{ 0 };

    enum Bits : uint32_t
    {
        kNoBits = 0,
        kWakeBit = 1,
        kWaitBit = 2,
    };
};

class WaitBucket
{
    constexpr static size_t kLock = 1;
    union
    {
        std::atomic_size_t m_headBits{ 0 };
        WaitEntry* m_debugHead;
    };
    WaitEntry* m_tail{ nullptr };

public:
    constexpr WaitBucket() noexcept = default;

    WaitEntry* lock() noexcept
    {
        size_t val;
        this_thread::spinWaitWithBackoff([this, &val] {
            val = m_headBits.fetch_or(kLock, std::memory_order_acquire);
            // Return true if we were the one that set the kLock bit
            return (val & kLock) == 0;
        });
        return reinterpret_cast<WaitEntry*>(val);
    }

    void unlock() noexcept
    {
        m_headBits.fetch_and(~kLock, std::memory_order_release);
    }

    void appendAndUnlock(WaitEntry& e) noexcept
    {
        CARB_ASSERT(!(size_t(std::addressof(e)) & kLock)); // Pointer has lock bit set!
        e.prev = m_tail;
        e.next = nullptr;
        if (m_tail)
        {
            m_tail->next = std::addressof(e);
            m_tail = std::addressof(e);
            m_headBits.fetch_and(~kLock, std::memory_order_release); // Clear lock bit
        }
        else
        {
            m_tail = std::addressof(e);
            m_headBits.store(size_t(std::addressof(e)), std::memory_order_release); // Also clears lock bit
        }
    }

    WaitEntry* remove(WaitEntry& e) noexcept
    {
        CARB_ASSERT(!(size_t(std::addressof(e)) & kLock)); // Pointer has lock bit set!
        if (e.prev)
        {
            e.prev->next = e.next;
            if (e.next)
            {
                e.next->prev = e.prev;
            }
            else
            {
                m_tail = e.prev;
            }
        }
        else
        {
            if (e.next)
            {
                e.next->prev = nullptr;
            }
            else
            {
                m_tail = nullptr;
            }
            m_headBits.store(size_t(e.next) | kLock, std::memory_order_relaxed); // Maintains lock bit
        }
        return e.next;
    }

    void removeAndUnlock(WaitEntry& e) noexcept
    {
        CARB_ASSERT(!(size_t(std::addressof(e)) & kLock)); // Pointer has lock bit set!
        if (e.prev)
        {
            e.prev->next = e.next;
            if (e.next)
            {
                e.next->prev = e.prev;
            }
            else
            {
                m_tail = e.prev;
            }
            unlock();
        }
        else
        {
            if (e.next)
            {
                e.next->prev = nullptr;
            }
            else
            {
                m_tail = nullptr;
            }
            m_headBits.store(size_t(e.next), std::memory_order_release); // Also clears lock bit
        }
    }

    static inline WaitBucket& bucket(const void* addr) noexcept
    {
        constexpr static size_t kNumWaitBuckets = 256; // Must be a power of two
        static WaitBucket waitBuckets[kNumWaitBuckets];

        static_assert(carb::math::isPowerOf2(kNumWaitBuckets), "Invalid assumption");
        return waitBuckets[(size_t(addr) >> 5) & (kNumWaitBuckets - 1)];
    }
};

template <typename T>
inline void wait(const std::atomic<T>& val, T compare)
{
    WaitEntry entry{ std::addressof(val) };

    using I = to_integral_t<T>;

    // Check before waiting
    if (reinterpret_as<I>(val.load(std::memory_order_acquire)) != reinterpret_as<I>(compare))
    {
        return;
    }

    WaitBucket& b = WaitBucket::bucket(std::addressof(val));
    b.lock();
    b.appendAndUnlock(entry);

    // Do the wait
    if (CARB_LIKELY(reinterpret_as<I>(val.load(std::memory_order_acquire)) == reinterpret_as<I>(compare)))
    {
        futex_wait(entry.wakeup, uint32_t(WaitEntry::kNoBits));
    }

    // Remove from the list
    b.lock();
    uint32_t v = entry.wakeup.load(std::memory_order_acquire);
    if (v == 0)
    {
        b.removeAndUnlock(entry);
    }
    else
    {
        // Something else already removed us
        b.unlock();
        // Wait while the wait bit is set (this should be rare)
        if (v & WaitEntry::kWaitBit)
        {
            futex_wait(entry.wakeup, v);
        }
    }
}

template <typename T, typename Rep, typename Period>
inline bool wait_for(const std::atomic<T>& val, T compare, const std::chrono::duration<Rep, Period>& duration)
{
    WaitEntry entry{ std::addressof(val) };

    using I = to_integral_t<T>;

    // Check before waiting
    if (reinterpret_as<I>(val.load(std::memory_order_acquire)) != reinterpret_as<I>(compare))
    {
        return true;
    }

    WaitBucket& b = WaitBucket::bucket(std::addressof(val));
    b.lock();
    b.appendAndUnlock(entry);

    // Do the wait
    bool finished = true;
    if (CARB_LIKELY(reinterpret_as<I>(val.load(std::memory_order_acquire)) == reinterpret_as<I>(compare)))
    {
        finished = futex_wait_for(entry.wakeup, uint32_t(WaitEntry::kNoBits), duration);
    }

    // Remove from the list
    b.lock();
    uint32_t v = entry.wakeup.load(std::memory_order_acquire);
    if (v == 0)
    {
        b.removeAndUnlock(entry);
    }
    else
    {
        // Something else already removed us
        b.unlock();
        // Wait while the wait bit is set (this should be rare and short)
        if (v & WaitEntry::kWaitBit)
        {
            futex_wait(entry.wakeup, v);
        }
    }
    return finished;
}

template <typename T, typename Clock, typename Duration>
inline bool wait_until(const std::atomic<T>& val, T compare, const std::chrono::time_point<Clock, Duration>& time_point)
{
    WaitEntry entry{ std::addressof(val) };

    using I = to_integral_t<T>;

    // Check before waiting
    if (reinterpret_as<I>(val.load(std::memory_order_acquire)) != reinterpret_as<I>(compare))
    {
        return true;
    }

    WaitBucket& b = WaitBucket::bucket(std::addressof(val));
    b.lock();
    b.appendAndUnlock(entry);

    // Do the wait
    bool finished = true;
    if (CARB_LIKELY(reinterpret_as<I>(val.load(std::memory_order_acquire)) == reinterpret_as<I>(compare)))
    {
        finished = futex_wait_until(entry.wakeup, uint32_t(WaitEntry::kNoBits), time_point);
    }

    // Remove from the list
    b.lock();
    uint32_t v = entry.wakeup.load(std::memory_order_acquire);
    if (v == 0)
    {
        b.removeAndUnlock(entry);
    }
    else
    {
        // Something else already removed us
        b.unlock();
        // Wait while the wait bit is set (this should be rare and short)
        if (v & WaitEntry::kWaitBit)
        {
            futex_wait(entry.wakeup, v);
        }
    }
    return finished;
}

inline void notify_one(void* addr)
{
    WaitBucket& b = WaitBucket::bucket(addr);
    for (WaitEntry* e = b.lock(); e; e = e->next)
    {
        if (e->addr == addr)
        {
            e->wakeup.store(WaitEntry::kWakeBit, std::memory_order_release);
            b.removeAndUnlock(*e);
            futex_wake_one(e->wakeup);
            return;
        }
    }
    b.unlock();
}

inline void notify_n(void* addr, size_t n)
{
    if (CARB_UNLIKELY(n == 0))
    {
        return;
    }

    WaitBucket& b = WaitBucket::bucket(addr);
    WaitEntry* wake = nullptr;
    WaitEntry* end = nullptr;
    for (WaitEntry* e = b.lock(); e; /* in loop */)
    {
        if (e->addr == addr)
        {
            WaitEntry* next = b.remove(*e);
            e->next = nullptr;
            if (end)
            {
                end->next = e;
                end = e;
            }
            else
            {
                wake = end = e;
            }
            // Wakeup with the wait bit
            e->wakeup.store(WaitEntry::kWaitBit | WaitEntry::kWakeBit, std::memory_order_release);
            e = next;

            if (!--n)
            {
                break;
            }
        }
        else
        {
            e = e->next;
        }
    }
    b.unlock();

    while (wake)
    {
        WaitEntry* e = wake;
        wake = wake->next;
        // Clear the wait bit so that only the wake bit is set
        e->wakeup.store(WaitEntry::kWakeBit, std::memory_order_release);
        futex_wake_one(e->wakeup);
    }
}

inline void notify_all(void* addr)
{
    WaitBucket& b = WaitBucket::bucket(addr);
    WaitEntry* wake = nullptr;
    WaitEntry* end = nullptr;
    for (WaitEntry* e = b.lock(); e; /* in loop */)
    {
        if (e->addr == addr)
        {
            WaitEntry* next = b.remove(*e);
            e->next = nullptr;
            if (end)
            {
                end->next = e;
                end = e;
            }
            else
            {
                wake = end = e;
            }
            // Wakeup with the wait bit
            e->wakeup.store(WaitEntry::kWaitBit | WaitEntry::kWakeBit, std::memory_order_release);
            e = next;
        }
        else
        {
            e = e->next;
        }
    }
    b.unlock();

    while (wake)
    {
        WaitEntry* e = wake;
        wake = wake->next;
        // Clear the wait bit so that only the wake bit is set
        e->wakeup.store(WaitEntry::kWakeBit, std::memory_order_release);
        futex_wake_one(e->wakeup);
    }
}

} // namespace ParkingLot

// Futex types that must use the ParkingLot
template <class T, size_t S = sizeof(T)>
class Futex
{
    static_assert(S == 1 || S == 2 || S == 8, "Unsupported size");
    using AtomicType = typename std::atomic<T>;

public:
    static void wait(const AtomicType& val, T compare)
    {
        ParkingLot::wait(val, compare);
    }
    template <class Rep, class Period>
    static bool wait_for(const AtomicType& val, T compare, const std::chrono::duration<Rep, Period>& duration)
    {
        return ParkingLot::wait_for(val, compare, duration);
    }
    template <class Clock, class Duration>
    static bool wait_until(const AtomicType& val, T compare, const std::chrono::time_point<Clock, Duration>& time_point)
    {
        return ParkingLot::wait_until(val, compare, time_point);
    }
    static void notify_one(AtomicType& val)
    {
        ParkingLot::notify_one(std::addressof(val));
    }
    static void notify_n(AtomicType& val, size_t count)
    {
        ParkingLot::notify_n(std::addressof(val), count);
    }
    static void notify_all(AtomicType& val)
    {
        ParkingLot::notify_all(std::addressof(val));
    }
};

// Specialize for the raw futex size (4 bytes)
template <class T>
class Futex<T, sizeof(uint32_t)>
{
    using AtomicType = typename std::atomic<T>;

public:
    static void wait(const AtomicType& val, T compare)
    {
        futex_wait(
            *reinterpret_cast<const std::atomic_uint32_t*>(std::addressof(val)), reinterpret_as<uint32_t>(compare));
    }
    template <class Rep, class Period>
    static bool wait_for(const AtomicType& val, T compare, const std::chrono::duration<Rep, Period>& duration)
    {
        return futex_wait_for(*reinterpret_cast<const std::atomic_uint32_t*>(std::addressof(val)),
                              reinterpret_as<uint32_t>(compare), duration);
    }
    template <class Clock, class Duration>
    static bool wait_until(const AtomicType& val, T compare, const std::chrono::time_point<Clock, Duration>& time_point)
    {
        return futex_wait_until(*reinterpret_cast<const std::atomic_uint32_t*>(std::addressof(val)),
                                reinterpret_as<uint32_t>(compare), time_point);
    }
    static void notify_one(AtomicType& val)
    {
        futex_wake_one(*reinterpret_cast<std::atomic_uint32_t*>(std::addressof(val)));
    }
    static void notify_n(AtomicType& val, size_t count)
    {
        futex_wake_n(*reinterpret_cast<std::atomic_uint32_t*>(std::addressof(val)), count);
    }
    static void notify_all(AtomicType& val)
    {
        futex_wake_all(*reinterpret_cast<std::atomic_uint32_t*>(std::addressof(val)));
    }
};

#endif

} // namespace detail
} // namespace thread
} // namespace carb
