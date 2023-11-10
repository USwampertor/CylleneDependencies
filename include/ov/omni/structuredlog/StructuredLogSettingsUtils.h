// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Utilities for the carb::settings::ISettings settings for structured logging.
 */
#pragma once

#include "IStructuredLog.h"
#include "IStructuredLogSettings.h"
#include "IStructuredLogSettings2.h"
#include "IStructuredLogFromILog.h"

#include "../../carb/extras/StringSafe.h"
#include "../../carb/settings/ISettings.h"
#include "../extras/PrivacySettings.h"
#include "../log/ILog.h"


namespace omni
{
namespace structuredlog
{

/** Constants for default and minimum values for various settings.
 *  @{
 */

/** The default value for the log size limit in bytes.
 *  See IStructuredLogSettings::setLogSizeLimit() for details.
 */
constexpr int64_t kDefaultLogSizeLimit = 50ll * 1024ll * 1024ll;

/** The minimum value that can be set for the log size limit in bytes.
 *  See IStructuredLogSettings::setLogSizeLimit() for details.
 */
constexpr int64_t kMinLogSizeLimit = 256ll * 1024ll;

/** The default log retention setting.
 *  See IStructuredLogSettings::setLogRetentionCount() for details.
 */
constexpr size_t kDefaultLogRetentionCount = 3;

/** The minimum allowed log retention setting.
 *  See IStructuredLogSettings::setLogRetentionCount() for details.
 */
constexpr size_t kMinLogRetentionCount = 1;

/** The default value for the event queue size in bytes.
 *  See IStructuredLogSettings::setEventQueueSize() for details.
 */
constexpr size_t kDefaultEventQueueSize = 2 * 1024 * 1024;

/** The minimum allowed event queue size in bytes.
 *  See IStructuredLogSettings::setEventQueueSize() for details.
 */
constexpr size_t kMinEventQueueSize = 512 * 1024;

/** The default mode for generating event IDs.
 *  See IStructuredLogSettings::setEventIdMode() for details.
 */
constexpr IdMode kDefaultIdMode = IdMode::eFastSequential;

/** The default type of event ID to generate.
 *  See IStructuredLogSettings::setEventIdMode() for details.
 */
constexpr IdType kDefaultIdType = IdType::eUuid;

/** Special value to indicate that the heartbeat event should be disabled.  Any other value
 *  will indicate that the heartbeat events will be generated.
 */
constexpr uint64_t kHeartbeatDisabled = 0;

/** The default minimum time between heartbeat events in seconds.  This can be
 *  @ref kHeartbeatDisabled to indicate that the heartbeat events are disabled.  For more
 *  details, please see IStructuredLogSettings2::setHeartbeatPeriod().
 */
constexpr uint64_t kDefaultHeartbeatPeriod = kHeartbeatDisabled;
/** @} */

/** Names for various settings that can be used to override some of the default settings.  Note
 *  that these will not override any values that are explicitly set by the host app itself.
 *  @{
 */
/** Global enable/disable for structured logging.  When set to `false`, the structured log system
 *  will be disabled.  This will prevent any event messages from being written out unless the
 *  host app explicitly wants them to.  When set to `true`, the structured log system will be
 *  enabled and event messages will be emitted normally.  This defaults to `false`.
 */
constexpr const char* kGlobalEnableSetting = "/structuredLog/enable";

/** The directory where the logs will be sent to.
 *  The default value is $HOME/.nvidia-omniverse/logs ($HOME on windows is %USERPOFILE%).
 *  This setting should not be set in most cases; this is mainly useful for testing.
 */
constexpr const char* kLogDirectory = "/structuredLog/logDirectory";

/** The default log name to use.  If a default log name is set, all events that do not use the
 *  @ref fEventFlagUseLocalLog flag will write their messages to this log file.  Events that
 *  do use the @ref fEventFlagUseLocalLog flag will write only to their schema's log file.  This
 *  value must be only the log file's name, not including its path.  The logs will always be
 *  created in the structured logging system's current log output path.  As a special case, this
 *  may also be set to either "/dev/stdout" or "/dev/stderr" (on all platforms) to output all
 *  events directly to stdout or stderr respectively.  When one of these log names are used, all
 *  events will be unconditionally written to stdout/stderr regardless of their event or schema
 *  flags.  There will also be no log header written out when events are being sent to stdout or
 *  stderr.  This defaults to an empty string.
 */
constexpr const char* kDefaultLogNameSetting = "/structuredLog/defaultLogName";

/** The setting path for the log retention count.  This controls how many log files will be
 *  left in the log directory when a log rotation occurs.  When a log file reaches its size
 *  limit, it is renamed and a new empty log with the original name is created.  A rolling
 *  history of the few most recent logs is maintained after a rotation.  This setting controls
 *  exactly how many of each log will be retained after a rotation.  This defaults to 3.
 */
constexpr const char* kLogRetentionCountSetting = "/structuredLog/logRetentionCount";

/** The setting path for the log size limit in megabytes.  When a log file reaches this size,
 *  it is rotated out by renaming it and creating a new log file with the original name.  If
 *  too many logs exist after this rotation, the oldest one is deleted.  This defaults to 50MB.
 */
constexpr const char* kLogSizeLimitSetting = "/structuredLog/logSizeLimit";

/** The setting path for the size of the event queue buffer in kilobytes.  The size of the
 *  event queue controls how many messages can be queued in the message processing queue before
 *  events start to get dropped (or a stall potentially occurs).  The event queue can fill up
 *  if the app is emitting messages from multiple threads at a rate that is higher than they
 *  can be processed or written to disk.  In general, there should not be a situation where
 *  the app is emitting messages at a rate that causes the queue to fill up.  However, this
 *  may be beyond the app's control if (for example) the drive the log is being written to
 *  is particularly slow or extremely busy.  This defaults to 2048KB.
 */
constexpr const char* kEventQueueSizeSetting = "/structuredLog/eventQueueSize";

/** The setting path for the event identifier mode.  This controls how event identifiers are
 *  generated.  Valid values are 'fast-sequential', `sequential`, and `random`.  Each has its
 *  own benefits and drawbacks:
 *    * `sequential` ensures that all generated event IDs are in sequential order.  When the
 *      event ID type is set to `UUID`, this will ensure that each generated event ID can be
 *      easily sorted after the previous one.  With a UUID type ID, this mode can be expensive
 *      to generate.  With a `uint64` ID, this is the most performant to generate.
 *    * `fast-sequential` is only effective when the event ID type is set to 'UUID'.  In this
 *      mode, the UUIDs that are generated are sequential, but in memory order, not lexicographical
 *      order.  It takes some extra effort to sort these events on the data analysis side, but
 *      they are generated very quickly.  When the event ID type is not 'UUID', this mode behaves
 *      in the same way as `sequential`.
 *    * `random` generates a random event ID for each new event.  This does not preserve any
 *      kind of order of events.  If the app does not require sequential events, this can be
 *      more performant to generate especially for UUIDs.
 *
 *  This defaults to 'fast-sequential.  This setting is not case sensitive.
 */
constexpr const char* kEventIdModeSetting = "/structuredLog/eventIdMode";

/** The setting path for the event identifier data type.  This determines what kind of event
 *  ID will be generated for each new event and how it will be printed out with each message.
 *  The following types are supported:
 *    * `UUID` generates a 128 bit universally unique identifier.  The event ID mode determines
 *      how one event ID will be related to the next.  This is printed into each event message
 *      in the standard UUID format ("00000000-0000-0000-0000-000000000000").  This type provides
 *      the most uniqueness and room for scaling in large data sets.
 *    * `uint64` generates a 64 bit integer identifier.  The event ID mode determines how one
 *      event ID will be related to the next.  This is printed into each event message as a
 *      simple decimal integer value.
 *
 *  This defaults to 'UUID'.  This setting is not case sensitive.
 */
constexpr const char* kEventIdTypeSetting = "/structuredLog/eventIdType";

/** The setting path for the log consumer toggle.  This enables or disables the redirection
 *  of normal Carbonite (ie: `CARB_LOG_*()`) and Omni (ie: `OMNI_LOG_*()`) messages as structured
 *  log events as well.  The log messages will still go to their original destination (stdout,
 *  stderr, log file, MSVC output window, etc) as well.  This defaults to `false`.
 */
constexpr const char* kEnableLogConsumerSetting = "/structuredLog/enableLogConsumer";

/** The setting path that will contain zero or more keys that will be used to disable schemas
 *  when they are first registered.  Each key under this setting will have a name that matches
 *  zero or schema names.  From a .schema file, this would match the "name" property.  From a
 *  JSON schema file, this would match the @a `#/schemaMeta/clientName` property.  The key's
 *  value is expected to be a boolean that indicates whether it is enabled upon registration.
 *
 *  The names of the keys under this path may either be a schema's full name or a wildcard
 *  string that matches to zero or more schema names.  In either version, the case of the
 *  non-wildcard portions of the key name is important.  The wildcard characters '*' (match
 *  to zero or more characters) and '?' (match to exactly one character) may be used.  This
 *  is only meant to be a simple wildcard filter, not a full regular expression.
 *
 *  For example, in a TOML file, these settings may be used to disable or enable multiple
 *  schemas:
 *  @rst
    .. code-block:: toml

        [structuredLog.state.schemas]
        "omni.test_schema" = false  # disable 'omni.test_schema' on registration.
        "omni.other_schema" = true  # enable 'omni.other_schema' on registration.
        "carb.*" = false            # disable all schemas starting with 'carb.'.

    @endrst
 *
 *  @note The keys in this setting path are inherently unordered.  If a set of dependent
 *  enable/disable settings is needed, the @ref kSchemasStateArraySetting setting path
 *  should be used instead.  This other setting allows an array to be specified that
 *  preserves the order of keys.  This is useful for doing things like disabling all
 *  schemas then only enabling a select few.
 */
constexpr const char* kSchemasStateListSetting = "/structuredLog/state/schemas";

/** The setting path that will contain zero or more keys that will be used to disable events
 *  when they are first registered.  Each key under this setting will have a name that matches
 *  zero or event names.  From a .schema file, this would match the "namespace" property plus
 *  one of the properties under @a `#/events/`.  From a JSON schema file, this would match one
 *  of the event properties under @a `#/definitions/events/`.  The key's value is expected to
 *  be a boolean that indicates whether it is enabled upon registration.
 *
 *  The names of the keys under this path may either be an event's full name or a wildcard
 *  string that matches to zero or more event names.  In either version, the case of the
 *  non-wildcard portions of the key name is important.  The wildcard characters '*' (match
 *  to zero or more characters) and '?' (match to exactly one character) may be used.  This
 *  is only meant to be a simple wildcard filter, not a full regular expression.
 *
 *  For example, in a TOML file, these settings may be used to disable or enable multiple
 *  events:
 *  @rst
    .. code-block:: toml

        [structuredLog.state.events]
        "com.nvidia.omniverse.fancy_event" = false
        "com.nvidia.carbonite.*" = false            # disable all 'com.nvidia.carbonite' events.

    @endrst
 *
 *  @note The keys in this setting path are inherently unordered.  If a set of dependent
 *  enable/disable settings is needed, the @ref kEventsStateArraySetting setting path
 *  should be used instead.  This other setting allows an array to be specified that
 *  preserves the order of keys.  This is useful for doing things like disabling all
 *  events then only enabling a select few.
 */
constexpr const char* kEventsStateListSetting = "/structuredLog/state/events";

/** The setting path to an array that will contain zero or more values that will be used to
 *  disable or enable schemas when they are first registered.  Each value in this array will
 *  have a name that matches zero or more schema names.  From a .schema file, this would match the
 *  "name" property.  From a JSON schema file, this would match the @a `#/schemaMeta/clientName`
 *  property.  The schema name may be optionally prefixed by either '+' or '-' to enable or
 *  disable (respectively) matching schemas.  Alternatively, the schema's name may be assigned
 *  a boolean value to indicate whether it is enabled or not.  If neither a '+'/'-' prefix nor
 *  a boolean assignment suffix is specified, 'enabled' is assumed.
 *
 *  The names in this array either be a schema's full name or a wildcard string that matches
 *  to zero or more schema names.  In either version, the case of the non-wildcard portions
 *  of the key name is important.  The wildcard characters '*' (match to zero or more characters)
 *  and '?' (match to exactly one character) may be used.  This is only meant to be a simple
 *  wildcard filter, not a full regular expression.
 *
 *  For example, in a TOML file, these settings may be used to disable or enable multiple
 *  schemas:
 *  @rst
    .. code-block:: toml

        structuredLog.schemaStates = [
            "-omni.test_schema",        # disable 'omni.test_schema' on registration.
            "omni.other_schema = true", # enable 'omni.other_schema' on registration.
            "-carb.*"                   # disable all schemas starting with 'carb.'.
        ]

    @endrst
 *
 *  @note TOML does not allow static arrays such as above to be appended to with later lines.
 *  Attempting to do so will result in a parsing error.
 */
constexpr const char* kSchemasStateArraySetting = "/structuredLog/schemaStates";

/** The setting path to an array that will contain zero or more values that will be used to
 *  disable or enable events when they are first registered.  Each value in this array will
 *  have a name that matches zero or more event names.  From a .schema file, this would match one
 *  of the property names under `#/events/`.  From a JSON schema file, this would match one
 *  of the event object names in @a `#/definitions/events/`.  The event name may be optionally
 *  prefixed by either '+' or '-' to enable or disable (respectively) matching event(s).
 *  Alternatively, the event's name may be assigned a boolean value to indicate whether it is
 *  enabled or not.  If neither a '+'/'-' prefix nor a boolean assignment suffix is specified,
 *  'enabled' is assumed.
 *
 *  The names in this array either be an event's full name or a wildcard string that matches
 *  to zero or more event names.  In either version, the case of the non-wildcard portions
 *  of the array entry name is important.  The wildcard characters '*' (match to zero or more characters)
 *  and '?' (match to exactly one character) may be used.  This is only meant to be a simple
 *  wildcard filter, not a full regular expression.
 *
 *  For example, in a TOML file, these settings may be used to disable or enable multiple
 *  schemas:
 *  @rst
    .. code-block:: toml

        structuredLog.schemaStates = [
            "-com.nvidia.omniverse.fancy_event",
            "com.nvidia.carbonite.* = false"            # disable all 'com.nvidia.carbonite' events.
        ]

    @endrst
 *
 *  @note that TOML does not allow static arrays such as above to be appended to with later lines.
 *  Attempting to do so will result in a parsing error.
 */
constexpr const char* kEventsStateArraySetting = "/structuredLog/eventStates";

/** The setting path that will contain the minimum number of seconds between heartbeat events
 *  A heartbeat event is one that is sent periodically to help calculate session lengths even
 *  if the expected 'exit' or 'crash' process lifetime events are missing.  These events can
 *  be missing in such unavoidable cases as a power loss, the user killing the process, a blue
 *  screen of death or kernel panic, etc.  These situations cannot be assumed as either crashes
 *  or normal exits, but at least these events can help indicate how long the actual session was.
 *  This can be set to 0 to disable the heartbeat messages.  By default the heartbeat events are
 *  disabled (ie: set to @ref kHeartbeatDisabled).
 */
constexpr const char* kHeartbeatPeriodSetting = "/structuredLog/heartbeatPeriod";
/** @} */


/** Enables or disables the structured logging log message redirection.
 *
 *  @param[in] enabled  Set to ``true`` to enable structured logging log message redirection.
 *                      Set to ``false`` to disable structured logging log message redirection.
 *  @returns ``true`` if logging redirection was successfully enabled.  Returns ``false``
 *           otherwise
 *
 *  This enables or disables structured logging log message redirection.  This system
 *  will monitor log messages and emit them as structured log messages.
 */
inline bool setStructuredLogLoggingEnabled(bool enabled = true)
{
    omni::core::ObjectPtr<IStructuredLog> strucLog;
    omni::core::ObjectPtr<IStructuredLogFromILog> log;


    strucLog = omni::core::borrow(omniGetStructuredLogWithoutAcquire());

    if (strucLog.get() == nullptr)
        return false;

    log = strucLog.as<IStructuredLogFromILog>();

    if (log.get() == nullptr)
        return false;

    if (enabled)
        log->enableLogging();

    else
        log->disableLogging();

    return true;
}

/** Checks the settings registry for structured log settings and makes them active.
 *
 *  @param[in] settings     The settings interface to use to retrieve configuration values.
 *                          This may not be `nullptr`.
 *  @returns No return value.
 *
 *  @remarks This sets appropriate default values for all the structured log related settings then
 *           attempts to retrieve their current values and set them as active.  This assumes
 *           that the settings hive has already been loaded from disk and made active in the
 *           main settings registry.
 *
 *  @thread_safety This call is thread safe.
 */
inline void configureStructuredLogging(carb::settings::ISettings* settings)
{
    omni::core::ObjectPtr<IStructuredLog> strucLog;
    omni::core::ObjectPtr<IStructuredLogSettings> ts;
    omni::core::ObjectPtr<IStructuredLogSettings2> ts2;
    omni::core::ObjectPtr<IStructuredLogFromILog> log;
    const char* value;
    int64_t count;
    IdMode idMode = kDefaultIdMode;
    IdType idType = kDefaultIdType;


    if (settings == nullptr)
        return;

    // ****** set appropriate defaults for each setting ******
    settings->setDefaultBool(kGlobalEnableSetting, false);
    settings->setDefaultString(kLogDirectory, "");
    settings->setDefaultString(kDefaultLogNameSetting, "");
    settings->setDefaultInt64(kLogRetentionCountSetting, kDefaultLogRetentionCount);
    settings->setDefaultInt64(kLogSizeLimitSetting, kDefaultLogSizeLimit / 1048576);
    settings->setDefaultInt64(kEventQueueSizeSetting, kDefaultEventQueueSize / 1024);
    settings->setDefaultString(kEventIdModeSetting, "fast-sequential");
    settings->setDefaultString(kEventIdTypeSetting, "UUID");
    settings->setDefaultBool(kEnableLogConsumerSetting, false);
    settings->setDefaultInt64(kHeartbeatPeriodSetting, kDefaultHeartbeatPeriod);


    // ****** grab the structured log settings object so the config can be set ******
    strucLog = omni::core::borrow(omniGetStructuredLogWithoutAcquire());

    if (strucLog.get() == nullptr)
        return;

    ts = strucLog.as<IStructuredLogSettings>();

    if (ts.get() == nullptr)
        return;


    // ****** retrieve the settings and make them active ******
    strucLog->setEnabled(omni::structuredlog::kBadEventId, omni::structuredlog::fEnableFlagAll,
                         settings->getAsBool(kGlobalEnableSetting));

    // set the default log name.
    value = settings->getStringBuffer(kDefaultLogNameSetting);

    if (value != nullptr && value[0] != 0)
        ts->setLogDefaultName(value);

    value = settings->getStringBuffer(kLogDirectory);

    if (value != nullptr && value[0] != 0)
        ts->setLogOutputPath(value);

    // set the log retention count.
    count = settings->getAsInt64(kLogRetentionCountSetting);
    ts->setLogRetentionCount(count);

    // set the log size limit.
    count = settings->getAsInt64(kLogSizeLimitSetting);
    ts->setLogSizeLimit(count * 1048576);

    // set the event queue size.
    count = settings->getAsInt64(kEventQueueSizeSetting);
    ts->setEventQueueSize(count * 1024);

    // set the event ID mode.
    value = settings->getStringBuffer(kEventIdModeSetting);

    if (carb::extras::compareStringsNoCase(value, "fast-sequential") == 0)
        idMode = IdMode::eFastSequential;

    else if (carb::extras::compareStringsNoCase(value, "sequential") == 0)
        idMode = IdMode::eSequential;

    else if (carb::extras::compareStringsNoCase(value, "random") == 0)
        idMode = IdMode::eRandom;

    else
        OMNI_LOG_WARN("unknown event ID mode '%s'.  Assuming 'fast-sequential'.", value);

    // set the event ID type.
    value = settings->getStringBuffer(kEventIdTypeSetting);

    if (carb::extras::compareStringsNoCase(value, "UUID") == 0)
        idType = IdType::eUuid;

    else if (carb::extras::compareStringsNoCase(value, "uint64") == 0)
        idType = IdType::eUint64;

    else
        OMNI_LOG_WARN("unknown event ID type '%s'.  Assuming 'UUID'.", value);

    ts->setEventIdMode(idMode, idType);

    // load the privacy settings and set the user ID from it.
    ts->loadPrivacySettings();

    // load the enable states for each schema and event.
    ts->enableSchemasFromSettings();

    value = omni::extras::PrivacySettings::getUserId();

    if (value != nullptr && value[0] != 0)
        ts->setUserId(value);

    // setup the structured log logger.
    log = strucLog.as<IStructuredLogFromILog>();

    if (log.get() == nullptr)
        return;

    if (settings->getAsBool(kEnableLogConsumerSetting))
        log->enableLogging();

    // setup the default heartbeat event period.
    ts2 = strucLog.as<IStructuredLogSettings2>();
    strucLog.release();

    if (ts2 != nullptr)
    {
        count = settings->getAsInt64(kHeartbeatPeriodSetting);
        ts2->setHeartbeatPeriod(count);
    }
}

} // namespace structuredlog
} // namespace omni
