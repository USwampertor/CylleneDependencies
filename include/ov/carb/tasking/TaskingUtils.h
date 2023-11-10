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
//! @brief carb.tasking utilities.
#pragma once

#include "ITasking.h"

#include "../thread/Util.h"

#include <atomic>
#include <condition_variable> // for std::cv_status
#include <functional>

namespace carb
{
namespace tasking
{

class RecursiveSharedMutex;

/**
 * This atomic spin lock conforms to C++ Named Requirements of <a
 * href="https://en.cppreference.com/w/cpp/named_req/Lockable">Lockable</a> which makes it compatible with
 * std::lock_guard.
 */
struct SpinMutex
{
public:
    /**
     * Constructs the SpinMutex.
     */
    constexpr SpinMutex() noexcept = default;

    CARB_PREVENT_COPY_AND_MOVE(SpinMutex);

    /**
     * Spins until the lock is acquired.
     *
     * See § 30.4.1.2.1 in the C++11 standard.
     */
    void lock() noexcept
    {
        this_thread::spinWaitWithBackoff([&] { return try_lock(); });
    }

    /**
     * Attempts to acquire the lock, on try, returns true if the lock was acquired.
     */
    bool try_lock() noexcept
    {
        return (!mtx.load(std::memory_order_relaxed) && !mtx.exchange(true, std::memory_order_acquire));
    }

    /**
     * Unlocks, wait-free.
     *
     * See § 30.4.1.2.1 in the C++11 standard.
     */
    void unlock() noexcept
    {
        mtx.store(false, std::memory_order_release);
    }

private:
    std::atomic_bool mtx{};
};

/**
 * Spin lock conforming to C++ named requirements of <a
 * href="https://en.cppreference.com/w/cpp/named_req/SharedMutex">SharedMutex</a>.
 *
 * @warning This implementation is non-recursive.
 */
struct SpinSharedMutex
{
public:
    /**
     * Constructor.
     */
    constexpr SpinSharedMutex() = default;

    CARB_PREVENT_COPY_AND_MOVE(SpinSharedMutex);

    /**
     * Spins until the shared mutex is exclusive-locked
     *
     * @warning It is an error to lock recursively or shared-lock when exclusive-locked, or vice versa.
     */
    void lock()
    {
        while (!try_lock())
        {
            CARB_HARDWARE_PAUSE();
        }
    }

    /**
     * Attempts to exclusive-lock the shared mutex immediately without spinning.
     *
     * @warning It is an error to lock recursively or shared-lock when exclusive-locked, or vice versa.
     * @returns true if the mutex was exclusive-locked; false if no exclusive lock could be obtained.
     */
    bool try_lock()
    {
        int expected = 0;
        return counter.compare_exchange_strong(expected, -1, std::memory_order_acquire, std::memory_order_relaxed);
    }

    /**
     * Unlocks the mutex previously exclusive-locked by this thread/task.
     *
     * @warning It is undefined behavior to unlock a mutex that is not owned by the current thread or task.
     */
    void unlock()
    {
        CARB_ASSERT(counter == -1);
        counter.store(0, std::memory_order_release);
    }

    /**
     * Attempts to shared-lock the shared mutex immediately without spinning.
     *
     * @warning It is an error to lock recursively or shared-lock when exclusive-locked, or vice versa.
     * @returns true if the mutex was shared-locked; false if no shared lock could be obtained.
     */
    bool try_lock_shared()
    {
        auto ctr = counter.load(std::memory_order_relaxed);
        if (ctr >= 0)
        {
            return counter.compare_exchange_strong(ctr, ctr + 1, std::memory_order_acquire, std::memory_order_relaxed);
        }
        return false;
    }

    /**
     * Spins until the shared mutex is shared-locked
     *
     * @warning It is an error to lock recursively or shared-lock when exclusive-locked, or vice versa.
     */
    void lock_shared()
    {
        auto ctr = counter.load(std::memory_order_relaxed);
        for (;;)
        {
            if (ctr < 0)
            {
                CARB_HARDWARE_PAUSE();
                ctr = counter.load(std::memory_order_relaxed);
            }
            else if (counter.compare_exchange_strong(ctr, ctr + 1, std::memory_order_acquire, std::memory_order_relaxed))
            {
                return;
            }
        }
    }

    /**
     * Unlocks the mutex previously shared-locked by this thread/task.
     *
     * @warning It is undefined behavior to unlock a mutex that is not owned by the current thread or task.
     */
    void unlock_shared()
    {
        int ctr = counter.fetch_sub(1, std::memory_order_release);
        CARB_ASSERT(ctr > 0);
        CARB_UNUSED(ctr);
    }

private:
    //   0 - unlocked
    // > 0 - Shared lock count
    //  -1 - Exclusive lock
    std::atomic<int> counter{ 0 };
};

/**
 * Wrapper for a carb::tasking::Counter
 */
class CounterWrapper
{
public:
    /**
     * Constructs a new CounterWrapper.
     *
     * @param target An optional (default:0) target value for the Counter to become signaled.
     */
    CounterWrapper(uint32_t target = 0)
        : m_counter(carb::getCachedInterface<ITasking>()->createCounterWithTarget(target))
    {
    }

    /**
     * Constructs a new CounterWrapper.
     *
     * @note Deprecated: The ITasking* parameter is no longer needed in this call.
     * @param tasking The acquired ITasking interface.
     * @param target An optional (default:0) target value for the Counter to become signaled.
     */
    CARB_DEPRECATED("ITasking no longer needed.")
    CounterWrapper(ITasking* tasking, uint32_t target = 0)
        : m_counter(carb::getCachedInterface<ITasking>()->createCounterWithTarget(target))
    {
        CARB_UNUSED(tasking);
    }

    /**
     * Destrutor
     *
     * @warning Destroying a Counter that is not signaled will assert in debug builds.
     */
    ~CounterWrapper()
    {
        carb::getCachedInterface<ITasking>()->destroyCounter(m_counter);
    }

    /**
     * @returns true if the Counter is signaled; false otherwise
     */
    CARB_DEPRECATED("The Counter interface is deprecated.") bool check() const
    {
        return try_wait();
    }

    /**
     * @returns true if the Counter is signaled; false otherwise
     */
    bool try_wait() const
    {
        return carb::getCachedInterface<ITasking>()->try_wait(m_counter);
    }

    /**
     * Blocks the current thread or task in a fiber-safe way until the Counter becomes signaled.
     */
    void wait() const
    {
        carb::getCachedInterface<ITasking>()->wait(m_counter);
    }

    /**
     * Blocks the current thread or task in a fiber-safe way until the Counter becomes signaled or a period of time has
     * elapsed.
     *
     * @param dur The amount of time to wait for.
     * @returns true if the Counter is signaled; false if the time period elapsed.
     */
    template <class Rep, class Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& dur) const
    {
        return carb::getCachedInterface<ITasking>()->wait_for(dur, m_counter);
    }

    /**
     * Blocks the current thread or task in a fiber-safe way until the Counter becomes signaled or the clock reaches the
     * given time point.
     *
     * @param tp The time point to wait until.
     * @returns true if the Counter is signaled; false if the clock time is reached.
     */
    template <class Clock, class Duration>
    bool wait_until(const std::chrono::time_point<Clock, Duration>& tp) const
    {
        return carb::getCachedInterface<ITasking>()->wait_until(tp, m_counter);
    }

    /**
     * Convertible to Counter*.
     */
    operator Counter*() const
    {
        return m_counter;
    }

    /**
     * Returns the acquired ITasking interface that was used to construct this object.
     * @note Deprecated: Use carb::getCachedInterface instead.
     */
    CARB_DEPRECATED("Use carb::getCachedInterface") ITasking* getTasking() const
    {
        return carb::getCachedInterface<ITasking>();
    }

    CARB_PREVENT_COPY_AND_MOVE(CounterWrapper);

private:
    Counter* m_counter;
};

/**
 * TaskGroup is a small and fast counter for tasks.
 *
 * TaskGroup blocks when tasks have "entered" the TaskGroup. It becomes signaled when all tasks have left the TaskGroup.
 */
class TaskGroup
{
public:
    CARB_PREVENT_COPY_AND_MOVE(TaskGroup);

    /**
     * Constructs an empty TaskGroup.
     */
    constexpr TaskGroup() = default;

    /**
     * TaskGroup destructor.
     *
     * @warning It is an error to destroy a TaskGroup that is not empty. Doing so can result in memory corruption.
     */
    ~TaskGroup()
    {
        CARB_CHECK(empty(), "Destroying busy TaskGroup!");
    }

    /**
     * Returns (with high probability) whether the TaskGroup is empty.
     *
     * As TaskGroup atomically tracks tasks, this function may return an incorrect value as another task may have
     * entered or left the TaskGroup before the return value could be processed.
     *
     * @returns `true` if there is high probability that the TaskGroup is empty (signaled); `false` otherwise.
     */
    bool empty() const
    {
        // This cannot be memory_order_relaxed because it does not synchronize with anything and would allow the
        // compiler to cache the value or hoist it out of a loop. Acquire semantics will require synchronization with
        // all other locations that release m_count.
        return m_count.load(std::memory_order_acquire) == 0;
    }

    /**
     * "Enters" the TaskGroup.
     *
     * @warning Every call to this function must be paired with leave(). It is generally better to use with().
     */
    void enter()
    {
        m_count.fetch_add(1, std::memory_order_acquire); // synchronizes-with all other locations releasing m_count
    }

    /**
     * "Leaves" the TaskGroup.
     *
     * @warning Every call to this function must be paired with an earlier enter() call. It is generally better to use
     * with().
     */
    void leave()
    {
        size_t v = m_count.fetch_sub(1, std::memory_order_release);
        CARB_ASSERT(v, "Mismatched enter()/leave() calls");
        if (v == 1)
        {
            carb::getCachedInterface<ITasking>()->futexWakeup(m_count, UINT_MAX);
        }
    }

    /**
     * Returns `true` if the TaskGroup is empty (signaled) with high probability.
     *
     * @returns `true` if there is high probability that the TaskGroup is empty (signaled); `false` otherwise.
     */
    bool try_wait() const
    {
        return empty();
    }

    /**
     * Blocks the calling thread or task until the TaskGroup becomes empty.
     */
    void wait() const
    {
        size_t v = m_count.load(std::memory_order_acquire); // synchronizes-with all other locations releasing m_count
        if (v)
        {
            ITasking* tasking = carb::getCachedInterface<ITasking>();
            while (v)
            {
                tasking->futexWait(m_count, v);
                v = m_count.load(std::memory_order_relaxed);
            }
        }
    }

    /**
     * Blocks the calling thread or task until the TaskGroup becomes empty or the given duration elapses.
     *
     * @param dur The duration to wait for.
     * @returns `true` if the TaskGroup has become empty; `false` if the duration elapses.
     */
    template <class Rep, class Period>
    bool try_wait_for(std::chrono::duration<Rep, Period> dur)
    {
        return try_wait_until(std::chrono::steady_clock::now() + dur);
    }

    /**
     * Blocks the calling thread or task until the TaskGroup becomes empty or the given time is reached.
     *
     * @param when The time to wait until.
     * @returns `true` if the TaskGroup has become empty; `false` if the given time is reached.
     */
    template <class Clock, class Duration>
    bool try_wait_until(std::chrono::time_point<Clock, Duration> when)
    {
        size_t v = m_count.load(std::memory_order_acquire); // synchronizes-with all other locations releasing m_count
        if (v)
        {
            ITasking* tasking = carb::getCachedInterface<ITasking>();
            while (v)
            {
                if (!tasking->futexWaitUntil(m_count, v, when))
                {
                    return false;
                }
                v = m_count.load(std::memory_order_relaxed);
            }
        }
        return true;
    }

    /**
     * A helper function for entering the TaskGroup during a call to `invoke()` and leaving afterwards.
     *
     * @param args Arguments to pass to `carb::cpp17::invoke`. The TaskGroup is entered (via \ref enter()) before the
     * invoke and left (via \ref leave()) when the invoke completes.
     * @returns the value returned by `carb::cpp17::invoke`.
     */
    template <class... Args>
    auto with(Args&&... args)
    {
        enter();
        CARB_SCOPE_EXIT
        {
            leave();
        };
        return carb::cpp17::invoke(std::forward<Args>(args)...);
    }

private:
    friend struct Tracker;
    friend struct RequiredObject;
    std::atomic_size_t m_count{ 0 };
};

/**
 * Wrapper for a carb::tasking::Mutex that conforms to C++ Named Requirements of <a
 * href="https://en.cppreference.com/w/cpp/named_req/Lockable">Lockable</a>.
 *
 * Non-recursive. If a recursive mutex is desired, use RecursiveMutexWrapper.
 */
class MutexWrapper
{
public:
    /**
     * Constructs a new MutexWrapper object
     */
    MutexWrapper() : m_mutex(carb::getCachedInterface<ITasking>()->createMutex())
    {
    }

    /**
     * Constructs a new MutexWrapper object
     * @note Deprecated: ITasking no longer needed.
     */
    CARB_DEPRECATED("ITasking no longer needed.")
    MutexWrapper(ITasking*) : m_mutex(carb::getCachedInterface<ITasking>()->createMutex())
    {
    }

    /**
     * Destructor
     *
     * @warning It is an error to destroy a mutex that is locked.
     */
    ~MutexWrapper()
    {
        carb::getCachedInterface<ITasking>()->destroyMutex(m_mutex);
    }

    /**
     * Attempts to lock the mutex immediately.
     *
     * @warning It is an error to lock recursively. Use RecursiveSharedMutex if recursive locking is desired.
     *
     * @returns true if the mutex was locked; false otherwise
     */
    bool try_lock()
    {
        return carb::getCachedInterface<ITasking>()->timedLockMutex(m_mutex, 0);
    }

    /**
     * Locks the mutex, waiting until it becomes available.
     *
     * @warning It is an error to lock recursively. Use RecursiveSharedMutex if recursive locking is desired.
     */
    void lock()
    {
        carb::getCachedInterface<ITasking>()->lockMutex(m_mutex);
    }

    /**
     * Unlocks a mutex previously acquired with try_lock() or lock()
     *
     * @warning It is undefined behavior to unlock a mutex that is not owned by the current thread or task.
     */
    void unlock()
    {
        carb::getCachedInterface<ITasking>()->unlockMutex(m_mutex);
    }

    /**
     * Attempts to lock a mutex within a specified duration.
     *
     * @warning It is an error to lock recursively. Use RecursiveSharedMutex if recursive locking is desired.
     *
     * @param duration The duration to wait for the mutex to be available
     * @returns true if the mutex was locked; false if the timeout period expired
     */
    template <class Rep, class Period>
    bool try_lock_for(const std::chrono::duration<Rep, Period>& duration)
    {
        return carb::getCachedInterface<ITasking>()->timedLockMutex(m_mutex, detail::convertDuration(duration));
    }

    /**
     * Attempts to lock a mutex waiting until a specific clock time.
     *
     * @warning It is an error to lock recursively. Use RecursiveSharedMutex if recursive locking is desired.
     *
     * @param time_point The clock time to wait until.
     * @returns true if the mutex was locked; false if the timeout period expired
     */
    template <class Clock, class Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& time_point)
    {
        return carb::getCachedInterface<ITasking>()->timedLockMutex(m_mutex, detail::convertAbsTime(time_point));
    }

    /**
     * Convertible to Mutex*.
     */
    operator Mutex*() const
    {
        return m_mutex;
    }

    /**
     * Returns the acquired ITasking interface that was used to construct this object.
     * @note Deprecated: Use carb::getCachedInterface instead.
     */
    CARB_DEPRECATED("Use carb::getCachedInterface") ITasking* getTasking() const
    {
        return carb::getCachedInterface<ITasking>();
    }

    CARB_PREVENT_COPY_AND_MOVE(MutexWrapper);

private:
    Mutex* m_mutex;
};

/**
 * Wrapper for a recursive carb::tasking::Mutex that conforms to C++ Named Requirements of <a
 * href="https://en.cppreference.com/w/cpp/named_req/Lockable">Lockable</a>.
 */
class RecursiveMutexWrapper
{
public:
    /**
     * Constructs a new RecursiveMutexWrapper object
     */
    RecursiveMutexWrapper() : m_mutex(carb::getCachedInterface<ITasking>()->createRecursiveMutex())
    {
    }

    /**
     * Constructs a new RecursiveMutexWrapper object
     * @note Deprecated: ITasking no longer needed.
     */
    CARB_DEPRECATED("ITasking no longer needed.")
    RecursiveMutexWrapper(ITasking*) : m_mutex(carb::getCachedInterface<ITasking>()->createRecursiveMutex())
    {
    }

    /**
     * Destructor
     *
     * @warning It is an error to destroy a mutex that is locked.
     */
    ~RecursiveMutexWrapper()
    {
        carb::getCachedInterface<ITasking>()->destroyMutex(m_mutex);
    }

    /**
     * Attempts to lock the mutex immediately.
     *
     * @returns true if the mutex was locked or already owned by this thread/task; false otherwise. If true is returned,
     * unlock() must be called to release the lock.
     */
    bool try_lock()
    {
        return carb::getCachedInterface<ITasking>()->timedLockMutex(m_mutex, 0);
    }

    /**
     * Locks the mutex, waiting until it becomes available. Call unlock() to release the lock.
     */
    void lock()
    {
        carb::getCachedInterface<ITasking>()->lockMutex(m_mutex);
    }

    /**
     * Unlocks a mutex previously acquired with try_lock() or lock()
     *
     * @note The unlock() function must be called for each successful lock.
     * @warning It is undefined behavior to unlock a mutex that is not owned by the current thread or task.
     */
    void unlock()
    {
        carb::getCachedInterface<ITasking>()->unlockMutex(m_mutex);
    }

    /**
     * Attempts to lock a mutex within a specified duration.
     *
     * @param duration The duration to wait for the mutex to be available
     * @returns true if the mutex was locked; false if the timeout period expired. If true is returned, unlock() must be
     * called to release the lock.
     */
    template <class Rep, class Period>
    bool try_lock_for(const std::chrono::duration<Rep, Period>& duration)
    {
        return carb::getCachedInterface<ITasking>()->timedLockMutex(m_mutex, detail::convertDuration(duration));
    }

    /**
     * Attempts to lock a mutex waiting until a specific clock time.
     *
     * @param time_point The clock time to wait until.
     * @returns true if the mutex was locked; false if the timeout period expired. If true is returned, unlock() must be
     * called to release the lock.
     */
    template <class Clock, class Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& time_point)
    {
        return carb::getCachedInterface<ITasking>()->timedLockMutex(m_mutex, detail::convertAbsTime(time_point));
    }

    /**
     * Convertible to Mutex*.
     */
    operator Mutex*() const
    {
        return m_mutex;
    }

    /**
     * Returns the acquired ITasking interface that was used to construct this object.
     * @note Deprecated: Use carb::getCachedInterface instead.
     */
    CARB_DEPRECATED("Use carb::getCachedInterface") ITasking* getTasking() const
    {
        return carb::getCachedInterface<ITasking>();
    }

    CARB_PREVENT_COPY_AND_MOVE(RecursiveMutexWrapper);

private:
    Mutex* m_mutex;
};

/**
 * Wrapper for a carb::tasking::Semaphore
 *
 * @note SemaphoreWrapper can be used for @rstref{Throttling <tasking-throttling-label>} tasks.
 */
class SemaphoreWrapper
{
public:
    /**
     * Constructs a new SemaphoreWrapper object
     *
     * @param value The initial value of the semaphore (i.e. how many times acquire() can be called without blocking).
     */
    SemaphoreWrapper(unsigned value) : m_sema(carb::getCachedInterface<ITasking>()->createSemaphore(value))
    {
    }

    /**
     * Constructs a new SemaphoreWrapper object
     *
     * @note Deprecated: ITasking no longer needed.
     * @param value The initial value of the semaphore (i.e. how many times acquire() can be called without blocking).
     */
    CARB_DEPRECATED("ITasking no longer needed.")
    SemaphoreWrapper(ITasking*, unsigned value) : m_sema(carb::getCachedInterface<ITasking>()->createSemaphore(value))
    {
    }

    /**
     * Destructor
     */
    ~SemaphoreWrapper()
    {
        carb::getCachedInterface<ITasking>()->destroySemaphore(m_sema);
    }

    /**
     * Increases the value of the semaphore, potentially unblocking any threads waiting in acquire().
     *
     * @param count The value to add to the Semaphore's value. That is, the number of threads to either unblock while
     * waiting in acquire(), or to allow to call acquire() without blocking.
     */
    void release(unsigned count = 1)
    {
        carb::getCachedInterface<ITasking>()->releaseSemaphore(m_sema, count);
    }

    /**
     * Reduce the value of the Semaphore by one, potentially blocking if the count is already zero.
     *
     * @note Threads that are blocked by acquire() must be released by other threads calling release().
     */
    void acquire()
    {
        carb::getCachedInterface<ITasking>()->waitSemaphore(m_sema);
    }

    /**
     * Attempts to reduce the value of the Semaphore by one. If the Semaphore's value is zero, false is returned.
     *
     * @returns true if the count of the Semaphore was reduced by one; false if the count is already zero.
     */
    bool try_acquire()
    {
        return carb::getCachedInterface<ITasking>()->timedWaitSemaphore(m_sema, 0);
    }

    /**
     * Attempts to reduce the value of the Semaphore by one, waiting until the duration expires if the value is zero.
     *
     * @returns true if the count of the Semaphore was reduced by one; false if the duration expires.
     */
    template <class Rep, class Period>
    bool try_acquire_for(const std::chrono::duration<Rep, Period>& dur)
    {
        return carb::getCachedInterface<ITasking>()->timedWaitSemaphore(m_sema, detail::convertDuration(dur));
    }

    /**
     * Attempts to reduce the value of the Semaphore by one, waiting until the given time point is reached if the value
     * is zero.
     *
     * @returns true if the count of the Semaphore was reduced by one; false if the time point is reached by the clock.
     */
    template <class Clock, class Duration>
    bool try_acquire_until(const std::chrono::time_point<Clock, Duration>& tp)
    {
        return carb::getCachedInterface<ITasking>()->timedWaitSemaphore(m_sema, detail::convertAbsTime(tp));
    }

    /**
     * Convertible to Semaphore*.
     */
    operator Semaphore*() const
    {
        return m_sema;
    }

    /**
     * Returns the acquired ITasking interface that was used to construct this object.
     * @note Deprecated: Use carb::getCachedInterface instead.
     */
    CARB_DEPRECATED("Use carb::getCachedInterface") ITasking* getTasking() const
    {
        return carb::getCachedInterface<ITasking>();
    }

    CARB_PREVENT_COPY_AND_MOVE(SemaphoreWrapper);

private:
    Semaphore* m_sema;
};

/**
 * Wrapper for a carb::tasking::SharedMutex that (mostly) conforms to C++ Named Requirements of SharedMutex.
 */
class SharedMutexWrapper
{
public:
    /**
     * Constructs a new SharedMutexWrapper object
     */
    SharedMutexWrapper() : m_mutex(carb::getCachedInterface<ITasking>()->createSharedMutex())
    {
    }

    /**
     * Constructs a new SharedMutexWrapper object
     * @note Deprecated: ITasking no longer needed.
     */
    CARB_DEPRECATED("ITasking no longer needed.")
    SharedMutexWrapper(ITasking*) : m_mutex(carb::getCachedInterface<ITasking>()->createSharedMutex())
    {
    }

    /**
     * Destructor
     *
     * @note It is an error to destroy a shared mutex that is locked.
     */
    ~SharedMutexWrapper()
    {
        carb::getCachedInterface<ITasking>()->destroySharedMutex(m_mutex);
    }

    /**
     * Attempts to shared-lock the shared mutex immediately.
     *
     * @note It is an error to lock recursively or shared-lock when exclusive-locked, or vice versa.
     *
     * @returns true if the mutex was shared-locked; false otherwise
     */
    bool try_lock_shared()
    {
        return carb::getCachedInterface<ITasking>()->timedLockSharedMutex(m_mutex, 0);
    }

    /**
     * Attempts to exclusive-lock the shared mutex immediately.
     *
     * @note It is an error to lock recursively or shared-lock when exclusive-locked, or vice versa.
     *
     * @returns true if the mutex was shared-locked; false otherwise
     */
    bool try_lock()
    {
        return carb::getCachedInterface<ITasking>()->timedLockSharedMutexExclusive(m_mutex, 0);
    }

    /**
     * Attempts to exclusive-lock the shared mutex within a specified duration.
     *
     * @note It is an error to lock recursively or shared-lock when exclusive-locked, or vice versa.
     *
     * @param duration The duration to wait for the mutex to be available
     * @returns true if the mutex was exclusive-locked; false if the timeout period expired
     */
    template <class Rep, class Period>
    bool try_lock_for(const std::chrono::duration<Rep, Period>& duration)
    {
        return carb::getCachedInterface<ITasking>()->timedLockSharedMutexExclusive(
            m_mutex, detail::convertDuration(duration));
    }

    /**
     * Attempts to shared-lock the shared mutex within a specified duration.
     *
     * @note It is an error to lock recursively or shared-lock when exclusive-locked, or vice versa.
     *
     * @param duration The duration to wait for the mutex to be available
     * @returns true if the mutex was shared-locked; false if the timeout period expired
     */
    template <class Rep, class Period>
    bool try_lock_shared_for(const std::chrono::duration<Rep, Period>& duration)
    {
        return carb::getCachedInterface<ITasking>()->timedLockSharedMutex(m_mutex, detail::convertDuration(duration));
    }

    /**
     * Attempts to exclusive-lock the shared mutex until a specific clock time.
     *
     * @note It is an error to lock recursively or shared-lock when exclusive-locked, or vice versa.
     *
     * @param time_point The clock time to wait until.
     * @returns true if the mutex was exclusive-locked; false if the timeout period expired
     */
    template <class Clock, class Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration>& time_point)
    {
        return try_lock_for(time_point - Clock::now());
    }

    /**
     * Attempts to shared-lock the shared mutex until a specific clock time.
     *
     * @note It is an error to lock recursively or shared-lock when exclusive-locked, or vice versa.
     *
     * @param time_point The clock time to wait until.
     * @returns true if the mutex was shared-locked; false if the timeout period expired
     */
    template <class Clock, class Duration>
    bool try_lock_shared_until(const std::chrono::time_point<Clock, Duration>& time_point)
    {
        return try_lock_shared_for(time_point - Clock::now());
    }

    /**
     * Shared-locks the shared mutex, waiting until it becomes available.
     *
     * @note It is an error to lock recursively or shared-lock when exclusive-locked, or vice versa.
     */
    void lock_shared()
    {
        carb::getCachedInterface<ITasking>()->lockSharedMutex(m_mutex);
    }

    /**
     * Unlocks a mutex previously shared-locked by this thread/task.
     *
     * @note It is undefined behavior to unlock a mutex that is not owned by the current thread or task.
     */
    void unlock_shared()
    {
        carb::getCachedInterface<ITasking>()->unlockSharedMutex(m_mutex);
    }

    /**
     * Exclusive-locks the shared mutex, waiting until it becomes available.
     *
     * @note It is an error to lock recursively or shared-lock when exclusive-locked, or vice versa.
     */
    void lock()
    {
        carb::getCachedInterface<ITasking>()->lockSharedMutexExclusive(m_mutex);
    }

    /**
     * Unlocks a mutex previously exclusive-locked by this thread/task.
     *
     * @note It is undefined behavior to unlock a mutex that is not owned by the current thread or task.
     */
    void unlock()
    {
        carb::getCachedInterface<ITasking>()->unlockSharedMutex(m_mutex);
    }

    /**
     * Convertible to SharedMutex*.
     */
    operator SharedMutex*() const
    {
        return m_mutex;
    }

    /**
     * Returns the acquired ITasking interface that was used to construct this object.
     * @note Deprecated: Use carb::getCachedInterface instead.
     */
    CARB_DEPRECATED("Use carb::getCachedInterface") ITasking* getTasking() const
    {
        return carb::getCachedInterface<ITasking>();
    }

    CARB_PREVENT_COPY_AND_MOVE(SharedMutexWrapper);

private:
    SharedMutex* m_mutex;
};

/**
 * Wrapper for carb::tasking::ConditionVariable
 */
class ConditionVariableWrapper
{
public:
    /**
     * Constructs a new ConditionVariableWrapper object
     */
    ConditionVariableWrapper() : m_cv(carb::getCachedInterface<ITasking>()->createConditionVariable())
    {
    }

    /**
     * Constructs a new ConditionVariableWrapper object
     * @note Deprecated: ITasking no longer needed.
     */
    CARB_DEPRECATED("ITasking no longer needed.")
    ConditionVariableWrapper(ITasking*) : m_cv(carb::getCachedInterface<ITasking>()->createConditionVariable())
    {
    }

    /**
     * Destructor
     *
     * @note It is an error to destroy a condition variable that has waiting threads.
     */
    ~ConditionVariableWrapper()
    {
        carb::getCachedInterface<ITasking>()->destroyConditionVariable(m_cv);
    }

    /**
     * Waits until the condition variable is notified.
     *
     * @note @p m must be locked when calling this function. The mutex will be unlocked while waiting and re-locked
     * before returning to the caller.
     *
     * @param m The mutex to unlock while waiting for the condition variable to be notified.
     */
    void wait(Mutex* m)
    {
        carb::getCachedInterface<ITasking>()->waitConditionVariable(m_cv, m);
    }

    /**
     * Waits until a predicate has been satisfied and the condition variable is notified.
     *
     * @note @p m must be locked when calling this function. The mutex will be locked when calling @p pred and when this
     * function returns, but unlocked while waiting.
     *
     * @param m The mutex to unlock while waiting for the condition variable to be notified.
     * @param pred A predicate that is called repeatedly. When it returns true, the function returns. If it returns
     * false, @p m is unlocked and the thread/task waits until the condition variable is notified.
     */
    template <class Pred>
    void wait(Mutex* m, Pred&& pred)
    {
        carb::getCachedInterface<ITasking>()->waitConditionVariablePred(m_cv, m, std::forward<Pred>(pred));
    }

    /**
     * Waits until the condition variable is notified or the specified duration expires.
     *
     * @note @p m must be locked when calling this function. The mutex will be unlocked while waiting and re-locked
     * before returning to the caller.
     *
     * @param m The mutex to unlock while waiting for the condition variable to be notified.
     * @param duration The amount of time to wait for.
     * @returns `std::cv_status::no_timeout` if the condition variable was notified; `std::cv_status::timeout` if the
     * timeout period expired.
     */
    template <class Rep, class Period>
    std::cv_status wait_for(Mutex* m, const std::chrono::duration<Rep, Period>& duration)
    {
        return carb::getCachedInterface<ITasking>()->timedWaitConditionVariable(
                   m_cv, m, detail::convertDuration(duration)) ?
                   std::cv_status::no_timeout :
                   std::cv_status::timeout;
    }

    /**
     * Waits until a predicate is satisfied and the condition variable is notified, or the specified duration expires.
     *
     * @note @p m must be locked when calling this function. The mutex will be unlocked while waiting and re-locked
     * before returning to the caller.
     *
     * @param m The mutex to unlock while waiting for the condition variable to be notified.
     * @param duration The amount of time to wait for.
     * @param pred A predicate that is called repeatedly. When it returns true, the function returns. If it returns
     * false, @p m is unlocked and the thread/task waits until the condition variable is notified.
     * @returns true if the predicate was satisfied; false if a timeout occurred.
     */
    template <class Rep, class Period, class Pred>
    bool wait_for(Mutex* m, const std::chrono::duration<Rep, Period>& duration, Pred&& pred)
    {
        return carb::getCachedInterface<ITasking>()->timedWaitConditionVariablePred(
            m_cv, m, detail::convertDuration(duration), std::forward<Pred>(pred));
    }

    /**
     * Waits until the condition variable is notified or the clock reaches the given time point.
     *
     * @note @p m must be locked when calling this function. The mutex will be unlocked while waiting and re-locked
     * before returning to the caller.
     *
     * @param m The mutex to unlock while waiting for the condition variable to be notified.
     * @param time_point The clock time to wait until.
     * @returns `std::cv_status::no_timeout` if the condition variable was notified; `std::cv_status::timeout` if the
     * timeout period expired.
     */
    template <class Clock, class Duration>
    std::cv_status wait_until(Mutex* m, const std::chrono::time_point<Clock, Duration>& time_point)
    {
        return carb::getCachedInterface<ITasking>()->timedWaitConditionVariable(
                   m_cv, m, detail::convertAbsTime(time_point)) ?
                   std::cv_status::no_timeout :
                   std::cv_status::timeout;
    }

    /**
     * Waits until a predicate is satisfied and the condition variable is notified or the clock reaches the given time
     * point.
     *
     * @note @p m must be locked when calling this function. The mutex will be unlocked while waiting and re-locked
     * before returning to the caller.
     *
     * @param m The mutex to unlock while waiting for the condition variable to be notified.
     * @param time_point The clock time to wait until.
     * @param pred A predicate that is called repeatedly. When it returns true, the function returns. If it returns
     * false, @p m is unlocked and the thread/task waits until the condition variable is notified.
     * @returns true if the predicate was satisfied; false if a timeout occurred.
     */
    template <class Clock, class Duration, class Pred>
    bool wait_until(Mutex* m, const std::chrono::time_point<Clock, Duration>& time_point, Pred&& pred)
    {
        return carb::getCachedInterface<ITasking>()->timedWaitConditionVariablePred(
            m_cv, m, detail::convertAbsTime(time_point), std::forward<Pred>(pred));
    }

    /**
     * Notifies one waiting thread/task to wake and check the predicate (if applicable).
     */
    void notify_one()
    {
        carb::getCachedInterface<ITasking>()->notifyConditionVariableOne(m_cv);
    }

    /**
     * Notifies all waiting threads/tasks to wake and check the predicate (if applicable).
     */
    void notify_all()
    {
        carb::getCachedInterface<ITasking>()->notifyConditionVariableAll(m_cv);
    }

    /**
     * Convertible to ConditionVariable*.
     */
    operator ConditionVariable*() const
    {
        return m_cv;
    }

    /**
     * Returns the acquired ITasking interface that was used to construct this object.
     * @note Deprecated: Use carb::getCachedInterface instead.
     */
    CARB_DEPRECATED("Use carb::getCachedInterface") ITasking* getTasking() const
    {
        return carb::getCachedInterface<ITasking>();
    }

    CARB_PREVENT_COPY_AND_MOVE(ConditionVariableWrapper);

private:
    ConditionVariable* m_cv;
};

/**
 * When instantiated, begins tracking the passed Trackers. At destruction, tracking on the given Trackers is ended.
 *
 * This is similar to the manner in which ITasking::addTask() accepts Trackers and begins tracking them prior to the
 * task starting, and then leaves them when the task finishes. This class allows performing the same tracking behavior
 * without the overhead of a task.
 */
class ScopedTracking
{
public:
    /**
     * Default constructor.
     */
    ScopedTracking() : m_tracker{ ObjectType::eNone, nullptr }
    {
    }

    /**
     * Constructor that accepts a Trackers object.
     * @param trackers The Trackers to begin tracking.
     */
    ScopedTracking(Trackers trackers);

    /**
     * Destructor. The Trackers provided to the constructor finish tracking when `this` is destroyed.
     */
    ~ScopedTracking();

    CARB_PREVENT_COPY(ScopedTracking);

    /**
     * Allows move-construct.
     */
    ScopedTracking(ScopedTracking&& rhs);

    /**
     * Allows move-assign.
     */
    ScopedTracking& operator=(ScopedTracking&& rhs) noexcept;

private:
    Object m_tracker;
};

inline constexpr RequiredObject::RequiredObject(const TaskGroup& tg)
    : Object{ ObjectType::eTaskGroup, const_cast<std::atomic_size_t*>(&tg.m_count) }
{
}

inline constexpr RequiredObject::RequiredObject(const TaskGroup* tg)
    : Object{ ObjectType::eTaskGroup, tg ? const_cast<std::atomic_size_t*>(&tg->m_count) : nullptr }
{
}

inline All::All(std::initializer_list<RequiredObject> il)
{
    static_assert(sizeof(RequiredObject) == sizeof(Object), "Invalid assumption");
    m_counter = carb::getCachedInterface<ITasking>()->internalGroupObjects(ITasking::eAll, il.begin(), il.size());
}

template <class InputIt, std::enable_if_t<detail::IsForwardIter<InputIt, RequiredObject>::value, bool>>
inline All::All(InputIt begin, InputIt end)
{
    static_assert(sizeof(RequiredObject) == sizeof(Object), "Invalid assumption");
    std::vector<RequiredObject> objects;
    for (; begin != end; ++begin)
        objects.push_back(*begin);
    m_counter =
        carb::getCachedInterface<ITasking>()->internalGroupObjects(ITasking::eAll, objects.data(), objects.size());
}

template <class InputIt, std::enable_if_t<detail::IsRandomAccessIter<InputIt, RequiredObject>::value, bool>>
inline All::All(InputIt begin, InputIt end)
{
    static_assert(sizeof(RequiredObject) == sizeof(Object), "Invalid assumption");
    size_t const count = end - begin;
    RequiredObject* objects = CARB_STACK_ALLOC(RequiredObject, count);
    size_t index = 0;
    for (; begin != end; ++begin)
        objects[index++] = *begin;
    CARB_ASSERT(index == count);
    m_counter = carb::getCachedInterface<ITasking>()->internalGroupObjects(ITasking::eAll, objects, count);
}

inline Any::Any(std::initializer_list<RequiredObject> il)
{
    static_assert(sizeof(RequiredObject) == sizeof(Object), "Invalid assumption");
    m_counter = carb::getCachedInterface<ITasking>()->internalGroupObjects(ITasking::eAny, il.begin(), il.size());
}

template <class InputIt, std::enable_if_t<detail::IsForwardIter<InputIt, RequiredObject>::value, bool>>
inline Any::Any(InputIt begin, InputIt end)
{
    static_assert(sizeof(RequiredObject) == sizeof(Object), "Invalid assumption");
    std::vector<RequiredObject> objects;
    for (; begin != end; ++begin)
        objects.push_back(*begin);
    m_counter =
        carb::getCachedInterface<ITasking>()->internalGroupObjects(ITasking::eAny, objects.data(), objects.size());
}

template <class InputIt, std::enable_if_t<detail::IsRandomAccessIter<InputIt, RequiredObject>::value, bool>>
inline Any::Any(InputIt begin, InputIt end)
{
    static_assert(sizeof(RequiredObject) == sizeof(Object), "Invalid assumption");
    size_t const count = end - begin;
    RequiredObject* objects = CARB_STACK_ALLOC(RequiredObject, count);
    size_t index = 0;
    for (; begin != end; ++begin)
        objects[index++] = *begin;
    CARB_ASSERT(index == count);
    m_counter = carb::getCachedInterface<ITasking>()->internalGroupObjects(ITasking::eAny, objects, count);
}

inline Tracker::Tracker(TaskGroup& grp) : Object{ ObjectType::eTaskGroup, &grp.m_count }
{
}

inline Tracker::Tracker(TaskGroup* grp) : Object{ ObjectType::eTaskGroup, grp ? &grp->m_count : nullptr }
{
}

inline ScopedTracking::ScopedTracking(Trackers trackers)
{
    Tracker const* ptrackers;
    size_t numTrackers;
    trackers.output(ptrackers, numTrackers);
    m_tracker = carb::getCachedInterface<ITasking>()->beginTracking(ptrackers, numTrackers);
}

inline ScopedTracking::~ScopedTracking()
{
    Object tracker = std::exchange(m_tracker, { ObjectType::eNone, nullptr });
    if (tracker.type == ObjectType::eTrackerGroup)
    {
        carb::getCachedInterface<ITasking>()->endTracking(tracker);
    }
}

inline ScopedTracking::ScopedTracking(ScopedTracking&& rhs)
    : m_tracker(std::exchange(rhs.m_tracker, { ObjectType::eNone, nullptr }))
{
}

inline ScopedTracking& ScopedTracking::operator=(ScopedTracking&& rhs) noexcept
{
    std::swap(m_tracker, rhs.m_tracker);
    return *this;
}

} // namespace tasking
} // namespace carb
