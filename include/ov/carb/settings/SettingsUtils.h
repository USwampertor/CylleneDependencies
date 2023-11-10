// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../dictionary/DictionaryUtils.h"
#include "../dictionary/ISerializer.h"
#include "ISettings.h"

#include <atomic>
#include <string>

namespace carb
{
namespace settings
{

inline std::string getStringFromItemValue(const ISettings* settings, const char* path, const std::string& defaultValue = "")
{
    const char* stringBuf = settings->createStringBufferFromItemValue(path);
    if (!stringBuf)
        return defaultValue;
    std::string returnString = stringBuf;
    settings->destroyStringBuffer(stringBuf);
    return returnString;
}

inline std::string getStringFromItemValueAt(const ISettings* settings,
                                            const char* path,
                                            size_t index,
                                            const std::string& defaultValue = "")
{
    const char* stringBuf = settings->createStringBufferFromItemValueAt(path, index);
    if (!stringBuf)
        return defaultValue;
    std::string returnString = stringBuf;
    settings->destroyStringBuffer(stringBuf);
    return returnString;
}

inline std::string getString(const ISettings* settings, const char* path, const std::string& defaultValue = "")
{
    const char* value = settings->getStringBuffer(path);
    if (!value)
        return defaultValue;
    return value;
}

inline std::string getStringAt(const ISettings* settings,
                               const char* path,
                               size_t index,
                               const std::string& defaultValue = "")
{
    const char* value = settings->getStringBufferAt(path, index);
    if (!value)
        return defaultValue;
    return value;
}

inline void setIntArray(ISettings* settings, const char* path, const std::vector<int>& array)
{
    int64_t tempArray[1] = { 0 };
    settings->setInt64Array(path, tempArray, 1);
    for (size_t i = 0, elementCount = array.size(); i < elementCount; ++i)
    {
        settings->setInt64At(path, i, (int64_t)array[i]);
    }
}

inline void setIntArray(ISettings* settings, const char* path, const std::vector<int64_t>& array)
{
    settings->setInt64Array(path, array.data(), array.size());
}

inline void setFloatArray(ISettings* settings, const char* path, const std::vector<float>& array)
{
    double tempArray[1] = { 0 };
    settings->setFloat64Array(path, tempArray, 1);
    for (size_t i = 0, elementCount = array.size(); i < elementCount; ++i)
    {
        settings->setFloat64At(path, i, (double)array[i]);
    }
}

inline void setFloatArray(ISettings* settings, const char* path, const std::vector<double>& array)
{
    settings->setFloat64Array(path, array.data(), array.size());
}

inline void setBoolArray(ISettings* settings, const char* path, const std::vector<bool>& array)
{
    const size_t arraySize = array.size();
    for (size_t i = 0; i < arraySize; ++i)
    {
        settings->setBoolAt(path, i, array[i]);
    }
}

inline std::vector<std::string> getStringArray(ISettings* settings, const char* path, const std::string& defaultValue = "")
{
    std::vector<std::string> array(settings->getArrayLength(path));
    for (size_t i = 0, arraySize = array.size(); i < arraySize; ++i)
    {
        array[i] = getStringAt(settings, path, i, defaultValue);
    }
    return array;
}

inline std::vector<std::string> getStringArrayFromItemValues(ISettings* settings,
                                                             const char* path,
                                                             const std::string& defaultValue = "")
{
    std::vector<std::string> array(settings->getArrayLength(path));
    for (size_t i = 0, arraySize = array.size(); i < arraySize; ++i)
    {
        array[i] = getStringFromItemValueAt(settings, path, i, defaultValue);
    }
    return array;
}

inline void setStringArray(ISettings* settings, const char* path, const std::vector<std::string>& array)
{
    const size_t arraySize = array.size();
    if (settings->getItemType(path) != dictionary::ItemType::eCount)
    {
        settings->destroyItem(path);
    }
    for (size_t i = 0; i < arraySize; ++i)
    {
        settings->setStringAt(path, i, array[i].c_str());
    }
}

inline void loadSettingsFromFile(ISettings* settings,
                                 const char* path,
                                 dictionary::IDictionary* dictionary,
                                 dictionary::ISerializer* serializer,
                                 const char* filename)
{
    carb::dictionary::Item* settingsFromFile = carb::dictionary::createDictionaryFromFile(serializer, filename);

    settings->update(path, settingsFromFile, nullptr, dictionary::overwriteOriginalWithArrayHandling, dictionary);
    dictionary->destroyItem(settingsFromFile);
}

inline void saveFileFromSettings(const ISettings* settings,
                                 dictionary::ISerializer* serializer,
                                 const char* path,
                                 const char* filename,
                                 dictionary::SerializerOptions serializerOptions)
{
    const dictionary::Item* settingsDictionaryAtPath = settings->getSettingsDictionary(path);
    dictionary::saveFileFromDictionary(serializer, settingsDictionaryAtPath, filename, serializerOptions);
}

template <typename ElementData, typename OnItemFnType>
inline void walkSettings(carb::dictionary::IDictionary* idict,
                         carb::settings::ISettings* settings,
                         dictionary::WalkerMode walkerMode,
                         const char* rootPath,
                         ElementData rootElementData,
                         OnItemFnType onItemFn,
                         void* userData)
{
    using namespace carb;

    if (!rootPath)
    {
        return;
    }

    if (rootPath[0] == 0)
        rootPath = "/";

    struct ValueToParse
    {
        std::string srcPath;
        ElementData elementData;
    };

    std::vector<ValueToParse> valuesToParse;
    valuesToParse.reserve(100);

    auto enqueueChildren = [&idict, &settings, &valuesToParse](const char* parentPath, ElementData parentElementData) {
        if (!parentPath)
        {
            return;
        }

        const dictionary::Item* parentItem = settings->getSettingsDictionary(parentPath);
        size_t numChildren = idict->getItemChildCount(parentItem);
        for (size_t chIdx = 0; chIdx < numChildren; ++chIdx)
        {
            const dictionary::Item* childItem = idict->getItemChildByIndex(parentItem, numChildren - chIdx - 1);
            const char* childItemName = idict->getItemName(childItem);
            std::string childPath;
            bool isRootParent = (idict->getItemParent(parentItem) == nullptr);
            if (isRootParent)
            {
                childPath = std::string(parentPath) + childItemName;
            }
            else
            {
                childPath = std::string(parentPath) + "/" + childItemName;
            }
            valuesToParse.push_back({ childPath, parentElementData });
        }
    };

    if (walkerMode == dictionary::WalkerMode::eSkipRoot)
    {
        const char* parentPath = rootPath;
        ElementData parentElementData = rootElementData;
        enqueueChildren(parentPath, parentElementData);
    }
    else
    {
        valuesToParse.push_back({ rootPath, rootElementData });
    }

    while (valuesToParse.size())
    {
        const ValueToParse& valueToParse = valuesToParse.back();
        std::string curItemPathStorage = std::move(valueToParse.srcPath);
        const char* curItemPath = curItemPathStorage.c_str();
        ElementData elementData = std::move(valueToParse.elementData);
        valuesToParse.pop_back();

        dictionary::ItemType curItemType = settings->getItemType(curItemPath);

        if (curItemType == dictionary::ItemType::eDictionary)
        {
            ElementData parentElementData = onItemFn(curItemPath, elementData, userData);
            enqueueChildren(curItemPath, parentElementData);
        }
        else
        {
            onItemFn(curItemPath, elementData, userData);
        }
    }
}

/**
 * This class provides means to add thread-safe local caching to the settings fields.
 * Allows to avoid explicit polling which can be expensive. Instead of polling, create
 * an instance of this class, and start tracking whenever needed. The internal value
 * will then be updated automatically.
 */
template <typename SettingType>
class ThreadSafeLocalCache
{
public:
    ThreadSafeLocalCache(SettingType initState = SettingType{}) : m_value(initState), m_valueDirty(false)
    {
    }
    ~ThreadSafeLocalCache()
    {
        stopTracking();
    }

    void startTracking(const char* settingPath)
    {
        CARB_ASSERT(settingPath, "Must specify a valid setting name.");
        CARB_ASSERT(m_subscription == nullptr,
                    "Already tracking this value, do not track again without calling stopTracking first.");

        Framework* f = getFramework();
        m_settings = f->tryAcquireInterface<settings::ISettings>();
        m_dictionary = f->tryAcquireInterface<dictionary::IDictionary>();
        if (!m_settings || !m_dictionary)
            return;

        m_valueSettingsPath = settingPath;
        m_value.store(m_settings->get<SettingType>(settingPath), std::memory_order_relaxed);
        m_valueDirty.store(false, std::memory_order_release);

        m_subscription = m_settings->subscribeToNodeChangeEvents(
            settingPath,
            [](const dictionary::Item* changedItem, dictionary::ChangeEventType changeEventType, void* userData) {
                if (changeEventType == dictionary::ChangeEventType::eChanged)
                {
                    ThreadSafeLocalCache* thisClassInstance = reinterpret_cast<ThreadSafeLocalCache*>(userData);
                    thisClassInstance->m_value.store(
                        thisClassInstance->getDictionaryInterface()->template get<SettingType>(changedItem),
                        std::memory_order_relaxed);
                    thisClassInstance->m_valueDirty.store(true, std::memory_order_release);
                }
            },
            this);
        if (m_subscription != nullptr)
        {
            f->addReleaseHook(m_settings, sOnRelease, this);
        }
    }
    void stopTracking()
    {
        if (m_subscription)
        {
            carb::getFramework()->removeReleaseHook(m_settings, sOnRelease, this);
            m_settings->unsubscribeToChangeEvents(m_subscription);
            m_subscription = nullptr;
        }
    }

    SettingType get() const
    {
        CARB_ASSERT(m_subscription, "Call startTracking before reading this variable.");
        return m_value.load(std::memory_order_relaxed);
    }

    operator SettingType() const
    {
        return get();
    }

    void set(SettingType value)
    {
        m_settings->set<SettingType>(m_valueSettingsPath.c_str(), value);
    }

    bool isValueDirty() const
    {
        return m_valueDirty.load(std::memory_order_relaxed);
    }
    void clearValueDirty()
    {
        m_valueDirty.store(false, std::memory_order_release);
    }

    const char* getSettingsPath() const
    {
        return m_valueSettingsPath.c_str();
    }

    inline dictionary::IDictionary* getDictionaryInterface() const
    {
        return m_dictionary;
    }

private:
    static void sOnRelease(void* iface, void* user)
    {
        // Settings has gone away, so our subscription is defunct
        static_cast<ThreadSafeLocalCache*>(user)->m_subscription = nullptr;
        carb::getFramework()->removeReleaseHook(iface, sOnRelease, user);
    }

    // NOTE: The callback may come in on another thread so wrap it in an atomic to prevent a race.
    std::atomic<SettingType> m_value;
    std::atomic<bool> m_valueDirty;
    std::string m_valueSettingsPath;
    dictionary::SubscriptionId* m_subscription = nullptr;
    dictionary::IDictionary* m_dictionary = nullptr;
    settings::ISettings* m_settings = nullptr;
};

template <>
class ThreadSafeLocalCache<const char*>
{
public:
    ThreadSafeLocalCache(const char* initState = "") : m_valueDirty(false)
    {
        std::lock_guard<std::mutex> guard(m_valueMutex);
        m_value = initState;
    }
    ~ThreadSafeLocalCache()
    {
        stopTracking();
    }

    void startTracking(const char* settingPath)
    {
        CARB_ASSERT(settingPath, "Must specify a valid setting name.");
        CARB_ASSERT(m_subscription == nullptr,
                    "Already tracking this value, do not track again without calling stopTracking first.");

        Framework* f = getFramework();
        m_settings = f->tryAcquireInterface<settings::ISettings>();
        m_dictionary = f->tryAcquireInterface<dictionary::IDictionary>();

        m_valueSettingsPath = settingPath;
        const char* valueRaw = m_settings->get<const char*>(settingPath);
        m_value = valueRaw ? valueRaw : "";
        m_valueDirty.store(false, std::memory_order_release);

        m_subscription = m_settings->subscribeToNodeChangeEvents(
            settingPath,
            [](const dictionary::Item* changedItem, dictionary::ChangeEventType changeEventType, void* userData) {
                if (changeEventType == dictionary::ChangeEventType::eChanged)
                {
                    ThreadSafeLocalCache* thisClassInstance = reinterpret_cast<ThreadSafeLocalCache*>(userData);
                    {
                        const char* valueStringBuffer =
                            thisClassInstance->getDictionaryInterface()->template get<const char*>(changedItem);
                        std::lock_guard<std::mutex> guard(thisClassInstance->m_valueMutex);
                        thisClassInstance->m_value = valueStringBuffer ? valueStringBuffer : "";
                    }
                    thisClassInstance->m_valueDirty.store(true, std::memory_order_release);
                }
            },
            this);
        if (m_subscription)
        {
            f->addReleaseHook(m_settings, sOnRelease, this);
        }
    }
    void stopTracking()
    {
        if (m_subscription)
        {
            m_settings->unsubscribeToChangeEvents(m_subscription);
            m_subscription = nullptr;
            carb::getFramework()->removeReleaseHook(m_settings, sOnRelease, this);
        }
    }

    const char* get() const
    {
        // Not a safe operation
        CARB_ASSERT(false);
        CARB_LOG_ERROR("Shouldn't use unsafe get on a ThreadSafeLocalCache<const char*>");
        return "";
    }

    operator const char*() const
    {
        // Not a safe operation
        return get();
    }

    std::string getStringSafe() const
    {
        // Not a safe operation
        CARB_ASSERT(m_subscription, "Call startTracking before reading this variable.");
        std::lock_guard<std::mutex> guard(m_valueMutex);
        return m_value;
    }

    void set(const char* value)
    {
        m_settings->set<const char*>(m_valueSettingsPath.c_str(), value);
    }

    bool isValueDirty() const
    {
        return m_valueDirty.load(std::memory_order_relaxed);
    }
    void clearValueDirty()
    {
        m_valueDirty.store(false, std::memory_order_release);
    }

    const char* getSettingsPath() const
    {
        return m_valueSettingsPath.c_str();
    }

    inline dictionary::IDictionary* getDictionaryInterface() const
    {
        return m_dictionary;
    }

private:
    static void sOnRelease(void* iface, void* user)
    {
        // Settings has gone away, so our subscription is defunct
        static_cast<ThreadSafeLocalCache*>(user)->m_subscription = nullptr;
        carb::getFramework()->removeReleaseHook(iface, sOnRelease, user);
    }

    // NOTE: The callback may come in on another thread so wrap it in a mutex to prevent a race.
    std::string m_value;
    mutable std::mutex m_valueMutex;
    std::atomic<bool> m_valueDirty;
    std::string m_valueSettingsPath;
    dictionary::SubscriptionId* m_subscription = nullptr;
    dictionary::IDictionary* m_dictionary = nullptr;
    settings::ISettings* m_settings = nullptr;
};


} // namespace settings
} // namespace carb
