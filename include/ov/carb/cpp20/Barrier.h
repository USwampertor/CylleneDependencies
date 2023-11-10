// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//! @brief Implementation of the C++20 std::barrier class.
#pragma once

#include "Atomic.h"
#include "Bit.h"

#include <utility>

namespace carb
{
namespace cpp20
{

//! \cond DEV
namespace detail
{

constexpr uint32_t kInvalidPhase = 0;

struct NullFunction
{
    constexpr NullFunction() noexcept = default;
    constexpr void operator()() noexcept
    {
    }
};

} // namespace detail
//! \endcond

// Handle case where Windows.h may have defined 'max'
#pragma push_macro("max")
#undef max

/**
 * Implements a C++20 barrier in C++14 semantics.
 *
 * A barrier is a thread coordination mechanism whose lifetime consists of a sequence of barrier phases, where
 * each phase allows at most an expected number of threads to block until the expected number of threads
 * arrive at the barrier. A barrier is useful for managing repeated tasks that are handled by multiple
 * threads.
 *
 * @see https://en.cppreference.com/w/cpp/thread/barrier
 * @tparam CompletionFunction A function object type that must meet the requirements of MoveConstructible and
 * Destructible. `std::is_nothrow_invocable_v<CompletionFunction&>` must be `true`. The default template argument is an
 * unspecified function object type that additionally meets the requirements of DefaultConstructible. Calling an lvalue
 * of it with no arguments has no effects. Every barrier behaves as if it holds an exposition-only non-static data
 * member `completion_` of type `CompletionFunction` and calls it by `completion_()` on every phase completion step.
 */
template <class CompletionFunction = detail::NullFunction>
class barrier
{
    CARB_PREVENT_COPY_AND_MOVE(barrier);

public:
    /**
     * Returns the maximum value of expected count supported by the implementation.
     * @returns the maximum value of expected count supported by the implementation.
     */
    static constexpr ptrdiff_t max() noexcept
    {
        return ptrdiff_t(INT_MAX);
    }

    /**
     * Constructor
     *
     * Sets both the initial expected count for each phase and the current expected count for the first phase to
     * @p expected, initializes the completion function object with `std::move(f)`, and then starts the first phase. The
     * behavior is undefined if @p expected is negative or greater than @ref max().
     *
     * @param expected Initial value of the expected count.
     * @param f Completion function object to be called on phase completion step.
     * @throws std::exception Any exception thrown by CompletionFunction's move constructor.
     */
    constexpr explicit barrier(ptrdiff_t expected, CompletionFunction f = CompletionFunction{})
        : m_emo(InitBoth{}, std::move(f), (uint64_t(1) << kPhaseBitShift) + uint32_t(::carb_min(expected, max()))),
          m_expected(uint32_t(::carb_min(expected, max())))
    {
        CARB_ASSERT(expected >= 0 && expected <= max());
    }

    /**
     * Destructor
     *
     * The behavior is undefined if any thread is concurrently calling a member function of the barrier.
     *
     * @note This implementation waits until all waiting threads have woken, but this is a stronger guarantee than the
     * standard.
     */
    ~barrier()
    {
        // Wait for destruction until all waiters are clear
        while (m_waiters.load(std::memory_order_acquire) != 0)
            std::this_thread::yield();
    }

    /**
     * An object type meeting requirements of MoveConstructible, MoveAssignable and Destructible.
     * @see arrive() wait()
     */
    class arrival_token
    {
        CARB_PREVENT_COPY(arrival_token);
        friend class barrier;
        uint32_t m_token{ detail::kInvalidPhase };
        arrival_token(uint32_t token) : m_token(token)
        {
        }

    public:
        //! Constructor
        arrival_token() = default;
        //! Move constructor
        //! @param rhs Other @c arrival_token to move from. @c rhs is left in a valid but empty state.
        arrival_token(arrival_token&& rhs) : m_token(std::exchange(rhs.m_token, detail::kInvalidPhase))
        {
        }
        //! Move-assign operator
        //! @param rhs Other @c arrival_token to move from. @c rhs is left in a valid but empty state.
        //! @returns @c *this
        arrival_token& operator=(arrival_token&& rhs)
        {
            m_token = std::exchange(rhs.m_token, detail::kInvalidPhase);
            return *this;
        }
    };

    /**
     * Arrives at barrier and decrements the expected count
     *
     * Constructs an @ref arrival_token object associated with the phase synchronization point for the current phase.
     * Then, decrements the expected count by @p update.
     *
     * This function executes atomically. The call to this function strongly happens-before the start of the phase
     * completion step for the current phase.
     *
     * The behavior is undefined if @p update is less than or equal zero or greater than the expected count for the
     * current barrier phase.
     *
     * @param update The value by which the expected count is decreased.
     * @returns The constructed @ref arrival_token object.
     * @throws std::system_error According to the standard, but this implementation does not throw. Instead an assertion
     * occurs.
     * @see wait()
     */
    CARB_NODISCARD arrival_token arrive(ptrdiff_t update = 1)
    {
        return arrival_token(uint32_t(_arrive(update).first >> kPhaseBitShift));
    }

    /**
     * Blocks at the phase synchronization point until its phase completion step is run.
     *
     * If @p arrival is associated with the phase synchronization point for the current phase of @c *this, blocks at the
     * synchronization point associated with @p arrival until the phase completion step of the synchronization point's
     * phase is run.
     *
     * Otherwise if @p arrival is associated with the phase synchronization point for the immediately preceding phase of
     * @c *this, returns immediately.
     *
     * Otherwise, i.e. if @p arrival is associated with the phase synchronization point for an earlier phase of @c *this
     * or any phase of a barrier object other than @c *this, the behavior is undefined.
     *
     * @param arrival An @ref arrival_token obtained by a previous call to @ref arrive() on the same barrier.
     * @throws std::system_error According to the standard, but this implementation does not throw. Instead an assertion
     * occurs.
     */
    void wait(arrival_token&& arrival) const
    {
        // Precondition: arrival is associated with the phase synchronization point for the current phase or the
        // immediately preceding phase.
        CARB_CHECK(arrival.m_token != 0); // No invalid tokens
        uint64_t data = m_emo.second.load(std::memory_order_acquire);
        uint32_t phase = uint32_t(data >> kPhaseBitShift);
        CARB_CHECK((phase - arrival.m_token) <= 1, "arrival %u is not the previous or current phase %u",
                   arrival.m_token, phase);

        if (phase != arrival.m_token)
            return;

        // Register as a waiter
        m_waiters.fetch_add(1, std::memory_order_relaxed);

        do
        {
            // Wait for the phase to change
            m_emo.second.wait(data, std::memory_order_relaxed);

            // Reload after waiting
            data = m_emo.second.load(std::memory_order_acquire);
            phase = uint32_t(data >> kPhaseBitShift);
        } while (phase == arrival.m_token);

        // Unregister as a waiter
        m_waiters.fetch_sub(1, std::memory_order_release);
    }

    /**
     * Arrives at barrier and decrements the expected count by one, then blocks until the current phase completes.
     *
     * Atomically decrements the expected count by one, then blocks at the synchronization point for the current phase
     * until the phase completion step of the current phase is run. Equivalent to `wait(arrive())`.
     *
     * The behavior is undefined if the expected count for the current phase is zero.
     *
     * @note If the current expected count is decremented to zero in the call to this function, the phase completion
     * step is run and this function does not block. If the current expected count is zero before calling this function,
     * the initial expected count for all subsequent phases is also zero, which means the barrier cannot be reused.
     *
     * @throws std::system_error According to the standard, but this implementation does not throw. Instead an assertion
     * occurs.
     */
    void arrive_and_wait()
    {
        // Two main differences over just doing arrive(wait()):
        // - We return immediately if _arrive() did the phase shift
        // - We don't CARB_CHECK that the phase is the current or preceding one since it is guaranteed
        auto result = _arrive(1);
        if (result.second)
            return;

        // Register as a waiter
        m_waiters.fetch_add(1, std::memory_order_relaxed);

        uint64_t data = result.first;
        uint32_t origPhase = uint32_t(data >> kPhaseBitShift), phase;
        do
        {
            // Wait for the phase to change
            m_emo.second.wait(data, std::memory_order_relaxed);

            // Reload after waiting
            data = m_emo.second.load(std::memory_order_acquire);
            phase = uint32_t(data >> kPhaseBitShift);
        } while (phase == origPhase);

        // Unregister as a waiter
        m_waiters.fetch_sub(1, std::memory_order_release);
    }

    /**
     * Decrements both the initial expected count for subsequent phases and the expected count for current phase by one.
     *
     * This function is executed atomically. The call to this function strongly happens-before the start of the phase
     * completion step for the current phase.
     *
     * The behavior is undefined if the expected count for the current phase is zero.
     *
     * @note This function can cause the completion step for the current phase to start. If the current expected count
     * is zero before calling this function, the initial expected count for all subsequent phases is also zero, which
     * means the barrier cannot be reused.
     *
     * @throws std::system_error According to the standard, but this implementation does not throw. Instead an assertion
     * occurs.
     */
    void arrive_and_drop()
    {
        uint32_t prev = m_expected.fetch_sub(1, std::memory_order_relaxed);
        CARB_CHECK(prev != 0); // Precondition failure: expected count for the current barrier phase must be greater
                               // than zero.

        _arrive(1);
    }

private:
    constexpr static int kPhaseBitShift = 32;
    constexpr static uint64_t kCounterMask = 0xffffffffull;

    CARB_ALWAYS_INLINE std::pair<uint64_t, bool> _arrive(ptrdiff_t update)
    {
        CARB_CHECK(update > 0 && update <= max());

        uint64_t pre = m_emo.second.fetch_sub(uint32_t(update), std::memory_order_acq_rel);
        CARB_CHECK(ptrdiff_t(int32_t(uint32_t(pre & kCounterMask))) >= update); // Precondition check

        bool completed = false;
        if (uint32_t(pre & kCounterMask) == uint32_t(update))
        {
            // Phase is now complete
            std::atomic_thread_fence(std::memory_order_acquire);
            _completePhase(pre - uint32_t(update));
            completed = true;
        }

        return std::make_pair(pre - uint32_t(update), completed);
    }

    void _completePhase(uint64_t data)
    {
        uint32_t expected = m_expected.load(std::memory_order_relaxed);

        // Run the completion routine before releasing threads
        m_emo.first()();

        // Increment the phase and don't allow the invalid phase
        uint32_t phase = uint32_t(data >> kPhaseBitShift);
        if (++phase == detail::kInvalidPhase)
            ++phase;

#if CARB_ASSERT_ENABLED
        // Should not have changed during completion function.
        uint64_t old = m_emo.second.exchange((uint64_t(phase) << kPhaseBitShift) + expected, std::memory_order_release);
        CARB_ASSERT(old == data);
#else
        m_emo.second.store((uint64_t(phase) << kPhaseBitShift) + expected, std::memory_order_release);
#endif

        // Release all waiting threads
        m_emo.second.notify_all();
    }

    // The MSB 32 bits of the atomic_uint64_t are the Phase; the other bits are the Counter
    EmptyMemberPair<CompletionFunction, cpp20::atomic_uint64_t> m_emo;
    std::atomic_uint32_t m_expected;
    mutable std::atomic_uint32_t m_waiters{ 0 };
};

#pragma pop_macro("max")

} // namespace cpp20
} // namespace carb
