// Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Helper functions to handle matching wildcard patterns.
 */
#pragma once

#include <stddef.h>
#include <cstdint>

namespace omni
{
/** Namespace for various string helper functions. */
namespace str
{

/** Checks if a string matches a wildcard pattern.
 *
 *  @param[in] str      The string to attempt to match to the pattern @p pattern.
 *                      This may not be `nullptr`.
 *  @param[in] pattern  The wildcard pattern to match against.  This may not be
 *                      `nullptr`.  The wildcard pattern may contain '?' to match
 *                      exactly one character (any character), or '*' to match
 *                      zero or more characters.
 *  @returns `true` if the string @p str matches the pattern.  Returns `false` if
 *           the pattern does not match the pattern.
 */
inline bool matchWildcard(const char* str, const char* pattern)
{
    const char* star = nullptr;
    const char* s0 = str;
    const char* s1 = s0;
    const char* p = pattern;
    while (*s0)
    {
        // Advance both pointers when both characters match or '?' found in pattern
        if ((*p == '?') || (*p == *s0))
        {
            s0++;
            p++;
            continue;
        }

        // * found in pattern, save index of *, advance a pattern pointer
        if (*p == '*')
        {
            star = p++;
            s1 = s0;
            continue;
        }

        // Current characters didn't match, consume character in string and rewind to star pointer in the pattern
        if (star)
        {
            p = star + 1;
            s0 = ++s1;
            continue;
        }

        // Characters do not match and current pattern pointer is not star
        return false;
    }

    // Skip remaining stars in pattern
    while (*p == '*')
    {
        p++;
    }

    return !*p; // Was whole pattern matched?
}

/** Attempts to match a string to a set of wildcard patterns.
 *
 *  @param[in] str              The string to attempt to match to the pattern @p pattern.
 *                              This may not be `nullptr`.
 *  @param[in] patterns         An array of patterns to attempt to match the string @p str
 *                              to.  Each pattern in this array has the same format as the
 *                              pattern in @ref matchWildcard().  This may not be `nullptr`.
 *  @param[in] patternsCount    The total number of wildcard patterns in @p patterns.
 *  @returns The pattern that the test string @p str matched to if successful.  Returns
 *           `nullptr` if the test string did not match any of the patterns.
 */
inline const char* matchWildcards(const char* str, const char* const* patterns, size_t patternsCount)
{
    for (size_t i = 0; i < patternsCount; ++i)
    {
        if (matchWildcard(str, patterns[i]))
        {
            return patterns[i];
        }
    }

    return nullptr;
}

/** Tests whether a string is potentially a wildcard pattern.
 *
 *  @param[in] pattern  The pattern to test as a wildcard.  This will be considered a wildcard
 *                      if it contains the special wildcard characters '*' or '?'.  This may not
 *                      be nullptr.
 *  @returns `true` if the pattern is likely a wildcard string.  Returns `false` if the pattern
 *           does not contain any of the special wildcard characters.
 */
inline bool isWildcardPattern(const char* pattern)
{
    for (const char* p = pattern; p[0] != 0; p++)
    {
        if (*p == '*' || *p == '?')
            return true;
    }

    return false;
}

} // namespace str
} // namespace omni
