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
//! @brief Carbonite mutex and recursive_mutex implementation.
#pragma once

#include "Futex.h"
#include "Util.h"
#include "../cpp20/Atomic.h"

#include <system_error>

#if CARB_PLATFORM_WINDOWS
#    include "../CarbWindows.h"
#endif

namespace carb
{
namespace thread
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail
{
#    if CARB_PLATFORM_WINDOWS
template <bool Recursive>
class BaseMutex
{
public:
    constexpr static bool kRecursive = Recursive;

    CARB_PREVENT_COPY_AND_MOVE(BaseMutex);

    constexpr BaseMutex() noexcept = default;
    ~BaseMutex()
    {
        CARB_FATAL_UNLESS(m_count == 0, "Mutex destroyed while busy");
    }

    void lock()
    {
        uint32_t const tid = this_thread::getId();
        if (!Recursive)
        {
            CARB_FATAL_UNLESS(tid != m_owner, "Recursion not allowed");
        }
        else if (tid == m_owner)
        {
            ++m_count;
            return;
        }
        AcquireSRWLockExclusive((PSRWLOCK)&m_lock);
        m_owner = tid;
        m_count = 1;
    }
    bool try_lock()
    {
        uint32_t const tid = this_thread::getId();
        if (!Recursive)
        {
            CARB_FATAL_UNLESS(tid != m_owner, "Recursion not allowed");
        }
        else if (tid == m_owner)
        {
            ++m_count;
            return true;
        }
        if (CARB_LIKELY(TryAcquireSRWLockExclusive((PSRWLOCK)&m_lock)))
        {
            m_owner = tid;
            m_count = 1;
            return true;
        }
        return false;
    }
    void unlock()
    {
        uint32_t tid = this_thread::getId();
        CARB_FATAL_UNLESS(m_owner == tid, "Not owner");
        if (--m_count == 0)
        {
            m_owner = kInvalidOwner;
            ReleaseSRWLockExclusive((PSRWLOCK)&m_lock);
        }
    }

private:
    constexpr static uint32_t kInvalidOwner = uint32_t(0);

    CARBWIN_SRWLOCK m_lock{ CARBWIN_SRWLOCK_INIT };
    uint32_t m_owner{ kInvalidOwner };
    int m_count{ 0 };
};

#    else
template <bool Recursive>
class BaseMutex;

// BaseMutex (non-recursive)
template <>
class BaseMutex<false>
{
public:
    constexpr static bool kRecursive = false;

    constexpr BaseMutex() noexcept = default;
    ~BaseMutex()
    {
        CARB_FATAL_UNLESS(m_lock.load(std::memory_order_relaxed) == Unlocked, "Mutex destroyed while busy");
    }

    void lock()
    {
        // Blindly attempt to lock
        LockState val = Unlocked;
        if (CARB_UNLIKELY(
                !m_lock.compare_exchange_strong(val, Locked, std::memory_order_acquire, std::memory_order_relaxed)))
        {
            CARB_FATAL_UNLESS(m_owner != this_thread::getId(), "Recursive locking not allowed");

            // Failed to lock and need to wait
            if (val == LockedMaybeWaiting)
            {
                m_lock.wait(LockedMaybeWaiting, std::memory_order_relaxed);
            }

            while (m_lock.exchange(LockedMaybeWaiting, std::memory_order_acquire) != Unlocked)
            {
                m_lock.wait(LockedMaybeWaiting, std::memory_order_relaxed);
            }

            CARB_ASSERT(m_owner == kInvalidOwner);
        }
        // Now inside the lock
        m_owner = this_thread::getId();
    }

    bool try_lock()
    {
        // Blindly attempt to lock
        LockState val = Unlocked;
        if (CARB_LIKELY(m_lock.compare_exchange_strong(val, Locked, std::memory_order_acquire, std::memory_order_relaxed)))
        {
            m_owner = this_thread::getId();
            return true;
        }
        CARB_FATAL_UNLESS(m_owner != this_thread::getId(), "Recursive locking not allowed");
        return false;
    }

    void unlock()
    {
        CARB_FATAL_UNLESS(m_owner == this_thread::getId(), "Not owner");
        m_owner = kInvalidOwner;
        LockState val = m_lock.exchange(Unlocked, std::memory_order_release);
        if (val == LockedMaybeWaiting)
        {
            m_lock.notify_one();
        }
    }

private:
    enum LockState : uint8_t
    {
        Unlocked = 0,
        Locked = 1,
        LockedMaybeWaiting = 2,
    };

    constexpr static uint32_t kInvalidOwner = 0;

    cpp20::atomic<LockState> m_lock{ Unlocked };
    uint32_t m_owner{ kInvalidOwner };
};

// BaseMutex (recursive)
template <>
class BaseMutex<true>
{
public:
    constexpr static bool kRecursive = true;

    constexpr BaseMutex() noexcept = default;
    ~BaseMutex()
    {
        CARB_FATAL_UNLESS(m_lock.load(std::memory_order_relaxed) == 0, "Mutex destroyed while busy");
    }

    void lock()
    {
        // Blindly attempt to lock
        uint32_t val = Unlocked;
        if (CARB_UNLIKELY(
                !m_lock.compare_exchange_strong(val, Locked, std::memory_order_acquire, std::memory_order_relaxed)))
        {
            // Failed to lock (or recursive)
            if (m_owner == this_thread::getId())
            {
                val = m_lock.fetch_add(DepthUnit, std::memory_order_relaxed);
                CARB_FATAL_UNLESS((val & DepthMask) != DepthMask, "Recursion overflow");
                return;
            }

            // Failed to lock and need to wait
            if ((val & ~DepthMask) == LockedMaybeWaiting)
            {
                m_lock.wait(val, std::memory_order_relaxed);
            }

            for (;;)
            {
                // Atomically set to LockedMaybeWaiting in a loop since the owning thread could be changing the depth
                while (!m_lock.compare_exchange_weak(
                    val, (val & DepthMask) | LockedMaybeWaiting, std::memory_order_acquire, std::memory_order_relaxed))
                    CARB_HARDWARE_PAUSE();
                if ((val & ~DepthMask) == Unlocked)
                    break;
                m_lock.wait((val & DepthMask) | LockedMaybeWaiting, std::memory_order_relaxed);
            }

            CARB_ASSERT(m_owner == kInvalidOwner);
        }
        // Now inside the lock
        m_owner = this_thread::getId();
    }

    bool try_lock()
    {
        // Blindly attempt to lock
        uint32_t val = Unlocked;
        if (CARB_LIKELY(m_lock.compare_exchange_strong(val, Locked, std::memory_order_acquire, std::memory_order_relaxed)))
        {
            // Succeeded, we now own the lock
            m_owner = this_thread::getId();
            return true;
        }
        // Failed (or recursive)
        if (m_owner == this_thread::getId())
        {
            // Recursive, increment the depth
            val = m_lock.fetch_add(DepthUnit, std::memory_order_acquire);
            CARB_FATAL_UNLESS((val & DepthMask) != DepthMask, "Recursion overflow");
            return true;
        }
        return false;
    }

    void unlock()
    {
        CARB_FATAL_UNLESS(m_owner == this_thread::getId(), "Not owner");
        uint32_t val = m_lock.load(std::memory_order_relaxed);
        if (!(val & DepthMask))
        {
            // Depth count is at zero, so this is the last unlock().
            m_owner = kInvalidOwner;
            uint32_t val = m_lock.exchange(Unlocked, std::memory_order_release);
            if (val == LockedMaybeWaiting)
            {
                m_lock.notify_one();
            }
        }
        else
            m_lock.fetch_sub(DepthUnit, std::memory_order_release);
    }

private:
    enum LockState : uint32_t
    {
        Unlocked = 0,
        Locked = 1,
        LockedMaybeWaiting = 2,

        DepthUnit = 1 << 2, // Each recursion count increment
        DepthMask = 0xFFFFFFFC // The 30 MSBs are used for the recursion count
    };

    constexpr static uint32_t kInvalidOwner = 0;

    cpp20::atomic_uint32_t m_lock{ Unlocked };
    uint32_t m_owner{ kInvalidOwner };
};
#    endif
} // namespace detail
#endif

/**
 * A Carbonite implementation of [std::mutex](https://en.cppreference.com/w/cpp/thread/mutex).
 *
 * @note Windows: `std::mutex` uses `SRWLOCK` for Win 7+, `CONDITION_VARIABLE` for Vista and the massive
 * `CRITICAL_SECTION` for pre-Vista. Due to this, the `std::mutex` class is about 80 bytes. Since Carbonite supports
 * Windows 10 and later, `SRWLOCK` is used exclusively. This implementation is 16 bytes. This version that uses
 * `SRWLOCK` is significantly faster on Windows than the portable implementation used for the linux version.
 *
 * @note Linux: `sizeof(std::mutex)` is 40 bytes on GLIBC 2.27, which is based on the size of `pthread_mutex_t`. The
 * Carbonite implementation of mutex is 8 bytes and at least as performant as `std::mutex` in the contended case.
 */
class mutex : public detail::BaseMutex<false>
{
    using Base = detail::BaseMutex<false>;

public:
    /**
     * Constructor.
     *
     * @note Unlike `std::mutex`, this implementation can be declared `constexpr`.
     */
    constexpr mutex() noexcept = default;

    /**
     * Destructor.
     * @warning `std::terminate()` is called if mutex is locked by a thread.
     */
    ~mutex() = default;

    /**
     * Locks the mutex, blocking until it becomes available.
     * @warning `std::terminate()` is called if the calling thread already has the mutex locked. Use recursive_mutex if
     * recursive locking is desired.
     * The calling thread must call unlock() at a later time to release the lock.
     */
    void lock()
    {
        Base::lock();
    }

    /**
     * Attempts to immediately lock the mutex.
     * @warning `std::terminate()` is called if the calling thread already has the mutex locked. Use recursive_mutex if
     * recursive locking is desired.
     * @returns `true` if the mutex was available and the lock was taken by the calling thread (unlock() must be called
     * from the calling thread at a later time to release the lock); `false` if the mutex could not be locked by the
     * calling thread.
     */
    bool try_lock()
    {
        return Base::try_lock();
    }

    /**
     * Unlocks the mutex.
     * @warning `std::terminate()` is called if the calling thread does not own the mutex.
     */
    void unlock()
    {
        Base::unlock();
    }
};

/**
 * A Carbonite implementation of [std::recursive_mutex](https://en.cppreference.com/w/cpp/thread/recursive_mutex).
 *
 * @note Windows: `std::recursive_mutex` uses `SRWLOCK` for Win 7+, `CONDITION_VARIABLE` for Vista and the massive
 * `CRITICAL_SECTION` for pre-Vista. Due to this, the `std::recursive_mutex` class is about 80 bytes. Since Carbonite
 * supports Windows 10 and later, `SRWLOCK` is used exclusively. This implementation is 16 bytes. This version that uses
 * `SRWLOCK` is significantly faster on Windows than the portable implementation used for the linux version.
 *
 * @note Linux: `sizeof(std::recursive_mutex)` is 40 bytes on GLIBC 2.27, which is based on the size of
 * `pthread_mutex_t`. The Carbonite implementation of mutex is 8 bytes and at least as performant as
 * `std::recursive_mutex` in the contended case.
 */
class recursive_mutex : public detail::BaseMutex<true>
{
    using Base = detail::BaseMutex<true>;

public:
    /**
     * Constructor.
     *
     * @note Unlike `std::recursive_mutex`, this implementation can be declared `constexpr`.
     */
    constexpr recursive_mutex() noexcept = default;

    /**
     * Destructor.
     * @warning `std::terminate()` is called if recursive_mutex is locked by a thread.
     */
    ~recursive_mutex() = default;

    /**
     * Locks the recursive_mutex, blocking until it becomes available.
     *
     * The calling thread must call unlock() at a later time to release the lock. There must be symmetrical calls to
     * unlock() for each call to lock() or successful call to try_lock().
     */
    void lock()
    {
        Base::lock();
    }

    /**
     * Attempts to immediately lock the recursive_mutex.
     * @returns `true` if the recursive_mutex was available and the lock was taken by the calling thread (unlock() must
     * be called from the calling thread at a later time to release the lock); `false` if the recursive_mutex could not
     * be locked by the calling thread. If the lock was already held by the calling thread, `true` is always returned.
     */
    bool try_lock()
    {
        return Base::try_lock();
    }

    /**
     * Unlocks the recursive_mutex.
     * @warning `std::terminate()` is called if the calling thread does not own the recursive_mutex.
     */
    void unlock()
    {
        Base::unlock();
    }
};

} // namespace thread
} // namespace carb
