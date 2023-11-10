// Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Interface to querying and adjusting structured logging settings.
 */
#pragma once

#include "StructuredLogCommon.h"
#include "IStructuredLogSettings.h"

#include "../core/IObject.h"


namespace omni
{
namespace structuredlog
{

class IStructuredLogSettings2;

// ****************************** IStructuredLogSettings2 interface *******************************
/** Interface for the second version of the IStructuredLogSettings interface.  This interface
 *  exposes more settings to control the behaviour of the structured logging system.  This
 *  object can be retrieved directly or by casting the main IStructuredLog interface to this
 *  type using `omni::core::ObjectPtr::as<>()`.
 */
class IStructuredLogSettings2_abi
    : public omni::core::Inherits<omni::structuredlog::IStructuredLogSettings_abi,
                                  OMNI_TYPE_ID("omni.structuredlog.IStructuredLogSettings2")>
{
protected:
    /** Retrieves the current heartbeat message period in seconds.
     *
     *  @returns The minimum time in seconds between heartbeat events.  This will be
     *           @ref omni::structuredlog::kHeartbeatDisabled if the heartbeat events are
     *           disabled.  When enabled, the heartbeat events will be generated within
     *           ~100ms of this scheduled time.  In general, it is expected that the heartbeat
     *           period be on the scale of one minute or more to reduce the overall amount of
     *           event traffic.
     */
    virtual uint64_t getHeartbeatPeriod_abi() noexcept = 0;

    /** Sets the new heartbeat event period in seconds.
     *
     *  @param[in] period   The minimum time in seconds between generated heartbeat events.
     *                      This may be @ref omni::structuredlog::kHeartbeatDisabled to disable
     *                      the heartbeat events.
     *  @returns No return value.
     *
     *  @remarks The heartbeat events can be used to get an estimate of a session's length even if
     *           the 'exit' or 'crash' process lifetime events are missing (ie: power loss, user
     *           kills the process, blue screen of death or kernel panic, etc).  The session can
     *           neither be assumed to have exited normally nor crashed with only these heartbeat
     *           events present however.
     */
    virtual void setHeartbeatPeriod_abi(uint64_t period) noexcept = 0;
};

} // namespace structuredlog
} // namespace omni

#define OMNI_BIND_INCLUDE_INTERFACE_DECL
#include "IStructuredLogSettings2.gen.h"

// insert custom API functions here.
/** @copydoc omni::structuredlog::IStructuredLogSettings_abi */
class omni::structuredlog::IStructuredLogSettings2
    : public omni::core::Generated<omni::structuredlog::IStructuredLogSettings2_abi>
{
};

#define OMNI_BIND_INCLUDE_INTERFACE_IMPL
#include "IStructuredLogSettings2.gen.h"
