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
//! @brief carb.profiler macros and helpers
#pragma once

#include "../Defines.h"

#include "../Framework.h"
#include "../cpp20/Atomic.h"
#include "IProfiler.h"

#include <cstdarg>
#include <cstdio>
#include <tuple>

#include "ProfilerUtils.h"

#if CARB_PROFILING || defined(DOXYGEN_BUILD)

/**
 * @defgroup Profiler Helper Macros
 *
 * All of the following macros do nothing if @ref g_carbProfiler is `nullptr` (i.e.
 * carb::profiler::registerProfilerForClient() has not been called).
 * @{
 */

#    ifndef DOXYGEN_BUILD
// The following are helper macros for the profiler.
#        define CARB_PROFILE_IF(cond, true_case, false_case) CARB_PROFILE_IF_HELPER(cond, true_case, false_case)

// Note: CARB_PROFILE_HAS_VARARGS only supports up to 10 args now. If more are desired, increase the sequences below
// and add test cases to TestProfiler.cpp
// This trick is from https://stackoverflow.com/a/36015150/1450686
#        if CARB_COMPILER_MSC
#            define CARB_PROFILE_HAS_VARARGS(x, ...) CARB_PROFILE_EXPAND_ARGS(CARB_PROFILE_AUGMENT_ARGS(__VA_ARGS__))
#        elif CARB_COMPILER_GNUC
#            define CARB_PROFILE_HAS_VARARGS(...)                                                                      \
                CARB_PROFILE_ARGCHK_PRIVATE2(0, ##__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0)
#        else
#            error Unsupported Compiler!
#        endif

// The following are implementation helpers not intended to be used
#        define CARB_PROFILE_IF_HELPER(cond, true_case, false_case)                                                    \
            CARB_JOIN(CARB_PROFILE_IF_HELPER_, cond)(true_case, false_case)
#        define CARB_PROFILE_IF_HELPER_0(true_case, false_case) false_case
#        define CARB_PROFILE_IF_HELPER_1(true_case, false_case) true_case

#        define CARB_PROFILE_AUGMENT_ARGS(...) unused, __VA_ARGS__
#        define CARB_PROFILE_EXPAND_ARGS(...)                                                                          \
            CARB_PROFILE_EXPAND(CARB_PROFILE_ARGCHK_PRIVATE(__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0))
#        define CARB_PROFILE_EXPAND(x) x
#        define CARB_PROFILE_ARGCHK_PRIVATE(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, count, ...) count
#        define CARB_PROFILE_ARGCHK_PRIVATE2(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, count, ...) count

#        define CARB_PROFILE_UFUNCFILE(func)                                                                           \
            [](const char* pfunc) -> const auto&                                                                       \
            {                                                                                                          \
                static auto tup = ::std::make_tuple(                                                                   \
                    ::g_carbProfiler->registerStaticString(pfunc), ::g_carbProfiler->registerStaticString(__FILE__));  \
                return tup;                                                                                            \
            }                                                                                                          \
            (func)

#        define CARB_PROFILE_UFUNCFILESTR(func, str)                                                                   \
            [](const char* pfunc, const char* pstr) -> const auto&                                                     \
            {                                                                                                          \
                static auto tup = ::std::make_tuple(::g_carbProfiler->registerStaticString(pfunc),                     \
                                                    ::g_carbProfiler->registerStaticString(__FILE__),                  \
                                                    ::g_carbProfiler->registerStaticString(pstr));                     \
                return tup;                                                                                            \
            }                                                                                                          \
            (func, str)

#        define CARB_PROFILE_FUNCFILE(func)                                                                            \
            [](const char* pfunc) -> const auto&                                                                       \
            {                                                                                                          \
                if (::g_carbProfiler)                                                                                  \
                {                                                                                                      \
                    static auto tup = ::std::make_tuple(::g_carbProfiler->registerStaticString(pfunc),                 \
                                                        ::g_carbProfiler->registerStaticString(__FILE__));             \
                    return tup;                                                                                        \
                }                                                                                                      \
                return ::carb::profiler::detail::emptyTuple2();                                                        \
            }                                                                                                          \
            (func)

#        define CARB_PROFILE_FUNCFILESTR(func, str)                                                                    \
            [](const char* pfunc, const char* pstr) -> const auto&                                                     \
            {                                                                                                          \
                if (::g_carbProfiler)                                                                                  \
                {                                                                                                      \
                    static auto tup = ::std::make_tuple(::g_carbProfiler->registerStaticString(pfunc),                 \
                                                        ::g_carbProfiler->registerStaticString(__FILE__),              \
                                                        ::g_carbProfiler->registerStaticString(pstr));                 \
                    return tup;                                                                                        \
                }                                                                                                      \
                return ::carb::profiler::detail::emptyTuple3();                                                        \
            }                                                                                                          \
            (func, str)

#        define CARB_PROFILE_CHECKMASK(mask)                                                                           \
            (((mask) ? (mask) : carb::profiler::kCaptureMaskDefault) &                                                 \
             g_carbProfilerMask.load(std::memory_order_acquire))

namespace carb
{
namespace profiler
{
namespace detail
{
// Helper functions for begin that take the tuples created by CARB_PROFILE_UFUNCFILE and CARB_PROFILE_UFUNCFILESTR
template <class... Args>
carb::profiler::ZoneId beginDynamicHelper(
    const uint64_t mask,
    const std::tuple<carb::profiler::StaticStringType, carb::profiler::StaticStringType>& tup,
    int line,
    const char* fmt,
    Args&&... args)
{
    if (!CARB_PROFILE_CHECKMASK(mask))
        return kNoZoneId;
    return ::g_carbProfiler->beginDynamic(
        mask, std::get<0>(tup), std::get<1>(tup), line, fmt, std::forward<Args>(args)...);
}
template <class... Args>
carb::profiler::ZoneId beginDynamicHelper(
    const carb::profiler::Channel& channel,
    const std::tuple<carb::profiler::StaticStringType, carb::profiler::StaticStringType>& tup,
    int line,
    const char* fmt,
    Args&&... args)
{
    if (!channel.isEnabled())
        return kNoZoneId;
    return ::g_carbProfiler->beginDynamic(
        channel.getMask(), std::get<0>(tup), std::get<1>(tup), line, fmt, std::forward<Args>(args)...);
}
inline carb::profiler::ZoneId beginStaticHelper(
    const uint64_t mask,
    const std::tuple<carb::profiler::StaticStringType, carb::profiler::StaticStringType, carb::profiler::StaticStringType>& tup,
    int line)
{
    if (!CARB_PROFILE_CHECKMASK(mask))
        return kNoZoneId;
    return ::g_carbProfiler->beginStatic(mask, std::get<0>(tup), std::get<1>(tup), line, std::get<2>(tup));
}
inline carb::profiler::ZoneId beginStaticHelper(
    const carb::profiler::Channel& channel,
    const std::tuple<carb::profiler::StaticStringType, carb::profiler::StaticStringType, carb::profiler::StaticStringType>& tup,
    int line)
{
    if (!channel.isEnabled())
        return kNoZoneId;
    return ::g_carbProfiler->beginStatic(channel.getMask(), std::get<0>(tup), std::get<1>(tup), line, std::get<2>(tup));
}
inline uint64_t maskHelper(uint64_t mask)
{
    return mask;
}
inline uint64_t maskHelper(const carb::profiler::Channel& channel)
{
    return channel.getMask();
}
inline bool enabled(uint64_t mask)
{
    return CARB_PROFILE_CHECKMASK(mask);
}
inline bool enabled(const carb::profiler::Channel& channel)
{
    return channel.isEnabled();
}

inline const std::tuple<StaticStringType, StaticStringType>& emptyTuple2()
{
    static auto tup = std::make_tuple(kInvalidStaticString, kInvalidStaticString);
    return tup;
}

inline const std::tuple<StaticStringType, StaticStringType, StaticStringType>& emptyTuple3()
{
    static auto tup = std::make_tuple(kInvalidStaticString, kInvalidStaticString, kInvalidStaticString);
    return tup;
}

} // namespace detail
} // namespace profiler
} // namespace carb

#    endif

/**
 * Declares a channel that can be used with the profiler.
 *
 * Channels can be used in place of a mask for macros such as \ref CARB_PROFILE_ZONE. Channels allow enabling and
 * disabling at runtime, or based on a settings configuration.
 *
 * Channels must have static storage and module lifetime, therefore this macro should be used at file-level, class-level
 * or namespace-level scope only. Any other use is undefined behavior.
 *
 * Channels must be declared in exactly one compilation unit for a given module. References to the channel can be
 * accomplished with \ref CARB_PROFILE_EXTERN_CHANNEL for other compilation units that desire to reference the channel.
 *
 * Channel settings are located under `/profiler/channels/<name>` and may have the following values:
 *   - `enabled` - (bool) whether this channel is enabled (reports to the profiler) or not
 *   - `mask` - (uint64_t) the mask used with the profiler
 *
 * @param name_ A string name for this channel. This is used to look up settings keys for this channel.
 * @param defaultMask_ The profiler works with the concept of masks. The profiler must have the capture mask enabled for
 *      this channel to report to the profiler. A typical value for this could be
 *      \ref carb::profiler::kCaptureMaskDefault.
 * @param defaultEnabled_ Whether this channel is enabled to report to the profiler by default.
 * @param symbol_ The symbol name that code would refer to this channel by.
 */
#    define CARB_PROFILE_DECLARE_CHANNEL(name_, defaultMask_, defaultEnabled_, symbol_)                                \
        ::carb::profiler::Channel symbol_((defaultMask_), (defaultEnabled_), "" name_)

/**
 * References a channel declared in another compilation unit.
 *
 * @param symbol_ The symbol name given to the \ref CARB_PROFILE_DECLARE_CHANNEL
 */
#    define CARB_PROFILE_EXTERN_CHANNEL(symbol_) extern ::carb::profiler::Channel symbol_

/**
 * Starts the profiler that has been registered with carb::profiler::registerProfilerForClient().
 *
 * When finished with the profiler it should be stopped with CARB_PROFILE_SHUTDOWN().
 *
 * @note This is typically done immediately after carb::profiler::registerProfilerForClient().
 */
#    define CARB_PROFILE_STARTUP()                                                                                     \
        do                                                                                                             \
        {                                                                                                              \
            if (::g_carbProfiler)                                                                                      \
            {                                                                                                          \
                ::g_carbProfiler->startup();                                                                           \
            }                                                                                                          \
        } while (0)

/**
 * Shuts down the profiler that has been registered with carb::profiler::registerProfilerForClient() and previously
 * started with CARB_PROFILE_STARTUP().
 *
 * @note This is typically done immediately before carb::profiler::deregisterProfilerForClient().
 */
#    define CARB_PROFILE_SHUTDOWN()                                                                                    \
        do                                                                                                             \
        {                                                                                                              \
            if (::g_carbProfiler)                                                                                      \
            {                                                                                                          \
                ::g_carbProfiler->shutdown();                                                                          \
            }                                                                                                          \
        } while (0)

/**
 * Registers a static string for use with the profiler.
 *
 * The profiler works by capturing events very quickly in the thread of execution that they happen in, and then
 * processing them later in a background thread. Since static/literal strings are contained in memory that may be
 * invalid once the module unloads, these static/literal strings are registered and copied by the profiler and this
 * macro returns a handle to the string that can be passed to the "static" function such as
 * @ref carb::profiler::IProfiler::beginStatic().
 *
 * @note This macro is used by other helper macros and is typically not used by applications.
 *
 * @warning Undefined behavior occurs if the given string is not a literal or static string.
 *
 * @returns A handle to the static string registered with the profiler. There is no need to unregister this string.
 */
#    define CARB_PROFILE_REGISTER_STRING(str)                                                                          \
        [](const char* pstr) {                                                                                         \
            if (::g_carbProfiler)                                                                                      \
            {                                                                                                          \
                static ::carb::profiler::StaticStringType p = ::g_carbProfiler->registerStaticString(pstr);            \
                return p;                                                                                              \
            }                                                                                                          \
            return ::carb::profiler::kInvalidStaticString;                                                             \
        }(str)

/**
 * A helper to set the capture mask.
 *
 * The capture mask is a set of 64 bits. Each profiling zone is *bitwise-and*'d with the capture mask. If the operation
 * matches the profiling zone mask then the event is included in the profiling output. Otherwise, the event is ignored.
 *
 * The default capture mask is profiler-specific, but typically has all bits set (i.e. includes everything).
 * @see carb::profiler::IProfiler::setCaptureMask()
 *
 * @warning Changing the capture mask after the profiler has been started causes undefined behavior.
 */
#    define CARB_PROFILE_SET_CAPTURE_MASK(mask)                                                                        \
        do                                                                                                             \
        {                                                                                                              \
            if (::g_carbProfiler)                                                                                      \
            {                                                                                                          \
                ::g_carbProfiler->setCaptureMask(mask);                                                                \
            }                                                                                                          \
        } while (0)

/**
 * Marks the beginning of a profiling zone.
 *
 * To end the profiling zone, use CARB_PROFILE_END().
 *
 * @warning Consider using CARB_PROFILE_ZONE() to automatically profile a scope. Manual begin and end sections can cause
 * programming errors and confuse the profiler if an end is skipped.
 *
 * @param maskOrChannel The event mask (see carb::profiler::setCaptureMask()) or a channel symbol name.
 * @param eventName The name of the profiling zone. This must be either a literal string or a printf-style format
 * string. Literal strings are far more efficient.
 * @param ... Optional printf-style variadic arguments corresponding to format specifiers in @p eventName.
 * @returns A carb::profiler::ZoneId that is unique to this zone and should be passed to CARB_PROFILE_END().
 */
#    define CARB_PROFILE_BEGIN(maskOrChannel, eventName, ...)                                                          \
        ::g_carbProfiler ?                                                                                             \
            CARB_PROFILE_IF(CARB_PROFILE_HAS_VARARGS(eventName, ##__VA_ARGS__),                                        \
                            ::carb::profiler::detail::beginDynamicHelper(                                              \
                                maskOrChannel, CARB_PROFILE_UFUNCFILE(__func__), __LINE__, eventName, ##__VA_ARGS__),  \
                            ::carb::profiler::detail::beginStaticHelper(                                               \
                                maskOrChannel, CARB_PROFILE_UFUNCFILESTR(__func__, eventName), __LINE__)) :            \
            (0 ? /*compiler validate*/ printf(eventName, ##__VA_ARGS__) : 0)

/**
 * Marks the end of a profiling zone previously started with CARB_PROFILE_BEGIN().
 *
 * @warning Consider using CARB_PROFILE_ZONE() to automatically profile a scope. Manual begin and end sections can cause
 * programming errors and confuse the profiler if an end is skipped.
 *
 * @param maskOrChannel The event mask or a channel symbol. This should match the value passed to CARB_PROFILE_BEGIN().
 * @param ... The carb::profiler::ZoneId returned from CARB_PROFILE_BEGIN(), if known. This will help the profiler to
 *    validate that the proper zone was ended.
 */
#    define CARB_PROFILE_END(maskOrChannel, ...)                                                                       \
        do                                                                                                             \
        {                                                                                                              \
            if (::g_carbProfiler)                                                                                      \
            {                                                                                                          \
                ::g_carbProfiler->CARB_PROFILE_IF(                                                                     \
                    CARB_PROFILE_HAS_VARARGS(maskOrChannel, ##__VA_ARGS__),                                            \
                    endEx(::carb::profiler::detail::maskHelper(maskOrChannel), ##__VA_ARGS__),                         \
                    end(::carb::profiler::detail::maskHelper(maskOrChannel)));                                         \
            }                                                                                                          \
        } while (0)

/**
 * Inserts a frame marker for the calling thread in the profiling output, for profilers that support frame markers.
 *
 * @note The name provided below must be the same for each set of frames, and called each time from the same thread.
 * For example you might have main thread frames that all are named "frame" and GPU frames that are named "GPU
 * frame". Some profilers (i.e. profiler-cpu to Tracy conversion) require that the name contain the word "frame."
 *
 * @param mask Deprecated and ignored for frame events.
 * @param frameName A name for the frame. This must either be a literal string or a printf-style format string. Literal
 *    strings are far more efficient. See the note above about frame names.
 * @param ... Optional printf-style variadic arguments corresponding to format specifiers in @p frameName.
 */
#    define CARB_PROFILE_FRAME(mask, frameName, ...)                                                                   \
        do                                                                                                             \
        {                                                                                                              \
            /* Use printf to validate the format string */                                                             \
            if (0)                                                                                                     \
            {                                                                                                          \
                printf(frameName, ##__VA_ARGS__);                                                                      \
            }                                                                                                          \
            if (::g_carbProfiler)                                                                                      \
            {                                                                                                          \
                CARB_PROFILE_IF(CARB_PROFILE_HAS_VARARGS(frameName, ##__VA_ARGS__),                                    \
                                ::g_carbProfiler->frameDynamic(mask, frameName, ##__VA_ARGS__),                        \
                                ::g_carbProfiler->frameStatic(mask, []() {                                             \
                                    static ::carb::profiler::StaticStringType p =                                      \
                                        ::g_carbProfiler->registerStaticString("" frameName);                          \
                                    return p;                                                                          \
                                }()));                                                                                 \
            }                                                                                                          \
        } while (0)

/**
 * Creates a profiling zone over a scope.
 *
 * This macro creates a temporary object on the stack that automatically begins a profiling zone at the point where this
 * macro is used, and automatically ends the profiling zone when it goes out of scope.
 *
 * @param maskOrChannel The event mask (see carb::profiler::setCaptureMask()) or a channel symbol.
 * @param zoneName The name of the profiling zone. This must be either a literal string or a printf-style format string.
 * Literal strings are far more efficient.
 * @param ... Optional printf-style variadic arguments corresponding to format specifiers in @p zoneName.
 */
#    define CARB_PROFILE_ZONE(maskOrChannel, zoneName, ...)                                                            \
        CARB_PROFILE_IF(CARB_PROFILE_HAS_VARARGS(zoneName, ##__VA_ARGS__),                                             \
                        ::carb::profiler::ProfileZoneDynamic CARB_JOIN(_carbZone, __LINE__)(                           \
                            (maskOrChannel), CARB_PROFILE_FUNCFILE(__func__), __LINE__, zoneName, ##__VA_ARGS__),      \
                        ::carb::profiler::ProfileZoneStatic CARB_JOIN(_carbZone, __LINE__)(                            \
                            (maskOrChannel), CARB_PROFILE_FUNCFILESTR(__func__, zoneName), __LINE__))

/**
 * A helper for CARB_PROFILE_ZONE() that automatically uses the function name as from `CARB_PRETTY_FUNCTION`.
 *
 * Equivalent, but faster than: `CARB_PROFILE_ZONE(mask, "%s", CARB_PRETTY_FUNCTION)`.
 *
 * @param maskOrChannel The event mask (see carb::profiler::setCaptureMask()) or a profiling channel.
 */
#    define CARB_PROFILE_FUNCTION(maskOrChannel)                                                                       \
        ::carb::profiler::ProfileZoneStatic CARB_JOIN(_carbZoneFunction, __LINE__)(                                    \
            (maskOrChannel), CARB_PROFILE_FUNCFILESTR(__func__, CARB_PRETTY_FUNCTION), __LINE__)

/**
 * Writes a named numeric value to the profiling output for profilers that support them.
 *
 * @note Supported types for @p value are `float`, `uint32_t` and `int32_t`.
 *
 * @param value The value to record.
 * @param maskOrChannel The event mask (see carb::profiler::setCaptureMask()) or a profiling channel.
 * @param valueName The name of the value. This must be either a literal string or a printf-style format string. Literal
 *    strings are far more efficient.
 * @param ... Optional printf-style variadic arguments corresponding to format specifiers in @p valueName.
 */
#    define CARB_PROFILE_VALUE(value, maskOrChannel, valueName, ...)                                                   \
        do                                                                                                             \
        {                                                                                                              \
            /* Use printf to validate the format string */                                                             \
            if (0)                                                                                                     \
            {                                                                                                          \
                printf(valueName, ##__VA_ARGS__);                                                                      \
            }                                                                                                          \
            if (::g_carbProfiler && ::carb::profiler::detail::enabled(maskOrChannel))                                  \
            {                                                                                                          \
                CARB_PROFILE_IF(                                                                                       \
                    CARB_PROFILE_HAS_VARARGS(valueName, ##__VA_ARGS__),                                                \
                    ::g_carbProfiler->valueDynamic(                                                                    \
                        ::carb::profiler::detail::maskHelper(maskOrChannel), value, valueName, ##__VA_ARGS__),         \
                    ::g_carbProfiler->valueStatic(::carb::profiler::detail::maskHelper(maskOrChannel), value, []() {   \
                        static ::carb::profiler::StaticStringType p =                                                  \
                            ::g_carbProfiler->registerStaticString("" valueName);                                      \
                        return p;                                                                                      \
                    }()));                                                                                             \
            }                                                                                                          \
        } while (0)

/**
 * Records an allocation event for a named memory pool for profilers that support them.
 *
 * @param maskOrChannel The event mask (see carb::profiler::setCaptureMask()) or a profiling channel.
 * @param ptr The memory address that was allocated.
 * @param size The size of the memory region beginning at @p ptr.
 * @param poolName The name of the memory pool. This must be either a literal string or a printf-style format string.
 * Literal strings are far more efficient.
 * @param ... Optional printf-style variadic arguments corresponding to format specifiers in @p poolName.
 */
#    define CARB_PROFILE_ALLOC_NAMED(maskOrChannel, ptr, size, poolName, ...)                                            \
        do                                                                                                               \
        {                                                                                                                \
            /* Use printf to validate the format string */                                                               \
            if (0)                                                                                                       \
            {                                                                                                            \
                printf(poolName, ##__VA_ARGS__);                                                                         \
            }                                                                                                            \
            if (::g_carbProfiler && ::carb::profiler::detail::enabled(maskOrChannel))                                    \
            {                                                                                                            \
                CARB_PROFILE_IF(CARB_PROFILE_HAS_VARARGS(poolName, ##__VA_ARGS__),                                       \
                                ::g_carbProfiler->allocNamedDynamic(::carb::profiler::detail::maskHelper(maskOrChannel), \
                                                                    ptr, size, poolName, ##__VA_ARGS__),                 \
                                ::g_carbProfiler->allocNamedStatic(                                                      \
                                    ::carb::profiler::detail::maskHelper(maskOrChannel), ptr, size, []() {               \
                                        static ::carb::profiler::StaticStringType p =                                    \
                                            ::g_carbProfiler->registerStaticString("" poolName);                         \
                                        return p;                                                                        \
                                    }()));                                                                               \
            }                                                                                                            \
        } while (0)

/**
 * Records a free event for a named memory pool for profilers that support them.
 *
 * @param maskOrChannel The event mask (see carb::profiler::setCaptureMask()) or a profiling channel. This should match
 *      the value passed to CARB_PROFILE_ALLOC_NAMED() for the same allocation.
 * @param ptr The memory address that was freed.
 * @param poolName The name of the memory pool. This must be either a literal string or a printf-style format string.
 * Literal strings are far more efficient.
 * @param ... Optional printf-style variadic arguments corresponding to format specifiers in @p poolName.
 */
#    define CARB_PROFILE_FREE_NAMED(maskOrChannel, ptr, poolName, ...)                                                 \
        do                                                                                                             \
        {                                                                                                              \
            /* Use printf to validate the format string */                                                             \
            if (0)                                                                                                     \
            {                                                                                                          \
                printf(poolName, ##__VA_ARGS__);                                                                       \
            }                                                                                                          \
            if (::g_carbProfiler && ::carb::profiler::detail::enabled(maskOrChannel))                                  \
            {                                                                                                          \
                CARB_PROFILE_IF(                                                                                       \
                    CARB_PROFILE_HAS_VARARGS(poolName, ##__VA_ARGS__),                                                 \
                    ::g_carbProfiler->freeNamedDynamic(                                                                \
                        ::carb::profiler::detail::maskHelper(maskOrChannel), ptr, poolName, ##__VA_ARGS__),            \
                    ::g_carbProfiler->freeNamedStatic(::carb::profiler::detail::maskHelper(maskOrChannel), ptr, []() { \
                        static ::carb::profiler::StaticStringType p =                                                  \
                            ::g_carbProfiler->registerStaticString("" poolName);                                       \
                        return p;                                                                                      \
                    }()));                                                                                             \
            }                                                                                                          \
        } while (0)

/**
 * Records an allocation event for profilers that support them.
 *
 * @param maskOrChannel The event mask (see carb::profiler::setCaptureMask()) or a profiling channel.
 * @param ptr The memory address that was allocated.
 * @param size The size of the memory region beginning at @p ptr.
 */
#    define CARB_PROFILE_ALLOC(maskOrChannel, ptr, size)                                                               \
        do                                                                                                             \
        {                                                                                                              \
            if (::g_carbProfiler && ::carb::profiler::detail::enabled(maskOrChannel))                                  \
            {                                                                                                          \
                ::g_carbProfiler->allocStatic(::carb::profiler::detail::maskHelper(maskOrChannel), ptr, size);         \
            }                                                                                                          \
        } while (0)

/**
 * Records a free event for profilers that support them.
 *
 * @param maskOrChannel The event mask (see carb::profiler::setCaptureMask()) or a profiling channel.
 * @param ptr The memory address that was freed.
 */
#    define CARB_PROFILE_FREE(maskOrChannel, ptr)                                                                      \
        do                                                                                                             \
        {                                                                                                              \
            if (::g_carbProfiler && ::carb::profiler::detail::enabled(maskOrChannel))                                  \
            {                                                                                                          \
                ::g_carbProfiler->freeStatic(::carb::profiler::detail::maskHelper(maskOrChannel), ptr);                \
            }                                                                                                          \
        } while (0)

/**
 * Records the name of a thread.
 *
 * @param tidOrZero The thread ID that is being named. A value of `0` indicates the current thread. Not all profilers
 * support values other than `0`.
 * @param threadName The name of the thread. This must be either a literal string or a printf-style format string.
 * Literal strings are far more efficient.
 * @param ... Optional printf-style variadic arguments corresponding to format specifiers in @p threadName.
 */
#    define CARB_NAME_THREAD(tidOrZero, threadName, ...)                                                               \
        do                                                                                                             \
        {                                                                                                              \
            /* Use printf to validate the format string */                                                             \
            if (0)                                                                                                     \
            {                                                                                                          \
                printf((threadName), ##__VA_ARGS__);                                                                   \
            }                                                                                                          \
            if (::g_carbProfiler)                                                                                      \
            {                                                                                                          \
                CARB_PROFILE_IF(CARB_PROFILE_HAS_VARARGS(threadName, ##__VA_ARGS__),                                   \
                                ::g_carbProfiler->nameThreadDynamic((tidOrZero), (threadName), ##__VA_ARGS__),         \
                                ::g_carbProfiler->nameThreadStatic((tidOrZero), []() {                                 \
                                    static ::carb::profiler::StaticStringType p =                                      \
                                        ::g_carbProfiler->registerStaticString("" threadName);                         \
                                    return p;                                                                          \
                                }()));                                                                                 \
            }                                                                                                          \
        } while (0)

/**
 * Records an instant event on a thread's timeline at the current time.
 *
 * @param maskOrChannel The event mask (see carb::profiler::setCaptureMask()) or a profiling channel.
 * @param type The type of the instant event that will be passed to carb::profiler::emitInstantStatic() or
 *   carb::profiler::emitInstantDynamic().
 * @param name The name of the event. This must either be a literal string or a printf-style format string with variadic
 *   arguments. Literal strings are far more efficient.
 * @param ... Optional printf-style variadic arguments corresponding to format specifiers in @p name.
 */
#    define CARB_PROFILE_EVENT(maskOrChannel, type, name, ...)                                                         \
        do                                                                                                             \
        {                                                                                                              \
            if (0)                                                                                                     \
                printf((name), ##__VA_ARGS__);                                                                         \
            if (::g_carbProfiler && ::carb::profiler::detail::enabled(maskOrChannel))                                  \
            {                                                                                                          \
                CARB_PROFILE_IF(                                                                                       \
                    CARB_PROFILE_HAS_VARARGS(name, ##__VA_ARGS__),                                                     \
                    static auto tup = ::std::make_tuple(::g_carbProfiler->registerStaticString(CARB_PRETTY_FUNCTION),  \
                                                        ::g_carbProfiler->registerStaticString(__FILE__));             \
                    ::g_carbProfiler->emitInstantDynamic(::carb::profiler::detail::maskHelper(maskOrChannel),          \
                                                         ::std::get<0>(tup), ::std::get<1>(tup), __LINE__, (type),     \
                                                         (name), ##__VA_ARGS__),                                       \
                    static auto tup = ::std::make_tuple(::g_carbProfiler->registerStaticString(CARB_PRETTY_FUNCTION),  \
                                                        ::g_carbProfiler->registerStaticString(__FILE__),              \
                                                        ::g_carbProfiler->registerStaticString(name));                 \
                    ::g_carbProfiler->emitInstantStatic(::carb::profiler::detail::maskHelper(maskOrChannel),           \
                                                        ::std::get<0>(tup), ::std::get<1>(tup), __LINE__, (type),      \
                                                        ::std::get<2>(tup)));                                          \
            }                                                                                                          \
        } while (0)

/**
 * Records the beginning of a flow event on the timeline at the current time for the current thread.
 *
 * Flow events draw an arrow from one point (the `BEGIN` location) to another point (the `END` location).
 * These two points can be in different threads but must have a matching @p id field. The @p id field is meant to be
 * unique across profiler runs, but may be reused as long as the @p name field matches across all `BEGIN` events and
 * events occur on the global timeline as `BEGIN` followed by `END`.
 *
 * This macro will automatically insert an instant event on the current thread's timeline.
 *
 * @param maskOrChannel The event mask (see carb::profiler::setCaptureMask()) or a profiling channel.
 * @param id A unique identifier that must also be passed to CARB_PROFILE_FLOW_END().
 * @param name The name of the event. This must either be a literal string or a printf-style format string with variadic
 *   arguments. Literal strings are far more efficient.
 * @param ... Optional printf-style variadic arguments corresponding to format specifiers in @p name.
 */
#    define CARB_PROFILE_FLOW_BEGIN(maskOrChannel, id, name, ...)                                                      \
        do                                                                                                             \
        {                                                                                                              \
            if (0)                                                                                                     \
                printf((name), ##__VA_ARGS__);                                                                         \
            if (::g_carbProfiler && ::carb::profiler::detail::enabled(maskOrChannel))                                  \
            {                                                                                                          \
                CARB_PROFILE_IF(                                                                                       \
                    CARB_PROFILE_HAS_VARARGS(name, ##__VA_ARGS__),                                                     \
                    static auto tup = ::std::make_tuple(::g_carbProfiler->registerStaticString(CARB_PRETTY_FUNCTION),  \
                                                        ::g_carbProfiler->registerStaticString(__FILE__));             \
                    ::g_carbProfiler->emitFlowDynamic(::carb::profiler::detail::maskHelper(maskOrChannel),             \
                                                      ::std::get<0>(tup), ::std::get<1>(tup), __LINE__,                \
                                                      ::carb::profiler::FlowType::Begin, (id), (name), ##__VA_ARGS__), \
                    static auto tup = ::std::make_tuple(::g_carbProfiler->registerStaticString(CARB_PRETTY_FUNCTION),  \
                                                        ::g_carbProfiler->registerStaticString(__FILE__),              \
                                                        ::g_carbProfiler->registerStaticString("" name));              \
                    ::g_carbProfiler->emitFlowStatic(::carb::profiler::detail::maskHelper(maskOrChannel),              \
                                                     ::std::get<0>(tup), ::std::get<1>(tup), __LINE__,                 \
                                                     ::carb::profiler::FlowType::Begin, (id), ::std::get<2>(tup)));    \
            }                                                                                                          \
        } while (0)

/**
 * Records the end of a flow event on the timeline at the current time for the current thread.
 *
 * @see CARB_PROFILE_FLOW_BEGIN()
 *
 * @param maskOrChannel The event mask or profiling channel. Must match the value given to CARB_PROFILE_FLOW_BEGIN().
 * @param id Th unique identifier passed to CARB_PROFILE_FLOW_BEGIN().
 */
#    define CARB_PROFILE_FLOW_END(maskOrChannel, id)                                                                   \
        do                                                                                                             \
        {                                                                                                              \
            if (::g_carbProfiler && ::carb::profiler::detail::enabled(maskOrChannel))                                  \
            {                                                                                                          \
                static auto tup = ::std::make_tuple(::g_carbProfiler->registerStaticString(CARB_PRETTY_FUNCTION),      \
                                                    ::g_carbProfiler->registerStaticString(__FILE__));                 \
                ::g_carbProfiler->emitFlowStatic(                                                                      \
                    ::carb::profiler::detail::maskHelper(maskOrChannel), ::std::get<0>(tup), ::std::get<1>(tup),       \
                    __LINE__, ::carb::profiler::FlowType::End, (id), ::carb::profiler::kInvalidStaticString);          \
            }                                                                                                          \
        } while (0)


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
#    define CARB_PROFILE_CREATE_GPU_CONTEXT(                                                                           \
        name, correlatedCpuTimestampNs, correlatedGpuTimestamp, gpuTimestampPeriodNs, graphicApi)                      \
        (::g_carbProfiler ? ::g_carbProfiler->createGpuContext(name, correlatedCpuTimestampNs, correlatedGpuTimestamp, \
                                                               gpuTimestampPeriodNs, graphicApi) :                     \
                            carb::profiler::kInvalidGpuContextId)

/**
 * Destroy a previously created GPU Context
 *
 * @param contextId ID of the context, returned by createGpuContext
 */
#    define CARB_PROFILE_DESTROY_GPU_CONTEXT(contextId)                                                                \
        do                                                                                                             \
        {                                                                                                              \
            if (::g_carbProfiler)                                                                                      \
            {                                                                                                          \
                ::g_carbProfiler->destroyGpuContext(contextId);                                                        \
            }                                                                                                          \
        } while (0)

/**
 * Submit context calibration information that allows correlating CPU and GPU clocks
 *
 * @param contextId ID of the context, returned by @ref carb::profiler::IProfiler::createGpuContext()
 * @param correlatedCpuTimestampNs The new CPU timestamp at the time of correlation (in nanoseconds)
 * @param previousCorrelatedCpuTimestampNs The CPU timestamp at the time of previous correlation (in nanoseconds)
 * @param correlatedGpuTimestamp The new raw GPU timestamp at the time of correlation
 */
#    define CARB_PROFILE_CALIBRATE_GPU_CONTEXT(                                                                        \
        contextId, correlatedCpuTimestampNs, previousCorrelatedCpuTimestampNs, correlatedGpuTimestamp)                 \
        ((::g_carbProfiler) ?                                                                                          \
             (::g_carbProfiler->calibrateGpuContext(                                                                   \
                 contextId, correlatedCpuTimestampNs, previousCorrelatedCpuTimestampNs, correlatedGpuTimestamp)) :     \
             false)


/**
 * Record the beginning of a new GPU timestamp query
 *
 * @param maskOrChannel Event capture mask or profiling channel.
 * @param contextId The id of the context as returned by @ref carb::profiler::IProfiler::createGpuContext()
 * @param queryId Unique query id (for identification when passing to @ref
 *   carb::profiler::IProfiler::setGpuQueryValue())
 * @param eventName The name for the event.
 */
#    define CARB_PROFILE_GPU_QUERY_BEGIN(maskOrChannel, contextId, queryId, eventName, ...)                            \
        do                                                                                                             \
        {                                                                                                              \
            if (0)                                                                                                     \
                printf((eventName), ##__VA_ARGS__);                                                                    \
            if (::g_carbProfiler && ::carb::profiler::detail::enabled(maskOrChannel))                                  \
            {                                                                                                          \
                static auto tup = ::std::make_tuple(::g_carbProfiler->registerStaticString(CARB_PRETTY_FUNCTION),      \
                                                    ::g_carbProfiler->registerStaticString(__FILE__));                 \
                CARB_PROFILE_IF(                                                                                       \
                    CARB_PROFILE_HAS_VARARGS(eventName, ##__VA_ARGS__),                                                \
                    ::g_carbProfiler->beginGpuQueryDynamic(::carb::profiler::detail::maskHelper(maskOrChannel),        \
                                                           ::std::get<0>(tup), ::std::get<1>(tup), __LINE__,           \
                                                           contextId, queryId, eventName, ##__VA_ARGS__),              \
                    ::g_carbProfiler->beginGpuQueryStatic(::carb::profiler::detail::maskHelper(maskOrChannel),         \
                                                          ::std::get<0>(tup), ::std::get<1>(tup), __LINE__, contextId, \
                                                          queryId, CARB_PROFILE_REGISTER_STRING("" eventName)));       \
            }                                                                                                          \
        } while (0)

/**
 * Record the end of a new GPU timestamp query
 *
 * @param maskOrChannel Event capture mask or profiling channel.
 * @param contextId The id of the context as returned by @ref carb::profiler::IProfiler::createGpuContext()
 * @param queryId Unique query id (for identification when passing to @ref
 *   carb::profiler::IProfiler::setGpuQueryValue())
 */
#    define CARB_PROFILE_GPU_QUERY_END(maskOrChannel, contextId, queryId)                                               \
        do                                                                                                              \
        {                                                                                                               \
            if (::g_carbProfiler && ::carb::profiler::detail::enabled(maskOrChannel))                                   \
            {                                                                                                           \
                ::g_carbProfiler->endGpuQuery(::carb::profiler::detail::maskHelper(maskOrChannel), contextId, queryId); \
            }                                                                                                           \
        } while (0)


/**
 * Set the value we've received from the GPU for a query (begin or end) we've issued in the past
 *
 * @param maskOrChannel Event capture mask or profiling channel
 * @param contextId The id of the context as returned by @ref carb::profiler::IProfiler::createGpuContext()
 * @param queryId Unique query id specified at begin/end time
 * @param gpuTimestamp Raw GPU timestamp value
 */
#    define CARB_PROFILE_GPU_SET_QUERY_VALUE(maskOrChannel, contextId, queryId, gpuTimestamp)                          \
        do                                                                                                             \
        {                                                                                                              \
            if (::g_carbProfiler && ::carb::profiler::detail::enabled(maskOrChannel))                                  \
            {                                                                                                          \
                ::g_carbProfiler->setGpuQueryValue(                                                                    \
                    ::carb::profiler::detail::maskHelper(maskOrChannel), contextId, queryId, gpuTimestamp);            \
            }                                                                                                          \
        } while (0)


/**
 *  Create a lockable context which we can use to tag lock operation
 * @note Do not use this macro directly. Use \ref carb::profiler::ProfiledMutex or
 * \ref carb::profiler::ProfiledSharedMutex instead.
 * @param maskOrChannel Event capture mask or profiling channel
 * @param isSharedLock If this shared for a shared lock
 * @param name The lockable context name
 */
#    define CARB_PROFILE_LOCKABLE_CREATE(maskOrChannel, isSharedLock, name)                                            \
        [](bool enabled, const uint64_t maskParam, const bool isSharedLockParam, const char* nameParam,                \
           const char* function) {                                                                                     \
            if (::g_carbProfiler && enabled)                                                                           \
            {                                                                                                          \
                static auto tup = ::std::make_tuple(::g_carbProfiler->registerStaticString(function),                  \
                                                    ::g_carbProfiler->registerStaticString(__FILE__));                 \
                return ::g_carbProfiler->createLockable(                                                               \
                    maskParam, nameParam, isSharedLockParam, ::std::get<0>(tup), ::std::get<1>(tup), __LINE__);        \
            }                                                                                                          \
            return ::carb::profiler::kInvalidLockableId;                                                               \
        }(::carb::profiler::detail::enabled(maskOrChannel), ::carb::profiler::detail::maskHelper(maskOrChannel),       \
          (isSharedLock), (name), CARB_PRETTY_FUNCTION)


/**
 * Destroy a lockable context
 * @note Do not use this macro directly. Use \ref carb::profiler::ProfiledMutex or
 * \ref carb::profiler::ProfiledSharedMutex instead.
 * @param lockableId the id of the lockable as returned by @ref carb::profiler::IProfiler::createLockable()
 */
#    define CARB_PROFILE_LOCKABLE_DESTROY(lockableId)                                                                  \
        do                                                                                                             \
        {                                                                                                              \
            if (::g_carbProfiler && lockableId != carb::profiler::kInvalidLockableId)                                  \
            {                                                                                                          \
                ::g_carbProfiler->destroyLockable((lockableId));                                                       \
            }                                                                                                          \
        } while (0)

/**
 * Records a lockable operation on a thread's timeline at the current time.
 * @note Do not use this macro directly. Use \ref carb::profiler::ProfiledMutex or
 * \ref carb::profiler::ProfiledSharedMutex instead.
 * @param lockableId the id of the lockable as returned by @ref carb::profiler::IProfiler::createLockable()
 * @param operation which lock operation to tag
 */
#    define CARB_PROFILE_LOCKABLE_OPERATION(lockableId, operation)                                                     \
        do                                                                                                             \
        {                                                                                                              \
            if (::g_carbProfiler && lockableId != carb::profiler::kInvalidLockableId)                                  \
            {                                                                                                          \
                ::g_carbProfiler->lockableOperation((lockableId), (operation));                                        \
            }                                                                                                          \
        } while (0)

/// @}
#else

#    define CARB_PROFILE_STARTUP() (void(0))
#    define CARB_PROFILE_SHUTDOWN() (void(0))
#    define CARB_PROFILE_REGISTER_STRING(str) (::carb::profiler::kInvalidStaticString)
#    define CARB_PROFILE_SET_CAPTURE_MASK(mask) (void(0))
#    define CARB_PROFILE_BEGIN(mask, eventName, ...) (void(0))
#    define CARB_PROFILE_END(mask) (void(0))
#    define CARB_PROFILE_FRAME(mask, frameName, ...) (void(0))
#    define CARB_PROFILE_ZONE(mask, zoneName, ...) (void(0))
#    define CARB_PROFILE_FUNCTION(mask) (void(0))
#    define CARB_PROFILE_VALUE(value, mask, valueName, ...) (void(0))
#    define CARB_NAME_THREAD(tidOrZero, threadName, ...) (void(0))
#    define CARB_PROFILE_CREATE_GPU_CONTEXT(                                                                           \
        name, correlatedCpuTimestampNs, correlatedGpuTimestamp, gpuTimestampPeriodNs, graphicApi)                      \
        (carb::profiler::kInvalidGpuContextId)
#    define CARB_PROFILE_DESTROY_GPU_CONTEXT(contextId) (void(0))
#    define CARB_PROFILE_CALIBRATE_GPU_CONTEXT(                                                                        \
        contextId, correlatedCpuTimestampNs, previousCorrelatedCpuTimestampNs, correlatedGpuTimestamp)                 \
        (void(0))
#    define CARB_PROFILE_GPU_QUERY_BEGIN(mask, queryId, eventName) (void(0))
#    define CARB_PROFILE_GPU_QUERY_END(mask, queryId) (void(0))
#    define CARB_PROFILE_GPU_SET_QUERY_VALUE(mask, contextId, queryId, gpuTimestamp) (void(0))
#    define CARB_PROFILE_LOCKABLE_CREATE(mask, name, isShared)
#    define CARB_PROFILE_LOCKABLE_DESTROY(mask, lockableId)
#    define CARB_PROFILE_LOCKABLE_OPERATION(mask, lockableId, operation)

#endif

/**
 * Placeholder macro for any work that needs to be done at the global scope for the profiler.
 * @note This is typically not used as it is included in the @ref CARB_GLOBALS_EX macro.
 */
#define CARB_PROFILER_GLOBALS()

namespace carb
{
namespace profiler
{

/**
 * Wrapper to add automatic profiling to a mutex
 */
template <class Mutex>
class ProfiledMutex
{
public:
    /**
     * Constructor.
     * @param profileMask The mask used to determine if events from this mutex are captured.
     * @param name The name of the mutex
     */
    ProfiledMutex(const uint64_t profileMask, const char* name) : ProfiledMutex(profileMask, false, name)
    {
    }

    /**
     * Constructor.
     * @param channel The profiling channel used to determine if events from this mutex are captured.
     * @param name The name of the mutex
     */
    ProfiledMutex(const carb::profiler::Channel& channel, const char* name) : ProfiledMutex(channel, false, name)
    {
    }

    /**
     * Destructor.
     */
    ~ProfiledMutex()
    {
        CARB_PROFILE_LOCKABLE_DESTROY(m_lockableId);
    }

    /**
     * Locks the underlying mutex and reports the event to the profiler.
     */
    void lock()
    {
        CARB_PROFILE_LOCKABLE_OPERATION(m_lockableId, LockableOperationType::BeforeLock);
        m_mutex.lock();
        CARB_PROFILE_LOCKABLE_OPERATION(m_lockableId, LockableOperationType::AfterLock);
    }

    /**
     * Attempts a lock on the underlying mutex and reports the event to the profiler if successful.
     * @returns \c true if successfully locked; \c false otherwise.
     */
    bool try_lock()
    {
        bool acquired = m_mutex.try_lock();
        if (acquired)
        {
            CARB_PROFILE_LOCKABLE_OPERATION(m_lockableId, LockableOperationType::AfterSuccessfulTryLock);
        }
        return acquired;
    }

    /**
     * Unlocks the underlying mutex and reports the event to the profiler.
     */
    void unlock()
    {
        m_mutex.unlock();
        CARB_PROFILE_LOCKABLE_OPERATION(m_lockableId, LockableOperationType::AfterUnlock);
    }

    /**
     * Returns a reference to the underlying mutex.
     * @returns a reference to the underlying mutex.
     */
    Mutex& getMutex()
    {
        return m_mutex;
    }

    /**
     * Returns a reference to the underlying mutex.
     * @returns a reference to the underlying mutex.
     */
    const Mutex& getMutex() const
    {
        return m_mutex;
    }

protected:
    /**
     * Protected Constructor.
     * @param profileMask The mask used to determine if events from this mutex are captured.
     * @param isSharedMutex A boolean representing whether `*this` represents a shared mutex.
     * @param name The name of the mutex
     */
    ProfiledMutex(const uint64_t profileMask, bool isSharedMutex, const char* name)
    {
        m_lockableId = CARB_PROFILE_LOCKABLE_CREATE(profileMask, isSharedMutex, name);
    }
    /**
     * Protected Constructor.
     * @param channel The channel used to determine if events from this mutex are captured.
     * @param isSharedMutex A boolean representing whether `*this` represents a shared mutex.
     * @param name The name of the mutex
     */
    ProfiledMutex(const carb::profiler::Channel& channel, bool isSharedMutex, const char* name)
    {
        m_lockableId = CARB_PROFILE_LOCKABLE_CREATE(channel, isSharedMutex, name);
    }

    //! The underlying mutex instance
    Mutex m_mutex;
    //! The lockable ID as returned by \ref carb::profiler::IProfiler::createLockable()
    LockableId m_lockableId;
};

/**
 * Wrapper to add automatic profiling to a shared mutex
 */
template <class Mutex>
class ProfiledSharedMutex : public ProfiledMutex<Mutex>
{
    using Base = ProfiledMutex<Mutex>;

public:
    /**
     * Constructor.
     * @param profileMask The mask used to determine if events from this mutex are captured.
     * @param name The name of the mutex
     */
    ProfiledSharedMutex(const uint64_t profileMask, const char* name) : Base(profileMask, true, name)
    {
    }

    /**
     * Constructor.
     * @param channel The profiling channel used to determine if events from this mutex are captured.
     * @param name The name of the mutex
     */
    ProfiledSharedMutex(const carb::profiler::Channel& channel, const char* name) : Base(channel, true, name)
    {
    }

    /**
     * Destructor.
     */
    ~ProfiledSharedMutex()
    {
    }

    /**
     * Locks the underlying mutex (shared) and reports the event to the profiler.
     */
    void lock_shared()
    {
        CARB_PROFILE_LOCKABLE_OPERATION(this->m_lockableId, LockableOperationType::BeforeLockShared);
        this->m_mutex.lock_shared();
        CARB_PROFILE_LOCKABLE_OPERATION(this->m_lockableId, LockableOperationType::AfterLockShared);
    }

    /**
     * Attempts a shared lock on the underlying mutex and reports the event to the profiler if successful.
     * @returns \c true if successfully locked; \c false otherwise.
     */
    bool try_lock_shared()
    {
        bool acquired = this->m_mutex.try_lock_shared();
        if (acquired)
        {
            CARB_PROFILE_LOCKABLE_OPERATION(this->m_lockableId, LockableOperationType::AfterSuccessfulTryLockShared);
        }
        return acquired;
    }

    /**
     * Unlocks (shared) the underlying mutex and reports the event to the profiler.
     */
    void unlock_shared()
    {
        this->m_mutex.unlock_shared();
        CARB_PROFILE_LOCKABLE_OPERATION(this->m_lockableId, LockableOperationType::AfterUnlockShared);
    }
};


void deregisterProfilerForClient() noexcept;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail
{

inline void updateMask(uint64_t mask)
{
    g_carbProfilerMask.store(mask, std::memory_order_release);
}

inline void releaseHook(void* iface, void*)
{
    cpp20::atomic_ref<IProfiler*>(g_carbProfiler).store(nullptr); // sequentially consistent
    getFramework()->removeReleaseHook(iface, &releaseHook, nullptr);
}

inline void frameworkReleaseHook(void*, void*)
{
    // Framework is going away, so make sure we get fully deregistered.
    deregisterProfilerForClient();
}

inline void loadHook(const PluginDesc&, void*)
{
    if (!g_carbProfiler)
    {
        IProfiler* profiler = getFramework()->tryAcquireInterface<IProfiler>();
        if (profiler)
        {
            if (profiler->setMaskCallback)
            {
                // Relaxed semantics since we will shortly be synchronizing on g_carbProfiler.
                g_carbProfilerMask.store(profiler->setMaskCallback(updateMask, true), std::memory_order_relaxed);
            }
            else
            {
                g_carbProfilerMask.store(uint64_t(-1), std::memory_order_relaxed); // not supported; let everything
                                                                                   // through
            }
            getFramework()->addReleaseHook(profiler, &detail::releaseHook, nullptr);
            cpp20::atomic_ref<IProfiler*>(g_carbProfiler).store(profiler, std::memory_order_seq_cst); // sequentially
                                                                                                      // consistent
        }
    }
}

inline bool& registered()
{
    static bool r{ false };
    return r;
}

inline LoadHookHandle& loadHookHandle()
{
    static carb::LoadHookHandle handle{};
    return handle;
}

} // namespace detail
#endif

/**
 * Allows access to the @ref g_carbProfiler global variable previously registered with @ref registerProfilerForClient().
 * @returns The value of @ref g_carbProfiler.
 */
inline IProfiler* getProfiler()
{
    return g_carbProfiler;
}

/**
 * Clears the @ref g_carbProfiler global variable and unregisters load and release hooks with the \ref Framework.
 */
inline void deregisterProfilerForClient() noexcept
{
    if (std::exchange(detail::registered(), false))
    {
        auto fw = getFramework();
        auto handle = std::exchange(detail::loadHookHandle(), kInvalidLoadHook);
        IProfiler* profiler = cpp20::atomic_ref<IProfiler*>(g_carbProfiler).exchange(nullptr, std::memory_order_seq_cst);
        if (fw)
        {
            if (profiler && fw->verifyInterface(profiler) && profiler->setMaskCallback)
            {
                profiler->setMaskCallback(detail::updateMask, false);
            }
            if (handle)
            {
                fw->removeLoadHook(handle);
            }
            fw->removeReleaseHook(nullptr, &detail::frameworkReleaseHook, nullptr);
            if (profiler)
            {
                fw->removeReleaseHook(profiler, &detail::releaseHook, nullptr);
            }

            // Unregister channels
            Channel::onProfilerUnregistered();
        }
    }
}

/**
 * Acquires the default IProfiler interface and assigns it to the @ref g_carbProfiler global variable.
 *
 * If a profiler is not yet loaded, a load hook is registered with the \ref Framework and when the profiler is loaded,
 * \ref g_carbProfiler will be automatically set for this module. If the profiler is unloaded, \ref g_carbProfiler will
 * be automatically set to `nullptr`.
 */
inline void registerProfilerForClient() noexcept
{
    if (!std::exchange(detail::registered(), true))
    {
        auto fw = getFramework();
        fw->addReleaseHook(nullptr, &detail::frameworkReleaseHook, nullptr);
        IProfiler* profiler = fw->tryAcquireInterface<IProfiler>();
        if (profiler)
        {
            if (profiler->setMaskCallback)
            {
                // Relaxed semantics since we will shortly be synchronizing on g_carbProfiler.
                g_carbProfilerMask.store(profiler->setMaskCallback(detail::updateMask, true), std::memory_order_relaxed);
            }
            else
            {
                g_carbProfilerMask.store(uint64_t(-1), std::memory_order_relaxed); // let everything through
            }
            bool b = fw->addReleaseHook(profiler, &detail::releaseHook, nullptr);
            CARB_ASSERT(b);
            CARB_UNUSED(b);
        }
        cpp20::atomic_ref<IProfiler*>(g_carbProfiler).store(profiler, std::memory_order_seq_cst); // sequentially
                                                                                                  // consistent
        detail::loadHookHandle() = fw->addLoadHook<IProfiler>(nullptr, &detail::loadHook, nullptr);

        // Register channels
        Channel::onProfilerRegistered();

        // Make sure this only happens once even if re-registered.
        static bool ensureDeregister = (atexit(&deregisterProfilerForClient), true);
        CARB_UNUSED(ensureDeregister);
    }
}

} // namespace profiler
} // namespace carb
