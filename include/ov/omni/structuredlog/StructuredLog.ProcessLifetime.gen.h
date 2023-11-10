// Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
// DO NOT MODIFY THIS FILE. This is a generated file.
// This file was generated from: StructuredLog.ProcessLifetime.json
//
#pragma once

#include <omni/log/ILog.h>
#include <omni/structuredlog/IStructuredLog.h>
#include <omni/structuredlog/JsonTree.h>
#include <omni/structuredlog/BinarySerializer.h>
#include <omni/structuredlog/StringView.h>

#include <memory>

namespace omni
{
namespace structuredlog
{

/** helper macro to send the 'event' event.
 *
 *  @param[in] eventFlags_ The flags to pass directly to the event handling
 *             function.  This may be any of the AllocFlags flags that can
 *             be passed to @ref omni::structuredlog::IStructuredLog::allocEvent().
 *  @param[in] event_ Parameter from schema at path '/event'.
 *             the name of the process lifetime event that occurred.  This may be
 *             any string to identify the event.  This event may be used by the
 *             host app as well to measure its own startup times.
 *  @param[in] context_ Parameter from schema at path '/context'.
 *             extra information about the event that occurred.  This may be
 *             nullptr or an empty string if not needed for a particular event.
 *  @returns no return value.
 *
 *  @remarks Event to send when the app starts, exits, crashes, etc.  This
 *           event can also be used to measure other host app events by simply
 *           sending different 'event' and 'context' strings.  The main purpose
 *           of this is to track session time and the relative frequency of
 *           crashes.
 *
 *  @sa @ref Schema_omni_processlifetime_1_1::event_sendEvent().
 *  @sa @ref Schema_omni_processlifetime_1_1::event_isEnabled().
 */
#define OMNI_OMNI_PROCESSLIFETIME_1_1_EVENT(eventFlags_, event_, context_)                                             \
    OMNI_STRUCTURED_LOG(omni::structuredlog::Schema_omni_processlifetime_1_1::event, eventFlags_, event_, context_)

/** helper macro to send the 'heartbeat' event.
 *
 *  @returns no return value.
 *
 *  @remarks Event to send periodically to give an indication that the process
 *           is still alive and the session is still valid.  This can also be
 *           used to measure session lengths in the case where no 'exit' or
 *           'crash' event can be found (ie: power loss, user kill, BSOD/kernel
 *           panic, etc).
 *
 *  @sa @ref Schema_omni_processlifetime_1_1::heartbeat_sendEvent().
 *  @sa @ref Schema_omni_processlifetime_1_1::heartbeat_isEnabled().
 */
#define OMNI_OMNI_PROCESSLIFETIME_1_1_HEARTBEAT()                                                                      \
    OMNI_STRUCTURED_LOG(omni::structuredlog::Schema_omni_processlifetime_1_1::heartbeat)

class Schema_omni_processlifetime_1_1
{
public:
    /** the event ID names used to send the events in this schema.  These IDs
     *  are used when the schema is first registered, and are passed to the
     *  allocEvent() function when sending the event.
     */
    enum : uint64_t
    {
        kEventEventId = OMNI_STRUCTURED_LOG_EVENT_ID(
            "omni.processlifetime", "com.nvidia.carbonite.processlifetime.event", "1.1", "0"),
        kHeartbeatEventId = OMNI_STRUCTURED_LOG_EVENT_ID(
            "omni.processlifetime", "com.nvidia.carbonite.processlifetime.heartbeat", "1.1", "0"),
    };

    Schema_omni_processlifetime_1_1() = default;

    /** Register this class with the @ref omni::structuredlog::IStructuredLog interface.
     *  @param[in] flags The flags to pass into @ref omni::structuredlog::IStructuredLog::allocSchema()
     *                   This may be zero or more of the @ref omni::structuredlog::SchemaFlags flags.
     *  @returns `true` if the operation succeded.
     *  @returns `false` if @ref omni::structuredlog::IStructuredLog couldn't be loaded.
     *  @returns `false` if a memory allocation failed.
     */
    static bool registerSchema(omni::structuredlog::IStructuredLog* strucLog) noexcept
    {
        return _registerSchema(strucLog);
    }

    /** Check whether this structured log schema is enabled.
     *  @param[in] eventId     the ID of the event to check the enable state for.
     *                         This must be one of the @a k*EventId symbols
     *                         defined above.
     *  @returns Whether this client is enabled.
     */
    static bool isEnabled(omni::structuredlog::EventId eventId) noexcept
    {
        return _isEnabled(eventId);
    }

    /** Enable/disable an event in this schema.
     *  @param[in] eventId     the ID of the event to enable or disable.
     *                         This must be one of the @a k*EventId symbols
     *                         defined above.
     *  @param[in] enabled     Whether is enabled or disabled.
     */
    static void setEnabled(omni::structuredlog::EventId eventId, bool enabled) noexcept
    {
        _setEnabled(eventId, enabled);
    }

    /** Enable/disable this schema.
     *  @param[in] enabled     Whether is enabled or disabled.
     */
    static void setEnabled(bool enabled) noexcept
    {
        _setEnabled(enabled);
    }

    /** event enable check helper functions.
     *
     *  @param[in] strucLog   The structured log object to use to send this event.  This
     *                        must not be nullptr.  It is the caller's responsibility
     *                        to ensure that a valid object is passed in.
     *  @returns `true` if the specific event and this schema are both enabled.
     *  @returns `false` if either the specific event or this schema is disabled.
     *
     *  @remarks These check if an event corresponding to the function name is currently
     *           enabled.  These are useful to avoid parameter evaluation before calling
     *           into one of the event emitter functions.  These will be called from the
     *           OMNI_STRUCTURED_LOG() macro.  These may also be called directly if an event
     *           needs to be emitted manually, but the only effect would be the potential
     *           to avoid parameter evaluation in the *_sendEvent() function.  Each
     *           *_sendEvent() function itself will also internally check if the event
     *           is enabled before sending it.
     *  @{
     */
    static bool event_isEnabled(omni::structuredlog::IStructuredLog* strucLog) noexcept
    {
        return strucLog->isEnabled(kEventEventId);
    }

    static bool heartbeat_isEnabled(omni::structuredlog::IStructuredLog* strucLog) noexcept
    {
        return strucLog->isEnabled(kHeartbeatEventId);
    }
    /** @} */

    /** Send the event 'com.nvidia.carbonite.processlifetime.event'
     *
     *  @param[in] strucLog The global structured log object to use to send
     *             this event.  This must not be nullptr.  It is the caller's
     *             responsibility to ensure a valid object is passed in.
     *  @param[in] eventFlags The flags to pass directly to the event handling
     *             function.  This may be any of the AllocFlags flags that can
     *             be passed to @ref omni::structuredlog::IStructuredLog::allocEvent().
     *  @param[in] event Parameter from schema at path '/event'.
     *             the name of the process lifetime event that occurred.  This may be
     *             any string to identify the event.  This event may be used by the
     *             host app as well to measure its own startup times.
     *  @param[in] context Parameter from schema at path '/context'.
     *             extra information about the event that occurred.  This may be
     *             nullptr or an empty string if not needed for a particular event.
     *  @returns no return value.
     *
     *  @remarks Event to send when the app starts, exits, crashes, etc.  This
     *           event can also be used to measure other host app events by simply
     *           sending different 'event' and 'context' strings.  The main purpose
     *           of this is to track session time and the relative frequency of
     *           crashes.
     */
    static void event_sendEvent(omni::structuredlog::IStructuredLog* strucLog,
                                AllocFlags eventFlags,
                                const omni::structuredlog::StringView& event,
                                const omni::structuredlog::StringView& context) noexcept
    {
        _event_sendEvent(strucLog, eventFlags, event, context);
    }

    /** Send the event 'com.nvidia.carbonite.processlifetime.heartbeat'
     *
     *  @param[in] strucLog The global structured log object to use to send
     *             this event.  This must not be nullptr.  It is the caller's
     *             responsibility to ensure a valid object is passed in.
     *  @returns no return value.
     *
     *  @remarks Event to send periodically to give an indication that the process
     *           is still alive and the session is still valid.  This can also be
     *           used to measure session lengths in the case where no 'exit' or
     *           'crash' event can be found (ie: power loss, user kill, BSOD/kernel
     *           panic, etc).
     */
    static void heartbeat_sendEvent(omni::structuredlog::IStructuredLog* strucLog) noexcept
    {
        _heartbeat_sendEvent(strucLog);
    }

private:
    /** This will allow us to disable array length checks in release builds,
     *  since they would have a negative performance impact and only be hit
     *  in unusual circumstances.
     */
    static constexpr bool kValidateLength = CARB_DEBUG;

    /** body for the registerSchema() public function. */
    static bool _registerSchema(omni::structuredlog::IStructuredLog* strucLog)
    {
        omni::structuredlog::AllocHandle handle = {};
        omni::structuredlog::SchemaResult result;
        uint8_t* buffer;
        omni::structuredlog::EventInfo events[2] = {};
        size_t bufferSize = 0;
        size_t total = 0;
        omni::structuredlog::SchemaFlags flags = 0;

        if (strucLog == nullptr)
        {
            OMNI_LOG_WARN(
                "no structured log object!  The schema "
                "'Schema_omni_processlifetime_1_1' "
                "will be disabled.");
            return false;
        }

        // calculate the tree sizes
        size_t event_size = _event_calculateTreeSize();
        size_t heartbeat_size = _heartbeat_calculateTreeSize();

        // calculate the event buffer size
        bufferSize += event_size;
        bufferSize += heartbeat_size;

        // begin schema creation
        buffer = strucLog->allocSchema("omni.processlifetime", "1.1", flags, bufferSize, &handle);
        if (buffer == nullptr)
        {
            OMNI_LOG_ERROR("allocSchema failed (size = %zu bytes)", bufferSize);
            return false;
        }

        // register all the events
        events[0].schema = _event_buildJsonTree(event_size, buffer + total);
        events[0].eventName = "com.nvidia.carbonite.processlifetime.event";
        events[0].parserVersion = 0;
        events[0].eventId = kEventEventId;
        events[0].flags = omni::structuredlog::fEventFlagCriticalEvent | omni::structuredlog::fEventFlagUseLocalLog;
        total += event_size;
        events[1].schema = _heartbeat_buildJsonTree(heartbeat_size, buffer + total);
        events[1].eventName = "com.nvidia.carbonite.processlifetime.heartbeat";
        events[1].parserVersion = 0;
        events[1].eventId = kHeartbeatEventId;
        events[1].flags = omni::structuredlog::fEventFlagCriticalEvent | omni::structuredlog::fEventFlagUseLocalLog;
        total += heartbeat_size;

        result = strucLog->commitSchema(handle, events, CARB_COUNTOF(events));
        if (result != omni::structuredlog::SchemaResult::eSuccess &&
            result != omni::structuredlog::SchemaResult::eAlreadyExists)
        {
            OMNI_LOG_ERROR(
                "failed to register structured log events "
                "{result = %s (%zu)}",
                getSchemaResultName(result), size_t(result));
            return false;
        }

        return true;
    }

    /** body for the isEnabled() public function. */
    static bool _isEnabled(omni::structuredlog::EventId eventId)
    {
        omni::structuredlog::IStructuredLog* strucLog = omniGetStructuredLogWithoutAcquire();
        return strucLog != nullptr && strucLog->isEnabled(eventId);
    }

    /** body for the setEnabled() public function. */
    static void _setEnabled(omni::structuredlog::EventId eventId, bool enabled)
    {
        omni::structuredlog::IStructuredLog* strucLog = omniGetStructuredLogWithoutAcquire();
        if (strucLog == nullptr)
            return;

        strucLog->setEnabled(eventId, 0, enabled);
    }

    /** body for the setEnabled() public function. */
    static void _setEnabled(bool enabled)
    {
        omni::structuredlog::IStructuredLog* strucLog = omniGetStructuredLogWithoutAcquire();
        if (strucLog == nullptr)
            return;

        strucLog->setEnabled(kEventEventId, omni::structuredlog::fEnableFlagWholeSchema, enabled);
    }

#if OMNI_PLATFORM_WINDOWS
#    pragma warning(push)
#    pragma warning(disable : 4127) // warning C4127: conditional expression is constant.
#endif

    /** body for the event_sendEvent() function. */
    static void _event_sendEvent(omni::structuredlog::IStructuredLog* strucLog,
                                 AllocFlags eventFlags,
                                 const omni::structuredlog::StringView& event,
                                 const omni::structuredlog::StringView& context) noexcept
    {
        omni::structuredlog::AllocHandle handle = {};

        // calculate the required buffer size for the event
        omni::structuredlog::BinaryBlobSizeCalculator calc;
        {
            if (kValidateLength && event.length() + 1 > UINT16_MAX)
            {
                OMNI_LOG_ERROR(
                    "length of parameter 'event' exceeds max value 65535 - "
                    "it will be truncated (size was %zu)",
                    event.length() + 1);
            }

            // property event
            calc.track(event);

            if (kValidateLength && context.length() + 1 > UINT16_MAX)
            {
                OMNI_LOG_ERROR(
                    "length of parameter 'context' exceeds max value 65535 - "
                    "it will be truncated (size was %zu)",
                    context.length() + 1);
            }

            // property context
            calc.track(context);
        }

        // write out the event into the buffer
        void* buffer = strucLog->allocEvent(0, kEventEventId, eventFlags, calc.getSize(), &handle);
        if (buffer == nullptr)
        {
            OMNI_LOG_ERROR(
                "failed to allocate a %zu byte buffer for structured log event "
                "'com.nvidia.carbonite.processlifetime.event'",
                calc.getSize());
            return;
        }

        omni::structuredlog::BlobWriter<CARB_DEBUG, _onStructuredLogValidationError> writer(buffer, calc.getSize());
        {
            // property event
            writer.copy(event);

            // property context
            writer.copy(context);
        }

        strucLog->commitEvent(handle);
    }

    /** body for the heartbeat_sendEvent() function. */
    static void _heartbeat_sendEvent(omni::structuredlog::IStructuredLog* strucLog) noexcept
    {
        omni::structuredlog::AllocHandle handle = {};

        // write out the event into the buffer
        if (strucLog->allocEvent(0, kHeartbeatEventId, 0, 0, &handle) == nullptr)
        {
            OMNI_LOG_ERROR(
                "failed to allocate a buffer for structured log event com.nvidia.carbonite.processlifetime.heartbeat");
            return;
        }
        strucLog->commitEvent(handle);
    }
#if OMNI_PLATFORM_WINDOWS
#    pragma warning(pop)
#endif

    /** Calculate JSON tree size for structured log event: com.nvidia.carbonite.processlifetime.event.
     *  @returns The JSON tree size in bytes for this event.
     */
    static size_t _event_calculateTreeSize()
    {
        // calculate the buffer size for the tree
        omni::structuredlog::JsonTreeSizeCalculator calc;
        calc.trackRoot();
        calc.trackObject(2); // object has 2 properties
        {
            // property event
            calc.trackName("event");
            calc.track(static_cast<const char*>(nullptr));

            // property context
            calc.trackName("context");
            calc.track(static_cast<const char*>(nullptr));
        }
        return calc.getSize();
    }

    /** Generate the JSON tree for structured log event: com.nvidia.carbonite.processlifetime.event.
     *  @param[in]    bufferSize The length of @p buffer in bytes.
     *  @param[inout] buffer     The buffer to write the tree into.
     *  @returns The JSON tree for this event.
     *  @returns nullptr if a logic error occurred or @p bufferSize was too small.
     */
    static omni::structuredlog::JsonNode* _event_buildJsonTree(size_t bufferSize, uint8_t* buffer)
    {
        CARB_MAYBE_UNUSED bool result;
        omni::structuredlog::BlockAllocator alloc(buffer, bufferSize);
        omni::structuredlog::JsonBuilder builder(&alloc);
        omni::structuredlog::JsonNode* base = static_cast<omni::structuredlog::JsonNode*>(alloc.alloc(sizeof(*base)));
        if (base == nullptr)
        {
            OMNI_LOG_ERROR(
                "failed to allocate the base node for event "
                "'com.nvidia.carbonite.processlifetime.event' "
                "{alloc size = %zu, buffer size = %zu}",
                sizeof(*base), bufferSize);
            return nullptr;
        }
        *base = {};

        // build the tree
        result = builder.createObject(base, 2); // object has 2 properties
        if (!result)
        {
            OMNI_LOG_ERROR("failed to create an object node (bad size calculation?)");
            return nullptr;
        }
        {
            // property event
            result = builder.setName(&base->data.objVal[0], "event");
            if (!result)
            {
                OMNI_LOG_ERROR("failed to set the object name (bad size calculation?)");
                return nullptr;
            }
            result = builder.setNode(&base->data.objVal[0], static_cast<const char*>(nullptr));
            if (!result)
            {
                OMNI_LOG_ERROR("failed to set type 'const char*' (shouldn't be possible)");
                return nullptr;
            }

            // property context
            result = builder.setName(&base->data.objVal[1], "context");
            if (!result)
            {
                OMNI_LOG_ERROR("failed to set the object name (bad size calculation?)");
                return nullptr;
            }
            result = builder.setNode(&base->data.objVal[1], static_cast<const char*>(nullptr));
            if (!result)
            {
                OMNI_LOG_ERROR("failed to set type 'const char*' (shouldn't be possible)");
                return nullptr;
            }
        }

        return base;
    }

    /** Calculate JSON tree size for structured log event: com.nvidia.carbonite.processlifetime.heartbeat.
     *  @returns The JSON tree size in bytes for this event.
     */
    static size_t _heartbeat_calculateTreeSize()
    {
        // calculate the buffer size for the tree
        omni::structuredlog::JsonTreeSizeCalculator calc;
        calc.trackRoot();
        calc.trackObject(0); // object has 0 properties
        {
        }
        return calc.getSize();
    }

    /** Generate the JSON tree for structured log event: com.nvidia.carbonite.processlifetime.heartbeat.
     *  @param[in]    bufferSize The length of @p buffer in bytes.
     *  @param[inout] buffer     The buffer to write the tree into.
     *  @returns The JSON tree for this event.
     *  @returns nullptr if a logic error occurred or @p bufferSize was too small.
     */
    static omni::structuredlog::JsonNode* _heartbeat_buildJsonTree(size_t bufferSize, uint8_t* buffer)
    {
        CARB_MAYBE_UNUSED bool result;
        omni::structuredlog::BlockAllocator alloc(buffer, bufferSize);
        omni::structuredlog::JsonBuilder builder(&alloc);
        omni::structuredlog::JsonNode* base = static_cast<omni::structuredlog::JsonNode*>(alloc.alloc(sizeof(*base)));
        if (base == nullptr)
        {
            OMNI_LOG_ERROR(
                "failed to allocate the base node for event "
                "'com.nvidia.carbonite.processlifetime.heartbeat' "
                "{alloc size = %zu, buffer size = %zu}",
                sizeof(*base), bufferSize);
            return nullptr;
        }
        *base = {};

        // build the tree
        result = builder.createObject(base, 0); // object has 0 properties
        if (!result)
        {
            OMNI_LOG_ERROR("failed to create an object node (bad size calculation?)");
            return nullptr;
        }
        {
        }

        return base;
    }

    /** The callback that is used to report validation errors.
     *  @param[in] s The validation error message.
     */
    static void _onStructuredLogValidationError(const char* s)
    {
        OMNI_LOG_ERROR("error sending a structured log event: %s", s);
    }
};

// asserts to ensure that no one's modified our dependencies
static_assert(omni::structuredlog::BlobWriter<>::kVersion == 0, "BlobWriter verison changed");
static_assert(omni::structuredlog::JsonNode::kVersion == 0, "JsonNode verison changed");
static_assert(sizeof(omni::structuredlog::JsonNode) == 24, "unexpected size");
static_assert(std::is_standard_layout<omni::structuredlog::JsonNode>::value, "this type needs to be ABI safe");
static_assert(offsetof(omni::structuredlog::JsonNode, type) == 0, "struct layout changed");
static_assert(offsetof(omni::structuredlog::JsonNode, flags) == 1, "struct layout changed");
static_assert(offsetof(omni::structuredlog::JsonNode, len) == 2, "struct layout changed");
static_assert(offsetof(omni::structuredlog::JsonNode, nameLen) == 4, "struct layout changed");
static_assert(offsetof(omni::structuredlog::JsonNode, name) == 8, "struct layout changed");
static_assert(offsetof(omni::structuredlog::JsonNode, data) == 16, "struct layout changed");

} // namespace structuredlog
} // namespace omni

OMNI_STRUCTURED_LOG_ADD_SCHEMA(omni::structuredlog::Schema_omni_processlifetime_1_1, omni_processlifetime, 1_1, 0);
