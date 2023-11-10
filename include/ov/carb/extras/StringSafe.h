// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Wrappers for libc string functions to avoid dangerous edge cases.
 */
#pragma once

// for vsnprintf() on windows
#ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#endif

#include "../Defines.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

// pyerrors.h defines vsnprintf to be _vsnprintf on Windows, which is non-standard and breaks things. In the more modern
// C++ that we're using, std::vsnprintf does what we want, so get rid of pyerrors.h's badness here.  As a service to
// others, we'll also undefine pyerrors.h's `snprintf` symbol.
#if defined(Py_ERRORS_H) && CARB_PLATFORM_WINDOWS
#    undef vsnprintf
#    undef snprintf
#endif

namespace carb
{
namespace extras
{

/** Compare two strings in a case sensitive manner.
 *
 *  @param[in] str1             The first string to compare.  This may not be `nullptr`.
 *  @param[in] str2             The second string to compare.  This may not be `nullptr`.
 *  @returns `0` if the two strings match.
 *  @returns A negative value if `str1` should be ordered before `str2`.
 *  @returns A positive value if `str1` should be ordered after `str2`.
 */
inline int32_t compareStrings(const char* str1, const char* str2)
{
    return strcmp(str1, str2);
}

/** Compare two strings in a case insensitive manner.
 *
 *  @param[in] str1             The first string to compare.  This may not be `nullptr`.
 *  @param[in] str2             The second string to compare.  This may not be `nullptr`.
 *  @returns `0` if the two strings match.
 *  @returns A negative value if `str1` should be ordered before `str2`.
 *  @returns A positive value if `str1` should be ordered after `str2`.
 */
inline int32_t compareStringsNoCase(const char* str1, const char* str2)
{
#if CARB_PLATFORM_WINDOWS
    return _stricmp(str1, str2);
#else
    return strcasecmp(str1, str2);
#endif
}

/**
 * Check if two memory regions overlaps.
 *
 * @param[in] ptr1 pointer to a first memory region.
 * @param[in] size1 size in bytes of the first memory region.
 * @param[in] ptr2 pointer to a second memory region.
 * @param[in] size2 size in bytes of the second memory region.
 *
 * @return true if memory regions overlaps or false if they are not.
 */
inline bool isMemoryOverlap(const void* ptr1, size_t size1, const void* ptr2, size_t size2)
{
    // We assume flat memory model.
    uintptr_t addr1 = uintptr_t(ptr1);
    uintptr_t addr2 = uintptr_t(ptr2);

    if (addr1 < addr2)
    {
        if (addr2 - addr1 >= size1)
        {
            return false;
        }
    }
    else if (addr1 > addr2)
    {
        if (addr1 - addr2 >= size2)
        {
            return false;
        }
    }

    return true;
}

/**
 * Copy a string with optional truncation.
 *
 * @param[out] dstBuf pointer to a destination buffer (can be nullptr in the case if dstBufSize is zero).
 * @param[in] dstBufSize size in characters of the destination buffer.
 * @param[in] srcString pointer to a source string.
 *
 * @return a number of copied characters to the destination buffer (not including the trailing \0).
 *
 * @remark This function copies up to dstBufSize - 1 characters from the 0-terminated string srcString
 * to the buffer dstBuf and appends trailing \0 to the result. This function is guarantee that the result
 * has trailing \0 as long as dstBufSize is larger than 0.
 */
inline size_t copyStringSafe(char* dstBuf, size_t dstBufSize, const char* srcString)
{
    CARB_ASSERT(dstBuf || dstBufSize == 0);
    CARB_ASSERT(srcString);

    if (dstBufSize > 0)
    {
        // Compute length of the source string to be copied.
        size_t copyLength = strlen(srcString);

        // Check the source and destination are not overlapped.
        CARB_ASSERT(!isMemoryOverlap(dstBuf, dstBufSize, srcString, copyLength + 1));

        if (copyLength >= dstBufSize)
        {
            copyLength = dstBufSize - 1;
            memcpy(dstBuf, srcString, copyLength);
        }
        else if (copyLength > 0)
        {
            memcpy(dstBuf, srcString, copyLength);
        }
        dstBuf[copyLength] = '\0';

        return copyLength;
    }

    return 0;
}

/**
 * Copy slice of string with optional truncation.
 *
 * @param[out] dstBuf pointer to a destination buffer (can be nullptr in the case if dstBufSize is zero).
 * @param[in] dstBufSize size in characters of the destination buffer.
 * @param[in] srcString pointer to a source string (can be nullptr in the case if maxCharacterCount is zero).
 * @param[in] maxCharacterCount maximum number of characters to be copied from the source string.
 *
 * @return a number of copied characters to the destination buffer (not including the trailing \0).
 *
 * @remarks This function copies up to min(dstBufSize - 1, maxCharacterCount) characters from the source string
 * srcString to the buffer dstBuf and appends trailing \0 to the result. This function is guarantee that the result has
 * trailing \0 as long as dstBufSize is larger than 0.
 */
inline size_t copyStringSafe(char* dstBuf, size_t dstBufSize, const char* srcString, size_t maxCharacterCount)
{
    CARB_ASSERT(dstBuf || dstBufSize == 0);
    CARB_ASSERT(srcString || maxCharacterCount == 0);

    // NOTE: We don't use strncpy_s in implementation even if it's available in the system because it places empty
    // string to the destination buffer in case of truncation of source string (see the detailed description at
    // https://en.cppreference.com/w/c/string/byte/strncpy).

    // Instead, we use always our own implementation which is tolerate to any case of truncation.

    if (dstBufSize > 0)
    {
        // Compute length of the source string slice to be copied.
        size_t copyLength = (maxCharacterCount > 0) ? strnlen(srcString, CARB_MIN(dstBufSize - 1, maxCharacterCount)) : 0;

        // Check the source and destination are not overlapped.
        CARB_ASSERT(!isMemoryOverlap(dstBuf, dstBufSize, srcString, copyLength));

        if (copyLength > 0)
        {
            memcpy(dstBuf, srcString, copyLength);
        }
        dstBuf[copyLength] = '\0';

        return copyLength;
    }

    return 0;
}

/**
 * A vsnprintf wrapper that clamps the return value.
 *
 * @param[out] dstBuf pointer to a destination buffer (can be nullptr in the case if dstBufSize is zero).
 * @param[in] dstBufSize size in characters of the destination buffer.
 * @param[in] fmtString pointer to a format string (passed to the vsnprintf call).
 * @param[in] argsList arguments list
 *
 * @return a number of characters written to the destination buffer (not including the trailing \0).
 *
 * @remarks This function is intended to be used in code where an index is incremented by snprintf.
 * In the following example, if vsnprintf() were used, idx can become larger than
 * len, causing wraparound errors, but with formatStringV(), idx will never
 * become larger than len.
 *
 *               idx += formatStringV(buf, len - idx, ...);
 *               idx += formatStringV(buf, len - idx, ...);
 *
 */
inline size_t formatStringV(char* dstBuf, size_t dstBufSize, const char* fmtString, va_list argsList)
{
    CARB_ASSERT(dstBuf || dstBufSize == 0);
    CARB_ASSERT(fmtString);

    if (dstBufSize > 0)
    {
        int rc = std::vsnprintf(dstBuf, dstBufSize, fmtString, argsList);
        size_t count = size_t(rc);
        if (rc < 0)
        {
            // We assume no output in a case of I/O error.
            dstBuf[0] = '\0';
            count = 0;
        }
        else if (count >= dstBufSize)
        {
            // ANSI C always adds the null terminator, older MSVCRT versions do not.
            dstBuf[dstBufSize - 1] = '\0';
            count = (dstBufSize - 1);
        }

        return count;
    }

    return 0;
}

/**
 * A snprintf wrapper that clamps the return value.
 *
 * @param[out] dstBuf pointer to a destination buffer (can be nullptr in the case if dstBufSize is zero).
 * @param[in] dstBufSize size in characters of the destination buffer.
 * @param[in] fmtString pointer to a format string (passed to the snprintf call).
 *
 * @return a number of characters written to the destination buffer (not including the trailing \0).
 *
 * @remarks This function is intended to be used in code where an index is incremented by snprintf.
 * In the following example, if snprintf() were used, idx can become larger than
 * len, causing wraparound errors, but with formatString(), idx will never
 * become larger than len.
 *
 *               idx += formatString(buf, len - idx, ...);
 *               idx += formatString(buf, len - idx, ...);
 *
 */
inline size_t formatString(char* dstBuf, size_t dstBufSize, const char* fmtString, ...) CARB_PRINTF_FUNCTION(3, 4);

inline size_t formatString(char* dstBuf, size_t dstBufSize, const char* fmtString, ...)
{
    size_t count;
    va_list argsList;

    va_start(argsList, fmtString);
    count = formatStringV(dstBuf, dstBufSize, fmtString, argsList);
    va_end(argsList);

    return count;
}

/** Test if one string is a prefix of the other.
 *  @param[in] str    The string to test.
 *  @param[in] prefix The prefix to test on @p str.
 *  @returns `true` if @p str begins with @p prefix.
 *  @returns `false` otherwise.
 */
inline bool isStringPrefix(const char* str, const char* prefix)
{
    for (size_t i = 0; prefix[i] != '\0'; i++)
    {
        if (str[i] != prefix[i])
        {
            return false;
        }
    }
    return true;
}

} // namespace extras
} // namespace carb
