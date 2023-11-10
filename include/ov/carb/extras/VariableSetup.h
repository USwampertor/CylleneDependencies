// Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Provides helper functions to retrieve setup variable values.
 */
#pragma once

#include "../Framework.h"
#include "../filesystem/IFileSystem.h"
#include "EnvironmentVariable.h"
#include "Path.h"

#include <map>
#include <string>

/** Namespace for all low level Carbonite functionality. */
namespace carb
{
/** Common namespace for extra helper functions and classes. */
namespace extras
{

/**
 * Helper function that reads string value form the string map or the environment variable, if map doesn't hold such
 * key.
 *
 * @param stringMapKey Key that should be present in the map.
 * @param stringMap String map containing values indexed by string key.
 * @param envVarName Environment variable name.
 * @return String value. Can be empty if neither string map value nor env contain the value.
 */
inline std::string getStringFromMapOrEnvVar(const char* stringMapKey,
                                            const std::map<std::string, std::string>& stringMap,
                                            const char* envVarName)
{
    std::string value;

    // Check if variable is specified in the command line arguments first, and otherwise -
    // try to read the environment variable.
    const auto stringMapIt = stringMap.find(stringMapKey);
    if (stringMapIt != stringMap.cend())
    {
        value = stringMapIt->second;
    }
    else
    {
        extras::EnvironmentVariable::getValue(envVarName, value);
    }
    return value;
}

/**
 * Determines application path and name. Priority (for path and name separately):
 * 1. String map (cmd arg)
 * 2. Env var
 * 3. Executable path/name (filesystem)
 *
 * @param stringMap String map - e.g. parsed args as output from the CmdLineParser.
 * @param appPath Resulting application path.
 * @param appName Resulting application name.
 */
inline void getAppPathAndName(const std::map<std::string, std::string>& stringMap,
                              std::string& appPath,
                              std::string& appName)
{
    Framework* f = getFramework();
    filesystem::IFileSystem* fs = f->acquireInterface<filesystem::IFileSystem>();

    // Initialize application path and name to the executable path and name,
    extras::Path execPathModified(fs->getExecutablePath());
#if CARB_PLATFORM_WINDOWS
    // Remove .exe extension on Windows
    execPathModified.replaceExtension("");
#endif
    appPath = execPathModified.getParent();
    appName = execPathModified.getFilename();

    // Override application name and path if command line argument or environment variables
    // are present.
    std::string appPathOverride = getStringFromMapOrEnvVar("app/path", stringMap, "CARB_APP_PATH");
    if (!appPathOverride.empty())
    {
        extras::Path normalizedAppPathOverride(appPathOverride);
        appPath = normalizedAppPathOverride.getNormalized();
    }
    std::string appNameOverride = getStringFromMapOrEnvVar("app/name", stringMap, "CARB_APP_NAME");
    if (!appNameOverride.empty())
    {
        appName = appNameOverride;
    }
}

} // namespace extras
} // namespace carb
