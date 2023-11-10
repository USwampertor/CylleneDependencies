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
//! @brief Defines the LocklessStack class.
#pragma once

#include "../Defines.h"
#include "../cpp20/Atomic.h"
#include "../thread/Mutex.h"
#include "../thread/Util.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

#if CARB_POSIX
#    include <dlfcn.h>
#endif

#if CARB_PLATFORM_WINDOWS || defined(DOXYGEN_BUILD)
//! On non-Windows platforms we fallback to a non-Lockless version that uses a mutex. This is because handling a pop()
//! requires potentially catching a SIGSEGV to restart the operation (similar to a cmpxchg failing). On Windows this is
//! easily done with SEH (Structured Exception Handling), but on Linux this requires a signal handler. If this file is
//! compiled into multiple dynamic objects then multiple signal handlers will be registered. Furthermore, the signal
//! handler of the currently active LocklessStack needs to be at the beginning of the handler chain. This is untenable
//! from a performance perspective. Therefore we use a mutex to ensure that all reads will be safe.
#    define CARB_LOCKLESSSTACK_IS_LOCKLESS 1
#else
#    define CARB_LOCKLESSSTACK_IS_LOCKLESS 0
#endif

namespace carb
{
namespace container
{

template <class T>
class LocklessStackLink;
template <class T, LocklessStackLink<T> T::*U>
class LocklessStack;

#ifndef DOXYGEN_BUILD
namespace detail
{
template <class T, LocklessStackLink<T> T::*U>
class LocklessStackHelpers;
template <class T, LocklessStackLink<T> T::*U>
class LocklessStackBase;
} // namespace detail
#endif

/**
 * Defines the link object. Each class contained in LocklessStack must have a member of type LocklessStackLink<T>. A
 * pointer to this member is required as the second parameter for LocklessStack.
 */
template <class T>
class LocklessStackLink
{
public:
    /**
     * Default constructor.
     */
    constexpr LocklessStackLink() = default;

private:
    CARB_VIZ LocklessStackLink<T>* m_next;

    friend T;
    template <class U, LocklessStackLink<U> U::*V>
    friend class detail::LocklessStackHelpers;
    template <class U, LocklessStackLink<U> U::*V>
    friend class detail::LocklessStackBase;
    template <class U, LocklessStackLink<U> U::*V>
    friend class LocklessStack;
};

#if !defined(DOXYGEN_BUILD)
namespace detail
{

#    if CARB_LOCKLESSSTACK_IS_LOCKLESS
class SignalHandler
{
public:
    static bool readNext(void** out, void* in)
    {
        // We do this in a SEH block (on Windows) because it's possible (though rare) that another thread could have
        // already popped and destroyed `cur` which would cause EXCEPTION_ACCESS_VIOLATION. By handling it in an
        // exception handler, we recover cleanly and try again. On 64-bit Windows, there is zero cost unless an
        // exception is thrown, at which point the kernel will find the Exception info and Unwind Info for the function
        // that we're in.
        __try
        {
            *out = *(void**)in;
            return true;
        }
        __except (1)
        {
            return false;
        }
    }
};
#    endif

template <class T, LocklessStackLink<T> T::*U>
class LocklessStackHelpers
{
public:
    // Access the LocklessStackLink member of `p`
    static LocklessStackLink<T>* link(T* p)
    {
        return std::addressof(p->*U);
    }

    // Converts a LocklessStackLink to the containing object
    static T* convert(LocklessStackLink<T>* p)
    {
        // We need to calculate the offset of our link member and calculate where T is.
        // Note that this doesn't work if T uses virtual inheritance
        size_t offset = (size_t) reinterpret_cast<char*>(&(((T*)0)->*U));
        return reinterpret_cast<T*>(reinterpret_cast<char*>(p) - offset);
    }
};

// Base implementations
#    if !CARB_LOCKLESSSTACK_IS_LOCKLESS
template <class T, LocklessStackLink<T> T::*U>
class LocklessStackBase : protected LocklessStackHelpers<T, U>
{
    using Base = LocklessStackHelpers<T, U>;

public:
    bool _isEmpty() const
    {
        return !m_head.load();
    }

    bool _push(T* first, T* last)
    {
        std::lock_guard<Lock> g(m_lock);
        // Relaxed because under the lock
        Base::link(last)->m_next = m_head.load(std::memory_order_relaxed);
        bool const wasEmpty = !m_head.load(std::memory_order_relaxed);
        m_head.store(Base::link(first), std::memory_order_relaxed);
        return wasEmpty;
    }

    T* _popOne()
    {
        std::unique_lock<Lock> g(m_lock);
        // Relaxed because under the lock
        auto cur = m_head.load(std::memory_order_relaxed);
        if (!cur)
        {
            return nullptr;
        }
        m_head.store(cur->m_next, std::memory_order_relaxed);
        return Base::convert(cur);
    }

    T* _popAll()
    {
        std::lock_guard<Lock> g(m_lock);
        // Relaxed because under the lock
        LocklessStackLink<T>* head = m_head.exchange(nullptr, std::memory_order_relaxed);
        return head ? Base::convert(head) : nullptr;
    }

    void _wait()
    {
        auto p = m_head.load();
        while (!p)
        {
            m_head.wait(p);
            p = m_head.load();
        }
    }

    template <class Rep, class Period>
    bool _waitFor(const std::chrono::duration<Rep, Period>& dur)
    {
        return _waitUntil(std::chrono::steady_clock::now() + dur);
    }

    template <class Clock, class Duration>
    bool _waitUntil(const std::chrono::time_point<Clock, Duration>& tp)
    {
        auto p = m_head.load();
        while (!p)
        {
            if (!m_head.wait_until(p, tp))
                return false;
            p = m_head.load();
        }
        return true;
    }

    void _notifyOne()
    {
        m_head.notify_one();
    }

    void _notifyAll()
    {
        m_head.notify_all();
    }

private:
    // Cannot be lockless if we don't have SEH
    // Testing reveals that mutex is significantly faster than spinlock in highly-contended cases.
    using Lock = carb::thread::mutex;
    Lock m_lock;
    cpp20::atomic<LocklessStackLink<T>*> m_head{ nullptr };
};
#    else
// Windows implementation: requires SEH and relies upon the fact that aligned pointers on modern OSes don't
// use at least 10 bits of the 64-bit space, so it uses those bits as a sequence number to ensure uniqueness between
// different threads competing to pop.
template <class T, LocklessStackLink<T> T::*U>
class LocklessStackBase : protected LocklessStackHelpers<T, U>
{
    using Base = LocklessStackHelpers<T, U>;

public:
    constexpr LocklessStackBase()
    {
    }
    bool _isEmpty() const
    {
        return !decode(m_head.load(std::memory_order_acquire));
    }

    bool _push(T* first, T* last)
    {
        // All OS bits should either be zero or one, and it needs to be 8-byte-aligned.
        LocklessStackLink<T>* lnk = Base::link(first);
        CARB_ASSERT((size_t(lnk) & kCPUMask) == 0 || (size_t(lnk) & kCPUMask) == kCPUMask, "Unexpected OS bits set");
        CARB_ASSERT((size_t(lnk) & ((1 << 3) - 1)) == 0, "Pointer not aligned properly");

        uint16_t seq;
        uint64_t expected = m_head.load(std::memory_order_acquire), temp;
        decltype(lnk) next;
        do
        {
            next = decode(expected, seq);
            Base::link(last)->m_next = next;
            temp = encode(lnk, seq + 1); // Increase sequence
        } while (CARB_UNLIKELY(
            !m_head.compare_exchange_strong(expected, temp, std::memory_order_release, std::memory_order_relaxed)));
        return !next;
    }

    T* _popOne()
    {
        uint64_t expected = m_head.load(std::memory_order_acquire);
        LocklessStackLink<T>* cur;
        uint16_t seq;

        bool isNull = false;

        this_thread::spinWaitWithBackoff([&] {
            cur = decode(expected, seq);
            if (!cur)
            {
                // End attempts because the stack is empty
                isNull = true;
                return true;
            }

            // Attempt to read the next value
            LocklessStackLink<T>* newhead;
            if (!detail::SignalHandler::readNext((void**)&newhead, cur))
            {
                // Another thread changed `cur`, so reload and try again.
                expected = m_head.load(std::memory_order_acquire);
                return false;
            }

            // Only push needs to increase `seq`
            uint64_t temp = encode(newhead, seq);
            return m_head.compare_exchange_strong(expected, temp, std::memory_order_release, std::memory_order_relaxed);
        });

        return isNull ? nullptr : Base::convert(cur);
    }

    T* _popAll()
    {
        uint16_t seq;
        uint64_t expected = m_head.load(std::memory_order_acquire), temp;
        for (;;)
        {
            LocklessStackLink<T>* head = decode(expected, seq);
            if (!head)
            {
                return nullptr;
            }

            // Keep the same sequence since only push() needs to increment the sequence
            temp = encode(nullptr, seq);
            if (CARB_LIKELY(
                    m_head.compare_exchange_weak(expected, temp, std::memory_order_release, std::memory_order_relaxed)))
            {
                return Base::convert(head);
            }
        }
    }

    void _wait()
    {
        uint64_t head = m_head.load(std::memory_order_acquire);
        while (!decode(head))
        {
            m_head.wait(head, std::memory_order_relaxed);
            head = m_head.load(std::memory_order_acquire);
        }
    }

    template <class Rep, class Period>
    bool _waitFor(const std::chrono::duration<Rep, Period>& dur)
    {
        return _waitUntil(std::chrono::steady_clock::now() + dur);
    }

    template <class Clock, class Duration>
    bool _waitUntil(const std::chrono::time_point<Clock, Duration>& tp)
    {
        uint64_t head = m_head.load(std::memory_order_acquire);
        while (!decode(head))
        {
            if (!m_head.wait_until(head, tp, std::memory_order_relaxed))
            {
                return false;
            }
            head = m_head.load(std::memory_order_acquire);
        }
        return true;
    }

    void _notifyOne()
    {
        m_head.notify_one();
    }

    void _notifyAll()
    {
        m_head.notify_all();
    }

private:
    // On 64-bit architectures, we make use of the fact that CPUs only use a certain number of address bits.
    // Intel CPUs require that these 8 to 16 most-significant-bits match (all 1s or 0s). Since 8 appears to be the
    // lowest common denominator, we steal 7 bits (to save the value of one of the bits so that they can match) for a
    // sequence number. The sequence is important as it causes the resulting stored value to change even if the stack is
    // pushing and popping the same value.
    //
    // Pointer compression drops the `m` and `z` bits from the pointer. `m` are expected to be consistent (all 1 or 0)
    // and match the most-significant `P` bit. `z` are expected to be zeros:
    //
    // 63 ------------------------------ BITS ------------------------------ 0
    // mmmmmmmP PPPPPPPP PPPPPPPP PPPPPPPP PPPPPPPP PPPPPPPP PPPPPPPP PPPPPzzz
    //
    // `m_head` is encoded as the shifted compressed pointer bits `P` with sequence bits `s`:
    // 63 ------------------------------ BITS ------------------------------ 0
    // PPPPPPPP PPPPPPPP PPPPPPPP PPPPPPPP PPPPPPPP PPPPPPPP PPPPPPss ssssssss

    static_assert(sizeof(size_t) == 8, "64-bit only");
    CARB_VIZ constexpr const static size_t kCpuBits = 7; // MSBs that are limited by CPU hardware and must match the
                                                         // 56th bit
    constexpr const static size_t kCPUMask = ((size_t(1) << (kCpuBits + 1)) - 1) << (63 - kCpuBits);
    CARB_VIZ constexpr const static size_t kSeqBits = kCpuBits + 3; // We also use the lowest 3 bits as part of the
                                                                    // sequence
    CARB_VIZ constexpr const static size_t kSeqMask = (size_t(1) << kSeqBits) - 1;
    CARB_VIZ cpp20::atomic_uint64_t m_head{ 0 };

    static LocklessStackLink<T>* decode(size_t val)
    {
        // Clear the `s` sequence bits and shift as a signed value to sign-extend so that the `m` bits are filled in to
        // match the most-significant `P` bit.
        return reinterpret_cast<LocklessStackLink<T>*>(ptrdiff_t(val & ~kSeqMask) >> kCpuBits);
    }
    static LocklessStackLink<T>* decode(size_t val, uint16_t& seq)
    {
        seq = val & kSeqMask;
        return decode(val);
    }
    static size_t encode(LocklessStackLink<T>* p, uint16_t seq)
    {
        // Shift the pointer value, dropping the most significant `m` bits and write the sequence number over the `z`
        // and space created in the least-significant area.
        return ((reinterpret_cast<size_t>(p) << kCpuBits) & ~kSeqMask) + (seq & uint16_t(kSeqMask));
    }
};
#    endif

} // namespace detail
#endif

/**
 * @brief Implements a lockless stack: a LIFO container that is thread-safe yet requires no kernel involvement.
 *
 * LocklessStack is designed to be easy-to-use. For a class `Foo` that you want to be contained in a LocklessStack, it
 * must have a member of type LocklessStackLink<Foo>. This member is what the LocklessStack will use for tracking data.
 *
 * Pushing to LocklessStack is simply done through LocklessStack::push(), which is entirely thread-safe. LocklessStack
 * ensures last-in-first-out (LIFO) for each producer pushing to LocklessStack. Multiple producers may be pushing to
 * LocklessStack simultaneously, so their items can become mingled, but each producer's pushed items will be strongly
 * ordered.
 *
 * Popping is done through LocklessStack::pop(), which is also entirely thread-safe. Multiple threads may all attempt to
 * pop from the same LocklessStack simultaneously.
 *
 * Simple example:
 * ```cpp
 * class Foo
 * {
 * public:
 *     LocklessStackLink<Foo> m_link;
 * };
 *
 * LocklessStack<Foo, &Foo::m_link> stack;
 * stack.push(new Foo);
 * Foo* p = stack.pop();
 * delete p;
 * ```
 *
 * @thread_safety LocklessStack is entirely thread-safe except where declared otherwise. No allocation happens with a
 * LocklessStack; instead the caller is responsible for construction/destruction of contained objects.
 *
 * \warning LocklessStack on non-Windows implementations is not Lockless (i.e. uses a mutex to synchronize). For an
 * explanation as to why, see \ref CARB_LOCKLESSSTACK_IS_LOCKLESS.
 *
 * @tparam T The type to contain.
 * @tparam U A pointer-to-member of a LocklessStackLink member within T (see above example).
 */
template <class T, LocklessStackLink<T> T::*U>
class LocklessStack final : protected detail::LocklessStackBase<T, U>
{
    using Base = detail::LocklessStackBase<T, U>;

public:
    /**
     * Constructor.
     */
    constexpr LocklessStack() = default;

    /**
     * Destructor.
     *
     * Asserts that isEmpty() returns true.
     */
    ~LocklessStack()
    {
        // Ensure the stack is empty
        CARB_ASSERT(isEmpty());
    }

    /**
     * Indicates whether the stack is empty.
     *
     * @warning Another thread may have modified the LocklessStack before this function returns.
     *
     * @returns `true` if the stack appears empty; `false` if items appear to exist in the stack.
     */
    bool isEmpty() const
    {
        return Base::_isEmpty();
    }

    /**
     * Pushes an item onto the stack.
     *
     * @param p The item to push onto the stack.
     *
     * @return `true` if the stack was previously empty prior to push; `false` otherwise. Note that this is atomically
     * correct as opposed to checking isEmpty() before push().
     */
    bool push(T* p)
    {
        return Base::_push(p, p);
    }

    /**
     * Pushes a contiguous block of entries from [ @p begin, @p end) onto the stack.
     *
     * @note All of the entries are guaranteed to remain strongly ordered and will not be interspersed with entries from
     * other threads. @p begin will be popped from the stack first.
     *
     * @param begin An <a href="https://en.cppreference.com/w/cpp/named_req/InputIterator">InputIterator</a> of the
     * first item to push. `*begin` must resolve to a `T&`.
     * @param end An off-the-end InputIterator after the last item to push.
     * @returns `true` if the stack was empty prior to push; `false` otherwise. Note that this is atomically correct
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
        if (begin == end)
        {
            return false;
        }

        // Walk the list and have them point to each other
        InputItRef last = begin;
        InputItRef iter = begin;
        for (iter++; iter != end; last = iter++)
        {
            Base::link(std::addressof(*last))->m_next = Base::link(std::addressof(*iter));
        }

        return Base::_push(std::addressof(*begin), std::addressof(*last));
    }

    /**
     * Pushes a block of pointers-to-entries from [ @p begin, @p end) onto the stack.
     *
     * @note All of the entries are guaranteed to remain strongly ordered and will not be interspersed with entries from
     * other threads. @p begin will be popped from the stack first.
     *
     * @param begin An <a href="https://en.cppreference.com/w/cpp/named_req/InputIterator">InputIterator</a> of the
     * first item to push. `*begin` must resolve to a `T*`.
     * @param end An off-the-end InputIterator after the last item to push.
     * @returns `true` if the stack was empty prior to push; `false` otherwise. Note that this is atomically correct
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
        if (begin == end)
        {
            return false;
        }

        // Walk the list and have them point to each other
        InputItPtr last = begin;
        InputItPtr iter = begin;
        for (iter++; iter != end; last = iter++)
        {
            Base::link(*last)->m_next = Base::link(*iter);
        }

        return Base::_push(*begin, *last);
    }

    /**
     * Pops an item from the top of the stack if available.
     *
     * @return An item popped from the stack. If the stack was empty, then `nullptr` is returned.
     */
    T* pop()
    {
        return Base::_popOne();
    }

    /**
     * Empties the stack.
     *
     * @note To perform an action on each item as it is popped, use forEach() instead.
     */
    void popAll()
    {
        Base::_popAll();
    }

    /**
     * Pops all available items from the stack calling a functionish object on each.
     *
     * First, pops all available items from `*this` and then calls @p f on each.
     *
     * @note As the pop is the first thing that happens, any new entries that get pushed while the function is executing
     * will NOT be popped and will remain in the stack when this function returns.
     *
     * @param f A functionish object that accepts a `T*` parameter. Called for each item that was popped from the stack.
     */
    template <class Func>
    void forEach(Func&& f)
    {
        T* p = Base::_popAll();
        LocklessStackLink<T>* h = p ? Base::link(p) : nullptr;
        while (h)
        {
            p = Base::convert(h);
            h = h->m_next;
            f(p);
        }
    }

    /**
     * Pushes an item onto the stack and notifies a waiting listener.
     *
     * Equivalent to doing `auto b = push(p); notifyOne(); return b;`.
     *
     * @see push(), notifyOne()
     *
     * @param p The item to push.
     * @returns `true` if the stack was empty prior to push; `false` otherwise. Note that this is atomically correct
     * as opposed to calling isEmpty() before push().
     */
    bool pushNotify(T* p)
    {
        bool b = push(p);
        notifyOne();
        return b;
    }

    /**
     * Blocks the calling thread until an item is available and returns it.
     *
     * Requires the item to be pushed with pushNotify(), notifyOne() or notifyAll().
     *
     * @see pop(), wait()
     *
     * @returns The first item popped from the stack.
     */
    T* popWait()
    {
        T* p = pop();
        while (!p)
        {
            wait();
            p = pop();
        }
        return p;
    }

    /**
     * Blocks the calling thread until an item is available and returns it or a timeout elapses.
     *
     * Requires the item to be pushed with pushNotify(), notifyOne() or notifyAll().
     *
     * @see pop(), waitFor()
     *
     * @param dur The duration to wait for an item to become available.
     * @returns the first item removed from the stack or `nullptr` if the timeout period elapses.
     */
    template <class Rep, class Period>
    T* popWaitFor(const std::chrono::duration<Rep, Period>& dur)
    {
        return popWaitUntil(std::chrono::steady_clock::now() + dur);
    }

    /**
     * Blocks the calling thread until an item is available and returns it or the clock reaches a time point.
     *
     * Requires the item to be pushed with pushNotify(), notifyOne() or notifyAll().
     *
     * @see pop(), waitUntil()
     *
     * @param tp The time to wait until for an item to become available.
     * @returns the first item removed from the stack or `nullptr` if the timeout period elapses.
     */
    template <class Clock, class Duration>
    T* popWaitUntil(const std::chrono::time_point<Clock, Duration>& tp)
    {
        T* p = pop();
        while (!p)
        {
            if (!waitUntil(tp))
            {
                return pop();
            }
            p = pop();
        }
        return p;
    }

    /**
     * Waits until the stack is non-empty.
     *
     * @note Requires notification that the stack is non-empty, such as from pushNotify(), notifyOne() or notifyAll().
     *
     * @note Though wait() returns, another thread may have popped the available item making the stack empty again. Use
     * popWait() if it is desired to ensure that the current thread can obtain an item.
     */
    void wait()
    {
        Base::_wait();
    }

    /**
     * Waits until the stack is non-empty or a specified duration has passed.
     *
     * @note Though waitFor() returns `true`, another thread may have popped the available item making the stack empty
     * again. Use popWaitFor() if it is desired to ensure that the current thread can obtain an item.
     *
     * @note Requires notification that the stack is non-empty, such as from pushNotify(), notifyOne() or notifyAll().
     *
     * @param dur The duration to wait for an item to become available.
     * @returns `true` if an item appears to be available; `false` if the timeout elapses.
     */
    template <class Rep, class Period>
    bool waitFor(const std::chrono::duration<Rep, Period>& dur)
    {
        return Base::_waitFor(dur);
    }

    /**
     * Waits until the stack is non-empty or a specific time is reached.
     *
     * @note Though waitUntil() returns `true`, another thread may have popped the available item making the stack empty
     * again. Use popWaitUntil() if it is desired to ensure that the current thread can obtain an item.
     *
     * @note Requires notification that the stack is non-empty, such as from pushNotify(), notifyOne() or notifyAll().
     *
     * @param tp The time to wait until for an item to become available.
     * @returns `true` if an item appears to be available; `false` if the time is reached.
     */
    template <class Clock, class Duration>
    bool waitUntil(const std::chrono::time_point<Clock, Duration>& tp)
    {
        return Base::_waitUntil(tp);
    }

    /**
     * Notifies a single waiting thread.
     *
     * Notifies a single thread waiting in wait(), waitFor(), waitUntil(), popWait(), popWaitFor(), or popWaitUntil() to
     * wake and check the stack.
     */
    void notifyOne()
    {
        Base::_notifyOne();
    }

    /**
     * Notifies all waiting threads.
     *
     * Notifies all threads waiting in wait(), waitFor(), waitUntil(), popWait(), popWaitFor(), or popWaitUntil() to
     * wake and check the stack.
     */
    void notifyAll()
    {
        Base::_notifyAll();
    }
};

} // namespace container
} // namespace carb
