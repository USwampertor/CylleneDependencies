// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
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

#include "../../carb/settings/ISettings.h"
#include "../core/IObject.h"
#include "../extras/PrivacySettings.h"


namespace omni
{
namespace structuredlog
{

class IStructuredLogSettings;


// ******************************* enums, types, and constants ************************************
/** Base type for a session ID.  This is an integer identifier that is chosen on startup and
 *  remains constant for the entire structured log session.  The session ID will be included in
 *  each message that is sent.
 */
using SessionId = uint64_t;

/** A special name for the default log output path.  This may be used to restore the default
 *  log path at any point with IStructuredLogSettings::setLogOutputPath().  This will never be
 *  returned from IStructuredLogSettings::getLogOutputPath().
 */
constexpr const char* kDefaultLogPathName = nullptr;

/** A special name to request the log file path for the default log (if set).  This may be passed
 *  to @ref omni::structuredlog::IStructuredLogSettings::getLogPathForEvent() instead of a
 *  specific event ID.
 */
constexpr EventId kDefaultLogPathEvent = 0;


/** Base type for the flags for that can be passed in the @a flags parameter to the
 *  @ref omni::structuredlog::IStructuredLogSettings::loadPrivacySettingsFromFile() function.
 */
using PrivacyLoadFlags = uint32_t;

/** Flag to indicate that the privacy settings keys that the privacy settings that could affect
 *  user privacy functionality should be explicitly reset to their default values before loading
 *  the new privacy file.
 */
constexpr PrivacyLoadFlags fPrivacyLoadFlagResetSettings = 0x00000001;


/** Names to control how the next event identifier is generated after each event message. */
enum class OMNI_ATTR("prefix=e") IdMode
{
    /** Each event identifier will be completely random.  There will be no ordering relationship
     *  between any two events.  This mode is useful when a sense of ordering is not needed but
     *  a very small probability of an event identifier collision is needed.
     */
    eRandom,

    /** Each event identifier will be incremented by one from the previous one.  When using a
     *  UUID identifier, this will increment the previous identifier by one starting from the
     *  rightmost value.  When using a 64-bit identifier, this the previous identifier will just
     *  be incremented by one.  This is useful when ordering is important and events may need
     *  to be sorted.
     */
    eSequential,

    /** Each event identifier will be incremented by one from the previous one, but a faster
     *  method will be used.  When using a UUID identifier, this will not produce an easily
     *  sortable set of identifiers, but it will be faster to produce versus the other methods.
     *  When using a 64-bit identifier, this mode is the same as @ref IdMode::eSequential and will
     *  produce an easily sortable set of identifiers.  This is useful when event handling
     *  performance is the most important.
     */
    eFastSequential,
};

/** Names to control what type of event identifiers will be used for each message. */
enum class OMNI_ATTR("prefix=e") IdType
{
    /** Generate a 128 bit UUID identifier.  The probability of an identifier collision between
     *  two events will be especially small with this type, especially when using random
     *  identifiers.  This however does have a small performance penalty to process these
     *  identifiers and could result in the event processing thread getting backed up if
     *  a large number of events are being pushed frequently.
     */
    eUuid,

    /** Generate a 64-bit integer identifier.  The probability of an identifier collision
     *  between two events will be higher but still small with this type, especially when
     *  using random identifiers.  This identifier type is more performant and more easily
     *  sortable however.  This identifier type is useful when event processing performance
     *  is most important.
     */
    eUint64,
};


// ****************************** IStructuredLogSettings interface ********************************
/** Structured log settings interface.  This allows a host app to modify some behavior of the
 *  structured log system for the process or to retrieve information about the current settings.
 *  These settings control features such as the event queue size, log rotation, event ID
 *  generation, the output log path, and the user ID.  Most of the default settings should be
 *  sufficient for most apps with the exception of the user ID.  For host apps that use at
 *  least one non-anonymized schema the only settings that must be set is the user ID.
 *
 *  This interface object can be acquired either by requesting it from the type factory or
 *  by casting an IStructuredLog object to this type.
 */
class IStructuredLogSettings_abi
    : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.structuredlog.IStructuredLogSettings")>
{
protected:
    // ****** host app structured log configuration accessor functions ******
    /** Retrieves the current event queue size.
     *
     *  @returns The current size of the event queue buffer in bytes.  Note that once the first
     *           event message is sent, the event queue will be running and this will continue
     *           to return the 'active' size of the event queue regardless of any new size that
     *           is passed to @ref omni::structuredlog::IStructuredLogSettings::setEventQueueSize().
     *           Any new size will only be made active once the event queue is stopped with
     *           @ref omni::structuredlog::IStructuredLogControl::stop().  The default size is 2MiB.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual size_t getEventQueueSize_abi() noexcept = 0;

    /** Retrieves the current maximum log file size.
     *
     *  @returns The current maximum log file size in bytes.  This helps to control when log files
     *           get rotated out and started anew for the process.  The number of old logs that
     *           are kept is controlled by the log retention count.  The default size is 50MB.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual int64_t getLogSizeLimit_abi() noexcept = 0;

    /** Retrieves the current log retention count.
     *
     *  @returns The maximum number of log files to retain when rotating log files.  When a new
     *           log is rotated out, the oldest one that is beyond this count will be deleted.
     *           The default is 3.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual size_t getLogRetentionCount_abi() noexcept = 0;

    /** Retrieves the current event identifier mode.
     *
     *  @returns The current event identifier mode.  The default is @ref omni::structuredlog::IdMode::eFastSequential.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual IdMode getEventIdMode_abi() noexcept = 0;

    /** Retrieves the current event identifier type.
     *
     *  @returns The current event identifier type.  The default is @ref omni::structuredlog::IdType::eUuid.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual IdType getEventIdType_abi() noexcept = 0;

    /** Retrieves the current log output path.
     *
     *  @returns The path that log files will be written to.  This defaults to the launcher app's
     *           default log directory.  The host app should override that if that location is
     *           not suitable.
     *
     *  @thread_safety This call is thread safe to retrieve the path string.  However, the string
     *                 itself may become invalidated after return if the log output path is
     *                 changed with a call to @ref omni::structuredlog::IStructuredLogSettings::setLogOutputPath().
     *                 It is
     *                 the host app's responsibility to ensure the log output path is not being
     *                 changed while this string is being used.  The returned string generally
     *                 should not be cached anywhere.  It should be retrieved from here any time
     *                 it is needed.
     */
    virtual const char* getLogOutputPath_abi() noexcept = 0;

    /** Retrieves the default log name if any has been set.
     *
     *  @returns The default log file name that will be used to output all events that are not
     *           explicitly marked with the @ref omni::structuredlog::fEventFlagUseLocalLog flag.
     *           This name will include the log output path as set by
     *           @ref omni::structuredlog::IStructuredLogSettings::getLogOutputPath().
     *  @returns `nullptr` if no default log name has been set with
     *           @ref omni::structuredlog::IStructuredLogSettings::setLogDefaultName()
     *           or the default log name has been cleared.
     *
     *  @thread_safety This call itself is thread safe.  However, the returned string is only
     *                 valid until either the log path or the default log name changes.  It is
     *                 the caller's responsibility to ensure the returned string is used safely.
     */
    virtual const char* getLogDefaultName_abi() noexcept = 0;

    /** Retrieves the log path that a given event will be written to.
     *
     *  @param[in] eventId  The ID of the event to retrieve the log path for.  This must be the
     *                      same ID used when the event was registered.  This may also be
     *                      @ref omni::structuredlog::kDefaultLogPathEvent to retrieve the
     *                      path for the default log.
     *  @returns The name and path of the log file that the requested event would go to if it
     *           were emitted with current settings.
     *  @returns the name and path of the default log file if kDefaultLogPathEvent is used for
     *           for the event ID.
     *  @returns `nullptr` if the given event ID is unknown or kDefaultLogPathEvent was used
     *           and no default log name has been set.
     *
     *  @thread_safety This call itself is thread safe.  However, the returned string is only
     *                 valid until either the log path or the default log name changes.  It is
     *                 the caller's responsibility to ensure the returned string is used safely.
     */
    virtual const char* getLogPathForEvent_abi(EventId eventId) noexcept = 0;

    /** Retrieves the current user ID.
     *
     *  @returns The current user ID.  This may be a user name or user identification number.
     *           The specific format and content is left entirely up to the host app itself.
     *           By default, the user ID will either be the user ID listed in the privacy
     *           settings file if present, or a random number if the user ID setting is not
     *           present.  When a random number is used for the user ID, it is useful for
     *           anonymizing user event messages.
     *
     *  @thread_safety This call is thread safe to retrieve the user ID.  However, the string
     *                 itself may become invalidated after return if the user ID is changed with
     *                 a call to to @ref omni::structuredlog::IStructuredLogSettings::setUserId().
     *                 It is the host app's responsibility to ensure the user ID is not being changed while this
     *                 string is being used.  The returned string generally should not be cached
     *                 anywhere.  It should be retrieved from here any time it is needed.
     */
    virtual const char* getUserId_abi() noexcept = 0;

    /** Retrieves the current session ID.
     *
     *  @returns The identifier for the current session if the current privacy settings allow it.
     *
     *  @remarks This retrieves the session ID for the current structured log session.  This ID is
     *           chosen when the structured log session starts and remains constant for its
     *           lifetime.
     */
    virtual SessionId getSessionId_abi() noexcept = 0;

    /** Sets the new event queue buffer size.
     *
     *  @param[in] sizeInBytes  the new event queue buffer size in bytes to use.  This will be
     *                          silently clamped to an acceptable minimum size.  The default is
     *                          1MB.
     *  @returns No return value.
     *
     *  @remarks This sets the new event queue buffer size.  A larger buffer allows more events
     *           to be sent and processed simultaneously in an app.  If the buffer ever fills
     *           up, events may be dropped if they cannot be processed fast enough.  For host apps
     *           that infrequently send small events, some memory can be saved by reducing this
     *           buffer size.  However, host apps that need to be able to send messages frequently,
     *           especially simultaneously from multiple threads, should use a larger buffer size
     *           to allow the event queue to keep up with the incoming event demand.
     *
     *  @remarks Once the event queue is running (ie: the first message has been sent), the size
     *           will not change until the queue is stopped.  Any new size set in here while the
     *           queue is running will still be stored as a 'pending' size that will be made
     *           active when @ref omni::structuredlog::IStructuredLogControl::stop() is called.
     *           However, during the time when the event queue is running, calls to
     *           @ref omni::structuredlog::IStructuredLogSettings::getEventQueueSize()
     *           will still return the queue's active size.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual void setEventQueueSize_abi(size_t sizeInBytes) noexcept = 0;

    /** Sets the log size limit.
     *
     *  @param[in] limitInBytes     The new log size limit in bytes.  This controls the maximum
     *                              size a log file can get up to before it is rotated out and
     *                              a new log is started.  This will be silently clamped to an
     *                              acceptable minimum size.  The default is 50MiB.
     *                              The minimum size limit is 256KiB.
     *  @returns No return value.
     *
     *  @remarks This sets the log size limit.  When a log reaches (approximately) this size,
     *           it will be rotated out and a new log file started.  The number of old log files
     *           that are kept is controlled by the log retention count.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual void setLogSizeLimit_abi(int64_t limitInBytes) noexcept = 0;

    /** Sets the log retention count.
     *
     *  @param[in] count    The maximum number of old log files to maintain.  When a log file
     *                      grows beyond the current size limit, it will be renamed and a new
     *                      log opened with the original name.  The oldest log file may be
     *                      deleted if there are more logs than this count.  The default is 3.
     *  @returns No return value.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual void setLogRetentionCount_abi(size_t count) noexcept = 0;

    /** Sets the current event identifier mode and type.
     *
     *  @param[in] mode     The new identifier mode to use.  This will take effect on the next
     *                      event that is sent.  The default is IdMode::eFastSequential.
     *  @param[in] type     The new identifier type to use.  This will take effect on the next
     *                      event that is sent.  The default is IdType::eUuid.
     *  @returns No return value.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual void setEventIdMode_abi(IdMode mode, IdType type) noexcept = 0;

    /** Sets the new log output path.
     *
     *  @param[in] path     The new log file path to use.  This may not be an empty string.  The
     *                      default is the Launcher app's expected log file path. This may be
     *                      either a relative or absolute path.  However, note that if this is
     *                      relative, the location of the log files may change if the process's
     *                      current working directory ever changes.  It is highly suggested that
     *                      only absolute paths be used.  This may also be
     *                      @ref omni::structuredlog::kDefaultLogPathName or nullptr to set the
     *                      log output path back to its default location.
     *  @returns No return value.
     *
     *  @remarks This changes the log file location for the calling process.  The log file
     *           locations for all registered schemas will be updated as well.  If a schema has
     *           been set to keep its log file open, it will be closed at this point (if already
     *           open).  The next event that is written to the log file will open a new log at
     *           the new location.  The old log file (if any) will not be moved from the previous
     *           location.
     *
     *  @note There is still a possible race condition with changing the log directory if events
     *        are pending for processing.  If the log file had been opened to write out a message
     *        at the same time this is changing the log path, the change in log path will not take
     *        effect until the next event message is being processed.
     *
     *  @thread_safety This call is thread safe.  However, it is the caller's responsibility to
     *                 ensure that no other host thread is simultaneously operating on the log
     *                 output path string returned from
     *                 @ref omni::structuredlog::IStructuredLogSettings::getLogOutputPath()
     *                 and that nothing else has cached that returned string since it could become
     *                 invalidated here.
     */
    virtual void setLogOutputPath_abi(OMNI_ATTR("c_str, in") const char* path) noexcept = 0;

    /** Sets the new default log name to use for message output.
     *
     *  @param[in] name     The new default name to set for the message log file.  This may be
     *                      an empty string or nullptr to restore the default behavior of writing
     *                      each new message to its own schema's log file.  This should just be
     *                      the name of the file and not include any path components.  This may
     *                      also be one of the special values `/dev/stdout` or `/dev/stderr` to
     *                      send the output to only the stdout or stderr streams.  In this case,
     *                      all events are output to these streams even if a given schema uses
     *                      the `fSchemaFlagUseLocalLog` or event uses the `fEventFlagUseLocalLog`
     *                      flag.  Otherwise, if a filename is given and this name contains any
     *                      path components (ie: a '/'), setting the default log name will fail.
     *                      The log output path should be set using
     *                      @ref omni::structuredlog::IStructuredLogSettings::setLogOutputPath() instead.
     *                      If this name does not have a file
     *                      extension, a ".log" extension will be added.  If the ".log" extension
     *                      is not desired, this may be suppressed by ending the name in ".".  In
     *                      this case, the final trailing "." will be removed from the name.  This
     *                      name may also contain the token '${pid}' that will be replaced by the
     *                      current process ID.
     *  @returns No return value.
     *
     *  @remarks This sets the new default log file name.  When set to a valid file name, all
     *           messages coming from events that do not use the @ref omni::structuredlog::fEventFlagUseLocalLog flag
     *           will be written to the log file using this name in the current log output
     *           directory.  When either this name or the log output directory changes, any
     *           open log files will be closed and reopened under the new name and path when
     *           the next event is written.
     *
     *  @note When log rotation occurs, the rotation number is inserted before the file extension
     *        (e.g. `filename.ext` is rotated to `filename.1.ext`).
     *        If no extension is detected, the number is just appended to the file name.
     *        A file extension is detected with the following regex: `\..{0,4}$`.
     */
    virtual void setLogDefaultName_abi(OMNI_ATTR("c_str, in") const char* name) noexcept = 0;

    /** Sets the user ID to use in messages.
     *
     *  @param[in] userId   The user ID to use with all event messages going forward.  This may
     *                      not be nullptr or an empty string.  The default is a random number.
     *  @returns No return value.
     *
     *  @remarks This sets the user ID to be used in the 'source' property of all event messages
     *           going forward.  This will not affect the user ID of any messages that are
     *           currently being processed at the time of this call.  This will affect all pending
     *           messages that are not being immediately processed however.  Only the host app
     *           should set this user ID.  External plugins and extensions should never change the
     *           user ID.
     *
     *  @thread_safety This call is thread safe.  However, it is the caller's responsibility to
     *                 ensure that no other host thread is simultaneously operating on the user
     *                 ID string returned from @ref omni::structuredlog::IStructuredLogSettings::getUserId() and that
     *                 nothing else has cached that returned string since it could become
     *                 invalidated here.
     */
    virtual void setUserId_abi(OMNI_ATTR("c_str, in, not_null") const char* userId) noexcept = 0;

    /** Attempts to load the privacy settings file.
     *
     *  @returns `true` if the privacy settings file was successfully loaded.
     *  @returns `false` if the privacy settings file could not be loaded.  This failure may be
     *           caused by the file being missing, failing to open the file due to permissions,
     *           failing to allocate memory to read in the file, the file not being formatted
     *           correctly as a TOML file, or failing to merge the new values into the settings
     *           registry.
     *
     *  @remarks This will attempt to load the privacy settings file for the current user.  Regardless
     *           of whether the file is successfully loaded, appropriate defaults will always be set
     *           for each of the expected privacy settings (as long as the ISettings interface is
     *           available).
     *
     *  @note This expects that some other system has already found and attempted to load the
     *        plugin that implements the ISettings interface.
     */
    virtual bool loadPrivacySettings_abi() noexcept = 0;

    /** Checks app settings to see if any schemas or events should be disabled or enabled.
     *
     *  @returns `true` if the settings registry was successfully checked for enable or disable
     *           states.  Returns `false` if the `ISettings` or `IDictionary` plugins have not
     *           been loaded yet.
     *
     *  @remarks This checks the settings registry to determine if any schemas or events should
     *           be disabled initially.  The keys in the settings registry that will be looked
     *           at are under @ref omni::structuredlog::kSchemasStateListSetting,
     *           @ref omni::structuredlog::kEventsStateListSetting,
     *           @ref omni::structuredlog::kEventsStateArraySetting,
     *           and @ref omni::structuredlog::kSchemasStateArraySetting.  Each of these parts
     *           of the settings registry expects the schema or event name to be specified in
     *           a different way.  Once the settings have been loaded, they are cached internally
     *           and will be used as the initial state of any newly registered schemas or events.
     *           Any state changes to events or schemas after these settings are cached can still
     *           be done programmatically with @ref omni::structuredlog::IStructuredLog::setEnabled().
     */
    virtual bool enableSchemasFromSettings_abi() noexcept = 0;
};

} // namespace structuredlog
} // namespace omni

#define OMNI_BIND_INCLUDE_INTERFACE_DECL
#include "IStructuredLogSettings.gen.h"

/** @copydoc omni::structuredlog::IStructuredLogSettings_abi */
class omni::structuredlog::IStructuredLogSettings
    : public omni::core::Generated<omni::structuredlog::IStructuredLogSettings_abi>
{
public:
    /** Attempts to load the privacy settings from a specific file.
     *
     *  @param[in] filename The name and path to the privacy settings file to load.  This is
     *                      expected to be a TOML formatted file.  This may be `nullptr` or
     *                      an empty string to reload the user default privacy settings.
     *  @param[in] flags    Flags to affect the behavior of this operation.  This must be
     *                      a combination of zero or more of the @ref PrivacyLoadFlags flags.
     *  @returns `true` if the privacy settings are successfully loaded from the given file.
     *           Returns `false` if the named file could not be found or the ISettings
     *           interface is not available.
     *
     *  @note This is not available when using structured logging in 'standalone' mode.
     *  @note This should only be used for testing purposes.  This should never be called
     *        in a production app.
     */
    bool loadPrivacySettingsFromFile(const char* filename, PrivacyLoadFlags flags)
    {
        carb::settings::ISettings* settings = carb::getCachedInterface<carb::settings::ISettings>();
        bool success;

        // The setting path to specify the path and filename of the privacy settings file to load.  If
        // this setting is specified, the privacy settings will be loaded from the named file instead
        // of the default location.  This file is expected to be TOML formatted and is expected to
        // provide the values for the "/privacy/" branch.  This should only be used for testing purposes.
        // This value defaults to not being defined.
        constexpr const char* kPrivacyFileSetting = "/structuredLog/privacySettingsFile";

        if (settings == nullptr)
            return false;

        if (filename == nullptr)
            filename = "";

        // explicitly reset the values that can affect whether 'internal' diagnostic data can be
        // sent.  This prevents extra data from being inadvertently sent if the 'privacy.toml'
        // file is deleted and recreated, then reloaded by the running app.  If the newly loaded
        // file also contains these settings, these values will simply be replaced.
        if ((flags & fPrivacyLoadFlagResetSettings) != 0)
        {
            settings->setString(omni::extras::PrivacySettings::kExtraDiagnosticDataOptInKey, "");
            settings->setBool(omni::extras::PrivacySettings::kExternalBuildKey, true);
        }

        settings->setString(kPrivacyFileSetting, filename);
        success = loadPrivacySettings_abi();
        settings->setString(kPrivacyFileSetting, "");
        return success;
    }
};


#define OMNI_BIND_INCLUDE_INTERFACE_IMPL
#include "IStructuredLogSettings.gen.h"
