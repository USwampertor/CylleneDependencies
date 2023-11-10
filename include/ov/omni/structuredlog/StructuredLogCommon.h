// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Common types/macros/functions for structured logging.
 */
#pragma once

#include "../core/IObject.h"
#include "../extras/ForceLink.h"


namespace omni
{
namespace structuredlog
{

/** Helper macro to piece together a unique event name to generate an ID from.
 *
 *  @param schemaName       The name of the schema that the event belongs to.  This should be
 *                          the name from the schema's "#/schemaMeta/clientName" property.
 *                          This value must be a string literal.
 *  @param eventName        The full name of the event that the ID is being generated for.
 *                          This should be the name of one of the objects in the
 *                          "#/definitions/events/" listing.  This value must be a string
 *                          literal.
 *  @param schemaVersion    The version of the schema the event belongs to.  This should be
 *                          the value from the schema's "#/schemaMeta/schemaVersion" property.
 *                          This value must be a string literal.
 *  @param parserVersion    The version of the object parser that this event ID is being built
 *                          for.  This should be the value of kParserVersion expressed as
 *                          a string.  This value must be a string literal.
 *  @returns A string representing the full name of an event.  This string is suitable for passing
 *           to OMNI_STRUCTURED_LOG_EVENT_ID() to generate a unique event ID from.
 */
#define OMNI_STRUCTURED_LOG_EVENT_ID(schemaName, eventName, schemaVersion, parserVersion)                              \
    CARB_HASH_STRING(schemaName "-" eventName "-" schemaVersion "." parserVersion)

#ifndef DOXYGEN_SHOULD_SKIP_THIS

// used internally in OMNI_STRUCTURED_LOG_ADD_SCHEMA().  Do not call directly.
#    define OMNI_STRUCTURED_LOG_SCHEMA_ADDED_NAME(name_, version_, parser_, line_)                                     \
        OMNI_STRUCTURED_LOG_SCHEMA_ADDED_NAME_INNER(name_, version_, parser_, line_)

// used internally in OMNI_STRUCTURED_LOG_SCHEMA_ADDED_NAME().  Do not call directly.
#    define OMNI_STRUCTURED_LOG_SCHEMA_ADDED_NAME_INNER(name_, version_, parser_, line_)                               \
        sSchema_##name_##_##version_##_##parser_##_##line_

#endif

/** Sets that a schema should be registered on module load.
 *
 *  @param schemaType_  The name and fully qualified namespace of the generated schema class
 *                      to be registered.  This will be added to a list of schemas to be
 *                      registered when the module is initialized.
 *  @param schemaName_  The name of the schema to be registered.  This may be any valid C++
 *                      token string, but should reflect the schema's name.  This is used
 *                      to make the initialization symbol more unique to the generated schema
 *                      class.
 *  @param version_     The schema's version expressed as a C++ token.  This should replace
 *                      any dots ("."), dashes ("-"), colons (":"), whitespace (" "), or
 *                      commas (",") with an underscore ("_").  This is used to make the
 *                      initialization symbol more unique to the generated schema class.
 *  @param parser_      The parser version being used for the generated schema expressed as
 *                      a C++ token.  This should replace any dots ("."), dashes ("-"), colons
 *                      (":"), whitespace (" "), or commas (",") with an underscore ("_").
 *                      This is used to make the initialization symbol more unique to the
 *                      generated schema class.
 *  @returns No return value.
 *
 *  @remarks This creates and registers a helper function that will call into a generated
 *           schema's registerSchema() function during omni core or framework initialization.
 *           The symbol used to force the registration at C++ initialization time is named
 *           based on the schema's name, schema's version, parser version, and the line number
 *           in the header file that this call appears on.  All of these values are used to
 *           differentiate this schema's registration from that of all others, even for
 *           different versions of the same generated schema.
 */
#define OMNI_STRUCTURED_LOG_ADD_SCHEMA(schemaType_, schemaName_, version_, parser_)                                    \
    CARB_ATTRIBUTE(weak)                                                                                               \
    CARB_DECLSPEC(selectany)                                                                                           \
    bool OMNI_STRUCTURED_LOG_SCHEMA_ADDED_NAME(schemaName_, version_, parser_, __LINE__) = []() {                      \
        omni::structuredlog::getModuleSchemas().push_back(&schemaType_::registerSchema);                               \
        return true;                                                                                                   \
    }();                                                                                                               \
    OMNI_FORCE_SYMBOL_LINK(                                                                                            \
        OMNI_STRUCTURED_LOG_SCHEMA_ADDED_NAME(schemaName_, version_, parser_, __LINE__), schemaRegistration)


/** Possible results from registering a new schema.  These indicate whether the schema was
 *  successfully registered or why it may have failed.  Each result code can be considered a
 *  failure unless otherwise noted.  In all failure cases, the schema's allocated data block
 *  will be destroyed before returning and no new events will be registered.
 */
enum class SchemaResult
{
    /** The new schema was successfully registered with a unique set of event identifiers. */
    eSuccess,

    /** The new schema exactly matches one that has already been successfully registered.  The
     *  events in the new schema are still valid and can be used, however no new action was
     *  taken to register the schema again.  This condition can always be considered successful.
     */
    eAlreadyExists,

    /** The new schema contains an event identifier that collides with an event in another schema.
     *  The schema that the existing event belongs to does not match this new one.  This often
     *  indicates that either the name of an event in the schema is not unique enough or that
     *  another version of the schema had already been registered.  This is often remedied by
     *  bumping the version number of the schema so that its event identifiers no longer matches
     *  the previous schema's event(s).
     */
    eEventIdCollision,

    /** The same schema was registered multiple times, but with different schema flags.  This is
     *  not allowed and will fail the new schema's registration.  This can be fixed by bumping
     *  the version of the new schema.
     */
    eFlagsDiffer,

    /** Too many events have been registered.  There is an internal limit of unique events that
     *  can be registered in any one process.  Failed schemas or schemas that exactly match an
     *  existing schema do not contribute their event count to this limit.  When this is
     *  returned, none of the new schema's events will be registered.  There is no recovering
     *  from this failure code.  This is often an indication that the process's events should be
     *  reorganized to not have so many.  The internal limit will be at least 65536 events.
     */
    eOutOfEvents,

    /** An invalid parameter was passed into IStructuredLog::commitSchema().  This includes a
     *  nullptr @a schemaBlock parameter, a nullptr event table, or a zero event count.  It is the
     *  caller's responsibility to ensure valid parameters are passed in.
     */
    eInvalidParameter,

    /** An event's schema payload information was not contained within the block of memory that
     *  was returned from IStructuredLog::allocSchema().  This is a requirement to ensure all the
     *  event's information memory is owned by the structured log core.
     */
    eEventNotInBlock,

    /** Memory could not be allocated for the new schema information object.  This can usually
     *  be considered fatal.
     */
    eOutOfMemory,
};


/** Base type for a unique ID of a registered event.  Each registered event is identified by an
 *  integer value that is derived from its name, schema, and version number.
 */
using EventId = uint64_t;

/** A special value to indicate a bad event identifier.  This is used as a failure code
 *  in some of the IStructuredLog accessor functions.
 */
constexpr EventId kBadEventId = ~1ull;


/** Retrieves a string containing the name of a SchemaResult value.
 *
 *  @param[in] result   The result code to convert to a string.
 *  @returns The result code's name as a string if a valid code is passed in.
 *  @returns "<unknown_result>" if an invalid or unknown result code is passed in.
 */
constexpr const char* getSchemaResultName(SchemaResult result)
{
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#    define OMNI_STRUCTUREDLOG_GETNAME(r, prefix)                                                                      \
        case r:                                                                                                        \
            return &(#r)[sizeof(#prefix) - 1]
    switch (result)
    {
        OMNI_STRUCTUREDLOG_GETNAME(SchemaResult::eSuccess, SchemaResult::e);
        OMNI_STRUCTUREDLOG_GETNAME(SchemaResult::eAlreadyExists, SchemaResult::e);
        OMNI_STRUCTUREDLOG_GETNAME(SchemaResult::eEventIdCollision, SchemaResult::e);
        OMNI_STRUCTUREDLOG_GETNAME(SchemaResult::eFlagsDiffer, SchemaResult::e);
        OMNI_STRUCTUREDLOG_GETNAME(SchemaResult::eOutOfEvents, SchemaResult::e);
        OMNI_STRUCTUREDLOG_GETNAME(SchemaResult::eInvalidParameter, SchemaResult::e);
        OMNI_STRUCTUREDLOG_GETNAME(SchemaResult::eEventNotInBlock, SchemaResult::e);
        OMNI_STRUCTUREDLOG_GETNAME(SchemaResult::eOutOfMemory, SchemaResult::e);
        default:
            return "<unknown_result>";
    }
#    undef OMNI_STRUCTUREDLOG_GETNAME
#endif
}

} // namespace structuredlog
} // namespace omni
