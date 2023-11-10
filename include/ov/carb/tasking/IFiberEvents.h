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
//! @brief IFiberEvents definition file.
#pragma once

#include "../Framework.h"
#include "../Interface.h"

namespace carb
{
namespace tasking
{

/**
 * Defines the fiber events interface that receives fiber-related notifications.
 *
 * This is a \a reverse interface. It is not implemented by *carb.tasking.plugin*. Instead, *carb.tasking.plugin* looks
 * for all instances of this interface and will call the functions to inform other plugins of fiber events. This can be
 * used, for example, by a profiler that wants to keep track of which fiber is running on a thread.
 *
 * Once \ref IFiberEvents::notifyFiberStart() has been called, this is a signal to the receiver that a task is executing
 * on the current thread, and will be executing on the current thread until \ref IFiberEvents::notifyFiberStop() is
 * called on the same thread. Between these two calls, the thread is executing in *Task context*, that is, within a task
 * submitted to *carb.tasking.plugin*. As such, it is possible to query information about the task, such as the context
 * handle ( \ref ITasking::getTaskContext()) or access task-local storage ( \ref ITasking::getTaskStorage() /
 * \ref ITasking::setTaskStorage()). However, **anything that could cause a task to yield is strictly prohibited**
 * in these functions and will produce undefined behavior. This includes but is not limited to yielding, waiting on any
 * task-aware synchronization primitive (i.e. locking a \ref Mutex), sleeping in a task-aware manner, suspending a task,
 * etc.
 *
 * @warning *carb.tasking.plugin* queries for all IFiberEvents interfaces only during startup, during
 *  @ref ITasking::changeParameters() and @ref ITasking::reloadFiberEvents(). If a plugin is loaded which exports
 *  \c IFiberEvents then you **must** perform one of these methods to receive notifications about fiber events.
 *
 * @warning **DO NOT EVER** call the functions; only *carb.tasking.plugin* should be calling these functions. Receiving
 * one of these function calls implies that *carb.tasking.plugin* is loaded, and these function calls can be coordinated
 * with certain *carb.tasking.plugin* actions (reading task-specific data, for instance).
 *
 * @note Notification functions are called in the context of the thread which caused the fiber event.
 */
struct IFiberEvents
{
    CARB_PLUGIN_INTERFACE("carb::tasking::IFiberEvents", 1, 0)

    /**
     * Specifies that a fiber started or resumed execution on the calling thread.
     *
     * Specifies that the calling thread is now running fiber with ID @p fiberId until notifyFiberStop() is called on
     * the same thread.
     *
     * @note A thread switching fibers will always call notifyFiberStop() before calling notifyFiberStart() with the new
     * fiber ID.
     *
     * @param fiberId A unique identifier for a fiber.
     */
    void(CARB_ABI* notifyFiberStart)(const uint64_t fiberId);

    /**
     * Specifies that a fiber yielded execution on the calling thread. It may or may not restart again at some later
     * point, on the same thread or a different one.
     *
     * Specifies that the calling thread has yielded fiber with ID @p fiberId and is now running its own context.
     *
     * @param fiberId A unique identifier for a fiber.
     */
    void(CARB_ABI* notifyFiberStop)(const uint64_t fiberId);
};
} // namespace tasking
} // namespace carb
