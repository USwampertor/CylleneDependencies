// Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Monitor interface for carb.profiler.
#pragma once

#include "../Defines.h"

#include "../Types.h"

namespace carb
{
namespace profiler
{

/**
 * A struct describing a specific profiling event.
 */
struct ProfileEvent
{
    //! A human-readable name for the event.
    const char* eventName;

    //! The thread ID that recorded this event. Comparable with `GetCurrentThreadID()` on Windows or `gettid()` on
    //! Linux.
    uint64_t threadId;

    //! The start timestamp for this event. Based on 10 nanoseconds units since IProfiler::startup() was called.
    uint64_t startTime;

    //! The total time in milliseconds elapsed for this event.
    float timeInMs;

    //! The stack depth for this event.
    uint16_t level;
};

//! An opaque pointer used by IProfileMonitor.
using ProfileEvents = struct ProfileEventsImpl*;

/**
 * Defines an interface to monitor profiling events.
 */
struct IProfileMonitor
{
    CARB_PLUGIN_INTERFACE("carb::profiler::IProfileMonitor", 1, 1)

    /**
     * Returns the profiled events for the previous frame (up to the previous \ref markFrameEnd() call).
     *
     * @returns an opaque pointer that refers to the previous frame's profiling information. This pointer must be
     * released with releaseLastProfileEvents() when the caller is finished with it.
     */
    ProfileEvents(CARB_ABI* getLastProfileEvents)();

    /**
     * Returns the number of profiling events for a ProfileEvents instance.
     *
     * @param events The ProfileEvents instance returned from getLastProfileEvents().
     * @returns The number of profiling events in the array returned by getLastProfileEventsData().
     */
    size_t(CARB_ABI* getLastProfileEventCount)(ProfileEvents events);

    /**
     * Returns an array of profiling events for a ProfileEvents instance.
     *
     * @param events The ProfileEvents instance returned from getLastProfileEvents().
     * @returns An array of ProfileEvent objects. The size of the array is returned by getLastProfileEventCount().
     */
    ProfileEvent*(CARB_ABI* getLastProfileEventsData)(ProfileEvents events);

    /**
     * Returns the number of thread IDs known to the ProfileEvents instance.
     *
     * @param events The ProfileEvents instance returned from getLastProfileEvents().
     * @returns The number of thread IDs in the array returned by getProfileThreadIds().
     */
    uint32_t(CARB_ABI* getProfileThreadCount)(ProfileEvents events);

    /**
     * Returns an array of thread IDs known to a ProfileEvents instance.
     *
     * @param events The ProfileEvents instance returned from getLastProfileEvents().
     * @returns An array of thread IDs. The size of the array is returned by getProfileThreadCount(). The thread IDs
     * are comparable with `GetCurrentThreadID()` on Windows and `gettid()` on Linux.
     */
    uint64_t const*(CARB_ABI* getProfileThreadIds)(ProfileEvents events);

    /**
     * Destroys a ProfileEvents instance.
     *
     * The data returned getLastProfileEventsData() and getProfileThreadIds() must not be referenced after this function
     * is called.
     *
     * @param events The ProfileEvents instance returned from getLastProfileEvents().
     */
    void(CARB_ABI* releaseLastProfileEvents)(ProfileEvents events);

    /**
     * Returns the thread ID that called \ref markFrameEnd() event.
     *
     * @param events The ProfileEvents instance returned from getLastProfileEvents().
     * @returns The thread ID that last called \ref markFrameEnd(). This thread ID is comparable with
     * `GetCurrentThreadID()` on Windows and `gettid()` on Linux.
     */
    uint64_t(CARB_ABI* getMainThreadId)(ProfileEvents events);

    /**
     * Marks the end of a frame's profile events.
     *
     * After this call, the previous frame's profile events are available via \ref getLastProfileEvents().
     */
    void(CARB_ABI* markFrameEnd)();
};

} // namespace profiler
} // namespace carb
