// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include <carb/dictionary/IDictionary.h>
#include <carb/logging/Log.h>
#include <carb/settings/ISettings.h>
#include <carb/settings/SettingsUtils.h>
#include <carb/tasking/TaskingUtils.h>
#include <omni/extras/StringHelpers.h>
#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>
#include "md5.h"

namespace rtx
{

/*

Usage:

// init
globalOverrides = Settings::createContext();
vp0Context = Settings::createContext();

// update loop
{
    // Frame begin
    // clone global settings
    Settings::cloneToContext(vp0Context, nullptr);
    Settings::getIDictionary()->makeIntAtPath(vp0Context, "path/to/int", 10);
    // ...
    Settings::applyOverridesToContext(vp0Context, globalOverrides);

    // ...

    // will have all the overrides
    Settings::getIDictionary()->getAsInt(vp0Context, "path/to/int");
}

*/

static constexpr char kDefaultSettingsPath[] = "/rtx-defaults";
static constexpr char kSettingsPath[] = "/rtx";

static constexpr char kDefaultPersistentSettingsPath[] = "/persistent-defaults";
static constexpr char kPersistentSettingsPath[] = "/persistent";

static constexpr char kTransientSettingsPath[] = "/rtx-transient";
static constexpr char kFlagsSettingsPath[] = "/rtx-flags";

// A note on internal setting handling.
// Settings in /rtx-transient/internal and /rtx/internal are automatically mapped to entries in /rtx-transient/hashed
// and /rtx/hashed, respectively. We use the MD5 hash of the full internal setting string as the key inside the "hashed"
// scope. See the comments in InternalSettings.h for more details.
static constexpr char kInternalSettingKey[] = "internal";
static constexpr char kHashedSettingKey[] = "hashed";
static constexpr const char* kInternalSettingRoots[] = {
    kSettingsPath,
    kTransientSettingsPath,
};

typedef uint32_t SettingFlags;
constexpr SettingFlags kSettingFlagNone = 0;
constexpr SettingFlags kSettingFlagTransient = 1 << 0; ///< Do not read or store in USD. Transient settings
                                                       ///< automatically disables reset feature. Note that settings
                                                       ///< under /rtx-transient are transient irrespective of this
                                                       ///< setting
constexpr SettingFlags kSettingFlagResetDisabled = 1 << 1; ///< Do not allow reset of the setting under /rtx-defaults/
constexpr SettingFlags kSettingFlagDefault = kSettingFlagNone;

// Worst case hashed setting root path length.
static constexpr size_t kHashedSettingPrefixMaxSize =
    CARB_MAX(carb::countOf(kSettingsPath), carb::countOf(kTransientSettingsPath)) + 1 + carb::countOf(kHashedSettingKey);
// Worst case hashed setting string length.
static constexpr size_t kHashedSettingCStringMaxLength = kHashedSettingPrefixMaxSize + MD5::kDigestStringSize + 1;

class RenderSettings
{
protected:
public:
    // Statically-sized container for hashed setting strings.
    // We use MD5 hashes which have constant size, so we can compute the worst case hashed setting string size and pass
    // them around on the stack instead of on the heap. In the future this may also help with hashing at build time.
    struct HashedSettingCString
    {
        char data[kHashedSettingCStringMaxLength];
    };

    static bool isPathRenderSettingsRoot(const char* path)
    {
        size_t rtxSettingsPathLen = strlen(kSettingsPath);
        size_t pathLen = strlen(path);
        if (pathLen < rtxSettingsPathLen)
        {
            CARB_LOG_ERROR("Could not find path %s", path);
            return "";
        }

        if (pathLen == rtxSettingsPathLen)
        {
            if (strcmp(path, kSettingsPath) == 0)
            {
                return true;
            }
        }

        return false;
    }

    static std::string getAssociatedDefaultsPath(const char* cPath)
    {
        std::string path(cPath);

        size_t pos = path.find_first_of("/", 1);

        std::string parentPath;
        std::string childPath;

        if (pos == std::string::npos)
        {
            parentPath = path;
        }
        else
        {
            parentPath = path.substr(0, pos);
            childPath = path.substr(pos);
        }

        return parentPath + "-defaults" + childPath;
    }

    template <typename KeyType, typename ValueType>
    static void stringToKeyTypeToValueTypeDictionary(const std::string& keyToValueDictStr,
                                                     std::unordered_map<KeyType, ValueType>& outputDict,
                                                     const char* debugStr)
    {
        // Convert from string to a dictionary mapping KeyType values to ValueType values
        //
        // Example syntax: 0:0; 1:1,2; 2:3,4; 3:5; 4:6; 5:6; 6:6; 7;6"
        // Example syntax: "AsphaltStandard:0; AsphaltWeathered:1; ConcreteRough:2"
        // I.e.: use semicolon (';') to separate dictionary entries from each other,
        //       use colon (':') to separate key from value and
        //       use comma (',') to separate entries in a list of values
        std::stringstream ss(keyToValueDictStr);
        static const char kMappingSep = ';';
        static const char kKeyValueSep = ':';

        // Split by ';' into key:value mappings
        std::vector<std::string> keyToValueStrVec = omni::extras::split(keyToValueDictStr, kMappingSep);
        for (auto& keyToValue : keyToValueStrVec)
        {
            // Split a mapping into its key and value
            std::vector<std::string> keyToValueStrVec = omni::extras::split(keyToValue, kKeyValueSep);

            if (keyToValueStrVec.size() == 2) // this is a key,value pair
            {
                // find an appropriate overloaded function to add a key:value pair to the dictionary
                addValueAtKey(keyToValueStrVec[0], keyToValueStrVec[1], outputDict, debugStr);
            }
        }
    }

    static inline bool hasSettingFlags(carb::settings::ISettings& settings, const char* settingPath)
    {
        std::string settingPathStr(kFlagsSettingsPath);
        settingPathStr += settingPath;
        return settings.getItemType(settingPathStr.c_str()) != carb::dictionary::ItemType::eCount;
    }

    static inline SettingFlags getSettingFlags(carb::settings::ISettings& settings, const char* settingPath)
    {
        std::string settingPathStr(kFlagsSettingsPath);
        settingPathStr += settingPath;
        return SettingFlags(settings.getAsInt(settingPathStr.c_str()));
    }

    static inline void setSettingFlags(carb::settings::ISettings& settings, const char* settingPath, SettingFlags flags)
    {
        std::string settingPathStr(kFlagsSettingsPath);
        settingPathStr += settingPath;
        settings.setInt64(settingPathStr.c_str(), flags);
    }

    static inline void removeSettingFlags(carb::settings::ISettings& settings, const char* settingPath)
    {
        std::string settingPathStr(kFlagsSettingsPath);
        settingPathStr += settingPath;
        settings.destroyItem(settingPathStr.c_str());
    }

    template <typename SettingType>
    static void setAndBackupDefaultSetting(const char* settingPath,
                                           SettingType value,
                                           SettingFlags flags = kSettingFlagDefault)
    {
        carb::settings::ISettings* settings = getISettings();
        settings->setDefault<SettingType>(settingPath, value);

        bool allowReset = (flags & (kSettingFlagTransient | kSettingFlagResetDisabled)) == 0;
        if (allowReset)
        {
            std::string defaultSettingPath = getAssociatedDefaultsPath(settingPath);
            if (!defaultSettingPath.empty())
            {
                std::string path(defaultSettingPath);
                if (path.substr(0, std::strlen(kPersistentSettingsPath)) == kPersistentSettingsPath)
                {
                    settings->set<SettingType>(defaultSettingPath.c_str(), value);
                }
                else
                {
                    settings->setDefault<SettingType>(
                        defaultSettingPath.c_str(), settings->get<SettingType>(settingPath));
                }
            }
        }
        // Set per setting flag (persistent, etc.)
        setSettingFlags(*settings, settingPath, flags);
    }

    static inline std::string getInternalSettingString(const char* s)
    {
        return std::string(getHashedSettingString(s).data);
    }

    static void updateSetting(const carb::dictionary::Item* changedItem,
                              carb::dictionary::ChangeEventType eventType,
                              void* userData)
    {
        CARB_UNUSED(eventType);
        if (!changedItem)
        {
            CARB_LOG_ERROR("Unexpected setting deletion");
        }

        const char* path = reinterpret_cast<const char*>(userData);
        carb::dictionary::IDictionary* dictionary = getIDictionary();
        carb::settings::ISettings* settings = getISettings();

        carb::dictionary::ItemType itemType = dictionary->getItemType(changedItem);
        if (itemType == carb::dictionary::ItemType::eString)
        {
            settings->setString(path, dictionary->getStringBuffer(changedItem));
        }
        else if (itemType == carb::dictionary::ItemType::eFloat)
        {
            settings->setFloat(path, dictionary->getAsFloat(changedItem));
        }
        else if (itemType == carb::dictionary::ItemType::eInt)
        {
            settings->setInt(path, dictionary->getAsInt(changedItem));
        }
        else if (itemType == carb::dictionary::ItemType::eBool)
        {
            settings->setBool(path, dictionary->getAsBool(changedItem));
        }
    }

    template <typename SettingType>
    static void setAndBackupPersistentDefaultSetting(const char* settingPath, SettingType value)
    {
        carb::settings::ISettings* settings = getISettings();

        std::string persistentPath = std::string("/persistent") + settingPath;
        settings->setDefault<SettingType>(persistentPath.c_str(), value);

        std::string defaultSettingPath = getAssociatedDefaultsPath(persistentPath.c_str());
        if (!defaultSettingPath.empty())
        {
            std::string path(defaultSettingPath);
            if (path.substr(0, std::strlen(kPersistentSettingsPath)) == kPersistentSettingsPath)
            {
                settings->set<SettingType>(defaultSettingPath.c_str(), value);
            }
            else
            {
                settings->set<SettingType>(
                    defaultSettingPath.c_str(), settings->get<SettingType>(persistentPath.c_str()));
            }
        }

        settings->subscribeToNodeChangeEvents(
            persistentPath.c_str(), RenderSettings::updateSetting, (void*)(settingPath));

        settings->set<SettingType>(settingPath, settings->get<SettingType>(persistentPath.c_str()));
    }


    template <typename SettingArrayType>
    static void setAndBackupDefaultSettingArray(const char* settingPath,
                                                const SettingArrayType* array,
                                                size_t arrayLength,
                                                SettingFlags flags = kSettingFlagDefault)
    {
        carb::settings::ISettings* settings = getISettings();
        settings->setDefaultArray<SettingArrayType>(settingPath, array, arrayLength);

        bool allowReset = (flags & (kSettingFlagTransient | kSettingFlagResetDisabled)) == 0;
        if (allowReset)
        {
            std::string defaultSettingPath = getAssociatedDefaultsPath(settingPath);
            if (!defaultSettingPath.empty())
            {
                settings->destroyItem(defaultSettingPath.c_str());
                size_t arrayLength = settings->getArrayLength(settingPath);
                for (size_t idx = 0; idx < arrayLength; ++idx)
                {
                    std::string elementPath = std::string(settingPath) + "/" + std::to_string(idx);
                    std::string defaultElementPath = std::string(defaultSettingPath) + "/" + std::to_string(idx);
                    settings->setDefault<SettingArrayType>(
                        defaultElementPath.c_str(), settings->get<SettingArrayType>(elementPath.c_str()));
                }
            }
        }
        // Set per setting flag (persistent, etc.)
        setSettingFlags(*settings, settingPath, flags);
    }

    static void resetSettingToDefault(const char* path)
    {
        carb::dictionary::IDictionary* dictionary = getIDictionary();
        carb::settings::ISettings* settings = getISettings();

        std::string defaultsPathStorage;
        defaultsPathStorage = getAssociatedDefaultsPath(path);

        const carb::dictionary::Item* srcItem = settings->getSettingsDictionary(defaultsPathStorage.c_str());
        carb::dictionary::ItemType srcItemType = dictionary->getItemType(srcItem);
        size_t srcItemArrayLength = dictionary->getArrayLength(srcItem);

        if ((srcItemType == carb::dictionary::ItemType::eDictionary) && (srcItemArrayLength == 0))
        {
            resetSectionToDefault(path);
            return;
        }

        switch (srcItemType)
        {
            case carb::dictionary::ItemType::eBool:
            {
                settings->set<bool>(path, dictionary->getAsBool(srcItem));
                break;
            }
            case carb::dictionary::ItemType::eInt:
            {
                settings->set<int32_t>(path, dictionary->getAsInt(srcItem));
                break;
            }
            case carb::dictionary::ItemType::eFloat:
            {
                settings->set<float>(path, dictionary->getAsFloat(srcItem));
                break;
            }
            case carb::dictionary::ItemType::eString:
            {
                settings->set<const char*>(path, dictionary->getStringBuffer(srcItem));
                break;
            }
            case carb::dictionary::ItemType::eDictionary:
            {
                if (srcItemArrayLength > 0)
                {
                    settings->update(path, srcItem, nullptr, carb::dictionary::kUpdateItemOverwriteOriginal, nullptr);
                }
                break;
            }
            default:
                break;
        }
    }

    static void resetSectionToDefault(const char* path)
    {
        carb::settings::ISettings* settings = getISettings();

        std::string defaultsPathStorage;
        const char* defaultsPath = nullptr;

        defaultsPathStorage = getAssociatedDefaultsPath(path);
        if (!defaultsPathStorage.empty())
        {
            defaultsPath = defaultsPathStorage.c_str();
        }

        if (defaultsPath)
        {
            // Do not delete existing original settings.
            // If the source item exists (rtx-default value), overwrite the destination (original rtx value).
            // Otherwise, leave them as-is. We want to keep original values until we find some default values in the
            // future. Plugins may load at a later time and we should not remove original values until we have some
            // default values.
            settings->update(path, settings->getSettingsDictionary(defaultsPath), nullptr,
                             carb::dictionary::kUpdateItemOverwriteOriginal, nullptr);
        }
        else
        {
            CARB_LOG_ERROR("%s: failed to resolve default paths", __func__);
        }
    }

    static carb::dictionary::Item* createContext()
    {
        return getIDictionary()->createItem(nullptr, "<rtx context>", carb::dictionary::ItemType::eDictionary);
    }
    static void destroyContext(carb::dictionary::Item* ctx)
    {
        getIDictionary()->destroyItem(ctx);
    }

    static void cloneToContext(carb::dictionary::Item* dstContext, carb::dictionary::Item* srcContext)
    {
        carb::dictionary::IDictionary* dict = getIDictionary();
        carb::settings::ISettings* settings = getISettings();

        dict->destroyItem(dstContext);
        if (srcContext)
        {
            dict->update(dstContext, "", srcContext, "", carb::dictionary::kUpdateItemOverwriteOriginal, nullptr);
        }
        else
        {
            dstContext = settings->createDictionaryFromSettings(kSettingsPath);
        }
    }

    static void applyOverridesToContext(carb::dictionary::Item* dstContext, carb::dictionary::Item* overridesContext)
    {
        if (!overridesContext)
        {
            CARB_LOG_ERROR("%s needs context to override from", __FUNCTION__);
        }
        carb::dictionary::IDictionary* dict = getIDictionary();

        dict->update(dstContext, "", overridesContext, "", carb::dictionary::kUpdateItemOverwriteOriginal, nullptr);
    }

    static carb::dictionary::IDictionary* getIDictionary()
    {
        return carb::getCachedInterface<carb::dictionary::IDictionary>();
    }

    static carb::settings::ISettings* getISettings()
    {
        return carb::getCachedInterface<carb::settings::ISettings>();
    }

    // Translates cleartext internal settings to corresponding hashed settings and removes the cleartext settings. The
    // latter ensures cleartext internal settings are not inadvertently saved to USD.
    static void hashInternalRenderSettings()
    {
        auto settings = getISettings();
        auto hashRtxSetting = [&settings](const char* itemPath, uint32_t, void*) -> uint32_t {
            const carb::dictionary::ItemType itemType = settings->getItemType(itemPath);

            if (itemType == carb::dictionary::ItemType::eDictionary)
            {
                return 0;
            }

            const auto newHashedPath = getHashedSettingString(itemPath);

            switch (settings->getItemType(itemPath))
            {
                case carb::dictionary::ItemType::eBool:
                    settings->setBool(newHashedPath.data, settings->getAsBool(itemPath));
                    break;
                case carb::dictionary::ItemType::eFloat:
                    settings->setFloat(newHashedPath.data, settings->getAsFloat(itemPath));
                    break;
                case carb::dictionary::ItemType::eInt:
                    settings->setInt(newHashedPath.data, settings->getAsInt(itemPath));
                    break;
                case carb::dictionary::ItemType::eString:
                    settings->setString(newHashedPath.data, settings->getStringBuffer(itemPath));
                    break;
                case carb::dictionary::ItemType::eDictionary:
                    CARB_FALLTHROUGH;
                default:
                    CARB_ASSERT(0);
            }

            if (hasSettingFlags(*settings, itemPath))
            {
                setSettingFlags(*settings, newHashedPath.data, getSettingFlags(*settings, itemPath));
                removeSettingFlags(*settings, itemPath);
            }

            return 0;
        };

        carb::dictionary::IDictionary* dictionary = getIDictionary();

        for (const char* root : kInternalSettingRoots)
        {
            std::stringstream ss;
            ss << root << '/' << kInternalSettingKey;
            const std::string internalRoot = ss.str();

            carb::settings::walkSettings(dictionary, settings, carb::dictionary::WalkerMode::eSkipRoot,
                                         internalRoot.c_str(), 0, hashRtxSetting, nullptr);
            settings->destroyItem(internalRoot.c_str());
        }
    }

private:
    // Key : Value = uint32_t : std::vector<uint32_t>
    static void addValueAtKey(std::string key,
                              std::string value,
                              std::unordered_map<uint32_t, std::vector<uint32_t>>& outputDict,
                              const char* debugStr)
    {
        static const char kListElemSep = ',';
        int intKey = 0;
        if (!omni::extras::stringToInteger(key, intKey))
        {
            CARB_LOG_WARN("Non-integer value %s set in \"%s\"", key.c_str(), debugStr);
        }

        // Split the value into a list of values
        std::vector<std::string> intVecStrVec = omni::extras::split(value, kListElemSep);

        int32_t intVecValueEntry = 0;
        outputDict[intKey].clear();
        for (auto& intVecValueEntryStr : intVecStrVec)
        {
            if (omni::extras::stringToInteger(intVecValueEntryStr, intVecValueEntry))
            {
                outputDict[intKey].push_back(intVecValueEntry);
            }
            else
            {
                CARB_LOG_WARN("Non-integer value %s set in \"%s\"", intVecValueEntryStr.c_str(), debugStr);
            }
        }
    }
    // Key : Value = std::string : uint32_t
    static void addValueAtKey(std::string key,
                              std::string value,
                              std::unordered_map<std::string, uint32_t>& outputDict,
                              const char* debugStr)
    {
        int32_t outInt = 0;
        if (omni::extras::stringToInteger(value, outInt))
        {
            outputDict[key] = outInt;
        }
        else
        {
            CARB_LOG_WARN("Non-integer value %s set in \"%s\"", key.c_str(), debugStr);
        }
    }

    static HashedSettingCString getHashedSettingString(const char* settingStr)
    {
        constexpr size_t sizeOfRtxSettingsRoot = carb::countOf(kSettingsPath) - 1;
        const bool isRtxSettingsPath =
            strncmp(settingStr, kSettingsPath, sizeOfRtxSettingsRoot) == 0 && settingStr[sizeOfRtxSettingsRoot] == '/';
        // Default to kTransientSettingsPath for anything outside /rtx/.
        // We don't promise that anything works for paths not rooted in /rtx[-transient]/internal, but implement
        // reasonable behavior rather than crash or map all invalid settings to a null string. Improperly rooted strings
        // will work fine except that hashInternalRenderSettings() won't know to translate them.
        const char* hashedRoot = isRtxSettingsPath ? kSettingsPath : kTransientSettingsPath;

        return getHashedSettingString(
            MD5::getDigestString(MD5::run((const uint8_t*)settingStr, strlen(settingStr))), hashedRoot);
    }

    static HashedSettingCString getHashedSettingString(const MD5::DigestString& digestStr, const char* root)
    {
        HashedSettingCString result = {};

        char* s = std::copy_n(root, strlen(root), result.data);
        *s++ = '/';
        s = std::copy_n(kHashedSettingKey, carb::countOf(kHashedSettingKey) - 1, s);
        *s++ = '/';
        s = std::copy_n(digestStr.s, MD5::kDigestStringSize, s);
        *s = '\0';

        return result;
    }
};

// Enabling Aperture mode adds 2 ray types.
inline bool enableAperture()
{
    return false;
}

inline bool enableMDLDistilledMtls()
{
    static int result = -1;

    if (result == -1)
    {
        carb::settings::ISettings* settings = carb::getCachedInterface<carb::settings::ISettings>();
        const bool distillMDLMaterials = settings->getAsBool("/rtx/mdltranslator/distillMaterial");

        if (distillMDLMaterials)
        {
            result = 1;
        }
        else
        {
            result = 0;
        }
    }

    return bool(result);
}

inline bool enableAbiskoMode()
{
    static int result = -1;

    if (result == -1)
    {
        carb::settings::ISettings* settings = carb::getCachedInterface<carb::settings::ISettings>();
        const char* renderMode = settings->getStringBuffer("/rtx/rendermode");
        bool enableAbisko = strcmp(renderMode, "abisko") == 0;

        if (enableAbisko)
        {
            result = 1;
        }
        else
        {
            result = 0;
        }
    }

    return bool(result);
}

// Returns how many ray types we are going to use during the whole Kit rendering. This CAN'T be changed once the
// renderer is initialized
inline uint32_t getRayTypeCount()
{
    static carb::tasking::SpinMutex spinMutex;
    static uint32_t result = ~0u;

    // Avoid locking if the result is already set
    if (result == ~0u)
    {
        std::unique_lock<carb::tasking::SpinMutex> lock(spinMutex);
        // Check the result again in case another thread set the result since the prior check
        if (result == ~0u)
        {
            // Use a temporary to accumulate to ensure other threads don't see intermediate values
            uint32_t rayCount = 2; // MATERIAL_RAY and VISIBILITY_RAY for RTX are always available

            if (!rtx::enableAbiskoMode()) // Abisko only has two ray types
            {
                if (rtx::enableAperture())
                {
                    rayCount += 2; // APERTURE_MATERIAL_RAY and APERTURE_VISIBILTY_RAY
                }

                if (rtx::enableMDLDistilledMtls())
                {
                    rayCount += 1; // DISTILLED_MATERIAL_RAY
                }
            }

            result = rayCount;
        }
    }

    return result;
}


/**
 * A helper setting equivalent to setDefaultBool() in addition to returning the status of setting the item.
 *
 * @return true if the item didn't exist and it was set, or false if it already existed and it did nothing.
 */
inline bool setDefaultBoolEx(carb::settings::ISettings* settings, const char* path, bool value)
{
    carb::dictionary::ItemType itemType = settings->getItemType(path);
    if (itemType == carb::dictionary::ItemType::eCount)
    {
        settings->setBool(path, value);
        return true;
    }
    else
    {
        return false;
    }
}

} // namespace rtx
