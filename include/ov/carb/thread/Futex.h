// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Carbonite Futex implementation.
#pragma once

#include "FutexImpl.h"

#include <atomic>

namespace carb
{
namespace thread
{

/**
 * Futex namespace.
 *
 * @warning Futex is a very low-level system; generally its use should be avoided. There are plenty of higher level
 * synchronization primitives built on top of Futex that should be used instead, such as `carb::cpp20::atomic`.
 *
 * FUTEX stands for Fast Userspace muTEX. Put simply, it's a way of efficiently blocking threads waiting for a condition
 * to become true. It is a low-level system, and a foundation for many synchronization primitives.
 *
 * Windows has a similar mechanism through WaitOnAddress. While WaitOnAddress allows the value waited on to be either
 * 1, 2, 4 or 8 bytes, Linux requires the value be 32-bit (4 bytes). For Linux sizes other than 4 bytes, detail::Futex
 * has its own simple futex implementation on top of the system futex implementation.
 *
 * Linux information: http://man7.org/linux/man-pages/man2/futex.2.html
 *
 * Windows information: https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitonaddress
 */
namespace futex
{

/**
 * Waits on a value until woken.
 *
 * The value at @p val is atomically compared with @p compare. If the values are not equal, this function returns
 * immediately. Otherwise, if the values are equal, this function sleeps the current thread. Waking is not automatic
 * when the value changes. The thread that changes the value must then call wake() to wake the waiting threads.
 *
 * @note Futexes are prone to spurious wakeups. It is the responsibility of the caller to determine whether a return
 * from wait() is spurious or valid.
 *
 * @param val The value that is read atomically. If this matches @p compare, the thread sleeps.
 * @param compare The expected value.
 */
template <class T>
inline void wait(const std::atomic<T>& val, T compare) CARB_NOEXCEPT
{
    detail::Futex<T>::wait(val, compare);
}

/**
 * Waits on a value until woken or timed out.
 *
 * The value at @p val is atomically compared with @p compare. If the values are not equal, this function returns
 * immediately. Otherwise, if the values are equal, this function sleeps the current thread. Waking is not automatic
 * when the value changes. The thread that changes the value must then call wake() to wake the waiting threads.
 *
 * @note Futexes are prone to spurious wakeups. It is the responsibility of the caller to determine whether a return
 * from wait() is spurious or valid.
 *
 * @note On Linux, interruptions by signals are treated as spurious wakeups.
 *
 * @param val The value that is read atomically. If this matches @p compare, the thread sleeps.
 * @param compare The expected value.
 * @param duration The relative time to wait.
 * @return `true` if woken legitimately or spuriously; `false` if timed out.
 */
template <class T, class Rep, class Period>
inline bool wait_for(const std::atomic<T>& val, T compare, const std::chrono::duration<Rep, Period>& duration) CARB_NOEXCEPT
{
    return detail::Futex<T>::wait_for(val, compare, duration);
}

/**
 * Waits on a value until woken or timed out.
 *
 * The value at @p val is atomically compared with @p compare. If the values are not equal, this function returns
 * immediately. Otherwise, if the values are equal, this function sleeps the current thread. Waking is not automatic
 * when the value changes. The thread that changes the value must then call wake() to wake the waiting threads.
 *
 * @note Futexes are prone to spurious wakeups. It is the responsibility of the caller to determine whether a return
 * from wait() is spurious or valid.
 *
 * @param val The value that is read atomically. If this matches @p compare, the thread sleeps.
 * @param compare The expected value.
 * @param time_point The absolute time point to wait until.
 * @return `true` if woken legitimately or spuriously; `false` if timed out.
 */
template <class T, class Clock, class Duration>
inline bool wait_until(const std::atomic<T>& val,
                       T compare,
                       const std::chrono::time_point<Clock, Duration>& time_point) CARB_NOEXCEPT
{
    return detail::Futex<T>::wait_until(val, compare, time_point);
}

/**
 * Wakes threads that are waiting in one of the @p futex wait functions.
 *
 * @note To wake all threads waiting on @p val, use wake_all().
 *
 * @param val The same value that was passed to wait(), wait_for() or wait_until().
 * @param count The number of threads to wake. To wake all threads, use wake_all().
 * @param maxCount An optimization for Windows that specifies the total number of threads that are waiting on @p addr.
 * If @p count is greater-than-or-equal-to @p maxCount then a specific API call that wakes all threads is used.
 * Ignored on Linux.
 */
template <class T>
inline void wake(std::atomic<T>& val, unsigned count, unsigned maxCount = unsigned(INT_MAX)) CARB_NOEXCEPT
{
    if (count >= maxCount)
        detail::Futex<T>::notify_all(val);
    else
        detail::Futex<T>::notify_n(val, count);
}

/**
 * Wakes one thread that is waiting in one of the @p futex wait functions.
 *
 * @param val The same value that was passed to wait(), wait_for() or wait_until().
 */
template <class T>
inline void wake_one(std::atomic<T>& val) CARB_NOEXCEPT
{
    detail::Futex<T>::notify_one(val);
}

/**
 * Wakes all threads that are waiting in one of the @p futex wait functions
 *
 * @param val The same value that was passed to wait(), wait_for() or wait_until().
 */
template <class T>
inline void wake_all(std::atomic<T>& val) CARB_NOEXCEPT
{
    detail::Futex<T>::notify_all(val);
}

} // namespace futex
} // namespace thread
} // namespace carb
