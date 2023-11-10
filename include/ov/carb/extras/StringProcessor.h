// Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "EnvironmentVariable.h"

#include "../logging/Log.h"

namespace carb
{
namespace extras
{

/**
 * Search for env vars like ${SOME_VAR} in a string and replace them with actual env var value
 */
inline std::string replaceEnvironmentVariables(const std::string& text)
{
    // At least '${}' is needed for that function to do processing
    if (text.size() < 3)
        return text;

    std::string result;
    result.reserve(text.size());

    size_t lastPatternIndex = 0;
    for (size_t i = 0; i < text.size() - 1; i++)
    {
        if (text[i] == '$' && text[i + 1] == '{')
        {
            result.append(text.substr(lastPatternIndex, i - lastPatternIndex));

            size_t pos = text.find('}', i + 1);
            if (pos != std::string::npos)
            {
                const std::string envVarName = text.substr(i + 2, pos - i - 2);
                std::string var;
                if (!envVarName.empty() && extras::EnvironmentVariable::getValue(envVarName.c_str(), var))
                {
                    result.append(var);
                }
                else
                {
                    CARB_LOG_ERROR("Environment variable: %s was not found.", envVarName.c_str());
                }

                lastPatternIndex = pos + 1;
                i = pos;
            }
        }
    };

    result.append(text.substr(lastPatternIndex, text.size() - lastPatternIndex));

    return result;
}
} // namespace extras
} // namespace carb
