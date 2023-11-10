// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Framework.h"
#include "../settings/ISettings.h"
#include "ILogging.h"
#include "Log.h"
#include "StandardLogger.h"

#include <vector>

namespace carb
{
namespace logging
{

/**
 * Converts a string to its equivalent OutputStream value.
 * @param[in] name The case-insensitive name of an output stream value.
 *
 * @returns This returns OutputStream::eStderr, if name is "stderr".
 * @reutrns This returns OutputStream::eDefault for any other name.
 */
inline OutputStream stringToOutputStream(const char* name)
{
    static constexpr struct
    {
        const char* name;
        OutputStream value;
    } kMappings[] = { { "stderr", OutputStream::eStderr } };

    for (size_t i = 0; i < CARB_COUNTOF(kMappings); i++)
    {
#if CARB_PLATFORM_WINDOWS
        if (_stricmp(kMappings[i].name, name) == 0)
#else
        if (strcasecmp(kMappings[i].name, name) == 0)
#endif
            return kMappings[i].value;
    }

    return OutputStream::eDefault;
}

/**
 * Configures global logging plugin with values from the config plugin values. Global logging
 * configuration specifies behavior for any loggers registered later, and doesn't dictate
 * neither how exactly any specific logger should operate, nor how the output will look like.
 *
 * Supported config fields:
 * - "level": string log level value, available options: "verbose"|"info"|"warning"|"error"|"fatal"
 * - "enabled": boolean value, enable or disable logging
 *
 * These values could be specified either per-source, in the source collection ("/log/sources/"),
 * for example, <source> level should be specified as "/log/sources/<source>/level", or globally,
 * as "/log/level". Similar pattern applies to "enabled" property.
 */
inline void configureLogging(settings::ISettings* settings)
{
    Framework* f = getFramework();
    logging::ILogging* logging = f->acquireInterface<logging::ILogging>();

    if (settings == nullptr)
        return;

    if (logging)
    {
        const char* kLogLevel = "/log/level";
        const char* kLogEnabled = "/log/enabled";
        const char* kLogAsync = "/log/async";

        // setting defaults
        settings->setDefaultString(kLogLevel, "Warning");
        settings->setDefaultBool(kLogEnabled, true);
        settings->setDefaultBool(kLogAsync, false);

        // The first order of business is to set logging according to config (this can be from file or command line):
        const int32_t logLevel = logging::stringToLevel(settings->getStringBuffer(kLogLevel));
        logging->setLevelThreshold(logLevel);

        const bool logEnabled = settings->getAsBool(kLogEnabled);
        logging->setLogEnabled(logEnabled);

        logging->setLogAsync(settings->getAsBool(kLogAsync));

        // Read config for source-specific setting overrides

        // First, read the sources collection
        const char* kLogSourcesKey = "/log/sources";

        const carb::dictionary::Item* logSources = settings->getSettingsDictionary(kLogSourcesKey);

        if (logSources != nullptr)
        {
            auto* dictInterface = f->acquireInterface<carb::dictionary::IDictionary>();
            // Traverse the sources collection to set per-source overrides
            for (size_t i = 0, totalChildren = dictInterface->getItemChildCount(logSources); i < totalChildren; ++i)
            {
                const carb::dictionary::Item* curSource = dictInterface->getItemChildByIndex(logSources, i);
                if (curSource == nullptr)
                {
                    CARB_LOG_ERROR("Null log source present in the configuration.");
                    continue;
                }

                const char* curSourceName = dictInterface->getItemName(curSource);
                if (curSourceName == nullptr)
                {
                    CARB_LOG_ERROR("Log source with no name present in the configuration.");
                    continue;
                }

                // Read the source level setting
                const carb::dictionary::Item* curLogLevel = dictInterface->getItem(curSource, "level");
                if (curLogLevel != nullptr)
                {
                    logging->setLevelThresholdForSource(
                        curSourceName, logging::LogSettingBehavior::eOverride,
                        logging::stringToLevel(dictInterface->getStringBuffer(curLogLevel)));
                }

                // Read the source enabled setting
                const carb::dictionary::Item* curLogEnabled = dictInterface->getItem(curSource, "enabled");
                if (curLogEnabled != nullptr)
                {
                    const bool isCurLogEnabled =
                        dictInterface->isAccessibleAs(dictionary::ItemType::eBool, curLogEnabled) ?
                            dictInterface->getAsBool(curLogEnabled) :
                            logEnabled;
                    logging->setLogEnabledForSource(
                        curSourceName, logging::LogSettingBehavior::eOverride, isCurLogEnabled);
                }
            }
        }
    }
}

// example-begin Configure StandardLogger
/**
 * Configures default logger with values from the config plugin values. Default logger configuration
 * specifies where to output the log stream and how the output will look.
 *
 * Further instructions on the meaning of fields is available from StandardLogger.h
 */
inline void configureDefaultLogger(settings::ISettings* settings)
{
    Framework* f = getFramework();
    logging::ILogging* logging = f->acquireInterface<logging::ILogging>();

    if (settings == nullptr)
        return;

    if (logging)
    {
        // Config settings for default logger
        logging::StandardLogger* logger = logging->getDefaultLogger();

        // setting defaults

        const char* kFilePath = "/log/file";
        const char* kFileFlushLevelPath = "/log/fileFlushLevel";
        const char* kFlushStandardStreamOutputPath = "/log/flushStandardStreamOutput";
        const char* kEnableStandardStreamOutputPath = "/log/enableStandardStreamOutput";
        const char* kEnableDebugConsoleOutputPath = "/log/enableDebugConsoleOutput";
        const char* kEnableColorOutputPath = "/log/enableColorOutput";
        const char* kProcessGroupIdPath = "/log/processGroupId";
        const char* kIncludeSourcePath = "/log/includeSource";
        const char* kIncludeChannelPath = "/log/includeChannel";
        const char* kIncludeFilenamePath = "/log/includeFilename";
        const char* kIncludeLineNumberPath = "/log/includeLineNumber";
        const char* kIncludeFunctionNamePath = "/log/includeFunctionName";
        const char* kIncludeTimeStampPath = "/log/includeTimeStamp";
        const char* kIncludeThreadIdPath = "/log/includeThreadId";
        const char* kSetElapsedTimeUnitsPath = "/log/setElapsedTimeUnits";
        const char* kIncludeProcessIdPath = "/log/includeProcessId";
        const char* kLogOutputStream = "/log/outputStream";
        const char* kOutputStreamLevelThreshold = "/log/outputStreamLevel";
        const char* kDebugConsoleLevelThreshold = "/log/debugConsoleLevel";
        const char* kFileOutputLevelThreshold = "/log/fileLogLevel";
        const char* kDetailLogPath = "/log/detail";
        const char* kFullDetailLogPath = "/log/fullDetail";
        const char* kFileAppend = "/log/fileAppend";
        const char* kForceAnsiColor = "/log/forceAnsiColor";

        settings->setDefaultString(kFileFlushLevelPath, "verbose");
        settings->setDefaultBool(kFlushStandardStreamOutputPath, false);

        settings->setDefaultBool(kEnableStandardStreamOutputPath, true);
        settings->setDefaultBool(kEnableDebugConsoleOutputPath, true);
        settings->setDefaultBool(kEnableColorOutputPath, true);
        settings->setDefaultInt(kProcessGroupIdPath, 0);

        settings->setDefaultBool(kIncludeSourcePath, true);
        settings->setDefaultBool(kIncludeChannelPath, true);
        settings->setDefaultBool(kIncludeFilenamePath, false);
        settings->setDefaultBool(kIncludeLineNumberPath, false);
        settings->setDefaultBool(kIncludeFunctionNamePath, false);
        settings->setDefaultBool(kIncludeTimeStampPath, false);
        settings->setDefaultBool(kIncludeThreadIdPath, false);
        settings->setDefaultBool(kIncludeProcessIdPath, false);
        settings->setDefaultBool(kDetailLogPath, false);
        settings->setDefaultBool(kFullDetailLogPath, false);
        settings->setDefaultBool(kForceAnsiColor, false);

        settings->setDefaultString(kLogOutputStream, "");

        settings->setDefaultString(kOutputStreamLevelThreshold, "verbose");
        settings->setDefaultString(kDebugConsoleLevelThreshold, "verbose");
        settings->setDefaultString(kFileOutputLevelThreshold, "verbose");

        // getting values from the settings
        logger->setStandardStreamOutput(logger, settings->getAsBool(kEnableStandardStreamOutputPath));
        logger->setDebugConsoleOutput(logger, settings->getAsBool(kEnableDebugConsoleOutputPath));

        LogFileConfiguration config{};
        settings->setDefaultBool(kFileAppend, config.append);
        config.append = settings->getAsBool(kFileAppend);

        logger->setFileConfiguration(logger, settings->getStringBuffer(kFilePath), &config);
        logger->setFileOuputFlushLevel(logger, logging::stringToLevel(settings->getStringBuffer(kFileFlushLevelPath)));
        logger->setFlushStandardStreamOutput(logger, settings->getAsBool(kFlushStandardStreamOutputPath));

        logger->setForceAnsiColor(logger, settings->getAsBool(kForceAnsiColor));
        logger->setColorOutputIncluded(logger, settings->getAsBool(kEnableColorOutputPath));
        logger->setMultiProcessGroupId(logger, settings->getAsInt(kProcessGroupIdPath));

        bool channel = settings->getAsBool(kIncludeSourcePath) && settings->getAsBool(kIncludeChannelPath);

        // if this is set, it enabled everything
        bool fullDetail = settings->getAsBool(kFullDetailLogPath);

        // if this is set, it enables everything except file name and PID
        bool detail = fullDetail || settings->getAsBool(kDetailLogPath);
        logger->setSourceIncluded(logger, detail || channel);
        logger->setFilenameIncluded(logger, fullDetail || settings->getAsBool(kIncludeFilenamePath));
        logger->setLineNumberIncluded(logger, detail || settings->getAsBool(kIncludeLineNumberPath));
        logger->setFunctionNameIncluded(logger, detail || settings->getAsBool(kIncludeFunctionNamePath));
        logger->setTimestampIncluded(logger, detail || settings->getAsBool(kIncludeTimeStampPath));
        logger->setThreadIdIncluded(logger, detail || settings->getAsBool(kIncludeThreadIdPath));
        logger->setElapsedTimeUnits(logger, settings->getStringBuffer(kSetElapsedTimeUnitsPath));
        logger->setProcessIdIncluded(logger, fullDetail || settings->getAsBool(kIncludeProcessIdPath));

        logger->setOutputStream(logger, stringToOutputStream(settings->getStringBuffer(kLogOutputStream)));

        logger->setStandardStreamOutputLevelThreshold(
            logger, logging::stringToLevel(settings->getStringBuffer(kOutputStreamLevelThreshold)));
        logger->setDebugConsoleOutputLevelThreshold(
            logger, logging::stringToLevel(settings->getStringBuffer(kDebugConsoleLevelThreshold)));
        logger->setFileOutputLevelThreshold(
            logger, logging::stringToLevel(settings->getStringBuffer(kFileOutputLevelThreshold)));
    }
}
// example-end
} // namespace logging
} // namespace carb
