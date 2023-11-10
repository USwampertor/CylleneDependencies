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
//! @brief Defines the LocklessQueue class
#pragma once

#include "../Defines.h"
#include "../cpp20/Atomic.h"
#include "../thread/Util.h"

#include <thread>

namespace carb
{

//! Carbonite container classes
namespace container
{

template <class T>
class LocklessQueueLink;
template <class T, LocklessQueueLink<T> T::*U>
class LocklessQueue;

/**
 * Defines the link object. Each class contained by LocklessQueue must have a member of type LocklessQueueLink.
 */
template <class T>
class LocklessQueueLink
{
public:
    /**
     * Default constructor.
     */
    constexpr LocklessQueueLink() = default;

    CARB_PREVENT_COPY_AND_MOVE(LocklessQueueLink);

private:
    CARB_VIZ std::atomic<T*> m_next;

    friend T;
    template <class U, LocklessQueueLink<U> U::*V>
    friend class LocklessQueue;
};

/**
 * @brief Implements a lockless queue: a FIFO queue that is thread-safe yet requires no kernel synchronization.
 *
 * LocklessQueue is designed to be easy-to-use. For a class `Foo` that you want to be contained in a LocklessQueue, it
 * must have a member of type LocklessQueueLink<Foo>. This member is what the LocklessQueue will use for tracking data.
 *
 * Pushing to LocklessQueue is simply done through LocklessQueue::push(), which is entirely thread-safe.
 * LocklessQueue ensures first-in-first-out (FIFO) for each producer pushing to LocklessQueue. Multiple producers
 * may be pushing into LocklessQueue simultaneously, so their items can become mingled, but each producer's pushed
 * items will be strongly ordered.
 *
 * Popping on the other hand is different for single-consumer vs. multiple-consumer. For
 * single-consumer (via the LocklessQueue::popSC() function) only one thread may be popping from LocklessQueue at any
 * given time. It is up to the caller to ensure this mutual exclusivity.
 *
 * If multiple-consumer is desired, use the LocklessQueue::popMC() function; it ensures additional
 * thread safety and is therefore higher cost. Furthermore LocklessQueue::popMC() has a contention back-off
 * capability that will attempt to solve high-contention situations with progressive spin and sleep if absolutely
 * necessary.
 *
 * Simple example:
 * ```cpp
 * class Foo
 * {
 * public:
 *     LocklessQueueLink<Foo> m_link;
 * };
 *
 * LocklessQueue<Foo, &Foo::m_link> queue;
 * queue.push(new Foo);
 * Foo* p = queue.popSC();
 * delete p;
 * ```
 *
 * @thread_safety LocklessQueue is entirely thread-safe except where declared otherwise. No allocation happens with a
 * LocklessQueue; instead the caller is responsible for construction/destruction of contained objects.
 *
 * @tparam T The type to contain.
 * @tparam U A pointer-to-member of a LocklessQueueLink member within T (see above example).
 */
template <class T, LocklessQueueLink<T> T::*U>
class CARB_VIZ LocklessQueue
{
public:
    /**
     * Constructs a new LocklessQueue.
     */
    constexpr LocklessQueue() : m_head(nullptr), m_tail(nullptr)
    {
    }

    CARB_PREVENT_COPY(LocklessQueue);

    /**
     * LocklessQueue is partially movable in that it can be move-constructed from another LocklessQueue, but cannot be
     * move-assigned. This is to guarantee that the state of the receiving LocklessQueue is a fresh state.
     */
    LocklessQueue(LocklessQueue&& rhs) : m_head(nullptr), m_tail(nullptr)
    {
        rhs._moveTo(*this);
    }

    /**
     * Cannot move-assign, only move-construct. This is to guarantee the state of the receiving LocklessQueue is a fresh
     * state.
     */
    LocklessQueue& operator=(LocklessQueue&&) = delete;

    /**
     * Destructor.
     *
     * @warning Asserts that isEmpty() returns `true`.
     */
    ~LocklessQueue()
    {
        // Not good to destroy when not empty
        CARB_ASSERT(isEmpty());
    }

    /**
     * Indicates whether the queue is empty.
     *
     * @warning Another thread may have modified the LocklessQueue before this function returns.
     *
     * @returns `true` if the queue appears empty; `false` if items appear to exist in the queue.
     */
    bool isEmpty() const
    {
        // Reading the tail is more efficient because much contention can happen on m_head
        return !m_tail.load(std::memory_order_relaxed);
    }

    /**
     * Pushes an entry onto the LocklessQueue.
     *
     * @param p The item to push into the queue.
     *
     * @returns `true` if the queue was empty prior to push; `false` otherwise. Note that this is atomically correct
     * as opposed to calling isEmpty() before push().
     */
    bool push(T* p)
    {
        // Make sure the node isn't already pointing at something
        next(p).store(nullptr, std::memory_order_relaxed);
        return _push(p, p);
    }

    /**
     * Pushes a contiguous block of entries from [ @p begin, @p end) onto the queue.
     *
     * @note All of the entries are guaranteed to remain strongly ordered and will not be interspersed with entries from
     * other threads.
     *
     * @param begin An <a href="https://en.cppreference.com/w/cpp/named_req/InputIterator">InputIterator</a> of the
     * first item to push. `*begin` must resolve to a `T&`.
     * @param end An off-the-end InputIterator after the last item to push.
     * @returns true if the queue was empty prior to push; `false` otherwise. Note that this is atomically correct
     * as opposed to calling isEmpty() before push().
     */
#ifndef DOXYGEN_BUILD
    template <class InputItRef,
              std::enable_if_t<std::is_convertible<decltype(std::declval<InputItRef&>()++, *std::declval<InputItRef&>()), T&>::value,
                               bool> = false>
#else
    template <class InputItRef>
#endif
    bool push(InputItRef begin, InputItRef end)
    {
        // Handle empty list
        if (begin == end)
            return false;

        // Walk through iterators and have them point to each other
        InputItRef last = begin;
        InputItRef iter = begin;
        iter++;
        for (; iter != end; last = iter++)
        {
            next(std::addressof(*last)).store(std::addressof(*iter), std::memory_order_relaxed);
        }

        next(std::addressof(*last)).store(nullptr, std::memory_order_relaxed);

        return _push(std::addressof(*begin), std::addressof(*last));
    }

    /**
     * Pushes a block of pointers-to-entries from [ @p begin, @p end) onto the queue.
     *
     * @note All of the entries are guaranteed to remain strongly ordered and will not be interspersed with entries from
     * other threads.
     *
     * @param begin An <a href="https://en.cppreference.com/w/cpp/named_req/InputIterator">InputIterator</a> of the
     * first item to push. `*begin` must resolve to a `T*`.
     * @param end An off-the-end InputIterator after the last item to push.
     * @returns true if the queue was empty prior to push; `false` otherwise. Note that this is atomically correct
     * as opposed to calling isEmpty() before push().
     */
#ifndef DOXYGEN_BUILD
    template <class InputItPtr,
              std::enable_if_t<std::is_convertible<decltype(std::declval<InputItPtr&>()++, *std::declval<InputItPtr&>()), T*>::value,
                               bool> = true>
#else
    template <class InputItPtr>
#endif
    bool push(InputItPtr begin, InputItPtr end)
    {
        // Handle empty list
        if (begin == end)
            return false;

        // Walk through iterators and have them point to each other
        InputItPtr last = begin;
        InputItPtr iter = begin;
        iter++;
        for (; iter != end; last = iter++)
        {
            next(*last).store(*iter, std::memory_order_relaxed);
        }

        next(*last).store(nullptr, std::memory_order_relaxed);

        return _push(*begin, *last);
    }

    /**
     * Ejects all entries from this queue as a new LocklessQueue.
     *
     * @note To clear all items use popAll().
     */
    CARB_NODISCARD LocklessQueue eject()
    {
        return LocklessQueue(std::move(*this));
    }

    /**
     * Empties the queue.
     *
     * @note To perform an action on each item as it is popped, use forEach() instead.
     */
    void popAll()
    {
        static T* const mediator = (T*)(size_t)-1;

        T* head;
        for (;;)
        {
            // The mediator acts as both a lock and a signal
            head = m_head.exchange(mediator, std::memory_order_relaxed);

            if (CARB_UNLIKELY(!head))
            {
                // The first xchg on the tail will tell the enqueuing thread that it is safe to blindly write out to the
                // head pointer. A cmpxchg honors the algorithm.
                T* m = mediator;
                if (CARB_UNLIKELY(!m_head.compare_exchange_strong(
                        m, nullptr, std::memory_order_relaxed, std::memory_order_relaxed)))
                {
                    // Couldn't write a nullptr back. Try again.
                    continue;
                }
                if (CARB_UNLIKELY(!!m_tail.load(std::memory_order_relaxed)))
                {
                    bool isNull;
                    // Wait for consistency
                    this_thread::spinWaitWithBackoff([&] { return this->consistent(isNull); });
                    if (!isNull)
                    {
                        // Try again.
                        continue;
                    }
                }
                // Nothing on the queue.
                return;
            }

            if (CARB_UNLIKELY(head == mediator))
            {
                // Another thread is in a pop function. Wait until m_head is no longer the mediator.
                this_thread::spinWaitWithBackoff([&] { return this->mediated(); });
                // Try again.
                continue;
            }
            break;
        }

        // Release our lock and swap with the tail
        m_head.store(nullptr, std::memory_order_release);
        m_tail.exchange(nullptr, std::memory_order_release);
    }

    /**
     * Pops all available items from the queue calling a functionish object on each.
     *
     * First, pops all available items from `*this` and then calls @p f on each.
     *
     * @note As the pop is the first thing that happens, any new entries that get pushed while the function is executing
     * will NOT be popped and will remain in LocklessQueue when this function returns.
     *
     * @param f A functionish object that accepts a `T*` parameter. Called for each item that was popped from the queue.
     */
    template <class Func>
    void forEach(Func&& f)
    {
        static T* const mediator = (T*)(size_t)-1;

        T* head;
        for (;;)
        {
            // The mediator acts as both a lock and a signal
            head = m_head.exchange(mediator, std::memory_order_relaxed);

            if (CARB_UNLIKELY(!head))
            {
                // The first xchg on the tail will tell the enqueuing thread that it is safe to blindly write out to the
                // head pointer. A cmpxchg honors the algorithm.
                T* m = mediator;
                if (CARB_UNLIKELY(!m_head.compare_exchange_strong(
                        m, nullptr, std::memory_order_relaxed, std::memory_order_relaxed)))
                {
                    // Couldn't write a nullptr back. Try again.
                    continue;
                }
                if (CARB_UNLIKELY(!!m_tail.load(std::memory_order_relaxed)))
                {
                    bool isNull;
                    // Wait for consistency
                    this_thread::spinWaitWithBackoff([&] { return this->consistent(isNull); });
                    if (!isNull)
                    {
                        // Try again.
                        continue;
                    }
                }
                // Nothing on the queue.
                return;
            }

            if (CARB_UNLIKELY(head == mediator))
            {
                // Another thread is in a pop function. Wait until m_head is no longer the mediator.
                this_thread::spinWaitWithBackoff([&] { return this->mediated(); });
                // Try again.
                continue;
            }
            break;
        }

        // Release our lock and swap with the tail
        m_head.store(nullptr, std::memory_order_release);
        T* e = m_tail.exchange(nullptr, std::memory_order_release);
        for (T *p = head, *n; p; p = n)
        {
            // Ensure that we have a next item (except for `e`; the end of the queue). It's possible
            // that a thread is in `push()` and has written the tail at the time of exchange, above,
            // but has not yet written the previous item's next pointer.
            n = next(p).load(std::memory_order_relaxed);
            if (CARB_UNLIKELY(!n && p != e))
                n = waitForEnqueue(next(p));
            f(p);
        }
    }

    /**
     * Pop first entry (Single-consumer).
     *
     * @thread_safety May only be done in a single thread and is mutually exclusive with all other functions that modify
     * LocklessQueue *except* push(). Use popMC() for a thread-safe pop function.
     *
     * @warning Debug builds will assert if a thread safety issue is detected.
     *
     * @returns the first item removed from the queue, or `nullptr` if the queue is empty.
     */
    T* popSC()
    {
#if CARB_ASSERT_ENABLED
        // For debug builds, swap with a mediator to ensure that another thread is not in this function
        static T* const mediator = (T*)(size_t)-1;
        T* h = m_head.exchange(mediator, std::memory_order_acquire);
        CARB_ASSERT(
            h != mediator, "LocklessQueue: Another thread is racing with popSC(). Use popMC() for multi-consumer.");
        while (CARB_UNLIKELY(!h))
        {
            h = m_head.exchange(nullptr, std::memory_order_release);
            if (h == mediator)
            {
                // We successfully swapped a nullptr for the mediator we put there.
                return nullptr;
            }
            // Another thread in push() could've put something else here, so check it again.
        }
#else
        // If head is null the queue is empty
        T* h = m_head.load(std::memory_order_acquire);
        if (CARB_UNLIKELY(!h))
        {
            return nullptr;
        }
#endif

        // Load the next item and store into the head
        T* n = next(h).load(std::memory_order_acquire);
        m_head.store(n, std::memory_order_relaxed);
        T* e = h;
        if (CARB_UNLIKELY(!n && !m_tail.compare_exchange_strong(
                                    e, nullptr, std::memory_order_release, std::memory_order_relaxed)))
        {
            // The next item was null, but we failed to write null to the tail, so another thread must have added
            // something. Read the next value from `h` and store it in the _head.
            n = next(h).load(std::memory_order_acquire);
            if (CARB_UNLIKELY(!n))
                n = waitForEnqueue(next(h));
            m_head.store(n, std::memory_order_relaxed);
        }

        // This isn't really necessary but prevents dangling pointers.
        next(h).store(nullptr, std::memory_order_relaxed);

        return h;
    }

    /**
     * Pop first entry (Multiple-consumer).
     *
     * @note In a highly-contentious situation, this function will back off and attempt to sleep in order to resolve the
     * contention.
     *
     * @returns the first item removed from the queue, or `nullptr` if the queue is empty.
     */
    T* popMC()
    {
        static T* const mediator = (T*)(size_t)-1;

        T* head;
        for (;;)
        {
            // The mediator acts as both a lock and a signal
            head = m_head.exchange(mediator, std::memory_order_relaxed);

            if (CARB_UNLIKELY(!head))
            {
                // The first xchg on the tail will tell the enqueuing thread that it is safe to blindly write out to the
                // head pointer. A cmpxchg honors the algorithm.
                T* m = mediator;
                if (CARB_UNLIKELY(!m_head.compare_exchange_strong(
                        m, nullptr, std::memory_order_relaxed, std::memory_order_relaxed)))
                {
                    // Couldn't write a nullptr back. Try again.
                    continue;
                }
                if (CARB_UNLIKELY(!!m_tail.load(std::memory_order_relaxed)))
                {
                    bool isNull;
                    // Wait for consistency
                    this_thread::spinWaitWithBackoff([&] { return this->consistent(isNull); });
                    if (!isNull)
                    {
                        // Try again.
                        continue;
                    }
                }
                // Nothing on the queue.
                return nullptr;
            }

            if (CARB_UNLIKELY(head == mediator))
            {
                // Another thread is in a pop function. Wait until m_head is no longer the mediator.
                this_thread::spinWaitWithBackoff([&] { return this->mediated(); });
                // Try again.
                continue;
            }
            break;
        }

        // Restore the head pointer to a sane value before returning.
        // If 'next' is nullptr, then this item _might_ be the last item.
        T* n = next(head).load(std::memory_order_relaxed);

        if (CARB_UNLIKELY(!n))
        {
            m_head.store(nullptr, std::memory_order_relaxed);
            // Try to clear the tail to ensure the queue is now empty.
            T* h = head;
            if (m_tail.compare_exchange_strong(h, nullptr, std::memory_order_release, std::memory_order_relaxed))
            {
                // Both head and tail are nullptr now.
                // Clear head's next pointer so that it's not dangling
                next(head).store(nullptr, std::memory_order_relaxed);
                return head;
            }
            // There must be a next item now.
            n = next(head).load(std::memory_order_acquire);
            if (CARB_UNLIKELY(!n))
                n = waitForEnqueue(next(head));
        }

        m_head.store(n, std::memory_order_relaxed);

        // Clear head's next pointer so that it's not dangling
        next(head).store(nullptr, std::memory_order_relaxed);
        return head;
    }

    /**
     * Pushes an item onto the queue and notifies a waiting listener.
     *
     * Equivalent to doing `auto b = push(p); notifyOne(); return b;`.
     *
     * @see push(), notifyOne()
     *
     * @param p The item to push.
     * @returns `true` if LocklessQueue was empty prior to push; `false` otherwise. Note that this is atomically correct
     * as opposed to calling isEmpty() before push().
     */
    bool pushNotify(T* p)
    {
        bool b = push(p);
        notifyOne();
        return b;
    }

    /**
     * Blocks the calling thread until an item is available and returns it (Single-consumer).
     *
     * Requires the item to be pushed with pushNotify(), notifyOne() or notifyAll().
     *
     * @thread_safety May only be done in a single thread and is mutually exclusive with all other functions that modify
     * LocklessQueue *except* push(). Use popMC() for a thread-safe pop function.
     *
     * @warning Debug builds will assert if a thread safety issue is detected.
     *
     * @see popSC(), wait()
     *
     * @returns The first item popped from the queue.
     */
    T* popSCWait()
    {
        T* p = popSC();
        while (!p)
        {
            wait();
            p = popSC();
        }
        return p;
    }

    /**
     * Blocks the calling thread until an item is available and returns it (Single-consumer) or a timeout elapses.
     *
     * Requires the item to be pushed with pushNotify(), notifyOne() or notifyAll().
     *
     * @thread_safety May only be done in a single thread and is mutually exclusive with all other functions that modify
     * LocklessQueue *except* push(). Use popMC() for a thread-safe pop function.
     *
     * @warning Debug builds will assert if a thread safety issue is detected.
     *
     * @see popSC(), waitFor()
     *
     * @param dur The duration to wait for an item to become available.
     * @returns The first item popped from the queue or `nullptr` if the timeout period elapses.
     */
    template <class Rep, class Period>
    T* popSCWaitFor(const std::chrono::duration<Rep, Period>& dur)
    {
        return popSCWaitUntil(std::chrono::steady_clock::now() + dur);
    }

    /**
     * Blocks the calling thread until an item is available and returns it (Single-consumer) or the clock reaches a time
     * point.
     *
     * Requires the item to be pushed with pushNotify(), notifyOne() or notifyAll().
     *
     * @thread_safety May only be done in a single thread and is mutually exclusive with all other functions that modify
     * LocklessQueue *except* push(). Use popMC() for a thread-safe pop function.
     *
     * @warning Debug builds will assert if a thread safety issue is detected.
     *
     * @see popSC(), waitUntil()
     *
     * @param tp The time to wait until for an item to become available.
     * @returns The first item popped from the queue or `nullptr` if the timeout period elapses.
     */
    template <class Clock, class Duration>
    T* popSCWaitUntil(const std::chrono::time_point<Clock, Duration>& tp)
    {
        T* p = popSC();
        while (!p)
        {
            if (!waitUntil(tp))
            {
                return popSC();
            }
            p = popSC();
        }
        return p;
    }

    /**
     * Blocks the calling thread until an item is available and returns it (Multiple-consumer).
     *
     * Requires the item to be pushed with pushNotify(), notifyOne() or notifyAll().
     *
     * @note In a highly-contentious situation, this function will back off and attempt to sleep in order to resolve the
     * contention.
     *
     * @see popMC(), wait()
     *
     * @returns the first item removed from the queue.
     */
    T* popMCWait()
    {
        T* p = popMC();
        while (!p)
        {
            wait();
            p = popMC();
        }
        return p;
    }

    /**
     * Blocks the calling thread until an item is available and returns it (Multiple-consumer) or a timeout elapses.
     *
     * Requires the item to be pushed with pushNotify(), notifyOne() or notifyAll().
     *
     * @note In a highly-contentious situation, this function will back off and attempt to sleep in order to resolve the
     * contention.
     *
     * @see popMC(), waitFor()
     *
     * @param dur The duration to wait for an item to become available.
     * @returns the first item removed from the queue or `nullptr` if the timeout period elapses.
     */
    template <class Rep, class Period>
    T* popMCWaitFor(const std::chrono::duration<Rep, Period>& dur)
    {
        return popMCWaitUntil(std::chrono::steady_clock::now() + dur);
    }

    /**
     * Blocks the calling thread until an item is available and returns it (Multiple-consumer) or the clock reaches a
     * time point.
     *
     * Requires the item to be pushed with pushNotify(), notifyOne() or notifyAll().
     *
     * @note In a highly-contentious situation, this function will back off and attempt to sleep in order to resolve the
     * contention.
     *
     * @see popMC(), waitUntil()
     *
     * @param tp The time to wait until for an item to become available.
     * @returns the first item removed from the queue or `nullptr` if the timeout period elapses.
     */
    template <class Clock, class Duration>
    T* popMCWaitUntil(const std::chrono::time_point<Clock, Duration>& tp)
    {
        T* p = popMC();
        while (!p)
        {
            if (!waitUntil(tp))
            {
                return popMC();
            }
            p = popMC();
        }
        return p;
    }

    /**
     * Waits until the queue is non-empty.
     *
     * @note Requires notification that the queue is non-empty, such as from pushNotify(), notifyOne() or notifyAll().
     *
     * @note Though wait() returns, another thread may have popped the available item making the queue empty again. Use
     * popSCWait() / popMCWait() if it is desired to ensure that the current thread can obtain an item.
     */
    void wait()
    {
        m_tail.wait(nullptr, std::memory_order_relaxed);
    }

    /**
     * Waits until LocklessQueue is non-empty or a specified duration has passed.
     *
     * @note Though waitFor() returns `true`, another thread may have popped the available item making the queue empty
     * again. Use popSCWaitFor() / popMCWaitFor() if it is desired to ensure that the current thread can obtain an item.
     *
     * @note Requires notification that the queue is non-empty, such as from pushNotify(), notifyOne() or notifyAll().
     *
     * @param dur The duration to wait for an item to become available.
     * @returns `true` if an item appears to be available; `false` if the timeout elapses.
     */
    template <class Rep, class Period>
    bool waitFor(const std::chrono::duration<Rep, Period>& dur)
    {
        return m_tail.wait_for(nullptr, dur, std::memory_order_relaxed);
    }

    /**
     * Waits until LocklessQueue is non-empty or a specific time is reached.
     *
     * @note Though waitUntil() returns `true`, another thread may have popped the available item making the queue empty
     * again. Use popSCWaitUntil() / popMCWaitUntil() if it is desired to ensure that the current thread can obtain an
     * item.
     *
     * @note Requires notification that the queue is non-empty, such as from pushNotify(), notifyOne() or notifyAll().
     *
     * @param tp The time to wait until for an item to become available.
     * @returns `true` if an item appears to be available; `false` if the time is reached.
     */
    template <class Clock, class Duration>
    bool waitUntil(const std::chrono::time_point<Clock, Duration>& tp)
    {
        return m_tail.wait_until(nullptr, tp, std::memory_order_relaxed);
    }

    /**
     * Notifies a single waiting thread.
     *
     * Notifies a single thread waiting in wait(), waitFor(), waitUntil(), popSCWait(), popMCWait(), popSCWaitFor(),
     * popMCWaitFor(), popSCWaitUntil(), or popMCWaitUntil() to wake and check the queue.
     */
    void notifyOne()
    {
        m_tail.notify_one();
    }

    /**
     * Notifies all waiting threads.
     *
     * Notifies all threads waiting in wait(), waitFor(), waitUntil(), popSCWait(), popMCWait(), popSCWaitFor(),
     * popMCWaitFor(), popSCWaitUntil(), or popMCWaitUntil() to wake and check the queue.
     */
    void notifyAll()
    {
        m_tail.notify_all();
    }

private:
    CARB_VIZ cpp20::atomic<T*> m_head;
    CARB_VIZ cpp20::atomic<T*> m_tail;

    constexpr static unsigned kWaitSpins = 1024;

    void _moveTo(LocklessQueue& rhs)
    {
        static T* const mediator = (T*)(size_t)-1;

        T* head;
        for (;;)
        {
            // The mediator acts as both a lock and a signal
            head = m_head.exchange(mediator, std::memory_order_relaxed);

            if (CARB_UNLIKELY(!head))
            {
                // The first xchg on the tail will tell the enqueuing thread that it is safe to blindly write out to the
                // head pointer. A cmpxchg honors the algorithm.
                T* m = mediator;
                if (CARB_UNLIKELY(!m_head.compare_exchange_strong(
                        m, nullptr, std::memory_order_relaxed, std::memory_order_relaxed)))
                {
                    // Couldn't write a nullptr back. Try again.
                    continue;
                }
                if (CARB_UNLIKELY(!!m_tail.load(std::memory_order_relaxed)))
                {
                    bool isNull;
                    // Wait for consistency
                    this_thread::spinWaitWithBackoff([&] { return this->consistent(isNull); });
                    if (!isNull)
                    {
                        // Try again.
                        continue;
                    }
                }
                // Nothing on the queue.
                return;
            }

            if (CARB_UNLIKELY(head == mediator))
            {
                // Another thread is in a pop function. Wait until m_head is no longer the mediator.
                this_thread::spinWaitWithBackoff([&] { return this->mediated(); });
                // Try again.
                continue;
            }
            break;
        }

        // Release our lock and swap with the tail
        m_head.store(nullptr, std::memory_order_release);
        T* e = m_tail.exchange(nullptr, std::memory_order_release);

        // rhs must be empty
        T* old;
        CARB_UNUSED(old);
        old = rhs.m_head.exchange(head, std::memory_order_relaxed);
        CARB_ASSERT(old == nullptr);
        old = rhs.m_tail.exchange(e, std::memory_order_release);
        CARB_ASSERT(old == nullptr);
    }

    bool _push(T* first, T* last)
    {
        // Swap the tail with our new last item
        T* token = m_tail.exchange(last, std::memory_order_release);
        CARB_ASSERT(token != last);
        if (CARB_LIKELY(token))
        {
            // The previous tail item now points to our new first item.
            next(token).store(first, std::memory_order_relaxed);
            return false;
        }
        else
        {
            // Queue was empty; head points to our first item
            m_head.store(first, std::memory_order_relaxed);
            return true;
        }
    }

    // This function name breaks naming paradigms so that it shows up prominently in stack traces.
    CARB_NOINLINE T* __WAIT_FOR_ENQUEUE__(std::atomic<T*>& ptr)
    {
        T* val;
        int spins = 0;
        while ((val = ptr.load(std::memory_order_relaxed)) == nullptr)
        {
            spins++;
            CARB_UNUSED(spins);
            std::this_thread::yield();
        }
        return val;
    }

    T* waitForEnqueue(std::atomic<T*>& ptr)
    {
        unsigned spins = kWaitSpins;
        T* val;
        while (CARB_UNLIKELY(spins-- > 0))
        {
            if (CARB_LIKELY((val = ptr.load(std::memory_order_relaxed)) != nullptr))
                return val;
            CARB_HARDWARE_PAUSE();
        }
        return __WAIT_FOR_ENQUEUE__(ptr);
    }

    // Predicate: returns `false` until m_head and m_tail are both null or non-null
    bool consistent(bool& isNull) const
    {
        T* h = m_head.load(std::memory_order_relaxed);
        T* t = m_tail.load(std::memory_order_relaxed);
        isNull = !t;
        return !h == !t;
    }

    // Predicate: returns Ready when _head is no longer the mediator
    bool mediated() const
    {
        static T* const mediator = (T*)(size_t)(-1);
        return m_head.load(std::memory_order_relaxed) != mediator;
    }

    static std::atomic<T*>& next(T* p)
    {
        return (p->*U).m_next;
    }
};

} // namespace container

} // namespace carb
