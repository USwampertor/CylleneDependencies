// Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"

#include <cstdint>

// example-begin Logger
namespace carb
{
namespace logging
{

/**
 * Defines an extension interface for logging backends to register with the ILogging system.
 *
 * @see ILogging::addLogger
 * @see ILogging::removeLogger
 */
struct Logger
{
    /**
     * Handler for a formatted log message. This function is called by ILogging if the Logger has
     * been registered via ILogging::AddLogger, log level passes the threshold (for module or
     * globally if not set for module), and logging is enabled (for module or globally if not set
     * for module).
     *
     * @param logger The logger interface - can be nullptr if not used by handleMessage
     * @param source The source of the message in UTF8 character encoding - commonly plugin name
     * @param level The severity level of the message
     * @param filename The file name where the message originated from.
     * @param functionName The name of the function where the message originated from.
     * @param lineNumber The line number where the message originated from
     * @param message The formatted message in UTF8 character encoding
     *
     * @warning Thread-safety: this function will potentially be called simultaneously from
     *          multiple threads.
     */
    void(CARB_ABI* handleMessage)(Logger* logger,
                                  const char* source,
                                  int32_t level,
                                  const char* filename,
                                  const char* functionName,
                                  int lineNumber,
                                  const char* message);
};

} // namespace logging
} // namespace carb
// example-end
