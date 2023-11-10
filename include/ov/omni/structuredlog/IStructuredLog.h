// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief The core structured logging interface.
 */
#pragma once

#include "StructuredLogCommon.h"

#include "../core/BuiltIn.h"
#include "../core/IObject.h"
#include "../core/Api.h"

#ifdef STRUCTUREDLOG_STANDALONE_MODE
#    include "../../carb/extras/Library.h"
#endif

#include <vector>


namespace omni
{
/** Structured logging and Telemetry. */
namespace structuredlog
{

class IStructuredLog;

// ******************************* enums, types, and constants ************************************
/** The expected base name for the structured log plugin.  This isn't strictly necessary unless
 *  the plugin needs to be explicitly loaded in standalone mode in an special manner.  By default
 *  the plugin is expected to be present in the same directory as the main executable.  If it is
 *  not in that location, it is the host app's responsibility to load the plugin dynamically
 *  before attempting to call any structured log functions (even `addModuleSchemas()`).  If the
 *  module is not loaded before making any calls, all calls will just silently fail and the
 *  structured log functionality will be in a disabled state.
 *
 *  The structured log plugin can be loading using carb::extras::loadLibrary().  Using the
 *  @ref carb::extras::fLibFlagMakeFullLibName flag will be useful unless the name is being
 *  constructed manually using carb::extras::createLibraryNameForModule().
 */
constexpr const char* OMNI_ATTR("no_py") kStructuredLogPluginName = "omni.structuredlog.plugin";

/** Base type for the version of the event payload parser to use.  This is used as part of a
 *  versioning scheme for the payload reader and event schema walker.
 */
using ParserVersion = uint16_t;

/** Base type for the handle to an allocated block of memory returned from either the
 *  @ref IStructuredLog::allocSchema() or @ref IStructuredLog::allocEvent() functions.
 *  These handles uniquely identify the allocated block but should be treated as opaque handles.
 */
using AllocHandle = void*;

/** A special string length value to indicate that a string parameter to a generated event
 *  sending function is null terminated and should have its length calculated instead of
 *  passing in an explicit length.
 */
constexpr size_t kNullTerminatedString = SIZE_MAX;

/** The current event payload parser version that will be used in the IStructuredLog interface.
 *  This symbol can be used by the generated code to set versioning information in each
 *  event payload.  This may be incremented in the future.
 */
constexpr ParserVersion kParserVersion = 0;


/** Approximate size of the maximum data payload in bytes that a message can contain that can be
 *  transmitted in a single message.  This is a matter of the typical size of message that
 *  can be sent to some data servers minus the average additional space needed for the message
 *  body and other identifying information.  Note that this is only an approximate guideline
 *  and should not be taken as an exact limit.  Also note that this does not account for the
 *  change in size related to data encoding methods such as Base64.  If a Base64 encoding is
 *  to be used for the data payload, the @ref kMaxMessageLengthBase64 limit should be used
 *  instead.
 */
constexpr size_t kMaxMessageLength = (10000000 - 256);

/** Approximate size of the maximum data payload in bytes that a message can contain that can be
 *  transmitted in a single message when the payload is encoded in Base64.  This is a matter of
 *  the typical message transmission limit for some data servers minus the average additional
 *  space needed for the message body and other identifying information, then converted to
 *  Base64's 6-to-8 bit encoding ratio (ie: every 6 bits of input data converts to 8 bits of
 *  encoded data).  Note that this is only an approximate guideline and should not be taken
 *  as an exact limit.
 */
constexpr size_t kMaxMessageLengthBase64 = (kMaxMessageLength * 6) / 8;


/** Base type for flags to control the behavior of the handling of a schema as a whole.  A schema
 *  encompasses the settings for a group of events that are all registered in a single call pair
 *  to @ref IStructuredLog::allocSchema() and @ref IStructuredLog::commitSchema().
 */
using SchemaFlags OMNI_ATTR("flag, prefix=fSchemaFlag") = uint32_t;

/** Flag to indicate that the log file should remain open between messages.  By default, each
 *  event message will cause the log file to be opened, the message written, then the log file
 *  closed.  This flag can make writing a large number of frequent event messages more efficient
 *  by avoiding the expense of opening and closing the log file repeatedly.  However, using this
 *  may also prevent the log file from being moved or deleted while an app is still running.  To
 *  work around that, all persistently open log files can be temporarily forced closed using
 *  @ref IStructuredLogControl::closeLog().
 */
constexpr SchemaFlags fSchemaFlagKeepLogOpen = 0x00000001;

/** Flag to indicate that the log file for this schema should include the process ID in the
 *  filename.  Note that using this flag will likely lead to a lot of log files being generated
 *  for an app since each session will create a [somewhat] unique file.  This will however
 *  reduce the possibility of potential performance issues related to many processes trying
 *  to lock and access the same file(s) simultaneously.  By default, the process ID is not
 *  included in a schema's log filename.
 */
constexpr SchemaFlags fSchemaFlagLogWithProcessId = 0x00000002;


/** Base type for flags to control the behavior of processing a single event.  These flags
 *  affect how events are processed and how they affect their log.
 */
using EventFlags OMNI_ATTR("flag, prefix=fEventFlag") = uint32_t;

/** Use the log file specified by the owning event's schema instead of the default log for the
 *  process.  This can be controlled at the per-event level and would be specified by either
 *  manually changing the flags in the generated event table or by passing a specific command
 *  line option to the code generator tool that would set this flag on all of the schema's
 *  events in the generated code.  This can be useful if an external plugin would like to
 *  have its events go to its own log file instead of the process's default log file.
 */
constexpr EventFlags fEventFlagUseLocalLog = 0x00000001;

/** Flag to indicate that this event is critical to succeed and should potentially block the
 *  calling thread on IStructuredLog::allocEvent() calls if the event queue is full.  The call
 *  will block until a buffer of the requested size can be successfully allocated instead of
 *  failing immediately.  This flag should be used sparingly since it could result in blocking
 *  the calling thread.
 */
constexpr EventFlags fEventFlagCriticalEvent = 0x00000002;

/** Flag to indicate that this event should be output to the stderr file.  If the
 *  @ref fEventFlagSkipLog flag is not also used, this output will be in addition to the
 *  normal output to the schema's log file.  If the @ref fEventFlagSkipLog flag is used,
 *  the normal log file will not be written to.  The default behavior is to only write the
 *  event to the schema's log file.  This can be combined with the @c fSchemaFlagOutputToStderr
 *  and @c fSchemaFlagSkipLog flags.
 */
constexpr EventFlags fEventFlagOutputToStderr = 0x00000010;

/** Flag to indicate that this event should be output to the stdout file.  If the
 *  @ref fEventFlagSkipLog flag is not also used, this output will be in addition to the
 *  normal output to the schema's log file.  If the @ref fEventFlagSkipLog flag is used,
 *  the normal log file will not be written to.  The default behavior is to only write the
 *  event to the schema's log file.  This can be combined with the @c fSchemaFlagOutputToStdout
 *  and @c fSchemaFlagSkipLog flags.
 */
constexpr EventFlags fEventFlagOutputToStdout = 0x00000020;

/** Flag to indicate that this event should not be output to the schema's specified log file.
 *  This flag is intended to be used in combination with the @ref fEventFlagOutputToStdout
 *  and @ref fEventFlagOutputToStderr flags to control which destination(s) each event log
 *  message would be written to.  The default behavior is to write each event message to
 *  the schema's specified log file.  Note that if this flag is used and neither the
 *  @ref fEventFlagOutputToStderr nor @ref fEventFlagOutputToStdout flags are used, it is
 *  effectively the same as disabling the event since there is no set destination for the
 *  message.
 */
constexpr EventFlags fEventFlagSkipLog = 0x00000040;


/** Base type for flags to control how events and schemas are enabled or disabled.  These flags
 *  can be passed to the @ref omni::structuredlog::IStructuredLog::setEnabled() function.
 */
using EnableFlags OMNI_ATTR("flag, prefix=fEnableFlag") = uint32_t;

/** Flag to indicate that a call to @ref IStructuredLog::setEnabled() should
 *  affect the entire schema that the named event ID belongs to instead of just the event.  When
 *  this flag is used, each of the schema's events will behave as though they are disabled.
 *  However, each event's individual enable state will not be modified.  When the schema is
 *  enabled again, the individual events will retain their previous enable states.
 */
constexpr EnableFlags fEnableFlagWholeSchema = 0x00000002;

/** Flag to indicate that the enable state of each event in a schema should be overridden when
 *  the @ref fEnableFlagWholeSchema flag is also used.  When this flag is used, the enable state
 *  of each event in the schema will be modified instead of just the enable state of the schema
 *  itself.  When this flag is not used, the default behavior is to change the enable state
 *  of just the schema but leave the enable states for its individual events unmodified.
 */
constexpr EnableFlags fEnableFlagOverrideEnableState = 0x00000004;

/** Flag to indicate that an enable state change should affect the entire system, not just
 *  one schema or event.  When this flag is used in @ref IStructuredLog::setEnabled(), the @a eventId
 *  parameter must be set to @ref kBadEventId.
 */
constexpr EnableFlags fEnableFlagAll = 0x00000008;


/** Base type for flags to control how new events are allocated.  These flags can be passed
 *  to the @ref IStructuredLog::allocEvent() function.
 */
using AllocFlags OMNI_ATTR("flag, prefix=fAllocFlag") = uint32_t;

/** Flag to indicate that the event should only be added to the queue on commit but that the
 *  consumer thread should not be started yet if it is not already running.
 */
constexpr AllocFlags fAllocFlagOnlyQueue = 0x00000010;


/** A descriptor for a single structured log event.  This struct should not need to be used
 *  externally but will be used by the generated code in the schema event registration helper
 *  function.  A schema consists of a set of one or more of these structs plus some additional
 *  name and version information.  The schema metadata is passed to @ref IStructuredLog::allocSchema().
 *  The array of these event info objects is passed to @ref IStructuredLog::commitSchema().
 */
struct EventInfo
{
    /** The fully qualified name of this event.  While there is no strict formatting requirement
     *  for this name, this should be an RDNS style name that specifies the full ownership chain
     *  of the event.  This should be similar to the following format:
     *    "com.nvidia.omniverse.<appName>.<eventName>"
     */
    const char* eventName;

    /** Flags controlling the behavior of this event when being generated or processed.  Once
     *  registered, these flags may not change.  This may be zero or more of the @ref EventFlags
     *  flags.
     */
    EventFlags flags;

    /** Version of the schema tree building object that is passed in through @ref schema.  This
     *  is used to indicate which version of the event payload block helper classes will be used
     *  to create and read the blocks.  In general this should be @ref kParserVersion.
     */
    ParserVersion parserVersion;

    /** The event ID that will be used to identify this event from external callers.  This should
     *  be a value that uniquely identifies the event's name, schema's version, and the JSON
     *  parser version (ie: @ref parserVersion above) that will be used.  Ideally this should be
     *  a hash of a string containing all of the above information.  It is the caller's
     *  responsibility to ensure the event ID is globally unique enough for any given app's usage.
     */
    uint64_t eventId;

    /** Schema tree object for this event.  This tree object is expected to be built in the
     *  block of memory that is returned from @ref IStructuredLog::allocSchema().  Optionally, this may
     *  be nullptr to indicate that the event's payload is intended to be empty (ie: the event
     *  is just intended to be an "I was here" event).
     */
    const void* schema;
};


// ********************************* IStructuredLog interface *************************************
/** Main structured log interface.  This should be treated internally as a global singleton.  Any
 *  attempt to create or acquire this interface will return the same object just with a new
 *  reference taken on it.
 *
 *  There are three main steps to using this interface:
 *    * Set up the interface.  For most app's usage, all of the default settings will suffice.
 *      The default log path will point to the Omniverse logs folder and the default user ID
 *      will be the one read from the current user's privacy settings file (if it exists).  If
 *      the default values for these is not sufficient, a new user ID should be set with
 *      @ref omni::structuredlog::IStructuredLogSettings::setUserId() and a log output path should be set with
 *      @ref omni::structuredlog::IStructuredLogSettings::setLogOutputPath().
 *      If the privacy settings file is not present,
 *      the user name will default to a random number.  The other defaults should be sufficient
 *      for most apps.  This setup only needs to be performed once by the host app if at all.
 *      The @ref omni::structuredlog::IStructuredLogSettings interface can be acquired either
 *      by casting an @ref omni::structuredlog::IStructuredLog
 *      object to that type or by directly creating the @ref omni::structuredlog::IStructuredLogSettings
 *      object using omni::core::ITypeFactory_abi::createType().
 *    * Register one or more event schemas.
 *      This is done with @ref omni::structuredlog::IStructuredLog::allocSchema() and
 *      @ref omni::structuredlog::IStructuredLog::commitSchema().
 *      At least one event must be registered for any events to
 *      be processed.  Once a schema has been registered, it will remain valid until the
 *      structured log module is unloaded from the process.  There is no way to forcibly
 *      unregister a set of events once registered.
 *    * Send zero or more events.  This is done with the @ref omni::structuredlog::IStructuredLog::allocEvent() and
 *      @ref omni::structuredlog::IStructuredLog::commitEvent() functions.
 *
 *  For the most part, the use of this interface will be dealt with in generated code.  This
 *  generated code will come from the 'omni.structuredlog' tool in the form of an inlined header
 *  file.  It is the host app's responsibility to call the header's schema registration helper
 *  function at some point on startup before any event helper functions are called.
 *
 *  All messages generated by this structured log system will be CloudEvents v1.0 compliant.
 *  These should be parsable by any tool that is capable of understanding CloudEvents.
 *
 *  Before an event can be sent, at least one schema describing at least one event must be
 *  registered.  This is done with the @ref omni::structuredlog::IStructuredLog::allocSchema() and
 *  @ref omni::structuredlog::IStructuredLog::commitSchema() functions.
 *  The @ref omni::structuredlog::IStructuredLog::allocSchema() function returns
 *  a handle to a block of memory owned by the structured log system and a pointer to that block's
 *  data.  The caller is responsible for both calculating the required size of the buffer before
 *  allocating, and filling it in with the schema data trees for each event that can be sent as
 *  part of that schema.  The helper functions in @ref omni::structuredlog::JsonTreeSizeCalculator and
 *  @ref omni::structuredlog::JsonBuilder can be used to build these trees.  Once these trees are built, they are
 *  stored in a number of entries in an array of @ref omni::structuredlog::EventInfo objects.  This array of event
 *  info objects is then passed to @ref omni::structuredlog::IStructuredLog::commitSchema() to complete the schema
 *  registration process.
 *
 *  Sending of a message is split into two parts for efficiency.  The general idea is that the
 *  caller will allocate a block of memory in the event queue's buffer and write its data directly
 *  there.
 *  The allocation occurs with the @ref omni::structuredlog::IStructuredLog::allocEvent() call.
 *  This will return a
 *  handle to the allocated block and a pointer to the first byte to start writing the payload
 *  data to.  The buffer's header will already have been filled in upon return from
 *  @ref omni::structuredlog::IStructuredLog::allocEvent().
 *  Once the caller has written its event payload information to
 *  the buffer, it will call @ref omni::structuredlog::IStructuredLog::commitEvent()
 *  to commit the message to the queue.  At
 *  this point, the message can be consumed by the event processing thread.
 *
 *  Multiple events may be safely allocated from and written to the queue's buffer simultaneously.
 *  There is no required order that the messages be committed in however.  If a buffer is
 *  allocated after one that has not been committed yet, and that newer event is committed first,
 *  the only side effect will be that the event processing thread will be stalled in processing
 *  new events until the first message is also committed.  This is important since the events
 *  would still need to be committed to the log in the correct order.
 *
 *  All events that are processed through this interface will be written to a local log file.  The
 *  log file will be periodically consumed by an external process (the Omniverse Transmitter app)
 *  that will send all the approved events to the telemetry servers.  Only events that have been
 *  approved by legal will be sent.  All other messages will be rejected and only remain in the
 *  log files on the local machine.
 */
class IStructuredLog_abi
    : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.structuredlog.IStructuredLog")>
{
protected:
    /** Checks whether a specific event or schema is enabled.
     *
     *  @param[in] eventId  The unique ID of the event to check the enable state of.  This
     *                      is the ID that was originally used to register the event.
     *  @returns `true` if both the requested event and its schema is enabled.
     *  @returns `false` if either the requested event or its schema is disabled.
     *
     *  @remarks This checks if a named event or its schema is currently enabled in the structured
     *           log system.  Individual events or entire schemas may be disabled at any given
     *           time.  Both the schema and each event has its own enable state.  A schema can be
     *           disabled while still leaving its events' previous enable/disable states
     *           unmodified.  When the schema is enabled again, its events will still retain their
     *           previous enable states.  Set @ref omni::structuredlog::IStructuredLog::setEnabled()
     *           for more information on how to enable and disable events and schemas.
     */
    virtual bool isEnabled_abi(EventId eventId) noexcept = 0;

    /** Sets the enable state for a structured log event or schema, or the system globally.
     *
     *  @param[in] eventId  The ID of the event to change the enable state of.  This is the ID
     *                      that was originally used to register the event.  If the
     *                      @ref omni::structuredlog::fEnableFlagAll flag is used, this must be
     *                      set to @ref omni::structuredlog::kBadEventId.
     *  @param[in] flags    Flags to control the behavior of this function.  This may be zero or
     *                      more of the @ref omni::structuredlog::EnableFlags flags.
     *                      If this includes the @ref omni::structuredlog::fEnableFlagAll
     *                      flag, @p eventId must be set to @ref omni::structuredlog::kBadEventId.
     *  @param[in] enabled  Set to true to enable the named event or schema.  Set to false to
     *                      disable the named event or schema.  If the
     *                      @ref omni::structuredlog::fEnableFlagAll flag is used, this sets the
     *                      new enable/disable state for the structured log system as a whole.
     *  @returns No return value.
     *
     *  @remarks This changes the current enable state for an event or schema.  The scope of the
     *           enable change depends on the flags that are passed in.  When an event is
     *           disabled (directly or from its schema or the structured log system being
     *           disabled), it will also prevent it from being generated manually.  In this case,
     *           any attempt to call @ref omni::structuredlog::IStructuredLog::allocEvent() for
     *           that disabled event or schema will simply fail immediately.
     *
     *  @remarks When a schema is disabled, it effectively disables all of its events.  Depending
     *           on the flag usage however (ie: @ref omni::structuredlog::fEnableFlagOverrideEnableState),
     *           disabling the schema may or may not change the enable states of each of its
     *           individual events as well (see @ref omni::structuredlog::fEnableFlagOverrideEnableState
     *           for more information).
     *
     *  @note The @ref omni::structuredlog::fEnableFlagAll flag should only ever by used by the
     *        main host app since this will affect the behavior of all modules' events regardless
     *        of their own internal state.  Disabling the entire system should also only ever be
     *        used sparingly in cases where it is strictly necessary (ie: compliance with local
     *        privacy laws).
     */
    virtual void setEnabled_abi(EventId eventId, EnableFlags flags, bool enabled) noexcept = 0;


    // ****** schema registration function ******
    /** Allocates a block of memory for an event schema.
     *
     *  @param[in] schemaName       The name of the schema being registered.  This may not be
     *                              nullptr or an empty string.  There is no set format for this
     *                              schema name, but it should at least convey some information
     *                              about the app's name and current release version.  This name
     *                              will be used to construct the log file name for the schema.
     *  @param[in] schemaVersion    The version number for the schema itself.  This may not be
     *                              nullptr or an empty string.  This should be the version number
     *                              of the schema itself, not of the app or component.  This will
     *                              be used to construct the 'dataschema' name that gets passed
     *                              along with each CloudEvents message header.
     *  @param[in] flags            Flags to control the behavior of the schema.  This may be
     *                              zero or more of the @ref omni::structuredlog::SchemaFlags flags.
     *  @param[in] size             The size of the block to allocate in bytes.  If this is 0, a
     *                              block will still be allocated, but its actual size cannot be
     *                              guaranteed.  The @ref omni::structuredlog::JsonTreeSizeCalculator
     *                              helper class can be used to calculate the size needs for the new schema.
     *  @param[out] outHandle       Receives the handle to the allocated memory block on success.
     *                              On failure, this receives nullptr.  Each successful call
     *                              must pass this handle to @ref omni::structuredlog::IStructuredLog::commitSchema()
     *                              even if an intermediate failure occurs.  In the case of a schema
     *                              tree creation failure, nullptr should be passed for @a events
     *                              in the corresponding @ref omni::structuredlog::IStructuredLog::commitSchema() call.
     *                              This will allow the allocated block to be cleaned up.
     *  @returns A pointer to the allocated block on success.  This block does not need to be
     *           explicitly freed - it will be managed internally.  This pointer will point to
     *           the first byte that can be written to by the caller and will be at least @p size
     *           bytes in length.  This pointer will always be aligned to the size of a pointer.
     *  @returns `nullptr` if no more memory is available.
     *  @returns `nullptr` if an invalid parameter is passed in.
     *
     *  @remarks This allocates a block of memory that the schema tree(s) for the event(s) in a
     *           schema can be created and stored in.  Pointers to the start of each event schema
     *           within this block are expected to be stored in one of the @ref omni::structuredlog::EventInfo objects
     *           that will later be passed to @ref omni::structuredlog::IStructuredLog::commitSchema().  The caller is
     *           responsible for creating and filling in returned block and the array of
     *           @ref omni::structuredlog::EventInfo objects.
     *           The @ref omni::structuredlog::BlockAllocator helper class may be used to
     *           allocate smaller chunks of memory from the returned block.
     *
     *  @note This should only be used in generated structured log source code.  This should not
     *        be directly except when absolutely necessary.  This should also not be used as a
     *        generic allocator.  Failure to use this properly will result in memory being
     *        leaked.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual uint8_t* allocSchema_abi(OMNI_ATTR("c_str, in, not_null") const char* schemaName,
                                     OMNI_ATTR("c_str, in, not_null") const char* schemaVersion,
                                     SchemaFlags flags,
                                     size_t size,
                                     OMNI_ATTR("out") AllocHandle* outHandle) noexcept = 0;

    /** Commits an allocated block and registers events for a single schema.
     *
     *  @param[in] schemaBlock  The block previously returned from
     *                          @ref omni::structuredlog::IStructuredLog::allocSchema()
     *                          that contains the built event trees for the schema to register.
     *                          These trees must be pointed to by the @ref omni::structuredlog::EventInfo::schema
     *                          members of the @p events array.
     *  @param[in] events       The table of events that belong to this schema.  This provides
     *                          information about each event such as its name, control flags,
     *                          event identifier, and a schema describing how to interpret its
     *                          binary blob on the consumer side.  This may not be nullptr.  Each
     *                          of the trees pointed to by @ref omni::structuredlog::EventInfo::schema in this table
     *                          must either be set to nullptr or point to an address inside the
     *                          allocated schema block @p schemaBlock that was returned from the
     *                          corresponding call to @ref omni::structuredlog::IStructuredLog::allocSchema().
     *                          If any of the schema trees point to an address outside of the schema block,
     *                          this call will fail.
     *  @param[in] eventCount   The total number of events in the @p events table.  At least one
     *                          event must be registered.  This may not be 0.
     *  @retval omni::structuredlog::SchemaResult::eSuccess if the new schema
     *           is successfully registered as a set of unique events.
     *  @retval omni::structuredlog::SchemaResult::eAlreadyExists if the new
     *           schema exactly matches one that has already been successfully registered.
     *           This can be considered a successful result.
     *           In this case, the schema block will be destroyed before return.
     *  @retval omni::structuredlog::SchemaResult::eEventIdCollision if the new schema contains an event whose
     *           identifier matches that of an event that has already been registered with another
     *           schema.  This indicates that the name of the event that was used to generate the
     *           identifier was likely not unique enough, or that two different versions of the
     *           same schema are trying to be registered without changing the schema's version
     *           number first.  No new events will be registered in this case and the schema
     *           block will be destroyed before return.
     *  @retval omni::structuredlog::SchemaResult::eFlagsDiffer if the new schema exactly matches another schema
     *           that has already been registered except for the schema flags that were used
     *           in the new one.  This is not allowed without a version change in the new schema.
     *           No new events will be registered in this case and the schema block will be
     *           destroyed before return.
     *  @returns Another @ref omni::structuredlog::SchemaResult error code for other types of failures.  No new events
     *           will be registered in this case and the schema block will be destroyed before
     *           return.
     *
     *  @remarks This registers a new schema and its events with the structured log system.  This
     *           will create a new set of events in the structured log system.  These events
     *           cannot be unregistered except by unloading the entire structured log system
     *           altogether.  Upon successful registration, the events in the schema can be
     *           emitted using the event identifiers they were registered with.
     *
     *  @remarks If the new schema matches one that has already been registered, The operation
     *           will succeed with the result @ref omni::structuredlog::SchemaResult::eAlreadyExists.
     *           The existing
     *           schema's name, version, flags, and event table (including order of events) must
     *           match the new one exactly in order to be considered a match.  If anything differs
     *           (even the flags for a single event or events out of order but otherwise the same
     *           content), the call will fail.  If the schema with the differing flags or event
     *           order is to be used, its version or name must be changed to avoid conflict with
     *           the existing schema.
     *
     *  @remarks When generating the event identifiers for @ref omni::structuredlog::EventInfo::eventId it is
     *           recommended that a string uniquely identifying the event be created then hashed
     *           using an algorithm such as FNV1.  The string should contain the schema's client
     *           name, the event's name, the schema's version, and any other values that may
     *           help to uniquely identify the event.  Once hashed, it is very unlikely that
     *           the event identifier will collide with any others.  This is the method that the
     *           code generator tool uses to create the unique event identifiers.
     *
     *  @remarks Up to 65536 (ie: 16-bit index values) events may be registered with the
     *           structured log system.  The performance of managing a list of events this large
     *           is unknown and not suggested however.  Each module, app, or component should only
     *           register its schema(s) once on startup.  This can be a relatively expensive
     *           operation if done frequently and unnecessarily.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual SchemaResult commitSchema_abi(OMNI_ATTR("in, out, not_null") AllocHandle schemaBlock,
                                          OMNI_ATTR("in, *in, count=eventCount, not_null") const EventInfo* events,
                                          size_t eventCount) noexcept = 0;


    // ****** event message generation functions ******
    /** Allocates a block of memory to store an event's payload data in.
     *
     *  @param[in]  version     The version of the parser that should be used to read this
     *                          event's payload data.  This should be @ref omni::structuredlog::kParserVersion.
     *                          If the structured log system that receives this message does not
     *                          support this particular version (ie: a newer module is run
     *                          on an old structured log system), the event message will simply
     *                          be dropped.
     *  @param[in]  eventId     the unique ID of the event that is being generated.  This ID must
     *                          exactly match the event ID that was provided in the
     *                          @ref omni::structuredlog::EventInfo::eventId value when the event
     *                          was registered.
     *  @param[in]  flags       Flags to control how the event's block is allocated.  This may
     *                          be a combination of zero or more of the @ref omni::structuredlog::AllocFlags
     *                          flags.
     *  @param[in]  payloadSize The total number of bytes needed to store the event's payload
     *                          data.  The caller is responsible for calculating this ahead
     *                          of time.  If the event does not have a payload, this should be
     *                          0.  The number of bytes should be calculated according to how
     *                          the requested event's schema (ie: @ref omni::structuredlog::EventInfo::schema) lays
     *                          it out in memory.
     *  @param[out] outHandle   Receives the handle to the allocated block of memory.  This
     *                          must be passed to IStructuredLog::commitEvent() once the caller
     *                          has finished writing all of the payload data to the returned
     *                          buffer.  The IStructuredLog::commitEvent() call acts as the
     *                          cleanup function for this handle.
     *  @returns A pointer to the buffer to use for the event's payload data if successfully
     *           allocated.  The caller should start writing its payload data at this address
     *           according to the formatting information in the requested event's schema.  This
     *           returned pointer will always be aligned to the size of a pointer.
     *  @returns `nullptr` if the requested event, its schema, or the entire system is currently
     *           disabled.
     *  @returns `nullptr` if the event queue's buffer is full and a buffer of the requested
     *           size could not be allocated.  In this case, a invalid handle will be returned
     *           in @p outHandle.  The IStructuredLog::commitEvent() function does not need to be
     *           called in this case.
     *  @returns `nullptr` if the event queue failed to be created or its processing thread failed
     *           start up.
     *  @returns `nullptr` if the given event ID is not valid.
     *
     *  @remarks This is the main entry point for creating an event message.  This allocates
     *           a block of memory that the caller can fill in with its event payload data.
     *           The caller is expected to fill in this buffer as quickly as possible.  Once
     *           the buffer has been filled, its handle must be passed to the
     *           @ref omni::structuredlog::IStructuredLog::commitEvent() function to finalize and send.
     *           Failing to pass a
     *           valid handle to @ref omni::structuredlog::IStructuredLog::commitEvent() will stall the event queue
     *           indefinitely.
     *
     *  @remarks If the requested event has been marked as 'critical' by using the event flag
     *           @ref omni::structuredlog::fEventFlagCriticalEvent, a blocking allocation will be used here instead.
     *           In this case, this will not fail due to the event queue being out of space.
     *
     *  @note This call will fail immediately if either the requested event, its schema, or
     *        the entire system has been explicitly disabled.  It is the caller's responsibility
     *        to both check the enable state of the event before attempting to send it (ie: to
     *        avoid doing unnecessary work), and to gracefully handle the potential of this
     *        call failing.
     *
     *  @note It is the caller's responsibility to ensure that no events are generated during
     *        C++ static destruction time for the process during shutdown.  Especially on
     *        Windows, doing so could result in an event being allocated but not committed
     *        thereby stalling the event queue.  This could lead to a hang on shutdown.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual uint8_t* allocEvent_abi(ParserVersion version,
                                    EventId eventId,
                                    AllocFlags flags,
                                    size_t payloadSize,
                                    OMNI_ATTR("out") AllocHandle* outHandle) noexcept = 0;

    /** finishes writing a message's payload and queues it for processing.
     *
     *  @param[in] handle   The handle to the queue buffer block to be committed.  This must not
     *                      be nullptr.  This must be the same handle that was returned through
     *                      @a outHandle on a recent call to @ref omni::structuredlog::IStructuredLog::allocEvent()
     *                      on this same thread.  Upon return, this handle will be invalid and should be
     *                      discarded by the caller.
     *  @returns No return value.
     *
     *  @remarks This commits a block that was previously allocated on this thread with
     *           @ref omni::structuredlog::IStructuredLog::allocEvent().
     *           It is required that the commit call occur on the
     *           same thread that the matching @ref omni::structuredlog::IStructuredLog::allocEvent()
     *           call was made on.
     *           Each successful @ref omni::structuredlog::IStructuredLog::allocEvent() call must
     *           be paired with exactly one
     *           @ref omni::structuredlog::IStructuredLog::commitEvent() call on the same thread.
     *           Failing to do so would result in the event queue thread stalling.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual void commitEvent_abi(OMNI_ATTR("in, not_null") const AllocHandle handle) noexcept = 0;
};

// skipping this because exhale can't handle the function pointer type
#ifndef DOXYGEN_SHOULD_SKIP_THIS
/** Registration function to install a schema with the structured logging system.
 *
 *  @param[in] log  A pointer to the global singleton structured logging system to install
 *                  the schema in.
 *  @returns `true` if the schema is successfully installed or was already installed.
 *  @returns `false` if the schema could not be installed.  This may be caused by a lack of
 *           available memory, or too many events have been registered in the system.
 */
using SchemaAddFn = bool (*)(IStructuredLog*);

/** Retrieves the local schema registration list for this module.
 *
 *  @returns The static list of schemas to register for this module.  This is intended to be
 *           a static list that can be built up at compile time to collect schemas to
 *           register.  Each module will have its own copy of this list.
 */
inline std::vector<SchemaAddFn>& getModuleSchemas()
{
    static std::vector<SchemaAddFn> sSchemas;
    return sSchemas;
}
#endif


} // namespace structuredlog
} // namespace omni

#ifdef OMNI_COMPILE_AS_DYNAMIC_LIBRARY
OMNI_API omni::structuredlog::IStructuredLog* omniGetStructuredLogWithoutAcquire();
#else
/**
 * Retrieves the module's structured log object. omni::core::IObject::acquire() is **not** called on the returned
 * pointer.
 *
 * The global omni::structuredlog::IStructuredLog instance can be configured by passing an
 * @ref omni::structuredlog::IStructuredLog to omniCoreStart().
 * If an instance is not provided, omniCoreStart() attempts to create one.
 *
 * @returns the calling module's structured log object.  The caller will not own a reference to
 *          the object.  If the caller intends to store the object for an extended period, it
 *          must take a reference to it using either a call to acquire() or by borrowing it into
 *          an ObjectPtr<> object.  This object must not be released by the caller unless a
 *          reference to it is explicitly taken.
 */
#    ifndef STRUCTUREDLOG_STANDALONE_MODE
inline omni::structuredlog::IStructuredLog* omniGetStructuredLogWithoutAcquire()
{
    return static_cast<omni::structuredlog::IStructuredLog*>(omniGetBuiltInWithoutAcquire(OmniBuiltIn::eIStructuredLog));
}
#    else
inline omni::structuredlog::IStructuredLog* omniGetStructuredLogWithoutAcquire()
{
    using GetFunc = omni::structuredlog::IStructuredLog* (*)();
    static GetFunc s_get = nullptr;

    if (s_get == nullptr)
    {
        carb::extras::LibraryHandle module = carb::extras::loadLibrary(
            omni::structuredlog::kStructuredLogPluginName, carb::extras::fLibFlagMakeFullLibName);

        s_get = carb::extras::getLibrarySymbol<GetFunc>(module, "omniGetStructuredLogWithoutAcquire_");

        if (s_get == nullptr)
            return nullptr;
    }

    return s_get();
}
#    endif
#endif

#define OMNI_BIND_INCLUDE_INTERFACE_DECL
#include "IStructuredLog.gen.h"

/** Common entry point for sending an event.
 *
 *  @param event_       The name of the event to send.  This is expected to be the 'short'
 *                      name for the event as named in its schema's *_sendEvent() function.
 *                      This short name will be the portion of the function name before the
 *                      '_sendEvent' portion of the name.
 *  @param ...          The set of parameters specific to the event to be sent.  These are
 *                      potentially different for each event.  Callers should refer to the
 *                      original schema to determine the actual set of parameters to send.
 *  @returns No return value.
 *
 *  @remarks This is intended to be used to send all structured log events instead of calling the
 *           generated schema class's functions directly.  This provides a consistent entry
 *           point into sending event messages and allows parameter evaluation to be delayed
 *           if either the event or schema is disabled.
 */
#define OMNI_STRUCTURED_LOG(event_, ...)                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        auto strucLog__ = omniGetStructuredLogWithoutAcquire();                                                        \
        if (strucLog__)                                                                                                \
        {                                                                                                              \
            if (event_##_isEnabled(strucLog__))                                                                        \
            {                                                                                                          \
                event_##_sendEvent(strucLog__, ##__VA_ARGS__);                                                         \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)


namespace omni
{
namespace structuredlog
{

//! A function that registers all schemas within a module.
//!
//! \note It is not necessary to call this function; it is automatically called by \ref carbOnPluginPreStartup
inline void addModulesSchemas() noexcept
{
    auto strucLog = omniGetStructuredLogWithoutAcquire();

    if (strucLog == nullptr)
        return;

    for (auto& schemaAddFn : getModuleSchemas())
    {
        schemaAddFn(strucLog);
    }
}

} // namespace structuredlog
} // namespace omni

/** @copydoc omni::structuredlog::IStructuredLog_abi */
class omni::structuredlog::IStructuredLog : public omni::core::Generated<omni::structuredlog::IStructuredLog_abi>
{
};

#define OMNI_BIND_INCLUDE_INTERFACE_IMPL
#include "IStructuredLog.gen.h"
