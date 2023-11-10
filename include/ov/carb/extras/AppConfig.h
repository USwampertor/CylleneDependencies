// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../dictionary/DictionaryUtils.h"
#include "EnvironmentVariableUtils.h"
#include "StringUtils.h"
#include "Path.h"

#include <map>
#include <string>


namespace carb
{
namespace extras
{

class ConfigLoadHelper
{
public:
    using CmdLineOptionsMap = std::map<std::string, std::string>;
    using PathwiseEnvOverridesMap = std::map<std::string, std::string>;
    using EnvVariablesMap = std::map<std::string, std::string>;

    // If the dict item is a special raw string then it returns pointer to the buffer past the special raw string marker
    // In all other cases it returns nullptr
    static inline const char* getRawStringFromItem(carb::dictionary::IDictionary* dictInterface,
                                                   const carb::dictionary::Item* item)
    {
        static constexpr char kSpecialRawStringMarker[] = "$raw:";

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

        constexpr size_t kMarkerLen = carb::countOf(kSpecialRawStringMarker) - 1;

        if (std::strncmp(stringBuffer, kSpecialRawStringMarker, kMarkerLen) != 0)
        {
            return nullptr;
        }

        return stringBuffer + kMarkerLen;
    }

    /**
     * Helper function to resolve references to environment variables and Elvis operators
     */
    static inline void resolveEnvVarReferencesInDict(dictionary::IDictionary* dictInterface,
                                                     dictionary::Item* dict,
                                                     const EnvVariablesMap* envVariables)
    {
        if (!dict || !envVariables)
        {
            return;
        }

        const auto itemResolver = [&](dictionary::Item* item, uint32_t elementData, void* userData) {
            CARB_UNUSED(elementData, userData);
            // Working only on string items
            if (dictInterface->getItemType(item) != dictionary::ItemType::eString)
            {
                return 0;
            }

            // Skipping raw strings
            if (getRawStringFromItem(dictInterface, item) != nullptr)
            {
                return 0;
            }

            const char* stringBuffer = dictInterface->getStringBuffer(item);
            const size_t stringBufferLen = std::strlen(stringBuffer);

            // We have an empty string, no reason to resolve it
            if (stringBufferLen == 0)
            {
                return 0;
            }

            std::pair<std::string, bool> resolveResult =
                extras::resolveEnvVarReferences(stringBuffer, stringBufferLen, *envVariables);

            if (!resolveResult.second)
            {
                CARB_LOG_ERROR("Error while resolving environment variable references for '%s'", stringBuffer);
                return 0;
            }

            const std::string& resolvedString = resolveResult.first;

            if (!resolvedString.empty())
            {
                dictInterface->setString(item, resolvedString.c_str());
            }
            else
            {
                dictInterface->destroyItem(item);
            }

            return 0;
        };

        const auto getChildByIndexMutable = [](dictionary::IDictionary* dict, dictionary::Item* item, size_t index) {
            return dict->getItemChildByIndexMutable(item, index);
        };

        dictionary::walkDictionary(dictInterface, dictionary::WalkerMode::eIncludeRoot, dict, 0, itemResolver, nullptr,
                                   getChildByIndexMutable);
    }

    using GetItemFullPathFuncPtr = std::string (*)(dictionary::IDictionary* dictInterface, const dictionary::Item* item);

    struct UpdaterData
    {
        dictionary::IDictionary* dictInterface;
        const char* loadedDictPath;
        GetItemFullPathFuncPtr getItemFullPathFunc;
    };

    static dictionary::UpdateAction onDictUpdateReporting(const dictionary::Item* dstItem,
                                                          dictionary::ItemType dstItemType,
                                                          const dictionary::Item* srcItem,
                                                          dictionary::ItemType srcItemType,
                                                          void* userData)
    {
        CARB_UNUSED(dstItemType, srcItem, srcItemType);
        const UpdaterData* updaterData = static_cast<const UpdaterData*>(userData);

        if (updaterData->getItemFullPathFunc)
        {
            std::string itemPath = updaterData->getItemFullPathFunc(updaterData->dictInterface, dstItem);
            CARB_LOG_VERBOSE("Replacing the '%s' item current value by the value from '%s' config.", itemPath.c_str(),
                             updaterData->loadedDictPath);
        }

        if (!dstItem)
        {
            return dictionary::UpdateAction::eOverwrite;
        }

        if (updaterData->dictInterface->getItemFlag(srcItem, dictionary::ItemFlag::eUnitSubtree))
        {
            return dictionary::UpdateAction::eReplaceSubtree;
        }

        return dictionary::UpdateAction::eOverwrite;
    };

    static inline GetItemFullPathFuncPtr getFullPathFunc()
    {
        return logging::getLogging()->getLevelThreshold() == logging::kLevelVerbose ? dictionary::getItemFullPath :
                                                                                      nullptr;
    }

    static dictionary::Item* resolveAndMergeNewDictIntoTarget(dictionary::IDictionary* dictInterface,
                                                              dictionary::Item* targetDict,
                                                              dictionary::Item* newDict,
                                                              const char* newDictSource,
                                                              const EnvVariablesMap* envVariablesMap)
    {
        if (!newDict)
        {
            return targetDict;
        }

        extras::ConfigLoadHelper::resolveEnvVarReferencesInDict(dictInterface, newDict, envVariablesMap);

        if (!targetDict)
        {
            return newDict;
        }

        UpdaterData updaterData = { dictInterface, newDictSource ? newDictSource : "Unspecified source",
                                    getFullPathFunc() };
        dictInterface->update(targetDict, nullptr, newDict, nullptr, onDictUpdateReporting, &updaterData);

        dictInterface->destroyItem(newDict);

        return targetDict;
    };

    /** Retrieve the standardized user config folder for a given system.
     *  @param[in] envVariablesMap     The environment variables to query for the
     *                                 config folder path.
     *                                 This may not be nullptr.
     *  @param[in] configSubFolderName The folder name within the config directory
     *                                 to append to the path.
     *                                 This may be nullptr to have the returned
     *                                 path be the config directory.
     *  @returns The path to the config directory that should be used.
     *  @returns An empty path if the required environment variables were not
     *           set. Note that this can't happen on Linux unless a user has
     *           explicitly broken their env vars.
     */
    static extras::Path getConfigUserFolder(const EnvVariablesMap* envVariablesMap,
                                            const char* configSubFolderName = "nvidia")
    {
#if CARB_PLATFORM_WINDOWS
        const char* kUserFolderEnvVar = "USERPROFILE";
#elif CARB_PLATFORM_LINUX
        const char* kUserFolderEnvVar = "XDG_CONFIG_HOME";
#elif CARB_PLATFORM_MACOS
        const char* kUserFolderEnvVar = "HOME";
#endif
        extras::Path userFolder;

        if (envVariablesMap)
        {
            // Resolving user folder
            auto searchResult = envVariablesMap->find(kUserFolderEnvVar);
            if (searchResult != envVariablesMap->end())
                userFolder = searchResult->second;

#if CARB_PLATFORM_LINUX
            // The XDG standard says that if XDG_CONFIG_HOME is undefined,
            // $HOME/.config is used
            if (userFolder.isEmpty())
            {
                auto searchResult = envVariablesMap->find("HOME");
                if (searchResult != envVariablesMap->end())
                {
                    userFolder = searchResult->second;
                    userFolder /= ".config";
                }
            }
#elif CARB_PLATFORM_MACOS
            if (!userFolder.isEmpty())
            {
                userFolder /= "Library/Application Support";
            }
#endif

            if (!userFolder.isEmpty() && configSubFolderName != nullptr)
                userFolder /= configSubFolderName;
        }

        return userFolder;
    }

    static dictionary::Item* applyPathwiseEnvOverrides(dictionary::IDictionary* dictionaryInterface,
                                                       dictionary::Item* combinedConfig,
                                                       const PathwiseEnvOverridesMap* pathwiseEnvOverridesMap,
                                                       const EnvVariablesMap* envVariablesMap)
    {
        if (pathwiseEnvOverridesMap)
        {
            dictionary::Item* pathwiseConfig = dictionaryInterface->createItem(
                nullptr, "<pathwise env override config>", dictionary::ItemType::eDictionary);
            if (pathwiseConfig)
            {
                dictionary::setDictionaryFromStringMapping(dictionaryInterface, pathwiseConfig, *pathwiseEnvOverridesMap);
                return extras::ConfigLoadHelper::resolveAndMergeNewDictIntoTarget(
                    dictionaryInterface, combinedConfig, pathwiseConfig, "environment variables override",
                    envVariablesMap);
            }

            CARB_LOG_ERROR("Couldn't process environment variables overrides");
        }
        return combinedConfig;
    }

    /**
     * Adds to the "targetDictionary" element at path "elementPath" as dictionary created from parsing elementValue as
     * JSON data
     *
     * @param [in] jsonSerializerInterface non-null pointer to the JSON ISerializer interface
     * @param [in] dictionaryInterface non-null pointer to the IDictionary interface
     * @param [in,out] targetDictionary dictionary into which element will be added
     * @param [in] elementPath path to the element in the dictionary
     * @param [in] elementValue JSON data
     */
    static void addCmdLineJSONElementToDict(dictionary::ISerializer* jsonSerializerInterface,
                                            dictionary::IDictionary* dictionaryInterface,
                                            dictionary::Item* targetDictionary,
                                            const std::string& elementPath,
                                            const std::string& elementValue)
    {
        if (elementPath.empty())
        {
            return;
        }

        CARB_ASSERT(elementValue.size() >= 2 && elementValue.front() == '{' && elementValue.back() == '}');

        // Parse provided JSON data
        dictionary::Item* parsedDictionary =
            jsonSerializerInterface->createDictionaryFromStringBuffer(elementValue.c_str(), elementValue.length());
        if (!parsedDictionary)
        {
            CARB_LOG_ERROR("Couldn't parse as JSON data command-line argument '%s'", elementPath.c_str());
            return;
        }

        // Merge result into target dictionary
        dictionaryInterface->update(targetDictionary, elementPath.c_str(), parsedDictionary, nullptr,
                                    dictionary::overwriteOriginalWithArrayHandling, dictionaryInterface);

        // Release allocated resources
        dictionaryInterface->destroyItem(parsedDictionary);
    }

    /**
     * Merges values set in the command line into provided configuration dictionary
     *
     * @param [in] dictionaryInterface non-null pointer to the IDictionary interface
     * @param [in,out] combinedConfig configuration dictionary into which parameters set in the command line will be
     * merged
     * @param [in] cmdLineOptionsMap map of parameters set in the command line
     * @param [in] envVariablesMap map of the parameters set in the environment variables. Used for resolving references
     * in the values of cmdLineOptionsMap
     *
     * @return "combinedConfig" with merged parameters set in the command line
     */
    static dictionary::Item* applyCmdLineOverrides(dictionary::IDictionary* dictionaryInterface,
                                                   dictionary::Item* combinedConfig,
                                                   const CmdLineOptionsMap* cmdLineOptionsMap,
                                                   const EnvVariablesMap* envVariablesMap)
    {
        if (cmdLineOptionsMap)
        {
            dictionary::Item* cmdLineOptionsConfig = dictionaryInterface->createItem(
                nullptr, "<cmd line override config>", dictionary::ItemType::eDictionary);
            if (cmdLineOptionsConfig)
            {
                dictionary::ISerializer* jsonSerializer = nullptr;
                bool checkedJsonSerializer = false;

                for (const auto& kv : *cmdLineOptionsMap)
                {
                    const std::string& trimmedValue = carb::extras::trimString(kv.second);

                    // First checking if there is enough symbols to check for array/JSON parameter
                    if (trimmedValue.size() >= 2)
                    {
                        // Check if value is "[ something ]" then we consider it to be an array
                        if (trimmedValue.front() == '[' && trimmedValue.back() == ']')
                        {
                            // Processing the array value
                            setDictionaryArrayElementFromStringValue(
                                dictionaryInterface, cmdLineOptionsConfig, kv.first, trimmedValue);
                            continue;
                        }
                        // Check if value is "{ something }" then we consider it to be JSON data
                        else if (trimmedValue.front() == '{' && trimmedValue.back() == '}')
                        {
                            // First check if the JsonSerializer was created already
                            if (!checkedJsonSerializer)
                            {
                                checkedJsonSerializer = true;
                                jsonSerializer = getFramework()->tryAcquireInterface<dictionary::ISerializer>(
                                    "carb.dictionary.serializer-json.plugin");
                                if (!jsonSerializer)
                                {
                                    CARB_LOG_ERROR(
                                        "Couldn't acquire JSON serializer for processing command line arguments");
                                }
                            }
                            if (jsonSerializer)
                            {
                                // Processing the JSON value
                                addCmdLineJSONElementToDict(
                                    jsonSerializer, dictionaryInterface, cmdLineOptionsConfig, kv.first, trimmedValue);
                            }
                            else
                            {
                                CARB_LOG_ERROR("No JSON serializer acquired. Cannot process command line parameter '%s'",
                                               kv.first.c_str());
                            }
                            continue;
                        }
                    }

                    // Processing as string representation of a dictionary value
                    dictionary::setDictionaryElementAutoType(
                        dictionaryInterface, cmdLineOptionsConfig, kv.first, kv.second);
                }

                return extras::ConfigLoadHelper::resolveAndMergeNewDictIntoTarget(
                    dictionaryInterface, combinedConfig, cmdLineOptionsConfig, "command line override", envVariablesMap);
            }
            else
            {
                CARB_LOG_ERROR("Couldn't process command line overrides");
            }
        }
        return combinedConfig;
    }
};

} // namespace extras
} // namespace carb
