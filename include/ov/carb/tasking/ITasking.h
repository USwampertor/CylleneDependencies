// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief carb.tasking interface definition file.
#pragma once

#include "../Interface.h"
#include "../InterfaceUtils.h"
#include "TaskingHelpers.h"

namespace carb
{

//! Namespace for *carb.tasking.plugin* and related utilities.
namespace tasking
{

/**
 * Default TaskingDesc plugin starts with.
 */
inline TaskingDesc getDefaultTaskingDesc()
{
    return TaskingDesc{};
}

/**
 * Defines a tasking plugin interface, acquired with carb::Framework::acquireInterface() when *carb.tasking.plugin* is
 * loaded.
 *
 * ITasking is started automatically on plugin startup. It uses default TaskingDesc, see getDefaultTaskingDesc().
 *
 * Several @rstref{ISettings keys <tasking_settings>} exist to provide debug behavior and to override default startup
 * behavior (but do not override a TaskingDesc provided to ITasking::changeParameters()).
 *
 * @thread_safety Unless otherwise specified, all functions in this interface can be called from multiple threads
 * simultaneously.
 */
struct ITasking
{
    // 0.1 - Initial version
    // 0.2 - Thread pinning, sleep, suspending/waking (not ABI compatible with 0.1)
    // 0.3 - Semaphore support, SharedMutex support
    // 0.4 - ConditionVariable support
    // 1.0 - Wait timeouts (git hash e13289c5a5)
    // 1.1 - changeTaskPriority() / executeMainTasks()
    // 1.2 - restart() -> changeParameters(); don't lose tasks when changing parameters
    // 1.3 - Respect task priority when resuming tasks that have slept, waited, or unsuspended (not an API change)
    // 1.4 - Stuck checking (not an API change)
    // 1.5 - internalGroupCounters()
    // 1.6 - createRecursiveMutex()
    // 2.0 - ITasking 2.0 (git hash f68ae95da7)
    // 2.1 - allocTaskStorage() / freeTaskStorage() / setTaskStorage() / getTaskStorage()
    // 2.2 - beginTracking() / endTracking()
    // 2.3 - internalNameTask()
    // 2.4 - reloadFiberEvents()
    CARB_PLUGIN_INTERFACE("carb::tasking::ITasking", 2, 4)

    /**
     * Changes the parameters under which the ITasking interface functions. This may stop and start threads, but will
     * not lose any tasks in progress or queued.
     *
     * @note This function reloads all registered @ref IFiberEvents interfaces so they will start receiving
     *  notifications. However, if this is the only change desired it is recommended to use @ref reloadFiberEvents()
     *  instead.
     *
     * @thread_safety It is unsafe to add any additional tasks while calling this function. The caller must ensure that
     * no new tasks are added until this function returns.
     *
     * @warning Calling this function from within a task context causes undefined behavior.
     *
     * @param desc    The tasking plugin descriptor.
     */
    void(CARB_ABI* changeParameters)(TaskingDesc desc);

    /**
     * Get TaskingDesc the plugin currently running with.
     *
     * @return The tasking plugin descriptor.
     */
    const TaskingDesc&(CARB_ABI* getDesc)();

    /**
     * Creates a Counter with target value of zero.
     *
     * @warning Prefer using CounterWrapper instead.
     *
     * @return The counter created.
     */
    Counter*(CARB_ABI* createCounter)();

    /**
     * Creates a counter with a specific target value.
     *
     * @warning Prefer using CounterWrapper instead.
     *
     * @param target    The target value of the counter. Yielding on this counter will wait for this target.
     * @return          The counter created.
     */
    Counter*(CARB_ABI* createCounterWithTarget)(uint32_t target);

    /**
     * Destroys the counter.
     *
     * @param counter    A counter.
     */
    void(CARB_ABI* destroyCounter)(Counter* counter);

    /**
     * Adds a task to the internal queue. Do not call this function directly; instead, use one of the helper functions
     * such as addTask(), addSubTask() or addThrottledTask().
     *
     * @param task       The task to queue.
     * @param counter    A counter to associate with this task. It will be incremented by 1.
     *   When the task completes, it will be decremented.
     * @return A TaskContext that can be used to refer to this task
     */
    //! @private
    TaskContext(CARB_ABI* internalAddTask)(TaskDesc task, Counter* counter);

    /**
     * Adds a group of tasks to the internal queue
     *
     * @param tasks       The tasks to queue.
     * @param taskCount   The number of tasks.
     * @param counter     A counter to associate with the task group as a whole.
     *   Initially it incremented by taskCount. When each task completes, it will be decremented by 1.
     */
    void(CARB_ABI* addTasks)(TaskDesc* tasks, size_t taskCount, Counter* counter);

    //! @private
    TaskContext(CARB_ABI* internalAddDelayedTask)(uint64_t delayNs, TaskDesc desc, Counter* counter);

    //! @private
    void(CARB_ABI* internalApplyRange)(size_t range, ApplyFn fn, void* context);

    /**
     * Yields execution to another task until counter reaches its target value.
     *
     * Tasks invoking this call can resume on different thread. If the task must resume on the same thread, use
     * PinGuard.
     *
     * @note deprecated Use wait() instead.
     *
     * @param counter   The counter to check.
     */
    CARB_DEPRECATED("Use wait() instead") void yieldUntilCounter(RequiredObject counter);

    /**
     * Yields execution to another task until counter reaches its target value or the timeout period elapses.
     *
     * Tasks invoking this call can resume on different thread. If the task must resume on the same thread, use
     * PinGuard.
     *
     * @note Deprecated: Use wait_for() or wait_until() instead.
     *
     * @param counter   The counter to check.
     * @param timeoutNs The number of nanoseconds to wait. Pass kInfinite to wait forever or 0 to try immediately
     * without waiting.
     * @return true if the counter period has completed; false if the timeout period elapses.
     */
    CARB_DEPRECATED("Use wait_for() or wait_until() instead.")
    bool timedYieldUntilCounter(RequiredObject counter, uint64_t timeoutNs);

    //! @private
    bool(CARB_ABI* internalCheckCounter)(Counter* counter);
    //! @private
    uint32_t(CARB_ABI* internalGetCounterValue)(Counter* counter);
    //! @private
    uint32_t(CARB_ABI* internalGetCounterTarget)(Counter* counter);
    //! @private
    uint32_t(CARB_ABI* internalFetchAddCounter)(Counter* counter, uint32_t value);
    //! @private
    uint32_t(CARB_ABI* internalFetchSubCounter)(Counter* counter, uint32_t value);
    //! @private
    void(CARB_ABI* internalStoreCounter)(Counter* counter, uint32_t value);

    /**
     * Checks if counter is at the counter's target value
     *
     * @note Deprecated: The Counter interface is deprecated.
     *
     * @param c The counter to check.
     * @return `true` if the counter is at the target value; `false` otherwise.
     */
    CARB_DEPRECATED("The Counter interface is deprecated.") bool checkCounter(Counter* c)
    {
        return internalCheckCounter(c);
    }

    /**
     * Retrieves the current value of the target. Note! Because of the threaded nature of counters, this
     * value may have changed by another thread before the function returns.
     *
     * @note Deprecated: The Counter interface is deprecated.
     *
     * @param counter The counter.
     * @return        The current value of the counter.
     */
    CARB_DEPRECATED("The Counter interface is deprecated.") uint32_t getCounterValue(Counter* counter)
    {
        return internalGetCounterValue(counter);
    }

    /**
     * Gets the target value for the Counter
     *
     * @note Deprecated: The Counter interface is deprecated.
     *
     * @param counter The counter to check.
     * @return        The target value of the counter.
     */
    CARB_DEPRECATED("The Counter interface is deprecated.") uint32_t getCounterTarget(Counter* counter)
    {
        return internalGetCounterTarget(counter);
    }

    /**
     * Atomically adds a value to the counter and returns the value held previously.
     *
     * The fetchAdd operation on the counter will be atomic, but this function as a whole is not atomic.
     *
     * @note Deprecated: The Counter interface is deprecated.
     *
     * @param counter The counter.
     * @param value   The value to add to the counter.
     * @return        The value of the counter before the addition.
     */
    CARB_DEPRECATED("The Counter interface is deprecated.") uint32_t fetchAddCounter(Counter* counter, uint32_t value)
    {
        return internalFetchAddCounter(counter, value);
    }

    /**
     * Atomically subtracts a value from the counter and returns the value held previously.
     *
     * The fetchSub operation on the counter will be atomic, but this function as a whole is not atomic.
     *
     * @note Deprecated: The Counter interface is deprecated.
     *
     * @param counter        The counter.
     * @param value          The value to subtract from the counter.
     * @return               The value of the counter before the addition.
     */
    CARB_DEPRECATED("The Counter interface is deprecated.") uint32_t fetchSubCounter(Counter* counter, uint32_t value)
    {
        return internalFetchSubCounter(counter, value);
    }

    /**
     * Atomically replaces the current value with desired on a counter.
     *
     * The store operation on the counter will be atomic, but this function as a whole is not atomic.
     *
     * @note Deprecated: The Counter interface is deprecated.
     *
     * @param counter        The counter.
     * @param value          The value to load into to the counter.
     */
    CARB_DEPRECATED("The Counter interface is deprecated.") void storeCounter(Counter* counter, uint32_t value)
    {
        return internalStoreCounter(counter, value);
    }

    /**
     * Yields execution. Task invoking this call will be put in the very end of task queue, priority is ignored.
     */
    void(CARB_ABI* yield)();

    /**
     * Causes the currently executing TaskContext to be "pinned" to the thread it is currently running on.
     *
     * @warning Do not call this function directly; instead use PinGuard.
     *
     * This function causes the current thread to be the only task thread that can run the current task. This is
     * necessary in some cases where thread specificity is required (those these situations are NOT recommended for
     * tasks): holding a mutex, or using thread-specific data, etc. Thread pinning is not efficient (the pinned thread
     * could be running a different task causing delays for the current task to be resumed, and wakeTask() must wait to
     * return until the pinned thread has been notified) and should therefore be avoided.
     *
     * Call unpinFromCurrentThread() to remove the pin, allowing the task to run on any thread.
     *
     * @note %All calls to pin a thread will issue a warning log message.
     *
     * @note It is assumed that the task is allowed to move to another thread during the pinning process, though this
     * may not always be the case. Only after pinToCurrentThread() returns will a task be pinned. Therefore, make sure
     * to call pinToCurrentThread() *before* any operation that requires pinning.
     *
     * @return true if the task was already pinned; false if the task was not pinned or if not called from Task Context
     * (i.e. getTaskContext() would return kInvalidTaskContext)
     */
    bool(CARB_ABI* pinToCurrentThread)();

    /**
     * Un-pins the currently executing TaskContext from the thread it is currently running on.
     *
     * @warning Do not call this function directly; instead use PinGuard.
     *
     * @return true if the task was successfully un-pinned; false if the task was not pinned or if not called from Task
     * Context (i.e. getTaskContext() would return kInvalidTaskContext)
     */
    bool(CARB_ABI* unpinFromCurrentThread)();

    /**
     * Creates a non-recursive mutex.
     *
     * @warning Prefer using MutexWrapper instead.
     *
     * @note Both createMutex() and createRecursiveMutex() return a Mutex object; it is up to the creator to ensure that
     * the Mutex object is used properly. A Mutex created with createMutex() will call `std::terminate()` if recursively
     * locked.
     *
     * @return The created non-recursive mutex.
     */
    Mutex*(CARB_ABI* createMutex)();

    /**
     * Destroys a mutex.
     *
     * @param The mutex to destroy.
     */
    void(CARB_ABI* destroyMutex)(Mutex* mutex);

    /**
     * Locks a mutex
     *
     * @param mutex The mutex to lock.
     */
    void lockMutex(Mutex* mutex);

    /**
     * Locks a mutex or waits for the timeout period to expire.
     *
     * @note Attempting to recursively lock a mutex created with createMutex() will abort. Use a mutex created with
     * createRecursiveMutex() to support recursive locking.
     *
     * @param mutex The mutex to lock.
     * @param timeoutNs The relative timeout in nanoseconds. Specify kInfinite to wait forever or 0 to try locking
     * without waiting.
     * @returns true if the calling thread/fiber now has ownership of the mutex; false if the timeout period expired.
     */
    bool(CARB_ABI* timedLockMutex)(Mutex* mutex, uint64_t timeoutNs);

    /**
     * Unlock a mutex
     *
     * @param The mutex to unlock.
     */
    void(CARB_ABI* unlockMutex)(Mutex* mutex);

    /**
     * Sleeps for the given number of nanoseconds. Prefer using sleep_for() or sleep_until()
     *
     * @note This function is fiber-aware. If currently executing in a fiber, the fiber will be yielded until the
     * requested amount of time has passed. If a thread is currently executing, then the thread will sleep.
     *
     * @param nanoseconds The amount of time to yield/sleep, in nanoseconds.
     */
    void(CARB_ABI* sleepNs)(uint64_t nanoseconds);

    /**
     * If the calling thread is running in "task context", that is, a fiber executing a task previously queued with
     * addTask(), this function returns a handle that can be used with suspendTask() and wakeTask().
     *
     * @return kInvalidTaskContext if the calling thread is not running within "task context"; otherwise, a TaskContext
     * handle is returned that can be used with suspendTask() and wakeTask(), as well as anywhere a RequiredObject is
     * used.
     */
    TaskContext(CARB_ABI* getTaskContext)();

    /**
     * Suspends the current task. Does not return until wakeTask() is called with the task's TaskContext (see
     * getTaskContext()).
     *
     * @note to avoid race-conditions between wakeTask() and suspendTask(), a wakeTask() that occurs before
     * suspendTask() has been called will cause suspendTask() to return true immediately without waiting.
     *
     * @return true when wakeTask() is called. If the current thread is not running in "task context" (i.e.
     * getTaskContext() would return kInvalidTaskContext), then this function returns false immediately.
     */
    bool(CARB_ABI* suspendTask)();

    /**
     * Wakes a task previously suspended with suspendTask().
     *
     * @note to avoid race-conditions between wakeTask() and suspendTask(), a wakeTask() that occurs before
     * suspendTask() has been called will cause suspendTask() to return true immediately without waiting. The wakeTask()
     * function returns immediately and does not wait for the suspended task to resume.
     *
     * wakeTask() cannot be called on the current task context (false will be returned). Additional situations that will
     * log (as a warning) and return false:
     * - The task context given already has a pending wake
     * - The task has finished
     * - The task context given is sleeping or otherwise waiting on an event (cannot be woken)
     * - The given TaskContext is not valid
     *
     * @param task The TaskContext (returned by getTaskContext()) for the task suspended with suspendTask().
     * @return true if the task was woken properly. false if a situation listed above occurs.
     */
    bool(CARB_ABI* wakeTask)(TaskContext task);

    /**
     * Blocks the current thread/task until the given Task has completed.
     *
     * Similar to yieldUntilCounter() but does not require a Counter object.
     *
     * @note Deprecated: Use wait() instead.
     *
     * @param task The TaskContext to wait on
     * @return true if the wait was successful; false if the TaskContext has already expired or was invalid.
     */
    CARB_DEPRECATED("Use wait() instead") bool waitForTask(TaskContext task);

    //! @private
    bool(CARB_ABI* internalTimedWait)(Object obj, uint64_t timeoutNs);

    /**
     * Checks the object specified in @p req to see if it is signaled.
     *
     * @param req The RequiredObject object to check.
     * @returns `true` if the object is signaled; `false` if the object is invalid or not signaled.
     */
    bool try_wait(RequiredObject req);

    /**
     * Blocks the calling thread or task until @p req is signaled.
     *
     * @param req The RequiredObject object to check.
     */
    void wait(RequiredObject req);

    /**
     * Blocks the calling thread or task until @p req is signaled or @p dur has elapsed.
     *
     * @param dur The duration to wait for.
     * @param req The RequiredObject object to check.
     * @returns `true` if the object is signaled; `false` if the object is invalid or not signaled, or @p dur elapses.
     */
    template <class Rep, class Period>
    bool wait_for(std::chrono::duration<Rep, Period> dur, RequiredObject req);

    /**
     * Blocks the calling thread or task until @p req is signaled or the clock reaches @p when.
     *
     * @param when The time_point to wait until.
     * @param req The RequiredObject object to check.
     * @returns `true` if the object is signaled; `false` if the object is invalid or not signaled, or @p when is
     * reached.
     */
    template <class Clock, class Duration>
    bool wait_until(std::chrono::time_point<Clock, Duration> when, RequiredObject req);

    /**
     * Creates a fiber-aware semaphore primitive.
     *
     * A semaphore is a gate that lets a certain number of tasks/threads through. This can also be used to throttle
     * tasks (see addThrottledTask()). When the count of a semaphore goes negative tasks/threads will wait on the
     * semaphore.
     *
     * @param value The starting value of the semaphore. Limited to INT_MAX. 0 means that any attempt to wait on the
     * semaphore will block until the semaphore is released.
     * @return A Semaphore object. When finished, dispose of the semaphore with destroySemaphore().
     *
     * @warning Prefer using SemaphoreWrapper instead.
     *
     * @note Semaphore can be used for @rstref{Throttling <tasking-throttling-label>} tasks.
     */
    Semaphore*(CARB_ABI* createSemaphore)(unsigned value);

    /**
     * Destroys a semaphore object created by createSemaphore()
     *
     * @param sema The semaphore to destroy.
     */
    void(CARB_ABI* destroySemaphore)(Semaphore* sema);

    /**
     * Releases (or posts, or signals) a semaphore.
     *
     * If a task/thread is waiting on the semaphore when it is released,
     * the task/thread is un-blocked and will be resumed. If no tasks/threads are waiting on the semaphore, the next
     * task/thread that attempts to wait will resume immediately.
     *
     * @param sema The semaphore to release.
     * @param count The number of tasks/threads to release.
     */
    void(CARB_ABI* releaseSemaphore)(Semaphore* sema, unsigned count);

    /**
     * Waits on a semaphore until it has been signaled.
     *
     * If the semaphore has already been signaled, this function returns immediately.
     *
     * @param sema The semaphore to wait on.
     */
    void waitSemaphore(Semaphore* sema);

    /**
     * Waits on a semaphore until it has been signaled or the timeout period expires.
     *
     * If the semaphore has already been signaled, this function returns immediately.
     *
     * @param sema The semaphore to wait on.
     * @param timeoutNs The relative timeout period in nanoseconds. Specify kInfinite to wait forever, or 0 to test
     * immediately without waiting.
     * @returns true if the semaphore count was decremented; false if the timeout period expired.
     */
    bool(CARB_ABI* timedWaitSemaphore)(Semaphore* sema, uint64_t timeoutNs);

    /**
     * Creates a fiber-aware SharedMutex primitive.
     *
     * @warning Prefer using SharedMutexWrapper instead.
     *
     * A SharedMutex (also known as a read/write mutex) allows either multiple threads/tasks to share the primitive, or
     * a single thread/task to own the primitive exclusively. Threads/tasks that request ownership of the primitive,
     * whether shared or exclusive, will be blocked until they can be granted the access level requested. SharedMutex
     * gives priority to exclusive access, but will not block additional shared access requests when exclusive access
     * is requested.
     *
     * @return A SharedMutex object. When finished, dispose of the SharedMutex with destroySharedMutex().
     */
    SharedMutex*(CARB_ABI* createSharedMutex)();

    /**
     * Requests shared access on a SharedMutex object.
     *
     * Use unlockSharedMutex() to release the shared lock. SharedMutex is not recursive.
     *
     * @param mutex The SharedMutex object.
     */
    void lockSharedMutex(SharedMutex* mutex);

    /**
     * Requests shared access on a SharedMutex object with a timeout period.
     *
     * Use unlockSharedMutex() to release the shared lock. SharedMutex is not recursive.
     *
     * @param mutex The SharedMutex object.
     * @param timeoutNs The relative timeout period in nanoseconds. Specify kInfinite to wait forever or 0 to test
     * immediately without waiting.
     * @returns true if the shared lock succeeded; false if timed out.
     */
    bool(CARB_ABI* timedLockSharedMutex)(SharedMutex* mutex, uint64_t timeoutNs);

    /**
     * Requests exclusive access on a SharedMutex object.
     *
     * Use unlockSharedMutex() to release the exclusive lock. SharedMutex is not recursive.
     *
     * @param mutex The SharedMutex object.
     */
    void lockSharedMutexExclusive(SharedMutex* mutex);

    /**
     * Requests exclusive access on a SharedMutex object with a timeout period.
     *
     * Use unlockSharedMutex() to release the exclusive lock. SharedMutex is not recursive.
     *
     * @param mutex The SharedMutex object.
     * @param timeoutNs The relative timeout period in nanoseconds. Specify kInfinite to wait forever or 0 to test
     * immediately without waiting.
     * @returns true if the exclusive lock succeeded; false if timed out.
     */
    bool(CARB_ABI* timedLockSharedMutexExclusive)(SharedMutex* mutex, uint64_t timeoutNs);

    /**
     * Releases a shared or an exclusive lock on a SharedMutex object.
     *
     * @param mutex The SharedMutex object.
     */
    void(CARB_ABI* unlockSharedMutex)(SharedMutex* mutex);

    /**
     * Destroys a SharedMutex previously created with createSharedMutex().
     *
     * @param mutex The SharedMutex object to destroy.
     */
    void(CARB_ABI* destroySharedMutex)(SharedMutex* mutex);

    /**
     * Creates a fiber-aware ConditionVariable primitive.
     *
     * @warning Prefer using ConditionVariableWrapper instead.
     *
     * ConditionVariable is a synchronization primitive that, together with a Mutex, blocks one or more threads or tasks
     * until a condition becomes true.
     *
     * @return The ConditionVariable object. Destroy with destroyConditionVariable() when finished.
     */
    ConditionVariable*(CARB_ABI* createConditionVariable)();

    /**
     * Destroys a previously-created ConditionVariable object.
     *
     * @param cv The ConditionVariable to destroy
     */
    void(CARB_ABI* destroyConditionVariable)(ConditionVariable* cv);

    /**
     * Waits on a ConditionVariable object until it is notified. Prefer using the helper function,
     * waitConditionVariablePred().
     *
     * The given Mutex must match the Mutex passed in by all other threads/tasks waiting on the ConditionVariable, and
     * must be locked by the current thread/task. While waiting, the Mutex is unlocked. When the thread/task is notified
     * the Mutex is re-locked before returning to the caller. ConditionVariables are allowed to spuriously wake up, so
     * best practice is to check the variable in a loop and sleep if the variable still does not match desired.
     *
     * @param cv The ConditionVariable to wait on.
     * @param m The Mutex that is locked by the current thread/task.
     */
    void waitConditionVariable(ConditionVariable* cv, Mutex* m);

    /**
     * Waits on a ConditionVariable object until it is notified or the timeout period expires. Prefer using the helper
     * function, timedWaitConditionVariablePred().
     *
     * The given Mutex must match the Mutex passed in by all other threads/tasks waiting on the ConditionVariable, and
     * must be locked by the current thread/task. While waiting, the Mutex is unlocked. When the thread/task is notified
     * the Mutex is re-locked before returning to the caller. ConditionVariables are allowed to spuriously wake up, so
     * best practice is to check the variable in a loop and sleep if the variable still does not match desired.
     *
     * @param cv The ConditionVariable to wait on.
     * @param m The Mutex that is locked by the current thread/task.
     * @param timeoutNs The relative timeout period in nanoseconds. Specify kInfinite to wait forever or 0 to test
     * immediately without waiting.
     * @returns true if the condition variable was notified; false if the timeout period expired.
     */
    bool(CARB_ABI* timedWaitConditionVariable)(ConditionVariable* cv, Mutex* m, uint64_t timeoutNs);

    /**
     * Wakes one thread/task currently waiting on the ConditionVariable.
     *
     * @note Having the Mutex provided to waitConditionVariable() locked while calling this function is recommended
     * but not required.
     *
     * @param cv The condition variable to notify
     */
    void(CARB_ABI* notifyConditionVariableOne)(ConditionVariable* cv);

    /**
     * Wakes all threads/tasks currently waiting on the ConditionVariable.
     *
     * @note Having the Mutex provided to waitConditionVariable() locked while calling this function is recommended
     * but not required.
     *
     * @param cv The condition variable to notify
     */
    void(CARB_ABI* notifyConditionVariableAll)(ConditionVariable* cv);

    /**
     * Changes a tasks priority.
     *
     * @note This can be used to change a task to execute on the main thread when it next resumes when using
     * Priority::eMain. If called from within the context of the running task, the task immediately suspends itself
     * until resumed on the main thread with the next call to executeMainTasks(), at which point this function will
     * return.
     *
     * @param ctx The \ref TaskContext returned by \ref getTaskContext() or \ref Future::task_if().
     * @param newPrio The Priority to change the task to.
     * @returns `true` if the priority change took effect; `false` if the TaskContext is invalid.
     */
    bool(CARB_ABI* changeTaskPriority)(TaskContext ctx, Priority newPrio);

    /**
     * Executes all tasks that have been queued with Priority::eMain until they finish or yield.
     *
     * @note Scheduled tasks (addTaskIn() / addTaskAt()) with Priority::eMain will only be executed during the next
     * executeMainTasks() call after the requisite time has elapsed.
     */
    void(CARB_ABI* executeMainTasks)();

    // Intended for internal use only; only for the RequiredObject object.
    // NOTE: The Counter returned from this function is a one-shot counter that is only intended to be passed as a
    // RequiredObject. It is immediately released.
    //! @private
    enum GroupType
    {
        eAny,
        eAll,
    };
    //! @private
    Counter*(CARB_ABI* internalGroupObjects)(GroupType type, Object const* counters, size_t count);

    /**
     * Creates a recursive mutex.
     *
     * @warning Prefer using RecursiveMutexWrapper instead.
     *
     * @note Both createMutex() and createRecursiveMutex() return a Mutex object; it is up to the creator to ensure that
     * the Mutex object is used properly. A Mutex created with createMutex() will call `std::terminate()` if recursively
     * locked.
     *
     * @return The created recursive mutex.
     */
    Mutex*(CARB_ABI* createRecursiveMutex)();

    /**
     * Attempts to cancel an outstanding task.
     *
     * If the task has already been started, has already been canceled or has completed, `false` is returned.
     *
     * If `true` is returned, then the task is guaranteed to never start, but every other side effect is as if the task
     * completed. That is, any Counter objects that were passed to addTask() will be decremented; any blocking calls to
     * waitForTask() will return `true`. The Future object for this task will no longer wait, but any attempt to read a
     * non-`void` value from it will call `std::terminate()`. If the addTask() call provided a TaskDesc::cancel member,
     * it will be called in the context of the calling thread and will finish before tryCancelTask() returns true.
     *
     * @param task The \ref TaskContext returned by \ref getTaskContext() or \ref Future::task_if().
     * @returns `true` if the task was successfully canceled and state reset as described above. `false` if the task has
     * cannot be canceled because it has already started, already been canceled or has already finished.
     */
    bool(CARB_ABI* tryCancelTask)(TaskContext task);

    //! @private
    bool(CARB_ABI* internalFutexWait)(const void* addr, const void* compare, size_t size, uint64_t timeoutNs);

    //! @private
    unsigned(CARB_ABI* internalFutexWakeup)(const void* addr, unsigned count);

    /**
     * Attempts to allocate task storage, which is similar to thread-local storage but specific to a task.
     *
     * Allocates a "key" for Task Storage. A value can be stored at this key location ("slot") that is specific to each
     * task. When the task finishes, @p fn is executed for any non-`nullptr` value stored in that slot.
     *
     * Values can be stored in the Task Storage slot with setTaskStorage() and getTaskStorage().
     *
     * When Task Storage is no longer needed, use freeTaskStorage() to return the slot to the system.
     *
     * @warning The number of slots are very limited. If no slots are available, kInvalidTaskStorageKey is returned.
     *
     * @param fn (Optional) A destructor function called when a task finishes with a non-`nullptr` value in the
     * allocated slot. The value stored with setTaskStorage() is passed to the destructor. If a destructor is not
     * desired, `nullptr` can be passed.
     * @returns An opaque TaskStorageKey representing the slot for the requested Task Storage data. If no slots are
     * available, kInvalidTaskStorageKey is returned.
     */
    TaskStorageKey(CARB_ABI* allocTaskStorage)(TaskStorageDestructorFn fn);

    /**
     * Frees a Task Storage slot.
     *
     * @note Any associated destructor function registered with allocTaskStorage() will not be called for any data
     * present in currently running tasks. Once freeTaskStorage() returns, the destructor function registered with
     * allocTaskStorage() will not be called for any data on any tasks.
     *
     * @param key The Task Storage key previously allocated with allocTaskStorage().
     */
    void(CARB_ABI* freeTaskStorage)(TaskStorageKey key);

    /**
     * Stores a value at a slot in Task Storage for the current task.
     *
     * The destructor function passed to allocTaskStorage() will be called with any non-`nullptr` values remaining in
     * Task Storage at the associated @p key when the task finishes.
     *
     * @warning This function can only be called from task context, otherwise `false` is returned.
     * @param key The Task Storage key previously allocated with allocTaskStorage().
     * @param value A value to store at the Task Storage slot described by @p key for the current task only.
     * @return `true` if the value was stored; `false` otherwise.
     */
    bool(CARB_ABI* setTaskStorage)(TaskStorageKey key, void* value);

    /**
     * Retrieves a value at a slot in Task Storage for the current task.
     *
     * The destructor function passed to allocTaskStorage() will be called with any non-`nullptr` values remaining in
     * Task Storage at the associated @p key when the task finishes.
     *
     * @warning This function can only be called from task context, otherwise `nullptr` is returned.
     * @param key The Task Storage key previously allocated with allocTaskStorage().
     * @returns The value previously passed to setTaskStorage(), or `nullptr` if not running in task context or a value
     * was not previously passed to setTaskStorage() for the current task.
     */
    void*(CARB_ABI* getTaskStorage)(TaskStorageKey key);

    // Do not call directly; use ScopedTracking instead.
    // Returns a special tracking object that MUST be passed to endTracking().
    //! @private
    Object(CARB_ABI* beginTracking)(Object const* trackers, size_t numTrackers);

    // Do not call directly; use ScopedTracking instead.
    //! @private
    void(CARB_ABI* endTracking)(Object tracker);

    /**
     * Retrieves debug information about a specific task.
     *
     * @note This information is intended for debug only and should not affect application state or decisions in the
     * application.
     *
     * @warning Since carb.tasking is an inherently multi-threaded API, the values presented as task debug information
     * may have changed in a worker thread in the short amount of time between when they were generated and when they
     * were read by the application. As such, the debug information was true at a previous point in time and should not
     * be considered necessarily up-to-date.
     *
     * @param task The TaskContext to retrieve information about.
     * @param[out] out A structure to fill with debug information about @p task. The TaskDebugInfo::sizeOf field must be
     *     pre-filled by the caller. May be `nullptr` to determine if @p task is valid.
     * @returns `true` if the TaskContext was valid and @p out (if non-`nullptr`) was filled with known information
     *     about @p task. `false` if @p out specified an unknown size or @p task does not refer to a valid task.
     */
    bool(CARB_ABI* getTaskDebugInfo)(TaskContext task, TaskDebugInfo* out);

    /**
     * Walks all current tasks and calls a callback function with debug info for each.
     *
     * @note This information is intended for debug only and should not affect application state or decisions in the
     * application.
     *
     * @warning Since carb.tasking is an inherently multi-threaded API, the values presented as task debug information
     * may have changed in a worker thread in the short amount of time between when they were generated and when they
     * were read by the application. As such, the debug information was true at a previous point in time and should not
     * be considered necessarily up-to-date.
     *
     * @param info A structure to fill with debug information about tasks encountered during the walk. The
     *     TaskDebugInfo::sizeOf field must be pre-filled by the caller.
     * @param fn A function to call for each task encountered. The function is called repeatedly with a different task
     *     each time, until all tasks have been visited or the callback function returns `false`.
     * @param context Application-specific context information that is passed directly to each invocation of @p fn.
     */
    bool(CARB_ABI* walkTaskDebugInfo)(TaskDebugInfo& info, TaskDebugInfoFn fn, void* context);

    //! @private
    void(CARB_ABI* internalApplyRangeBatch)(size_t range, size_t batchHint, ApplyBatchFn fn, void* context);

    //! @private
    void(CARB_ABI* internalBindTrackers)(Object required, Object const* ptrackes, size_t numTrackers);

    //! @private
    void(CARB_ABI* internalNameTask)(TaskContext task, const char* name, bool dynamic);

    /**
     * Instructs ITasking to reload all IFiberEvents interfaces.
     *
     * The @ref IFiberEvents interface is used by @ref ITasking to notify listeners that are interested in fiber-switch
     * events. All @ref IFiberEvents interfaces are queried from the @ref carb::Framework by @ref ITasking only at
     * startup, or when @ref changeParameters() is called, or when `reloadFiberEvents()` is called.
     *
     * Unlike @ref changeParameters(), this function is safe to call from within a task, or from multiple threads
     * simultaneously, and tasks can be added while this function is executing.
     *
     * @note This function is a task system synchronization point, requiring all task threads to synchronize and pause
     *  before reloading @ref IFiberEvents interfaces. Generally this happens rapidly, but if a task thread is busy, the
     *  entire tasking system will wait for the task to finish or enter a wait state before reloading @ref IFiberEvents
     *  interfaces.
     */
    void(CARB_ABI* reloadFiberEvents)();

    ///////////////////////////////////////////////////////////////////////////
    // Helper functions

    /**
     * Yields execution to another task until `counter == value`.
     *
     * Task invoking this call will resume on the same thread due to thread pinning. Thread pinning is not efficient.
     * See pinToCurrentThread() for details.
     *
     * @param counter             The counter to check.
     */
    void yieldUntilCounterPinThread(RequiredObject counter);

    /**
     * Checks @p pred in a loop until it returns true, and waits on a ConditionVariable if @p pred returns false.
     *
     * @param cv The ConditionVariable to wait on
     * @param m The Mutex associated with the ConditionVariable. Must be locked by the calling thread/task.
     * @param pred A function-like predicate object in the form `bool(void)`. waitConditionVariablePred() returns when
     * @p pred returns true.
     */
    template <class Pred>
    void waitConditionVariablePred(ConditionVariable* cv, Mutex* m, Pred&& pred)
    {
        while (!pred())
        {
            this->waitConditionVariable(cv, m);
        }
    }

    /**
     * Checks @p pred in a loop until it returns true or the timeout period expires, and waits on a ConditionVariable
     * if @p pred returns false.
     *
     * @param cv The ConditionVariable to wait on
     * @param m The Mutex associated with the ConditionVariable. Must be locked by the calling thread/task.
     * @param timeoutNs The relative timeout period in nanoseconds. Specify @ref kInfinite to wait forever or 0 to test
     * immediately without waiting.
     * @param pred A function-like predicate object in the form `bool(void)`. waitConditionVariablePred() returns when
     * @p pred returns true.
     * @returns `true` if the predicate returned `true`; `false` if the timeout period expired
     */
    template <class Pred>
    bool timedWaitConditionVariablePred(ConditionVariable* cv, Mutex* m, uint64_t timeoutNs, Pred&& pred)
    {
        while (!pred())
            if (!this->timedWaitConditionVariable(cv, m, timeoutNs))
                return false;
        return true;
    }

    /**
     * Executes a task synchronously.
     *
     * @note To ensure that the task executes in task context, the function is called directly if already in task
     * context. If called from non-task context, @p f is executed by a call to addTask() but this function does not
     * return until the subtask is complete.
     *
     * @param priority The priority of the task to execute. Only used if not called in task context.
     * @param f A C++ "Callable" object (i.e. functor, lambda, [member] function ptr) that optionally returns a value.
     * @param args Arguments to pass to @p f.
     * @return The return value of @p f.
     */
    template <class Callable, class... Args>
    auto awaitSyncTask(Priority priority, Callable&& f, Args&&... args);

    /**
     * Runs the given function-like object as a task.
     *
     * @param priority  The priority of the task to execute.
     * @param trackers (optional) A `std::initializer_list` of zero or more Tracker objects. Note that this *must* be a
     * temporary object. The Tracker objects can be used to determine task completion or to provide input/output
     * parameters to the task system.
     * @param f A C++ "Callable" object (i.e. functor, lambda, [member] function ptr) that optionally returns a value
     * @param args Arguments to pass to @p f
     * @return A Future based on the return type of @p f
     */
    template <class Callable, class... Args>
    auto addTask(Priority priority, Trackers&& trackers, Callable&& f, Args&&... args);

    /**
     * Adds a task to the internal queue.
     *
     * @note Deprecated: The other addTask() (and variant) functions accept lambdas and function-like objects, and are
     * designed to simplify adding tasks and add tasks succinctly. Prefer using those functions.
     *
     * @param desc       The TaskDesc describing the task.
     * @param counter    A counter to associate with this task. It will be incremented by 1.
     *   When the task completes, it will be decremented.
     * @return A TaskContext that can be used to refer to this task
     */
    CARB_DEPRECATED("Use a C++ addTask() function") TaskContext addTask(TaskDesc desc, Counter* counter)
    {
        return this->internalAddTask(desc, counter);
    }

    /**
     * Runs the given function-like object as a task when a Semaphore is signaled.
     *
     * @param throttler (optional) A Semaphore used to throttle the number of tasks that can run concurrently. The task
     * waits until the semaphore is signaled (released) before starting, and then signals the semaphore after the task
     * has executed.
     * @param priority The priority of the task to execute.
     * @param trackers (optional) A `std::initializer_list` of zero or more Tracker objects. Note that this *must* be a
     * temporary object. The Tracker objects can be used to determine task completion or to provide input/output
     * parameters to the task system.
     * @param f A C++ "Callable" object (i.e. functor, lambda, [member] function ptr) that optionally returns a value
     * @param args Arguments to pass to @p f
     * @return A Future based on the return type of @p f
     */
    template <class Callable, class... Args>
    auto addThrottledTask(Semaphore* throttler, Priority priority, Trackers&& trackers, Callable&& f, Args&&... args);

    /**
     * Runs the given function-like object as a task once a Counter reaches its target.
     *
     * @param requiredObject (optional) An object convertible to RequiredObject (such as a task or Future).
     * that will, upon completing, trigger the execution of this task.
     * @param priority  The priority of the task to execute.
     * @param trackers (optional) A `std::initializer_list` of zero or more Tracker objects. Note that this *must* be a
     * temporary object. The Tracker objects can be used to determine task completion or to provide input/output
     * parameters to the task system.
     * @param f A C++ "Callable" object (i.e. functor, lambda, [member] function ptr) that optionally returns a value
     * @param args Arguments to pass to @p f
     * @return A Future based on the return type of @p f
     */
    template <class Callable, class... Args>
    auto addSubTask(RequiredObject requiredObject, Priority priority, Trackers&& trackers, Callable&& f, Args&&... args);

    /**
     * Runs the given function-like object as a task once a Counter reaches its target and when a Semaphore is signaled.
     *
     * @param requiredObject (optional) An object convertible to RequiredObject (such as a task or Future).
     * that will, upon completing, trigger the execution of this task.
     * @param throttler (optional) A semaphore used to throttle the number of tasks that can run concurrently. Once
     * requiredObject becomes signaled, the task waits until the semaphore is signaled (released) before starting, and
     * then signals the semaphore after the task has executed.
     * @param priority The priority of the task to execute.
     * @param trackers (optional) A `std::initializer_list` of zero or more Tracker objects. Note that this *must* be a
     * temporary object. The Tracker objects can be used to determine task completion or to provide input/output
     * parameters to the task system.
     * @param f A C++ "Callable" object (i.e. functor, lambda, [member] function ptr) that optionally returns a value
     * @param args Arguments to pass to @p f
     * @return A Future based on the return type of @p f
     */
    template <class Callable, class... Args>
    auto addThrottledSubTask(RequiredObject requiredObject,
                             Semaphore* throttler,
                             Priority priority,
                             Trackers&& trackers,
                             Callable&& f,
                             Args&&... args);

    /**
     * Adds a task to occur after a specific duration has passed.
     *
     * @param dur The duration to wait for. The task is not started until this duration elapses.
     * @param priority The priority of the task to execute
     * @param trackers (optional) A `std::initializer_list` of zero or more Tracker objects. Note that this *must* be a
     * temporary object. The Tracker objects can be used to determine task completion or to provide input/output
     * parameters to the task system.
     * @param f A C++ "Callable" object (i.e. functor, lambda, [member] function ptr) that optionally returns a value
     * @param args Arguments to pass to @p f
     * @return A Future based on the return type of @p f
     */
    template <class Callable, class Rep, class Period, class... Args>
    auto addTaskIn(const std::chrono::duration<Rep, Period>& dur,
                   Priority priority,
                   Trackers&& trackers,
                   Callable&& f,
                   Args&&... args);

    /**
     * Adds a task to occur at a specific point in time
     *
     * @param when The point in time at which to begin the task
     * @param priority The priority of the task to execute
     * @param trackers (optional) A `std::initializer_list` of zero or more Tracker objects. Note that this *must* be a
     * temporary object. The Tracker objects can be used to determine task completion or to provide input/output
     * parameters to the task system.
     * @param f A C++ "Callable" object (i.e. functor, lambda, [member] function ptr) that optionally returns a value
     * @param args Arguments to pass to @p f
     * @return A Future based on the return type of @p f
     */
    template <class Callable, class Clock, class Duration, class... Args>
    auto addTaskAt(const std::chrono::time_point<Clock, Duration>& when,
                   Priority priority,
                   Trackers&& trackers,
                   Callable&& f,
                   Args&&... args);

    /**
     * Processes a range from `[0..range)` calling a functor for each index, potentially from different threads.
     *
     * @note This function does not return until @p f has been called (and returned) on every index from [0..
     * @p range)
     * @warning Since @p f can be called from multiple threads simultaneously, all operations it performs must
     * be thread-safe. Additional consideration must be taken since mutable captures of any lambdas or passed in
     * @p args will be accessed simultaneously by multiple threads so care must be taken to ensure thread safety.
     * @note Calling this function recursively will automatically scale down the parallelism in order to not overburden
     * the system.
     * @note As there is overhead to calling \p f repeatedly, it is more efficient to use \ref applyRangeBatch() with
     * `batchHint = 0` and a `f` that handles multiple indexes on one invocation.
     *
     * See the @rstref{additional documentation <tasking-parallel-for>} for `applyRange`.
     *
     * @param range The number of times to call @p f.
     * @param f A C++ "Callable" object (i.e. functor, lambda, [member] function ptr) that is repeatedly called until
     * all indexes in `[0..range)` have been processed, potentially from different threads. It is invoked with
     * parameters `f(args..., index)` where `index` is within the range `[0..range)`.
     * @param args Arguments to pass to @p f
     */
    template <class Callable, class... Args>
    void applyRange(size_t range, Callable f, Args&&... args);

    /**
     * Processes a range from `[0..range)` calling a functor for batches of indexes, potentially from different threads.
     *
     * @note This function does not return until @p f has been called (and returned) for every index from
     * `[0..range)`
     * @warning Since @p f can be called from multiple threads simultaneously, all operations it performs must
     * be thread-safe. Additional consideration must be taken since mutable captures of any lambdas or passed in
     * @p args will be accessed simultaneously by multiple threads so care must be taken to ensure thread safety.
     * @note Calling this function recursively will automatically scale down the parallelism in order to not overburden
     * the system.
     *
     * See the @rstref{additional documentation <tasking-parallel-for>} for `applyRange`.
     *
     * @param range The number of times to call @p f.
     * @param batchHint A recommendation of batch size to determine the range of indexes to pass to @p f for processing.
     * A value of 0 uses an internal heuristic to divide work, which is recommended in most cases. This value is a hint
     * to the internal heuristic and therefore \p f may be invoked with a different range size.
     * @param f A C++ "Callable" object (i.e. functor, lambda, [member] function ptr) that is repeatedly called until
     * all indexes in `[0..range)` have been processed, potentially from different threads. It is invoked with
     * parameters `f(args..., startIndex, endIndex)` where `[startIndex..endIndex)` is the range of indexes that must be
     * processed by that invocation of `f`. Note that `endIndex` is a past-the-end index and must not actually be
     * processed by that invocation of `f`.
     * @param args Arguments to pass to @p f
     */
    template <class Callable, class... Args>
    void applyRangeBatch(size_t range, size_t batchHint, Callable f, Args&&... args);

    /**
     * Processes a range from [begin..end) calling a functor for each index, potentially from different threads.
     *
     * @note This function does not return until @p f has been called (and returned) on every index from [begin..
     * @p end)
     * @warning Since @p f can be called from multiple threads simultaneously, all operations it performs must
     * be thread-safe. Additional consideration must be taken since mutable captures of any lambdas or passed in
     * @p args will be accessed simultaneously by multiple threads so care must be taken to ensure thread safety.
     * @note Calling this function recursively will automatically scale down the parallelism in order to not overburden
     * the system.
     *
     * @param begin The starting value passed to @p f
     * @param end The ending value. Every T(1) step in [begin, end) is passed to @p f
     * @param f A C++ "Callable" object (i.e. functor, lambda, [member] function ptr) that optionally returns a value.
     * The index value from [begin..end) is passed as the last parameter (after any passed @p args).
     * @param args Arguments to pass to @p f
     */
    template <class T, class Callable, class... Args>
    void parallelFor(T begin, T end, Callable f, Args&&... args);

    /**
     * Processes a stepped range from [begin..end) calling a functor for each step, potentially from different threads.
     *
     * @note This function does not return until @p f has been called (and returned) on every index from [begin..
     * @p end)
     * @warning Since @p f can be called from multiple threads simultaneously, all operations it performs must
     * be thread-safe. Additional consideration must be taken since mutable captures of any lambdas or passed in
     * @p args will be accessed simultaneously by multiple threads so care must be taken to ensure thread safety.
     * @note Calling this function recursively will automatically scale down the parallelism in order to not overburden
     * the system.
     *
     * @param begin The starting value passed to @p f
     * @param end The ending value. Every @p step in [begin, end) is passed to @p f
     * @param step The step size to determine every value passed to @p f
     * @param f A C++ "Callable" object (i.e. functor, lambda, [member] function ptr) that optionally returns a value.
     * The stepped value from [begin..end) is passed as the last parameter (after any passed @p args).
     * @param args Arguments to pass to @p f
     */
    template <class T, class Callable, class... Args>
    void parallelFor(T begin, T end, T step, Callable f, Args&&... args);

    /**
     * Causes the current thread or task to sleep for the specified time.
     *
     * @note This function is fiber-aware. If currently executing in a fiber, the fiber will be yielded until the
     * requested amount of time has passed. If a thread is currently executing, then the thread will sleep.
     *
     * @param dur The duration to sleep for
     */
    template <class Rep, class Period>
    void sleep_for(const std::chrono::duration<Rep, Period>& dur)
    {
        sleepNs(detail::convertDuration(dur));
    }
    /**
     * Causes the current thread or task to sleep until the specified time.
     *
     * @note This function is fiber-aware. If currently executing in a fiber, the fiber will be yielded until the
     * requested amount of time has passed. If a thread is currently executing, then the thread will sleep.
     *
     * @param tp The absolute time point to sleep until
     */
    template <class Clock, class Duration>
    void sleep_until(const std::chrono::time_point<Clock, Duration>& tp)
    {
        sleepNs(detail::convertAbsTime(tp));
    }

    /**
     * A fiber-safe futex implementation: if @p val equals @p compare, the thread or task sleeps until woken.
     *
     * @warning Futexes are complicated and error-prone. Prefer using higher-level synchronization primitives.
     *
     * @param val The atomic value to check.
     * @param compare The value to compare against. If @p val matches this, then the calling thread or task sleeps until
     * futexWakeup() is called.
     */
    template <class T>
    void futexWait(const std::atomic<T>& val, T compare)
    {
        bool b = internalFutexWait(&val, &compare, sizeof(T), kInfinite);
        CARB_ASSERT(b);
        CARB_UNUSED(b);
    }

    /**
     * A fiber-safe futex implementation: if @p val equals @p compare, the thread or task sleeps until woken or the
     * timeout period expires.
     *
     * @warning Futexes are complicated and error-prone. Prefer using higher-level synchronization primitives.
     *
     * @param val The atomic value to check.
     * @param compare The value to compare against. If @p val matches this, then the calling thread or task sleeps until
     * futexWakeup() is called.
     * @param dur The maximum duration to wait.
     * @returns `true` if @p val doesn't match @p compare or if futexWakeup() was called; `false` if the timeout period
     * expires.
     */
    template <class T, class Rep, class Period>
    bool futexWaitFor(const std::atomic<T>& val, T compare, std::chrono::duration<Rep, Period> dur)
    {
        return internalFutexWait(&val, &compare, sizeof(T), detail::convertDuration(dur));
    }

    /**
     * A fiber-safe futex implementation: if @p val equals @p compare, the thread or task sleeps until woken or the
     * specific time is reached.
     *
     * @warning Futexes are complicated and error-prone. Prefer using higher-level synchronization primitives.
     *
     * @param val The atomic value to check.
     * @param compare The value to compare against. If @p val matches this, then the calling thread or task sleeps until
     * futexWakeup() is called.
     * @param when The clock time to wait until.
     * @returns `true` if @p val doesn't match @p compare or if futexWakeup() was called; `false` if the clock time is
     * reached.
     */
    template <class T, class Clock, class Duration>
    bool futexWaitUntil(const std::atomic<T>& val, T compare, std::chrono::time_point<Clock, Duration> when)
    {
        return internalFutexWait(&val, &compare, sizeof(T), detail::convertAbsTime(when));
    }

    /**
     * Wakes threads or tasks waiting in futexWait(), futexWaitFor() or futexWaitUntil().
     *
     * @warning Futexes are complicated and error-prone. Prefer using higher-level synchronization primitives.
     *
     * @param val The same `val` passed to futexWait(), futexWaitFor() or futexWaitUntil().
     * @param count The number of threads or tasks to wakeup. To wake all waiters use `UINT_MAX`.
     * @returns The number of threads or tasks that were waiting and are now woken.
     */
    template <class T>
    unsigned futexWakeup(const std::atomic<T>& val, unsigned count)
    {
        return internalFutexWakeup(&val, count);
    }

    /**
     * Binds any number of \ref Tracker objects to the given \ref RequiredObject. Effectively allows adding trackers to
     * a given object.
     *
     * Previously this was only achievable through a temporary task:
     * ```cpp
     * // Old way: a task that would bind `taskGroup` to `requiredObject`
     * tasking->addSubTask(requiredObject, Priority::eDefault, { taskGroup }, []{});
     * // New way: direct binding:
     * tasking->bindTrackers(requiredObject, { taskGroup });
     * ```
     * The previous method wasted time in that one of the task threads would eventually have to pop the task from the
     * queue and run an empty function. Calling `bindTrackers()` does not waste this time.
     *
     * However, there are some "disadvantages." The `addSubTask()` method would allocate a \ref TaskContext, return a
     * \ref Future, and could be canceled. These features were seldom needed, hence this function.
     *
     * @param requiredObject An object convertible to RequiredObject (such as a task or Future). The given \p trackers
     * will be bound to this required object.
     * @param trackers A `std::initializer_list` of zero or more Tracker objects. Note that this *must* be a
     * temporary object. The Tracker objects can be used to determine task completion or to provide input/output
     * parameters to the task system.
     */
    void bindTrackers(RequiredObject requiredObject, Trackers&& trackers);

    /**
     * Sets a name for a task for debugging purposes, similar to how threads can be named.
     *
     * This function is optimized for a literal string: passing a literal string as @p name does not copy the string but
     * instead retains the pointer as it is guaranteed to never change. Passing a non-literal string will result in a
     * copy.
     *
     * The task name is visible in the debugger as @rstref{debug information <showing-all-tasks>} for a task.
     *
     * Retrieving the task name can be accomplished through @ref getTaskDebugInfo().
     *
     * @note It is often easier to name the task as it's created by passing a task name as a @ref Tracker object to
     *     @ref addTask() or the other task creation functions.
     *
     * @thread_safety It is safe to set a task name from multiple threads, though inadvisable. Reading the task name via
     *     @ref getTaskDebugInfo() while it is being changed in a different thread is not strongly ordered and may
     *     result in an empty string being read, or random bytes as the name string, but will not result in a crash.
     * @tparam T A type that is convertible to `const char*`. See @p name below.
     * @param task The @ref TaskContext to name. If this is not a valid task, or the task has already completed or has
     *     been cancelled, nothing happens.
     * @param name Either a `const char*` (dynamic string) or a `const char (&)[N]` (literal string) as the string name.
     *     May be `nullptr` to un-set a task name. Dynamic strings will be copied before the call returns. Literal
     *     strings will be retained by pointer value.
     */
    template <class T, std::enable_if_t<std::is_convertible<T, const char*>::value, bool> = false>
    void nameTask(TaskContext task, T&& name)
    {
        internalNameTask(task, name, !detail::is_literal_string<T>::value);
    }
};

/**
 * Causes the currently executing TaskContext to be "pinned" to the thread it is currently running on until PinGuard is
 * destroyed.
 *
 * Appropriately handles recursive pinning. This class causes the current thread to be the only task thread that can run
 * the current task. This is necessary in some cases where thread specificity is required (those these situations are
 * NOT recommended for tasks): holding a mutex, or using thread-specific data, etc. Thread pinning is not efficient (the
 * pinned thread could be running a different task causing delays for the current task to be resumed, and wakeTask()
 * must wait to return until the pinned thread has been notified) and should therefore be avoided.
 *
 * @note It is assumed that the task is allowed to move to another thread during the pinning process, though this may
 * not always be the case. Only after the PinGuard is constructed will a task be pinned. Therefore, make sure to
 * construct PinGuard *before* any operation that requires pinning.
 */
class PinGuard
{
public:
    /**
     * Constructs a PinGuard and enters the "pinned" scope.
     */
    PinGuard() : m_wasPinned(carb::getCachedInterface<ITasking>()->pinToCurrentThread())
    {
    }

    /**
     * Constructs a PinGuard and enters the "pinned" scope.
     * @note Deprecated: ITasking no longer needed.
     */
    CARB_DEPRECATED("ITasking no longer needed.")
    PinGuard(ITasking*) : m_wasPinned(carb::getCachedInterface<ITasking>()->pinToCurrentThread())
    {
    }

    /**
     * Destructs a PinGuard and leaves the "pinned" scope.
     */
    ~PinGuard()
    {
        if (!m_wasPinned)
            carb::getCachedInterface<ITasking>()->unpinFromCurrentThread();
    }

private:
    bool m_wasPinned;
};

} // namespace tasking
} // namespace carb

#include "ITasking.inl"
