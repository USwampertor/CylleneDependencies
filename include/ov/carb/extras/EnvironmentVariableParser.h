// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief Parses environment variables into map of key/value pairs
//
#pragma once

#include "../Defines.h"

#include "StringUtils.h"
#include "Unicode.h"

#include <cstring>
#include <map>
#include <string>

#if CARB_PLATFORM_WINDOWS
#    include "../CarbWindows.h"
#else
#    ifndef DOXYGEN_BUILD
extern "C"
{
    extern char** environ;
}
#    endif
#endif

namespace carb
{
namespace extras
{

/**
 * Parses environment variables into program options or environment variables
 */
class EnvironmentVariableParser
{
public:
    /**
     * Construct an environment parser looking for variables starting with @p prefix
     */
    EnvironmentVariableParser(const char* prefix) : m_prefix(prefix)
    {
    }

    /**
     * Parse the environment. Variables starting with prefix given to constructor are separated
     * into program options, whereas all others are parsed as normal environment variables
     */
    void parse()
    {
        m_pathwiseOverrides.clear();
        CARB_MSC_ONLY(parseForMsvc());
        CARB_NOT_MSC(parseForOthers());
    }

    //! key/value pairs of the parsed environment variables
    using Options = std::map<std::string, std::string>;

    /**
     * The map of program options that matched prefix.
     * @return map of key/value options
     */
    const Options& getOptions()
    {
        return m_pathwiseOverrides;
    }

    /**
     * The map of all environment variables that did not the prefix.
     * @return map of key/value environment variables
     */
    const Options& getEnvVariables()
    {
        return m_envVariables;
    }

private:
#if CARB_COMPILER_MSC
    void parseForMsvc()
    {
        class EnvVarsBlockWatchdog
        {
        public:
            EnvVarsBlockWatchdog()
            {
                m_envsBlock = GetEnvironmentStringsW();
            }
            ~EnvVarsBlockWatchdog()
            {
                if (m_envsBlock)
                {
                    FreeEnvironmentStringsW(m_envsBlock);
                }
            }

            const wchar_t* getEnvsBlock()
            {
                return m_envsBlock;
            }

        private:
            wchar_t* m_envsBlock = nullptr;
        };

        EnvVarsBlockWatchdog envVarsBlovkWatchdog;

        const wchar_t* envsBlock = envVarsBlovkWatchdog.getEnvsBlock();
        if (!envsBlock)
        {
            return;
        }

        const wchar_t* var = (LPWSTR)envsBlock;
        while (*var)
        {
            std::string varUtf8 = carb::extras::convertWideToUtf8(var);
            size_t equalSignPos = 0;
            size_t varLen = (size_t)lstrlenW(var);
            while (equalSignPos < varLen && var[equalSignPos] != L'=')
            {
                ++equalSignPos;
            }

            var += varLen + 1;

            if (equalSignPos == 0 || equalSignPos == varLen)
            {
                continue;
            }

            std::string varNameUtf8(varUtf8.c_str(), equalSignPos);
            _processAndAddOption(varNameUtf8.c_str(), varUtf8.c_str() + equalSignPos + 1);
        }
    }
#else
    void parseForOthers()
    {
        char** envsBlock = environ;
        if (!envsBlock || !*envsBlock)
        {
            return;
        }

        while (*envsBlock)
        {
            size_t equalSignPos = 0;
            const char* var = *envsBlock;
            size_t varLen = (size_t)strlen(var);
            while (equalSignPos < varLen && var[equalSignPos] != '=')
            {
                ++equalSignPos;
            }

            ++envsBlock;

            if (equalSignPos == 0 || equalSignPos == varLen)
            {
                continue;
            }

            std::string varName(var, equalSignPos);
            _processAndAddOption(varName.c_str(), var + equalSignPos + 1);
        }
    }
#endif

    void _processAndAddOption(const char* varName, const char* varValue)
    {
        if (!m_prefix.empty() && startsWith(varName, m_prefix))
        {
            std::string path = varName + m_prefix.size();
            for (size_t i = 0, pathLen = path.length(); i < pathLen; ++i)
            {
                // TODO: WRONG, must be utf8-aware replacement
                if (path[i] == '_')
                    path[i] = '/';
            }
            m_pathwiseOverrides.insert(std::make_pair(path.c_str(), varValue));
        }
        else
        {
            CARB_ASSERT(varName);
            CARB_ASSERT(varValue);
            m_envVariables[varName] = varValue;
        }
    }

    // Path-wise overrides
    Options m_pathwiseOverrides;
    // Explicit env variables
    Options m_envVariables;
    std::string m_prefix;
};

} // namespace extras
} // namespace carb
