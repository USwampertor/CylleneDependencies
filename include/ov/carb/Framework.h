// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Core header for registering and acquiring interfaces.
#pragma once

#include "Defines.h"
#include "Memory.h"
#include "Types.h"

#include <cstddef>
#include <cstdint>

// free() can be #define'd which can interfere below, so handle that here
#ifdef free
#    define CARB_FREE_UNDEFINED
#    pragma push_macro("free")
#    undef free
#endif

namespace carb
{

//! Defines the current major version of the Carbonite framework.
//!
//! Incrementing this variable causes great chaos as it represents a breaking change to users.  Increment only with
//! great thought.
#define CARBONITE_MAJOR 0

//! Defines the current minor version of the Carbonite framework.
//!
//! This value is increment when non-breaking changes are made to the framework.
#define CARBONITE_MINOR 6

//! Defines the current version of the Carbonite framework.
constexpr struct Version kFrameworkVersion = { CARBONITE_MAJOR, CARBONITE_MINOR };

//! Four character code used to identify a @ref PluginRegistrationDesc object that is likely to
//! have further data provided in it.
constexpr FourCC kCarb_FourCC = CARB_MAKE_FOURCC('C', 'A', 'R', 'B');

//! Describes the different functions a plugin can define for use by carb::Framework.
//!
//! Populate this struct and register a plugin with carb::Framework::registerPlugin() for static plugins.
//!
//! Dynamic plugins are registered via @ref CARB_PLUGIN_IMPL.
struct PluginRegistrationDesc
{
    //! This or @ref onPluginRegisterExFn required. Preferred over @ref onPluginRegisterExFn.
    OnPluginRegisterFn onPluginRegisterFn;
    OnPluginStartupFn onPluginStartupFn; //!< Can be `nullptr`.
    OnPluginShutdownFn onPluginShutdownFn; //!< Can be `nullptr`.
    GetPluginDepsFn getPluginDepsFn; //!< Can be `nullptr`.
    OnReloadDependencyFn onReloadDependencyFn; //!< Can be `nullptr`.
    OnPluginPreStartupFn onPluginPreStartupFn; //!< Can be `nullptr`.
    OnPluginPostShutdownFn onPluginPostShutdownFn; //!< Can be `nullptr`.
    OnPluginRegisterExFn onPluginRegisterExFn; //!< Can be `nullptr`.
    OnPluginStartupExFn onPluginStartupExFn = nullptr; //!< Can be `nullptr`. Preferred over @ref onPluginStartupFn.
    OnPluginRegisterEx2Fn onPluginRegisterEx2Fn = nullptr; //!< Can be `nullptr`. Preferred over onPluginRegisterFn and
                                                           //!< onPluginRegisterExFn.

    //! These members exists as a version of PluginRegistrationDesc without changing the framework version to simplify
    //! adoption. Static plugins that use Framework::registerPlugin() but were compiled with an earlier version of this
    //! struct that did not have these members will not produce the required bit pattern,
    //! thereby instructing the Framework that the subsequent members are not valid and cannot be read.
    FourCC const checkValue{ kCarb_FourCC };

    //! The size of this object in bytes.  This is only valid if the @ref checkValue member is set
    //! to @ref kCarb_FourCC.  If it is not, this member and other following members will not be
    //! accessed in order to avoid undefined behavior.
    size_t const sizeofThis{ sizeof(PluginRegistrationDesc) };

    OnPluginQuickShutdownFn onPluginQuickShutdownFn = nullptr; //!< Can be `nullptr`. Function that will be called for
                                                               //!< the plugin if
                                                               //!< \ref carb::quickReleaseFrameworkAndTerminate() is
                                                               //!< invoked.

    //! Specifies the framework version required by this plugin.
    Version frameworkVersion{ kFrameworkVersion };
};

//! Describes parameters for finding plugins on disk. Multiple search paths, matching wildcards, and exclusion wildcards
//! can be specified.  Used primarily by @ref Framework::loadPlugins.
//!
//! Call @ref PluginLoadingDesc::getDefault() to instantiate this object, as it will correctly set defaults.
struct PluginLoadingDesc
{
    //! List of folders in which to search for plugins.
    //!
    //! This may contain relative or absolute paths.  All relative paths will be resolved relative to @ref
    //! carb::filesystem::IFileSystem::getAppDirectoryPath(), not the current working directory.  Absolute paths in the
    //! list will be searched directly. If search paths configuration is invalid (e.g. search paths count is zero), the
    //! fallback values are taken from the default plugin desc.
    //!
    //! Defaults to the directory containing the process's executable.
    const char* const* searchPaths;
    size_t searchPathCount; //!< Number of entries in @ref searchPaths.  Defaults to 1.
    bool searchRecursive; //!< Is search recursive in search folders.  Default to `false`.

    //! List of Filename wildcards to select loaded files. `*` and `?` can be used, e.g. "carb.*.pl?gin"
    //!
    //! Defaults to "*.plugin".  This can lead to unnecessary plugins being loaded.
    const char* const* loadedFileWildcards;
    size_t loadedFileWildcardCount; //!< Number of entries in @ref loadedFileWildcards. Defaults to 1.

    //! List of filename wildcards to mark loaded files as reloadable. Framework will treat them specially to allow
    //! overwriting source plugins and will monitor them for changes.
    //!
    //! Defaults to `nullptr`.
    const char* const* reloadableFileWildcards;
    size_t reloadableFileWildcardCount; //!< Number of entries in @ref reloadableFileWildcards. Defaults to 0.

    //! If `true`, load and store the plugins interface information, then immediately unload the plugin until needed.
    //! When one of plugin's interfaces is acquired, the library will be loaded again.
    //!
    //! Defaults to `false`.
    bool unloadPlugins;

    //! List of filename wildcards to select excluded files. `*` and `?` can be used.
    //!
    //! Defaults to `nullptr`.
    const char* const* excludedFileWildcards;
    size_t excludedFileWildcardCount; //!< Number of entries in @ref excludedFileWildcards. Defaults to 0.

    //! Returns a PluginLoadDesc with sensible defaults.
    static PluginLoadingDesc getDefault()
    {
        static constexpr const char* defaultSearchPath = "";
        static constexpr const char* defaultLoadedFileWildcard = "*.plugin";
        return { &defaultSearchPath, 1, false, &defaultLoadedFileWildcard, 1, nullptr, 0, false, nullptr, 0 };
    }
};

//! Flags for use with \ref carb::AcquireInterfaceOptions
enum AcquireInterfaceFlags : uint64_t
{
    //! Default search type, a plugin name may be specified in `typeParam`.
    eAIFDefaultType = 0,

    //! Acquire interface from interface specified in `typeParam`.
    eAIFFromInterfaceType,

    //! Acquire interface from library specified in `typeParam`.
    eAIFFromLibraryType,

    //! New types can be added here

    //! Count of types.
    eAIFNumTypes,

    //! A mask that contains all of the above types.
    fAIFTypeMask = 0xf,

    //! The interface acquire is optional and may fail without error logging.
    fAIFOptional = (1 << 4),

    //! The interface acquire will only succeed if the plugin is already initialized.
    fAIFNoInitialize = (1 << 5),
};
static_assert(eAIFNumTypes <= fAIFTypeMask, "Too many types for mask");

//! A structure used with \ref Framework::internalAcquireInterface(). Typically callers should use one of the adapter
//! functions such as \ref Framework::tryAcquireInterface() and not use this directly.
struct AcquireInterfaceOptions
{
    //! Size of this structure for versioning.
    size_t sizeofThis;

    //! The client requesting this interface
    const char* clientName;

    //! The interface requested
    InterfaceDesc desc;

    //! Type and flags. One Type must be specified as well as any flags.
    AcquireInterfaceFlags flags;

    //! Context interpreted based on the type specified in the `flags` member.
    const void* typeParam;
};
static_assert(std::is_standard_layout<AcquireInterfaceOptions>::value, "Must be standard layout");

//! Result of loading a plugin.  Used by @ref carb::Framework::loadPlugin.  Non-negative values indicated success.
enum class LoadPluginResult : int32_t
{
    //! Plugin was attempted to be loaded from a temporary path in use by the framework.
    eForbiddenPath = -3,

    //! Invalid argument passed to @ref Framework::loadPlugin.
    eInvalidArg = -2,

    //! An unspecified error occurred.  The plugin was not loaded.
    eFailed = -1,

    //! The plugin was successfully loaded.
    eSucceeded = 0,

    //! The plugin was loaded as an ONI plugin.
    eSucceededAsOmniverseNativeInterface = 1,

    //! The plugin is already loaded.
    eAlreadyLoaded = 2,
};

//! Release Hook function
//!
//! Called when the @ref carb::Framework (or an interface) is being released, before the actual release is done. Add a
//! release hook with @ref carb::Framework::addReleaseHook(). Registered release hooks can be removed with @ref
//! carb::Framework::removeReleaseHook().
//!
//! @param iface The interface that is being released. If the framework is being released, this is `nullptr`.
//!
//! @param userData The data passed to @ref carb::Framework::addReleaseHook().
using ReleaseHookFn = void (*)(void* iface, void* userData);

//! Load Hook function
//!
//! Called when a plugin is loaded for the first time and the requested interface becomes available. The interface must
//! be acquired with \ref Framework::tryAcquireInterface() or \ref Framework::acquireInterface() etc.
//!
//! The thread that first acquires the interface will call all load hooks for that interface before the interface value
//! is returned from the Framework. All other threads that acquire that interface will wait until load hooks have been
//! called. Calling load hooks is done without an internal Framework mutex locked, so other threads are able to acquire
//! other interfaces while load hooks are executing.
//!
//! It is safe for a `LoadHookFn` to call \ref Framework::removeLoadHook() for the handle that caused it to be called,
//! or any other handle.
//!
//! @see Framework::addLoadHook() Framework::removeLoadHook()
//! @param plugin The \ref PluginDesc for the plugin that has now loaded.
//! @param userData The `void*` that was passed to \ref Framework::addLoadHook().
using LoadHookFn = void (*)(const PluginDesc& plugin, void* userData);

//! Acquire the Carbonite framework for an application.
//!
//! Do not call this method directly.  Rather, call a helper function such as @ref OMNI_CORE_INIT, @ref
//! carb::acquireFrameworkAndRegisterBuiltins or @ref carb::acquireFrameworkForBindings.  Of the methods above, @ref
//! OMNI_CORE_INIT is preferred for most applications.
//!
//! The Carbonite framework is a singleton object, it will be created on the first acquire call. Subsequent calls to
//! acquire return the same instance.
//!
//! This function is expected to be used by applications, which links with the framework.
//!
//! Plugins should not use this function.  Rather, plugins should use @ref carb::getFramework().
//!
//! @thread_safety This function may be called from multiple threads simultaneously.
//!
//! @param appName The application name requesting the framework. Must not be `nullptr`.
//!
//! @param frameworkVersion specifies the minimum framework version expected by the application.  `nullptr` is return if
//! the minimum version cannot be met.
//!
//! @return The Carbonite framework. Can be `nullptr`.
//!
//! @see @ref carb::releaseFramework().
CARB_DYNAMICLINK carb::Framework* acquireFramework(const char* appName, Version frameworkVersion = kFrameworkVersion);

//! Returns `true` if the Carbonite framework has been created and is still alive.  Creation happens at the first @ref
//! carb::acquireFramework() call and ends at any @ref carb::releaseFramework() call.
CARB_DYNAMICLINK bool isFrameworkValid();

//! Retrieves the Carbonite SDK version string,
//!
//! @returns A string describing the current Carbonite SDK version.  This will be the same value
//!          as the @ref CARB_SDK_VERSION value that was set when the SDK was built.
//!
//! @note This version is intended for use in host apps that link directly to the `carb` library.
//!       Libraries that don't link directly to it such as plugins will not be able to call
//!       into this without first dynamically importing it.  Plugins should instead call this
//!       through `carb::getFramework()->getSdkVersion()`.
CARB_DYNAMICLINK const char* carbGetSdkVersion();

//! Tests whether the Carbonite SDK headers match the version of used to build the framework.
//!
//! @param[in] version  The version string to compare to the version stored in the Carbonite
//!                     framework library.  This is expected to be the value of the
//!                     @ref CARB_SDK_VERSION symbol found in `carb/SdkVersion.h`.
//! @returns `true` if the version of the headers matches the version of the framework library
//!          that is currently loaded.  Returns `false` if the version string in the headers
//!          does not match the version of the framework library.  If the library does not
//!          match the headers, it is not necessarily a fatal problem.  It does however
//!          indicate that issues may occur and that there may have been a building or
//!          packaging problem for the host app.
#define CARB_IS_SAME_SDK_VERSION(version) (strcmp(version, carbGetSdkVersion()) == 0)

//! Releases the Carbonite framework immediately.
//!
//! In some cases more, than one client can acquire the framework (e.g. scripting bindings), but only one of the clients
//! should be responsible for releasing it.
//!
//! @thread_safety May be called from any thread.
CARB_DYNAMICLINK void releaseFramework();

//! Releases the Carbonite framework immediately and exits the process, without running C/C++ atexit() registered
//! functions or static destructors.
//!
//! @note This function does not return.
//!
//! @warning This function must not be called from within a DLL, shared object, or plugin.
//!
//! This function performs the following sequence:
//! 1. Calls any exported \ref carbOnPluginQuickShutdown on all loaded plugins, if the framework is acquired. No plugins
//!    are unloaded, unregistered, nor have their interfaces destroyed.
//! 2. Calls any registered Framework release hooks (see \ref carb::Framework::addReleaseHook) in reverse order of
//!    registration, if the framework is acquired.
//! 3. Flushes stdout/stderr.
//! 4. Calls `TerminateProcess()` on Windows or `_exit()` on Linux and MacOS.
//!
//! @thread_safety May be called from any thread.
//! @param exitCode The exit code that the process will exit with.
CARB_DYNAMICLINK void quickReleaseFrameworkAndTerminate [[noreturn]] (int exitCode);

//! Defines the framework for creating Carbonite applications and plugins.
//!
//! See \carb_framework_overview for high-level documentation on core concepts, using @ref Framework, and creating
//! plugins.
//!
//! Plugins are shared libraries with a .plugin.dll/.so suffix. The plugins are named with the .plugin suffix to support
//! plugin discovery and support cohabitation with other supporting .dll/.so libraries in the same folder. It is a
//! recommended naming pattern, but not mandatory.
//!
//! Plugin library file format:
//!
//! - Windows: <plugin-name>.plugin.dll
//! - Linux:   lib<plugin-name>.plugin.so
//!
//! A plugin implements one or many interfaces and has a name which uniquely identifies it to the framework. The
//! plugin's name usually matches the filename, but it is not mandatory, the actual plugin name is provided by the
//! plugin via @ref carb::OnPluginRegisterFn.
//!
//! "Static" plugin can also be registered with @ref Framework::registerPlugin() function, thus no shared library will
//! be involved.
//!
//! @ref Framework comes with 3 static plugins:
//!
//! - @ref carb::logging::ILogging
//! - @ref carb::filesystem::IFileSystem
//! - @ref carb::assert::IAssert
//!
//! These plugins are used by @ref Framework itself. Without @ref carb::logging::ILogging, @ref Framework won't be able
//! to log messages. Without @ref carb::filesystem::IFileSystem, @ref Framework won't be able to load any "dynamic"
//! plugins. Without @ref carb::assert::IAssert, assertion failures will simply write a message to stderr and abort.
//!
//! It's up to the application to register these needed plugins.  @ref OMNI_CORE_INIT() performs this registration on
//! the user's behalf.
//!
//! The term "client" is often used across the @ref Framework API. Client is either:
//!
//! - A plugin. Here the client name is the same as the plugin name.
//!
//! - An application. The module which dynamically links with the Framework and uses @ref carb::acquireFramework().
//!
//! - Scripting bindings. This is technically similar to an application, in that it dynamically links with the @ref
//!   Framework and uses @ref carb::acquireFramework().
//!
//! Clients are uniquely identified by their name. Many functions accept client name as an argument. This allows @ref
//! Framework to create a dependency tree of clients.  This dependency tree allows the safe unloading of plugins.
//!
//! @thread_safety Unless otherwise noted, @ref Framework functions are thread-safe and may be called from multiple
//! threads simultaneously.
struct Framework
{
    /**
     * Load and register plugins from shared libraries.
     */
    void loadPlugins(const PluginLoadingDesc& desc = PluginLoadingDesc::getDefault());

    /**
     * Load and register plugins from shared libraries.  Prefer using @ref loadPlugins.
     */
    void(CARB_ABI* loadPluginsEx)(const PluginLoadingDesc& desc);

    /**
     * Unloads all plugins, including registered "static" plugins (see @ref Framework::registerPlugin).
     */
    void(CARB_ABI* unloadAllPlugins)();

    /**
     * Acquires the typed plugin interface, optionally from a specified plugin.
     *
     * If `nullptr` is passed as @p pluginName this method selects the default plugin for the given interface type.
     * Default plugin selection happens on the first such acquire call for a particular interface name and locked until
     * after this interface is released. By default the interface with highest version is selected.
     *
     * If the plugin has not yet been started, it will be loaded and started (\ref carbOnPluginStartup called) by this
     * call.
     *
     * @ref Framework::setDefaultPlugin can be used to explicitly set which plugin to set as default, but it should be
     * called before the first acquire call.
     *
     * If acquire fails, `nullptr` is returned and an error is logged.
     *
     * @param pluginName The option to specify a plugin (implementation) that you specifically want. Pass `nullptr` to
     * search for all plugins.
     *
     * @return The requested plugin interface or `nullptr` if an error occurs (an error message is logged).
     *
     * @see See @ref tryAcquireInterface(const char*) for a version of this method that does not log errors.
     */
    template <typename T>
    T* acquireInterface(const char* pluginName = nullptr);

    /**
     * Tries to acquire the typed plugin interface, optionally from a specified plugin.
     *
     * If `nullptr` is passed as @p pluginName this method selects the default plugin for the given interface type.
     * Default plugin selection happens on the first such acquire call for a particular interface name and locked until
     * after this interface is released. By default the interface with highest version is selected.
     *
     * If the plugin has not yet been started, it will be loaded and started (\ref carbOnPluginStartup called) by this
     * call.
     *
     * @ref Framework::setDefaultPlugin can be used to explicitly set which plugin to set as default, but it should be
     * called before the first acquire call.
     *
     * @param pluginName The option to specify a plugin (implementation) that you specifically want. Pass `nullptr` to
     * search for all plugins.
     *
     * @return The requested plugin interface or `nullptr` if an error occurs.
     */
    template <typename T>
    T* tryAcquireInterface(const char* pluginName = nullptr);

    /**
     * Acquires the typed plugin interface from the same plugin as the provided interface.
     *
     * Example:
     *
     * @code{.cpp}
     *  Foo* foo = framework->acquireInterface<Foo>();
     *
     *  // the returned 'bar' interface is from the same plugin as 'foo'.
     *  Bar* bar = framework->acquireInterface<Bar>(foo);
     * @endcode
     *
     * If foo and bar are not nullptr, they are guaranteed to be on the same plugin.
     *
     * @param pluginInterface The interface that was returned from acquireInterface. It will be used to select a
     * plugin with requested interface.
     *
     * @return The typed plugin interface that is returned and will be started, or `nullptr` if the interface cannot be
     * acquired (an error is logged).
     *
     * @see See @ref tryAcquireInterface(const void*) for a version of this method that does not log errors.
     */
    template <typename T>
    T* acquireInterface(const void* pluginInterface);

    /**
     * Tries to acquire the typed plugin interface from the same plugin as the provided interface.
     *
     * Example:
     *
     * @code{.cpp}
     *  Foo* foo = framework->acquireInterface<Foo>();
     *
     *  // the returned 'bar' interface is from the same plugin as 'foo'.
     *  Bar* bar = framework->tryAcquireInterface<Bar>(foo);
     * @endcode
     *
     * If foo and bar are not nullptr, they are guaranteed to be on the same plugin.
     *
     * @param pluginInterface The interface that was returned from acquireInterface. It will be used to select a
     * plugin with requested interface.
     *
     * @return The typed plugin interface that is returned and will be started, or `nullptr` if the interface cannot be
     * acquired.
     */
    template <typename T>
    T* tryAcquireInterface(const void* pluginInterface);

    /**
     * Acquires to the typed plugin interface from the given dynamic library file.
     *
     * @note If the given library was not a registered plugin, the Framework will attempt to register the library as a
     * new plugin.
     *
     * If the plugin has not yet been started, it will be loaded and started (\ref carbOnPluginStartup called) by this
     * call.
     *
     * @param libraryPath The library path to acquire the interface from. Can be absolute or relative (to the current
     * working directory) path to a dynamic (.dll/.so/.dylib) library Carbonite plugin.
     *
     * @return The typed plugin interface (guaranteed to be from the given library) or `nullptr`.  If `nullptr` is
     * returned, an error is logged.
     *
     * @see See @ref tryAcquireInterfaceFromLibrary(const char*) for a version of this method that does not log errors.
     */
    template <typename T>
    T* acquireInterfaceFromLibrary(const char* libraryPath);

    /**
     * Tries to acquire the typed plugin interface from the given dynamic library file.
     *
     * @note If the given library was not a registered plugin, the Framework will attempt to register the library as a
     * new plugin.
     *
     * If the plugin has not yet been started, it will be loaded and started (\ref carbOnPluginStartup called) by this
     * call.
     *
     * This function works exactly as @ref Framework::acquireInterfaceFromLibrary(const char*), except if acquire fails
     * it returns `nullptr` and doesn't log an error.
     *
     * @param libraryPath The library path to acquire the interface from. Can be absolute or relative (to the current
     * working directory) path to a dynamic (.dll/.so/.dylib) library Carbonite plugin.
     *
     * @return The typed plugin interface or `nullptr` if the library file was not found or an error occurred.
     */
    template <typename T>
    T* tryAcquireInterfaceFromLibrary(const char* libraryPath);

    /**
     * Tries to acquire the typed plugin interface if and only if it has been previously acquired, optionally from a
     * specified plugin.
     *
     * If `nullptr` is passed as @p pluginName this method selects the default plugin for the given interface type.
     * Default plugin selection happens on the first such acquire call for a particular interface name and locked until
     * after this interface is released. By default the interface with highest version is selected.
     *
     * Unlike \ref tryAcquireInterface, this function will only acquire an interface if the plugin providing it is
     * already started (it won't attempt to start the plugin). This is useful during \ref carbOnPluginShutdown when a
     * circularly-dependent interface may have already been released by the Framework and attempting to reload it would
     * result in an error.
     *
     * @ref Framework::setDefaultPlugin can be used to explicitly set which plugin to set as default, but it should be
     * called before the first acquire call.
     *
     * @param pluginName The option to specify a plugin (implementation) that you specifically want. Pass `nullptr` to
     * search for all plugins.
     *
     * @return The requested plugin interface or `nullptr` if an error occurs or the plugin is not started.
     */
    template <typename T>
    T* tryAcquireExistingInterface(const char* pluginName = nullptr);

    /**
     * Gets the number of plugins with the specified interface.
     *
     * @return The number of plugins with the specified interface.
     */
    template <typename T>
    uint32_t getInterfacesCount();

    //! Acquires all interfaces of the given type.
    //!
    //! The given output array must be preallocated. @p interfacesSize tells this method the size of the array.
    //!
    //! If @p interfaces is to small, the array is filled as much as possible and an error is logged.
    //!
    //! If @p interfaces is to big, entries past the required size will not be written.
    //!
    //! Upon output, `nullptr` may randomly appear in `interfaces`.  This represents failed internal calls to @ref
    //! tryAcquireInterface.  No error is logged in this case.
    //!
    //! @param interfaces Preallocated array that will hold the acquired interfaces.  Values in this array must be
    //! preset to `nullptr` in order to determine which entries in the array are valid upon output.
    //!
    //! @param interfacesSize Number of preallocated array elements.  See @ref Framework::getInterfacesCount().
    //!
    //! @rst
    //! .. warning::
    //!     Carefully read this method's documentation, as it has a slew of design issues.  It's use is not
    //!     recommended.
    //! @endrst
    template <typename T>
    void acquireInterfaces(T** interfaces, uint32_t interfacesSize);

    //! Acquires the plugin interface pointer from an interface description.
    //!
    //! This is an internal function. Use @ref Framework::acquireInterface(const char*) instead.
    //!
    //! @rst
    //! .. deprecated:: 135.0
    //!     If explicit client functionality is needed, please use ``internalAcquireInterface`` instead.
    //!     However, note that this function will only be available beginning with Carbonite 135.0.
    //! @endrst
    //!
    //! @param clientName The client requesting the plugin.  This is used to form a dependency graph between clients.
    //! Must not be `nullptr`.
    //!
    //! @param desc The plugin interface description
    //!
    //! @param pluginName The plugin that you specifically want.  If `nullptr`, the interface's "default" plugin is
    //! used.
    //!
    //! @return The returned function pointer for the interface being queried and started.  If `nullptr` is returned, an
    //! error is logged.
    //!
    //! @see See @ref tryAcquireInterfaceWithClient for a version of this method that does not log errors.
    void*(CARB_ABI* acquireInterfaceWithClient)(const char* clientName, InterfaceDesc desc, const char* pluginName);
    static_assert(kFrameworkVersion.major == 0, "Remove above function in next Framework version");

    //! Tries to acquires the plugin interface pointer from an interface description.
    //!
    //! This method has the same contract as @ref Framework::acquireInterfaceWithClient except an error is not logged if
    //! the interface could not be acquired.
    //!
    //! This is an internal function. Use @ref Framework::tryAcquireInterface(const char*) instead.
    //!
    //! @rst
    //! .. deprecated:: 135.0
    //!     If explicit client functionality is needed, please use ``internalAcquireInterface`` instead.
    //!     However, note that this function will only be available beginning with Carbonite 135.0.
    //! @endrst
    //!
    //! @param clientName The client requesting the plugin.  This is used to form a dependency graph between clients.
    //! Must not be `nullptr`.
    //!
    //! @param desc The plugin interface description
    //!
    //! @param pluginName The plugin that you specifically want.  If `nullptr`, the interface's "default" plugin is
    //! used.
    //!
    //! @return The returned function pointer for the interface being queried and started, or `nullptr` if an error
    //! occurs.
    void*(CARB_ABI* tryAcquireInterfaceWithClient)(const char* clientName, InterfaceDesc desc, const char* pluginName);
    static_assert(kFrameworkVersion.major == 0, "Remove above function in next Framework version");

    //! Acquires the typed plugin interface from the same plugin as the provided interface.
    //!
    //! This is an internal function. Use @ref Framework::acquireInterface(const char*) instead.
    //!
    //! @rst
    //! .. deprecated:: 135.0
    //!     If explicit client functionality is needed, please use ``internalAcquireInterface`` instead.
    //!     However, note that this function will only be available beginning with Carbonite 135.0.
    //! @endrst
    //!
    //! @param clientName The client requesting the plugin.  This is used to form a dependency graph between clients.
    //! Must not be `nullptr`.
    //!
    //! @param desc The plugin interface description.
    //!
    //! @param pluginInterface The interface that was returned from acquireInterface. It will be used to select a plugin
    //! with requested interface.
    //!
    //! @return The returned function pointer for the interface being queried and started.  If `nullptr` is returned, an
    //! error is logged.
    //!
    //! @see See @ref tryAcquireInterfaceFromInterfaceWithClient for a version of this method that does not log errors.
    void*(CARB_ABI* acquireInterfaceFromInterfaceWithClient)(const char* clientName,
                                                             InterfaceDesc desc,
                                                             const void* pluginInterface);
    static_assert(kFrameworkVersion.major == 0, "Remove above function in next Framework version");

    //! Tries to acquires the typed plugin interface from the same plugin as the provided interface.
    //!
    //! This method has the same contract as @ref Framework::acquireInterfaceFromInterfaceWithClient except an error is
    //! not logged if the interface could not be acquired.
    //!
    //! This is an internal function. Use @ref Framework::tryAcquireInterface(const char*) instead.
    //!
    //! @rst
    //! .. deprecated:: 135.0
    //!     If explicit client functionality is needed, please use ``internalAcquireInterface`` instead.
    //!     However, note that this function will only be available beginning with Carbonite 135.0.
    //! @endrst
    //!
    //! @param clientName The client requesting the plugin.  This is used to form a dependency graph between clients.
    //! Must not be `nullptr`.
    //!
    //! @param desc The plugin interface description.
    //!
    //! @param pluginInterface The interface that was returned from acquireInterface. It will be used to select a plugin
    //! with requested interface.
    //!
    //! @return The returned function pointer for the interface being queried and started, or `nullptr` if an error
    //! occurs.
    void*(CARB_ABI* tryAcquireInterfaceFromInterfaceWithClient)(const char* clientName,
                                                                InterfaceDesc desc,
                                                                const void* pluginInterface);
    static_assert(kFrameworkVersion.major == 0, "Remove above function in next Framework version");

    //! Acquires the plugin interface pointer from an interface description and a filename.
    //!
    //! @note If the given library was not a registered plugin, the Framework will attempt to register the library as a
    //! new plugin.
    //!
    //! This is an internal function. Use @ref Framework::acquireInterfaceFromLibrary(const char*) instead.
    //!
    //! @rst
    //! .. deprecated:: 135.0
    //!     If explicit client functionality is needed, please use ``internalAcquireInterface`` instead.
    //!     However, note that this function will only be available beginning with Carbonite 135.0.
    //! @endrst
    //!
    //! @param clientName The client requesting the plugin.  This is used to form a dependency graph between clients.
    //! Must not be `nullptr`.
    //!
    //! @param desc The plugin interface description
    //!
    //! @param libraryPath The filename to acquire the interface from. Can be absolute or relative path to actual
    //! .dll/.so Carbonite plugin. Path is relative to the current working directory.  Must not be `nullptr`.
    //!
    //! @return The returned function pointer for the interface being queried and started.  If `nullptr` is returned, an
    //! error is logged.
    //!
    //! @see See @ref tryAcquireInterfaceFromLibraryWithClient for a version of this method that does not log errors.
    void*(CARB_ABI* acquireInterfaceFromLibraryWithClient)(const char* clientName,
                                                           InterfaceDesc desc,
                                                           const char* libraryPath);
    static_assert(kFrameworkVersion.major == 0, "Remove above function in next Framework version");

    //! Tries to acquire the plugin interface pointer from an interface description and a filename.
    //!
    //! This method has the same contract as @ref Framework::acquireInterfaceFromLibraryWithClient except an error is
    //! not logged if the interface could not be acquired.
    //!
    //! @note If the given library was not a registered plugin, the Framework will attempt to register the library as a
    //! new plugin.
    //!
    //! This is an internal function. Use @ref Framework::tryAcquireInterfaceFromLibrary(const char*) instead.
    //!
    //! @rst
    //! .. deprecated:: 135.0
    //!     If explicit client functionality is needed, please use ``internalAcquireInterface`` instead.
    //!     However, note that this function will only be available beginning with Carbonite 135.0.
    //! @endrst
    //!
    //! @param clientName The client requesting the plugin.  This is used to form a dependency graph between clients.
    //! Must not be `nullptr`.
    //!
    //! @param desc The plugin interface description
    //!
    //! @param libraryPath The filename to acquire the interface from. Can be absolute or relative path to actual
    //! .dll/.so Carbonite plugin. Path is relative to the current working directory.  Must not be `nullptr`.
    //!
    //! @return The returned function pointer for the interface being queried and started, or `nullptr` on error.
    void*(CARB_ABI* tryAcquireInterfaceFromLibraryWithClient)(const char* clientName,
                                                              InterfaceDesc desc,
                                                              const char* libraryPath);
    static_assert(kFrameworkVersion.major == 0, "Remove above function in next Framework version");

    //! Gets the number of plugins with the specified interface descriptor.
    //!
    //! @param interfaceDesc The interface descriptor to get the plugin count.
    //!
    //! @return The number of plugins with the specified interface descriptor.
    uint32_t(CARB_ABI* getInterfacesCountEx)(InterfaceDesc interfaceDesc);

    //! Acquires all interfaces of the given type.
    //!
    //! The given output array must be preallocated. @p interfacesSize tells this method the size of the array.
    //!
    //! If @p interfaces is to small, the array is filled as much as possible and an error is logged.
    //!
    //! If @p interfaces is to big, entries past the required size will not be written.
    //!
    //! Upon output, `nullptr` may randomly appear in `interfaces`.  This represents failed internal calls to @ref
    //! tryAcquireInterface.  No error is logged in this case.
    //!
    //! This is an internal function. Use @ref Framework::acquireInterfaces() instead.
    //!
    //! @param clientName The client requesting the plugin.  This is used to form a dependency graph between clients.
    //! Must not be `nullptr`.
    //!
    //! @param desc The plugin interface description
    //!
    //! @param interfaces Preallocated array that will hold the acquired interfaces.  Values in this array must be
    //! preset to `nullptr` in order to determine which entries in the array are valid upon output.
    //!
    //! @param interfacesSize Number of preallocated array elements.  See @ref Framework::getInterfacesCount().
    //!
    //! @rst
    //! .. warning::
    //!     Carefully read this method's documentation, as it has a slew of design issues.  It's use is not
    //!     recommended.
    //! @endrst
    void(CARB_ABI* acquireInterfacesWithClient)(const char* clientName,
                                                InterfaceDesc interfaceDesc,
                                                void** interfaces,
                                                uint32_t interfacesSize);

    //! Releases the use of an interface that is no longer needed.
    //!
    //! Correct plugin interface type is expected, compile-time check is performed.
    //!
    //! @param pluginInterface The interface that was returned from acquireInterface
    template <typename T>
    void releaseInterface(T* pluginInterface);

    //! \cond DEV

    //! Releases the use of an interface that is no longer needed.
    //!
    //! This is an internal function. Use @ref Framework::releaseInterface() instead.
    //!
    //! @param clientName The client requesting the plugin.  This is used to form a dependency graph between clients.
    //! Must not be `nullptr`.
    //!
    //! @param pluginInterface The interface that was returned from @ref Framework::acquireInterface.
    void(CARB_ABI* releaseInterfaceWithClient)(const char* clientName, void* pluginInterface);

    //! \endcond

    //! Gets the plugin descriptor for a specified plugin.
    //!
    //! @param pluginName The plugin that you specifically want to get the descriptor for.  Must not be `nullptr`.
    //!
    //! @return The @ref PluginDesc, it will be filled with zeros if the plugin doesn't exist.  The returned memory will
    //! be valid as long as the plugin is loaded.
    const PluginDesc&(CARB_ABI* getPluginDesc)(const char* pluginName);

    //! Gets the plugin descriptor for an interface returned from @ref Framework::acquireInterface.
    //!
    //! @param pluginInterface The interface that was returned from acquireInterface
    //!
    //! @return The PluginDesc, it will be filled with zeros if wrong interface pointer is provided.
    const PluginDesc&(CARB_ABI* getInterfacePluginDesc)(void* pluginInterface);

    //! Gets the plugins with the specified interface descriptor.
    //!
    //! @param interfaceDesc The interface descriptor to get the plugins for.
    //!
    //! @param outPlugins The array to be populated with the plugins of size @ref Framework::getInterfacesCount().
    //! This array must be set to all zeros before given to this function in order to be able to tell the number of
    //! entries written.
    //!
    //! @rst
    //! .. danger::
    //!
    //!    Do not use this method. The caller will be unable to correctly size ``outPlugins``.  The size of the number
    //!    of loaded plugins matching ``interfaceDesc`` may change between the call to
    //!    :cpp:func:`carb::Framework::getInterfacesCount` and this method.
    //! @endrst
    void(CARB_ABI* getCompatiblePlugins)(InterfaceDesc interfaceDesc, PluginDesc* outPlugins);

    //! Gets the number of registered plugins.
    //!
    //! @return The number of registered plugins.
    size_t(CARB_ABI* getPluginCount)();

    //! Gets all registered plugins.
    //!
    //! @param outPlugins The array to be populated with plugin descriptors of size @ref Framework::getPluginCount().
    //!
    //! @rst
    //! .. danger::
    //!
    //!    Do not use this method. The caller will be unable to correctly size ``outPlugins``.  The number of plugins
    //!    may change between the call to :cpp:member:`carb::Framework::getPluginCount` and this method.
    //! @endrst
    void(CARB_ABI* getPlugins)(PluginDesc* outPlugins);

    //! Attempts to reload all plugins that are currently loaded.
    void(CARB_ABI* tryReloadPlugins)();

    //! Register a "static" plugin.
    //!
    //! While typical plugins are "dynamic" and loaded from shared libraries (see @ref Framework::loadPlugins), a
    //! "static" plugin can be added by calling this function from an application or another plugin. The contract is
    //! exactly the same: you provide a set of functions (some of which are optional), which usually are looked for in a
    //! shared library by the framework. It can be useful in some special scenarios where you want to hijack particular
    //! interfaces or limited in your ability to produce new shared libraries.
    //!
    //! It is important that the plugin name provided by @ref PluginRegistrationDesc::onPluginRegisterFn function is
    //! unique, registration will fail otherwise.
    //!
    //! @param clientName The client registering the plugin.  This is used to form a dependency graph between clients.
    //! Must not be `nullptr`.
    //!
    //! @param desc The plugin registration description.
    //!
    //! @return If registration was successful.
    bool(CARB_ABI* registerPlugin)(const char* clientName, const PluginRegistrationDesc& desc);

    //! Try to unregister a plugin.
    //!
    //! If plugin is in use, which means one if its interfaces was acquired by someone and not yet released, the
    //! unregister will fail. Both "dynamic" (shared libraries) and "static" (see @ref Framework::registerPlugin)
    //! plugins can be unregistered.
    //!
    //! @param pluginName The plugin to be unregistered.
    //!
    //! @return If unregistration was successful.
    bool(CARB_ABI* unregisterPlugin)(const char* pluginName);

    //! The descriptor for registering builtin @ref carb::logging::ILogging interface implementation.
    const PluginRegistrationDesc&(CARB_ABI* getBuiltinLoggingDesc)();

    //! The descriptor for registering builtin @ref carb::filesystem::IFileSystem interface implementation.
    const PluginRegistrationDesc&(CARB_ABI* getBuiltinFileSystemDesc)();

    //! Sets the default plugin to be used when an interface type is acquired.
    //!
    //! The mechanism of default interfaces allows @ref Framework to guarantee that every call to
    //! `acquireInterface<Foo>()` will return the same `Foo` interface pointer for everyone. The only way to bypass it
    //! is by explicitly passing the `pluginName` of the interface you want to acquire.
    //!
    //! It is important to note that if the interface was previously already acquired, the effect of this function won't
    //! take place until it is released by all holders. So it is recommended to set defaults as early as possible.
    //!
    //! @tparam T The interface type.
    //! @param pluginName The name of the plugin (e.g. "carb.profiler-cpu.plugin") that will be set as default. Must not
    //! be `nullptr`.
    template <class T>
    void setDefaultPlugin(const char* pluginName);

    //! \cond DEV

    //! Sets the default plugin to be used when the given interface is acquired.
    //!
    //! The mechanism of default interfaces allows @ref Framework to guarantee that every call to
    //! `acquireInterface<Foo>()` will return the same `Foo` interface pointer for everyone. The only way to bypass it
    //! is by explicitly passing the `pluginName` of the interface you want to acquire.
    //!
    //! It is important to note that if the interface was previously already acquired, the effect of this function won't
    //! take place until it is released by all holders. So it is recommended to set defaults as early as possible.
    //!
    //! @param clientName The client registering the plugin.  This is used to form a dependency graph between clients.
    //! Must not be `nullptr`.
    //!
    //! @param desc The plugin interface description.
    //!
    //! @param pluginName The plugin that will be set as default. Must not be `nullptr`.
    void(CARB_ABI* setDefaultPluginEx)(const char* clientName, InterfaceDesc desc, const char* pluginName);

    //! \endcond

    //! Sets the temporary path where the framework will store data for reloadable plugins.
    //!
    //! This function must be called before loading any reloadable plugins. By default @ref Framework creates a
    //! temporary folder in the executable's folder.
    //!
    //! @param tempPath Temporary folder path.
    void(CARB_ABI* setReloadableTempPath)(const char* tempPath);

    //! Returns temporary path where the framework will store data for reloadable plugins.
    //!
    //! @return Temporary path for reloadable data.  The returned memory is valid until the @ref
    //! Framework::setReloadableTempPath is called or the @ref Framework is destroyed.
    const char*(CARB_ABI* getReloadableTempPath)();

    //! Returns Carbonite version and build information.
    //!
    //! The format is: `v{major}.{minor} [{shortgithash} {gitbranch} {isdirty}]` where:
    //!
    //! - major - `kFrameworkVersion.major`
    //! - minor - `kFrameworkVersion.minor`
    //! - shortgithash - output of `git rev-parse --short HEAD`
    //! - gitbranch - output of `git rev-parse --abbrev-ref HEAD`
    //! -  isdirty - `DIRTY` if `git status --porcelain` is not empty
    //!
    //! Examples:
    //!
    //! - `v1.0 [56ab220c master]`
    //! - `v0.2 [f2fc1ba1 dev/mfornander/harden DIRTY]`
    const char*(CARB_ABI* getBuildInfo)();

    //! Checks if the provided plugin interface matches the requirements.
    //!
    //! @param interfaceCandidate The interface that was provided by the user.
    //!
    //! @return If the interface candidate matches template interface requirements, returns @p interfaceCandidate.
    //! Otherwise, returns `nullptr`.
    template <typename T>
    T* verifyInterface(T* interfaceCandidate);

    //! Checks if provided plugin interface matches the requirements.
    //!
    //! Do not directly use this method.  Instead, use @ref Framework::verifyInterface.
    //!
    //! @param desc The interface description that sets the compatibility requirements.
    //!
    //! @param interfaceCandidate The interface that was provided by the user.
    //!
    //! @return if the interface candidate matches @p desc, returns @p interfaceCandidate. Otherwise, returns `nullptr`.
    void*(CARB_ABI* verifyInterfaceEx)(InterfaceDesc desc, void* interfaceCandidate);

    //! The descriptor for registering builtin @ref carb::assert::IAssert interface implementation.
    const PluginRegistrationDesc&(CARB_ABI* getBuiltinAssertDesc)();

    //! The descriptor for registering builtin @ref carb::thread::IThreadUtil interface implementation.
    const PluginRegistrationDesc&(CARB_ABI* getBuiltinThreadUtilDesc)();

    //! Load and register a plugin from the given filename.
    //!
    //! Call @ref unloadPlugin() to unload the plugin at @p libraryPath.
    //!
    //! @param libraryPath Name of the shared library.  Must not be `nullptr`.
    //!
    //! @param reloadable Treat the plugin as reloadable.
    //!
    //! @param unload Grab the list of interfaces from the plugin and then unload it. If the user tries to acquire one
    //! of the retrieved interfaces, the plugin will be lazily reloaded.
    //!
    //! @return Returns a non-negative value on success, negative value otherwise.
    LoadPluginResult(CARB_ABI* loadPlugin)(const char* libraryPath, bool reloadable, bool unload);

    //! Unloads the plugin at the given shared library path.
    //!
    //! @param Path to shared library.  Must not be `nullptr`.
    //!
    //! @returns Returns `true` if a plugin was loaded at the given path and successfully unloaded.  `false` otherwise.
    bool(CARB_ABI* unloadPlugin)(const char* libraryPath);

    //! Adds a release hook for either the framework or a specific interface.
    //!
    //! A release hook can be added multiple times with the same or different user data, in which case it will be called
    //! multiple times. It is up to the caller to ensure uniqueness if uniqueness is desired. To remove a release hook,
    //! call @ref carb::Framework::removeReleaseHook() with the same parameters.
    //!
    //! @param iface The interface (returned by @ref carb::Framework::acquireInterface()) to monitor for release. If
    //! `nullptr` is specified, the release hook will be called when the @ref carb::Framework itself is unloaded.
    //!
    //! @param fn The release hook callback function that will be called. Must not be `nullptr`.
    //!
    //! @param user Data to be passed to the release hook function. May be `nullptr`.
    //!
    //! @returns Returns `true` if the interface was found and the release hook was added successfully; `false`
    //! otherwise.
    //!
    //! @rst
    //!
    //! .. danger::
    //!
    //!     It is *expressly forbidden* to call back into :cpp:type:`carb::Framework` in any way during the
    //!     :cpp:type:`carb::ReleaseHookFn` callback. Doing so results in undefined behavior. The only exception to this
    //!     rule is calling `removeReleaseHook()`.
    //!
    //! @endrst
    bool(CARB_ABI* addReleaseHook)(void* iface, ReleaseHookFn fn, void* user);

    //! Removes a release hook previously registered with @ref carb::Framework::addReleaseHook().
    //!
    //! The same parameters supplied to @ref carb::Framework::addReleaseHook() must be provided in order to identify the
    //! correct release hook to remove. It is safe to call this function from within the release hook callback.
    //!
    //! @param iface The interface previously passed to @ref addReleaseHook().
    //!
    //! @param fn The function previously passed to @ref addReleaseHook().
    //!
    //! @param user The user data parameter previously passed to @ref addReleaseHook().
    //!
    //! @returns Returns `true` if the release hook was found and removed. If it was not found, `false` is returned.
    //!
    //! @rst
    //!
    //! .. danger::
    //!
    //!     It is *expressly forbidden* to call back into :cpp:type:`carb::Framework` in any way during the
    //!     :cpp:type:`carb::ReleaseHookFn` callback. Doing so results in undefined behavior. The only exception to this
    //!     rule is calling `removeReleaseHook()`.
    //!
    //! @endrst
    bool(CARB_ABI* removeReleaseHook)(void* iface, ReleaseHookFn fn, void* user);

    //! @private
    CARB_DEPRECATED("Use carbReallocate() instead")
    void*(CARB_ABI* internalRealloc)(void* prev, size_t newSize, size_t align);
    static_assert(kFrameworkVersion.major == 0, "Remove Framework::internalRealloc in next Framework version");

    //! Allocates a block of memory.
    //!
    //! @note Any plugin (or the executable) may allocate the memory and a different plugin (or the executable) may free
    //! or reallocate it.
    //!
    //! @warning It is undefined behavior to use memory allocated with this function or @ref reallocate() after the
    //! Carbonite framework has been shut down.
    //!
    //! @param size The size of the memory block requested, in bytes. Specifying '0' will return a valid pointer that
    //! can be passed to @ref free but cannot be used to store any information.
    //! @param align The minimum alignment (in bytes) of the memory block requested. Must be a power of two. Values less
    //!     than `sizeof(size_t)` are ignored. `0` indicates to use default system alignment (typically
    //!     `2 * sizeof(void*)`).
    //! @returns A non-`nullptr` memory block of @p size bytes with minimum alignment @p align. If an error occurred,
    //!     or memory could not be allocated, `nullptr` is returned. The memory is not initialized.
    CARB_DEPRECATED("Use carb::allocate() instead") void* allocate(size_t size, size_t align = 0)
    {
        return carb::allocate(size, align);
    }
    static_assert(kFrameworkVersion.major == 0, "Remove Framework::allocate in next Framework version");

    //! Frees a block of memory previously allocated with @ref allocate().
    //!
    //! @note Any plugin (or the executable) may allocate the memory and a different plugin (or the executable) may
    //! free it.
    //!
    //! @param p The block of memory previously returned from @ref allocate() or @ref reallocate(), or `nullptr`.
    CARB_DEPRECATED("Use carb::deallocate() instead") void free(void* p)
    {
        return carb::deallocate(p);
    }
    static_assert(kFrameworkVersion.major == 0,
                  "Remove Framework::free and CARB_FREE_UNDEFINED in next Framework version");

    //! Reallocates a block of memory previously allocated with @ref allocate().
    //!
    //! This function changes the size of the memory block pointed to by @p p to @p size bytes with @p align alignment.
    //! The contents are unchanged from the start of the memory block up to the minimum of the old size and @p size. If
    //! @p size is larger than the old size, the added memory is not initialized. If @p p is `nullptr`, the call is
    //! equivalent to `allocate(size, align)`; if @p size is `0` and @p p is not `nullptr`, the call is equivalent to
    //! `free(p)`. Unless @p p is `nullptr`, it must have been retrieved by an earlier call to @ref allocate() or
    //! @ref reallocate(). If the memory region was moved in order to resize it, @p p will be freed as with `free(p)`.
    //!
    //! @note Any plugin (or the executable) may allocate the memory and a different plugin (or the executable) may
    //! reallocate it.
    //!
    //! @warning It is undefined behavior to use memory allocated with this function or @ref allocate() after the
    //! Carbonite framework has been shut down.
    //!
    //! @param p The block of memory previously returned from @ref allocate() or @ref reallocate() if resizing is
    //!     resizing is desired. If `nullptr` is passed as this parameter, the call behaves as if
    //!     `allocate(size, align)` was called.
    //! @param size The size of the memory block requested, in bytes. See above for further explanation.
    //! @param align The minimum alignment (in bytes) of the memory block requested. Must be a power of two. Values less
    //!     than `sizeof(size_t)` are ignored. Changing the alignment from a previous allocation is undefined behavior.
    //!     `0` indicates to use default system alignment (typically `2 * sizeof(void*)`).
    //! @returns A pointer to a block of memory of @p size bytes with minimum alignment @p align, unless an error
    //!     occurs in which case `nullptr` is returned. If @p p is `nullptr` and @p size is `0` then `nullptr` is also
    //!     returned.
    CARB_DEPRECATED("Use carb::reallocate() instead") void* reallocate(void* p, size_t size, size_t align = 0)
    {
        return carb::reallocate(p, size, align);
    }
    static_assert(kFrameworkVersion.major == 0, "Remove Framework::reallocate in next Framework version");

    //! Retrieves the Carbonite SDK version string,
    //!
    //! @returns A string describing the current Carbonite SDK version.  This will be the same value
    //!          as the @ref CARB_SDK_VERSION value that was set when the SDK was built.
    //!
    //! @note This version is intended for use in plugins.  Since Carbonite plugins aren't directly
    //!       linked to the `carb` library, access to carbGetSdkVersion() isn't as easy as calling
    //!       a library function.  This version just provides access to the same result from a
    //!       location that is better guaranteed accessible to plugins.
    const char*(CARB_ABI* getSdkVersion)();

    //! Adds a load hook that is called when an interface becomes available.
    //!
    //! No attempt is made to load the plugin. This can be used as a notification mechanism when a plugin cannot be
    //! loaded immediately (due to circular dependencies for instance) but may be loaded later. To remove the load hook,
    //! use \ref removeLoadHook(). It is possible to register multiple load hooks with the same parameters, but this is
    //! not recommended and will cause the function to be called multiple times with the same parameters.
    //!
    //! See \ref LoadHookFn for a discussion on how and when load hooks are called.
    //!
    //! @see LoadHookFn removeLoadHook()
    //! @tparam T The interface type
    //! @param pluginName the name of the specific plugin desired that exposes \c T, or \c nullptr for any plugin.
    //! @param func the \ref LoadHookFn to call when the given interface becomes available. This function may be called
    //! multiple times if multiple plugins that expose interface \c T are loaded.
    //! @param userData application-specific data that is supplied to \p func when it is called.
    //! @returns A \ref LoadHookHandle uniquely identifying this load hook; \ref kInvalidLoadHook if an error occurs.
    //! When finished with the load hook, call \ref removeLoadHook().
    template <class T>
    LoadHookHandle addLoadHook(const char* pluginName, LoadHookFn func, void* userData);

    //! @private
    LoadHookHandle(CARB_ABI* internalAddLoadHook)(
        const InterfaceDesc& iface, const char* plugin, const char* clientName, LoadHookFn fn, void* user, bool add);

    //! Removes a previously-registered load hook.
    //!
    //! It is safe to remove the load hook from within the load hook callback.
    //!
    //! @param handle The \ref LoadHookHandle returned from \ref addLoadHook().
    //! @returns Returns \c true if the load hook was found and removed. If it was not found, \c false is returned.
    bool(CARB_ABI* removeLoadHook)(LoadHookHandle handle);

    //! Registers a client as a script binding or script language owner. Typically handled by CARB_BINDINGS().
    //!
    //! This function is used to notify the Carbonite framework of dependencies from a script language. This allows
    //! proper dependency tracking and shutdown ordering. For instance, if a python binding loads an interface from
    //! *carb.assets.plugin*, it appears to Carbonite that a non-plugin client requested the interface. However, if
    //! python was started from *carb.scripting-python.plugin*, then it becomes necessary to establish a dependency
    //! relationship between *carb.scripting-python.plugin* and any plugins loaded from python bindings. This function
    //! has two purposes in this example: the *carb.scripting-python.plugin* will register itself as
    //! \ref BindingType::Owner for @p scriptType `python`. All bindings automatically register themselves as
    //! \ref BindingType::Binding for @p scriptType `python` through `CARB_BINDINGS()`. Whenever the binding acquires an
    //! interface, all registered \ref BindingType::Owner clients gain a dependency on the acquired interface.
    //!
    //! @param type The \ref BindingType of \p clientName.
    //! @param clientName A plugin or binding's client name (`g_carbClientName` typically created by `CARB_GLOBALS()` or
    //!     `CARB_BINDINGS()`).
    //! @param scriptType A user-defined script type, such as "python" or "lua". Must match between owner and bindings.
    //!     Not case-sensitive.
    void(CARB_ABI* registerScriptBinding)(BindingType type, const char* clientName, const char* scriptType);

    //! The main framework access function for acquiring an interface.
    //!
    //! @note This function is generally not intended to be used directly; instead, consider one of the many type-safe
    //! adapter functions such as \ref tryAcquireInterface().
    //!
    //! @warning This function will be `nullptr` in Carbonite releases prior to 135.0
    //!
    //! @param options The structure containing the options for acquiring the interface.
    //! @returns The interface pointer for the interface being acquired. May be `nullptr` if the interface could not be
    //!     acquired. Verbose logging will explain the entire acquisition process. Warning and Error logs may be
    //!     produced depending on options.
    void*(CARB_ABI* internalAcquireInterface)(const AcquireInterfaceOptions& options);
};
} // namespace carb

//! The client's name.
//!
//! A "client" can be one of the following in the Carbonite framework:
//!
//! - A plugin. Here the client name is the same as the plugin name.
//!
//! - An application.
//!
//! - Scripting bindings.
//!
//! Clients are uniquely identified by their name. Many functions accept client name as an argument. This allows @ref
//! carb::Framework to create a dependency tree of clients.  This dependency tree allows the safe unloading of
//! plugins.
CARB_WEAKLINK CARB_HIDDEN const char* g_carbClientName;

//! Defines the client's global @ref carb::Framework pointer.
//!
//! Do not directly access this pointer.  Rather use helper methods like @ref carb::getFramework() and @ref
//! carb::isFrameworkValid().
CARB_WEAKLINK CARB_HIDDEN carb::Framework* g_carbFramework;

//! Global symbol to enforce the use of CARB_GLOBALS() in Carbonite modules.  Do not modify or use
//! this value.
//!
//! If there is an unresolved symbol linker error about this symbol (build time or run time), it
//! means that the CARB_GLOBALS() macro was not called at the global scope in the module.  This
//! exists to ensure that all the global symbols related to each Carbonite module have been
//! properly defined and initialized.
extern bool g_needToCall_CARB_GLOBALS_atGlobalScope;

//! Defines global variables for use by Carbonite. Call this macro from the global namespace.
//!
//! Do not call this macro directly.  Rather:
//!
//! - For applications, call @ref OMNI_APP_GLOBALS.
//!
//! - For Carbonite plugins, call @ref CARB_PLUGIN_IMPL.
//!
//! - For ONI plugins, call @ref OMNI_MODULE_GLOBALS.
#define CARB_FRAMEWORK_GLOBALS(clientName)                                                                             \
    CARB_HIDDEN bool g_needToCall_CARB_GLOBALS_atGlobalScope = carb::detail::setClientName(clientName);

namespace carb
{
namespace detail
{
//! Sets the client name for the calling module.
//!
//! @param[in] clientName   A string literal containing the name of the calling plugin or
//!                         executable.  This string must be guaranteed constant for the
//!                         lifetime of the module.
//! @returns `true`.
//!
//! @note This should not be called directly.  This is called as part of CARB_FRAMEWORK_GLOBALS().
inline bool setClientName(const char* clientName)
{
    g_carbClientName = clientName;
    return true;
}

} // namespace detail

//! Gets the Carbonite framework.
//!
//! The @ref carb::Framework can be `nullptr` for applications if it hasn't acquired it (see @ref
//! carb::acquireFramework()). It can also be `nullptr` for a plugin if the plugin is used externally and was not loaded
//! by framework itself.
//!
//! After starting up, @ref carb::getFramework() can be considered a getter for a global singleton that is the @ref
//! carb::Framework.
//!
//! @return The Carbonite framework.
inline Framework* getFramework()
{
    return g_carbFramework;
}

inline void Framework::loadPlugins(const PluginLoadingDesc& desc)
{
    return this->loadPluginsEx(desc);
}

template <typename T>
T* Framework::verifyInterface(T* interfaceCandidate)
{
    const auto desc = T::getInterfaceDesc();
    return static_cast<T*>(getFramework()->verifyInterfaceEx(desc, interfaceCandidate));
}

template <typename T>
T* Framework::acquireInterface(const char* pluginName)
{
    const char* clientName = g_needToCall_CARB_GLOBALS_atGlobalScope ? g_carbClientName : nullptr;
    if (this->internalAcquireInterface)
        return static_cast<T*>(this->internalAcquireInterface(
            { sizeof(AcquireInterfaceOptions), clientName, T::getInterfaceDesc(), eAIFDefaultType, pluginName }));
    else
        return static_cast<T*>(this->acquireInterfaceWithClient(clientName, T::getInterfaceDesc(), pluginName));
}

template <typename T>
T* Framework::tryAcquireInterface(const char* pluginName)
{
    const char* clientName = g_needToCall_CARB_GLOBALS_atGlobalScope ? g_carbClientName : nullptr;
    if (this->internalAcquireInterface)
        return static_cast<T*>(
            this->internalAcquireInterface({ sizeof(AcquireInterfaceOptions), clientName, T::getInterfaceDesc(),
                                             AcquireInterfaceFlags(eAIFDefaultType | fAIFOptional), pluginName }));
    else
        return static_cast<T*>(this->tryAcquireInterfaceWithClient(clientName, T::getInterfaceDesc(), pluginName));
}

template <typename T>
T* Framework::acquireInterface(const void* pluginInterface)
{
    const char* clientName = g_needToCall_CARB_GLOBALS_atGlobalScope ? g_carbClientName : nullptr;
    if (this->internalAcquireInterface)
        return static_cast<T*>(
            this->internalAcquireInterface({ sizeof(AcquireInterfaceOptions), clientName, T::getInterfaceDesc(),
                                             eAIFFromInterfaceType, pluginInterface }));
    else
        return static_cast<T*>(
            this->acquireInterfaceFromInterfaceWithClient(clientName, T::getInterfaceDesc(), pluginInterface));
}

template <typename T>
T* Framework::tryAcquireInterface(const void* pluginInterface)
{
    const char* clientName = g_needToCall_CARB_GLOBALS_atGlobalScope ? g_carbClientName : nullptr;
    if (this->internalAcquireInterface)
        return static_cast<T*>(this->internalAcquireInterface(
            { sizeof(AcquireInterfaceOptions), clientName, T::getInterfaceDesc(),
              AcquireInterfaceFlags(eAIFFromInterfaceType | fAIFOptional), pluginInterface }));
    else
        return static_cast<T*>(
            this->tryAcquireInterfaceFromInterfaceWithClient(clientName, T::getInterfaceDesc(), pluginInterface));
}

template <typename T>
T* Framework::acquireInterfaceFromLibrary(const char* libraryPath)
{
    const char* clientName = g_needToCall_CARB_GLOBALS_atGlobalScope ? g_carbClientName : nullptr;
    if (this->internalAcquireInterface)
        return static_cast<T*>(this->internalAcquireInterface(
            { sizeof(AcquireInterfaceOptions), clientName, T::getInterfaceDesc(), eAIFFromLibraryType, libraryPath }));
    else
        return static_cast<T*>(
            this->acquireInterfaceFromLibraryWithClient(clientName, T::getInterfaceDesc(), libraryPath));
}

template <typename T>
T* Framework::tryAcquireInterfaceFromLibrary(const char* libraryPath)
{
    const char* clientName = g_needToCall_CARB_GLOBALS_atGlobalScope ? g_carbClientName : nullptr;
    if (this->internalAcquireInterface)
        return static_cast<T*>(
            this->internalAcquireInterface({ sizeof(AcquireInterfaceOptions), clientName, T::getInterfaceDesc(),
                                             AcquireInterfaceFlags(eAIFFromLibraryType | fAIFOptional), libraryPath }));
    else
        return static_cast<T*>(
            this->tryAcquireInterfaceFromLibraryWithClient(clientName, T::getInterfaceDesc(), libraryPath));
}

template <typename T>
T* Framework::tryAcquireExistingInterface(const char* pluginName)
{
    const char* clientName = g_needToCall_CARB_GLOBALS_atGlobalScope ? g_carbClientName : nullptr;
    return this->internalAcquireInterface ?
               static_cast<T*>(this->internalAcquireInterface(
                   { sizeof(AcquireInterfaceOptions), clientName, T::getInterfaceDesc(),
                     AcquireInterfaceFlags(eAIFDefaultType | fAIFOptional | fAIFNoInitialize), pluginName })) :
               nullptr;
}

template <typename T>
uint32_t Framework::getInterfacesCount()
{
    const InterfaceDesc desc = T::getInterfaceDesc();
    return this->getInterfacesCountEx(desc);
}

template <typename T>
void Framework::acquireInterfaces(T** interfaces, uint32_t interfacesSize)
{
    const InterfaceDesc desc = T::getInterfaceDesc();
    const char* clientName = g_needToCall_CARB_GLOBALS_atGlobalScope ? g_carbClientName : nullptr;
    this->acquireInterfacesWithClient(clientName, desc, reinterpret_cast<void**>(interfaces), interfacesSize);
}

template <typename T>
void Framework::releaseInterface(T* pluginInterface)
{
    (void)(T::getInterfaceDesc()); // Compile-time check that the type is plugin interface
    const char* clientName = g_needToCall_CARB_GLOBALS_atGlobalScope ? g_carbClientName : nullptr;
    this->releaseInterfaceWithClient(clientName, pluginInterface);
}

template <typename T>
void Framework::setDefaultPlugin(const char* pluginName)
{
    const char* clientName = g_needToCall_CARB_GLOBALS_atGlobalScope ? g_carbClientName : nullptr;
    this->setDefaultPluginEx(clientName, T::getInterfaceDesc(), pluginName);
}

template <typename T>
LoadHookHandle Framework::addLoadHook(const char* pluginName, LoadHookFn func, void* user)
{
    const char* clientName = g_needToCall_CARB_GLOBALS_atGlobalScope ? g_carbClientName : nullptr;
    return this->internalAddLoadHook(T::getInterfaceDesc(), pluginName, clientName, func, user, true);
}

} // namespace carb

#ifdef CARB_FREE_UNDEFINED
#    pragma pop_macro("free")
#    undef CARB_FREE_UNDEFINED
#endif
