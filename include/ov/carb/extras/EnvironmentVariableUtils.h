// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief Provides a helper class for setting and restoring environment variables.
//
#pragma once

#include "../logging/Log.h"

#include <map>
#include <vector>

namespace carb
{
namespace extras
{

/**
 * Resolves environment variable references in the source buffer
 *
 * @param sourceBuffer buffer of text to resolve
 * @param sourceBufferLength length of source buffer
 * @param envVariables map of name/value pairs of environment variables
 * @return The resolved string is the first member of the returned pair
 *         the second bool member indicates if the resolve was without errors
 */
inline std::pair<std::string, bool> resolveEnvVarReferences(const char* sourceBuffer,
                                                            size_t sourceBufferLength,
                                                            const std::map<std::string, std::string>& envVariables) noexcept
{
    std::pair<std::string, bool> result{};

    if (!sourceBuffer)
    {
        CARB_LOG_ERROR("Null source buffer for resolving environment variable references.");
        return result;
    }

    if (CARB_UNLIKELY(sourceBufferLength == 0))
    {
        result.second = true;
        return result;
    }

    // Collecting the env vars references
    struct EnvVarReference
    {
        const char* referenceStart;
        const char* referenceEnd;
    };

    std::vector<EnvVarReference> envReferences;

    constexpr const char kEnvVarPrefix[] = "$env{";
    constexpr const size_t kEnvVarPrefixLength = carb::countOf(kEnvVarPrefix) - 1;
    constexpr const char kEnvVarPostfix = '}';
    constexpr const size_t kEnvVarPostfixLength = 1;

    const char* endEnvPos = sourceBuffer;
    for (const char* curReferencePos = std::strstr(endEnvPos, kEnvVarPrefix); curReferencePos != nullptr;
         curReferencePos = std::strstr(endEnvPos, kEnvVarPrefix))
    {
        endEnvPos = std::strchr(curReferencePos, kEnvVarPostfix);
        if (!endEnvPos)
        {
            CARB_LOG_ERROR("Couldn't find the end of the environment variable reference: '%s'", curReferencePos);
            return result;
        }
        endEnvPos += kEnvVarPostfixLength;
        envReferences.emplace_back(EnvVarReference{ curReferencePos, endEnvPos });
    }

    if (envReferences.empty())
    {
        result.first = std::string(sourceBuffer, sourceBufferLength);
        result.second = true;
        return result;
    }

    // return true if resolved all the env variables in the provided buffer
    auto envVarResolver = [&](const char* buffer, const char* bufferEnd, size_t firstRefIndexToTry,
                              size_t& firstUnprocessedRefIndex, std::string& resolvedString) -> bool {
        const char* curBufPos = buffer;
        const size_t envReferencesSize = envReferences.size();
        size_t curRefIndex = firstRefIndexToTry;
        bool resolved = true;
        while (curRefIndex < envReferencesSize && envReferences[curRefIndex].referenceStart < buffer)
        {
            ++curRefIndex;
        }
        for (; curBufPos < bufferEnd && curRefIndex < envReferencesSize; ++curRefIndex)
        {
            const EnvVarReference& curRef = envReferences[curRefIndex];
            if (curRef.referenceEnd > bufferEnd)
            {
                break;
            }

            // adding the part before env variable
            if (curBufPos < curRef.referenceStart)
            {
                resolvedString.append(curBufPos, curRef.referenceStart);
            }

            // resolving the env variable
            std::string varName(curRef.referenceStart + kEnvVarPrefixLength, curRef.referenceEnd - kEnvVarPostfixLength);
            if (CARB_LIKELY(!varName.empty()))
            {
                auto searchResult = envVariables.find(varName);
                if (searchResult != envVariables.end())
                {
                    resolvedString.append(searchResult->second);
                }
                else
                {
                    // Couldn't resolve the reference
                    resolved = false;
                }
            }
            else
            {
                CARB_LOG_WARN("Found environment variable reference with empty name.");
            }

            curBufPos = curRef.referenceEnd;
        }

        firstUnprocessedRefIndex = curRefIndex;

        // adding the rest of the string
        if (curBufPos < bufferEnd)
        {
            resolvedString.append(curBufPos, bufferEnd);
        }

        return resolved;
    };

    // Check for the Elvis operator
    constexpr const char* const kElvisOperator = "?:";
    constexpr size_t kElvisOperatorLen = 2;

    const char* const elvisPos = ::strstr(sourceBuffer, kElvisOperator);

    const size_t leftPartSize = elvisPos ? elvisPos - sourceBuffer : sourceBufferLength;

    size_t rightPartEnvRefFirstIndex = 0;
    const bool allResolved =
        envVarResolver(sourceBuffer, sourceBuffer + leftPartSize, 0, rightPartEnvRefFirstIndex, result.first);

    // switch to using the right part if we couldn't resolve everything on the left part of the Elvis op
    if (elvisPos && !allResolved)
    {
        result.first.clear();
        envVarResolver(elvisPos + kElvisOperatorLen, sourceBuffer + sourceBufferLength, rightPartEnvRefFirstIndex,
                       rightPartEnvRefFirstIndex, result.first);
    }
    result.second = true;
    return result;
}

} // namespace extras
} // namespace carb
