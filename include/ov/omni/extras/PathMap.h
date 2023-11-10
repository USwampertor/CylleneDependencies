// Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief A set of helper templates to provide platform specific behavior around path handling.
 */
#pragma once

#include "../core/Platform.h"
#include "../../carb/extras/Utf8Parser.h"
#include "../../carb/CarbWindows.h"

#include <map>
#include <unordered_map>
#if !OMNI_PLATFORM_WINDOWS
#    include <algorithm>
#endif


namespace omni
{
/** common namespace for extra helper functions and classes. */
namespace extras
{

#if OMNI_PLATFORM_WINDOWS
/** Custom hash functor for a case sensitive or insensitive hash based on the OS.  On Windows
 *  this allows the keys with any casing to hash to the same bucket for lookup.  This reimplements
 *  the std::hash<std::string> FNV1 functor except that it first lower-cases each character.  This
 *  intentionally avoids allocating a new string that is lower-cased for performance reasons.
 *  On Linux, this just uses std::hash<> directly which implements a case sensitive hash.
 *
 *  On Windows, note that this uses the Win32 function LCMapStringW() to do the lower case
 *  conversion.  This is done because functions such as towlower() do not properly lower case
 *  codepoints outside of the ASCII range unless the current codepage for the calling thread
 *  is set to one that specifically uses those extended characters.  Since changing the codepage
 *  for the thread is potentially destructive, we need to use another way to lower case
 *  the string's codepoints for hashing.
 */
struct PathHash
{
    /** Accumulates a buffer of bytes into an FNV1 hash.
     *
     *  @param[in] value    The initial hash value to accumulate onto.  For the first call
     *                      in a sequence, this should be std::_FNV_offset_basis.
     *  @param[in] data     The buffer of data to accumulate into the hash.  This may not
     *                      be `nullptr`.
     *  @param[in] bytes    The total number of bytes to accumulate into the hash.
     *  @returns The accumulated hash value.  This value is suitable to pass back into this
     *           function in the @p value parameter for the next buffer being accumulated.
     */
    size_t accumulateHash(size_t value, const void* data, size_t bytes) const noexcept
    {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(data);

        for (size_t i = 0; i < bytes; i++)
        {
            value ^= ptr[i];
            value *= std::_FNV_prime;
        }

        return value;
    }

    /** Case insensitive string hashing operator.
     *
     *  @param[in] key  The key string to be hashed.  This may be any length and contain any
     *                  UTF-8 codepoints.  This may be empty.
     *  @returns The calculated FNV1 hash of the given string as though it had been converted to
     *           all lower case first.  This will ensure that hashes match for the same string
     *           content excluding case.
     */
    size_t operator()(const std::string& key) const noexcept
    {
        carb::extras::Utf8Iterator it(key.c_str());
        carb::extras::Utf8Iterator::CodePoint cp;
        size_t hash = std::_FNV_offset_basis;
        size_t count;
        int result;


        while (it)
        {
            cp = *it;
            it++;

            count = carb::extras::Utf8Parser::encodeUtf16CodePoint(cp, &cp);
            result = LCMapStringW(CARBWIN_LOCALE_INVARIANT, CARBWIN_LCMAP_LOWERCASE, reinterpret_cast<WCHAR*>(&cp),
                                  (int)count, reinterpret_cast<WCHAR*>(&cp), 2);

            if (result != 0)
                hash = accumulateHash(hash, &cp, count * sizeof(WCHAR));
        }

        return hash;
    }
};

/** Custom comparison functor for a case insensitive comparison.  This does a simple case
 *  insensitive string comparison on the contents of each string's buffer.  The return
 *  value indicates the ordering of the two string inputs.
 *
 *  Note that this uses the Win32 function LCMapStringW() to do the lower case conversion.
 *  This is done because functions such as towlower() do not properly lower case codepoints
 *  outside of the ASCII range unless the current codepage for the calling thread is set
 *  to one that specifically uses those extended characters.  Since changing the codepage
 *  for the thread is potentially destructive, we need to use another way to lower case
 *  the strings' codepoints for comparison.
 */
struct PathCompare
{
    /** Case insensitive string comparison operator.
     *
     *  @param[in] left     The first string to compare.  This may be of any length and contain
     *                      any UTF-8 codepoint.
     *  @param[in] right    The second string to compare.  This may be of any length and contain
     *                      any UTF-8 codepoint.
     *  @returns `0` if the two strings are equal disregarding case.
     *  @returns A negative value if the first string should be lexicographical ordered before
     *           the second string disregarding case.
     *  @returns A positive value if the first string should be lexicographical ordered after
     *           the second string disregarding case.
     */
    int32_t operator()(const std::string& left, const std::string& right) const noexcept
    {
        carb::extras::Utf8Iterator itLeft(left.c_str());
        carb::extras::Utf8Iterator itRight(right.c_str());
        carb::extras::Utf8Iterator::CodePoint l;
        carb::extras::Utf8Iterator::CodePoint r;
        size_t countLeft;
        size_t countRight;
        int resultLeft;
        int resultRight;


        // walk the strings and compare the lower case versions of each codepoint.
        while (itLeft && itRight)
        {
            l = *itLeft;
            r = *itRight;
            itLeft++;
            itRight++;

            countLeft = carb::extras::Utf8Parser::encodeUtf16CodePoint(l, &l);
            countRight = carb::extras::Utf8Parser::encodeUtf16CodePoint(r, &r);

            resultLeft = LCMapStringW(CARBWIN_LOCALE_INVARIANT, CARBWIN_LCMAP_LOWERCASE, reinterpret_cast<WCHAR*>(&l),
                                      (int)countLeft, reinterpret_cast<WCHAR*>(&l), 2);
            resultRight = LCMapStringW(CARBWIN_LOCALE_INVARIANT, CARBWIN_LCMAP_LOWERCASE, reinterpret_cast<WCHAR*>(&r),
                                       (int)countRight, reinterpret_cast<WCHAR*>(&r), 2);

            if (l != r)
                return l - r;
        }

        // the strings only match if both ended at the same point and all codepoints matched.
        if (!itLeft && !itRight)
            return 0;

        if (!itLeft)
            return -1;

        return 1;
    }
};

/** Custom greater-than functor for a case insensitive map lookup.  This does a simple case
 *  insensitive string comparison on the contents of each string's buffer.
 */
struct PathGreater
{
    bool operator()(const std::string& left, const std::string& right) const noexcept
    {
        PathCompare compare;
        return compare(left, right) > 0;
    }
};

/** Custom less-than functor for a case insensitive map lookup.  This does a simple case
 *  insensitive string comparison on the contents of each string's buffer.
 */
struct PathLess
{
    bool operator()(const std::string& left, const std::string& right) const noexcept
    {
        PathCompare compare;
        return compare(left, right) < 0;
    }
};

/** Custom equality functor for a case insensitive map lookup.  This does a simple case
 *  insensitive string comparison on the contents of each string's buffer.
 */
struct PathEqual
{
    bool operator()(const std::string& left, const std::string& right) const noexcept
    {
        // the two strings are different lengths -> they cannot possibly match => fail.
        if (left.length() != right.length())
            return false;

        PathCompare compare;
        return compare(left, right) == 0;
    }
};


#else
/** Custom hash functor for a case sensitive or insensitive hash based on the OS.  On Windows
 *  this allows the keys with any casing to hash to the same bucket for lookup.  This reimplements
 *  the std::hash<std::string> FNV1 functor except that it first lower-cases each character.  This
 *  intentionally avoids allocating a new string that is lower-cased for performance reasons.
 *  On Linux, this just uses std::hash<> directly which implements a case sensitive hash.
 *
 *  On Windows, note that this uses the Win32 function LCMapStringW() to do the lower case
 *  conversion.  This is done because functions such as towlower() do not properly lower case
 *  codepoints outside of the ASCII range unless the current codepage for the calling thread
 *  is set to one that specifically uses those extended characters.  Since changing the codepage
 *  for the thread is potentially destructive, we need to use another way to lower case
 *  the string's codepoints for hashing.
 */
using PathHash = std::hash<std::string>;

/** Custom greater-than functor for a case sensitive or insensitive map lookup.  On Windows,
 *  this does a simple case insensitive string comparison on the contents of each string's buffer.
 *  On Linux, this performs a case sensitive string comparison.
 */
using PathGreater = std::greater<std::string>;

/** Custom less-than functor for a case sensitive or insensitive map lookup.  On Windows, this does
 *  a simple case insensitive string comparison on the contents of each string's buffer.  On
 *  Linux, this performs a case sensitive string comparison.
 */
using PathLess = std::less<std::string>;

/** Custom equality functor for a case sensitive or insensitive map lookup.  On Windows, this does
 *  a simple case insensitive string comparison on the contents of each string's buffer.  On
 *  Linux, this performs a case sensitive string comparison.
 */
using PathEqual = std::equal_to<std::string>;
#endif

/** @brief A map to store file paths and associated data according to local OS rules.
 *
 *  Templated type to use to create a map from a file name or file path to another object.
 *  The map implementation is based on the std::map<> implementation.  This will treat the
 *  key as though it is a file path on the local system - on Windows the comparisons will
 *  be case insensitive while on Linux they will be case sensitive.  This is also suitable
 *  for storing environment variables since they are also treated in a case insensitive manner
 *  on Windows and case insensitive on Linux.
 *
 *  @tparam T       The type for the map to contain as the value for each path name key.
 *  @tparam Compare The string comparison functor to use when matching path name keys.
 *
 *  @note A std::map<> object is usually implemented as a red-black tree and will take on
 *        that algorithm's performance and storage characteristics.  Please consider this
 *        when choosing a container type.
 */
template <typename T, class Compare = PathLess>
using PathMap = std::map<std::string, T, Compare>;

/** @brief An unordered map to store file paths and associated data according to local OS rules.
 *
 *  Templated type to use to create an unordered map from a file name or file path to another
 *  object.  The map implementation is based on the std::unordered_map<> implementation.  This
 *  will treat the key as though it is a file path on the local system - on Windows the
 *  comparisons will be case insensitive while on Linux they will be case sensitive.  This
 *  is also suitable for storing environment variables since they are also treated in a case
 *  insensitive manner on Windows and case insensitive on Linux.
 *
 *  @tparam T           The type for the map to contain as the value for each path name key.
 *  @tparam Hash        The string hashing functor to use to generate hash values for each path
 *                      name key.
 *  @tparam KeyEqual    The string comparison functor to use to test if two path name keys are
 *                      equal
 *
 *  @note A std::unordered_map<> object is usually implemented as a hash table of lists or trees
 *        and will take on that algorithm's performance and storage characteristics.  Please
 *        consider this when choosing a container type.
 */
template <typename T, class Hash = PathHash, class KeyEqual = PathEqual>
using UnorderedPathMap = std::unordered_map<std::string, T, Hash, KeyEqual>;

} // namespace extras
} // namespace omni
