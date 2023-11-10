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
//! @brief carb.profiler interface definition file.
#pragma once

#include "../Interface.h"

#include <atomic>
#include <cstdint>

namespace carb
{

//! Namespace for *carb.profiler* and related utilities.
namespace profiler
{

constexpr uint64_t kCaptureMaskNone = 0; ///< Captures no events, effectively disabling the profiler.
constexpr uint64_t kCaptureMaskAll = (uint64_t)-1; ///< Captures all events
constexpr uint64_t kCaptureMaskDefault = uint64_t(1); ///< If zero is provided to an event function, it becomes this.
constexpr uint64_t kCaptureMaskProfiler = uint64_t(1) << 63; ///< The mask used by the profiler for profiling itself.

/// A type representing a static string returned by IProfiler::registerStaticString().
using StaticStringType = size_t;

/// Returned as an error by IProfiler::registerStaticString() if the string could not be registered.
constexpr StaticStringType kInvalidStaticString = StaticStringType(0);

/// An opaque ID returned by IProfiler::beginStatic() / IProfiler::beginDynamic() that should be returned in
/// IProfiler::endEx() to validate that the zone was closed properly.
using ZoneId = size_t;

/// A marker that is returned IProfiler::beginStatic() / IProfiler::beginDynamic() on error and can be passed to
/// IProfiler::endEx() to prevent zone validation checking.
constexpr ZoneId kUnknownZoneId = 0;

/// A marker returned by IProfiler::beginStatic() / IProfiler::beginDynamic() to indicate that the zone
/// should be discarded, typically because it doesn't match the current capture mask.
constexpr ZoneId kNoZoneId = ZoneId(-1);

/// The type of flow event passed to IProfiler::emitFlowStatic() / IProfiler::emitFlowDynamic(). Typically used only by
/// profiler macros.
enum class FlowType : uint8_t
{
    Begin, ///< A flow begin point.
    End ///< A flow end point.
};

/// The type of instant event passed to IProfiler::emitInstantStatic() / IProfiler::emitInstantDynamic().
enum class InstantType : uint8_t
{
    Thread, ///< Draws a vertical line through the entire process.
    Process ///< Similar to a thread profile zone with zero duration.
};

/// ID for a GPU context created with IProfiler::createGpuContext
using GpuContextId = uint8_t;

/// Special value to indicate that a GPU context ID is invalid.
constexpr uint8_t kInvalidGpuContextId = (uint8_t)-1;


/// ID for a Lockable context created with IProfiler::createLockable
using LockableId = uint32_t;

/// Special value to indicate that a LockableId is invalid.
constexpr uint32_t kInvalidLockableId = (uint32_t)-1;

/// The type of lockable operation event
enum class LockableOperationType : uint8_t
{
    BeforeLock, ///< This notes on the timeline immediately before locking a non shared lock
    AfterLock, ///< This notes on the timeline immediately after locking a non shared lock
    AfterUnlock, ///< This notes on the timeline immediately after unlocking a non shared lock
    AfterSuccessfulTryLock, ///< This notes on the timeline immediately after successfully try locking a non shared lock
    BeforeLockShared, ///< This notes on the timeline immediately before locking a shared lock
    AfterLockShared, ///< This notes on the timeline immediately after locking a shared lock
    AfterUnlockShared, ///< This notes on the timeline immediately after unlocking a shared lock
    AfterSuccessfulTryLockShared, ///< This notes on the timeline immediately after successfully try locking a shared
                                  ///< lock
};

//! A callback used for \ref IProfiler::setMaskCallback(). Typically handled automatically by
//! \ref carb::profiler::registerProfilerForClient().
using MaskCallbackFn = void (*)(uint64_t);

/**
 * Defines the profiler system that is associated with the Framework.
 *
 * It is not recommended to use this interface directly, rather use macros provided in Profile.h, such as
 * @ref CARB_PROFILE_ZONE().
 *
 * Event names are specified as string which  can have formatting, which provides string behavior hints, but whether
 * to use those hints is up to the profiler backend implementation.
 */
struct IProfiler
{
    CARB_PLUGIN_INTERFACE("carb::profiler::IProfiler", 1, 4)

    /**
     * Starts up the profiler for use.
     */
    void(CARB_ABI* startup)();

    /**
     * Shuts down the profiler and cleans up resources.
     */
    void(CARB_ABI* shutdown)();

    /**
     * Set capture mask. Capture mask provides a way to filter out certain profiling events.
     * Condition (eventMask & captureMask) == eventMask is evaluated, and if true, event
     * is recorded. The default capture mask is kCaptureMaskAll
     *
     * @note Calling from multiple threads is not recommended as threads will overwrite each values. Calls to this
     * function should be serialized.
     *
     * @warning Changing the capture mask after the profiler has been started causes undefined behavior.
     *
     * @param mask Capture mask.
     * @returns the previous Capture mask.
     */
    uint64_t(CARB_ABI* setCaptureMask)(const uint64_t mask);

    /**
     * Gets the current capture mask
     *
     * @returns The current capture mask
     */
    uint64_t(CARB_ABI* getCaptureMask)();

    /**
     * Starts the profiling event. This event could be a fiber-based event (i.e. could yield and resume on
     * another thread) if tasking scheduler provides proper `startFiber`/`stopFiber` calls.
     *
     * @param mask Event capture mask.
     * @param function Static string (see @ref registerStaticString()) of the function where the profile zone is located
     * (usually `__func__`). If supportsDynamicSourceLocations(), this may be a `const char*` cast to @ref
     * StaticStringType.
     * @param file Static string (see @ref registerStaticString()) of the filename where the profile zone was started
     * (usually `__FILE__`). If supportsDynamicSourceLocations(), this may be a `const char*` cast to @ref
     * StaticStringType.
     * @param line Line number in the file where the profile zone was started (usually __LINE__).
     * @param nameFmt The event name format, followed by args. For beginStatic() this must be a \ref StaticStringType
     * from registerStaticString().
     * @returns An opaque ZoneId that should be passed to endEx().
     */
    ZoneId(CARB_ABI* beginStatic)(
        const uint64_t mask, StaticStringType function, StaticStringType file, int line, StaticStringType nameFmt);

    //! @copydoc IProfiler::beginStatic()
    ZoneId(CARB_ABI* beginDynamic)(
        const uint64_t mask, StaticStringType function, StaticStringType file, int line, const char* nameFmt, ...)
        CARB_PRINTF_FUNCTION(5, 6);

    /**
     * Stops the profiling event. This event could be a fiber-based event (i.e. could yield and resume on
     * another thread) if tasking scheduler provides proper `startFiber`/`stopFiber` calls.
     *
     * @warning This call is deprecated.  Please use endEx() instead.  This function will not be removed
     *          but should also not be called in new code.
     *
     * @param mask Event capture mask.
     */
    void(CARB_ABI* end)(const uint64_t mask);

    /**
     * Inserts a frame marker for the calling thread in the profiling output, for profilers that support frame markers.
     *
     * @note The name provided below must be the same for each set of frames, and called each time from the same thread.
     * For example you might have main thread frames that all are named "frame" and GPU frames that are named "GPU
     * frame". Some profilers (i.e. profiler-cpu to Tracy conversion) require that the name contain the word "frame."
     *
     * @param mask Deprecated and ignored for frame events.
     * @param nameFmt The frame set name format, followed by args. For frameStatic() this must be a
     * \ref StaticStringType from registerStaticString().
     */
    void(CARB_ABI* frameStatic)(const uint64_t mask, StaticStringType nameFmt);
    /// @copydoc frameStatic
    void(CARB_ABI* frameDynamic)(const uint64_t mask, const char* nameFmt, ...) CARB_PRINTF_FUNCTION(2, 3);

    /**
     * Send floating point value to the profiler.
     *
     * @param mask Value capture mask.
     * @param value Value.
     * @param valueFmt The value name format, followed by args. For valueFloatStatic() this must be a
     * \ref StaticStringType
     * from registerStaticString().
     */
    void(CARB_ABI* valueFloatStatic)(const uint64_t mask, float value, StaticStringType valueFmt);
    /// @copydoc valueFloatStatic
    void(CARB_ABI* valueFloatDynamic)(const uint64_t mask, float value, const char* valueFmt, ...)
        CARB_PRINTF_FUNCTION(3, 4);

    /**
     * Send signed integer value to the profiler.
     *
     * @param mask Value capture mask.
     * @param value Value.
     * @param valueFmt The value name format, followed by args. For valueIntStatic() this must be a
     * \ref StaticStringType from registerStaticString().
     */
    void(CARB_ABI* valueIntStatic)(const uint64_t mask, int32_t value, StaticStringType valueFmt);
    /// @copydoc valueIntStatic
    void(CARB_ABI* valueIntDynamic)(const uint64_t mask, int32_t value, const char* valueFmt, ...)
        CARB_PRINTF_FUNCTION(3, 4);

    /**
     * Send unsigned integer value to the profiler.
     *
     * @param mask Value capture mask.
     * @param value Value.
     * @param valueFmt The value name format, followed by args. For valueUIntStatic() this must be a \ref
     * StaticStringType from registerStaticString().
     */
    void(CARB_ABI* valueUIntStatic)(const uint64_t mask, uint32_t value, StaticStringType valueFmt);
    /// @copydoc valueUIntStatic
    void(CARB_ABI* valueUIntDynamic)(const uint64_t mask, uint32_t value, const char* valueFmt, ...)
        CARB_PRINTF_FUNCTION(3, 4);

    /**
     * Sets a threads name.
     *
     * @param tidOrZero The thread ID to name, or 0 to name the current thread.
     * @param nameFmt The thread name format, followed by args. For nameThreadStatic() this must be a
     * \ref StaticStringType from registerStaticString().
     */
    void(CARB_ABI* nameThreadStatic)(uint64_t tidOrZero, StaticStringType threadName);
    /// @copydoc nameThreadStatic
    void(CARB_ABI* nameThreadDynamic)(uint64_t tidOrZero, const char* threadName, ...) CARB_PRINTF_FUNCTION(2, 3);

    /**
     * Checks if the profiler supports dynamic source locations.
     *
     * Dynamic source locations allow the `file` and `func` parameters to functions such as beginStatic() and
     * beginDynamic() to be a transient non-literal string on the heap or stack.
     *
     * @returns `true` if dynamic source locations are supported; `false` if they are not supported.
     */
    bool(CARB_ABI* supportsDynamicSourceLocations)();

    /**
     * Helper functions to send a arbitrary type to the profiler.
     *
     * @tparam T The type of the parameter to send to the profiler.
     * @param mask Value capture mask.
     * @param value Value.
     * @param valueFmt The value name format, followed by args. For valueStatic() this must be a \ref StaticStringType
     * from registerStaticString().
     */
    template <typename T>
    void valueStatic(uint64_t mask, T value, StaticStringType valueFmt);

    /// @copydoc valueStatic
    /// @param args Additional arguments that correspond to printf-style format string @p valueFmt.
    template <typename T, typename... Args>
    void valueDynamic(uint64_t mask, T value, const char* valueFmt, Args&&... args);

    /**
     * Helper function for registering a static string.
     *
     * @note The profiler must copy all strings. By registering a static string, you are making a contract with the
     * profiler that the string at the provided address will never change. This allows the string to be passed by
     * pointer as an optimization without needing to copy the string.
     *
     * @note This function should be called only once per string. The return value should be captured in a variable and
     * passed to the static function such as beginStatic(), frameStatic(), valueStatic(), etc.
     *
     * @param string The static string to register. This must be a string literal or otherwise a string whose address
     * will never change.
     * @returns A \ref StaticStringType that represents the registered static string. If the string could not be
     * registered, kInvalidStaticString is returned.
     */
    StaticStringType(CARB_ABI* registerStaticString)(const char* string);

    /**
     * Send memory allocation event to the profiler for custom pools.
     *
     * @param mask Value capture mask.
     * @param ptr Memory address.
     * @param size Amount of bytes allocated.
     * @param name Static or formatted string which contains the name of the pool.
     */
    void(CARB_ABI* allocNamedStatic)(const uint64_t mask, const void* ptr, uint64_t size, StaticStringType name);
    /// @copydoc allocNamedStatic
    void(CARB_ABI* allocNamedDynamic)(const uint64_t mask, const void* ptr, uint64_t size, const char* nameFmt, ...)
        CARB_PRINTF_FUNCTION(4, 5);

    /**
     * Send memory free event to the profiler for custom pools.
     *
     * @param mask Value capture mask.
     * @param ptr Memory address.
     * @param name Static or formatted string which contains the name of the pool.
     */
    void(CARB_ABI* freeNamedStatic)(const uint64_t mask, const void* ptr, StaticStringType valueFmt);
    /// @copydoc freeNamedStatic
    void(CARB_ABI* freeNamedDynamic)(const uint64_t mask, const void* ptr, const char* nameFmt, ...)
        CARB_PRINTF_FUNCTION(3, 4);

    /**
     * Send memory allocation event to the profiler on the default pool.
     *
     * @param mask Value capture mask.
     * @param ptr Memory address.
     * @param size Amount of bytes allocated.
     */
    void(CARB_ABI* allocStatic)(const uint64_t mask, const void* ptr, uint64_t size);

    /**
     * Send memory free event to the profiler on the default pool.
     *
     * @param mask Value capture mask.
     * @param ptr Memory address.
     */
    void(CARB_ABI* freeStatic)(const uint64_t mask, const void* ptr);

    /**
     * Stops the profiling event that was initiated by beginStatic() or beginDynamic().
     *
     * @param mask Event capture mask.
     * @param zoneId The ZoneId returned from beginStatic() or beginDynamic().
     */
    void(CARB_ABI* endEx)(const uint64_t mask, ZoneId zoneId);

    /**
     * Records an instant event on a thread's timeline at the current time. Generally not used directly; instead use the
     * @ref CARB_PROFILE_EVENT() macro.
     *
     * @param mask Event capture mask.
     * @param function Static string (see @ref registerStaticString()) of the name of the function containing this event
     * (typically `__func__`). If supportsDynamicSourceLocations(), this may be a `const char*` cast to @ref
     * StaticStringType.
     * @param file Static string (see @ref registerStaticString()) of the name of the source file containing this event
     * (typically `__FILE__`). If supportsDynamicSourceLocations(), this may be a `const char*` cast to @ref
     * StaticStringType.
     * @param line The line number in @p file containing this event (typically `__LINE__`).
     * @param type The type of instant event.
     * @param nameFmt The name for the event.
     */
    void(CARB_ABI* emitInstantStatic)(const uint64_t mask,
                                      StaticStringType function,
                                      StaticStringType file,
                                      int line,
                                      InstantType type,
                                      StaticStringType nameFmt);
    /// @copydoc emitInstantStatic
    /// @note This function is slower than using emitInstanceStatic().
    /// @param ... `printf`-style varargs for @p nameFmt.
    void(CARB_ABI* emitInstantDynamic)(const uint64_t mask,
                                       StaticStringType function,
                                       StaticStringType file,
                                       int line,
                                       InstantType type,
                                       const char* nameFmt,
                                       ...) CARB_PRINTF_FUNCTION(6, 7);

    /**
     * Puts a flow event on the timeline at the current line. Generally not used directly; instead use the
     * @ref CARB_PROFILE_FLOW_BEGIN() and @ref CARB_PROFILE_FLOW_END() macros.
     *
     * Flow events draw an arrow from one point (the @ref FlowType::Begin location) to another point (the @ref
     * FlowType::End location). These two points can be in different threads but must have a matching @p id field. Only
     * the @ref FlowType::Begin event must specify a @p name. The @p id field is meant to be unique across profiler
     * runs, but may be reused as long as the @p name field matches across all @ref FlowType::Begin events and events
     * occur on the global timeline as @ref FlowType::Begin followed by @ref FlowType::End.
     *
     * A call with @ref FlowType::Begin will automatically insert an instant event on the current thread's timeline.
     *
     * @param mask Event capture mask.
     * @param function Static string (see @ref registerStaticString()) of the name of the function containing this
     * event. If supportsDynamicSourceLocations(), this may be a `const char*` cast to @ref StaticStringType.
     * @param file Static string (see @ref registerStaticString()) of the name of the source file containing this event
     * (typically `__FILE__`). If supportsDynamicSourceLocations(), this may be a `const char*` cast to @ref
     * StaticStringType.
     * @param line The line number in @p file containing this event (typically `__LINE__`).
     * @param type The type of flow marker.
     * @param id A unique identifier to tie `Begin` and `End` events together.
     * @param name The name for the flow event. Only required for @ref FlowType::Begin; if specified for
     *      @ref FlowType::End it must match exactly or be `nullptr`.
     */
    void(CARB_ABI* emitFlowStatic)(const uint64_t mask,
                                   StaticStringType function,
                                   StaticStringType file,
                                   int line,
                                   FlowType type,
                                   uint64_t id,
                                   StaticStringType name);
    /// @copydoc emitFlowStatic
    /// @note This function is slower than using emitFlowStatic().
    /// @param ... `printf`-style varargs for @p nameFmt.
    void(CARB_ABI* emitFlowDynamic)(const uint64_t mask,
                                    StaticStringType function,
                                    StaticStringType file,
                                    int line,
                                    FlowType type,
                                    uint64_t id,
                                    const char* name,
                                    ...) CARB_PRINTF_FUNCTION(7, 8);

    /**
     * Create a new GPU profiling context that allows injecting timestamps coming from a GPU in a deferred manner
     *
     * @param name name of the context
     * @param correlatedCpuTimestampNs correlated GPU clock timestamp (in nanoseconds)
     * @param correlatedGpuTimestamp correlated GPU clock timestamp (raw value)
     * @param gpuTimestampPeriodNs is the number of nanoseconds required for a GPU timestamp query to be incremented
     * by 1.
     * @param graphicApi string of graphic API used ['vulkan'/'d3d12']
     * @returns a valid ID or kInvalidGpuContextId if creation fails
     */
    GpuContextId(CARB_ABI* createGpuContext)(const char* name,
                                             int64_t correlatedCpuTimestampNs,
                                             int64_t correlatedGpuTimestamp,
                                             float gpuTimestampPeriodNs,
                                             const char* graphicApi);

    /**
     * Destroy a previously created GPU Context
     *
     * @param contextId id of the context, returned by createGpuContext
     */
    void(CARB_ABI* destroyGpuContext)(GpuContextId contextId);

    /**
     * Submit context calibration information that allows correlating CPU and GPU clocks
     *
     * @param contextId id of the context, returned by createGpuContext
     * @param correlatedCpuTimestampNs the new CPU timestamp at the time of correlation (in nanoseconds)
     * @param previousCorrelatedCpuTimestamp the CPU timestamp at the time of previous correlation (in nanoseconds)
     * @param correlatedGpuTimestamp the new raw GPU timestamp at the time of correlation
     */
    bool(CARB_ABI* calibrateGpuContext)(GpuContextId contextId,
                                        int64_t correlatedCpuTimestampNs,
                                        int64_t previousCorrelatedCpuTimestampNs,
                                        int64_t correlatedGpuTimestamp);
    /**
     * Record the beginning of a new GPU timestamp query
     *
     * @param mask Event capture mask.
     * @param function Static string (see @ref registerStaticString()) of the name of the function containing this event
     * (typically `__func__`). If supportsDynamicSourceLocations(), this may be a `const char*` cast to @ref
     * StaticStringType.
     * @param file Static string (see @ref registerStaticString()) of the name of the source file containing this event
     * (typically `__FILE__`). If supportsDynamicSourceLocations(), this may be a `const char*` cast to @ref
     * StaticStringType.
     * @param line The line number in @p file containing this event (typically `__LINE__`).
     * @param contextId the id of the context as returned by @ref createGpuContext
     * @param queryId unique query id (for identification when passing to setGpuQueryValue)
     * @param name The name for the event.
     */
    void(CARB_ABI* beginGpuQueryStatic)(const uint64_t mask,
                                        StaticStringType functionName,
                                        StaticStringType fileName,
                                        int line,
                                        GpuContextId contextId,
                                        uint32_t queryId,
                                        StaticStringType name);


    //! @copydoc IProfiler::beginGpuQueryStatic()
    void(CARB_ABI* beginGpuQueryDynamic)(const uint64_t mask,
                                         StaticStringType functionName,
                                         StaticStringType fileName,
                                         int line,
                                         GpuContextId contextId,
                                         uint32_t queryId,
                                         const char* nameFmt,
                                         ...) CARB_PRINTF_FUNCTION(7, 8);

    /**
     * Record the end of a new GPU timestamp query
     *
     * @param mask Event capture mask.
     * @param contextId the id of the context as returned by @ref createGpuContext
     * @param queryId unique query id (for identification when passing to setGpuQueryValue)
     */
    void(CARB_ABI* endGpuQuery)(const uint64_t mask, GpuContextId contextId, uint32_t queryId);
    /**
     * Set the value we've received from the GPU for a query (begin or end) we've issued in the past
     *
     * @param contextId the id of the context as returned by @ref createGpuContext
     * @param queryId unique query id specified at begin/end time
     * @param gpuTimestamp raw GPU timestamp value
     */
    void(CARB_ABI* setGpuQueryValue)(const uint64_t mask, GpuContextId contextId, uint32_t queryId, int64_t gpuTimestamp);

    /**
     * Create a lockable context which we can use to tag lock operation
     *
     * @param mask Event capture mask. If the mask does not match the current capture mask, the lockable is not created
     * and \ref kInvalidLockableId is returned.
     * @param name context name
     * @param isSharedLock if this shared for a shared lock
     * @param functionName Static string (see @ref registerStaticString()) of the name of the function containing this
     * event (typically `__func__`). If supportsDynamicSourceLocations(), this may be a `const char*` cast to @ref
     * StaticStringType.
     * @param fileName Static string (see @ref registerStaticString()) of the name of the source file containing this
     * event (typically `__FILE__`). If supportsDynamicSourceLocations(), this may be a `const char*` cast to @ref
     * StaticStringType.
     * @param line The line number in @p file containing this event (typically `__LINE__`).
     */
    LockableId(CARB_ABI* createLockable)(const uint64_t mask,
                                         const char* name,
                                         const bool isSharedLock,
                                         StaticStringType functionName,
                                         StaticStringType fileName,
                                         int line);

    /**
     * Destroy a lockable context
     *
     * @param lockableId the id of the lockable as returned by @ref createLockable
     */
    void(CARB_ABI* destroyLockable)(LockableId lockableId);

    /**
     * Record a lockable operation
     *
     * @param lockableId the id of the lockable as returned by @ref createLockable
     * @param operation which lock operation to tag
     */
    void(CARB_ABI* lockableOperation)(LockableId lockableId, LockableOperationType operation);

    /**
     * Used by \ref carb::profiler::registerProfilerForClient() and \ref carb::profiler::deregisterProfilerForClient()
     * to register a callback for keeping the profiler mask up to date.
     *
     * @param func The callback function to register.
     * @param enabled \c true to register the callback, \c false to unregister the callback.
     * @returns The current profiler mask.
     */
    uint64_t(CARB_ABI* setMaskCallback)(MaskCallbackFn func, bool enabled);
};

#ifndef DOXYGEN_BUILD
namespace detail
{
template <typename T>
class ValueInvoker;

template <>
class ValueInvoker<float>
{
public:
    static void invokeStatic(IProfiler& profiler, uint64_t mask, float value, StaticStringType valueFmt)
    {
        profiler.valueFloatStatic(mask, value, valueFmt);
    }
    template <typename... Args>
    static void invokeDynamic(IProfiler& profiler, uint64_t mask, float value, const char* valueFmt, Args&&... args)
    {
        profiler.valueFloatDynamic(mask, value, valueFmt, std::forward<Args>(args)...);
    }
};

template <>
class ValueInvoker<int32_t>
{
public:
    static void invokeStatic(IProfiler& profiler, uint64_t mask, int32_t value, StaticStringType valueFmt)
    {
        profiler.valueIntStatic(mask, value, valueFmt);
    }
    template <typename... Args>
    static void invokeDynamic(IProfiler& profiler, uint64_t mask, int32_t value, const char* valueFmt, Args&&... args)
    {
        profiler.valueIntDynamic(mask, value, valueFmt, std::forward<Args>(args)...);
    }
};

template <>
class ValueInvoker<uint32_t>
{
public:
    static void invokeStatic(IProfiler& profiler, uint64_t mask, uint32_t value, StaticStringType valueFmt)
    {
        profiler.valueUIntStatic(mask, value, valueFmt);
    }
    template <typename... Args>
    static void invokeDynamic(IProfiler& profiler, uint64_t mask, uint32_t value, const char* valueFmt, Args&&... args)
    {
        profiler.valueUIntDynamic(mask, value, valueFmt, std::forward<Args>(args)...);
    }
};
} // namespace detail
#endif

template <typename T>
inline void IProfiler::valueStatic(uint64_t mask, T value, StaticStringType valueFmt)
{
    using ValueInvoker = typename detail::ValueInvoker<T>;
    ValueInvoker::invokeStatic(*this, mask, value, valueFmt);
}

template <typename T, typename... Args>
inline void IProfiler::valueDynamic(uint64_t mask, T value, const char* valueFmt, Args&&... args)
{
    using ValueInvoker = typename detail::ValueInvoker<T>;
    ValueInvoker::invokeDynamic(*this, mask, value, valueFmt, std::forward<Args>(args)...);
}


} // namespace profiler
} // namespace carb

/**
 * Global pointer used to store the @ref carb::profiler::IProfiler interface.
 *
 * A copy of this pointer is stored in each Carbonite client (i.e. plugin/app). For applications, this pointer is
 * declared by @ref OMNI_APP_GLOBALS. For plugins, this pointer is declared by @ref CARB_PROFILER_GLOBALS via @ref
 * OMNI_MODULE_GLOBALS.
 *
 * This pointer is an implementation detail transparent to users.  However, a linker error pointing to this variable
 * usually indicates one of the `_GLOBALS` macros mentioned above were not called.
 */
CARB_WEAKLINK carb::profiler::IProfiler* g_carbProfiler;

/**
 * A global variable used as a cache for the result of \ref carb::profiler::IProfiler::getCaptureMask().
 *
 * \ref carb::profiler::registerProfilerForClient() will register a callback function with the profiler (if supported)
 * that will keep this variable updated. This variable can be checked inline before calling into the IProfiler
 * interface.
 */
CARB_WEAKLINK std::atomic_uint64_t g_carbProfilerMask;
