// Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "Framework.h"
#include "extras/Path.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace carb
{
/**
 * Get all registered plugins and collect folders they are located in.
 */
inline std::unordered_set<std::string> getPluginFolders()
{
    Framework* framework = carb::getFramework();
    std::vector<PluginDesc> plugins(framework->getPluginCount());
    framework->getPlugins(plugins.data());
    std::unordered_set<std::string> folders;
    for (const auto& desc : plugins)
    {
        extras::Path p(desc.libPath);
        const std::string& folder = p.getParent();
        if (!folder.empty())
        {
            folders.insert(folder);
        }
    }
    return folders;
}
} // namespace carb
