// Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"

#if CARB_PLATFORM_WINDOWS

#    include "../CarbWindows.h"
#    include "../logging/Log.h"
#    include "Unicode.h"

#    include <algorithm>
#    include <string>

namespace carb
{
namespace extras
{

/**
 * Converts a UTF-8 file path to Windows system file path.
 * Slashes are replaced with backslashes, long path prefix is appended if required.
 * In most cases, you won't need this function. Use IFileSystem interface when working with files and folders in a
 * file system.
 *
 * @param path Input string to convert, in UTF-8 encoding.
 * @return Wide string containing Windows system file path or empty string if conversion cannot be performed.
 */
inline std::wstring convertCarboniteToWindowsPath(const std::string& path);

/**
 * Converts Windows system file path to a UTF-8 file path.
 * Backslashes are replaced with slashes, long path prefix is removed.
 * In most cases, you won't need this function. Use IFileSystem interface when working with files and folders in a
 * file system.
 *
 * @param pathW Input string to convert, in Unicode (Windows native) encoding.
 * @return UTF-8 encoded file path or empty string if conversion cannot be performed.
 */
inline std::string convertWindowsToCarbonitePath(const std::wstring& pathW);

/**
 * Fixes Windows system file path. Can be in handy when you split or join file path components.
 * If the file path is too long and doesn't have long path prefix, the prefix is added.
 * If the file path is short and has long path prefix, the prefix is removed.
 * Otherwise, the path is not modified.
 *
 * @param pathW Input string to convert, in Unicode (Windows native) encoding.
 * @return Valid Windows system file path.
 */
inline std::wstring fixWindowsPathPrefix(const std::wstring& pathW);

/**
 * Converts Windows path string into a canonical form.
 * If it's not possible, original path is returned.
 *
 * @param pathW Windows system file path, in Unicode (Windows native) encoding.
 * @return The canonical form of the input path.
 */
inline std::wstring getWindowsCanonicalPath(const std::wstring& pathW);

/**
 * Retrieves the full path and file name of the specified file.
 * If it's not possible, original path is returned.
 *
 * @param pathW Windows system file path, in Unicode (Windows native) encoding.
 * @return The full path and file name of the input file.
 */
inline std::wstring getWindowsFullPath(const std::wstring& pathW);

inline std::wstring convertCarboniteToWindowsPath(const std::string& path)
{
    std::wstring pathW = convertUtf8ToWide(path);
    if (pathW == kUnicodeToWideFailure)
    {
        return L"";
    }
    std::replace(pathW.begin(), pathW.end(), L'/', L'\\');
    return fixWindowsPathPrefix(pathW);
}

inline std::string convertWindowsToCarbonitePath(const std::wstring& pathW)
{
    bool hasPrefix = (pathW.compare(0, 4, L"\\\\?\\") == 0);
    std::string path = convertWideToUtf8(pathW.c_str() + (hasPrefix ? 4 : 0));
    if (path == kUnicodeToUtf8Failure)
    {
        return "";
    }
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

inline std::wstring fixWindowsPathPrefix(const std::wstring& pathW)
{
    bool hasPrefix = (pathW.compare(0, 4, L"\\\\?\\") == 0);

    if (pathW.size() >= CARBWIN_MAX_PATH && !hasPrefix)
    {
        return L"\\\\?\\" + pathW;
    }
    if (pathW.size() < CARBWIN_MAX_PATH && hasPrefix)
    {
        return pathW.substr(4, pathW.size() - 4);
    }

    return pathW;
}

inline std::wstring getWindowsCanonicalPath(const std::wstring& pathW)
{
    wchar_t* canonical = nullptr;
    if (PathAllocCanonicalize(pathW.c_str(), CARBWIN_PATHCCH_ALLOW_LONG_PATHS, &canonical) == CARBWIN_S_OK)
    {
        std::wstring result = canonical;
        LocalFree(canonical);
        return result;
    }

    CARB_LOG_WARN("The path '%s' could not be canonicalized!", extras::convertWindowsToCarbonitePath(pathW).c_str());
    return pathW;
}

inline std::wstring getWindowsFullPath(const std::wstring& pathW)
{
    DWORD size = GetFullPathNameW(pathW.c_str(), 0, nullptr, nullptr);
    if (size != 0)
    {
        std::wstring fullPathName;
        fullPathName.resize(size - 1);
        if (GetFullPathNameW(pathW.c_str(), size, &fullPathName[0], nullptr) != 0)
        {
            return fullPathName;
        }
    }

    CARB_LOG_WARN("Can't retrieve the full path of '%s'!", extras::convertWindowsToCarbonitePath(pathW).c_str());
    return pathW;
}

inline void adjustWindowsDllSearchPaths()
{
    // MSDN:
    // https://docs.microsoft.com/en-us/windows/desktop/api/libloaderapi/nf-libloaderapi-setdefaultdlldirectories
    // LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
    // This value is a combination of LOAD_LIBRARY_SEARCH_APPLICATION_DIR, LOAD_LIBRARY_SEARCH_SYSTEM32, and
    // LOAD_LIBRARY_SEARCH_USER_DIRS.
    SetDefaultDllDirectories(CARBWIN_LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
}

} // namespace extras
} // namespace carb

#endif
