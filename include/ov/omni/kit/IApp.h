// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include <carb/Interface.h>
#include <carb/events/EventsUtils.h>
#include <carb/events/IEvents.h>
#include <carb/extras/Timer.h>
#include <carb/profiler/Profile.h>
#include <omni/String.h>


namespace omni
{

namespace ext
{
class ExtensionManager;
}

namespace kit
{

class IRunLoopRunner;


/**
 * Application descriptor.
 */
struct AppDesc
{
    const char* carbAppName; ///! Carb application name. If nullptr it is derived from the executable filename.
    const char* carbAppPath; ///! Carb application path. If nullptr it is derived from the executable folder.
    int argc;
    char** argv;
};

/**
 * Build information. Baked at build time.
 */
struct BuildInfo
{
    omni::string kitVersion; ///! Full kit version, e.g. `103.5+release.7032.aac30830.tc.windows-x86_64.release'
    omni::string kitVersionShort; ///! Short kit version, currently major.minor. e.g. `103.5`
    omni::string kitVersionHash; ///! Git hash of kit build, 8 letters, e.g. `aac30830`
    omni::string kernelVersion; ///! Full kit kernel version, e.g.
                                ///`103.5+release.7032.aac30830.tc.windows-x86_64.release'
};

/**
 * Platform information.
 */
struct PlatformInfo
{
    const char* config; ///! "debug" or "release"
    const char* platform; ///! "windows-x86_64", "linux-x86_64", "linux-aarch64" etc.
    const char* pythonVersion; ///! "cp36", "cp37" etc.
};

/**
 * App information. Basic configuration of current app run, comes mostly from kit file and other settings.
 */
struct AppInfo
{
    omni::string filename; ///! App filename. Name of a kit file or just 'kit'
    omni::string name; ///! App name. It is app/name setting if defined, otherwise same as `filename`
    omni::string version; ///! App version. Version in kit file or kit version
    omni::string versionShort; ///! Short app version, currently major.minor, e.g. `2021.3`
    omni::string environment; ///! Name of the environment we are running in. (/app/environment/name setting, e.g.:
                              ///! teamcity, launcher, etm, default)
    bool isExternal; ///! Is external (public) configuration
};

/**
 * Run Loop. Contains various event streams pumped every spin of run loop.
 */
class RunLoop
{
public:
    carb::events::IEventStream* preUpdate; ///! Pre update events pushed every loop and stream is pumped.
    carb::events::IEventStream* update; ///! Update events pushed every loop and stream is pumped.
    carb::events::IEventStream* postUpdate; ///! Post update events pushed every loop and stream is pumped.
    carb::events::IEventStream* messageBus; ///! Stream for extensions to push events to, pumped every loop.
};

/**
 * How to handle passed arguments when restarting an app.
 */
enum class RestartArgsPolicy
{
    eAppend, ///! Append to existing args
    eReplace, ///! Replace existing args
};

/**
 * Few predefined run loop names. Serve as hints, doesn't have to use only those.
 */
constexpr const char* const kRunLoopDefault = "main";
constexpr const char* const kRunLoopSimulation = "simulation";
constexpr const char* const kRunLoopRendering = "rendering";
constexpr const char* const kRunLoopUi = "ui";

class IAppScripting;

/**
 * App shutdown events pumped into IApp::getShutdownEventStream():
 */
constexpr const carb::events::EventType kPostQuitEventType = 0;
constexpr const carb::events::EventType kPreShutdownEventType = 1;

/**
 * App startup events pumped into IApp::getStartupEventStream():
 */
const carb::events::EventType kEventAppStarted = CARB_EVENTS_TYPE_FROM_STR("APP_STARTED");
const carb::events::EventType kEventAppReady = CARB_EVENTS_TYPE_FROM_STR("APP_READY");

/**
 * Main Kit Core Application plugin.
 *
 * It runs all Kit extensions and contains necessary pieces to make them work together: settings, events, python, update
 * loop
 */
class IApp
{
public:
    CARB_PLUGIN_INTERFACE("omni::kit::IApp", 1, 3);

    //////////// main API ////////////

    /**
     * Startup the application.
     */
    virtual void startup(const AppDesc& desc) = 0;

    /**
     * Call one update loop iteration on application.
     */
    virtual void update() = 0;

    /**
     * Is application running? When false - stop calling update() and call shutdown().
     */
    virtual bool isRunning() = 0;

    /**
     * Shutdown the application. Returns execution code (error code).
     */
    virtual int shutdown() = 0;

    /**
     * Inline helper to startup, loop and shutdown (see implementation below).
     */
    int run(const AppDesc& desc);

    //////////// extensions API ////////////

    /**
     * Quit application (doesn't quit immediately. Signals that application should be finishing now).
     */
    virtual void postQuit(int returnCode = 0) = 0;

    /**
     * Log events stream. Contains logging events. Pumped every update.
     */
    virtual carb::events::IEventStream* getLogEventStream() = 0;

    /**
     * Replays recorded log messages for the specified target.
     */
    virtual void replayLogMessages(carb::logging::Logger* target) = 0;

    /**
     * Toggles log message recording.
     */
    virtual void toggleLogMessageRecording(bool logMessageRecordingEnabled) = 0;

    /**
     * Get Run Loop by name. It is being created if it doesn't exist. `nullptr` can be passed to get default run loop.
     */
    virtual RunLoop* getRunLoop(const char* name) = 0;

    /**
     * Returns whether the run loop specified is active
     */
    virtual bool isRunLoopAlive(const char* name) = 0;

    /**
     * Requests the run loop to terminate
     * Set block=true to not return until run loop has completed execution
     */
    virtual void terminateRunLoop(const char* name, bool block) = 0;

    /**
     * Pre-update events stream. Pre-update events and pushed and poped every frame.
     */
    carb::events::IEventStream* getPreUpdateEventStream(const char* runLoopName = kRunLoopDefault)
    {
        return getRunLoop(runLoopName)->preUpdate;
    }

    /**
     * Update events stream. Update events and pushed and poped every frame.
     */
    carb::events::IEventStream* getUpdateEventStream(const char* runLoopName = kRunLoopDefault)
    {
        return getRunLoop(runLoopName)->update;
    }

    /**
     * Post-update events stream. Post-update events and pushed and poped every frame.
     */
    carb::events::IEventStream* getPostUpdateEventStream(const char* runLoopName = kRunLoopDefault)
    {
        return getRunLoop(runLoopName)->postUpdate;
    }

    /**
     * Non-specific message bus. Arbitrary clients push to it, application pops it once per frame after the post-update.
     */
    carb::events::IEventStream* getMessageBusEventStream(const char* runLoopName = kRunLoopDefault)
    {
        return getRunLoop(runLoopName)->messageBus;
    }

    /**
     * Set particular Run Loop runner implementation. This function must be called only once during app startup. If
     * no `IRunLoopRunner` was set application will quit. If `IRunLoopRunner` is set it becomes responsible for spinning
     * run loops. Application will call in functions on `IRunLoopRunner` interface to communicate the intent.
     */
    virtual void setRunLoopRunner(IRunLoopRunner* runner) = 0;

    /**
     * Extension manager instance. Refer to it for API to control extensions.
     */
    virtual omni::ext::ExtensionManager* getExtensionManager() = 0;

    /**
     * Get python scripting engine API.
     */
    virtual IAppScripting* getPythonScripting() = 0;

    /**
     * Build version string.
     */
    virtual const char* getBuildVersion() = 0;

    /**
     * Is this Kit App plugin was built in DEBUG configuration.
     *
     * @return true if DEBUG, false if RELEASE.
     */
    virtual bool isDebugBuild() = 0;

    /**
     * Information about platform currently being run on.
     */
    virtual PlatformInfo getPlatformInfo() = 0;

    /**
     * @param timeScale Desired time scale for the returned time interval (seconds, milliseconds, etc.)
     * @return time passed since the last application startup call.
     */
    virtual double getTimeSinceStart(carb::extras::Timer::Scale timeScale = carb::extras::Timer::Scale::eMilliseconds) = 0;

    /**
     * @return time passed since the last update in milliseconds.
     */
    virtual double getLastUpdateTime() = 0;

    /**
     * Current update number. Starts with 0, incremented every update.
     */
    virtual uint32_t getUpdateNumber() = 0;

    /**
     * Generic function to output messages targeted for developers. Usually they go into stdout and carb log.
     */
    virtual void printAndLog(const char* message) = 0;

    /**
     * Shutdown events stream. Contains shutdown events. Pumped when shutdown is requested.
     */
    virtual carb::events::IEventStream* getShutdownEventStream() = 0;

    /**
     * Request shutdown interruption. Will not interrup uncancellable shutdown requests.
     */
    virtual bool tryCancelShutdown(const char* reason) = 0;

    /**
     * Similar to postQuit, but thutdown requested this way cannot be interrupted with tryCancelShutdown.
     */
    virtual void postUncancellableQuit(int returnCode) = 0;

    /**
     * Startup events stream. Contains app startup events, like `kEventAppStarted`. Pumped with each event.
     */
    virtual carb::events::IEventStream* getStartupEventStream() = 0;

    /**
     * Is app in "ready" state? true if `kEventAppReady` was fired.
     */
    virtual bool isAppReady() = 0;

    /**
     * Use this function before the first update to delay `kEventAppReady` until the next update.
     *
     * On the first update app will be considered "ready", unless it was delayed by explicit API call. It can be delayed
     * each update up to infinity, but eventually, when not, it would send "ready" event and switch into "ready" state.
     */
    virtual void delayAppReady(const char* requesterName) = 0;

    /**
     * Information about kit build. Baked at build time.
     */
    virtual const BuildInfo& getBuildInfo() = 0;

    /**
     * Information about running application.
     */
    virtual const AppInfo& getAppInfo() = 0;

    /**
     * Restart the application.
     *
     * Quit current process and start a new one. A new process is started with the same command line arguments as the
     * current one. More arguments can be appended to the command line or completely replaced.
     *
     * To quit a regular postQuit() is used, unless uncancellable is set to true. In that case the
     * postUncancellableQuit() is used.
     *
     * @param args Command line arguments for a new process.
     * @param argCount Number of command line arguments in the array.
     * @param argsPolicy Defines if @p args replaces or appends arguments for this process.
     * @param uncancellable If true postUncancellableQuit() is used to quit the current process instead of postQuit().
     */
    virtual void restart(const char* const* args = nullptr,
                         size_t argCount = 0,
                         RestartArgsPolicy argsPolicy = RestartArgsPolicy::eAppend,
                         bool uncancellable = false) = 0;
};


/**
 * Scripting events.
 */
// TODO(anov): switch back to using string hash when fix for IEventStream.push binding is merged in from Carbonite.
const carb::events::EventType kScriptingEventCommand = 0; // CARB_EVENTS_TYPE_FROM_STR("SCRIPT_COMMAND");
const carb::events::EventType kScriptingEventStdOut = 1; // CARB_EVENTS_TYPE_FROM_STR("SCRIPT_STDOUT");
const carb::events::EventType kScriptingEventStdErr = 2; // CARB_EVENTS_TYPE_FROM_STR("SCRIPT_STDERR");

/**
 * Scripting Engine part of application.
 */
class IAppScripting
{
public:
    /**
     * Run script from a string.
     *
     * @param str Content of the script to execute.
     * @param commandName Command name will be included in kScriptingEventCommand event.
     * @param executeAsFile Write string to a temp file and execute from it. For intance, that allows inspect std module
     * to get sources.
     *
     * Returns true if execution was successful.
     */
    virtual bool executeString(const char* str, const char* commandName = nullptr, bool executeAsFile = false) = 0;

    /**
     * Run script from file.
     * Returns true if execution was successful.
     */
    virtual bool executeFile(const char* path, const char* const* args, size_t argCount) = 0;

    virtual bool addSearchScriptFolder(const char* path) = 0;

    virtual bool removeSearchScriptFolder(const char* path) = 0;

    /**
     * Scripting event stream. Execution of scripts produces stdout, stderr and command itself output which are pushed
     * into that stream immediately.
     * events: kScriptingEventCommand, kScriptingEventStdOut, kScriptingEventStdErr
     * payload:
     *      "text" (string) - Message output.
     */
    virtual carb::events::IEventStream* getEventStream() = 0;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                Inline Functions                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline int IApp::run(const AppDesc& desc)
{
    this->startup(desc);
    while (this->isRunning())
    {
        this->update();
    }
    return this->shutdown();
}


} // namespace kit
} // namespace omni
