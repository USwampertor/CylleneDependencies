// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//! @brief Implementation of the C++20 std::latch class.
#pragma once

#include "Atomic.h"

#include <algorithm>
#include <thread>

namespace carb
{
namespace cpp20
{

// Handle case where Windows.h may have defined 'max'
#pragma push_macro("max")
#undef max

/**
 * Implements a C++20 latch in C++14 semantics.
 *
 * The latch class is a downward counter of type @c std::ptrdiff_t which can be used to synchronize threads. The value
 * of the counter is initialized on creation. Threads may block on the latch until the counter is decremented to zero.
 * There is no possibility to increase or reset the counter, which makes the latch a single-use @ref barrier.
 *
 * @thread_safety Concurrent invocations of the member functions of @c latch, except for the destructor, do not
 * introduce data races.
 *
 * Unlike @ref barrier, @c latch can be decremented by a participating thread more than once.
 */
class latch
{
    CARB_PREVENT_COPY_AND_MOVE(latch);

public:
    /**
     * The maximum value of counter supported by the implementation
     * @returns The maximum value of counter supported by the implementation.
     */
    static constexpr ptrdiff_t max() noexcept
    {
        return ptrdiff_t(UINT_MAX);
    }

    /**
     * Constructor
     *
     * Constructs a latch and initializes its internal counter. The behavior is undefined if @p expected is negative or
     * greater than @ref max().
     * @param expected The initial value of the internal counter.
     */
    constexpr explicit latch(ptrdiff_t expected) noexcept : m_counter(uint32_t(::carb_min(max(), expected)))
    {
        CARB_ASSERT(expected >= 0 && expected <= max());
    }

    /**
     * Destructor
     *
     * The behavior is undefined if any thread is concurrently calling a member function of the latch.
     *
     * @note This implementation waits until all waiting threads have woken, but this is a stronger guarantee than the
     * standard.
     */
    ~latch() noexcept
    {
        // Wait until we have no waiters
        while (m_waiters.load(std::memory_order_acquire) != 0)
            std::this_thread::yield();
    }

    /**
     * Decrements the counter in a non-blocking manner
     *
     * Atomically decrements the internal counter by @p update without blocking the caller. If the count reaches zero,
     * all blocked threads are unblocked.
     *
     * If @p update is greater than the value of the internal counter or is negative, the behavior is undefined.
     *
     * This operation strongly happens-before all calls that are unblocked on this latch.
     *
     * @param update The value by which the internal counter is decreased.
     * @throws std::system_error According to the standard, but this implementation does not throw. Instead an assertion
     * occurs.
     */
    void count_down(ptrdiff_t update = 1) noexcept
    {
        CARB_ASSERT(update >= 0);

        // `fetch_sub` returns value before operation
        uint32_t count = m_counter.fetch_sub(uint32_t(update), std::memory_order_release);
        CARB_CHECK((count - uint32_t(update)) <= count); // Malformed if we go below zero or overflow
        if ((count - uint32_t(update)) == 0)
        {
            // Wake all waiters
            m_counter.notify_all();
        }
    }

    // Returns whether the latch has completed. Allowed to return spuriously false with very low probability.
    /**
     * Tests if the internal counter equals zero
     *
     * Returns @c true only if the internal counter has reached zero. This function may spuriously return @c false with
     * very low probability even if the internal counter has reached zero.
     *
     * @note The reason why a spurious result is permitted is to allow implementations to use a memory order more
     * relaxed than @c std::memory_order_seq_cst.
     *
     * @return With very low probability @c false, otherwise `cnt == 0` where `cnt` is the value of the internal
     * counter.
     */
    bool try_wait() const noexcept
    {
        return m_counter.load(std::memory_order_acquire) == 0;
    }

    /**
     * Blocks until the counter reaches zero
     *
     * Blocks the calling thread until the internal counter reaches zero. If it is zero already, returns immediately.
     * @throws std::system_error According to the standard, but this implementation does not throw.
     */
    void wait() const noexcept
    {
        uint32_t count = m_counter.load(std::memory_order_acquire);
        if (count != 0)
        {
            // Register as a waiter
            m_waiters.fetch_add(1, std::memory_order_relaxed);
            _wait(count);
        }
    }

    /**
     * Decrements the counter and blocks until it reaches zero
     *
     * Atomically decrements the internal counter by @p update and (if necessary) blocks the calling thread until the
     * counter reaches zero. Equivalent to `count_down(update); wait();`.
     *
     * If @p update is greater than the value of the internal counter or is negative, the behavior is undefined.
     *
     * @param update The value by which the internal counter is decreased.
     * @throws std::system_error According to the standard, but this implementation does not throw. Instead an assertion
     * occurs.
     */
    void arrive_and_wait(ptrdiff_t update = 1) noexcept
    {
        uint32_t original = m_counter.load(std::memory_order_acquire);
        if (original == uint32_t(update))
        {
            // We're the last and won't be waiting.
#if CARB_ASSERT_ENABLED
            uint32_t updated = m_counter.exchange(0, std::memory_order_release);
            CARB_ASSERT(updated == original);
#else
            m_counter.store(0, std::memory_order_release);
#endif

            // Wake all waiters
            m_counter.notify_all();
            return;
        }

        // Speculatively register as a waiter
        m_waiters.fetch_add(1, std::memory_order_relaxed);

        original = m_counter.fetch_sub(uint32_t(update), std::memory_order_release);
        if (CARB_UNLIKELY(original == uint32_t(update)))
        {
            // Wake all waiters and unregister as a waiter
            m_counter.notify_all();
            m_waiters.fetch_sub(1, std::memory_order_release);
        }
        else
        {
            CARB_CHECK(original >= uint32_t(update)); // Malformed if we underflow
            _wait(original - uint32_t(update));
        }
    }

private:
    mutable cpp20::atomic_uint32_t m_counter;
    mutable cpp20::atomic_uint32_t m_waiters{ 0 };

    CARB_ALWAYS_INLINE void _wait(uint32_t count) const noexcept
    {
        CARB_ASSERT(count != 0);
        do
        {
            m_counter.wait(count, std::memory_order_relaxed);
            count = m_counter.load(std::memory_order_acquire);
        } while (count != 0);

        // Done waiting
        m_waiters.fetch_sub(1, std::memory_order_release);
    }
};

#pragma pop_macro("max")

} // namespace cpp20
} // namespace carb
