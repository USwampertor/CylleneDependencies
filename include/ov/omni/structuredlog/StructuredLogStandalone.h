// Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Helper header to be able to use the `omni.structuredlog.plugin` plugin in a standalone
 *         mode.  When using this mode, the rest of the Carbonite framework is not necessary.
 *         Only the single module `omni.structuredlog.plugin` library will be needed.  The main
 *         feature of this header is the `omni::structuredlog::StructuredLogStandalone` helper
 *         class.  This manages loading the structured log library, registers all schemas for
 *         the calling module, and allows access to the supported structured log interfaces.
 *
 *  @note In order to use the structured logging system in standalone mode, this header must
 *        be included instead of any other structured log headers.  This header will pull in
 *        all other structured log interface headers that are supported in standalone mode.
 *        Other structured log headers are neither guaranteed to compile nor function as
 *        expected in a non-Carbonite app.
 *
 *  @note It is left up to the host app to handle launching the telemetry transmitter app
 *        if that is needed.  When used in standalone mode in a non-Carbonite app, this header
 *        is only intended to provide the functionality for emitting log messages.  The host
 *        app can either rely on another external Omniverse app to launch the transmitter for
 *        it, or launch it manually if needed.
 */
#pragma once

#define STRUCTUREDLOG_STANDALONE_MODE 1

#include "../../carb/Defines.h"
#include "IStructuredLog.h"
#include "IStructuredLogSettings.h"
#include "IStructuredLogControl.h"


namespace omni
{
namespace structuredlog
{

/** Helper class to provide structured log functionality in non-Carbonite based apps.  This
 *  provides loading and shutdown functionality for the library and also allows for some
 *  common setup in one easy call.  This class is intended to be able to gracefully fail
 *  if the structured log plugin isn't available or couldn't be loaded for any reason.
 *  As long as the init() method returns `true`, it can be assumed that all functionality
 *  and features are present and available.
 *
 *  Once initialized, this object doesn't necessarily need to be interacted with directly
 *  any further.  As long as the object exists, the structured log functionality is available.
 *  Once this object is destroyed however, the structured log functionality cannot be
 *  guaranteed to be available any more.  It is intended for this object to be instantiated
 *  and initialized in main() or at the global scope in the process' main module.  It can
 *  also be used from other libraries if they want to integrate structured logging as well.
 *  Only a single instance of this object should be necessary.
 *
 *  Before any structured logging features can be used, the object must be initialized
 *  with init().  This allows the log path and the default log filename to be specified
 *  and will also load the library and register all the schemas for the calling module.
 *  Note that if no schemas are registered, no log messages will be emitted, the calls
 *  will just be silently ignored.  If modules other than the process' main module also
 *  have schemas to be registered, they can either call registerSchemas() from this
 *  class (from the process's single instantiation) or they can make a call into
 *  `omni::structuredlog::addModulesSchemas()` from within the other modules.  It is
 *  safe to call those functions even if no schemas are used in a module or if the
 *  structured log system has not been initialized yet.  If either are called before
 *  the structured log system has been initialized, an attempt will be made to load
 *  the library first.
 *
 *  @note On Windows it is expected that this object be instantiated in the process'
 *        main module.  If it is instantiated from a DLL it will not be guaranteed
 *        that all pending log messages will be flushed to disk before the process
 *        exits.  If instantiating this from a DLL is unavoidable, it is the app's
 *        responsibility to call flush() before shutdown to ensure all messages have
 *        been flushed to disk.
 *
 *        This requirement is caused by the way that Windows processes shutdown and
 *        is unfortunately not possible to work around.  When exiting the process
 *        by returning from main(), the CRT is shutdown completely before any DLLs
 *        get a chance to do any kind of cleanup task, and ntdll will kill all threads
 *        except the exiting one.  This means that there is a possibility that attempting
 *        to flush the queue could result in a deadlock.  Further, if any cleanup code
 *        tries to use a win32 locking primitive (ie: SRW lock, critical section, etc)
 *        the process may just be terminated immediately even in the middle of cleanup.
 */
class StructuredLogStandalone
{
public:
    StructuredLogStandalone() = default;

    /** Destructor: flushes the logging queue to disk and cleans up.
     *
     *  @remarks This ensures that all log messages have been flushed to disk and puts the
     *           structured logging system in a state where it can safely be cleaned up
     *           without issue.
     */
    ~StructuredLogStandalone()
    {
        // stop the log queue to guarantee that all messages have been flushed to disk.
        flush();

        // release the objects explicitly to make debugging any release issues easier.  Note that
        // this isn't strictly necessary since they will be released anyway when cleaning up this
        // object.
        log.release();
        settings.release();
        control.release();
    }

    bool init(const char* logPath = nullptr, const char* defaultLogName = nullptr)
    {
        // When in standalone mode, the structured log plugin is set to load itself when the
        // omniGetStructuredLogWithoutAcquire() function is called by anything.  This is the main
        // entry point to grab its instance in standalone mode.  It is called by (among other things)
        // `omni::structuredlog::addModuleSchemas()`.  This function needs to be called regardless
        // in standalone mode in order to register the schemas that have been included in the
        // calling module.  It must be called once by each module that wants to use structured
        // logging in standalone mode.  In non-standalone mode (ie: with carb), this step is done
        // automatically on module load.
        registerSchemas();

        // grab the structured log object so we can grab the settings interface from it and setup
        // the configuration that's needed here.  There isn't strictly anything that we must do here,
        // but in our case we want to change the log directory and default log name.  We can also do
        // things like change the user ID, the queue size, some of the message formatting, etc.  All
        // of these additional tasks are done through the `omni::structuredlog::IStructuredLogSettings`
        // interface.  This can be acquired with `strucLog.as<omni::structuredlog::IStructuredLogSettings>()`.
        log = omni::core::borrow(omniGetStructuredLogWithoutAcquire());

        if (log == nullptr)
            return false;

        settings = log.as<omni::structuredlog::IStructuredLogSettings>();
        control = log.as<omni::structuredlog::IStructuredLogControl>();

        if (settings != nullptr)
        {
            if (logPath != nullptr)
                settings->setLogOutputPath(logPath);

            if (defaultLogName != nullptr)
                settings->setLogDefaultName(defaultLogName);
        }

        return log != nullptr && settings != nullptr && control != nullptr;
    }

    /** Registers all schemas used by the calling module.
     *
     *  @returns No return value.
     *
     *  @remarks This registers all schemas that have been included in the calling module.  When
     *           any source file in any module includes a schema header, an entry for it is
     *           automatically added to a list local to the module.  When this is called from
     *           within the context of that module, all schemas for that module will be registered
     *           and become available for use.
     *
     *  @remarks This must be called from each module that includes a schema header.  If it is
     *           not, emitting a log message for an unregistered schema will be silently ignored.
     *           It is possible however that the same schema could be used in multiple modules.
     *           If that is the case, it only needs to be registered once, then all modules in
     *           the process may use it.  It is safe to register any given schema multiple times.
     *           After it is registered once, later attempts to re-register it will just succeed
     *           immediately.
     *
     *  @note This is called from init() as well.  Any module that calls init() does not also have
     *        to explicitly call this.
     */
    void registerSchemas()
    {
        addModulesSchemas();
    }

    /** Flushes all pending log messages to disk.
     *
     *  @returns No return value.
     *
     *  @remarks This flushes all pending log messages to disk.  Upon return, any messages that
     *           has been issued before the call will have made it to disk.  If there is another
     *           thread emitting a message during this call, it is undefined whether it will
     *           be fully flushed to disk.  This should be called in situations where the caller
     *           can guarantee that no messages are in the process of being emitted.
     *
     *  @note This should be called at points where messages must be guaranteed to be present on
     *        disk.  This includes process exit time.  This will be called implicitly when this
     *        object is destroyed, but if an exit path is taken that will not guarantee this
     *        object is destroyed (ie: calling `_exit()`, `TerminateProcess()`, etc), this
     *        can be called explicitly to accomplish the same result.
     */
    void flush()
    {
        if (control != nullptr)
            control->stop();
    }

    /** Various structured log objects.  These are publicly accessible so that callers can use
     *  them directly to get direct access to the structured log features that may be needed.
     *  As long as init() succeeds, these objects will be valid.
     *  @{
     */
    /** Main IStructuredLog instance object.  This is a global singleton that provides direct
     *  access to the functionality for registering new schemas (manually), enabling and disabling
     *  events or schemas, and emitting messages.
     */
    omni::core::ObjectPtr<omni::structuredlog::IStructuredLog> log;

    /** Structured log settings interface.  This is used to make changes to the various settings
     *  for the structured logging system and to retrieve information about its current settings.
     *  The most common uses of this are to change the log directory or name (though that is
     *  already done more easily through init()).
     */
    omni::core::ObjectPtr<omni::structuredlog::IStructuredLogSettings> settings;

    /** Structured log control interface.  This is used to stop and flush the message queue and
     *  to ensure any open log files are closed.  Closing a log file is only necessary for example
     *  to ensure a log file would not prevent a directory from being deleted on Windows.
     */
    omni::core::ObjectPtr<omni::structuredlog::IStructuredLogControl> control;
    /** @} */
};

} // namespace structuredlog
} // namespace omni
