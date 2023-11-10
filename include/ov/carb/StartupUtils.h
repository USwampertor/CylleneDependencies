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
//! @brief Contains @ref carb::startupFramework() and @ref carb::shutdownFramework().  Consider using @ref
//! OMNI_CORE_INIT(), which invokes these methods for you in a safe manner.
#pragma once

#include "Framework.h"
#include "crashreporter/CrashReporterUtils.h"
#include "dictionary/DictionaryUtils.h"
#include "extras/AppConfig.h"
#include "extras/CmdLineParser.h"
#include "extras/EnvironmentVariableParser.h"
#include "extras/EnvironmentVariableUtils.h"
#include "extras/Path.h"
#include "extras/VariableSetup.h"
#include "filesystem/IFileSystem.h"
#include "l10n/L10nUtils.h"
#include "logging/Log.h"
#include "logging/LoggingSettingsUtils.h"
#include "logging/StandardLogger.h"
#include "profiler/Profile.h"
#include "settings/ISettings.h"
#include "tokens/ITokens.h"
#include "tokens/TokensUtils.h"
#include "../omni/structuredlog/StructuredLogSettingsUtils.h"

#include <array>
#include <map>
#include <string>
#include <vector>

namespace carb
{

//! Parameters passed to @ref carb::startupFramework().
struct StartupFrameworkDesc
{
    //! A string containing either one of two things:
    //!
    //! * A path to a configuration file.
    //!
    //! * A raw string contain the configuration (in either JSON or TOML format based on @p configFormat ).
    //!
    //! @ref carb::startupFramework() will first check to see if the string maps to an existing file, and if not, the
    //! string is treated as a raw configuration string.
    const char* configString; // Path to a config file or string with configuration data

    char** argv; //!< Array of command line arguments
    int argc; //!< Number of command line arguments

    //! An array of search paths for plugins.
    //!
    //! Relative search paths are relative to the executable's directory, not the current working directory.
    //!
    //! These search paths will be used when loading the base set of carbonite plugins (such as carb.settings.plugin),
    //! then this will be set as the default value for the @ref carb::settings::ISettings key `/pluginSearchPaths` (this
    //! allows the setting to be overridden if you set it in config.toml or pass it on the command line).
    //!
    //! Passing an empty array will result in the executable directory being used as the default search path.
    //!
    //! This option is needed when the base set of Carbonite plugins are not inside of the executable's directory;
    //! otherwise, `/pluginSearchPaths` could be set in config.toml or via the command line.
    //!
    //! Defaults to `nullptr`.
    const char* const* initialPluginsSearchPaths;
    size_t initialPluginsSearchPathCount; //!< Size of array of paths to search for plugins

    //! Prefix of command line arguments serving as overrides for configuration values.  Default is `--/`.
    const char* cmdLineParamPrefix;

    //! Prefix of environment variables serving as overrides for configuration values.  Default is `OMNI_APPNAME_`.
    const char* envVarsParamPrefix;

    const char* configFormat; //!< The selected config format ("toml", "json", etc).  Default is "toml".

    const char* appNameOverride; //!< Override automatic app name search.  Defaults to `nullptr`.
    const char* appPathOverride; //!< Override automatic app path search. Defaults to `nullptr`.

    bool disableCrashReporter; //!< If `true`, the crash reporter plugin will not be loaded.  Defaults to `false`.

    //! Returns a @ref StartupFrameworkDesc with default values.
    static StartupFrameworkDesc getDefault()
    {
        static constexpr const char* kDefaultCmdLineParamPrefix = "--/";
        static constexpr const char* kDefaultEnvVarsParamPrefix = "OMNI_APPNAME_";
        static constexpr const char* kDefaultConfigFormat = "toml";

        StartupFrameworkDesc result{};

        result.cmdLineParamPrefix = kDefaultCmdLineParamPrefix;
        result.envVarsParamPrefix = kDefaultEnvVarsParamPrefix;
        result.configFormat = kDefaultConfigFormat;

        return result;
    }
};

/**
 * Simple plugin loading function wrapper that loads plugins matching multiple patterns.
 *
 * Consider using @ref carb::startupFramework(), which calls this function with user defined paths via config files, the
 * environment, and the command line.
 *
 * @param pluginNamePatterns String that contains plugin names pattern - wildcards are supported.
 * @param pluginNamePatternCount Number of items in @p pluginNamePatterns.
 * @param searchPaths Array of paths to look for plugins in.
 * @param searchPathCount Number of paths in searchPaths array.
 */
inline void loadPluginsFromPatterns(const char* const* pluginNamePatterns,
                                    size_t pluginNamePatternCount,
                                    const char* const* searchPaths = nullptr,
                                    size_t searchPathCount = 0)
{
    Framework* f = getFramework();
    PluginLoadingDesc desc = PluginLoadingDesc::getDefault();
    desc.loadedFileWildcards = pluginNamePatterns;
    desc.loadedFileWildcardCount = pluginNamePatternCount;
    desc.searchPaths = searchPaths;
    desc.searchPathCount = searchPathCount;
    f->loadPlugins(desc);
}

/**
 * Simple plugin loading function wrapper that loads plugins matching a single pattern.
 *
 * Consider using @ref carb::startupFramework(), which calls this function with user defined paths via config files, the
 * environment, and the command line.
 *
 * @param pluginNamePattern String that contains a plugin pattern - wildcards are supported.
 * @param searchPaths Array of paths to look for plugins in.
 * @param searchPathCount Number of paths in searchPaths array.
 */
inline void loadPluginsFromPattern(const char* pluginNamePattern,
                                   const char* const* searchPaths = nullptr,
                                   size_t searchPathCount = 0)
{
    const char* plugins[] = { pluginNamePattern };
    loadPluginsFromPatterns(plugins, countOf(plugins), searchPaths, searchPathCount);
}

//! Internal
namespace detail
{

//! Loads plugins based on settings specified in the given @p settings object.
//!
//! The settings read populated a @ref carb::PluginLoadingDesc.  The settings read are:
//!
//! @rst
//!
//! /pluginSearchPaths
//!     Array of paths in which to search for plugins.
//!
//! /pluginSearchRecursive
//!     If ``true`` recursively each path in `/pluginSearchPaths`.
//!
//! /reloadablePlugins
//!     Array of plugin wildcards that mark plugins as reloadable.
//!
//! /pluginsLoaded
//!     Wildcard of plugins to load.
//!
//! /pluginsExcluded
//!     Wildcard of plugins that match `/pluginsLoaded` but should not be loaded.
//!
//! @endrst
//!
//! Do not use this function directly.  Rather, call @ref carb::startupFramework().
inline void loadPluginsFromConfig(settings::ISettings* settings)
{
    if (settings == nullptr)
        return;

    Framework* f = getFramework();

    // Initialize the plugin loading description to default configuration,
    // and override parts of it to the config values, if present.

    PluginLoadingDesc loadingDesc = PluginLoadingDesc::getDefault();

    // Check if plugin search paths are present in the config, and override if present
    const char* kPluginSearchPathsKey = "/pluginSearchPaths";
    std::vector<const char*> pluginSearchPaths(settings->getArrayLength(kPluginSearchPathsKey));

    if (!pluginSearchPaths.empty())
    {
        settings->getStringBufferArray(kPluginSearchPathsKey, pluginSearchPaths.data(), pluginSearchPaths.size());
        loadingDesc.searchPaths = pluginSearchPaths.data();
        loadingDesc.searchPathCount = pluginSearchPaths.size();
    }

    const char* kPluginSearchRecursive = "/pluginSearchRecursive";
    // Is search recursive?
    if (settings->isAccessibleAs(carb::dictionary::ItemType::eBool, kPluginSearchRecursive))
    {
        loadingDesc.searchRecursive = settings->getAsBool(kPluginSearchRecursive);
    }

    // Check/override reloadable plugins if present
    const char* kReloadablePluginsKey = "/reloadablePlugins";
    std::vector<const char*> reloadablePluginFiles(settings->getArrayLength(kReloadablePluginsKey));

    if (!reloadablePluginFiles.empty())
    {
        settings->getStringBufferArray(kReloadablePluginsKey, reloadablePluginFiles.data(), reloadablePluginFiles.size());
        loadingDesc.reloadableFileWildcards = reloadablePluginFiles.data();
        loadingDesc.reloadableFileWildcardCount = reloadablePluginFiles.size();
    }

    // Check/override plugins to load if present
    const char* kPluginsLoadedKey = "/pluginsLoaded";
    std::vector<const char*> pluginsLoaded;
    if (settings->getItemType(kPluginsLoadedKey) == dictionary::ItemType::eDictionary)
    {
        pluginsLoaded.resize(settings->getArrayLength(kPluginsLoadedKey));
        settings->getStringBufferArray(kPluginsLoadedKey, pluginsLoaded.data(), pluginsLoaded.size());
        loadingDesc.loadedFileWildcards = pluginsLoaded.size() ? pluginsLoaded.data() : nullptr;
        loadingDesc.loadedFileWildcardCount = pluginsLoaded.size();
    }

    const char* kPluginsExcludedKey = "/pluginsExcluded";
    std::vector<const char*> pluginsExcluded;
    if (settings->getItemType(kPluginsExcludedKey) == dictionary::ItemType::eDictionary)
    {
        pluginsExcluded.resize(settings->getArrayLength(kPluginsExcludedKey));
        settings->getStringBufferArray(kPluginsExcludedKey, pluginsExcluded.data(), pluginsExcluded.size());
        loadingDesc.excludedFileWildcards = pluginsExcluded.size() ? pluginsExcluded.data() : nullptr;
        loadingDesc.excludedFileWildcardCount = pluginsExcluded.size();
    }

    // Load plugins based on the resulting desc
    if (loadingDesc.loadedFileWildcardCount)
        f->loadPlugins(loadingDesc);
}

//! Sets @ref carb::Framework's "default" plugins from the given @p settings `/defaultPlugins` key.
//!
//! In short, this function calls @ref carb::Framework::setDefaultPlugin for each plugin name in `/defaultPlugins`.
//! However, since the interface type cannot be specified, plugins listed in `/defaultPlugins` will become the default
//! plugin for \a all interfaces they provide.
//!
//! This function assumes the plugins in `/defaultPlugins` have already been loaded.
//!
//! The following keys are used from @p settings:
//!
//! @rst
//!  /defaultPlugins
//!      A list of plugin names.  These plugins become the default plugins to use when acquire their interfaces.
//! @endrst
//!
//! Do not use this function directly.  Rather, call @ref carb::startupFramework().
inline void setDefaultPluginsFromConfig(settings::ISettings* settings)
{
    if (settings == nullptr)
        return;

    Framework* f = getFramework();

    // Default plugins
    const char* kDefaultPluginsKey = "/defaultPlugins";
    std::vector<const char*> defaultPlugins(settings->getArrayLength(kDefaultPluginsKey));
    if (!defaultPlugins.empty())
    {
        settings->getStringBufferArray(kDefaultPluginsKey, defaultPlugins.data(), defaultPlugins.size());

        for (const char* pluginName : defaultPlugins)
        {
            // Set plugin as default for all interfaces it provides
            const PluginDesc& pluginDesc = f->getPluginDesc(pluginName);
            for (size_t i = 0; i < pluginDesc.interfaceCount; i++)
            {
                f->setDefaultPluginEx(g_carbClientName, pluginDesc.interfaces[i], pluginName);
            }
        }
    }
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
// If the dict item is a special raw string then it returns pointer to the buffer past the special raw string marker
// In all other cases it returns nullptr
inline const char* getRawStringFromItem(carb::dictionary::IDictionary* dictInterface, const carb::dictionary::Item* item)
{
    if (!dictInterface || !item)
    {
        return nullptr;
    }

    if (dictInterface->getItemType(item) != dictionary::ItemType::eString)
    {
        return nullptr;
    }

    const char* stringBuffer = dictInterface->getStringBuffer(item);
    if (!stringBuffer)
    {
        return nullptr;
    }

    constexpr char kSpecialRawStringMarker[] = "$raw:";
    constexpr size_t kMarkerLen = carb::countOf(kSpecialRawStringMarker) - 1;

    if (std::strncmp(stringBuffer, kSpecialRawStringMarker, kMarkerLen) != 0)
    {
        return nullptr;
    }

    return stringBuffer + kMarkerLen;
}

class LoadSettingsHelper
{
public:
    struct SupportedConfigInfo
    {
        const char* configFormatName;
        const char* serializerPluginName;
        const char* configExt;
    };

    LoadSettingsHelper()
    {
        Framework* f = getFramework();
        m_fs = f->acquireInterface<filesystem::IFileSystem>();
    }

    struct LoadSettingsDesc
    {
        std::string appDir; // Application directory
        std::string appName; // Application name
        const char* configStringOrPath; // Configuration string that can be null, string containing configuration data
                                        // (in selected configFormat) or a path to a config file

        const extras::ConfigLoadHelper::CmdLineOptionsMap* cmdLineOptionsMap; // Mapping of the command line options

        const extras::ConfigLoadHelper::PathwiseEnvOverridesMap* pathwiseEnvOverridesMap; // Mapping of path-wise
                                                                                          // environment variables that
                                                                                          // will be mapped into
                                                                                          // corresponding settings

        const extras::ConfigLoadHelper::EnvVariablesMap* envVariablesMap; // Mapping of common environment variables

        const char* const* pluginSearchPaths; // Array of directories used by the system to search for plugins
        size_t pluginSearchPathCount; // Number of elements in the pluginSearchPaths
        const char* cmdLineConfigPath; // Path to a file containing config override (in selected configFormat), can be
                                       // null
        const char* configFormat; // Selected configuration format that is supported by the system

        inline static LoadSettingsDesc getDefault() noexcept
        {
            LoadSettingsDesc result{};

            Framework* f = getFramework();
            filesystem::IFileSystem* fs = f->acquireInterface<filesystem::IFileSystem>();

            extras::Path execPathStem(extras::getPathStem(fs->getExecutablePath()));
            // Initialize application path and name to the executable path and name
            result.appName = execPathStem.getFilename();
            result.appDir = execPathStem.getParent();

            result.configFormat = "toml";

            return result;
        }

        inline void overwriteWithNonEmptyParams(const LoadSettingsDesc& other) noexcept
        {
            if (!other.appDir.empty())
            {
                appDir = other.appDir;
            }
            if (!other.appName.empty())
            {
                appName = other.appName;
            }
            if (other.configStringOrPath)
            {
                configStringOrPath = other.configStringOrPath;
            }
            if (other.cmdLineOptionsMap)
            {
                cmdLineOptionsMap = other.cmdLineOptionsMap;
            }
            if (other.pathwiseEnvOverridesMap)
            {
                pathwiseEnvOverridesMap = other.pathwiseEnvOverridesMap;
            }
            if (other.envVariablesMap)
            {
                envVariablesMap = other.envVariablesMap;
            }
            if (other.pluginSearchPaths)
            {
                pluginSearchPaths = other.pluginSearchPaths;
                pluginSearchPathCount = other.pluginSearchPathCount;
            }
            if (other.cmdLineConfigPath)
            {
                cmdLineConfigPath = other.cmdLineConfigPath;
            }
            if (other.configFormat)
            {
                configFormat = other.configFormat;
            }
        }
    };

    void loadBaseSettingsPlugins(const char* const* pluginSearchPaths, size_t pluginSearchPathCount)
    {
        Framework* f = getFramework();

        // clang-format off
        const char* plugins[] = {
            "carb.dictionary.plugin",
            "carb.settings.plugin",
            "carb.tokens.plugin",
            m_selectedConfigInfo ? m_selectedConfigInfo->serializerPluginName : "carb.dictionary.serializer-toml.plugin"
        };
        // clang-format on
        loadPluginsFromPatterns(plugins, countOf(plugins), pluginSearchPaths, pluginSearchPathCount);

        m_idict = f->tryAcquireInterface<dictionary::IDictionary>();
        if (m_idict == nullptr)
        {
            CARB_LOG_INFO("Couldn't acquire dictionary::IDictionary interface on startup to load the settings.");
            return;
        }

        m_settings = f->tryAcquireInterface<settings::ISettings>();
        if (m_settings == nullptr)
        {
            CARB_LOG_INFO("Couldn't acquire settings::ISettings interface on startup to load the settings.");
        }
    }

    class ConfigStageLoader
    {
    public:
        static constexpr const char* kConfigSuffix = ".config";
        static constexpr const char* kOverrideSuffix = ".override";

        ConfigStageLoader(filesystem::IFileSystem* fs,
                          dictionary::ISerializer* configSerializer,
                          LoadSettingsHelper* helper,
                          const SupportedConfigInfo* selectedConfigInfo,
                          const extras::ConfigLoadHelper::EnvVariablesMap* envVariablesMap)
            : m_fs(fs),
              m_configSerializer(configSerializer),
              m_helper(helper),
              m_selectedConfigInfo(selectedConfigInfo),
              m_envVariablesMap(envVariablesMap)
        {
            m_possibleConfigPathsStorage.reserve(4);
        }

        dictionary::Item* loadAndMergeSharedUserSpaceConfig(const extras::Path& userFolder,
                                                            dictionary::Item* combinedConfig,
                                                            std::string* sharedUserSpaceFilepath)
        {
            if (!userFolder.isEmpty())
            {
                m_possibleConfigPathsStorage.clear();
                m_possibleConfigPathsStorage.emplace_back(userFolder / "omni" + kConfigSuffix +
                                                          m_selectedConfigInfo->configExt);

                return tryLoadAnySettingsAndMergeIntoTarget(m_configSerializer, combinedConfig,
                                                            m_possibleConfigPathsStorage, m_envVariablesMap,
                                                            sharedUserSpaceFilepath);
            }
            return combinedConfig;
        }

        dictionary::Item* loadAndMergeAppSpecificUserSpaceConfig(const extras::Path& userFolder,
                                                                 const std::string& appName,
                                                                 dictionary::Item* combinedConfig,
                                                                 std::string* appSpecificUserSpaceFilepath)
        {
            if (!userFolder.isEmpty())
            {
                m_possibleConfigPathsStorage.clear();
                m_possibleConfigPathsStorage.emplace_back(userFolder / appName + kConfigSuffix +
                                                          m_selectedConfigInfo->configExt);

                return tryLoadAnySettingsAndMergeIntoTarget(m_configSerializer, combinedConfig,
                                                            m_possibleConfigPathsStorage, m_envVariablesMap,
                                                            appSpecificUserSpaceFilepath);
            }
            return combinedConfig;
        }

        dictionary::Item* loadAndMergeLocalSpaceConfig(const std::string& appDir,
                                                       const std::string& appName,
                                                       dictionary::Item* combinedConfig,
                                                       std::string* localSpaceConfigFilepath)
        {
            const extras::Path cwd(m_fs->getCurrentDirectoryPath());
            const extras::Path appDirPath(appDir);
            const extras::Path exePath(m_fs->getExecutableDirectoryPath());
            const std::string appConfig = appName + kConfigSuffix + m_selectedConfigInfo->configExt;

            m_possibleConfigPathsStorage.clear();
            m_possibleConfigPathsStorage.emplace_back(cwd / appConfig);
            if (!appDir.empty())
            {
                m_possibleConfigPathsStorage.emplace_back(appDirPath / appConfig);
            }
            if (appDirPath != exePath)
            {
                m_possibleConfigPathsStorage.emplace_back(exePath / appConfig);
            }

            return tryLoadAnySettingsAndMergeIntoTarget(m_configSerializer, combinedConfig, m_possibleConfigPathsStorage,
                                                        m_envVariablesMap, localSpaceConfigFilepath);
        }

        dictionary::Item* loadAndMergeSharedUserSpaceConfigOverride(dictionary::Item* combinedConfig,
                                                                    const std::string& sharedUserSpaceFilepath)
        {
            if (!sharedUserSpaceFilepath.empty())
            {
                m_possibleConfigPathsStorage.clear();
                addPossiblePathOverridesForSearch(
                    extras::getPathStem(sharedUserSpaceFilepath), m_selectedConfigInfo->configExt);

                return tryLoadAnySettingsAndMergeIntoTarget(
                    m_configSerializer, combinedConfig, m_possibleConfigPathsStorage, m_envVariablesMap, nullptr);
            }
            return combinedConfig;
        }

        dictionary::Item* loadAndMergeAppSpecificUserSpaceConfigOverride(dictionary::Item* combinedConfig,
                                                                         const std::string& appSpecificUserSpaceFilepath)
        {
            if (!appSpecificUserSpaceFilepath.empty())
            {
                m_possibleConfigPathsStorage.clear();
                addPossiblePathOverridesForSearch(
                    extras::getPathStem(appSpecificUserSpaceFilepath), m_selectedConfigInfo->configExt);

                return tryLoadAnySettingsAndMergeIntoTarget(
                    m_configSerializer, combinedConfig, m_possibleConfigPathsStorage, m_envVariablesMap, nullptr);
            }
            return combinedConfig;
        }

        dictionary::Item* loadAndMergeLocalSpaceConfigOverride(dictionary::Item* combinedConfig,
                                                               const std::string& localSpaceConfigFilepath)
        {
            if (!localSpaceConfigFilepath.empty())
            {
                m_possibleConfigPathsStorage.clear();
                addPossiblePathOverridesForSearch(
                    extras::getPathStem(localSpaceConfigFilepath), m_selectedConfigInfo->configExt);

                return tryLoadAnySettingsAndMergeIntoTarget(
                    m_configSerializer, combinedConfig, m_possibleConfigPathsStorage, m_envVariablesMap, nullptr);
            }
            return combinedConfig;
        }

        dictionary::Item* loadAndMergeCustomConfig(dictionary::Item* combinedConfig,
                                                   const char* filepath,
                                                   dictionary::ISerializer* customSerializer = nullptr)
        {
            m_possibleConfigPathsStorage.clear();
            m_possibleConfigPathsStorage.emplace_back(filepath);

            dictionary::ISerializer* configSerializer = customSerializer ? customSerializer : m_configSerializer;

            return tryLoadAnySettingsAndMergeIntoTarget(
                configSerializer, combinedConfig, m_possibleConfigPathsStorage, m_envVariablesMap, nullptr);
        }

    private:
        void addPossiblePathOverridesForSearch(const std::string& pathStem, const char* extension)
        {
            m_possibleConfigPathsStorage.emplace_back(pathStem + kOverrideSuffix + extension);
            m_possibleConfigPathsStorage.emplace_back(pathStem + extension + kOverrideSuffix);
        }

        dictionary::Item* tryLoadAnySettingsAndMergeIntoTarget(dictionary::ISerializer* configSerializer,
                                                               dictionary::Item* targetDict,
                                                               const std::vector<std::string>& possibleConfigPaths,
                                                               const extras::ConfigLoadHelper::EnvVariablesMap* envVariablesMap,
                                                               std::string* loadedDictPath)
        {
            if (loadedDictPath)
            {
                loadedDictPath->clear();
            }

            dictionary::Item* loadedDict = nullptr;
            for (const auto& curConfigPath : possibleConfigPaths)
            {
                const char* dictFilename = curConfigPath.c_str();
                if (!m_fs->exists(dictFilename))
                {
                    continue;
                }
                loadedDict = dictionary::createDictionaryFromFile(configSerializer, dictFilename);
                if (loadedDict)
                {
                    if (loadedDictPath)
                    {
                        *loadedDictPath = dictFilename;
                    }
                    CARB_LOG_INFO("Found and loaded settings from: %s", dictFilename);
                    break;
                }
                else
                {
                    CARB_LOG_ERROR("Couldn't load the '%s' config data from file '%s'",
                                   m_selectedConfigInfo->configFormatName, dictFilename);
                    break;
                }
            }

            dictionary::IDictionary* dictionaryInterface = m_helper->getDictionaryInterface();
            return extras::ConfigLoadHelper::resolveAndMergeNewDictIntoTarget(
                dictionaryInterface, targetDict, loadedDict, loadedDictPath ? loadedDictPath->c_str() : nullptr,
                envVariablesMap);
        }

        std::vector<std::string> m_possibleConfigPathsStorage;

        filesystem::IFileSystem* m_fs = nullptr;
        dictionary::ISerializer* m_configSerializer = nullptr;
        LoadSettingsHelper* m_helper = nullptr;
        const SupportedConfigInfo* m_selectedConfigInfo = nullptr;
        const extras::ConfigLoadHelper::EnvVariablesMap* m_envVariablesMap = nullptr;
    };

    inline dictionary::ISerializer* acquireOrLoadSerializerFromConfigInfo(const LoadSettingsDesc& params,
                                                                          const SupportedConfigInfo* configInfo)
    {
        dictionary::ISerializer* configSerializer =
            getFramework()->tryAcquireInterface<dictionary::ISerializer>(configInfo->serializerPluginName);

        if (configSerializer)
            return configSerializer;

        return loadConfigSerializerPlugin(params.pluginSearchPaths, params.pluginSearchPathCount, configInfo);
    }

    inline dictionary::Item* readConfigStages(const LoadSettingsDesc& params,
                                              std::string* localSpaceConfigFilepath,
                                              std::string* customConfigFilepath,
                                              std::string* cmdLineConfigFilepath)
    {
        if (!m_configSerializer)
        {
            return nullptr;
        }

        CARB_LOG_INFO("Using '%s' format for config files.", m_selectedConfigInfo->configFormatName);

        dictionary::Item* combinedConfig = nullptr;

        extras::Path userFolder = extras::ConfigLoadHelper::getConfigUserFolder(params.envVariablesMap);

        std::string sharedUserSpaceFilepath;
        std::string appSpecificUserSpaceFilepath;

        ConfigStageLoader configStageLoader(m_fs, m_configSerializer, this, m_selectedConfigInfo, params.envVariablesMap);

        // Base configs
        combinedConfig =
            configStageLoader.loadAndMergeSharedUserSpaceConfig(userFolder, combinedConfig, &sharedUserSpaceFilepath);
        combinedConfig = configStageLoader.loadAndMergeAppSpecificUserSpaceConfig(
            userFolder, params.appName, combinedConfig, &appSpecificUserSpaceFilepath);
        combinedConfig = configStageLoader.loadAndMergeLocalSpaceConfig(
            params.appDir, params.appName, combinedConfig, localSpaceConfigFilepath);

        // Overrides
        combinedConfig =
            configStageLoader.loadAndMergeSharedUserSpaceConfigOverride(combinedConfig, sharedUserSpaceFilepath);
        combinedConfig = configStageLoader.loadAndMergeAppSpecificUserSpaceConfigOverride(
            combinedConfig, appSpecificUserSpaceFilepath);
        combinedConfig =
            configStageLoader.loadAndMergeLocalSpaceConfigOverride(combinedConfig, *localSpaceConfigFilepath);

        tokens::ITokens* tokensInterface = carb::getFramework()->tryAcquireInterface<tokens::ITokens>();

        // Loading text configuration override
        if (params.configStringOrPath)
        {
            std::string configPath;
            if (tokensInterface)
            {
                configPath = tokens::resolveString(tokensInterface, params.configStringOrPath);
            }
            else
            {
                configPath = params.configStringOrPath;
            }

            if (m_fs->exists(configPath.c_str()))
            {
                std::string configExt = extras::Path(configPath).getExtension();
                const SupportedConfigInfo* configInfo = getConfigInfoFromExtension(configExt.c_str());
                dictionary::ISerializer* customSerializer = acquireOrLoadSerializerFromConfigInfo(params, configInfo);

                if (customConfigFilepath)
                    *customConfigFilepath = configPath;
                combinedConfig =
                    configStageLoader.loadAndMergeCustomConfig(combinedConfig, configPath.c_str(), customSerializer);
            }
            else
            {
                dictionary::Item* textConfigurationOverride =
                    m_configSerializer->createDictionaryFromStringBuffer(params.configStringOrPath);

                if (textConfigurationOverride)
                {
                    CARB_LOG_INFO("Loaded text configuration override");
                    combinedConfig = extras::ConfigLoadHelper::resolveAndMergeNewDictIntoTarget(
                        m_idict, combinedConfig, textConfigurationOverride, "text configuration override",
                        params.envVariablesMap);
                }
                else
                {
                    CARB_LOG_ERROR("Couldn't process provided config string as a '%s' config file or config data",
                                   m_selectedConfigInfo->configFormatName);
                }
            }
        }

        // Loading custom file configuration override
        if (params.cmdLineConfigPath)
        {
            std::string configPath;
            if (tokensInterface)
            {
                configPath = tokens::resolveString(tokensInterface, params.cmdLineConfigPath);
            }
            else
            {
                configPath = params.cmdLineConfigPath;
            }

            if (m_fs->exists(configPath.c_str()))
            {
                std::string configExt = extras::Path(configPath).getExtension();
                const SupportedConfigInfo* configInfo = getConfigInfoFromExtension(configExt.c_str());
                dictionary::ISerializer* customSerializer = acquireOrLoadSerializerFromConfigInfo(params, configInfo);

                if (cmdLineConfigFilepath)
                    *cmdLineConfigFilepath = params.cmdLineConfigPath;
                combinedConfig =
                    configStageLoader.loadAndMergeCustomConfig(combinedConfig, configPath.c_str(), customSerializer);
            }
            else
            {
                CARB_LOG_ERROR("The config file '%s' provided via command line doesn't exist", params.cmdLineConfigPath);
            }
        }

        combinedConfig = extras::ConfigLoadHelper::applyPathwiseEnvOverrides(
            m_idict, combinedConfig, params.pathwiseEnvOverridesMap, params.envVariablesMap);
        combinedConfig = extras::ConfigLoadHelper::applyCmdLineOverrides(
            m_idict, combinedConfig, params.cmdLineOptionsMap, params.envVariablesMap);

        return combinedConfig;
    }

    const auto& getSupportedConfigTypes()
    {
        static const std::array<SupportedConfigInfo, 2> kSupportedConfigTypes = {
            { { "toml", "carb.dictionary.serializer-toml.plugin", ".toml" },
              { "json", "carb.dictionary.serializer-json.plugin", ".json" } }
        };
        return kSupportedConfigTypes;
    }

    const SupportedConfigInfo* getConfigInfoFromExtension(const char* configExtension)
    {
        const std::string parmsConfigExt = configExtension;

        for (const auto& curConfigInfo : getSupportedConfigTypes())
        {
            const char* curConfigExtEnd = curConfigInfo.configExt + std::strlen(curConfigInfo.configExt);
            if (std::equal(curConfigInfo.configExt, curConfigExtEnd, parmsConfigExt.begin(), parmsConfigExt.end(),
                           [](char l, char r) { return std::tolower(l) == std::tolower(r); }))
            {
                return &curConfigInfo;
            }
        }

        return nullptr;
    }

    const SupportedConfigInfo* getConfigInfoFromFormatName(const char* configFormat)
    {
        const std::string parmsConfigFormat = configFormat;

        for (const auto& curConfigInfo : getSupportedConfigTypes())
        {
            const char* curConfigFormatEnd = curConfigInfo.configFormatName + std::strlen(curConfigInfo.configFormatName);
            if (std::equal(curConfigInfo.configFormatName, curConfigFormatEnd, parmsConfigFormat.begin(),
                           parmsConfigFormat.end(), [](char l, char r) { return std::tolower(l) == std::tolower(r); }))
            {
                return &curConfigInfo;
            }
        }

        return nullptr;
    }

    void selectConfigType(const char* configFormat)
    {
        m_selectedConfigInfo = getConfigInfoFromFormatName(configFormat);
        if (!m_selectedConfigInfo)
        {
            CARB_LOG_ERROR("Unsupported configuration format: %s. Falling back to %s", configFormat,
                           getSupportedConfigTypes()[0].configFormatName);
            m_selectedConfigInfo = &getSupportedConfigTypes()[0];
        }
    }

    static dictionary::ISerializer* loadConfigSerializerPlugin(const char* const* pluginSearchPaths,
                                                               size_t pluginSearchPathCount,
                                                               const SupportedConfigInfo* configInfo)
    {
        if (!configInfo)
        {
            return nullptr;
        }

        dictionary::ISerializer* configSerializer =
            getFramework()->tryAcquireInterface<dictionary::ISerializer>(configInfo->serializerPluginName);
        if (!configSerializer)
        {
            loadPluginsFromPattern(configInfo->serializerPluginName, pluginSearchPaths, pluginSearchPathCount);
            configSerializer =
                getFramework()->tryAcquireInterface<dictionary::ISerializer>(configInfo->serializerPluginName);
        }
        if (!configSerializer)
        {
            CARB_LOG_ERROR("Couldn't acquire ISerializer interface on startup for parsing '%s' settings.",
                           configInfo->configFormatName);
        }
        return configSerializer;
    }

    void loadSelectedConfigSerializerPlugin(const char* const* pluginSearchPaths, size_t pluginSearchPathCount)
    {
        m_configSerializer = loadConfigSerializerPlugin(pluginSearchPaths, pluginSearchPathCount, m_selectedConfigInfo);
    }

    void fixRawStrings(dictionary::Item* combinedConfig)
    {
        // Fixing the special raw strings
        auto rawStringsFixer = [&](dictionary::Item* item, uint32_t elementData, void* userData) {
            CARB_UNUSED(elementData, userData);

            const char* rawString = getRawStringFromItem(m_idict, item);
            if (!rawString)
            {
                return 0;
            }

            // buffering the value to be implementation-safe
            const std::string value(rawString);
            m_idict->setString(item, value.c_str());
            return 0;
        };

        const auto getChildByIndexMutable = [](dictionary::IDictionary* dict, dictionary::Item* item, size_t index) {
            return dict->getItemChildByIndexMutable(item, index);
        };

        dictionary::walkDictionary(m_idict, dictionary::WalkerMode::eIncludeRoot, combinedConfig, 0, rawStringsFixer,
                                   nullptr, getChildByIndexMutable);
    }

    dictionary::IDictionary* getDictionaryInterface() const
    {
        return m_idict;
    }

    dictionary::ISerializer* getConfigSerializerInterface() const
    {
        return m_configSerializer;
    }

    settings::ISettings* getSettingsInterface() const
    {
        return m_settings;
    }

    dictionary::Item* createEmptyDict(const char* name = "<config>")
    {
        dictionary::Item* item = m_idict->createItem(nullptr, name, dictionary::ItemType::eDictionary);
        if (!item)
        {
            CARB_LOG_ERROR("Couldn't create empty configuration");
        }
        return item;
    };

private:
    filesystem::IFileSystem* m_fs = nullptr;
    dictionary::IDictionary* m_idict = nullptr;
    dictionary::ISerializer* m_configSerializer = nullptr;
    settings::ISettings* m_settings = nullptr;

    const SupportedConfigInfo* m_selectedConfigInfo = nullptr;
};

/**
 * Helper function to initialize the settings and tokens plugins from different configuration sources
 */
inline void loadSettings(const LoadSettingsHelper::LoadSettingsDesc& settingsDesc)
{
    Framework* f = getFramework();

    // Preparing settings parameters
    LoadSettingsHelper::LoadSettingsDesc params = LoadSettingsHelper::LoadSettingsDesc::getDefault();
    params.overwriteWithNonEmptyParams(settingsDesc);

    LoadSettingsHelper loadSettingsHelper;
    loadSettingsHelper.selectConfigType(params.configFormat);
    loadSettingsHelper.loadBaseSettingsPlugins(params.pluginSearchPaths, params.pluginSearchPathCount);

    filesystem::IFileSystem* fs = f->acquireInterface<filesystem::IFileSystem>();
    tokens::ITokens* tokensInterface = f->tryAcquireInterface<tokens::ITokens>();
    // Initializing tokens
    if (tokensInterface)
    {
        const char* kExePathToken = "exe-path";
        const char* kExeFilenameToken = "exe-filename";

        carb::extras::Path exeFullPath = fs->getExecutablePath();

        tokensInterface->setInitialValue(kExePathToken, exeFullPath.getParent().getStringBuffer());
        tokensInterface->setInitialValue(kExeFilenameToken, exeFullPath.getFilename().getStringBuffer());
    }

    settings::ISettings* settings = loadSettingsHelper.getSettingsInterface();

    std::string localSpaceConfigFilepath;
    std::string customConfigFilepath;
    std::string cmdLineConfigFilepath;

    if (settings)
    {
        loadSettingsHelper.loadSelectedConfigSerializerPlugin(params.pluginSearchPaths, params.pluginSearchPathCount);

        dictionary::Item* combinedConfig = nullptr;

        combinedConfig = loadSettingsHelper.readConfigStages(
            params, &localSpaceConfigFilepath, &customConfigFilepath, &cmdLineConfigFilepath);

        if (!combinedConfig)
        {
            dictionary::IDictionary* dictionaryInterface = loadSettingsHelper.getDictionaryInterface();
            CARB_LOG_INFO("Using empty configuration for settings as no other sources created it.");
            combinedConfig = dictionaryInterface->createItem(nullptr, "<settings>", dictionary::ItemType::eDictionary);
        }

        if (!combinedConfig)
        {
            CARB_LOG_ERROR("Couldn't initialize settings because no configuration were created.");
        }
        else
        {
            loadSettingsHelper.fixRawStrings(combinedConfig);

            // Making the settings from the result dictionary
            settings->initializeFromDictionary(combinedConfig);
        }
    }
    else
    {
        CARB_LOG_INFO("Couldn't acquire ISettings interface on startup to load settings.");
    }

    // Initializing tokens
    if (tokensInterface)
    {
        const char* kLocalSpaceConfigPathToken = "local-config-path";
        const char* kLocalSpaceConfigPathTokenStr = "${local-config-path}";
        const char* kCustomConfigPathToken = "custom-config-path";
        const char* kCmdLineConfigPathToken = "cli-config-path";

        if (!localSpaceConfigFilepath.empty())
        {
            tokensInterface->setInitialValue(kLocalSpaceConfigPathToken, localSpaceConfigFilepath.c_str());
        }
        else
        {
            tokensInterface->setInitialValue(kLocalSpaceConfigPathToken, fs->getCurrentDirectoryPath());
        }

        if (!customConfigFilepath.empty())
        {
            tokensInterface->setInitialValue(kCustomConfigPathToken, customConfigFilepath.c_str());
        }
        else
        {
            tokensInterface->setInitialValue(kCustomConfigPathToken, kLocalSpaceConfigPathTokenStr);
        }

        if (!cmdLineConfigFilepath.empty())
        {
            tokensInterface->setInitialValue(kCmdLineConfigPathToken, cmdLineConfigFilepath.c_str());
        }
        else
        {
            tokensInterface->setInitialValue(kCmdLineConfigPathToken, kLocalSpaceConfigPathTokenStr);
        }
    }
    else
    {
        CARB_LOG_INFO("Couldn't acquire tokens interface and initialize default tokens.");
    }
}

#endif // DOXYGEN_SHOULD_SKIP_THIS
} // namespace detail

//! Loads the framework configuration based on a slew of input parameters.
//!
//! First see @ref carb::StartupFrameworkDesc for an idea of the type of data this function accepts.
//!
//! At a high-level this function:
//!
//!  - Determines application path from CLI args and env vars (see @ref carb::extras::getAppPathAndName()).
//!  - Sets application path as filesystem root
//!  - Loads plugins for settings: *carb.settings.plugin*, *carb.dictionary.plugin*, *carb.tokens.plugins* and any
//!    serializer plugin.
//!  - Searches for config file, loads it and applies CLI args overrides.
//!
//! Rather than this function, consider using @ref OMNI_CORE_INIT(), which handles both starting and shutting down the
//! framework for you in your application.
inline void loadFrameworkConfiguration(const StartupFrameworkDesc& params)
{
    Framework* f = getFramework();
    const StartupFrameworkDesc& defaultStartupFrameworkDesc = StartupFrameworkDesc::getDefault();

    const char* cmdLineParamPrefix = params.cmdLineParamPrefix;
    if (!cmdLineParamPrefix)
    {
        cmdLineParamPrefix = defaultStartupFrameworkDesc.cmdLineParamPrefix;
    }

    const char* envVarsParamPrefix = params.envVarsParamPrefix;
    if (!envVarsParamPrefix)
    {
        envVarsParamPrefix = defaultStartupFrameworkDesc.envVarsParamPrefix;
    }

    const char* configFormat = params.configFormat;
    if (!configFormat)
    {
        configFormat = defaultStartupFrameworkDesc.configFormat;
    }

    char** const argv = params.argv;
    const int argc = params.argc;

    extras::CmdLineParser cmdLineParser(cmdLineParamPrefix);
    cmdLineParser.parse(argv, argc);
    const extras::CmdLineParser::Options& args = cmdLineParser.getOptions();

    const char* cmdLineConfigPath = nullptr;
    bool verboseConfiguration = false;
    int32_t startLogLevel = logging::getLogging()->getLevelThreshold();
    if (argv && argc > 0)
    {
        auto findOptionIndex = [=](const char* option) {
            for (int i = 0; i < argc; ++i)
            {
                const char* curArg = argv[i];
                if (curArg && !strcmp(curArg, option))
                {
                    return i;
                }
            }
            return -1;
        };
        auto findOptionValue = [=](const char* option) -> const char* {
            const int optionIndex = findOptionIndex(option);
            if (optionIndex == -1)
            {
                return nullptr;
            }
            if (optionIndex >= argc - 1)
            {
                CARB_LOG_ERROR("Argument not present for the '%s' option", option);
            }
            return argv[optionIndex + 1];
        };

        // Parsing verbose configuration option
        const char* const kVerboseConfigKey = "--verbose-config";
        verboseConfiguration = findOptionIndex(kVerboseConfigKey) != -1;
        if (verboseConfiguration)
        {
            logging::getLogging()->setLevelThreshold(logging::kLevelVerbose);
        }

        // Parsing cmd line for "--config-path" argument
        const char* const kConfigPathKey = "--config-path";
        cmdLineConfigPath = findOptionValue(kConfigPathKey);
        if (cmdLineConfigPath)
        {
            CARB_LOG_INFO("Using '%s' as the value for '%s'", cmdLineConfigPath, kConfigPathKey);
        }

        // Parsing config format from the command line
        const char* kConfigFormatKey = "--config-format";
        const char* const configFormatValue = findOptionValue(kConfigFormatKey);
        if (configFormatValue)
        {
            configFormat = configFormatValue;
        }
    }

    carb::extras::EnvironmentVariableParser envVarsParser(envVarsParamPrefix);
    envVarsParser.parse();

    filesystem::IFileSystem* fs = f->acquireInterface<filesystem::IFileSystem>();

    // Prepare application path and name, which will be used to initialize the IFileSystem default root folder,
    // and also as one of the variants of configuration file name and location.
    std::string appPath, appName;
    extras::getAppPathAndName(args, appPath, appName);
    // If explicitly specified - override this search logic. That means an application doesn't give a control over
    // app path and/or app name through settings and env vars.
    if (params.appNameOverride)
        appName = params.appNameOverride;
    if (params.appPathOverride)
        appPath = params.appPathOverride;
    CARB_LOG_INFO("App path: %s, name: %s", appPath.c_str(), appName.c_str());

    // set the application path for the process.  This will be one of the locations we search for
    // the config file by default.
    fs->setAppDirectoryPath(appPath.c_str());

    // Loading settings from config and command line.
    {
        detail::LoadSettingsHelper::LoadSettingsDesc loadSettingsParams =
            detail::LoadSettingsHelper::LoadSettingsDesc::getDefault();
        loadSettingsParams.appDir = appPath;
        loadSettingsParams.appName = appName;
        loadSettingsParams.configStringOrPath = params.configString;
        loadSettingsParams.cmdLineOptionsMap = &args;
        loadSettingsParams.pathwiseEnvOverridesMap = &envVarsParser.getOptions();
        loadSettingsParams.envVariablesMap = &envVarsParser.getEnvVariables();
        loadSettingsParams.pluginSearchPaths = params.initialPluginsSearchPaths;
        loadSettingsParams.pluginSearchPathCount = params.initialPluginsSearchPathCount;
        loadSettingsParams.cmdLineConfigPath = cmdLineConfigPath;
        loadSettingsParams.configFormat = configFormat;
        detail::loadSettings(loadSettingsParams);
    }

    // restoring the starting log level
    if (verboseConfiguration)
    {
        logging::getLogging()->setLevelThreshold(startLogLevel);
    }
}

//! Configures the framework given a slew of input parameters.
//!
//! First see @ref carb::StartupFrameworkDesc for an idea of the type of data this function accepts.
//!
//! At a high-level this function:
//!
//!  - Configures logging with config file
//!  - Loads plugins according to config file with (see \ref detail::loadPluginsFromConfig())
//!  - Configures default plugins according to config file (see \ref detail::setDefaultPluginsFromConfig())
//!  - Starts the default profiler (if loaded)
//!
//! Rather than this function, consider using @ref OMNI_CORE_INIT(), which handles both starting and shutting down the
//! framework for you in your application.
inline void configureFramework(const StartupFrameworkDesc& params)
{
    Framework* f = getFramework();

    if (!params.disableCrashReporter)
    {
        // Startup the crash reporter
        loadPluginsFromPattern(
            "carb.crashreporter-*", params.initialPluginsSearchPaths, params.initialPluginsSearchPathCount);
        crashreporter::registerCrashReporterForClient();
    }

    auto settings = f->tryAcquireInterface<carb::settings::ISettings>();
    // Configure logging plugin and its default logger
    logging::configureLogging(settings);
    logging::configureDefaultLogger(settings);
    omni::structuredlog::configureStructuredLogging(settings);

    // Uploading leftover dumps asynchronously
    if (settings != nullptr)
    {
        if (!params.disableCrashReporter)
        {
            const char* const kStarupDumpsUploadKey = "/app/uploadDumpsOnStartup";
            settings->setDefaultBool(kStarupDumpsUploadKey, true);
            if (settings->getAsBool(kStarupDumpsUploadKey))
            {
                crashreporter::sendAndRemoveLeftOverDumpsAsync();
            }
        }

        // specify the plugin search paths in settings so that loadPluginsFromConfig()
        // will have the search paths to look through
        const char* kPluginSearchPathsKey = "/pluginSearchPaths";

        // only set this if nothing else has been manually set
        settings->setDefaultStringArray(
            kPluginSearchPathsKey, params.initialPluginsSearchPaths, params.initialPluginsSearchPathCount);
    }

    // Load plugins using supplied configuration

    detail::loadPluginsFromConfig(settings);

    // Configure default plugins as present in the config
    detail::setDefaultPluginsFromConfig(settings);

#if !CARB_PLATFORM_MACOS // CC-669: avoid registering this on Mac OS since it's unimplemented
    // Starting up profiling
    // This way of registering profiler allows to enable/disable profiling in the config file, by
    // allowing/denying to load profiler plugin.
    carb::profiler::registerProfilerForClient();
    CARB_PROFILE_STARTUP();
#endif

    carb::l10n::registerLocalizationForClient();
}

//! Starts/Configures the framework given a slew of input parameters.
//!
//! First see @ref carb::StartupFrameworkDesc for an idea of the type of data this function accepts.
//!
//! At a high-level this function:
//!
//!  - Calls \ref loadFrameworkConfiguration(), which:
//!  - Determines application path from CLI args and env vars (see @ref carb::extras::getAppPathAndName()).
//!  - Sets application path as filesystem root
//!  - Loads plugins for settings: *carb.settings.plugin*, *carb.dictionary.plugin*, *carb.tokens.plugins* and any
//!    serializer plugin.
//!  - Searches for config file, loads it and applies CLI args overrides.
//!  - Calls \ref configureFramework(), which:
//!  - Configures logging with config file
//!  - Loads plugins according to config file
//!  - Configures default plugins according to config file
//!  - Starts the default profiler (if loaded)
//!
//! Rather than this function, consider using @ref OMNI_CORE_INIT(), which handles both starting and shutting down the
//! framework for you in your application.
inline void startupFramework(const StartupFrameworkDesc& params)
{
    loadFrameworkConfiguration(params);
    configureFramework(params);
}

//! Tears down the Carbonite framework.
//!
//! At a high level, this function:
//!  - Shuts down the profiler system (if running)
//!  - Calls \ref profiler::deregisterProfilerForClient(), \ref crashreporter::deregisterCrashReporterForClient(), and
//!    l10n::deregisterLocalizationForClient().
//!
//! \note It is not necessary to manually call this function if \ref OMNI_CORE_INIT is used, since that macro will
//! ensure that the Framework is released and shut down.
inline void shutdownFramework()
{
    CARB_PROFILE_SHUTDOWN();

    profiler::deregisterProfilerForClient();
    crashreporter::deregisterCrashReporterForClient();
    carb::l10n::deregisterLocalizationForClient();
}

} // namespace carb
