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
//! @brief Carbonite Spinlock implementation.
#pragma once

#include "../Defines.h"

#include <atomic>
#include <thread>

namespace carb
{
namespace thread
{

/** Namespace for Carbonite private threading details. */
namespace detail
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS
class RecursionPolicyDisallow
{
public:
    constexpr RecursionPolicyDisallow() = default;

    bool ownsLock() const
    {
        return std::this_thread::get_id() == m_owner;
    }
    void enter()
    {
        auto cur = std::this_thread::get_id();
        CARB_FATAL_UNLESS(cur != m_owner, "Recursion is not allowed");
        m_owner = cur;
    }
    bool tryLeave()
    {
        CARB_FATAL_UNLESS(ownsLock(), "Not owning thread");
        m_owner = std::thread::id(); // clear the owner
        return true;
    }

private:
    std::thread::id m_owner{};
};

class RecursionPolicyAllow
{
public:
    constexpr RecursionPolicyAllow() = default;

    bool ownsLock() const
    {
        return std::this_thread::get_id() == m_owner;
    }
    void enter()
    {
        auto cur = std::this_thread::get_id();
        if (cur == m_owner)
            ++m_recursion;
        else
        {
            CARB_ASSERT(m_owner == std::thread::id()); // owner should be clear
            m_owner = cur;
            m_recursion = 1;
        }
    }
    bool tryLeave()
    {
        CARB_FATAL_UNLESS(ownsLock(), "Not owning thread");
        if (--m_recursion == 0)
        {
            m_owner = std::thread::id(); // clear the owner
            return true;
        }
        return false;
    }

private:
    std::thread::id m_owner{};
    size_t m_recursion{ 0 };
};
#endif

/**
 * Spinlock and RecursiveSpinlock are locking primitives that never enter the kernel to wait.
 *
 * @note Do not use SpinlockImpl directly; instead use Spinlock or RecursiveSpinlock.
 *
 * This class meets Cpp17BasicLockable and Cpp17Lockable named requirements.
 *
 * @warning Using Spinlock is generally discouraged and can lead to worse performance than using carb::thread::mutex or
 * another synchronization primitive that can wait.
 */
template <class RecursionPolicy>
class SpinlockImpl
{
public:
    /**
     * Constructor.
     */
    constexpr SpinlockImpl() = default;

    /**
     * Destructor.
     */
    ~SpinlockImpl() = default;

    CARB_PREVENT_COPY(SpinlockImpl);

    /**
     * Locks the spinlock, spinning the current thread until it becomes available.
     * If not called from RecursiveSpinlock and the calling thread already owns the lock, `std::terminate()` is called.
     * The calling thread must call unlock() at a later time to release the lock.
     */
    void lock()
    {
        if (!m_rp.ownsLock())
        {
            // Spin trying to set the lock bit
            while (CARB_UNLIKELY(!!m_lock.fetch_or(1, std::memory_order_acquire)))
            {
                CARB_HARDWARE_PAUSE();
            }
        }
        m_rp.enter();
    }

    /**
     * Unlocks the spinlock.
     * @warning `std::terminate()` is called if the calling thread does not own the spinlock.
     */
    void unlock()
    {
        if (m_rp.tryLeave())
        {
            // Released the lock
            m_lock.store(0, std::memory_order_release);
        }
    }

    /**
     * Attempts to immediately lock the spinlock.
     * If not called from RecursiveSpinlock and the calling thread already owns the lock, `std::terminate()` is called.
     * @returns `true` if the spinlock was available and the lock was taken by the calling thread (unlock() must be
     * called from the calling thread at a later time to release the lock); `false` if the spinlock could not be locked
     * by the calling thread.
     */
    bool try_lock()
    {
        if (!m_rp.ownsLock())
        {
            // See if we can set the lock bit
            if (CARB_UNLIKELY(!!m_lock.fetch_or(1, std::memory_order_acquire)))
            {
                // Failed!
                return false;
            }
        }
        m_rp.enter();
        return true;
    }

    /**
     * Returns true if the calling thread owns this spinlock.
     * @returns `true` if the calling thread owns this spinlock; `false` otherwise.
     */
    bool isLockedByThisThread() const
    {
        return m_rp.ownsLock();
    }

private:
    std::atomic<size_t> m_lock{ 0 };
    RecursionPolicy m_rp;
};

} // namespace detail

/**
 * A spinlock implementation that allows recursion.
 */
using RecursiveSpinlock = detail::SpinlockImpl<detail::RecursionPolicyAllow>;

/**
 * A spinlock implementation that does not allow recursion.
 * @warning Attempts to use this class in a recursive manner will call `std::terminate()`.
 */
using Spinlock = detail::SpinlockImpl<detail::RecursionPolicyDisallow>;

} // namespace thread
} // namespace carb
