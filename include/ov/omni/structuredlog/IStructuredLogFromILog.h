// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief An interface for redirecting @ref omni::log::ILog messages to structured logging.
 */
#pragma once

#include "StructuredLogCommon.h"

#include "../core/IObject.h"

namespace omni
{
namespace structuredlog
{

class IStructuredLogFromILog;


// ****************************** IStructuredLogFromILog interface ********************************
/** This interface controls the ability to send Carbonite and Omniverse logging through the
 *  structured log system. The output is equivalent to the standard logging output, except that it
 *  is in JSON, so it will be easier for programs to consume.
 *
 *  The default state of structured log logging is off, but it can be enabled by calling
 *  @ref omni::structuredlog::IStructuredLogFromILog::enableLogging() or setting `/structuredLog/enableLogConsumer`
 *  to true with ISettings.
 */
class IStructuredLogFromILog_abi
    : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.structuredlog.IStructuredLogFromILog")>
{
protected:
    // ****** structured logging functions ******
    /** Enable the structured log logger.
     *  @remarks Enabling this will result in all Carbonite logging (e.g. CARB_LOG_*) and all
     *           Omniverse logging (e.g. OMNI_LOG_*) going to the structured log log file.
     *           This may be useful if you want the logs to be consumed by some sort of log reader
     *           program.
     *           These log events will be sent to the default structured log system's log file if
     *           there is one; they will otherwise go to a log file named
     *           "omni.structuredlog.logging-{version}".
     *           These log events will not be sent to the collection servers.
     */
    virtual void enableLogging_abi() noexcept = 0;

    /** Disables the structured log logger.
     *  @remarks After this is called, log messages will no longer be sent as structured log
     *           events to the structured log system's log file.
     */
    virtual void disableLogging_abi() noexcept = 0;

    /** Get the @ref omni::structuredlog::EventId of the logging schema.
     *  @retval omni::structuredlog::EventId of the logging schema.
     *
     *  @remarks This is only needed if you need to query properties of the logging schema,
     *           such as the log file name.
     *
     *  @note The logging schema will still be valid when logging is disabled.
     */
    virtual EventId getLoggingEventId_abi() noexcept = 0;
};

} // namespace structuredlog
} // namespace omni

#define OMNI_BIND_INCLUDE_INTERFACE_DECL
#include "IStructuredLogFromILog.gen.h"

/** @copydoc omni::structuredlog::IStructuredLogFromILog_abi */
class omni::structuredlog::IStructuredLogFromILog
    : public omni::core::Generated<omni::structuredlog::IStructuredLogFromILog_abi>
{
};


#define OMNI_BIND_INCLUDE_INTERFACE_IMPL
#include "IStructuredLogFromILog.gen.h"
