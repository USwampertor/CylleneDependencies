// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Compiler time helper functions.
 */
#pragma once

#include "../../carb/Defines.h"

#include <cstdint>

namespace omni
{
/** Namespace for various compile time helpers. */
namespace compiletime
{

/** Returns the length of the string at compile time.
 *
 *  @param[in] str  The string to calculate the length of.  This string must either be a string
 *                  literal or another `constexpr` symbol that can be evaluated at compile time.
 *                  This may not be `nullptr`.
 *  @returns The total number of elements in the string.  Note that this does not necessarily
 *           count the length in characters of a UTF-8 string properly.  It effectively counts
 *           how many entries (ie: bytes in this case) are in the string's buffer up to but
 *           not including the terminating null character.
 */
constexpr size_t strlen(const char* str)
{
    return *str ? 1 + strlen(str + 1) : 0;
}

/** Compile time std::strcmp().  Return 0 if equal, not 0 otherwise.
 *
 *  @param[in] a    The first string to compare.  This must either be a string literal or another
 *                  `constexpr` symbol that can be evaluated at compile time.  This may not be
 *                  `nullptr`.
 *  @param[in] b    The second string to compare.  This must either be a string literal or another
 *                  `constexpr` symbol that can be evaluated at compile time.  This may not be
 *                  `nullptr`.
 *  @returns `0` if the two strings match exactly.  Returns a negative value if the string @p a
 *           should be ordered before @p b alphanumerically.  Returns a positive value if the
 *           string @p a should be ordered after @p b alphanumerically.
 */
constexpr int strcmp(const char* a, const char* b)
{
    return ((*a == *b) ? (*a ? strcmp(a + 1, b + 1) : 0) : (*a - *b));
}

// compile time unit tests...
static_assert(0 == strlen(""), "compiletime::strlen logic error");
static_assert(1 == strlen("a"), "compiletime::strlen logic error");
static_assert(2 == strlen("ab"), "compiletime::strlen logic error");
static_assert(strcmp("b", "c") < 0, "compiletime::strcmp logic error");
static_assert(strcmp("b", "a") > 0, "compiletime::strcmp logic error");
static_assert(strcmp("b", "b") == 0, "compiletime::strcmp logic error");
static_assert(strcmp("", "") == 0, "compiletime::strcmp logic error");
static_assert(strcmp("", "a") < 0, "compiletime::strcmp logic error");
static_assert(strcmp("a", "") > 0, "compiletime::strcmp logic error");
static_assert(strcmp("carbonite", "carb") > 0, "compiletime::strcmp logic error");

} // namespace compiletime
} // namespace omni
