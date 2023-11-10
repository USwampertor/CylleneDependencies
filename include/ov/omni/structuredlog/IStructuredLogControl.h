// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief The structured log control interface.
 */
#pragma once

#include "StructuredLogCommon.h"

#include "../core/IObject.h"


namespace omni
{
namespace structuredlog
{

class IStructuredLogControl;


// ******************************* enums, types, and constants ************************************
/** A special value to indicate that an operation should affect all registered schemas.  This
 *  can be used when closing all persistently open log files with
 *  @ref IStructuredLogControl::closeLog().
 */
constexpr EventId kAllSchemas = ~2ull;


// ******************************* IStructuredLogControl interface ********************************
/** Structured log state control interface.  This allows for some control over the processing of
 *  events and log files for the structured log system.  The structured log system's event queue
 *  can be temporarily stopped if needed or the output log for a schema may be closed.  Each of
 *  these operations is only temporary as the event queue will be restarted and the log file
 *  opened when the next event is queued with @ref omni::structuredlog::IStructuredLog::allocEvent().
 *
 *  This interface object can be acquired either by requesting it from the type factory or
 *  by casting an @ref omni::structuredlog::IStructuredLog object to this type.
 */
class IStructuredLogControl_abi
    : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.structuredlog.IStructuredLogControl")>
{
protected:
    /** Closes one or more schema's persistently open log file(s).
     *
     *  @param[in] event   The ID of the event to close the log for.  This may also be
     *                     @ref omni::structuredlog::kAllSchemas to close all log file for
     *                     the process.  The log file for the schema that the given event
     *                     belongs to will be closed.
     *  @returns No return value.
     *
     *  @remarks This closes the persistently open log file(s) for one or more schemas.  This
     *           operation will effectively be ignored for schemas that were not registered
     *           using the @ref omni::structuredlog::fSchemaFlagKeepLogOpen schema flag since those schemas will
     *           not leave their logs open.  The named log files will only remain closed until
     *           the next attempt to write an event message to it.  It is the responsibility
     *           of the host app to ensure no events are written to the effected log file(s)
     *           for the duration that the log needs to remain closed.
     *
     *  @note This call itself is thread safe.  However the log file may be reopened if a
     *        pending event is processed in the event queue or a new event is sent while
     *        the calling thread expects the log to remain closed.  It is the caller's
     *        responsibility to either stop the event queue, disable structured logging, or
     *        prevent other events from being sent while the log(s) need to remain closed.
     */
    virtual void closeLog_abi(EventId event) noexcept = 0;

    /** Stop the structured log event consumer thread.
     *
     *  @returns No return value.
     *
     *  @remarks This stops the structured log event processing system.  This will stop the
     *           processing thread and flush all pending event messages to the log.  The
     *           processing system will be restarted when the next
     *           @ref omni::structuredlog::IStructuredLog::allocEvent()
     *           call is made to attempt to send a new message.  This call is useful if the
     *           structured log plugin needs to be unloaded.  If the processing thread is left
     *           running, it will prevent the plugin from being unloaded (or even being attempted
     *           to be unloaded).  This can also be used to temporarily disable the structured
     *           log system if its not needed or wanted.  If the structured log system needs to
     *           be disabled completely, a call to @ref omni::structuredlog::IStructuredLog::setEnabled()
     *           using the @ref omni::structuredlog::fEnableFlagAll flag should be made before stopping
     *           the event queue.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual void stop_abi() noexcept = 0;
};

} // namespace structuredlog
} // namespace omni

#define OMNI_BIND_INCLUDE_INTERFACE_DECL
#include "IStructuredLogControl.gen.h"

/** @copydoc omni::structuredlog::IStructuredLogControl_abi */
class omni::structuredlog::IStructuredLogControl
    : public omni::core::Generated<omni::structuredlog::IStructuredLogControl_abi>
{
};


#define OMNI_BIND_INCLUDE_INTERFACE_IMPL
#include "IStructuredLogControl.gen.h"
