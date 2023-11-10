// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include <carb/Framework.h>
#include <carb/IObject.h>
#include <carb/Interface.h>
#include <carb/events/IEvents.h>

namespace omni
{
namespace ext
{

/**
 * Version struct, follows Semantic Versioning 2.0. https://semver.org/
 */
struct Version
{
    int32_t major;
    int32_t minor;
    int32_t patch;
    const char* prerelase;
    const char* build;
};

struct ExtensionInfo
{
    const char* id; ///! Unique Id == python module name
    const char* name; ///! name part of id
    const char* packageId; ///! packageId
    Version version;
    bool enabled;
    const char* title; ///! Extension can provide descriptive title (optional)
    const char* path;
};

enum class ExtensionPathType
{
    eCollection, // Folder with extensions
    eDirectPath, // Direct path to extension (as single file or a folder)
    eExt1Folder, // Deprecated Ext 1.0 Folder
    eCollectionUser, // Folder with extensions, read and stored in persistent settings
    eCollectionCache, // Folder with extensions, used for caching extensions downloaded from the registry
    eCount
};

struct ExtensionFolderInfo
{
    const char* path;
    ExtensionPathType type;
};

struct RegistryProviderInfo
{
    const char* name;
};

using ExtensionSummaryFlag = uint32_t;
static constexpr ExtensionSummaryFlag kExtensionSummaryFlagNone = 0;
static constexpr ExtensionSummaryFlag kExtensionSummaryFlagAnyEnabled = (1 << 1);
static constexpr ExtensionSummaryFlag kExtensionSummaryFlagBuiltin = (1 << 2);
static constexpr ExtensionSummaryFlag kExtensionSummaryFlagInstalled = (1 << 3);

struct ExtensionSummary
{
    const char* fullname; // ext_name-ext_tag
    ExtensionSummaryFlag flags;
    ExtensionInfo enabledVersion; // if any
    ExtensionInfo latestVersion; // latest version extension
};

/**
 * Extension manager change event stream events
 */
const carb::events::EventType kEventScriptChanged = CARB_EVENTS_TYPE_FROM_STR("SCRIPT_CHANGED");
const carb::events::EventType kEventFolderChanged = CARB_EVENTS_TYPE_FROM_STR("FOLDER_CHANGED");

/**
 * Extra events sent to IApp::getMessageBusEventStream() by extension manager
 */
const carb::events::EventType kEventRegistryRefreshBegin = CARB_EVENTS_TYPE_FROM_STR("omni.ext.REGISTRY_REFRESH_BEGIN");
const carb::events::EventType kEventRegistryRefreshEndSuccess =
    CARB_EVENTS_TYPE_FROM_STR("omni.ext.REGISTRY_REFRESH_END_SUCCESS");
const carb::events::EventType kEventRegistryRefreshEndFailure =
    CARB_EVENTS_TYPE_FROM_STR("omni.ext.REGISTRY_REFRESH_END_FAILURE");
const carb::events::EventType kEventExtensionPullBegin = CARB_EVENTS_TYPE_FROM_STR("omni.ext.EXTENSION_PULL_BEGIN");
const carb::events::EventType kEventExtensionPullEndSuccess =
    CARB_EVENTS_TYPE_FROM_STR("omni.ext.EXTENSION_PULL_END_SUCCESS");
const carb::events::EventType kEventExtensionPullEndFailure =
    CARB_EVENTS_TYPE_FROM_STR("omni.ext.EXTENSION_PULL_END_FAILURE");


/**
 * Version lock generation parameters
 */
struct VersionLockDesc
{
    bool separateFile;
    bool skipCoreExts;
    bool overwrite;
};

/**
 * Index refresh or extension pull state communicated by registry provider back to the extension manager.
 */
enum class DownloadState
{
    eDownloading,
    eDownloadSuccess,
    eDownloadFailure
};

/**
 * Input to running custom extension solver.
 */
struct SolverInput
{
    const char** extensionNames; ///! List of extension names to solve (to enable). Names can include version:
                                 ///`omni.foo-1.2.3`
    size_t extensionNameCount;
    bool addEnabled; ///! Automatically add already enabled extension to the input (to take into account)
    bool returnOnlyDisabled; ///! If true exclude from the result extensions that are currently already enabled
};


/**
 * Interface to be implemented by registry providers.
 *
 * Extension manager will use it to pull (when resolving dependencies) and publish extensions.
 */
class IRegistryProvider : public carb::IObject
{
public:
    /**
     * Trigger index async refresh. That function maybe called many time and returns immediately. If returned result is
     * `DownloadState::eDownloadSuccess` next call to `syncIndex()` won't block to download index, but use
     * downloaded instead.
     */
    virtual DownloadState refreshIndex() = 0;

    /**
     * Extension manager will call that function from time to time to get remote index. The structure of this dict is a
     * map from extension ids to extension configuration (mostly extension.toml files).
     */
    virtual carb::dictionary::Item* syncIndex() = 0;

    /**
     * Extension manager will call that function when user triggers extension publishing.
     */
    virtual bool publishExtension(const char* extPath, carb::dictionary::Item* extDict) = 0;

    /**
     * Extension manager will call that function when user triggers extension unpublishing (removing from remote).
     */
    virtual bool unpublishExtension(const char* extId) = 0;

    /**
     * Extension manager will call that function when user triggers extension pulling. Extension id here is from the
     * remote index, so the registry should have it available and needs to download it during the functioncall.
     */
    virtual bool pullExtension(const char* extId, const char* extFolder) = 0;

    /**
     * Async version of extension pulling. That function can be called many times and returns immediately.
     */
    virtual DownloadState pullExtensionAsync(const char* extId, const char* extFolder) = 0;

    /**
     * Set max stripping level of registry to sync (currently 1 or 0).
     * The idea is to first sync to smaller index version (stripped) quickly and try solve deps. Then fallback to
     * full index. It works as follows:
     *  - For each provider setMaxStrippingLevel(1) and if true returned call syncIndex().
     *  - Try to solve dependencies, if succeed stop
     *  - If can't solve, go to level 0: setMaxStrippingLevel(0) and if true returned call syncIndex().
     * This way if provider doesn't support level 1, it can sync to level 0 and return false when
     * setMaxStrippingLevel(0) is called to avoid resyncing again. That scales to any number of levels in the future.
     */
    virtual bool setMaxStrippingLevel(int32_t level) = 0;
};

using IRegistryProviderPtr = carb::ObjectPtr<IRegistryProvider>;

/**
 * Interface to be implemented to add new extension path protocols.
 *
 * Extension manager will call it. It must return local file system path for the provided url to be registered instead
 * of url. It is a job of protocol provider to update and maintain that local path. Extension manager will treat that
 * pass the same as regular local extension search paths.
 */
class IPathProtocolProvider : public carb::IObject
{
public:
    virtual const char* addPath(const char* url) = 0;

    virtual void removePath(const char* url) = 0;
};

using IPathProtocolProviderPtr = carb::ObjectPtr<IPathProtocolProvider>;

class IExtensionManagerHooks;

class ExtensionManager : public carb::IObject
{
public:
    /**
     * Process all accumulated changes of extension since last call of this function. If extension was changed it will
     * be reloaded. If it was added, removed (including adding new folders) - changes will also be applied in this
     * function. If any changes happened accoding events are pushed into events stream:
     * @ref kEventScriptChanged
     * @ref kEventFolderChanged
     */
    virtual void processAndApplyAllChanges() = 0;

    /**
     * Gets number of registered local extensions.
     */
    virtual size_t getExtensionCount() const = 0;

    /**
     * Fills the array of ExtensionInfo with registered local extensions.
     *
     * @param extensions The extension info array to be filled. It must be large enough to hold @ref getExtensionCount
     * entries.
     */
    virtual void getExtensions(ExtensionInfo* extensions) const = 0;

    /**
     * Get an extension info dictionary for a single extension by id.
     */
    virtual carb::dictionary::Item* getExtensionDict(const char* extId) const = 0;

    void setExtensionEnabled(const char* extensionId, bool enabled)
    {
        setExtensionEnabledBatch(&extensionId, 1, enabled);
    }

    virtual void setExtensionEnabledBatch(const char** extensionId, size_t extensionIdCount, bool enabled) = 0;

    /**
     * Enable/disable extension immediately (in that call), returns False if failed.
     */
    bool setExtensionEnabledImmediate(const char* extensionId, bool enabled)
    {
        return setExtensionEnabledBatchImmediate(&extensionId, 1, enabled);
    }
    virtual bool setExtensionEnabledBatchImmediate(const char** extensionId, size_t extensionIdCount, bool enabled) = 0;

    /**
     * Set extensions to exclude on following solver/startup routines. They persist until next call to this function.
     */
    virtual void setExtensionsExcluded(const char** extensionId, size_t extensionIdCount) = 0;

    /**
     * Gets number of monitored extension folders.
     *
     * @return Extension folder count.
     */
    virtual size_t getFolderCount() const = 0;

    /**
     * Gets monitored extension folders.
     * @param extensions The extension folder info array to be filled. It must be large enough to hold @ref
     * getFolderCount entries.
     */
    virtual void getFolders(ExtensionFolderInfo* extensionFolderInfos) const = 0;

    /**
     * Add extension path (folder or direct path).
     * @param type How to treat a path.
     */
    virtual void addPath(const char* path, ExtensionPathType type = ExtensionPathType::eCollection) = 0;

    /**
     * Remove extension path.
     */
    virtual void removePath(const char* folder) = 0;

    /**
     * Extensions change event stream.
     *
     * Event is triggered when extensions are added, removed, toggled.
     */
    virtual carb::events::IEventStream* getChangeEventStream() const = 0;

    /**
     * Interface to install hooks to "extend" extension manager.
     */
    virtual IExtensionManagerHooks* getHooks() const = 0;

    /**
     * Add new registry provider, name must be unique `false` returned otherwise.
     */
    virtual bool addRegistryProvider(const char* providerName, IRegistryProvider* provider) = 0;

    /**
     * Remove registry provider.
     */
    virtual void removeRegistryProvider(const char* providerName) = 0;

    /**
     * Gets number of registry providers.
     */
    virtual size_t getRegistryProviderCount() const = 0;

    /**
     * Gets registry providers.
     * @param extensions The registry provider info array to be filled. It must be large enough to hold @ref
     * getRegistryProviderCount entries.
     */
    virtual void getRegistryProviders(RegistryProviderInfo* infos) = 0;

    /**
     * Non blocking call to start registry refresh routine.
     */
    virtual void refreshRegistry() = 0;

    /**
     * Blocking call to synchronize with remote registry.
     */
    virtual bool syncRegistry() = 0;

    /**
     * Gets number of extensions in remote registry.
     */
    virtual size_t getRegistryExtensionCount() const = 0;

    /**
     * Fills the array of ExtensionInfo with compatible extensions in a remote registry.
     *
     * @param extensions The extension info array to be filled. It must be large enough to hold @ref
     * getRegistryExtensionCount entries.
     */
    virtual void getRegistryExtensions(ExtensionInfo* extensions) const = 0;

    /**
     * Gets number of extension packages in remote registry.
     */
    virtual size_t getRegistryExtensionPackageCount() const = 0;

    /**
     * Fills the array of ExtensionInfo with all extension packages in a remote registry.
     *
     * That function will return all extensions in the registry, including packages for other platforms, incompatible
     * with current runtime.
     *
     * @param extensions The extension info array to be filled. It must be large enough to hold @ref
     * getRegistryPackageCount entries.
     */
    virtual void getRegistryExtensionPackages(ExtensionInfo* extensions) const = 0;

    /**
     * Get an extension info dictionary for a single extension by id from registry.
     */
    virtual carb::dictionary::Item* getRegistryExtensionDict(const char* extId) = 0;

    /**
     * Package and upload extension to the registry.
     *
     * Note: If providerName is empty and there are multiple providers `/app/extensions/registryPublishDefault` setting
     * is used.
     */
    virtual bool publishExtension(const char* extId, const char* providerName = "", bool allowOverwrite = false) = 0;

    /**
     * Remove extension from the registry.
     *
     * Note: If providerName is empty and there are multiple providers `/app/extensions/registryPublishDefault` setting
     * is used.
     */
    virtual bool unpublishExtension(const char* extId, const char* providerName = "") = 0;

    /**
     * Download extension from the registry.
     */
    virtual bool pullExtension(const char* extId) = 0;

    /**
     * Download extension from the registry async. This function returns immediately.
     */
    virtual void pullExtensionAsync(const char* extId) = 0;

    /**
     * Fetch all extension summaries. Summary are extensions grouped by version. One summary per fullname(name+tag).
     * Returned array is valid until next call of this function.
     */
    virtual void fetchExtensionSummaries(ExtensionSummary** summaries, size_t* summaryCount) = 0;

    /**
     * Fetch all matching compatible extensions fullname(name+tag) and optionally a version.
     *
     * Example of valid 'fullname' input: "omni.foo", "omni.foo-1", "omni.foo-0.9"
     *
     * Returned array is valid until next call of this function.
     *
     * Partial version can also be passed. So `omni.foo`, `omni.foo-2`, `omni.foo-1.2.3` all valid input for this
     * function.
     */
    virtual void fetchExtensionVersions(const char* fullname, ExtensionInfo** extensions, size_t* extensionCount) = 0;

    /**
     * Fetch all matching extension packages fullname(name+tag) and optionally a version.
     *
     * Example of valid 'fullname' input: "omni.foo", "omni.foo-1", "omni.foo-0.9"
     *
     * That function will return all extensions in the registry, including packages for other platforms, incompatible
     * with current runtime.
     *
     * Returned array is valid until next call of this function.
     *
     * Partial version can also be passed. So `omni.foo`, `omni.foo-2`, `omni.foo-1.2.3` all valid input for this
     * function.
     */
    virtual void fetchExtensionPackages(const char* fullname, ExtensionInfo** extensions, size_t* extensionCount) = 0;

    /**
     * Disable all enabled extensions. When manager is destroyed it is called automatically.
     */
    virtual void disableAllExtensions() = 0;

    /**
     * Read user extension paths from persitent settings and add to search path as eCollectionUser.
     */
    virtual void addUserPaths() = 0;

    /**
     * Read cache extension paths from settings and add to search path as eCollectionCache. It is necessary for registry
     * usage.
     */
    virtual void addCachePath() = 0;

    /**
     * Generate settings that contains versions of started dependencies to lock them (experimental).
     */
    virtual bool generateVersionLock(const char* extId, const VersionLockDesc& desc) = 0;

    /**
     * Add new path protocol provider, scheme must be unique `false` returned otherwise.
     */
    virtual bool addPathProtocolProvider(const char* scheme, IPathProtocolProvider* provider) = 0;

    /**
     * Remove path protocol provider.
     */
    virtual void removePathProtocolProvider(const char* scheme) = 0;

    /**
     * Run extension dependencies solver on the input.
     *
     * Input is a list of extension, they can be names, full id, partial versions like `ommi.foo-2`.
     *
     * If solver succeeds it returns a list of extensions that satisfy the input and `true`, otherwise `errorMessage`
     * contains explanation of what failed.
     *
     * Returned array and error message is valid until next call of this function.
     */
    virtual bool solveExtensions(const SolverInput& input,
                                 ExtensionInfo** extensions,
                                 size_t* extensionCount,
                                 const char** errorMessage) = 0;

    /**
     * Gets number of local extension package count.
     */
    virtual size_t getExtensionPackageCount() const = 0;

    /**
     * Fills the array of ExtensionInfo with local extension packages.
     *
     * That function will return all local extensions, including packages for other platforms, incompatible
     * with current targets.

     * @param extensions The extension info array to be filled. It must be large enough to hold @ref
     getExtensionPackageCount
     * entries.
     */
    virtual void getExtensionPackages(ExtensionInfo* extensions) const = 0;

    /**
     * Uninstall downloaded extension (remove).
     */
    virtual bool uninstallExtension(const char* extId) = 0;
};

struct IExtensions
{
    CARB_PLUGIN_INTERFACE("omni::ext::IExtensions", 1, 1)

    /**
     * Create new extension manager.
     * @param changeEventStream Stream for to push change events to. Manager will not pump the stream. Can be nullptr.
     */
    ExtensionManager*(CARB_ABI* createExtensionManagerPtr)(carb::events::IEventStream* changeEventStream);
    carb::ObjectPtr<ExtensionManager> createExtensionManager(carb::events::IEventStream* changeEventStream);
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                             Extension Manager Hooks                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/**
 * Different moments in extension state lifetime.
 */
enum class ExtensionStateChangeType
{
    eBeforeExtensionEnable, ///! Right before extension being enabled.
    eAfterExtensionEnable, ///! Right after extension was enabled.
    eBeforeExtensionShutdown, ///! Right before extension being disabled.
    eAfterExtensionShutdown, ///! Right after extension being disabled.
    eCount,
};

/**
 * Interface to implement for extension state change hooks.
 */
class IExtensionStateChangeHook : public carb::IObject
{
public:
    virtual void onStateChange(const char* extId, ExtensionStateChangeType type) = 0;
};

using IExtensionStateChangeHookPtr = carb::ObjectPtr<IExtensionStateChangeHook>;


/**
 * Hook call order.
 */
using Order = int32_t;

/**
 * Default order.
 */
static constexpr Order kDefaultOrder = 0;


/**
 * Hook holder. Hook is valid while the holder is alive.
 */
class IHookHolder : public carb::IObject
{
};

using IHookHolderPtr = carb::ObjectPtr<IHookHolder>;


/**
 * Extension manager sublass with all the hooks that can be installed into it.
 */
class IExtensionManagerHooks
{
public:
    /**
     * Install hook on extension state change. Hook will be called when extension is getting enabled or disabled.
     *
     * You can filter for extensions with specific config/dict to only be called for those. That allows to implement
     * new configuration parameters handled by your hook.
     *
     * @param hook Instance.
     * @param type Extension state change moment to hook into.
     * @param extFullName Extension name to look for. Hook is only called for extensions with matching name. Can be
     * empty.
     * @param extDictPath Extension dictionary path to look for. Hook is only called if it is present.
     * @param order Hook call order (if there are multiple).
     * @param hookName Hook name for debugging and logging.
     * @return Hook Holder object. When it dies hook deactivates.
     */
    IHookHolderPtr createExtensionStateChangeHook(IExtensionStateChangeHook* hook,
                                                  ExtensionStateChangeType type,
                                                  const char* extFullName = "",
                                                  const char* extDictPath = "",
                                                  Order order = kDefaultOrder,
                                                  const char* hookName = nullptr);
    virtual IHookHolder* createExtensionStateChangeHookPtr(IExtensionStateChangeHook* hook,
                                                           ExtensionStateChangeType type,
                                                           const char* extFullName = "",
                                                           const char* extDictPath = "",
                                                           Order order = kDefaultOrder,
                                                           const char* hookName = nullptr) = 0;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                Inline Functions                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////// ExtensionManagerHooks //////////////

inline IHookHolderPtr IExtensionManagerHooks::createExtensionStateChangeHook(IExtensionStateChangeHook* hook,
                                                                             ExtensionStateChangeType type,
                                                                             const char* extFullName,
                                                                             const char* extDictPath,
                                                                             Order order,
                                                                             const char* hookName)
{
    return carb::stealObject(
        this->createExtensionStateChangeHookPtr(hook, type, extFullName, extDictPath, order, hookName));
}


////////////// IExtensions //////////////

inline carb::ObjectPtr<ExtensionManager> IExtensions::createExtensionManager(carb::events::IEventStream* changeEventStream)
{
    return carb::stealObject(this->createExtensionManagerPtr(changeEventStream));
}

} // namespace ext
} // namespace omni
