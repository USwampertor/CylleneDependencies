// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
///! @file
///! @brief Main interface header for ICrashReporter and related types and values.
#pragma once

#include "../Interface.h"
#include "../Types.h"

namespace carb
{
/** Namespace for the crash reporter. */
namespace crashreporter
{

/** Prototype for a callback that indicates when a crash dump upload has completed.
 *
 *  @param[in] userData     The opaque user data object that was originally passed to the
 *                          @ref ICrashReporter::sendAndRemoveLeftOverDumpsAsync() function
 *                          in the @a userData parameter.
 *  @returns No return value.
 *
 *  @remarks This callback function will be performed when the upload of old crash dump files
 *           has completed, successfully or otherwise.  At this point, the upload request made
 *           by the corresponding @ref ICrashReporter::sendAndRemoveLeftOverDumpsAsync() call
 *           has completed.  However, this does not necessarily mean that the thread created
 *           by it has exited.  If another call was made, a new request would have been queued
 *           on that same thread and would be serviced next by the same thread.
 *
 *  @note This callback is both separate and different from the callback specified by the
 *        @ref OnCrashSentFn prototype.  This particular callback is only performed when the
 *        full upload request of all existing old crash dump files completes whereas the
 *        @ref OnCrashSentFn callback is performed every time any single upload completes.
 */
using OnDumpSubmittedFn = void (*)(void* userData);

/** Result codes used to notify subscribers of crash dump uploads whether an upload succeed
 *  or not.  These result codes are passed to the callback function specified in calls to
 *  @ref ICrashReporter::addCrashSentCallback().
 */
enum class CrashSentResult
{
    eSuccess, ///< The upload completed successfully.
    eFailure ///< The upload failed for some unspecified reason.
};

/** Possible types that a volatile metadata value could be.  These are used to determine which
 *  type of value is to be returned from a volatile metadata callback function and how that value
 *  is to be converted into a string to be sent as metadata.  The return type of the callback is
 *  split into common primitive types to discourage implementors of the callbacks from using their
 *  own potentially dangerous methods of converting the metadata value to a string.
 */
enum class MetadataValueType
{
    eInteger, ///< The callback will return a signed 64-bit integer value.
    eUInteger, ///< The callback will return an unsigned 64-bit integer value.
    eFloat, ///< The callback will return a 64-bit floating point value.
    eString, ///< The callback will return an arbitrary length UTF-8 encoded string.
    eWideString, ///< The callback will return an arbitrary length wide string (`wchar_t` characters).
};

/** Provides a single piece of additional information or context to a crash upload complete
 *  callback function.  This is stored as a key/value pair.  An array of these objects is
 *  passed to the @ref OnCrashSentFn callback to provide extra context to why a crash dump
 *  upload may have failed or additional information about a successful upload.  This
 *  information is typically only useful for display to a user or to be output to a log.
 */
struct CrashSentInfo
{
    const char* key; ///< The key name for this piece of information.
    const char* value; ///< The specific value associated with the given key.
};

/** Prototype for a callback function that is performed any time a dump is successfully uploaded.
 *
 *  @param[in] crashSentResult  The result code of the upload operation.  Currently this only
 *                              indicates whether the upload was successful or failed.  Further
 *                              information about the upload operation can be found in the
 *                              @p infoData array.
 *  @param[in] infoData         An array of zero or more key/value pairs containing additional
 *                              information for the upload operation.  On failure, this may
 *                              include the status code or status message from the server.  On
 *                              success, this may include a unique fingerprint for the crash
 *                              dump that was uploaded.  This array will contain exactly
 *                              @p infoDataCount items.
 *  @param[in] infoDataCount    The total number of items in the @p infoData array.
 *  @param[in] userData         The opaque caller specified data object that was provided when
 *                              this callback was originally registered.  It is the callee's
 *                              responsibility to know how to successfully make use of this
 *                              value.
 *  @returns No return value.
 *
 *  @remarks This callback is performed every time a crash dump file upload completes.  This
 *           will be called whether the upload is successful or not.  This will not however
 *           be called if crash dump uploads are disabled (ie: the
 *           `/crashreporter/devOnlyOverridePrivacyAndForceUpload` setting is false and the
 *           user has not provided 'performance' consent) or the file that an upload was requested
 *           for was missing some required metadata (ie: the `/crashreporter/product` and
 *           `/crashreporter/version` settings).  In both those cases, no upload attempt will be
 *           made.
 *
 *  @remarks The following key/value pair is defined for this callback when using the
 *           `carb.crashreporter-breakpad.plugin` implementation:
 *              * "response": A string containing the HTTP server's response to the upload
 *                            attempt.  If this string needs to persist, it must be copied
 *                            by the callee.
 *
 *  @thread_safety Calls to this callback will be serialized.  It is however the callee's
 *                 responsibility to safely access any additional objects including the
 *                 @p userData object and any global resources.
 */
using OnCrashSentFn = void (*)(CrashSentResult crashSentResult,
                               const CrashSentInfo* infoData,
                               size_t infoDataCount,
                               void* userData);

/** Opaque handle for a single registered @ref OnCrashSentFn callback function.  This is
 *  returned from ICrashReporter::addCrashSentCallback() and can be passed back to
 *  ICrashReporter::removeCrashSentCallback() to unregister it.
 */
struct CrashSentCallbackId;

/** Prototype for a callback function used to resolve symbol information.
 *
 *  @param[in] address  The address of the symbol being resolved.
 *  @param[in] name     If the symbol resolution was successful, this will be the name of the
 *                      symbol that @p address is contained in.  If the resolution fails, this
 *                      will be `nullptr`.  If non-`nullptr`, this string must be copied before
 *                      returning from the callback function if it needs to persist.
 *  @param[in] userData The opaque user data passed to the @ref ICrashReporter::resolveSymbol()
 *                      function.
 *  @returns No return value.
 *
 *  @remarks This callback is used to deliver the results of an attempt to resolve the name of
 *           a symbol in the current process.  This callback is always performed synchronously
 *           to the call to ICrashReporter::resolveSymbol().
 */
using ResolveSymbolFn = void (*)(const void* address, const char* name, void* userData);


/** Metadata value callback function prototype.
 *
 *  @param[in] context  The opaque context value that was used when the metadata value was
 *                      originally registered.
 *  @returns The current value of the metadata at the time of the call.
 *
 *  @note Because these callbacks may be called during the handling of a crash, the calling thread
 *        and other threads may be in an unstable or undefined state when these are called.
 *        Implementations of these callbacks should avoid any allocations and locks if in any way
 *        avoidable.  See ICrashReporter::addVolatileMetadataValue() for more information on how
 *        these callbacks should behave.
 */
using OnGetMetadataIntegerFn = int64_t (*)(void* context);

/** @copydoc OnGetMetadataIntegerFn */
using OnGetMetadataUIntegerFn = uint64_t (*)(void* context);

/** @copydoc OnGetMetadataIntegerFn */
using OnGetMetadataFloatFn = double (*)(void* context);

/** Metadata value callback function prototype.
 *
 *  @param[out] buffer      Receives the string value.  This must be UTF-8 encoded and must not
 *                          exceed @p maxLength bytes including the null terminator.  This buffer
 *                          will never be `nullptr`. Writing a null terminator is optional.
 *  @param[in] maxLength    The maximum number of bytes including the null terminator that can fit
 *                          in the buffer @p buffer.  This will never be 0.  It is the callback's
 *                          responsibility to ensure no more than this many bytes is written to
 *                          the output buffer.
 *  @param[in] context      The opaque context value that was used when the metadata value was
 *                          originally registered.
 *  @returns The total number of bytes **not including the null terminator character** that were
 *           written to the output buffer. This value **MUST** not exceed \p maxLength.
 */
using OnGetMetadataStringFn = size_t (*)(char* buffer, size_t maxLength, void* context);

/** Metadata value callback function prototype.
 *
 *  @param[out] buffer      Receives the string value.  This must be wide characters (`wchar_t`) and not
 *                          exceed @p maxLength *characters* including the null terminator.  This buffer
 *                          will never be `nullptr`. Writing a null terminator is optional.
 *  @param[in] maxLength    The maximum number of *characters* including the null terminator that can fit
 *                          in the buffer @p buffer.  This will never be 0.  It is the callback's
 *                          responsibility to ensure no more than this many characters is written to
 *                          the output buffer.
 *  @param[in] context      The opaque context value that was used when the metadata value was
 *                          originally registered.
 *  @returns The total number of *characters* **not including any null terminator** that were
 *           written to the output buffer. This value **MUST** not exceed \p maxLength.
 */
using OnGetMetadataWideStringFn = size_t (*)(wchar_t* buffer, size_t maxLength, void* context);

/** Descriptor of a single metadata callback function.  This describes which type of callback is
 *  being contained and the pointer to the function to call.
 */
struct MetadataValueCallback
{
    /** The type of the callback.  This indicates which of the callbacks in the @ref fn union
     *  below will be called to retrieve the value.
     */
    MetadataValueType type;

    /** A union containing the different types of function pointers for this callback.  Exactly
     *  one of these will be chosen based on @ref type.
     */
    union
    {
        OnGetMetadataIntegerFn getInteger; ///< Callback returning a signed 64-bit integer.
        OnGetMetadataUIntegerFn getUInteger; ///< Callback returning an unsigned 64-bit integer.
        OnGetMetadataFloatFn getFloat; ///< Callback returning a 64-bit floating point value.
        OnGetMetadataStringFn getString; ///< Callback returning an arbitrary length string.
        OnGetMetadataWideStringFn getWString; ///< Callback returning an arbitrary length wide string.
    } fn;
};

/** Registration identifier for a single metadata value.  This is only used to unregister the
 *  callback that was registered with the original metadata.
 */
using MetadataId = size_t;

/** Special metadata identifier to indicate an invalid metadata value or general failure in
 *  registering the value with addVolatileMetadata*().
 */
constexpr MetadataId kInvalidMetadataId = (MetadataId)(-1ll);

/** Special metadata identifier to indicate that a bad parameter was passed into one of the
 *  ICrashReporter::addVolatileMetadata*() functions.  This is not a valid identifier and will be
 *  ignored if passed to ICrashReporter::removeVolatileMetadataValue().
 */
constexpr MetadataId kMetadataFailBadParameter = (MetadataId)(-2ll);

/** Special metadata identifier to indicate that the key being registered is either a known
 *  reserved key or has already been registered as a volatile metadata key.  This is not a valid
 *  identifier and will be ignored if passed to ICrashReporter::removeVolatileMetadataValue().
 */
constexpr MetadataId kMetadataFailKeyAlreadyUsed = (MetadataId)(-3ll);


/** ICrashReporter is the interface to implement a plugin that catches and reports information
 *  about the crash to either a local file, a server, or both.
 *
 *  ICrashReporter is an optional plugin that is automatically loaded by the framework and doesn't
 *  need to be specifically listed in the configuration.  If an ICrashReporter plugin is found,
 *  it's enabled.  Only one ICrashReporter instance is supported at a time.
 *
 *  The crash report itself consists of multiple parts.  Some parts are only present on certain
 *  supported platforms.  All generated crash dump files will appear in the directory named by the
 *  "/crashreporter/dumpDir" setting.  If no value is provided, the current working directory
 *  is used instead.  The following parts could be expected:
 *    * A minidump file.  This is only generated on Windows.  This file will contain the state of
 *      the process's threads, stack memory, global memory space, register values, etc at the time
 *      of the crash.  This file will end in '.dmp'.
 *    * A stack trace of the crash point file.  This could be produced on all platforms.  This
 *      file will end in '.txt'.
 *    * A metadata file.  This is a TOML formatted file that contains all the metadata values that
 *      were known by the crash reporter at the time of the crash.  This file will end in '.toml'.
 *
 *  The crash reporter may have any number of arbitrary metadata values associated with it.  These
 *  values are defined as key/value pair strings.  There are two ways a metadata value can be
 *  defined:
 *    * Add a value to the `/crashreporter/data/` branch of the settings registry.  This can be
 *      done directly through the ISettings interface, adding a value to one of the app's config
 *      files, or by using the addCrashMetadata() utility function.  These values should be set
 *      once and either never or very rarely modified.  There is a non-trivial amount of work
 *      related to collecting a new metadata value in this manner that could lead to an overall
 *      performance impact if done too frequently.
 *    * Add a key and data callback to collect the current value of a metadata key for something
 *      that changes frequently.  This type of metadata value is added with addVolatileMetadata()
 *      on this interface.  These values may change as frequently as needed.  The current value
 *      will only ever be collected when a crash does occur or when the callback is removed.
 *
 *  Once a metadata value has been added to the crash reporter, it cannot be removed.  The value
 *  will remain even if the key is removed from `/crashreporter/data/` or its value callback is
 *  removed.  This is intentional so that as much data as possible can be collected to be sent
 *  with the crash report as is possible.
 *
 *  If a metadata key is registered as a volatile value, it will always override a key of the
 *  same name that is found under the `/crashreporter/data/` branch of the settings registry.
 *  Even if the volatile metadata value is removed or unregistered, it will still override any
 *  key of the same name found in the settings registry.
 *
 *  Metadata key names may or may not be case sensitive depending on their origin.  If a metadata
 *  value comes from the settings registry, its name is case sensitive since the settings registry
 *  is also case sensitive.  Metadata values that are registered as volatile metadata values do
 *  not have case sensitive names.  Attempting to register a new value under the same key but with
 *  different casing will fail since it would overwrite an existing name.  This difference is
 *  intentional to avoid confusion in the metadata output.  When adding metadata values through
 *  the settings registry, care should be taken to use consistent casing to avoid confusion in
 *  the output.
 */
struct ICrashReporter
{
    // 2.3: Added MetadataValueType::eWideString
    CARB_PLUGIN_INTERFACE("carb::crashreporter::ICrashReporter", 2, 3)

    /**
     * Upon crash, a crash dump is written to disk, uploaded, and then removed.  However, due to settings or because the
     * application is in an undefined state, the upload may fail. This method can be used on subsequent runs of the
     * application to attempt to upload/cleanup previous failed uploads.
     *
     * This method returns immediately, performing all uploads/removals asynchronously.  Supply an optional callback to
     * be notified when the uploads/removals have been completed.  The callback will be performed regardless of whether
     * the upload is successful.  However, each crash dump file will only be removed from the local file system if its
     * upload was successful and the "/crashreporter/preserveDump" setting is `false`.  A future call to this function
     * will try the upload again on failed crash dumps.
     *
     * The callback will be performed on the calling thread before return if there is no upload task to perform or if
     * the crash reporter is currently disabled.  In all other cases, the callback will be performed in the context of
     * another thread.  It is the caller's responsibility to ensure all accesses made in the callback are thread safe.
     * The supplied callback may neither directly nor indirectly access this instance of ICrashReporter.
     *
     * @thread_safety This method is thread safe and can be called concurrently.
     *
     * @param onDumpSubmitted The callback function to be called when the dumps are uploaded and deleted.
     * @param userData The user data to be passed to the callback function.
     */
    void(CARB_ABI* sendAndRemoveLeftOverDumpsAsync)(OnDumpSubmittedFn onDumpSubmitted, void* userData);

    /**
     * Adds a new callback that is called after sending (successfully or not) a crash dump to a server.
     *
     * Registration of multiple callbacks is allowed and all registered callbacks will be called serially (the order in
     * which callbacks are called is undefined). It is allowed to use the same callback function (and userData) multiple
     * times.
     *
     * This method is thread safe and can be called concurrently.
     *
     * The supplied callback may neither directly nor indirectly access this instance of ICrashReporter.
     *
     * @param onCrashSent The new callback to register, must not be nullptr.
     * @param userData The user data to be passed to the callback function, can be nullptr.
     *
     * @return Not null if the provided callback was successfully registered, nullptr otherwise.
     */
    CrashSentCallbackId*(CARB_ABI* addCrashSentCallback)(OnCrashSentFn onCrashSent, void* userData);

    /**
     * Removes previously registered callback.
     *
     * This method is thread safe and can be called concurrently.
     *
     * The given parameter is the id returned from addCrashSentCallback.
     *
     * The given callback id can be nullptr or an invalid id.
     *
     * @param callbackId The callback to remove. A null or invalid pointer is accepted (though may produce an error
     * message).
     */
    void(CARB_ABI* removeCrashSentCallback)(CrashSentCallbackId* callbackId);

    /**
     * Attempts to resolve a given address to a symbolic name using debugging features available to the system.
     *
     * If symbol resolution fails or is not available, @p func is called with a `nullptr` name.
     *
     * @note This function can be extremely slow. Use for debugging only.
     *
     * @param address The address to attempt to resolve.
     * @param func The func to call upon resolution
     * @param user User-specific data to be passed to @p func
     *
     * @thread_safety The callback function is always performed synchronously to this call.  It
     *                is the callee's responsibility to ensure safe access to both the @p user
     *                pointer and any global resources.
     */
    void(CARB_ABI* resolveSymbol)(const void* address, ResolveSymbolFn func, void* user);

    /** Adds a new volatile metadata value to the crash report.
     *
     *  @param[in] keyName      The name of the metadata key to set.  This must only contain
     *                          printable ASCII characters except for a double quote ('"'),
     *                          slash ('/'), or whitespace.  It is the caller's responsibility
     *                          to ensure the key name will not be overwriting another system's
     *                          metadata value.  One way to do this is to prefix the key name
     *                          with the name of the extension or plugin (sanitized to follow
     *                          the above formatting rules).  Volatile metadata key names are
     *                          not case sensitive.  This may not be nullptr or an empty string.
     *  @param[in] maxLength    The maximum number of characters, including the null terminator,
     *                          that the metadata's value will occupy when its value is retrieved.
     *                          This is ignored for integer and floating point values (the maximum
     *                          size for those types will always be used regardless of the value).
     *                          When retrieved, if the value is longer than this limit, the new
     *                          metadata value will truncated.  This may be 0 for integer and
     *                          floating point value types.  For string values, there may be
     *                          an arbitrary amount of extra space added internally.  This is
     *                          often for padding or alignment purposes.  Callers should however
     *                          neither count on this space being present nor expect any strings
     *                          to always be truncated at an exact length.
     *  @param[in] callback     The callback and data type that will provide the value for the new
     *                          metadata key.  This may not contain a `nullptr` callback function.
     *                          See below for notes on what the callback function may and may not
     *                          do.
     *  @param[in] context      An opaque context pointer that will be passed to the callback
     *                          function when called.  This will not be accessed or evaluated in
     *                          any way, but must remain valid for the entire duration that the
     *                          callback is registered here.
     *  @returns An identifier that can be used to unregister the callback in the event that the
     *           owning module needs to be unloaded.  It is the caller's responsibility to ensure
     *           that the metadata callback is properly unregistered with a call to
     *           removeVolatileMetadataValue() before it unloads.
     *
     *  @retval kMetadataFailBadParameter if an invalid parameter is passed in.
     *  @retval kMetadataFailKeyAlreadyUsed if the given key name is already in use or is a reserved name.
     *  @retval kInvalidMetadataId if a crash dump is currently in progress during this call.
     *
     *  @remarks This registers a new volatile metadata value with the crash reporter.  This new
     *           value includes a callback that will be used to acquire the most recent value of
     *           the metadata key when a crash does occur.  The value may be provided as either a
     *           signed or unsigned integer (64 bit), a floating point value (64 bit), or a string
     *           of arbitrary length.  Callback types are intentionally provided for each type to
     *           discourage the implementations from doing their own string conversions that could
     *           be dangerous while handling a crash event.
     *
     *  @remarks Because the process may be in an unstable or delicate state when the callback
     *           is performed to retrieve the metadata values, there are several restrictions on
     *           what the callback function can and cannot do.  In general, the callback function
     *           should provide the metadata value as quickly and simply as possible.  An ideal
     *           case would be just to return the current value of a local, global, or member
     *           variable.  Some guidelines are:
     *            * Do not perform any allocations or call into anything that may perform an
     *              allocation.  At the time of a crash many things could have gone wrong and the
     *              allocations could fail or hang for various reasons.
     *            * Do not use any STL container classes other than to retrieve a current value.
     *              Many STL container class operations can implicitly perform an allocation
     *              to resize a buffer, array, new node, etc.  If a resize, copy, or assign
     *              operation is unavoidable, try to use a container class that provides the
     *              possibility to reserve space for expected operations early (ie: string,
     *              vector, etc).
     *            * Avoid doing anything that may use a mutex or other locking primitive that
     *              is not in a strictly known state at the time.  During a crash, the state of
     *              any lock could be undefined leading to a hang if an attempt is made to
     *              acquire it.  If thread safety is a concern around accessing the value, try
     *              using an atomic variable instead of depending on a lock.
     *            * Do not make any calls into ICrashReporter from the callback function.  This
     *              will result in a deadlock.
     *            * Under no circumstances should a new thread be created by the callback.
     *
     *  @note The addVolatileMetadata() helper functions have been provided to make it easier
     *        to register callbacks for each value type.  Using these is preferable to calling
     *        into internalAddVolatileMetadata() directly.
     *
     *  @sa internalAddVolatileMetadata(), addVolatileMetadata().
     */
    /** @private */
    MetadataId(CARB_ABI* internalAddVolatileMetadata)(const char* keyName,
                                                      size_t maxLength,
                                                      MetadataValueCallback* callback,
                                                      void* context);

    /** Adds a new volatile metadata value to the crash report.
     *
     *  @param[in] keyName      The name of the metadata key to set.  This must only contain
     *                          printable ASCII characters except for a double quote ('"'),
     *                          slash ('/'), or whitespace.  It is the caller's responsibility
     *                          to ensure the key name will not be overwriting another system's
     *                          metadata value.  One way to do this is to prefix the key name
     *                          with the name of the extension or plugin (sanitized to follow
     *                          the above formatting rules).  Volatile metadata key names are
     *                          not case sensitive.  This may not be `nullptr` or an empty string.
     *  @param[in] callback     The callback function that will provide the value for the new
     *                          metadata key.  This may not be a `nullptr` callback function.
     *                          See below for notes on what the callback function may and may not
     *                          do.
     *  @param[in] context      An opaque context pointer that will be passed to the callback
     *                          function when called.  This will not be accessed or evaluated in
     *                          any way, but must remain valid for the entire duration that the
     *                          callback is registered here.
     *  @returns An identifier that can be used to unregister the callback in the event that the
     *           owning module needs to be unloaded.  It is the caller's responsibility to ensure
     *           that the metadata callback is properly unregistered with a call to
     *           removeVolatileMetadataValue() before it unloads.
     *
     *  @retval kMetadataFailBadParameter if an invalid parameter is passed in.
     *  @retval kMetadataFailKeyAlreadyUsed if the given key name is already in use or is a reserved name.
     *  @retval kInvalidMetadataId if a crash dump is currently in progress during this call.
     *
     *  @remarks This registers a new volatile metadata value with the crash reporter.  This new
     *           value includes a callback that will be used to acquire the most recent value of
     *           the metadata key when a crash does occur.  The value may be provided as either a
     *           signed or unsigned integer (64 bit), a floating point value (64 bit), or a string
     *           of arbitrary length.  Callback types are intentionally provided for each type to
     *           discourage the implementations from doing their own string conversions that could
     *           be dangerous while handling a crash event.
     *
     *  @remarks Because the process may be in an unstable or delicate state when the callback
     *           is performed to retrieve the metadata values, there are several restrictions on
     *           what the callback function can and cannot do.  In general, the callback function
     *           should provide the metadata value as quickly and simply as possible.  An ideal
     *           case would be just to return the current value of a local, global, or member
     *           variable.  Some guidelines are:
     *            * Do not perform any allocations or call into anything that may perform an
     *              allocation.  At the time of a crash, many things could have gone wrong and
     *              allocations could fail or hang for various reasons.
     *            * Do not use any STL container classes other than to retrieve a current value.
     *              Many STL container class operations can implicitly perform an allocation
     *              to resize a buffer, array, new node, etc.  If a resize, copy, or assign
     *              operation is unavoidable, try to use a container class that provides the
     *              possibility to reserve space for expected operations early (ie: string,
     *              vector, etc).
     *            * Avoid doing anything that may use a mutex or other locking primitive that
     *              is not in a strictly known state at the time.  During a crash, the state of
     *              any lock could be undefined leading to a hang if an attempt is made to
     *              acquire it.  If thread safety is a concern around accessing the value, try
     *              using an atomic variable instead of depending on a lock.
     *            * Do not make any calls into ICrashReporter from the callback function.  This
     *              will result in a deadlock.
     *            * Under no circumstances should a new thread be created by the callback.
     *
     *  @thread_safety This call is thread safe.
     */
    MetadataId addVolatileMetadata(const char* keyName, OnGetMetadataIntegerFn callback, void* context);

    /** @copydoc addVolatileMetadata(const char*,OnGetMetadataIntegerFn,void*) */
    MetadataId addVolatileMetadata(const char* keyName, OnGetMetadataUIntegerFn callback, void* context);

    /** @copydoc addVolatileMetadata(const char*,OnGetMetadataIntegerFn,void*) */
    MetadataId addVolatileMetadata(const char* keyName, OnGetMetadataFloatFn callback, void* context);

    /** @copydoc addVolatileMetadata(const char*,OnGetMetadataIntegerFn,void*)
     *  @param[in] maxLength    The maximum number of characters, including the null terminator,
     *                          that the metadata's value will occupy when its value is retrieved.
     *                          When retrieved, if the value is longer than this limit, this new
     *                          metadata value will be truncated.  There may be an arbitrary
     *                          amount of extra space added internally.  This is often done for
     *                          padding or alignment purposes.  Callers should however neither
     *                          count on this space being present nor expect any strings to always
     *                          be truncated at an exact length.
     */
    MetadataId addVolatileMetadata(const char* keyName, size_t maxLength, OnGetMetadataStringFn callback, void* context);

    /** @copydoc addVolatileMetadata(const char*,size_t,OnGetMetadataStringFn,void*) */
    MetadataId addVolatileMetadata(const char* keyName, size_t maxLength, OnGetMetadataWideStringFn callback, void* context);

    /** Removes a previously registered volatile metadata value.
     *
     *  @param[in] id   The identifier of the metadata value to remove.  This was returned from
     *                  a previous successful call to addVolatileMetadata*().  This call will be
     *                  ignored if the identifier is invalid.
     *  @returns No return value.
     *
     *  @remarks This removes a volatile metadata value from the crash reporter.  The value will
     *           be retrieved from the callback and stored internally before it is removed from
     *           the crash reporter.  The given identifier will be invalid upon return.
     *
     *  @sa internalAddVolatileMetadata(), addVolatileMetadata().
     */
    void(CARB_ABI* removeVolatileMetadataValue)(MetadataId id);
};

inline MetadataId ICrashReporter::addVolatileMetadata(const char* keyName, OnGetMetadataIntegerFn callback, void* context)
{
    MetadataValueCallback data;

    data.type = MetadataValueType::eInteger;
    data.fn.getInteger = callback;

    return internalAddVolatileMetadata(keyName, 0, &data, context);
}

inline MetadataId ICrashReporter::addVolatileMetadata(const char* keyName, OnGetMetadataUIntegerFn callback, void* context)
{
    MetadataValueCallback data;

    data.type = MetadataValueType::eUInteger;
    data.fn.getUInteger = callback;

    return internalAddVolatileMetadata(keyName, 0, &data, context);
}

inline MetadataId ICrashReporter::addVolatileMetadata(const char* keyName, OnGetMetadataFloatFn callback, void* context)
{
    MetadataValueCallback data;

    data.type = MetadataValueType::eFloat;
    data.fn.getFloat = callback;

    return internalAddVolatileMetadata(keyName, 0, &data, context);
}

inline MetadataId ICrashReporter::addVolatileMetadata(const char* keyName,
                                                      size_t maxLength,
                                                      OnGetMetadataStringFn callback,
                                                      void* context)
{
    MetadataValueCallback data;

    data.type = MetadataValueType::eString;
    data.fn.getString = callback;

    return internalAddVolatileMetadata(keyName, maxLength, &data, context);
}

inline MetadataId ICrashReporter::addVolatileMetadata(const char* keyName,
                                                      size_t maxLength,
                                                      OnGetMetadataWideStringFn callback,
                                                      void* context)
{
    MetadataValueCallback data;

    data.type = MetadataValueType::eWideString;
    data.fn.getWString = callback;

    return internalAddVolatileMetadata(keyName, maxLength, &data, context);
}

} // namespace crashreporter
} // namespace carb
