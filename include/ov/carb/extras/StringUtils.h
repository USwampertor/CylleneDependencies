// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"

#include "Utf8Parser.h"

#include <cstring>
#include <string>
#include <algorithm>
#include <cctype>


namespace carb
{
namespace extras
{

/**
 * Checks if the string begins with the given prefix.
 *
 * @param[in] str a non-null pointer to the 0-terminated string.
 * @param[in] prefix a non-null pointer to the 0-terminated prefix.
 *
 * @return true if the string begins with provided prefix, false otherwise.
 */
inline bool startsWith(const char* str, const char* prefix)
{
    CARB_ASSERT(str != nullptr);
    CARB_ASSERT(prefix != nullptr);

    if (0 == *str)
    {
        // empty string vs empty (or not) prefix
        return (0 == *prefix);
    }
    else if (0 == *prefix)
    {
        // non empty string with empty prefix
        return true;
    }

    size_t n2 = strlen(prefix);
    return (0 == strncmp(str, prefix, n2));
}

/**
 * Checks if the string begins with the given prefix.
 *
 * @param[in] str a non-null pointer to the 0-terminated string.
 * @param[in] prefix a const reference the std::string prefix.
 *
 * @return true if the string begins with provided prefix, false otherwise.
 */
inline bool startsWith(const char* str, const std::string& prefix)
{
    CARB_ASSERT(str != nullptr);

    if (0 == *str)
    {
        // empty string vs empty (or not) prefix
        return (prefix.empty());
    }
    else if (prefix.empty())
    {
        // non empty string with empty prefix
        return true;
    }

    std::string::size_type n2 = prefix.length();
    std::string::size_type n1 = strnlen(str, static_cast<size_t>(n2 + 1));
    if (n1 < n2)
    {
        // string is shorter than prefix
        return false;
    }

    CARB_ASSERT(n2 > 0 && n1 >= n2);
    return (0 == prefix.compare(0, n2, str, n2));
}

/**
 * Checks if the string begins with the given prefix.
 *
 * @param[in] str a const reference to the std::string object.
 * @param[in] prefix a non-null pointer to the 0-terminated prefix.
 *
 * @return true if the string begins with provided prefix, false otherwise.
 */
inline bool startsWith(const std::string& str, const char* prefix)
{
    CARB_ASSERT(prefix != nullptr);

    if (str.empty())
    {
        // empty string vs empty (or not) prefix
        return (0 == *prefix);
    }
    else if (0 == *prefix)
    {
        // non empty string with empty prefix
        return true;
    }

    std::string::size_type n2 = strlen(prefix);
    std::string::size_type n1 = str.length();
    if (n1 < n2)
    {
        // string is shorter than prefix
        return false;
    }

    CARB_ASSERT(n2 > 0 && n1 >= n2);
    return (0 == str.compare(0, n2, prefix));
}

/**
 * Checks if the string begins with the given prefix.
 *
 * @param[in] str a const reference to the std::string object.
 * @param[in] prefix a const reference to the std::string prefix.
 *
 * @return true if the string begins with provided prefix, false otherwise.
 */
inline bool startsWith(const std::string& str, const std::string& prefix)
{
    if (str.empty())
    {
        // empty string vs empty (or not) prefix
        return prefix.empty();
    }
    else if (prefix.empty())
    {
        // non empty string with empty prefix
        return true;
    }

    std::string::size_type n2 = prefix.length();
    std::string::size_type n1 = str.length();
    if (n1 < n2)
    {
        // string is shorter than prefix
        return false;
    }

    CARB_ASSERT(n2 > 0 && n1 >= n2);
    return (0 == str.compare(0, n2, prefix));
}

/**
 * Checks if the string ends with the given suffix.
 *
 * @param[in] str a non-null pointer to the 0-terminated string.
 * @param[in] suffix a non-null pointer to the 0-terminated suffix.
 *
 * @return true if the string ends with provided suffix, false otherwise.
 */
inline bool endsWith(const char* str, const char* suffix)
{
    CARB_ASSERT(str != nullptr);
    CARB_ASSERT(suffix != nullptr);

    if (0 == *str)
    {
        // empty string vs empty (or not) suffix
        return (0 == *suffix);
    }
    else if (0 == *suffix)
    {
        // non empty string with empty suffix
        return true;
    }

    size_t n2 = strlen(suffix);
    size_t n1 = strlen(str);
    if (n1 < n2)
    {
        // string is shorter than suffix
        return false;
    }

    CARB_ASSERT(n2 > 0 && n1 >= n2);
    return (0 == memcmp(str + n1 - n2, suffix, n2));
}

/**
 * Checks if the string ends with the given suffix.
 *
 * @param[in] str a non-null pointer to the 0-terminated string.
 * @param[in] suffix a const reference to the std::string suffix.
 *
 * @return true if the string ends with provided suffix, false otherwise.
 */
inline bool endsWith(const char* str, const std::string& suffix)
{
    CARB_ASSERT(str != nullptr);

    if (0 == *str)
    {
        // empty string vs empty (or not) suffix
        return (suffix.empty());
    }
    else if (suffix.empty())
    {
        // non empty string with empty suffix
        return true;
    }

    std::string::size_type n2 = suffix.length();
    std::string::size_type n1 = strlen(str);
    if (n1 < n2)
    {
        // string is shorter than suffix
        return false;
    }

    CARB_ASSERT(n2 > 0 && n1 >= n2);
    return (0 == suffix.compare(0, n2, str + n1 - n2, n2));
}

/**
 * Checks if the string ends with the given suffix.
 *
 * @param[in] str a const reference to the std::string object.
 * @param[in] suffix a non-null pointer to the 0-terminated suffix.
 *
 * @return true if the string ends with provided suffix, false otherwise.
 */
inline bool endsWith(const std::string& str, const char* suffix)
{
    CARB_ASSERT(suffix != nullptr);

    if (str.empty())
    {
        // empty string vs empty (or not) suffix
        return (0 == *suffix);
    }
    else if (0 == *suffix)
    {
        // non empty string with empty suffix
        return true;
    }

    std::string::size_type n2 = strlen(suffix);
    std::string::size_type n1 = str.length();
    if (n1 < n2)
    {
        // string is shorter than suffix
        return false;
    }

    CARB_ASSERT(n2 > 0 && n1 >= n2);
    return (0 == str.compare(n1 - n2, n2, suffix));
}

/**
 * Checks if the string ends with the given suffix.
 *
 * @param[in] str a const reference to the std::string object.
 * @param[in] suffix a const reference to the std::string suffix.
 *
 * @return true if the string ends with provided suffix, false otherwise.
 */
inline bool endsWith(const std::string& str, const std::string& suffix)
{
    if (str.empty())
    {
        // empty string vs empty (or not) prefix
        return suffix.empty();
    }
    else if (suffix.empty())
    {
        // non empty string with empty prefix
        return true;
    }

    std::string::size_type n2 = suffix.length();
    std::string::size_type n1 = str.length();
    if (n1 < n2)
    {
        // string is shorter than suffix
        return false;
    }

    CARB_ASSERT(n2 > 0 && n1 >= n2);
    return (0 == str.compare(n1 - n2, n2, suffix));
}

/**
 * Trims start of the provided string from the whitespace characters as classified by the
 * currently installed C locale in-place
 *
 * @param[in,out] str for trimming
 */
inline void trimStringStartInplace(std::string& str)
{
    // Note: using cast of val into `unsigned char` as std::isspace expects unsigned char values
    // https://en.cppreference.com/w/cpp/string/byte/isspace
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](std::string::value_type val) {
                  return !std::isspace(static_cast<unsigned char>(val));
              }));
}

/**
 * Trims end of the provided string from the whitespace characters as classified by the
 * currently installed C locale in-place
 *
 * @param[in,out] str for trimming
 */
inline void trimStringEndInplace(std::string& str)
{
    // Note: using cast of val into `unsigned char` as std::isspace expects unsigned char values
    // https://en.cppreference.com/w/cpp/string/byte/isspace
    str.erase(std::find_if(str.rbegin(), str.rend(),
                           [](std::string::value_type val) { return !std::isspace(static_cast<unsigned char>(val)); })
                  .base(),
              str.end());
}

/**
 * Trims start and end of the provided string from the whitespace characters as classified by the
 * currently installed C locale in-place
 *
 * @param[in,out] str for trimming
 */
inline void trimStringInplace(std::string& str)
{
    trimStringStartInplace(str);
    trimStringEndInplace(str);
}

/**
 * Creates trimmed (as classified by the currently installed C locale)
 * from the start copy of the provided string
 *
 * @param[in] str string for trimming
 *
 * @return trimmed from the start copy of the provided string
 */
inline std::string trimStringStart(std::string str)
{
    trimStringStartInplace(str);
    return str;
}

/**
 * Creates trimmed (as classified by the currently installed C locale)
 * from the end copy of the provided string
 *
 * @param[in] str string for trimming
 *
 * @return trimmed from the end copy of the provided string
 */
inline std::string trimStringEnd(std::string str)
{
    trimStringEndInplace(str);
    return str;
}

/**
 * Creates trimmed (as classified by the currently installed C locale)
 * from the start and the end copy of the provided string
 *
 * @param[in] str string for trimming
 *
 * @return trimmed from the start and the end copy of the provided string
 */
inline std::string trimString(std::string str)
{
    trimStringInplace(str);
    return str;
}

/**
 * Trims start of the provided valid utf-8 string from the whitespace characters in-place
 *
 * @param[inout] str The UTF-8 string to be trimmed.
 */
inline void trimStringStartInplaceUtf8(std::string& str)
{
    if (str.empty())
    {
        return;
    }

    Utf8Parser::CodePoint decodedCodePoint = 0;
    const Utf8Parser::CodeByte* nonWhitespacePos = str.data();

    while (true)
    {
        const Utf8Parser::CodeByte* nextPos = Utf8Parser::nextCodePoint(
            nonWhitespacePos, Utf8Parser::kNullTerminated, &decodedCodePoint, Utf8Parser::fDecodeUseDefault);

        // If walked through the whole string and non-whitespace encountered then the whole string is just whitespace
        if (!nextPos)
        {
            str.clear();
            return;
        }

        if (Utf8Parser::isSpaceCodePoint(decodedCodePoint))
        {
            // If encountered a whitespace character then proceed to the next code point
            nonWhitespacePos = nextPos;
            continue;
        }

        // Not a whitespace, stop checking
        break;
    }

    const size_t removedCharsCount = nonWhitespacePos - str.data();
    if (removedCharsCount)
    {
        str.erase(0, removedCharsCount);
    }
}

/**
 * Trims end of the provided valid utf-8 string from the whitespace characters in-place
 *
 * @param[inout] str The string to be trimmed.
 */
inline void trimStringEndInplaceUtf8(std::string& str)
{
    if (str.empty())
    {
        return;
    }

    const Utf8Parser::CodeByte* dataBufStart = str.data();
    const Utf8Parser::CodeByte* dataBufEnd = dataBufStart + str.size();
    // Walking the provided string data in codepoints in reverse
    // Note: `Utf8Parser::lastCodePoint` checks the provided data from back to front thus the search of whitespace is
    // in linear time
    Utf8Parser::CodePoint decodedCodePoint{};
    const Utf8Parser::CodeByte* curSearchPosAtEnd =
        Utf8Parser::lastCodePoint(dataBufStart, str.size(), &decodedCodePoint);
    const Utf8Parser::CodeByte* prevSearchPosAtEnd = dataBufEnd;
    while (curSearchPosAtEnd != nullptr)
    {
        if (!Utf8Parser::isSpaceCodePoint(decodedCodePoint))
        {
            // Non-space code point was found
            // Remove all symbols starting with the previous codepoint
            if (prevSearchPosAtEnd < dataBufEnd)
            {
                str.erase(prevSearchPosAtEnd - dataBufStart);
            }
            return;
        }

        prevSearchPosAtEnd = curSearchPosAtEnd;
        curSearchPosAtEnd = Utf8Parser::lastCodePoint(dataBufStart, curSearchPosAtEnd - dataBufStart, &decodedCodePoint);
    }

    // Either the whole string consists from space characters or an invalid sequence was encountered
    str.clear();
}

/**
 * Trims start and end of the provided valid utf-8 string from the whitespace characters in-place
 *
 * @param[inout] str The UTF-8 string to be trimmed.
 */
inline void trimStringInplaceUtf8(std::string& str)
{
    trimStringStartInplaceUtf8(str);
    trimStringEndInplaceUtf8(str);
}

/**
 * Creates trimmed from the start copy of the provided valid utf-8 string
 *
 * @param[in] str The UTF-8 string to be trimmed.
 *
 * @return trimmed from the start copy of the provided string
 */
inline std::string trimStringStartUtf8(std::string str)
{
    trimStringStartInplaceUtf8(str);
    return str;
}

/**
 * Creates trimmed from the end copy of the provided valid utf-8 string
 *
 * @param[in] str The UTF-8 string to be trimmed.
 *
 * @return trimmed from the end copy of the provided string
 */
inline std::string trimStringEndUtf8(std::string str)
{
    trimStringEndInplaceUtf8(str);
    return str;
}

/**
 * Creates trimmed from the start and the end copy of the provided valid utf-8 string
 *
 * @param[in] str The UTF-8 string to be trimmed.
 *
 * @return trimmed from the start and the end copy of the provided string
 */
inline std::string trimStringUtf8(std::string str)
{
    trimStringInplaceUtf8(str);
    return str;
}

} // namespace extras
} // namespace carb
