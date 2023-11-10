// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//! @brief Implementation of utilities for \ref carb::tokens::ITokens.
#pragma once

#include "../InterfaceUtils.h"
#include "../logging/Log.h"
#include "ITokens.h"

#include <string>
#include <algorithm>

namespace carb
{
namespace tokens
{

/**
 * Helper for resolving a token string. The resolve result (resolve code) is placed in the optional parameter.
 *
 * @param tokens tokens interface (passing a null pointer will result in an error)
 * @param str string for token resolution (passing a null pointer will result in an error)
 * @param resolveFlags flags that modify token resolution process
 * @param resolveResult optional parameter for receiving resulting resolve code
 *
 * @return true if the operation was successful false otherwise
 */
inline std::string resolveString(const ITokens* tokens,
                                 const char* str,
                                 ResolveFlags resolveFlags = kResolveFlagNone,
                                 ResolveResult* resolveResult = nullptr)
{
    // Defaulting to an error result thus it's possible to just log an error message and return an empty string if
    // anything goes wrong
    if (resolveResult)
    {
        *resolveResult = ResolveResult::eFailure;
    }

    if (!tokens)
    {
        CARB_LOG_ERROR("Couldn't acquire ITokens interface.");
        return std::string();
    }
    if (!str)
    {
        CARB_LOG_ERROR("Can't resolve a null token string.");
        return std::string();
    }

    const size_t strLen = std::strlen(str);

    ResolveResult resResult;
    size_t resolvedStringSize = tokens->calculateDestinationBufferSize(
        str, strLen, StringEndingMode::eNoNullTerminator, resolveFlags, &resResult);

    if (resResult == ResolveResult::eFailure)
    {
        CARB_LOG_ERROR("Couldn't calculate required buffer size for token resolution of string: %s", str);
        return std::string();
    }

    // Successful resolution to an empty string
    if (resolvedStringSize == 0)
    {
        if (resolveResult)
        {
            *resolveResult = ResolveResult::eSuccess;
        }
        return std::string();
    }

    // C++11 guarantees that strings are continuous in memory
    std::string resolvedString;
    resolvedString.resize(resolvedStringSize);

    const ResolveResult resolveResultLocal =
        tokens->resolveString(str, strLen, &resolvedString.front(), resolvedString.size(),
                              StringEndingMode::eNoNullTerminator, resolveFlags, nullptr);

    if (resolveResultLocal != ResolveResult::eSuccess)
    {
        CARB_LOG_ERROR("Couldn't successfully resolve provided string: %s", str);
        return std::string();
    }

    if (resolveResult)
    {
        *resolveResult = ResolveResult::eSuccess;
    }

    return resolvedString;
}

/**
 * A helper function that escapes necessary symbols in the provided string so that they won't be recognized as related
 * to token parsing
 * @param str a string that requires preprocessing to evade the token resolution (a string provided by a user or some
 * other data that must not be a part of token resolution)
 * @return a string with necessary modification so it won't participate in token resolution
 */
inline std::string escapeString(const std::string& str)
{
    constexpr char kSpecialChar = '$';
    const size_t countSpecials = std::count(str.begin(), str.end(), kSpecialChar);
    if (!countSpecials)
    {
        return str;
    }

    std::string result;
    result.reserve(str.length() + countSpecials);

    for (char curChar : str)
    {
        result.push_back(curChar);
        if (curChar == kSpecialChar)
        {
            result.push_back(kSpecialChar);
        }
    }
    return result;
}

} // namespace tokens
} // namespace carb
