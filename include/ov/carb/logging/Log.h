// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Framework.h"
#include "ILogging.h"

#include "../../omni/log/ILog.h"

#include <cctype>
#include <cstdint>
#include <cstdio>

// example-begin Log levels
namespace carb
{
namespace logging
{
/**
 * Verbose level, this is for detailed diagnostics messages. Expect to see some verbose messages on every frame under
 * certain conditions.
 */
const int32_t kLevelVerbose = -2;
/**
 * Info level, this is for informational messages. They are usually triggered on state changes and typically we should
 * not see the same message on every frame.
 */
const int32_t kLevelInfo = -1;
/**
 * Warning level, this is for warning messages. Something could be wrong but not necessarily an error. Therefore
 * anything that could be a problem but cannot be determined to be an error should fall into this category. This is the
 * default log level threshold, if nothing else was specified via configuration or startup arguments. This is also the
 * reason why it has a value of 0 (default as zero).
 */
const int32_t kLevelWarn = 0;
/**
 * Error level, this is for error messages. An error has occurred but the program can continue.
 */
const int32_t kLevelError = 1;
/**
 * Fatal level, this is for messages on unrecoverable errors. An error has occurred and the program cannot continue.
 * After logging such a message the caller should take immediate action to exit the program or unload the module.
 */
const int32_t kLevelFatal = 2;
} // namespace logging
} // namespace carb
// example-end

CARB_WEAKLINK int32_t g_carbLogLevel = -1;
CARB_WEAKLINK carb::logging::LogFn g_carbLogFn CARB_PRINTF_FUNCTION(6, 7);
CARB_WEAKLINK carb::logging::ILogging* g_carbLogging;

/** A printf that will never be executed so that you will get compiler warnings
 *  if there are any format errors.
 *  This is not necessary on GCC or Clang because they support __attribute__((format))
 *  MSVC's comma elision is broken so this needs the format as a separate parameter,
 *  rather than taking all arguments as __VA_ARGS__.
 */
#if CARB_COMPILER_GNUC
#    define CARB_FAKE_PRINTF(fmt, ...)                                                                                 \
        do                                                                                                             \
        {                                                                                                              \
        } while (0)
#else
#    define CARB_FAKE_PRINTF(fmt, ...)                                                                                 \
        do                                                                                                             \
        {                                                                                                              \
            if (false)                                                                                                 \
                ::printf(fmt, ##__VA_ARGS__);                                                                          \
        } while (0)
#endif


// Use the fake printf() trick to validate the inputs
#define CARB_LOG(level, fmt, ...)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        auto lvl = (level);                                                                                            \
        if (g_carbLogFn && g_carbLogLevel <= lvl)                                                                      \
        {                                                                                                              \
            CARB_FAKE_PRINTF(fmt, ##__VA_ARGS__);                                                                      \
            g_carbLogFn(g_carbClientName, lvl, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__);                      \
        }                                                                                                              \
    } while (0)

#define CARB_LOG_VERBOSE(fmt, ...) CARB_LOG(carb::logging::kLevelVerbose, fmt, ##__VA_ARGS__)
#define CARB_LOG_INFO(fmt, ...) CARB_LOG(carb::logging::kLevelInfo, fmt, ##__VA_ARGS__)
#define CARB_LOG_WARN(fmt, ...) CARB_LOG(carb::logging::kLevelWarn, fmt, ##__VA_ARGS__)
#define CARB_LOG_ERROR(fmt, ...) CARB_LOG(carb::logging::kLevelError, fmt, ##__VA_ARGS__)
#define CARB_LOG_FATAL(fmt, ...) CARB_LOG(carb::logging::kLevelFatal, fmt, ##__VA_ARGS__)

#define CARB_LOG_ONCE(level, fmt, ...)                                                                                   \
    do                                                                                                                   \
    {                                                                                                                    \
        static bool CARB_JOIN(logged_, __LINE__) =                                                                       \
            (g_carbLogFn && g_carbLogLevel <= (level)) ?                                                                 \
                (false ?                                                                                                 \
                     (::printf(fmt, ##__VA_ARGS__), true) :                                                              \
                     (g_carbLogFn(g_carbClientName, (level), __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__), true)) : \
                false;                                                                                                   \
        CARB_UNUSED(CARB_JOIN(logged_, __LINE__));                                                                       \
    } while (0)

#define CARB_LOG_VERBOSE_ONCE(fmt, ...) CARB_LOG_ONCE(carb::logging::kLevelVerbose, fmt, ##__VA_ARGS__)
#define CARB_LOG_INFO_ONCE(fmt, ...) CARB_LOG_ONCE(carb::logging::kLevelInfo, fmt, ##__VA_ARGS__)
#define CARB_LOG_WARN_ONCE(fmt, ...) CARB_LOG_ONCE(carb::logging::kLevelWarn, fmt, ##__VA_ARGS__)
#define CARB_LOG_ERROR_ONCE(fmt, ...) CARB_LOG_ONCE(carb::logging::kLevelError, fmt, ##__VA_ARGS__)
#define CARB_LOG_FATAL_ONCE(fmt, ...) CARB_LOG_ONCE(carb::logging::kLevelFatal, fmt, ##__VA_ARGS__)

#define CARB_LOG_GLOBALS()

namespace carb
{
namespace logging
{
inline ILogging* getLogging()
{
    return g_carbLogging;
}

inline void registerLoggingForClient() noexcept
{
    g_carbLogging = getFramework()->tryAcquireInterface<ILogging>();
    if (g_carbLogging)
    {
        g_carbLogging->registerSource(g_carbClientName, [](int32_t logLevel) { g_carbLogLevel = logLevel; });
        g_carbLogFn = g_carbLogging->log;
    }

    omni::log::addModulesChannels();
}

inline void deregisterLoggingForClient() noexcept
{
    omni::log::removeModulesChannels();

    if (g_carbLogging)
    {
        g_carbLogFn = nullptr;
        if (getFramework() && getFramework()->verifyInterface<ILogging>(g_carbLogging))
        {
            g_carbLogging->unregisterSource(g_carbClientName);
        }
        g_carbLogging = nullptr;
    }
}

struct StringToLogLevelMapping
{
    const char* name;
    int32_t level;
};

const StringToLogLevelMapping kStringToLevelMappings[] = { { "verbose", kLevelVerbose },
                                                           { "info", kLevelInfo },
                                                           { "warning", kLevelWarn },
                                                           { "error", kLevelError },
                                                           { "fatal", kLevelFatal } };

const size_t kStringToLevelMappingsCount = CARB_COUNTOF(kStringToLevelMappings);

/**
 * Function infers integer log level from the parameter string. It allows for partial matching, i.e.
 * "warn" (as well as just "w") will also return `kLevelWarn`.
 */
inline int32_t stringToLevel(const char* levelString)
{
    const int32_t kFallbackLevel = kLevelFatal;
    if (!levelString)
        return kFallbackLevel;

    // Since our log level identifiers start with different characters, we're allowed to just compare the first one.
    // However, whether the need arises, it is worth making a score-based system, where full match will win over
    // partial match, if there are similarly starting log levels.
    int lcLevelChar = tolower((int)levelString[0]);
    for (size_t lm = 0; lm < kStringToLevelMappingsCount; ++lm)
    {
        int lcMappingChar = tolower((int)kStringToLevelMappings[lm].name[0]);
        if (lcLevelChar == lcMappingChar)
            return kStringToLevelMappings[lm].level;
    }

    // Ideally, this should never happen if level string is valid.
    CARB_ASSERT(false);
    CARB_LOG_ERROR("Unknown log level string: %s", levelString);
    return kFallbackLevel;
}
} // namespace logging
} // namespace carb
