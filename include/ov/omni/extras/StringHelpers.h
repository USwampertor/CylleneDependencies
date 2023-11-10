// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>

#include <carb/Types.h>

namespace omni
{
namespace extras
{

#pragma push_macro("max")
#undef max

inline std::vector<std::string> split(const std::string& s, char d, size_t count = std::numeric_limits<size_t>::max())
{
    std::vector<std::string> res;
    std::stringstream ss(s);
    size_t i = 0;
    while (ss.good() && i < count)
    {
        std::string tmp;
        if (i == count - 1)
        {
            std::getline(ss, tmp, (char)0);
        }
        else
        {
            std::getline(ss, tmp, d);
        }

        if (!tmp.empty())
        {
            res.push_back(tmp);
        }
        i++;
    }
    return res;
}

#pragma pop_macro("max")

inline bool endsWith(const std::string& str, const std::string& ending)
{
    if (ending.size() > str.size())
    {
        return false;
    }
    return std::equal(ending.rbegin(), ending.rend(), str.rbegin());
}

inline bool startsWith(const std::string& str, const std::string& prefix)
{
    return (str.length() >= prefix.length() && str.compare(0, prefix.size(), prefix) == 0);
}

inline void toLower(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](char c) { return (char)std::tolower(c); });
}

inline bool stringToInteger(const std::string& str, int32_t& outResult)
{
    char* numericEnd;

    uint32_t val = (int32_t)strtoll(str.c_str(), &numericEnd, 0);
    if (*numericEnd == '\0')
    {
        outResult = val;
        return true;
    }

    double fVal = strtod(str.c_str(), &numericEnd);
    if (*numericEnd == '\0')
    {
        outResult = static_cast<int32_t>(fVal);
        return true;
    }

    return false;
}


constexpr const char* const kInt2Delimiters = ",x";
inline bool stringToInt2(const std::string& str, carb::Int2& outResult, const char* delims = kInt2Delimiters)
{
    // try splitting by different delimeters

    for (int delimIndex = 0; delims[delimIndex] != 0; delimIndex++)
    {
        auto delim = delims[delimIndex];
        auto pathAndOther = split(str, delim);

        if (pathAndOther.size() == 2)
        {
            int32_t x, y;

            if (extras::stringToInteger(pathAndOther[0], x) && extras::stringToInteger(pathAndOther[1], y))
            {
                outResult = { x, y };
                return true;
            }
        }
    }

    return false;
}

constexpr const char* const kTrimCharsDefault = " \t\n\r\f\v";

inline std::string& rtrim(std::string& s, const char* t = kTrimCharsDefault)
{
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

inline std::string& ltrim(std::string& s, const char* t = kTrimCharsDefault)
{
    s.erase(0, s.find_first_not_of(t));
    return s;
}

inline std::string& trim(std::string& s, const char* t = kTrimCharsDefault)
{
    return ltrim(rtrim(s, t), t);
}

inline std::string trimCopy(const std::string& s, const char* t = kTrimCharsDefault)
{
    std::string copy = s;
    return trim(copy, t);
}

inline void replaceAll(std::string& str, const std::string& subStr, const std::string& replaceWith)
{
    size_t pos = str.find(subStr);
    while (pos != std::string::npos)
    {
        str.replace(pos, subStr.size(), replaceWith);
        pos = str.find(subStr, pos + replaceWith.size());
    }
}

/**
 * compare two strings ignoring case
 */
inline bool stringCompareCaseInsensitive(const std::string& str1, const std::string& str2)
{
    return ((str1.size() == str2.size()) && std::equal(str1.begin(), str1.end(), str2.begin(), [](char a, char b) {
                return std::tolower(a) == std::tolower(b);
            }));
}

/**
 * Compare paths for equality.
 * It will also convert characters of paths to lower case before comparison if needed (Currently on Windows only,
 * since it is case insensitive).
 */
inline bool isPathEqual(const std::string& strLeft, const std::string& strRight)
{
#if CARB_PLATFORM_WINDOWS
    if (strLeft.size() != strRight.size())
    {
        return false;
    }

    return stringCompareCaseInsensitive(strLeft, strRight);
#else
    return (strLeft == strRight);
#endif
}

} // namespace extras
} // namespace omni
