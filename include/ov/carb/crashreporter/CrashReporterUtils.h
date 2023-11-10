// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
///! @file
///! @brief Utility helper functions for the crash reporter.
#pragma once

#include "../Framework.h"
#include "../InterfaceUtils.h"
#include "../logging/Log.h"
#include "../settings/ISettings.h"
#include "ICrashReporter.h"

#include <string.h>
#include <future>

/** Global accessor object for the loaded ICrashReporter object.  This is intended to be used
 *  as a shortcut for accessing the @ref carb::crashreporter::ICrashReporter instance if the
 *  crash reporter plugin has been loaded in the process.  This will be `nullptr` if the
 *  crash reporter plugin is not loaded.  This symbol is unique to each plugin module and
 *  will be filled in by the framework upon load if the crash reporter plugin is present.
 *  Callers should always check if this value is `nullptr` before accessing it.  This should
 *  not be accessed during or after framework shutdown.
 */
CARB_WEAKLINK carb::crashreporter::ICrashReporter* g_carbCrashReporter;

/** Defines global symbols specifically related to the crash reporter.  Currently none are
 *  defined, but this provides a placeholder in the event some need to be added.
 */
#define CARB_CRASH_REPORTER_GLOBALS()

namespace carb
{
/** Namespace for the crash reporter. */
namespace crashreporter
{

/** Registers the crash reporter for this process and sets it up.
 *
 *  @returns No return value.
 *
 *  @remarks This installs the crash reporter in the calling process.  This will include
 *           installing the crash handler hook and setting up its state according to the
 *           current values in the `/crashreporter/` branch of the settings registry.
 *           If the ISettings interface is not available, the crash reporter will only
 *           use its default settings and many features will be disabled.  In this case
 *           the disabled features will include monitoring for changes to the various
 *           `/crashreporter/` settings, specifying metadata to include in crash reports,
 *           and controlling how and where the crash dump files are written out.
 *
 *  @note When the process is shutting down, the crash reporter should be disabled
 *        by calling @ref carb::crashreporter::deregisterCrashReporterForClient().
 *        It is the host app's responsibility to properly disable the crash reporter
 *        before the plugin is unloaded.
 *
 *  @thread_safety This operation is not thread safe.  It is the caller's responsibility
 *                 to ensure this is only called from a single thread at any given time.
 *                 However, this will be automatically called during Carbonite framework
 *                 startup (in carb::startupFramework()) and does not necessarily need
 *                 to be called directly.
 */
inline void registerCrashReporterForClient()
{
    g_carbCrashReporter = getFramework()->tryAcquireInterface<ICrashReporter>();
}

/** Deregisters and disables the crash reporter for the calling process.
 *
 *  @returns No return value.
 *
 *  @remarks This removes the crash reporter interface from the global variable
 *           @ref g_carbCrashReporter so that callers cannot access it further.
 *           The crash reporter plugin is also potentially unloaded.
 *
 *  @thread_safety This operation is not thread safe.  It is the caller's responsibility
 *                 to ensure this is only called from a single thread at any given time.
 *                 However, this will be automatically called during Carbonite framework
 *                 shutdown (in carb::shutdownFramework()) and does not necessarily need
 *                 to be called directly.
 */
inline void deregisterCrashReporterForClient()
{
    if (g_carbCrashReporter)
    {
        getFramework()->releaseInterface(g_carbCrashReporter);
        g_carbCrashReporter = nullptr;
    }
}

/** Attempts to upload any crash dump files left by a previously crashed process.
 *
 *  @returns A future that can be used to check on the completion of the upload operation.
 *           The operation is fully asynchronous and will proceed on its own.  The future
 *           object will be signaled once the operation completes, successfully or otherwise.
 *
 *  @remarks This starts off the process of checking for and uploading old crash dump files
 *           that may have been left over by a previous crashed process.  This situation can
 *           occur if the upload failed in the previous process (ie: network connection
 *           issue, etc), or the process crashed again during the upload.  A list of old
 *           crash dump files will be searched for in the currently set dump directory
 *           (as set by `/crashreporter/dumpDir`).  If any are found, they will be uploaded
 *           one by one to the currently set upload URL (`/crashreporter/url`).  Each
 *           crash dump file will be uploaded with its original metadata if the matching
 *           metadata file can be found.  Once a file has been successfully uploaded to
 *           the given upload URL, it will be deleted from local storage unless the
 *           `/crashreporter/preserveDump` setting is `true`.  This entire process will
 *           be skipped if the `/crashreporter/skipOldDumpUpload` setting is `true` and
 *           this call will simply return immediately.
 *
 *  @thread_safety This function is thread safe.  If multiple calls are made while an upload
 *                 is still in progress, a new task will just be added to the upload queue
 *                 instead of starting off another upload thread.
 *
 *  @note If an upload is in progress when the process tries to exit or the crash reporter
 *        plugin tries to unload, any remaining uploads will be canceled, but the current
 *        upload operation will wait to complete.  If this is a large file being uploaded
 *        or the internet connection's upload speed is particularly slow, this could potentially
 *        take a long time.  There is unfortunately no reliable way to cancel this upload
 *        in progress currently.
 */
inline std::future<void> sendAndRemoveLeftOverDumpsAsync()
{
    std::unique_ptr<std::promise<void>> sentPromise(new std::promise<void>());

    std::future<void> sentFuture(sentPromise->get_future());

    if (g_carbCrashReporter)
    {
        const auto finishCallback = [](void* promisePtr) {
            auto sentPromise = reinterpret_cast<std::promise<void>*>(promisePtr);
            sentPromise->set_value();
            delete sentPromise;
        };
        g_carbCrashReporter->sendAndRemoveLeftOverDumpsAsync(finishCallback, sentPromise.release());
    }
    else
    {
        CARB_LOG_WARN("No crash reporter present, dumps uploading isn't available.");
        sentPromise->set_value();
    }
    return sentFuture;
}

/** Namespace for internal helper functions. */
namespace detail
{
/** Sanitizes a string to be usable as a key name in the settings registry.
 *
 *  @param[in] keyName  The key string to be sanitized.  This may be any string in theory, but
 *                      should really be a short descriptive name for a crash metadata value or
 *                      extra crash file.  All non-ASCII, non-numeric characters will be replaced
 *                      with underscores.
 *  @returns A string containing the sanitized key name.
 *
 *  @note This is called internally by the addExtraCrashFile() and isExtraCrashFileKeyUsed()
 *        functions and should not be called directly.
 */
inline std::string sanitizeExtraCrashFileKey(const char* keyName)
{
    std::string key = keyName;

    // sanitize the key name so that it contains only database friendly characters.
    for (auto& c : key)
    {
        if (c <= ' ' || c >= 127 || strchr("\"'\\/,#$%^&*()!~`[]{}|<>?;:=+.\t\b\n\r ", c) != nullptr)
        {
            c = '_';
        }
    }

    return key;
}
} // namespace detail

/** Adds a metadata value to the crash reporter.
 *
 *  @tparam T           The type of the value to set.  This may be any value type that is
 *                      compatible with @a std::to_string().
 *  @param[in] keyName  The name of the metadata key to set.  This must only contain printable
 *                      ASCII characters except for a double quote ('"'), slash ('/'), or
 *                      whitespace.  These rules get the key to a format that can be accepted
 *                      by the settings registry.  Note that further sanitization on the key
 *                      name may also occur later.  Any character that is not suitable for a
 *                      database key name will be replaced by an underscore ('_').  It is the
 *                      caller's responsibility to ensure the key name will not be overwriting
 *                      another system's metadata value.  One way to do this is to prefix the
 *                      key name with the name of the extension or plugin (sanitized to follow
 *                      the above formatting rules).
 *  @param[in] value    The value to add to the crash reporter's metadata table.  This may be
 *                      any string that is accepted by carb::settings::ISettings::setString()
 *                      as a new value.  Note that this will remove the metadata value if it is
 *                      set to `nullptr` or an empty string.
 *  @returns `true` if the new metadata value is successfully set.  Returns `false` otherwise.
 *
 *  @remarks This adds a new metadata value to the crash reporter.  When a crash occurs, all
 *           values added through here will be collected and transmitted as metadata to
 *           accompany the crash report.  The metadata value will value will be added (or
 *           updated) to the crash reporter by adding (or updating) a key under the
 *           "/crashreporter/data/" settings branch.
 *
 *  @note This should not be called frequently to update the value of a piece of metadata.
 *        Doing so will be likely to incur a performance hit since the crash reporter watches
 *        for changes on the "/crashreporter/data/" settings branch that is modified here.
 *        Each time the branch changes, the crash reporter's metadata list is updated.  If
 *        possible, the value for any given piece of metadata should only be updated when
 *        it either changes or just set once on startup and left alone.
 */
template <typename T>
inline bool addCrashMetadata(const char* keyName, T value)
{
    return addCrashMetadata(keyName, std::to_string(value).c_str());
}

/** @copydoc carb::crashreporter::addCrashMetadata(const char*,T). */
template <>
inline bool addCrashMetadata(const char* keyName, const char* value)
{
    carb::settings::ISettings* settings = carb::getCachedInterface<carb::settings::ISettings>();
    std::string key;

    if (settings == nullptr)
        return false;

    key = detail::sanitizeExtraCrashFileKey(keyName);
    settings->setString((std::string("/crashreporter/data/") + key).c_str(), value);

    return true;
}

/** Adds an extra file to be uploaded when a crash occurs.
 *
 *  @param[in] keyName  The name of the key to give to the file.  This is what the file will be
 *                      uploaded as.  Using the file's original name should be fine in most
 *                      cases, however it should not contain characters such as '/' or '\'
 *                      at the very least.  Non-ASCII characters should be avoided if possible
 *                      too.  It is the caller's responsibility to ensure adding this new file
 *                      will not overwrite another upload file with the same key name.  This
 *                      may not use the reserved name 'upload_file_minidump'.  This key name
 *                      string will always be sanitized to only contain database friendly
 *                      characters.  All invalid characters will be replaced by an underscore
 *                      ('_').
 *  @param[in] filename The full path to the file to upload.  This may be a relative or absolute
 *                      path.  The file may or may not exist at the time of this call, it will
 *                      still be added to the list of files to be uploaded.  If the file does not
 *                      exist at the time of the crash, it will be filtered out of the list at
 *                      that point.  A warnings message will be written out for each listed file
 *                      that is missing at the time of the crash however.
 *  @returns `true` if the new entry is added to the list.  Returns `false` if the file could
 *           not be added.  This failure will only occur if the `ISettings` interface is not
 *           available.  Note that a `true` return does not necessarily mean that the new file
 *           was fully added to the list.  It would have been written to the list in the settings
 *           registry, but may have been ignored by the crash reporter if the same key was given
 *           as a previous file.
 *
 *  @remarks This adds a filename to be tracked to upload with the next crash report that is
 *           generated.  This setting is not persistent across sessions.  If no crash occurs,
 *           the file will not be uploaded anywhere.  This cannot be used to rename a file that
 *           has already been added to the upload list (ie: change the filename under an existing
 *           key).  If a second filename is specified with the same key, it will be ignored.
 *
 *  @note Extra files added with this function will not be deleted once a crash report is
 *        successfully uploaded.  Only the crash report's main dump file and metadata files
 *        will be deleted in this case.
 */
inline bool addExtraCrashFile(const char* keyName, const char* filename)
{
    carb::settings::ISettings* settings = carb::getCachedInterface<carb::settings::ISettings>();
    std::string key;

    if (settings == nullptr)
        return false;

    // sanitize the key name so that it contains only database friendly characters.
    key = detail::sanitizeExtraCrashFileKey(keyName);

    settings->setString(("/crashreporter/files/" + key).c_str(), filename);

    return true;
}

/** Checks whether a key for an extra crash report file has already been used.
 *
 *  @param[in] keyName  The name of the key to be used.  This will be used to identify the extra
 *                      file in the settings registry.  See addExtraCrashFile() for more
 *                      information on how this value is used.
 *  @returns `true` if the crash file key has been used already.  Returns `false` otherwise.
 *
 *  @remarks When adding new extra files to a crash report, it is the caller's responsibility
 *           that an existing filename will not be overwritten by addExtraCrashFile().  This
 *           function can be used to check whether a given key had already been used to add
 *           an extra file to the crash report.
 */
inline bool isExtraCrashFileKeyUsed(const char* keyName)
{
    carb::settings::ISettings* settings = carb::getCachedInterface<carb::settings::ISettings>();
    std::string key;

    if (settings == nullptr)
        return false;

    // sanitize the key name so that it contains only database friendly characters.
    key = detail::sanitizeExtraCrashFileKey(keyName);

    return settings->isAccessibleAs(carb::dictionary::ItemType::eString, ("/crashreporter/files/" + key).c_str());
}

} // namespace crashreporter
} // namespace carb
