// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//! @brief Implementation of the C++20 std::counting_semaphore class.
#pragma once

#include "../cpp20/Atomic.h"
#include "../thread/Futex.h"

#include <algorithm>
#include <thread>

namespace carb
{
/** Namespace for C++14 compatible versions of some C++20 STL classes and functions. */
namespace cpp20
{
#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail
{

#    if CARB_PLATFORM_WINDOWS
constexpr ptrdiff_t kSemaphoreValueMax = LONG_MAX;
#    else
constexpr ptrdiff_t kSemaphoreValueMax = INT_MAX;
#    endif

} // namespace detail
#endif

// Handle case where Windows.h may have defined 'max'
#pragma push_macro("max")
#undef max

/**
 * C++20-compatible counting semaphore class.
 *
 * A counting_semaphore is a lightweight synchronization primitive that can control access to a shared resource. Unlike
 * a mutex, a counting_semaphore allows more than one concurrent access to the same resource, for at least
 * `least_max_value` concurrent accessors. The program is ill-formed if `least_max_value` is negative.
 *
 * This is a C++14-compatible implementation of std::counting_semaphore from C++20 draft spec dated 11/13/2019.
 *
 * @note `sizeof(counting_semaphore)` is 8 bytes for `least_max_value > 1`. A specialization exists for
 *      `least_max_value == 1` where the size is only 1 byte, also known as @ref binary_semaphore.
 *
 * @tparam least_max_value      The maximum count value that this semaphore can reach. This
 *                              indicates the number of threads or callers that can simultaneously
 *                              successfully acquire this semaphore. May not be less than or equal to zero.
 *
 * @thread_safety This class is thread-safe. However, attempting to destruct before all threads have returned from any
 *     function (especially the wait functions) is malformed and will lead to undefined behavior.
 */
template <ptrdiff_t least_max_value = detail::kSemaphoreValueMax>
class CARB_VIZ counting_semaphore
{
    CARB_PREVENT_COPY_AND_MOVE(counting_semaphore);

public:
    /** Constructor: initializes a new semaphore object with a given count.
     *
     *  @param[in] desired  The initial count value for the semaphore.  This must be a positive
     *                      value or zero.  If set to zero, the semaphore will be 'unowned' on
     *                      creation.  If set to any other value, the semaphore will only be able
     *                      to be acquired by at most @a least_max_value minus @p desired other
     *                      threads or callers until it is released @p desired times.
     */
    constexpr explicit counting_semaphore(ptrdiff_t desired) noexcept
        : m_data(::carb_min(::carb_max(ptrdiff_t(0), desired), least_max_value))
    {
        static_assert(least_max_value >= 1, "semaphore needs a count of at least 1");
        static_assert(least_max_value <= detail::kSemaphoreValueMax, "semaphore count too high");
    }

    /**
     * Destructor
     *
     * On Linux, performs a `CARB_CHECK` to verify that no waiters are present when `*this` is destroyed.
     *
     * @note On Windows, `ExitProcess()` (or returning from `main()`) causes all threads to be terminated before
     * `atexit()` registered functions are called (and static objects are cleaned up). This has the unpleasant side
     * effect of potentially terminating threads that are waiting on a semaphore and will never get the chance to clean
     * up their waiting count. Therefore, this check is linux only.
     */
    ~counting_semaphore() noexcept
    {
#if CARB_PLATFORM_LINUX
        // Make sure we don't have any waiters when we are destroyed
        CARB_CHECK((m_data.load(std::memory_order_acquire) >> kWaitersShift) == 0, "Semaphore destroyed with waiters");
#endif
    }

    /** Retrieves the maximum count value this semaphore can reach.
     *
     *  @returns The maximum count value for this semaphore.  This will never be zero.
     *
     *  @thread_safety This call is thread safe.
     */
    static constexpr ptrdiff_t max() noexcept
    {
        return least_max_value;
    }

    /** Releases references on this semaphore and potentially wakes another waiting thread.
     *
     *  @param[in] update   The number of references to atomically increment this semaphore's
     *                      counter by.  This number of waiting threads will be woken as a
     *                      result.
     *  @return No return value.
     *
     *  @remarks This releases zero or more references on this semaphore.  If a reference is
     *           released, another waiting thread could potentially be woken and acquire this
     *           semaphore again.
     *
     *  @thread_safety This call is thread safe.
     */
    void release(ptrdiff_t update = 1) noexcept
    {
        CARB_ASSERT(update >= 0);

        uint64_t d = m_data.load(std::memory_order_relaxed), u;
        for (;;)
        {
            // The standard is somewhat unclear here. Preconditions are that update >= 0 is true and update <= max() -
            // counter is true. And it throws system_error when an exception is required. So I supposed that it's likely
            // that violating the precondition would cause a system_error exception which doesn't completely make sense
            // (I would think runtime_error would make more sense). However, throwing at all is inconvenient, as is
            // asserting/crashing/etc. Therefore, we clamp the update value here.
            u = ::carb_min(update, max() - ptrdiff_t(d & kValueMask));
            if (CARB_LIKELY(m_data.compare_exchange_weak(d, d + u, std::memory_order_release, std::memory_order_relaxed)))
                break;
        }

        // At this point, the Semaphore could be destroyed by another thread. Therefore, we shouldn't access any other
        // members (taking the address of m_data below is okay because that would not actually read any memory that
        // may be destroyed)

        // waiters with a value have been notified already by whatever thread added the value. Only wake threads that
        // haven't been woken yet.
        ptrdiff_t waiters = ptrdiff_t(d >> kWaitersShift);
        ptrdiff_t value = ptrdiff_t(d & kValueMask);
        ptrdiff_t wake = ::carb_min(ptrdiff_t(u), waiters - value);
        if (wake > 0)
        {
            // cpp20::atomic only has notify_one() and notify_all(). Call the futex system directly to wake N.
            thread::futex::wake(m_data, unsigned(size_t(wake)), unsigned(size_t(waiters)));
        }
    }

    /** Acquires a reference to this semaphore.
     *
     *  @returns No return value.
     *
     *  @remarks This blocks until a reference to this semaphore can be successfully acquired.
     *           This is done by atomically decrementing the semaphore's counter if it is greater
     *           than zero.  If the counter is zero, this will block until the counter is greater
     *           than zero.  The counter is incremented by calling release().
     *
     *  @thread_safety This call is thread safe.
     */
    void acquire() noexcept
    {
        if (CARB_LIKELY(fast_acquire(false)))
            return;

        // Register as a waiter
        uint64_t d =
            m_data.fetch_add(uint64_t(1) << kWaitersShift, std::memory_order_relaxed) + (uint64_t(1) << kWaitersShift);
        for (;;)
        {
            if ((d & kValueMask) == 0)
            {
                // Need to wait
                m_data.wait(d, std::memory_order_relaxed);

                // Reload
                d = m_data.load(std::memory_order_relaxed);
            }
            else
            {
                // Try to unregister as a waiter and grab a token at the same time
                if (CARB_LIKELY(m_data.compare_exchange_weak(d, d - 1 - (uint64_t(1) << kWaitersShift),
                                                             std::memory_order_acquire, std::memory_order_relaxed)))
                    return;
            }
        }
    }

    /** Attempts to acquire a reference to this semaphore.
     *
     *  @returns `true` if the semaphore's counter was greater than zero and it was successfully
     *           atomically decremented.  Returns `false` if the counter was zero and the
     *           semaphore could not be acquired.  This will never block even if the semaphore
     *           could not be acquired.
     *
     *  @thread_safety This call is thread safe.
     */
    bool try_acquire() noexcept
    {
        return fast_acquire(true);
    }

    /** Attempts to acquire a reference to this semaphore for a specified relative time.
     *
     *  @tparam Rep     The representation primitive type for the duration value.
     *  @tparam Period  The duration's time scale value (ie: milliseconds, seconds, etc).
     *  @param[in] duration The amount of time to try to acquire this semaphore for.  This is
     *                      specified as a duration relative to the call time.
     *  @returns `true` if the semaphore's counter was greater than zero and it was successfully
     *           atomically decremented within the specified time limit.  Returns `false` if the
     *           counter was zero and the semaphore could not be acquired within the time limit.
     *           This will only block for up to approximately the specified time limit.
     *
     *  @thread_safety This call is thread safe.
     */
    template <class Rep, class Period>
    bool try_acquire_for(const std::chrono::duration<Rep, Period>& duration) noexcept
    {
        if (CARB_LIKELY(fast_acquire(false)))
            return true;

        if (duration.count() <= 0)
            return false;

        // Register as a waiter
        uint64_t d =
            m_data.fetch_add(uint64_t(1) << kWaitersShift, std::memory_order_relaxed) + (uint64_t(1) << kWaitersShift);
        while ((d & kValueMask) != 0)
        {
            // Try to unregister as a waiter and grab a token at the same time
            if (CARB_LIKELY(m_data.compare_exchange_weak(
                    d, d - 1 - (uint64_t(1) << kWaitersShift), std::memory_order_acquire, std::memory_order_relaxed)))
                return true;
        }

        // Now we need to wait, but do it with absolute time so that we properly handle spurious futex wakeups
        auto time_point = std::chrono::steady_clock::now() + thread::detail::clampDuration(duration);
        for (;;)
        {
            if (!m_data.wait_until(d, time_point, std::memory_order_relaxed))
            {
                // Timed out. Unregister as a waiter
                m_data.fetch_sub(uint64_t(1) << kWaitersShift, std::memory_order_relaxed);
                return false;
            }
            // Reload after wait
            d = m_data.load(std::memory_order_relaxed);
            if ((d & kValueMask) != 0)
            {
                // Try to unreference as a waiter and grab a token at the same time
                if (CARB_LIKELY(m_data.compare_exchange_weak(d, d - 1 - (uint64_t(1) << kWaitersShift),
                                                             std::memory_order_acquire, std::memory_order_relaxed)))
                    return true;
            }
        }
    }

    /** Attempts to acquire a reference to this semaphore until a specified absolute time.
     *
     *  @tparam Clock       The clock to use as a time source to compare the time limit to.
     *  @tparam Duration    The duration type associated with the specified clock.
     *  @param[in] time_point   The absolute time to try to acquire this semaphore for.  This is
     *                          specified as a time point from the given clock @a Clock.
     *  @returns `true` if the semaphore's counter was greater than zero and it was successfully
     *           atomically decremented before the specified time limit.  Returns `false` if the
     *           counter was zero and the semaphore could not be acquired before the time limit.
     *           This will only block up until approximately the specified time limit.
     *
     *  @thread_safety This call is thread safe.
     */
    template <class Clock, class Duration>
    bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& time_point) noexcept
    {
        if (CARB_LIKELY(fast_acquire(false)))
            return true;

        // Register as a waiter
        uint64_t d =
            m_data.fetch_add(uint64_t(1) << kWaitersShift, std::memory_order_relaxed) + (uint64_t(1) << kWaitersShift);
        for (;;)
        {
            if ((d & kValueMask) == 0)
            {
                // Need to wait
                if (!m_data.wait_until(d, time_point, std::memory_order_relaxed))
                {
                    // Timed out. Unregister as a waiter
                    m_data.fetch_sub(uint64_t(1) << kWaitersShift, std::memory_order_relaxed);
                    return false;
                }
                // Reload after wait
                d = m_data.load(std::memory_order_relaxed);
            }
            else
            {
                // Try to unregister as a waiter and grab a token at the same time
                if (CARB_LIKELY(m_data.compare_exchange_weak(d, d - 1 - (uint64_t(1) << kWaitersShift),
                                                             std::memory_order_acquire, std::memory_order_relaxed)))
                    return true;
            }
        }
    }

#ifndef DOXYGEN_SHOULD_SKIP_THIS
protected:
    // The 32 most significant bits are the waiters; the lower 32 bits is the value of the semaphore
    CARB_VIZ cpp20::atomic_uint64_t m_data;
    constexpr static int kWaitersShift = 32;
    constexpr static unsigned kValueMask = 0xffffffff;

    CARB_ALWAYS_INLINE bool fast_acquire(bool needResolution) noexcept
    {
        uint64_t d = m_data.load(needResolution ? std::memory_order_acquire : std::memory_order_relaxed);
        for (;;)
        {
            if (uint32_t(d & kValueMask) == 0)
                return false;

            if (CARB_LIKELY(m_data.compare_exchange_weak(d, d - 1, std::memory_order_acquire, std::memory_order_relaxed)))
                return true;

            if (!needResolution)
                return false;
        }
    }
#endif
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
/** Specialization for the case of a semaphore with a maximum count of 1.  This is treated as
 *  a binary semaphore - it can only be acquired by one caller at a time.
 */
template <>
class CARB_VIZ counting_semaphore<1>
{
    CARB_PREVENT_COPY_AND_MOVE(counting_semaphore);

public:
    static constexpr ptrdiff_t max() noexcept
    {
        return 1;
    }

    constexpr explicit counting_semaphore(ptrdiff_t desired) noexcept
        : m_val(uint8_t(size_t(::carb_min(::carb_max(ptrdiff_t(0), desired), max()))))
    {
    }

    void release(ptrdiff_t update = 1) noexcept
    {
        if (CARB_UNLIKELY(update <= 0))
            return;

        CARB_ASSERT(update == 1); // precondition failure

        if (!m_val.exchange(1, std::memory_order_release))
            m_val.notify_one();
    }

    void acquire() noexcept
    {
        for (;;)
        {
            uint8_t old = m_val.exchange(0, std::memory_order_acquire);
            if (CARB_LIKELY(old == 1))
                break;

            CARB_ASSERT(old == 0); // m_val can only be 0 or 1
            m_val.wait(0, std::memory_order_relaxed);
        }
    }

    bool try_acquire() noexcept
    {
        uint8_t old = m_val.exchange(0, std::memory_order_acquire);
        CARB_ASSERT(old <= 1); // m_val can only be 0 or 1
        return old == 1;
    }

    template <class Rep, class Period>
    bool try_acquire_for(const std::chrono::duration<Rep, Period>& duration) noexcept
    {
        return try_acquire_until(std::chrono::steady_clock::now() + thread::detail::clampDuration(duration));
    }

    template <class Clock, class Duration>
    bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& time_point) noexcept
    {
        for (;;)
        {
            uint8_t old = m_val.exchange(0, std::memory_order_acquire);
            if (CARB_LIKELY(old == 1))
                return true;

            CARB_ASSERT(old == 0); // m_val can only be 0 or 1
            if (!m_val.wait_until(0, time_point, std::memory_order_relaxed))
                return false;
        }
    }

protected:
    CARB_VIZ cpp20::atomic_uint8_t m_val;
};
#endif

/** Alias for a counting semaphore that can only be acquired by one caller at a time. */
using binary_semaphore = counting_semaphore<1>;

#pragma pop_macro("max")

} // namespace cpp20
} // namespace carb
