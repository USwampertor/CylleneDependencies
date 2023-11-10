// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include <carb/settings/SettingsUtils.h>

#include <string>
#include <vector>

namespace carb
{
namespace settings
{


/**
 * Get setting value if present, otherwise return defaultValue.
 */
template <typename T>
T getSettingOrDefault(const char* path, T defaultValue = {})
{
    ISettings* s = getCachedInterface<ISettings>();
    if (s->getItemType(path) == dictionary::ItemType::eCount)
        return defaultValue;
    return s->get<T>(path);
}

/**
 * Set default setting value and return current value.
 */
template <typename T>
T setDefaultAndGetSetting(const char* path, T defaultValue = {})
{
    ISettings* s = getCachedInterface<ISettings>();
    s->setDefault<T>(path, defaultValue);
    return s->get<T>(path);
}

/**
 * Append to the end of string array in settings.
 */
inline void appendToStringArray(const char* path, const std::vector<std::string>& values)
{
    if (values.empty())
        return;
    ISettings* s = getCachedInterface<ISettings>();
    std::vector<std::string> v = settings::getStringArray(s, path);
    v.insert(v.end(), values.begin(), values.end());
    settings::setStringArray(s, path, v);
}

} // namespace settings
} // namespace carb
