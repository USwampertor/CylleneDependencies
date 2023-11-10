// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"
#include "../logging/Log.h"
#include "StringUtils.h"
#include "Unicode.h"

#include <carb/cpp17/StringView.h>

#include <algorithm>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

#if CARB_PLATFORM_WINDOWS
#    include "../CarbWindows.h"
#elif CARB_PLATFORM_LINUX
#    include <argz.h>
#    include <fstream>
#elif CARB_PLATFORM_MACOS
#    include <crt_externs.h>
#endif

namespace carb
{
namespace extras
{

class CmdLineParser
{
public:
    using Options = std::map<std::string, std::string>;

    CmdLineParser(const char* prefix) : m_prefix(prefix)
    {
    }

    void parse(char** argv, int argc)
    {
        if (argc > 0)
        {
            parseFromArgs(argv, argc);
        }
        else
        {
            parseFromCLI();
        }
    }

    const Options& getOptions() const
    {
        return m_carbOptions;
    }

private:
    void parseFromCLI()
    {
        m_currentKey.clear();

#if CARB_PLATFORM_WINDOWS
        LPWSTR* argv;
        int argc;

        argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (nullptr == argv)
        {
            return;
        }
        parseFromArgs(argv, argc);
        LocalFree(argv);
#elif CARB_PLATFORM_LINUX
        std::string cmdLine;
        std::getline(std::fstream("/proc/self/cmdline", std::ios::in), cmdLine);

        int argc = argz_count(cmdLine.c_str(), cmdLine.size());
        char** argv = static_cast<char**>(std::malloc((argc + 1) * sizeof(char*)));
        argz_extract(cmdLine.c_str(), cmdLine.size(), argv);
        parseFromArgs(argv, argc);
        std::free(argv);
#elif CARB_PLATFORM_MACOS
        int argc = *_NSGetArgc();
        char** argv = *_NSGetArgv();
        parseFromArgs(argv, argc);
#endif
    }

    void parseArg(const std::string& arg)
    {
        // Waiting value for a key?
        if (!m_currentKey.empty())
        {
            std::string value = arg;
            preprocessValue(value);
            m_carbOptions[m_currentKey] = value;
            m_currentKey.clear();
            return;
        }

        // Process only keys starting with kCarbPrefix
        if (strncmp(arg.c_str(), m_prefix.c_str(), m_prefix.size()) == 0)
        {
            // Split to key and value
            std::size_t pos = arg.find("=");
            if (pos != std::string::npos)
            {
                const std::string key = carb::extras::trimString(arg.substr(m_prefix.size(), pos - m_prefix.size()));
                if (!key.empty())
                {
                    std::string value = arg.substr(pos + 1, arg.size() - pos - 1);
                    preprocessValue(value);
                    carb::extras::trimStringInplace(value);
                    m_carbOptions[key] = value;
                }
                else
                {
                    CARB_LOG_WARN("Encountered key-value pair with empty key in command line: %s", arg.c_str());
                }
            }
            else
            {
                // Save key and wait for next value
                m_currentKey = arg.substr(m_prefix.size(), arg.size() - m_prefix.size());
                carb::extras::trimStringInplace(m_currentKey);
            }
        }
    }

#if CARB_PLATFORM_WINDOWS
    void parseFromArgs(wchar_t** argv, int argc)
    {
        if (argc < 0 || argv == nullptr)
            return;

        for (int i = 1; i < argc; ++i)
        {
            if (argv[i])
            {
                std::string arg = extras::convertWideToUtf8(argv[i]);
                parseArg(arg);
            }
        }
    }
#endif

    void parseFromArgs(char** argv, int argc)
    {
        if (argc < 0 || argv == nullptr)
            return;

        for (int i = 1; i < argc; ++i)
        {
            if (argv[i])
            {
                std::string arg = argv[i];
                parseArg(arg);
            }
        }
    }

    void preprocessValue(std::string& value)
    {
        // If surrounded by single quotes - replace them with double quotes for json compatibility
        if (!value.empty() && value[0] == '\'' && value[value.size() - 1] == '\'')
        {
            value[0] = '"';
            value[value.size() - 1] = '"';
        }
    }

private:
    Options m_carbOptions;
    std::string m_currentKey;
    std::string m_prefix;
};


/**
 * Converts an array of command-line arguments to a single command-line string.
 *
 * It handles spaces and quotes in the arguments.
 *
 * @param argv The array of command-line arguments.
 * @param argc The number of arguments in the array.
 * @return A string that represents the command-line arguments.
 */
inline std::string argvToCommandLine(const char* const* argv, size_t argc)
{
    std::ostringstream os;
    for (size_t i = 0; i < argc; ++i)
    {
        if (i > 0)
            os << " ";

        // quote this argument if it contains tabs, spaces, quotes, newlines, or carriage returns
        cpp17::string_view sv{ argv[i] };
        if (sv.find_first_of(" \t\n\r\"", 0, 5) != std::string::npos)
        {
            os << "\"";
            os << sv.data();
            os << "\"";
        }
        else
        {
            os << sv.data();
        }
    }
    return os.str();
}


} // namespace extras
} // namespace carb
