// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"

#include <string>
#include <vector>

namespace carb
{
namespace extras
{

// These routines provide very basic non-optimal support for CSV files.
inline std::vector<std::string> fromCsvString(const char* string)
{
    // The rules of CSV are pretty simple.
    // ',''s '"''s and new lines should be encased in "'s

    bool inQuote = false;
    bool escaped = false;
    std::string current;
    std::vector<std::string> result;

    while (*string)
    {
        if (escaped)
        {
            if (*string == 'n')
            {
                string += '\n';
            }
            current += *string;
            escaped = false;
        }
        else if (*string == '\\')
        {
            escaped = true;
        }
        else if (inQuote)
        {
            if (*string == '\"')
            {
                if (*(string + 1) == '\"')
                {
                    current += "\"";
                    string++;
                }
                else
                {
                    inQuote = false;
                }
            }
            else
            {
                current += *string;
            }
        }
        else if (*string == ',')
        {
            result.emplace_back(std::move(current));
            current.clear();
        }
        else
        {
            current += *string;
        }

        string++;
    }

    if (result.empty() && current.empty())
        return result;

    result.emplace_back(std::move(current));
    return result;
}

inline std::string toCsvString(const std::vector<std::string>& columns)
{
    std::string result;

    for (size_t i = 0; i < columns.size(); i++)
    {
        if (i != 0)
            result += ",";
        auto& column = columns[i];

        // Determine if the string has anything that needs escaping
        bool needsEscaping = false;
        for (auto c : column)
        {
            if (c == '\n' || c == '"' || c == ',')
            {
                needsEscaping = true;
            }
        }
        if (!needsEscaping)
        {
            result += column;
        }
        else
        {
            result += "\"";
            for (auto c : column)
            {
                if (c == '\n')
                {
                    result += "\\n";
                }
                if (c == '"')
                {
                    result += "\"\"";
                }
                else
                {
                    result += c;
                }
            }
            result += "\"";
        }
    }
    return result;
}


} // namespace extras
} // namespace carb
