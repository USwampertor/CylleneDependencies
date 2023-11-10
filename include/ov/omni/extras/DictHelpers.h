// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include <carb/dictionary/DictionaryUtils.h>

#include <string>
#include <vector>

namespace carb
{
namespace dictionary
{

/**
 * Get value if present, otherwise return defaultValue.
 */
template <typename T>
T getValueOrDefault(const Item* baseItem, const char* path, T defaultValue)
{
    IDictionary* d = getCachedDictionaryInterface();
    auto item = d->getItem(baseItem, path);
    if (!item)
        return defaultValue;
    return d->get<T>(item);
}

/**
 * Make string array at path if doesn't exist and set values in it.
 */
inline Item* makeStringArrayAtPath(Item* baseItem, const char* path, const std::vector<std::string>& arr)
{
    IDictionary* dict = getCachedDictionaryInterface();
    ScopedWrite g(*dict, baseItem);
    Item* item = dict->getItemMutable(baseItem, path);
    if (!item)
    {
        item = dict->createItem(baseItem, path, ItemType::eDictionary);
    }

    for (size_t i = 0, count = arr.size(); i < count; ++i)
    {
        dict->setStringAt(item, i, arr[i].c_str());
    }
    return item;
}

} // namespace dictionary

} // namespace carb
