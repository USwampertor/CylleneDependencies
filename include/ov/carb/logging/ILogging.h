// Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Interface.h"

namespace carb
{
namespace logging
{


struct StandardLogger;
struct Logger;

/**
 * Defines a callback type for setting log level for every source.
 */
typedef void(CARB_ABI* SetLogLevelFn)(int32_t logLevel);

/**
 * Defines a log setting behavior.
 */
enum class LogSettingBehavior
{
    eInherit,
    eOverride
};

typedef void (*LogFn)(const char* source,
                      int32_t level,
                      const char* fileName,
                      const char* functionName,
                      int lineNumber,
                      const char* fmt,
                      ...);

/**
 * Defines the log system that is associated with the Framework.
 *
 * This interface defines the log system, which is a singleton object. It can be used at any moment, including
 * before the startup of the Framework and after the Framework was shutdown. It allows a user to setup the logging
 * behavior in advance and allows the Framework to log during its initialization.
 *
 * Logger - is an interface for logging backend. ILogging can contain multiple Loggers and every message will be passed
 * to every logger. There is one implementation of a Logger provided - StandardLogger. It can log into file, console and
 * debug window. ILogging starts up with one instance of StandardLogger, which can be retrieved by calling
 * getDefaultLogger(). It is added by default, but can be removed with
 * getLogging()->removeLogger(getLogging()->getDefaultLogger()) call.
 *
 * ILogging supports multiple sources of log messages. Source is just a name to differentiate the origins of a message.
 * Every plugin, application and Framework itself are different sources. However user can add more custom sources if
 * needed.
 *
 * Use the logging macros from Log.h for all logging in applications and plugins.
 *
 * There are 2 log settings: log level (to control log severity threshold) and log enabled (to toggle whole logging).
 * Both of them can be set globally and per source.
 */
struct ILogging
{
    CARB_PLUGIN_INTERFACE("carb::logging::ILogging", 1, 0)

    /**
     * Logs a formatted message to the specified log source and log level.
     *
     * This API is used primarily by the CARB_LOG_XXXX macros.
     *
     * @param source The log source to log the message to.
     * @param level The level to log the message at.
     * @param fileName The file name to log to.
     * @param functionName The name of the function where the message originated from.
     * @param lineNumber The line number
     * @param fmt The print formatted message.
     * @param ... The variable arguments for the formatted message variables.
     */
    void(CARB_ABI* log)(const char* source,
                        int32_t level,
                        const char* fileName,
                        const char* functionName,
                        int lineNumber,
                        const char* fmt,
                        ...) CARB_PRINTF_FUNCTION(6, 7);

    /**
     * Sets global log level threshold. Messages below this threshold will be dropped.
     *
     * @param level The log level to set.
     */
    void(CARB_ABI* setLevelThreshold)(int32_t level);

    /**
     * Gets global log level threshold. Messages below this threshold will be dropped.
     *
     * @return Global log level.
     */
    int32_t(CARB_ABI* getLevelThreshold)();

    /**
     * Sets global log enabled setting.
     *
     * @param enabled Global log enabled setting.
     */
    void(CARB_ABI* setLogEnabled)(bool enabled);

    /**
     * If global log is enabled.
     *
     * @return Global log enabled.
     */
    bool(CARB_ABI* isLogEnabled)();

    /**
     * Sets log level threshold for the specified source. Messages below this threshold will be dropped.
     * Per source log settings can either inherit global or override it, it is configured with
     * LogSettingBehavior::eInherit and LogSettingBehavior::eOverride accordingly.
     *
     * @param source The source to set log level setting for. Must not be nullptr.
     * @param behavior The log setting behavior for the source.
     * @param level The log level to set. This param is ignored if behavior is set to LogSettingBehavior::eInherit.
     */
    void(CARB_ABI* setLevelThresholdForSource)(const char* source, LogSettingBehavior behavior, int32_t level);

    /**
     * Sets log enabled setting for the specified source.
     * Per source log settings can either inherit global or override it, it is configured with
     * LogSettingBehavior::eInherit and LogSettingBehavior::eOverride accordingly.
     *
     * @param source The source to set log enabled setting for. Must not be nullptr.
     * @param behavior The log setting behavior for the source.
     * @param enabled The log enabled setting. This param is ignored if behavior is set to LogSettingBehavior::eInherit.
     */
    void(CARB_ABI* setLogEnabledForSource)(const char* source, LogSettingBehavior behavior, bool enabled);

    /**
     * Reset all log settings set both globally and per source.
     * Log system resets to the defaults: log is enabled and log level is 'warn'.
     */
    void(CARB_ABI* reset)();

    /**
     * Adds a logger to the ILogging.
     *
     * @param logger The logger to be added.
     */
    void(CARB_ABI* addLogger)(Logger* logger);

    /**
     * Removes the logger from the ILogging.
     *
     * @param logger The logger to be removed.
     */
    void(CARB_ABI* removeLogger)(Logger* logger);

    /**
     * Gets the default logger. To disable this logger pass it to ILogging::removeLogger.
     * This logger instance is owned by the ILogging and users should never call
     * destroy on it.
     *
     * @return The default logger.
     */
    StandardLogger*(CARB_ABI* getDefaultLogger)();

    /**
     * Use this method to create additional StandardLogger instances. This can be
     * useful when you want to log to multiple output destinations but want different
     * configuration for each. Use addLogger to make it active.
     *
     * @return New StandardLogger. The lifetime must be managed by caller, destroy by
     *         calling destroyStandardLogger function. In order to activate the logger add
     *         it by calling ILogging::addLogger.
     */
    StandardLogger*(CARB_ABI* createStandardLogger)();

    /**
     * Use this method to destroy a StandardLogger that was created via createStandardLogger.
     *
     * @param logger The logger to be destroyed.
     */
    void(CARB_ABI* destroyStandardLogger)(StandardLogger* logger);

    /**
     * Register new logging source.
     * This function is mainly automatically used by plugins, application and the Framework itself to create
     * their own logging sources.
     *
     * Custom logging source can also be created if needed. It is the user's responsibility to call
     * ILogging::unregisterSource in that case.
     *
     * It is the source responsibility to track its log level. The reason is that it allows to filter out
     * logging before calling into the logging system, thus making the performance cost for the disabled logging
     * negligible. It is done by providing a callback `setLevelThreshold` to be called by ILogging when any setting
     * which influences this source is changed.
     *
     * @param source The source name. Must not be nullptr.
     * @param setLevelThreshold The callback to be called to update log level. ILogging::unregisterSource must be called
     * when the callback is no longer valid.
     */
    void(CARB_ABI* registerSource)(const char* source, SetLogLevelFn setLevelThreshold);

    /**
     * Unregister logging source.
     *
     * @param source The source name. Must not be nullptr.
     */
    void(CARB_ABI* unregisterSource)(const char* source);

    /**
     * Instructs the logging system to deliver all log messages to the Logger backends asynchronously. Async logging is
     * OFF by default.
     *
     * This causes log() calls to be buffered so that log() may return as quickly as possible. A background thread then
     * issues these buffered log messages to the registered Logger backend objects.
     *
     * @param logAsync True to use async logging; false to disable async logging.
     * @return true if was previously using async logging; false if was previously using synchronous logging.
     */
    bool(CARB_ABI* setLogAsync)(bool logAsync);

    /**
     * Returns whether the ILogging system is using async logging.
     * @return true if currently using async logging; false if currently using synchronous logging
     */
    bool(CARB_ABI* getLogAsync)();

    /**
     * When ILogging is in async mode, wait until all log messages have flushed out to the various loggers.
     */
    void(CARB_ABI* flushLogs)();
};
} // namespace logging
} // namespace carb
