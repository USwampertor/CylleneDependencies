// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Utilities for uploading telemetry events.
 */
#pragma once

#include "IStructuredLogSettings.h"
#include "../extras/UniqueApp.h"
#include "../../carb/launcher/ILauncher.h"
#include "../../carb/launcher/LauncherUtils.h"
#include "../../carb/settings/ISettings.h"

namespace omni
{
namespace telemetry
{

/** Telemetry transmitter app settings.  These control the behavior of the transmitter
 *  app.  They can be specified either on the command line (prefixed by '--'), or in
 *  the transmitter app's config file (`omni.telemetry.transmitter.toml`).
 *  @{
 */
/** This setting will cause the transmitter to stay alive until it is manually killed.
 *  This setting is mostly meant for developer use, but could also be used in server
 *  or farm environments where Omniverse apps are frequently run and exited.  This will
 *  bypass the normal exit condition check of testing whether all apps that tried to
 *  launch the transmitter have exited on their own.  When this option is disabled,
 *  once all apps have exited, the transmitter will exit on its own.  When this option
 *  is enabled, this exit condition will simply be ignored.  This defaults to `false`.
 *
 *  Note that if no schemas are available for the transmitter to process, this option
 *  is ignored.  In that case, the transmitter will simply exit immediately after
 *  trying to download the schemas package.
 */
constexpr const char* const kStayAliveSetting = "/telemetry/stayAlive";

/** The time, in seconds, that the transmitter will wait between polling the log files.
 *  This determines how reactive the transmitter will be to checking for new messages and
 *  how much work it potentially does in the background.  This defaults to 60 seconds.
 */
constexpr const char* const kPollTimeSetting = "/telemetry/pollTime";

/** The mode to run the transmitter in.  The value of this setting can be either "dev" or "test".
 *  By default, the transmitter will run in "production" mode.  In "dev" mode, the transmitter
 *  will use the default development schemas URL.  In "test" mode, the default staging endpoint
 *  URL will be used.  The "test" mode is only supported in debug builds.  If "test" is used
 *  in a release build, it will be ignored and the production endpoint will be used instead.
 *
 *  Note that if the schemas package for the requested mode is empty, the transmitter will
 *  just immediately exit since there is nothing for it to process.
 */
constexpr const char* const kTelemetryModeSetting = "/telemetry/mode";

/** This allows the transmitter to run as the root user on Linux.
 *  The root user is disabled by default because it could make some of the transmitter's
 *  files non-writable by regular users.  By default, if the transmitter is launched as
 *  the root user or with `sudo`, it will report an error and exit immediately.  If it is
 *  intended to launch as the root or super user, this option must be explicitly specified
 *  so that there is a clear intention from the user.  The default value is `false`.  This
 *  setting is ignored on Windows.
 */
constexpr const char* const kAllowRootSetting = "/telemetry/allowRoot";

/** The list of restricted regions for the transmitter.  If the transmitter is launched
 *  in one of these regions, it will exit immediately on startup.  The entries in this
 *  list are either the country names or the two-letter ISO 3166-1 alpha-2 country code.
 *  Entries in the list are separated by commas (','), bars ('|'), slash ('/'), or whitespace.
 *  Whitespace should not be used when specifying the option on the command line.  It is
 *  typically best to use the two-letter country codes in the list since they are standardized.
 *  This defaults to an empty list.
 */
constexpr const char* const kRestrictedRegionsSetting = "/telemetry/restrictedRegions";

/** The assumption of success or failure that should be assumed if the country name and code
 *  could not be retrieved.  Set this to `true` if the transmitter should be allowed to run
 *  if the country code or name could not be retrieved.  Set this to `false` if the transmitter
 *  should not be allowed to run in that error case.  This defaults to `true`.
 */
constexpr const char* const kRestrictedRegionAssumptionSetting = "/telemetry/restrictedRegionAssumption";

/** The log file path that will be used for any transmitter processes launched.
 *  If it is not specified, the log path from @ref IStructuredLogSettings::getLogOutputPath()
 *  will be used; If @ref IStructuredLogSettings could not be acquired for some reason,
 *  the CWD will be used.
 */
constexpr const char* const kLogFileSetting = "/telemetry/log/file";

/** The log level that will be used for any transmitter process launched.
 *  If this is not specified, the parent process's log level will be used.
 */
constexpr const char* const kLogLevelSetting = "/telemetry/log/level";


/** This settings key holds an object or an array of objects.
 *  Each object is one transmitter instance.
 *  In the settings JSON, this would look something like:
 *  @code{JSON}
 *  {
 *      "telemetry": {
 *          "transmitter": [
 *              {
 *                  "endpoint": "https://telemetry.not-a-real-url.nvidia.com",
 *              },
 *              {
 *                  "endpoint": "https://metrics.also-not-a-real-url.nvidia.com"
 *              }
 *          ]
 *      }
 *  }
 *  @endcode
 *  You can also specify this on command line like this.
 *  You must ensure that all entries under `/telemetry/transmitter` are contiguous numeric indices
 *  otherwise `/telemetry/transmitter` will be interpreted as a single object.
 *  @code{bash}
 *  "--/telemetry/transmitter/0/endpoint=https://omniverse.kratos-telemetry.nvidia.com" \
 *  "--/telemetry/transmitter/1/endpoint=https://gpuwa.nvidia.com/dataflow/sandbox-jfeather-omniverse/posting"
 *  @endcode
 *  A transmitter instance sends data to a telemetry endpoint with a specific
 *  configuration.
 *  Each instance can be configured to use a different protocol and a different
 *  set of schema URLs, so data sent to each endpoint can be substantially different.
 *  To send data to multiple endpoints, you set up an array of objects under
 *  this settings key.
 *  Note that
 */
constexpr const char* const kTransmitterSetting = "/telemetry/transmitter";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  If this is set to `true`, the transmitter will ignore the `seek` field in the header
 *  and start parsing from the start of each log again.  This is only intended to be used
 *  for testing purposes.  This defaults to `false`.
 */
constexpr const char* const kResendEventsSettingLeaf = "resendEvents";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  The maximum number of bytes to send in one transmission to the server.  Event messages
 *  will be sent in batches as large as possible up to this size.  If more message data is
 *  available than this limit, it will be broken up into multiple transmission units.  This
 *  must be less than or equal to the transmission limit of the endpoint the messages are
 *  being sent to.  If this is larger than the server's limit, large message buffers will
 *  simply be rejected and dropped.  This defaults to 10MiB.
 */
constexpr const char* const kTransmissionLimitSettingLeaf = "transmissionLimit";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  This sets the maximum number of messages to process in a single pass on this transmitter.
 *  The transmitter will stop processing log messages and start to upload messages when it
 *  has found, validated, processed, and queued at most this number of messages on any given
 *  transmitter object.  Only validated and queued messages will count toward this limit.
 *  This limit paired with @ref kTransmissionLimitSettingLeaf helps to limit how much of any
 *  log file is processed and transmitted at any given point.  This defaults to 10000 messages.
 */
constexpr const char* const kQueueLimitSettingLeaf = "queueLimit";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  Sets the URL to send the telemetry events to.  This can be used to send the events to a
 *  custom endpoint.  This will override the default URL inferred by @ref kTelemetryModeSetting.
 *  By default, the @ref kTelemetryModeSetting setting will determine the endpoint URL to use.
 *  You can set this as an array if you want to specify multiple fallback endpoints
 *  to use if the retry limit is exhausted; each endpoint will be tried in order
 *  and will be abandoned if connectivity fails after the retry limit.
 */
constexpr const char* const kTelemetryEndpointSettingLeaf = "endpoint";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  This specifies the URL or URLs to download approved schemas from.  This may be used to
 *  override the default URL and the one specified by @ref kTelemetryModeSetting.  This may
 *  be treated either as a single URL string value or as an array of URL strings.  The usage
 *  depends on how the setting is specified by the user or config.  If an array of URLs is
 *  given, they are assumed to be in priority order starting with the first as the highest
 *  priority URL.  The first one that successfully downloads will be used as the schema
 *  package.  The others are considered to be backup URLs.  This will be overridden by both
 *  @ref kTelemetrySchemaFileSetting and @ref kTelemetrySchemasDirectorySetting.  This
 *  defaults to the Omniverse production schemas URL.
 *
 *  @note In debug builds, this schemas package URL may also point to a local file.  This
 *        may be specified in both the single string and array forms of this setting.  The
 *        local file URI may be in the `file://` scheme form or as just a local (absolute)
 *        or relative) filename.
 */
constexpr const char* const kTelemetrySchemasUrlSettingLeaf = "schemasUrl";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  This specifies that authentication should be enabled when sending event messages to the
 *  telemetry endpoint.  When disabled, this will prevent the auth token from being retrieved
 *  from the Omniverse Launcher app.  This should be used in situations where the Omniverse
 *  Launcher app is not running or an endpoint that does not need or expect authorization is
 *  used.  If this is not used and the auth token cannot be retrieved from the Launcher app, the
 *  transmitter will go into an idle mode where nothing is processed or sent.  This mode is
 *  expecting the Launcher app to become available at some future point.  Setting this option
 *  to `false` will disable the authentication checks and just attempt to push the events to
 *  the specified telemetry endpoint URL.  Note that the default endpoint URL will not work
 *  if this option is used since it expected authentication.  The default value is `true`.
 */
constexpr const char* const kTelemetryAuthenticateSettingLeaf = "authenticate";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  This specifies the URL to download the authentication token from.  This option will be
 *  ignored if @ref kTelemetryAuthenticateSetting is `false`.  A file will be expected to
 *  be downloaded from this URL.  The downloaded file is expected to be JSON formatted and
 *  is expected to contain the authentication token in the format that the authentication
 *  server expects.  The name of the data property in the file that contains the actual
 *  token to be sent with each message is specified in @ref kTelemetryAuthTokenKeyNameSetting.
 *  The data property in the file that contains the token's expiry date is specified in
 *  @ref kTelemetryAuthTokenExpiryNameSetting.  Alternatively, this setting may also point
 *  to a file on disk (either with the 'file://' protocol or by naming the file directly).
 *  If a file on disk is named, it is assumed to either be JSON formatted and also contain
 *  the token data and expiry under the same key names as given with
 *  @ref kTelemetryAuthTokenKeyNameSetting and @ref kTelemetryAuthTokenExpiryNameSetting,
 *  or it will be a file whose entire contents will be the token itself.  The latter mode
 *  is only used when the @ref kTelemetryAuthTokenKeyNameSetting setting is an empty string.
 *  By default, the URL for the Omniverse Launcher's authentication web API will be used.
 */
constexpr const char* const kTelemetryAuthTokenUrlSettingLeaf = "authTokenUrl";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  This specifies the name of the key in the downloaded authentication token's JSON data
 *  that contains the actual token data to be sent with each set of uploaded messages.
 *  This option will be ignored if @ref kTelemetryAuthenticateSetting is `false`.
 *  This must be in the format that the authentication server expects.  The token data
 *  itself can be any length, but is expected to be string data in the JSON file.  The
 *  key is looked up in a case sensitive manner.  This defaults to "idToken".
 */
constexpr const char* const kTelemetryAuthTokenKeyNameSettingLeaf = "authTokenKeyName";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  This specifies the name of the key in the downloaded authentication token's JSON data
 *  that contains the optional expiry date for the token.  This option will be ignored if
 *  @ref kTelemetryAuthenticateSetting is `false`.If this property exists, it will be used
 *  to predict when the token should be retrieved again.  If this property does not exist,
 *  the token will only be retrieved again if sending a message results in a failure due
 *  to a permission error.  This property is expected to be string data in the JSON
 *  file.  Its contents must be formatted as an RFC3339 date/time string.  This defaults to
 *  "expires".
 */
constexpr const char* const kTelemetryAuthTokenExpiryNameSettingLeaf = "authTokenExpiryName";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  What serialization protocol is being used by the transmitter.
 *  This can be set to one of three possible (case-insensitive) values:
 *    - "default":  This is the default protocol.  The event will not be modified from its
 *                  original generated format and content.  This includes leaving the `data`
 *                  field as a nested JSON object.  This will send events in batches (as many
 *                  as are available at the time) to the server with each JSON event separated
 *                  by a newline (aka JSON lines format).
 *    - "defaultWithStringify": This sends batches of events at a time to the server with each
 *                  JSON event separated by a newline (aka JSON lines format).  Structuredlog
 *                  event format is largely left as-is; the only change is that the `data`
 *                  field's object is stringified.  This is the only difference between this
 *                  protocol and the @ref `default` protocol above.
 *    - "NVDF":     This is also a batch serialization protocol, except that the event property
 *                  names are modified to follow the expectations of the server.
 *                  This flattens each event's `data` payload into the event base.
 *                  The `time` field is renamed to `ts_created` and its format
 *                  is changed to a *nix epoch time in milliseconds.
 *                  The `id` field is renamed to `_id`.
 *                  All data fields are also prefixed in a Hungarian-notation
 *                  style system.
 */
constexpr const char* const kEventProtocolLeaf = "eventProtocol";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  The tag name that will be used to record how much of the file was processed.
 *  Unless `kResendEventsSetting` is set to `true`, the seek tags are used to
 *  tell the transmitter how much of the log has been processed so far so that
 *  it won't try to send old events again.
 *  The default value of this is "seek".
 *  You may want to change the seek tag name if you need to send data to multiple
 *  telemetry endpoints with separate transmitter processes.
 *
 *  @note The telemetry log files have a 256 byte header.
 *        Each seek tag will add 13 bytes to the header + the length of the tag
 *        name.
 *        When the log header space is exhausted, the header will not be updated,
 *        which is equivalent to having `kResendEventsSetting` is set to `true`.
 *        Please avoid long tag names to avoid this risk.
 */
constexpr const char* const kSeekTagNameLeaf = "seekTagName";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  This specifies the expected type of the authentication token.  This can be any of
 *  "auto", "jwt", or "api-key".  By default this is set to "auto" and will attempt to
 *  detect the type of authentication token based on where the @ref kTelemetryAuthTokenUrlSetting
 *  setting points.  If the value points to a URL, a JWT will be assumed.  If the value points
 *  to a file on disk or directly contains the token itself, a long-lived API key will be assumed.
 *  If an alternate storage method is needed but the assumed type doesn't match the actual
 *  token type, this setting can be used to override the auto detection.  The default value is
 *  "auto".  This setting will be ignored if @ref kTelemetryAuthenticateSetting is set to false.
 */
constexpr const char* const kTelemetryAuthTokenTypeLeaf = "authTokenType";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  The number of days before the current time where events in a log start to be considered old.
 *  When set to 0 (and this setting isn't overridden on a per-schema or per-event level), all
 *  events are processed as normal and no events are considered old.  When set to a non-zero
 *  value, any events that are found to be older than this number of days will be processed
 *  differently depending on the flags specified globally (@ref kIgnoreOldEventsSetting and
 *  @ref kPseudonymizeOldEventsSetting), at the schema level (``fSchemaFlagIgnoreOldEvents``
 *  and ``fSchemaFlagPseudonymizeOldEvents``), and at the event level (``fEventFlagIgnoreOldEvents``
 *  and ``fEventFlagPseudonymizeOldEvents``).  If no special flags are given, the default
 *  behavior is to anonymize old events before transmitting them.  This will only override the
 *  old events threshold given at the schema (``#/oldEventsThreshold``) or event
 *  (``#/events/<eventName>/oldEventsThreshold``) if it is a smaller non-zero value than the
 *  values given at the lower levels.  This defaults to 0 days (ie: disables checking for 'old'
 *  events).
 */
constexpr const char* const kOldEventThresholdSettingLeaf = "oldEventThreshold";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  Flag to indicate that when an old event is detected, it should simply be discarded instead
 *  of being anonymized or pseudonymized before transmission.  This is useful if processing
 *  old events is not interesting for analysis or if transmitting an old event will violate a
 *  data retention policy.  An event is considered old if it is beyond the 'old events threshold'
 *  (see @ref kOldEventThresholdSetting).  By default, old events will be anonymized before being
 *  transmitted.  If this flag is set here at the global level, it will override any old event
 *  settings from the schema and event levels.
 */
constexpr const char* const kIgnoreOldEventsSettingLeaf = "ignoreOldEvents";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  Flag to indicate that when an old event is detected, it should be pseudonymized instead of
 *  anonymized before transmission.  This setting is ignored for any given event if the
 *  @ref kIgnoreOldEventsSetting setting, ``fSchemaFlagIgnoreOldEvents`` flag, or
 *  ``fEventFlagIgnoreOldEvents`` flag is used for that event.  When not specified, the default
 *  behavior is to anonymize old events before transmission.
 */
constexpr const char* const kPseudonymizeOldEventsSettingLeaf = "pseudonymizeOldEvents";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  The number of attempts to transmit data that will be made before giving up.
 *  This can be set to -1 to retry forever.
 *  This setting is only important in a multi-endpoint context; if one endpoint
 *  goes offline, a retry limit will allow the transmitter to give up and start
 *  sending data to other endpoints.
 *  There is an exponential backoff after every retry, so the first retry will
 *  occur after 1 second, then 2 second, then 4 seconds, etc.
 *  The backoff takes the transmission time into account, so the 4 second backoff
 *  would only wait 1 second if the failed transmission took 3 seconds.
 *  A setting of 5 will roughly wait for 1 minute in total.
 *  A setting of 11 will roughly wait for 1 hour in total.
 *  After the 12th wait, wait time will no longer increase, so each subsequent
 *  wait will only last 4096 seconds.
 */
constexpr const char* const kRetryLimitSettingLeaf = "retryLimit";

/** This setting is an optional member of the @ref kTransmitterSetting object.
 *  This controls which messages will be considered by the transmitter and
 *  which will be filtered out before validating against a schema.  This is
 *  intended to match each message's transmission mode against the mode that
 *  the transmitter is currently running in.  This value can be one of the
 *  following:
 *   * "all": allow all messages to be validated against schemas.
 *   * "matching": allow messages whose source mode matches the transmitter's
 *          current run mode.
 *   * "matching+": allow messages whose source mode matches the transmitter's
 *          current run mode or higher.
 *   * "test": only consider messages from the 'test' source mode.
 *   * "dev": only consider messages from the 'dev' source mode.
 *   * "prod": only consider messages from the 'prod' source mode.
 *   * "dev,prod" (or other combinations): only consider messages from the listed
 *          source modes.
 *
 *  This defaults to "all".  This setting value may be used either on a
 *  per-transmitter basis when using multiple transmitters, or it may be used in
 *  the legacy setting `/telemetry/messageMatchMode`.  If both the legacy and the
 *  per-transmitter setting are present, the per-transmitter ones will always
 *  override the legacy setting.
 *
 *  @note Setting this to a value other than "all" would result in certain messages
 *        not being transmitted because they were rejected during a previous run and
 *        were skipped over in the log.  It is intended that if this setting is used
 *        for a given transmitter, it is likely to be used with that same matching
 *        mode most often on any given machine.  Note that multiple transmitters with
 *        different matching modes can also be used.
 */
constexpr const char* const kMessageMatchingModeLeaf = "messageMatchMode";
/** @} */


/** Create a guard with the standard name and path for the transmitter.
 *  @returns A UniqueApp guard that is used to let the transmitter know when
 *           it should exit on its own.  Telemetry clients should use this to
 *           'connect' to the transmitter (note that this does not require the
 *           transmitter to be running yet). See launchTransmitter() for an
 *           example usage.
 *  @remarks This guard is needed by all telemetry clients to let the transmitter
 *           know when all clients have shut down so it can exit as well.
 */
inline extras::UniqueApp createGuard()
{
    constexpr const char* kLaunchGuardNameSetting = "/telemetry/launchGuardName";
    carb::settings::ISettings* settings;
    const char* guardName = "omni.telemetry.transmitter";
    auto getGuardPath = []() -> std::string {
#if OMNI_PLATFORM_LINUX
        // XDG_RUNTIME_DIR is the standard place for putting runtime IPC objects on Linux
        const char* runDir = getenv("XDG_RUNTIME_DIR");
        if (runDir != nullptr)
        {
            return runDir;
        }
#endif

        core::ObjectPtr<omni::structuredlog::IStructuredLog> strucLog;
        core::ObjectPtr<omni::structuredlog::IStructuredLogSettings> settings;

        strucLog = omni::core::borrow(omniGetStructuredLogWithoutAcquire());
        if (strucLog.get() == nullptr)
        {
            OMNI_LOG_ERROR("failed to get IStructuredLog");
            return "";
        }

        settings = strucLog.as<structuredlog::IStructuredLogSettings>();
        if (settings.get() == nullptr)
        {
            OMNI_LOG_ERROR("failed to get IStructuredLogSettings");
            return "";
        }

        return settings->getLogOutputPath();
    };

    settings = carb::getCachedInterface<carb::settings::ISettings>();

    if (settings != nullptr && settings->isAccessibleAs(carb::dictionary::ItemType::eString, kLaunchGuardNameSetting))
        guardName = settings->getStringBuffer(kLaunchGuardNameSetting);

    return extras::UniqueApp(getGuardPath().c_str(), guardName);
}

/** Determine the directory where the logs of a launched transmitter process should go.
 *  @returns The log output directory.
 */
inline std::string getTransmitterLogPath()
{
    constexpr const char* kLogFileFallback = "./omni.telemetry.transmitter.log";
    const char* logFile = nullptr;
    carb::settings::ISettings* settings = carb::getFramework()->tryAcquireInterface<carb::settings::ISettings>();
    core::ObjectPtr<omni::structuredlog::IStructuredLog> strucLog =
        omni::core::borrow(omniGetStructuredLogWithoutAcquire());
    core::ObjectPtr<omni::structuredlog::IStructuredLogSettings> strucLogSettings;

    if (settings != nullptr)
    {
        settings->setDefaultString(kLogFileSetting, "");
        logFile = settings->getStringBuffer(kLogFileSetting);
        if (logFile != nullptr && logFile[0] != '\0')
            return logFile;
    }
    else
    {
        CARB_LOG_ERROR("failed to acquire ISettings");
    }

    if (strucLog.get() == nullptr)
    {
        OMNI_LOG_ERROR("failed to get IStructuredLog - using CWD");
        return kLogFileFallback;
    }

    strucLogSettings = strucLog.as<structuredlog::IStructuredLogSettings>();
    if (strucLogSettings.get() == nullptr)
    {
        OMNI_LOG_ERROR("failed to get IStructuredLogSettings - using CWD for the log file");
        return kLogFileFallback;
    }

    return std::string(strucLogSettings->getLogOutputPath()) + "/omni.telemetry.transmitter.log";
}

/** Launch the telemetry transmitter process.
 *  @param[in] transmitterPath The path to the transmitter binary and .py file.
 *  @param[in] pythonHome The path to your python install directory.
 *  @param[in] pythonPath The path to the Carbonite python bindings.
 *  @param[in] extraArgs Extra arguments to pass to the transmitter process.
 *  @returns `true` if the transmitter child process is successfully launched.
 *           Returns `false` otherwise.
 *
 *  @remarks This function will launch an instance of the telemetry transmitter.
 *           Any app using telemetry should launch the transmitter.
 *           The transmitter process will upload telemetry events from the logs
 *           to the Kratos endpoint.
 *           The transmitter is currently dependent on the Omniverse Launcher to
 *           obtain an authentication token to send telemetry.
 *           Only one transmitter process will exist on a system at a time; if
 *           one is launched while another exists, the new instance will exit.
 *           The transmitter process will run until all telemetry client
 *           processes have exited.
 *
 *  @note The `/telemetry/log` tree of settings exists to specify log options for
 *        the transmitter without affecting the parent process.
 *         - `/telemetry/log/file` is passed to the transmitter as `/log/file'.
 *         - `/telemetry/log/level` is passed to the transmitter as `/log/level'.
 *
 *  @note This transmitter process won't log to the standard streams by default.
 *        Logs will go to a standard log file path instead, but `--/log/file` can
 *        be used to override this.
 *        The transmitter process will exit after the host app, so logging to
 *        the standard streams will result in messages being written into terminal
 *        over the user's prompt.
 */
inline bool launchTransmitter(const char* transmitterPath, const std::vector<std::string> extraArgs = {})
{
#if OMNI_PLATFORM_WINDOWS
    const char* ext = ".exe";
#else
    const char* ext = "";
#endif
    carb::launcher::ILauncher* launcher = carb::getFramework()->tryAcquireInterface<carb::launcher::ILauncher>();
    carb::settings::ISettings* settings = carb::getFramework()->tryAcquireInterface<carb::settings::ISettings>();
    extras::UniqueApp guard = createGuard();
    carb::launcher::ArgCollector args;
    carb::launcher::EnvCollector env;
    carb::launcher::LaunchDesc desc = {};
    size_t envCount;
    const char* logLevel = nullptr;

    if (launcher == nullptr)
    {
        OMNI_LOG_ERROR("failed to acquire ILauncher");
        return false;
    }


    guard.connectClientProcess();

    args.format("%s/omni.telemetry.transmitter%s", transmitterPath, ext);

    args.add("/telemetry", "--/", carb::launcher::fSettingsEnumFlagRecursive);
    args.add("/log", "--/", carb::launcher::fSettingsEnumFlagRecursive, [](const char* path, void* ctx) {
        CARB_UNUSED(ctx);
        return strcmp(path, "/log/enableStandardStreamOutput") != 0 && strcmp(path, "/log/file") != 0 &&
               strcmp(path, "/log/level") != 0;
    });

    // output will not go to the standard streams because it's annoying if
    // output goes to your terminal after your process finishes and there is no
    // way to ensure the transmitter shuts down before your process
    args.add("--/log/enableStandardStreamOutput=false");

    args.format("--/log/file=%s", getTransmitterLogPath().c_str());

    if (settings == nullptr)
    {
        OMNI_LOG_ERROR("failed to acquire ISettings");
    }
    else
    {
        settings->setDefaultString(kLogLevelSetting, "");
        logLevel = settings->getStringBuffer(kLogLevelSetting);
        if (logLevel != nullptr && logLevel[0] != '\0')
        {
            args.format("--/log/level=%s", logLevel);
        }
        else
        {
            logLevel = settings->getStringBuffer("/log/level");
            if (logLevel != nullptr && logLevel[0] != '\0')
            {
                args.format("--/log/level=%s", logLevel);
            }
        }
    }

    for (const std::string& arg : extraArgs)
    {
        args.add(arg);
    }

    // add the parent environment but remove the `CARB_APP_PATH` and `CARB_APP_NAME` variables
    // since they will negatively affect the behaviour of the transmitter.  Note that since
    // we're explicitly adding the full parent environment here and just subtracting some
    // variables, we need to also use the `fLaunchFlagNoInheritEnv` flag when launching the
    // child process so that ILauncher doesn't implicitly add them back.
    env.add();
    envCount = env.getCount();
    env.remove("CARB_APP_PATH");
    env.remove("CARB_APP_NAME");

    if (env.getCount() != envCount)
    {
        desc.flags |= carb::launcher::fLaunchFlagAllowBadEnv | carb::launcher::fLaunchFlagNoInheritEnv;
        desc.env = env.getEnv();
        desc.envCount = env.getCount();
    }

    desc.argv = args.getArgs(&desc.argc);
    desc.flags |= carb::launcher::fLaunchFlagNoStdStreams;

    return launcher->launchProcessDetached(desc) != carb::launcher::kBadId;
}


} // namespace telemetry
} // namespace omni
