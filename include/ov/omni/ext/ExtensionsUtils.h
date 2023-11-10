// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "IExtensions.h"

#include <carb/ObjectUtils.h>
#include <carb/dictionary/DictionaryUtils.h>
#include <carb/settings/ISettings.h>
#include <carb/settings/SettingsUtils.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>


namespace omni
{
namespace ext
{

inline std::string getExtensionPath(ExtensionManager* manager, const char* extId)
{
    auto dict = carb::dictionary::getCachedDictionaryInterface();
    carb::dictionary::Item* infoDict = manager->getExtensionDict(extId);
    return dict->get<const char*>(infoDict, "path");
}


inline const char* getEnabledExtensionId(ExtensionManager* manager, const char* extFullName)
{
    size_t count;
    ExtensionInfo* extensions;
    manager->fetchExtensionVersions(extFullName, &extensions, &count);
    for (size_t i = 0; i < count; i++)
    {
        if (extensions[i].enabled)
            return extensions[i].id;
    }
    return nullptr;
}


/**
 * Check if extension is enabled by name (fullname, like "omni.kit.loop-default"). There could be multiple versions
 * of extension with such name registered. That function doesn't care about version and just returns if any of them is
 * enabled.
 */
inline bool isExtensionEnabled(ExtensionManager* manager, const char* extFullName)
{
    return getEnabledExtensionId(manager, extFullName) != nullptr;
}


class ExtensionStateChangeHookLambda : public IExtensionStateChangeHook
{
public:
    ExtensionStateChangeHookLambda(const std::function<void(const char*, ExtensionStateChangeType)>& fn) : m_fn(fn)
    {
    }

    void onStateChange(const char* extId, ExtensionStateChangeType type) override
    {
        if (m_fn)
            m_fn(extId, type);
    }

private:
    std::function<void(const char*, ExtensionStateChangeType)> m_fn;

    CARB_IOBJECT_IMPL
};

inline IHookHolderPtr createExtensionStateChangeHook(
    IExtensionManagerHooks* hooks,
    const std::function<void(const char* extId, ExtensionStateChangeType type)>& onStateChange,
    ExtensionStateChangeType type,
    const char* extFullName = "",
    const char* extDictPath = "",
    Order order = kDefaultOrder,
    const char* hookName = nullptr)
{
    return hooks->createExtensionStateChangeHook(
        carb::stealObject(new ExtensionStateChangeHookLambda(onStateChange)).get(), type, extFullName, extDictPath,
        order, hookName);
}

/**
 * Call callback if extension is enabled and hook in extension system to wait for extension to be
 * enabled and then disabled (can happen multiple times if extension is reloaded).
 *
 * Returns a pair of both subscriptions holders.
 */
inline std::pair<IHookHolderPtr, IHookHolderPtr> subscribeToExtensionEnable(
    ExtensionManager* manager,
    const std::function<void(const char* extId)>& onEnable,
    const std::function<void(const char* extId)>& onDisable = nullptr,
    const char* extFullName = "",
    const char* hookName = nullptr)
{
    // Already enabled?
    if (extFullName && extFullName[0] != '\0')
    {
        const char* enabledExtId = getEnabledExtensionId(manager, extFullName);
        if (enabledExtId)
            onEnable(enabledExtId);
    }

    // Subscribe for enabling:
    IHookHolderPtr holder0 = createExtensionStateChangeHook(
        manager->getHooks(), [onEnable = onEnable](const char* extId, ExtensionStateChangeType) { onEnable(extId); },
        ExtensionStateChangeType::eAfterExtensionEnable, extFullName, "", kDefaultOrder, hookName);

    // Otionally subscripbe for disabling
    IHookHolderPtr holder1;
    if (onDisable)
        holder1 = createExtensionStateChangeHook(
            manager->getHooks(),
            [onDisable = onDisable](const char* extId, ExtensionStateChangeType) { onDisable(extId); },
            ExtensionStateChangeType::eBeforeExtensionShutdown, extFullName, "", kDefaultOrder, hookName);
    return std::make_pair(holder0, holder1);
}


struct ExtPathUrl
{
    std::string scheme;
    std::string path;
};

inline ExtPathUrl parseExtUrl(const std::string& url)
{
    const std::string kSchemeDelimiter = ":";
    auto pos = url.find(kSchemeDelimiter);
    if (pos == std::string::npos || pos == 1)
        return { "", url };

    ExtPathUrl res = {};
    res.scheme = url.substr(0, pos);
    res.path = url.substr(pos + kSchemeDelimiter.size() + 1);
    res.path = res.path.erase(0, res.path.find_first_not_of("/"));
    return res;
}

} // namespace ext
} // namespace omni
