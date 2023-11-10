// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Provides an interface that handles various threading utility operations.
 */
#pragma once

#include "../Interface.h"


/** Namespace for all low level Carbonite functionality. */
namespace carb
{
/** Namespace for all threading operations. */
namespace thread
{

/** Base type for flags to the task relay system.  These are the fRelayFlag* flags defined below.
 *  These are to be passed to the framework's runRelayTask() function through its descriptor.
 */
using RelayFlags = uint64_t;

/** Flag to indicate that a relay task should block until the task completes.  When not used,
 *  the default behavior is to run the task as a fire-and-forget operation.  In this case, the
 *  task descriptor will be shallow copied.  It is the task's responsibility to clean up any
 *  resources used in the descriptor's @a context value before returning.  When this flag is
 *  used, the runRelayTask() call will block until the task completes.  In this case, either
 *  the task function itself or the caller can handle any resource clean up.
 *
 *  Note that using this flag will effectively cause the task queue to be flushed.  Any pending
 *  non-blocking calls will be completed before the new task is run.
 */
constexpr RelayFlags fRelayFlagBlocking = 0x8000000000000000ull;

/** Force the execution of the task even if a failure related to relaying the task occurs.
 *  This will effectively cause the task to run in the context of the thread that originally
 *  called Framework::runRelayTask() (and therefore imply the @ref fRelayFlagBlocking flag).
 */
constexpr RelayFlags fRelayFlagForce = 0x4000000000000000ull;

/** Flags available for use in the relay task itself.  These flags will be passed to the relay
 *  function unmodified.  The set bits in this mask may be used for any task-specific purpose.
 *  If any of the cleared bits in this mask are set in the task's flags (aside from the above
 *  defined flags), the runRelayTask() call will fail.
 */
constexpr RelayFlags fRelayAvailableFlagsMask = 0x0000ffffffffffffull;

/** Prototype for a relayed task function.
 *
 *  @param[in] flags        The flags controlling the behavior of this relay task.  This may
 *                          include either @ref fRelayFlagBlocking or any of the set bits in
 *                          @ref fRelayAvailableFlagsMask.  The bits in the mask may be used
 *                          for any purpose specific to the task function.
 *  @param[inout] context   An opaque value specific to this task.  This will be passed unmodified
 *                          to the task function from the task's descriptor when it is executed.
 *                          If the task function needs to return a value, it may be done through
 *                          this object.
 *  @returns No return value.
 *
 *  @remarks The prototype for a task function to be executed on the relayed task thread.  This
 *           function is expected to complete its task and return in a timely fashion.  This
 *           should never block or perform a task that has the possibility of blocking for an
 *           extended period of time.
 *
 *  @note Neither this function nor anything it calls may throw an exception unless the task
 *        function itself catches and handles that exception.  Throwing an exception that goes
 *        uncaught by the task function will result in the process being terminated.
 */
using RelayTaskFn = void(CARB_ABI*)(RelayFlags flags, void* context);

/** A descriptor of the relay task to be performed.  This provides the task function itself,
 *  the context value and flags to pass to it, and space to store the result for blocking
 *  task calls.
 *
 *  For non-blocking tasks, this descriptor will be shallow copied and queued for
 *  execution.  The caller must guarantee that any pointer parameters passed to the
 *  task function will remain valid until the task itself finishes execution.  In this
 *  case the task function itself is also responsible for cleaning up any resources that
 *  are passed to the task function through its context object.  As a result of this
 *  requirement, none of the values passed to the task should be allocated on the stack.
 *
 *  For blocking calls, the task will be guaranteed to be completed by the time
 *  runRelayTask() returns.  In this case, the result of the operation (if any) will be
 *  available in the task descriptor.  Any of the task's objects may be allocated on the
 *  stack if needed since it is guaranteed to block until the task completes.  Either the
 *  task or the caller may clean up any resources in this case.
 */
struct RelayTaskDesc
{
    /** The task function to be executed.  The task function itself is responsible for managing
     *  any thread safe access to any passed in values.
     */
    RelayTaskFn task;

    /** Flags that control the behavior of this task.  This may be a combination of zero or
     *  more of the @ref RelayFlags flags and zero or more task-specific flags.
     */
    RelayFlags flags;

    /** An opaque context value to be passed to the task function when it executes.  This will
     *  not be accessed or modified in any way before being passed to the task function.  The
     *  task function itself is responsible for knowing how to properly interpret this value.
     */
    void* context;
};

/** Possible result codes for Framework::runRelayTask(). */
enum class RelayResult
{
    /** The task was executed successfully. */
    eSuccess,

    /** A bad flag bit was used by the caller. */
    eBadParam,

    /** The task thread failed to launch. */
    eThreadFailure,

    /** The relay system has been shutdown on process exit and will not accept any new tasks. */
    eShutdown,

    /** The task was successfully run, but had to be forced to run on the calling thread due to
     *  the relayed task thread failing to launch.
     */
    eForced,

    /** Failed to allocate memory for a non-blocking task. */
    eNoMemory,
};

/** An interface to provide various thread utility operations.  Currently the only defined
 *  operation is to run a task in a single common thread regardless of the thread that requests
 *  the operation.
 */
struct IThreadUtil
{
    CARB_PLUGIN_INTERFACE("carb::thread::IThreadUtil", 1, 0)

    /** Relays a task to run on an internal worker thread.
     *
     *  @param[inout] desc  The descriptor of the task to be run.  This task includes a function
     *                      to execute and a context value to pass to that function.  This
     *                      descriptor must remain valid for the entire lifetime of the execution
     *                      of the task function.  If the @ref fRelayFlagBlocking flag is used,
     *                      this call will block until the task completes.  In this case, the
     *                      caller will be responsible for cleaning up any resources used or
     *                      returned by the task function.  If the flag is not used, the task
     *                      function itself will be responsible for cleaning up any resources
     *                      before it returns.
     *  @retval RelayResult::eSuccess if executing the task is successful.
     *  @returns An error code describing how the task relay failed.
     *
     *  @remarks This relays a task to run on an internal worker thread.  This worker thread will
     *           be guaranteed to continue running (in an efficient sleep state when not running
     *           a task) for the entire remaining lifetime of the process.  The thread will be
     *           terminated during shutdown of the process.
     *
     *  @remarks The intention of this function is to be able to run generic tasks on a worker
     *           thread that is guaranteed to live throughout the process's lifetime.  Other
     *           similar systems in other plugins have the possibility of being unloaded before
     *           the end of the process which would lead to the task worker thread(s) being
     *           stopped early.  Certain tasks such may require the instantiating thread to
     *           survive longer than the lifetime of a dynamic plugin.
     *
     *  @remarks A task function may queue another task when it executes.  However, the behavior
     *           may differ depending on whether the newly queued task is flagged as being
     *           blocking or non-blocking.  A recursive blocking task will be executed immediately
     *           in the context of the task thread, but will interrupt any other pending tasks
     *           that were queued ahead of it.  A recursive non-blocking task will always maintain
     *           queuing order however.  Note that recursive tasks may require extra care to
     *           wait for or halt upon module shutdown or unload.  In these cases, it is still
     *           the caller's responsibility to ensure all tasks queued by a module are safely
     *           and properly stopped before that module is unloaded.
     *
     *  @note This should only be used in situations where it is absolutely necessary.  In
     *        cases where performance or more flexibility are needed, other interfaces such as
     *        ITasking should be used instead.
     *
     *  @note If a caller in another module queues a non-blocking task, it is that caller's
     *        responsibility to ensure that task has completed before its module is unloaded.
     *        This can be accomplished by queuing a do-nothing blocking task.
     */
    RelayResult(CARB_ABI* runRelayTask)(RelayTaskDesc& desc);
};

} // namespace thread
} // namespace carb
