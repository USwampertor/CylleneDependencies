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
//! @brief carb.tasking type definitions
#pragma once

#include "../Defines.h"

namespace carb
{
namespace tasking
{

/**
 * Used to create dependencies between tasks and to wait for a set of tasks to finish.
 *
 * @note Prefer using CounterWrapper.
 *
 * @see ITasking::createCounter(), ITasking::createCounterWithTarget(), ITasking::destroyCounter(),
 * ITasking::yieldUntilCounter(), ITasking::timedYieldUntilCounter(), ITasking::checkCounter(),
 * ITasking::getCounterValue(), ITasking::getCounterTarget(), ITasking::fetchAddCounter(), ITasking::fetchSubCounter(),
 * ITasking::storeCounter()
 */
class Counter DOXYGEN_EMPTY_CLASS;

/**
 * A fiber-aware mutex: a synchronization primitive for mutual exclusion. Only one thread/fiber can "own" the mutex at
 * a time.
 *
 * @note Prefer using MutexWrapper.
 *
 * @see ITasking::createMutex(), ITasking::destroyMutex(), ITasking::lockMutex(), ITasking::timedLockMutex(),
 * ITasking::unlockMutex(), ITasking::createRecursiveMutex()
 */
class Mutex DOXYGEN_EMPTY_CLASS;

/**
 * A fiber-aware semaphore: a synchronization primitive that limits to N threads/fibers.
 *
 * @note Prefer using SemaphoreWrapper.
 *
 * @see ITasking::createSemaphore(), ITasking::destroySemaphore(), ITasking::releaseSemaphore(),
 * ITasking::waitSemaphore(), ITasking::timedWaitSemaphore()
 */
class Semaphore DOXYGEN_EMPTY_CLASS;

/**
 * A fiber-aware shared_mutex: a synchronization primitive that functions as a multiple-reader/single-writer lock.
 *
 * @note Prefer using SharedMutexWrapper.
 *
 * @see ITasking::createSharedMutex(), ITasking::lockSharedMutex(), ITasking::timedLockSharedMutex(),
 * ITasking::lockSharedMutexExclusive(), ITasking::timedLockSharedMutexExclusive(), ITasking::unlockSharedMutex(),
 * ITasking::destroySharedMutex()
 */
class SharedMutex DOXYGEN_EMPTY_CLASS;

/**
 * A fiber-aware condition_variable: a synchronization primitive that, together with a Mutex, blocks one or more threads
 * or tasks until a condition becomes true.
 *
 * @note Prefer using ConditionVariableWwrapper.
 *
 * @see ITasking::createConditionVariable(), ITasking::destroyConditionVariable(), ITasking::waitConditionVariable(),
 * ITasking::timedWaitConditionVariable(), ITasking::notifyConditionVariableOne(),
 * ITasking::notifyConditionVariableAll()
 */
class ConditionVariable DOXYGEN_EMPTY_CLASS;

struct ITasking;

/**
 * A constant for ITasking wait functions indicating "infinite" timeout
 */
constexpr uint64_t kInfinite = uint64_t(-1);

/**
 * Defines a task priority.
 */
enum class Priority
{
    eLow, //!< Low priority. Tasks will be executed after higher priority tasks.
    eMedium, //!< Medium priority.
    eHigh, //!< High priority. Tasks will be executed before lower priority tasks.

    eMain, //!< A special priority for tasks that are only executed during ITasking::executeMainTasks()

    eCount, //!< The number of Priority classes

    // Aliases
    eDefault = eMedium, //!< Alias for eMedium priority.
};

/**
 * Object type for Object.
 *
 * @note These are intended to be used only by helper classes such as RequiredObject.
 */
enum class ObjectType
{
    eNone, //!< Null/no object.
    eCounter, //!< Object::data refers to a Counter*.
    eTaskContext, //!< Object::data refers to a TaskContext.
    ePtrTaskContext, //!< Object::data refers to a TaskContext*.
    eTaskGroup, //!< Object::data is a pointer to a std::atomic_size_t. @see TaskGroup
    eSharedState, //!< Object::data is a pointer to a detail::SharedState. Not used internally by carb.tasking.
    eFutex1, //!< Object::data is a pointer to a std::atomic_uint8_t. Signaled on zero.
    eFutex2, //!< Object::data is a pointer to a std::atomic_uint16_t. Signaled on zero.
    eFutex4, //!< Object::data is a pointer to a std::atomic_uint32_t. Signaled on zero.
    eFutex8, //!< Object::data is a pointer to a std::atomic_uint64_t. Signaled on zero.
    eTrackerGroup, //!< Object::data is a pointer to an internal tracking object.
    eTaskName, //!< Object::data is a `const char*` to be copied and used as a task name.
    eTaskNameLiteral, //!< Object::data is a `const char*` that can be retained because it is a literal.
};


/**
 * The function to execute as a task.
 *
 * @param taskArg The argument passed to ITasking::addTask() variants.
 */
using OnTaskFn = void (*)(void* taskArg);

/**
 * The function executed by ITasking::applyRange()
 *
 * @param index The ApplyFn is called once for every integer @p index value from 0 to the range provided to
 * ITasking::applyRange().
 * @param taskArg The argument passed to ITasking::applyRange().
 */
using ApplyFn = void (*)(size_t index, void* taskArg);

/**
 * The function executed by ITasking::applyRangeBatch()
 *
 * @note This function differs from \ref ApplyFn in that it must handle a contiguous range of indexes determined by
 * `[startIndex, endIndex)`.
 * @warning The item at index \p endIndex is \b not to be processed by this function. In other words, the range handled
 * by this function is:
 * ```cpp
 *     for (size_t i = startIndex; i != endIndex; ++i)
 *         array[i]->process();
 * ```
 * @param startIndex The initial index that must be handled by this function call.
 * @param endIndex The after-the-end index representing the range of indexes that must be handled by this function call.
 * The item at this index is after-the-end of the assigned range and <strong>must not be processed</strong>.
 * @param taskArg The argument passed to ITasking::applyRangeBatch().
 */
using ApplyBatchFn = void (*)(size_t startIndex, size_t endIndex, void* taskArg);

/**
 * A destructor function for a Task Storage slot.
 *
 * This function is called when a task completes with a non-`nullptr` value in the respective Task Storage slot.
 * @see ITasking::allocTaskStorage()
 * @param arg The non-`nullptr` value stored in a task storage slot.
 */
using TaskStorageDestructorFn = void (*)(void* arg);

/**
 * An opaque handle representing a Task Storage slot.
 */
using TaskStorageKey = size_t;

/**
 * Represents an invalid TaskStorageKey.
 */
constexpr TaskStorageKey kInvalidTaskStorageKey = size_t(-1);

/**
 * An opaque handle that is used with getTaskContext(), suspendTask() and wakeTask().
 */
using TaskContext = size_t;

/**
 * A specific value for TaskContext that indicates a non-valid TaskContext.
 */
constexpr TaskContext kInvalidTaskContext = 0;

/**
 * The absolute maximum number of fibers that ITasking will create.
 */
constexpr uint32_t kMaxFibers = 65535;

/**
 * A generic @rstref{ABI-safe <abi-compatibility>} representation of multiple types.
 */
struct Object
{
    ObjectType type; //!< The ObjectType of the represented type.
    void* data; //!< Interpreted based on the ObjectType provided.
};

/**
 * Defines a task descriptor.
 */
struct TaskDesc
{
    /// Must be set to sizeof(TaskDesc).
    size_t size{ sizeof(TaskDesc) };

    /// The task function to execute
    OnTaskFn task;

    /// The argument passed to the task function
    void* taskArg;

    /// The priority assigned to the task
    Priority priority;

    /// If not nullptr, then the task will only start when this counter reaches its target value. Specifying the counter
    /// here is more efficient than having the task function yieldUntilCounter().
    Object requiredObject;

    /// If waitSemaphore is not nullptr, then the task will wait on the semaphore before starting. This can be used to
    /// throttle tasks. If requiredObject is also specified, then the semaphore is not waited on until requiredObject
    /// has reached its target value. Specifying the semaphore here is more efficient than having the task function
    /// wait on the semaphore.
    Semaphore* waitSemaphore;

    /// Optional. An OnTaskFn that is executed only when ITasking::tryCancelTask() successfully cancels the task. Called
    /// in the context of ITasking::tryCancelTask(). Typically provided to destroy taskArg.
    OnTaskFn cancel;

    // Internal only
    //! @private
    Object const* trackers{ nullptr };
    //! @private
    size_t numTrackers{ 0 };

    /// Constructor.
    constexpr TaskDesc(OnTaskFn task_ = nullptr,
                       void* taskArg_ = nullptr,
                       Priority priority_ = Priority::eLow,
                       Counter* requiredCounter_ = nullptr,
                       Semaphore* waitSemaphore_ = nullptr,
                       OnTaskFn cancel_ = nullptr)
        : task(task_),
          taskArg(taskArg_),
          priority(priority_),
          requiredObject{ ObjectType::eCounter, requiredCounter_ },
          waitSemaphore(waitSemaphore_),
          cancel(cancel_)
    {
    }
};

/**
 * Defines a tasking plugin descriptor.
 */
struct TaskingDesc
{
    /**
     * The size of the fiber pool, limited to kMaxFibers.
     *
     * Every task must be assigned a fiber before it can execute. A fiber is like a thread stack, but carb.tasking can
     * choose when the fibers run, as opposed to threads where the OS schedules them.
     *
     * A value of 0 means to use kMaxFibers.
     */
    uint32_t fiberCount;

    /**
     * The number of worker threads.
     *
     * A value of 0 means to use \ref carb::thread::hardware_concurrency().
     */
    uint32_t threadCount;

    /**
     * The optional array of affinity values for every thread.
     *
     * If set to `nullptr`, affinity is not set. Otherwise it must contain `threadCount` number of elements. Each
     * affinity value is a CPU index in the range [0 - `carb::thread::hardware_concurrency()`)
     */
    uint32_t* threadAffinity;

    /**
     * The stack size per fiber. 0 indicates to use the system default.
     */
    uint64_t stackSize;
};

/**
 * Debug state of a task.
 */
enum class TaskDebugState
{
    Pending, //!< The task has unmet pre-requisites and cannot be started yet.
    New, //!< The task has passed all pre-requisites and is waiting to be assigned to a task thread.
    Running, //!< The task is actively running on a task thread.
    Waiting, //!< The task has been started but is currently waiting and is not running on a task thread.
    Finished, //!< The task has finished or has been canceled.
};

/**
 * Defines debug information about a task retrieved by ITasking::getTaskDebugInfo() or ITasking::walkTaskDebugInfo().
 *
 * @note This information is intended for debug only and should not affect application state or decisions in the
 * application.
 *
 * @warning Since carb.tasking is an inherently multi-threaded API, the values presented as task debug information
 * may have changed in a worker thread in the short amount of time between when they were generated and when they were
 * read by the application. As such, the debug information was true at a previous point in time and should not be
 * considered necessarily up-to-date.
 */
struct TaskDebugInfo
{
    //! Size of this struct, used for versioning.
    size_t sizeOf{ sizeof(TaskDebugInfo) };

    //! The TaskContext handle for the task.
    TaskContext context{};

    //! The state of the task.
    TaskDebugState state{};

    //! The task function for this task that was submitted to ITasking::addTask() (or variant function), if known. May
    //! be `nullptr` if the task has finished or was canceled.
    OnTaskFn task{};

    //! The task argument for this task that was submitted to ITasking::addTask() (or variant function), if known. May
    //! be `nullptr` if the task has finished or was canceled.
    void* taskArg{};

    //! Input: the maximum number of frames that can be stored in the memory pointed to by the `creationCallstack`
    //! member.
    //! Output: the number of frames that were stored in the memory pointed to by the `creationCallstack` member.
    size_t numCreationFrames{ 0 };

    //! The callstack that called ITasking::addTask() (or variant function). The callstack is only available if
    //! carb.tasking is configured to capture callstacks with setting */plugins/carb.tasking.plugin/debugTaskBacktrace*.
    //!
    //! @note If this value is desired, prior to calling ITasking::getTaskDebugInfo() set this member to a buffer that
    //! will be filled by the ITasking::getTaskDebugInfo() function. Set `numCreationFrames` to the number of frames
    //! that can be contained in the buffer. After calling ITasking::getTaskDebugInfo(), this member will contain the
    //! available creation callstack frames and `numCreationFrames` will be set to the number of frames that could be
    //! written.
    void** creationCallstack{ nullptr };

    //! Input: the maximum number of frames that can be stored in the memory pointed to by the `waitingCallstack`
    //! member.
    //! Output: the number of frames that were stored in the memory pointed to by the `waitingCallstack` member.
    size_t numWaitingFrames{ 0 };

    //! The callstack of the task when waiting. This is only captured if carb.tasking is configured to capture
    //! callstacks with setting */plugins/carb.tasking.plugin/debugTaskBacktrace* and if `state` is
    //! TaskDebugState::Waiting.
    //!
    //! @warning Capturing this value is somewhat unsafe as debug information is not stored in a way that will impede
    //! task execution whatsoever (i.e. with synchronization), therefore information is gathered from a running task
    //! without stopping it. As such, reading the waiting callstack may produce bad data and in extremely rare cases
    //! cause a crash. If the state changes while gathering info, `state` may report TaskDebugState::Waiting but
    //! `numWaitingFrames` may be `0` even though some data was written to the buffer pointed to by `waitingCallstack`.
    //!
    //! @note If this value is desired, prior to calling ITasking::getTaskDebugInfo() set this member to a buffer that
    //! will be filled by the ITasking::getTaskDebugInfo() function. Set `numWaitingFrames` to the number of frames that
    //! can be contained in the buffer. After calling ITasking::getTaskDebugInfo(), this member will contain the
    //! available waiting callstack frames and `numWaitingFrames` will be set to the number of frames that could be
    //! written.
    void** waitingCallstack{ nullptr };

    //! Input: the maximum number of characters that can be stored in the memory pointed to by the `taskName` member.
    //! Output: the number of characters written (including the NUL terminator) to the memory pointed to by the
    //! `taskName` member.
    size_t taskNameSize{ 0 };

    //! A optional buffer that will be filled with the task name if provided.
    //!
    //! @note If this value is desired, prior to calling ITasking::getTaskDebugInfo() set this member to a buffer that
    //! will be filled by the ITasking::getTaskDebugInfo() function. Set `taskNameSize` to the number of characters that
    //! can be contained in the buffer. After calling ITasking::getTaskDebugInfo(), this member will contain the
    //! NUL-terminated task name and `taskNameSize` will be set to the number of characters that could be written
    //! (including the NUL-terminator).
    char* taskName{ nullptr };
};

//! Callback function for ITasking::walkTaskDebugInfo().
//! @param info The TaskDebugInfo structure passed to ITasking::walkTaskDebugInfo(), filled with information about a
//!     task.
//! @param context The `context` field passed to ITasking::walkTaskDebugInfo().
//! @return `true` if walking tasks should continue; `false` to terminate walking tasks.
using TaskDebugInfoFn = bool (*)(const TaskDebugInfo& info, void* context);

#ifndef DOXYGEN_BUILD
namespace detail
{

template <class T>
struct GenerateFuture;

template <class T>
class SharedState;

} // namespace detail

struct Trackers;
struct RequiredObject;

template <class T>
class Promise;
template <class T>
class SharedFuture;
#endif

/**
 * A Future is a counterpart to a Promise. It is the receiving end of a one-way, one-time asynchronous communication
 * channel for transmitting the result of an asynchronous operation.
 *
 * Future is very similar to <a href="https://en.cppreference.com/w/cpp/thread/future">std::future</a>
 *
 * Communication starts by creating a Promise. The Promise has an associated Future that can be retrieved once via
 * Promise::get_future(). The Promise and the Future both reference a "shared state" that is used to communicate the
 * result. When the result is available, it is set through Promise::set_value() (or the promise can be broken through
 * Promise::setCanceled()), at which point the shared state becomes Ready and the Future will be able to retrieve the
 * value through Future::get() (or determine cancellation via Future::isCanceled()).
 *
 * Task functions like ITasking::addTask() return a Future where the Promise side is the return value from the callable
 * passed when the task is created.
 *
 * Future is inherently a "read-once" object. Once Future::get() is called, the Future becomes invalid. However,
 * SharedFuture can be used (created via Future::share()) to retain the value. Many threads can wait on a SharedFuture
 * and access the result simultaneously through SharedFuture::get().
 *
 * There are three specializations of Future:
 * * Future<T>: The base specialization, used to communicate objects between tasks/threads.
 * * Future<T&>: Reference specialization, used to communicate references between tasks/threads.
 * * Future<void>: Void specialization, used to communicate stateless events between tasks/threads.
 *
 * The `void` specialization of Future is slightly different:
 * * Future<void> does not have Future::isCanceled(); cancellation state cannot be determined.
 */
template <class T = void>
class Future
{
public:
    /**
     * Creates a future in an invalid state (valid() would return false).
     */
    constexpr Future() noexcept = default;

    /**
     * Destructor.
     */
    ~Future();

    /**
     * Futures are movable.
     */
    Future(Future&& rhs) noexcept;

    /**
     * Futures are movable.
     */
    Future& operator=(Future&& rhs) noexcept;

    /**
     * Tests to see if this Future is valid.
     *
     * @returns true if get() and wait() are supported; false otherwise
     */
    bool valid() const noexcept;

    /**
     * Checks to see if a value can be read from this Future
     *
     * @warning Undefined behavior to call this if valid() == `false`.
     * @returns true if a value can be read from this Future; false if the value is not yet ready
     */
    bool try_wait() const;

    /**
     * Waits until a value can be read from this Future
     * @warning Undefined behavior to call this if valid() == `false`.
     */
    void wait() const;

    /**
     * Waits until a value can be read from this Future, or the timeout period expires.
     *
     * @warning Undefined behavior to call this if valid() == `false`.
     * @param dur The relative timeout period.
     * @returns true if a value can be read from this Future; false if the timeout period expires before the value can
     * be read
     */
    template <class Rep, class Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& dur) const;

    /**
     * Waits until a value can be read from this Future, or the timeout period expires.
     *
     * @warning Undefined behavior to call this if valid() == `false`.
     * @param when The absolute timeout period.
     * @returns true if a value can be read from this Future; false if the timeout period expires before the value can
     * be read
     */
    template <class Clock, class Duration>
    bool wait_until(const std::chrono::time_point<Clock, Duration>& when) const;

    /**
     * Waits until the future value is ready and returns the value. Resets the Future to an invalid state.
     *
     * @warning This function will call `std::terminate()` if the underlying task has been canceled with
     * ITasking::tryCancelTask() or the Promise was broken. Use isCanceled() to determine if the value is safe to read.
     *
     * @returns The value passed to Promise::set_value().
     */
    T get();

    /**
     * Returns whether the Promise has been broken (or if this Future represents a task, the task has been canceled).
     *
     * @warning Undefined behavior to call this if valid() == `false`.
     * @note The `void` specialization of Future does not have this function.
     * @returns `true` if the task has been canceled; `false` if the task is still pending or has a valid value to read.
     */
    bool isCanceled() const;

    /**
     * Transfers the Future's shared state (if any) to a SharedFuture and leaves `*this` invalid (valid() == `false`).
     * @returns A SharedFuture with the same shared state as `*this`.
     */
    SharedFuture<T> share();

    /**
     * Returns a valid TaskContext if this Future represents a task.
     *
     * @note Futures can be returned from \ref ITasking::addTask() and related functions or from
     * \ref Promise::get_future(). Only Future objects returned from \ref ITasking::addTask() will return a valid
     * pointer from task_if().
     *
     * @returns A pointer to a TaskContext if this Future was created from \ref ITasking::addTask() or related
     * functions; `nullptr` otherwise. The pointer is valid as long as the Future exists and the result from
     * \ref valid() is `true`.
     */
    const TaskContext* task_if() const;

    /**
     * Convertible to RequiredObject.
     */
    operator RequiredObject() const;

    /**
     * Syntactic sugar around ITasking::addSubTask() that automatically passes the value from get() into `Callable` and
     * resets the Future to an invalid state.
     *
     * @warning This resets the Future to an invalid state since the value is being consumed by the sub-task.
     *
     * @note This can be used to "chain" tasks together.
     *
     * @warning If the dependent task is canceled then the sub-task will call `std::terminate()`. When canceling the
     * dependent task you must first cancel the sub-task.
     *
     * @warning For non-`void` specializations, it is undefined behavior to call this if valid() == `false`.
     *
     * @param prio The priority of the task to execute.
     * @param trackers (optional) A `std::initializer_list` of zero or more Tracker objects. Note that this *must* be a
     * temporary object. The Tracker objects can be used to determine task completion or to provide input/output
     * parameters to the task system.
     * @param f A C++ "Callable" object (i.e. functor, lambda, [member] function ptr) that optionally returns a value.
     * The Callable object must take the Future's `T` type as its last parameter.
     * @param args Arguments to pass to @p f
     * @return A Future based on the return type of @p f
     */
    template <class Callable, class... Args>
    auto then(Priority prio, Trackers&& trackers, Callable&& f, Args&&... args);

private:
    template <class U>
    friend struct detail::GenerateFuture;
    template <class U>
    friend class Promise;
    template <class U>
    friend class SharedFuture;
    CARB_PREVENT_COPY(Future);

    constexpr Future(detail::SharedState<T>* state) noexcept;
    Future(TaskContext task, detail::SharedState<T>* state) noexcept;

    detail::SharedState<T>* m_state{ nullptr };
};

#ifndef DOXYGEN_BUILD
template <>
class Future<void>
{
public:
    constexpr Future() noexcept = default;
    ~Future();

    Future(Future&& rhs) noexcept;
    Future& operator=(Future&& rhs) noexcept;

    bool valid() const noexcept;

    bool try_wait() const;
    void wait() const;
    template <class Rep, class Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& dur) const;
    template <class Clock, class Duration>
    bool wait_until(const std::chrono::time_point<Clock, Duration>& when) const;

    void get();
    SharedFuture<void> share();

    const TaskContext* task_if() const;

    operator RequiredObject() const;

    template <class Callable, class... Args>
    auto then(Priority prio, Trackers&& trackers, Callable&& f, Args&&... args);

private:
    template <class U>
    friend struct detail::GenerateFuture;
    template <class U>
    friend class Future;
    template <class U>
    friend class Promise;
    template <class U>
    friend class SharedFuture;
    friend struct Tracker;

    TaskContext* ptask();

    detail::SharedState<void>* state() const noexcept;

    Future(TaskContext task);
    Future(detail::SharedState<void>* state);

    Object m_obj{ ObjectType::eNone, nullptr };
};
#endif

/**
 * SharedFuture is a shareable version of Future. Instead of Future::get() invalidating the Future and returning the
 * value one time, multiple SharedFuture objects can reference the same shared state and allow multiple threads to
 * wait and access the result value simultaneously.
 *
 * SharedFuture is similar to <a href="https://en.cppreference.com/w/cpp/thread/shared_future">std::shared_future</a>
 *
 * The same specializations (and their limitations) exist as with Future.
 */
template <class T = void>
class SharedFuture
{
public:
    /**
     * Default constructor. Constructs a SharedFuture where valid() == `false`.
     */
    SharedFuture() noexcept = default;

    /**
     * Copy constructor. Holds the same state (if any) as @p other.
     * @param other A SharedFuture to copy state from.
     */
    SharedFuture(const SharedFuture<T>& other) noexcept;

    /**
     * Move constructor. Moves the shared state (if any) from @p other.
     *
     * After this call, @p other will report valid() == `false`.
     * @param other A SharedFuture to move state from.
     */
    SharedFuture(SharedFuture<T>&& other) noexcept;

    /**
     * Transfers the shared state (if any) from @p fut.
     *
     * After construction, @p fut will report valid() == `false`.
     * @param fut A Future to move state from.
     */
    SharedFuture(Future<T>&& fut) noexcept;

    /**
     * Destructor.
     */
    ~SharedFuture();

    /**
     * Copy-assign operator. Holds the same state (if any) as @p other after releasing any shared state previously held.
     * @param other A SharedFuture to copy state from.
     * @returns `*this`
     */
    SharedFuture<T>& operator=(const SharedFuture<T>& other);

    /**
     * Move-assign operator. Swaps shared states with @p other.
     * @param other A SharedFuture to swap states with.
     * @returns `*this`
     */
    SharedFuture<T>& operator=(SharedFuture<T>&& other) noexcept;

    /**
     * Waits until the shared state is Ready and retrieves the value stored.
     * @warning Undefined behavior if valid() == `false`.
     * @returns A const reference to the stored value.
     */
    const T& get() const;

    /**
     * Checks if the SharedFuture references a shared state.
     *
     * This is only `true` for default-constructed SharedFuture or when moved from. Unlike Future, SharedFuture does not
     * invalidate once the value is read with Future::get().
     * @returns `true` if this SharedFuture references a shared state; `false` otherwise.
     */
    bool valid() const noexcept;

    /**
     * Checks to see if the shared state is Ready without waiting.
     *
     * @warning Undefined behavior to call this if valid() == `false`.
     * @returns `true` if the shared state is Ready; `false` otherwise.
     */
    bool try_wait() const;

    /**
     * Blocks the task or thread and waits for the shared state to become Ready. try_wait() == `true` after this call
     * and get() will immediately return a value.
     *
     * @warning Undefined behavior to call this if valid() == `false`.
     */
    void wait() const;

    /**
     * Blocks the task or thread until @p dur has elapsed or the shared state becomes Ready.
     *
     * If `true` is returned, get() will return a value immediately.
     * @warning Undefined behavior to call this if valid() == `false`.
     * @param dur The duration to wait for.
     * @returns `true` If the shared state is Ready; `false` if the timeout period elapsed.
     */
    template <class Rep, class Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& dur) const;

    /**
     * Blocks the task or thread until @p when is reached or the shared state becomes Ready.
     *
     * If `true` is returned, get() will return a value immediately.
     * @warning Undefined behavior to call this if valid() == `false`.
     * @param when The clock time to wait until.
     * @returns `true` If the shared state is Ready; `false` if the timeout period elapsed.
     */
    template <class Clock, class Duration>
    bool wait_until(const std::chrono::time_point<Clock, Duration>& when) const;

    /**
     * Returns whether the task promising a value to this Future has been canceled.
     *
     * @warning Undefined behavior to call this if valid() == `false`.
     * @note The `void` specialization of SharedFuture does not have this function.
     * @returns `true` if the task has been canceled or promise broken; `false` if the task is still pending, promise
     * not yet fulfilled, or has a valid value to read.
     */
    bool isCanceled() const;

    /**
     * Convertible to RequiredObject.
     */
    operator RequiredObject() const;

    /**
     * Returns a valid TaskContext if this SharedFuture represents a task.
     *
     * @note Futures can be returned from addTask() and related functions or from Promise::get_future(). Only Future
     * objects returned from addTask() and transferred to SharedFuture will return a valid pointer from task_if().
     *
     * @returns A pointer to a TaskContext if this SharedFuture was created from addTask() or related functions;
     * `nullptr` otherwise. The pointer is valid as long as the SharedFuture exists and the response from valid() would
     * be consistent.
     */
    const TaskContext* task_if() const;

    /**
     * Syntactic sugar around ITasking::addSubTask() that automatically passes the value from get() into `Callable`.
     * Unlike Future::then(), the SharedFuture is not reset to an invalid state.
     *
     * @note This can be used to "chain" tasks together.
     *
     * @warning If the dependent task is canceled then the sub-task will call `std::terminate()`. When canceling the
     * dependent task you must first cancel the sub-task.
     *
     * @param prio The priority of the task to execute.
     * @param trackers (optional) A `std::initializer_list` of zero or more Tracker objects. Note that this *must* be a
     * temporary object. The Tracker objects can be used to determine task completion or to provide input/output
     * parameters to the task system.
     * @param f A C++ "Callable" object (i.e. functor, lambda, [member] function ptr) that optionally returns a value.
     * The Callable object must take `const T&` as its last parameter.
     * @param args Arguments to pass to @p f
     * @return A Future based on the return type of @p f
     */
    template <class Callable, class... Args>
    auto then(Priority prio, Trackers&& trackers, Callable&& f, Args&&... args);

private:
    detail::SharedState<T>* m_state{ nullptr };
};

#ifndef DOXYGEN_BUILD
template <class T>
class SharedFuture<T&>
{
public:
    constexpr SharedFuture() noexcept = default;
    SharedFuture(const SharedFuture& other) noexcept;
    SharedFuture(SharedFuture&& other) noexcept;
    SharedFuture(Future<T&>&& fut) noexcept;
    ~SharedFuture();

    SharedFuture& operator=(const SharedFuture& other);
    SharedFuture& operator=(SharedFuture&& other) noexcept;

    T& get() const;
    bool valid() const noexcept;

    bool try_wait() const;
    void wait() const;
    template <class Rep, class Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& dur) const;
    template <class Clock, class Duration>
    bool wait_until(const std::chrono::time_point<Clock, Duration>& when) const;

    bool isCanceled() const;

    operator RequiredObject() const;

    const TaskContext* task_if() const;

    template <class Callable, class... Args>
    auto then(Priority prio, Trackers&& trackers, Callable&& f, Args&&... args);

private:
    detail::SharedState<T&>* m_state{ nullptr };
};

template <>
class SharedFuture<void>
{
public:
    constexpr SharedFuture() noexcept = default;
    SharedFuture(const SharedFuture<void>& other) noexcept;
    SharedFuture(SharedFuture<void>&& other) noexcept;
    SharedFuture(Future<void>&& fut) noexcept;
    ~SharedFuture();

    SharedFuture<void>& operator=(const SharedFuture<void>& other);
    SharedFuture<void>& operator=(SharedFuture<void>&& other) noexcept;

    void get() const;
    bool valid() const noexcept;

    bool try_wait() const;
    void wait() const;
    template <class Rep, class Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& dur) const;
    template <class Clock, class Duration>
    bool wait_until(const std::chrono::time_point<Clock, Duration>& when) const;
    operator RequiredObject() const;

    const TaskContext* task_if() const;

    template <class Callable, class... Args>
    auto then(Priority prio, Trackers&& trackers, Callable&& f, Args&&... args);

private:
    friend struct Tracker;
    TaskContext* ptask();
    detail::SharedState<void>* state() const;
    Object m_obj{ ObjectType::eNone, nullptr };
};
#endif

/**
 * A facility to store a value that is later acquired asynchronously via a Future created via Promise::get_future().
 *
 * The carb.tasking implementation is very similar to the C++11 <a
 * href="https://en.cppreference.com/w/cpp/thread/promise">std::promise</a>.
 *
 * A promise has a "shared state" that is shared with the Future that it creates through Promise::get_future().
 *
 * A promise is a single-use object. The get_future() function may only be called once, and either set_value() or
 * setCanceled() may only be called once.
 *
 * A promise that is destroyed without ever having called set_value() or setCanceled() is consider a broken promise and
 * automatically calls setCanceled().
 *
 * There are three specializations of Promise:
 * * Promise<T>: The base specialization, used to communicate objects between tasks/threads.
 * * Promise<T&>: Reference specialization, used to communicate references between tasks/threads.
 * * Promise<void>: Void specialization, used to communicate stateless events between tasks/threads.
 *
 * The `void` specialization of Promise is slightly different:
 * * Promise<void> does not have Promise::setCanceled(); cancellation state cannot be determined.
 */
template <class T = void>
class Promise
{
    CARB_PREVENT_COPY(Promise);

public:
    /**
     * Default constructor.
     *
     * Initializes the shared state.
     */
    Promise();

    /**
     * Can be move-constructed.
     */
    Promise(Promise&& other) noexcept;

    /**
     * Destructor.
     *
     * If the shared state has not yet received a value with set_value(), then it is canceled and made Ready similarly
     * to setCanceled().
     */
    ~Promise();

    /**
     * Can be move-assigned.
     */
    Promise& operator=(Promise&& other) noexcept;

    /**
     * Swaps the shared state with @p other's.
     *
     * @param other A Promise to swap shared states with.
     */
    void swap(Promise& other) noexcept;

    /**
     * Atomically retrieves and clears the Future from this Promise that shares the same state.
     *
     * A Future::wait() call will wait until the shared state becomes Ready.
     *
     * @warning `std::terminate()` will be called if this function is called more than once.
     *
     * @returns A Future with the same shared state as this Promise.
     */
    Future<T> get_future();

    /**
     * Atomically stores the value in the shared state and makes the state Ready.
     *
     * @warning Only one call of set_value() or setCanceled() is allowed. Subsequent calls will result in a call to
     * `std::terminate()`.
     *
     * @param value The value to atomically set into the shared state.
     */
    void set_value(const T& value);

    /**
     * Atomically stores the value in the shared state and makes the state Ready.
     *
     * @warning Only one call of set_value() or setCanceled() is allowed. Subsequent calls will result in a call to
     * `std::terminate()`.
     *
     * @param value The value to atomically set into the shared state.
     */
    void set_value(T&& value);

    /**
     * Atomically sets the shared state to canceled and makes the state Ready. This is a broken promise.
     *
     * @warning Calling Future::get() will result in a call to `std::terminate()`; Future::isCanceled() will return
     * `true`.
     */
    void setCanceled();

private:
    using State = detail::SharedState<T>;
    State* m_state{ nullptr };
};

#ifndef DOXYGEN_BUILD
template <class T>
class Promise<T&>
{
    CARB_PREVENT_COPY(Promise);

public:
    Promise();
    Promise(Promise&& other) noexcept;

    ~Promise();

    Promise& operator=(Promise&& other) noexcept;

    void swap(Promise& other) noexcept;

    Future<T&> get_future();

    void set_value(T& value);
    void setCanceled();

private:
    using State = detail::SharedState<T&>;
    State* m_state{ nullptr };
};

template <>
class Promise<void>
{
    CARB_PREVENT_COPY(Promise);

public:
    Promise();
    Promise(Promise&& other) noexcept;

    ~Promise();

    Promise& operator=(Promise&& other) noexcept;

    void swap(Promise& other) noexcept;

    Future<void> get_future();

    void set_value();

private:
    using State = detail::SharedState<void>;
    State* m_state{ nullptr };
};
#endif

} // namespace tasking
} // namespace carb
