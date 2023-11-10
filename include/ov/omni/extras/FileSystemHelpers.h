// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include <carb/InterfaceUtils.h>
#include <carb/extras/Path.h>
#include <carb/filesystem/IFileSystem.h>
#include <carb/tokens/TokensUtils.h>

#include <string>
#include <vector>

namespace omni
{
namespace extras
{
inline carb::filesystem::IFileSystem* getFileSystem()
{
    return carb::getCachedInterface<carb::filesystem::IFileSystem>();
}

/*
 * Walk folder and gather all subfolders and files.
 */
inline std::vector<std::string> getDirectoryItems(const std::string& folder)
{
    std::vector<std::string> items;
    getFileSystem()->forEachDirectoryItem(folder.c_str(),
                                          [](const carb::filesystem::DirectoryItemInfo* const info, void* userData) {
                                              decltype(items)* _items = static_cast<decltype(items)*>(userData);
                                              _items->push_back(info->path);
                                              return carb::filesystem::WalkAction::eContinue;
                                          },
                                          &items);
    return items;
}

/*
 * Walk folder and gather all either subfolders or files depending on a type.
 */
inline std::vector<std::string> getDirectoryItemsOfType(const std::string& folder,
                                                        carb::filesystem::DirectoryItemType type)
{
    struct UserData
    {
        carb::filesystem::DirectoryItemType type;
        std::vector<std::string> items;
    };
    UserData data{ type, {} };

    getFileSystem()->forEachDirectoryItem(folder.c_str(),
                                          [](const carb::filesystem::DirectoryItemInfo* const info, void* userData) {
                                              decltype(data)* _data = static_cast<decltype(data)*>(userData);
                                              if (info->type == _data->type)
                                                  _data->items.push_back(info->path);
                                              return carb::filesystem::WalkAction::eContinue;
                                          },
                                          &data);
    return data.items;
}

/*
 * Walk folder and gather all subfolders.
 */
inline std::vector<std::string> getSubfolders(const std::string& folder)
{
    return getDirectoryItemsOfType(folder, carb::filesystem::DirectoryItemType::eDirectory);
}

/*
 * Walk all folders and gather all subfolders.
 */
inline std::vector<std::string> getSubfolders(const std::vector<std::string>& folders)
{
    std::vector<std::string> allSubFolders;
    for (const std::string& folder : folders)
    {
        const std::vector<std::string> subFolders = getSubfolders(folder);
        allSubFolders.insert(allSubFolders.end(), subFolders.begin(), subFolders.end());
    }
    return allSubFolders;
}

/*
 * Resolve path by resolving all tokens and converting relative pass to absolute using root path.
 */
inline std::string resolvePath(const std::string& path, const std::string& root = "")
{
    auto tokens = carb::getCachedInterface<carb::tokens::ITokens>();

    carb::extras::Path resultPath = carb::tokens::resolveString(tokens, path.c_str());

    // If relative - relative to the root
    if (!root.empty() && resultPath.isRelative())
    {
        resultPath = carb::extras::Path(root).join(resultPath);
    }
    return resultPath;
}

/*
 * Read file content in a string. Returns a pair of bool, std::string. Where bool indicates if read was succesfull.
 */
inline std::pair<bool, std::string> readFile(const char* path)
{
    auto fs = getFileSystem();
    if (fs->exists(path))
    {
        carb::filesystem::File* file = fs->openFileToRead(path);
        if (file)
        {
            size_t size = fs->getFileSize(file);
            if (size)
            {
                std::string buffer(size, ' ');
                fs->readFileChunk(file, &buffer[0], size);
                fs->closeFile(file);
                return std::make_pair(true, buffer);
            }
            else
            {
                fs->closeFile(file);
                return std::make_pair(true, "");
            }
        }
    }
    return std::make_pair(false, "");
}

} // namespace extras

} // namespace omni
