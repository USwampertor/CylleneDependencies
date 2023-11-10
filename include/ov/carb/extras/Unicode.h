// Copyright (c) 2018-2022, NVIDIA CORPORATION. All rights reserved.
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

#include <cstdint>
#include <string>
#if CARB_PLATFORM_LINUX
#    include <iconv.h>
#else
#    include <codecvt>
#    include <locale>
#endif


namespace carb
{
namespace extras
{
/**
 * Collection of Unicode conversion utilities.
 *
 * UTF-8 encoding is used throughout this code base. However, there will be cases when you interact with OS (Windows)
 * or third party libraries (like glfw) where you need to convert to/from another representation. In those cases use the
 * conversion utilities offered here - *don't roll your own.* We know it's tempting to use std::codecvt but it's not
 * portable to all the operating systems we support (Tegra) and it will throw exceptions on range errors (which are
 * surprisingly common) if you use the default constructors (which is what everybody does).
 *
 * If you need a facility that isn't offered here then please add it to this collection. Read more about the
 * Unicode strategy employed in this code base at docs/Unicode.md.
 */

/**
 * Failure message returned from convertUtf32ToUtf8 code point function, usually because of invalid input code point
 */
const std::string kCodePointToUtf8Failure = "[?]";

/** Flags to alter the behavior of convertUtf32ToUtf8(). */
using ConvertUtf32ToUtf8Flags = uint32_t;

/** When this flag is passed to convertUtf32ToUtf8(), the string returned on
 *  conversion failure will be @ref kCodePointToUtf8Failure.
 *  If this is flag is not passed, the UTF-8 encoding of U+FFFD (the Unicode
 *  replacement character) will be returned on conversion failure.
 */
constexpr ConvertUtf32ToUtf8Flags fUseLegacyFailureString = 0x01;

/**
 * Convert a UTF-32 code point to UTF-8 encoded string
 *
 * @param utf32CodePoint The code point to convert, in UTF-32 encoding
 * @param flags These alter the behavior of this function.
 * @return The UTF-8 encoded version of utf32CodePoint.
 * @return If the codepoint could not be converted to UTF-8, the returned value
 *         depends on @p flags.
 *         If @p flags contains @ref fUseLegacyFailureString, then @ref
 *         kCodePointToUtf8Failure will be returned; otherwise, U+FFFD encoded
 *         in UTF-8 is returned.
 */
inline std::string convertUtf32ToUtf8(uint32_t codePointUtf32, ConvertUtf32ToUtf8Flags flags = fUseLegacyFailureString)
{
    char32_t u32Str[] = { codePointUtf32, '\0' };
    std::string out = convertUtf32StringToUtf8(u32Str);

    // if conversion fails, the string will just end early.
    if (out.empty() && codePointUtf32 != 0)
    {
        return (flags & fUseLegacyFailureString) != 0 ? kCodePointToUtf8Failure : u8"\uFFFD";
    }

    return out;
}

#if CARB_PLATFORM_WINDOWS

/** A value that was returned by a past version of convertWideToUtf8() on failure.
 *  Current versions will insert U+FFFD on characters that failed to parse.
 */
const std::string kUnicodeToUtf8Failure = "[failure-converting-to-utf8]";

/** A value that was returned by a past version of convertUtf8ToWide() on failure.
 *  Current versions will insert U+FFFD on characters that failed to parse.
 */
const std::wstring kUnicodeToWideFailure = L"[failure-converting-to-wide]";


/**
 * Converts a Windows wide string to UTF-8 string
 * Do not use this function for Windows file path conversion! Instead, use extras::convertWindowsToCarbonitePath(const
 * std::wstring& pathW) function for proper slashes handling and long file path support.
 *
 * @param wide Input string to convert, in Windows UTF16 encoding.
 * @note U+FFFD is returned if an invalid Unicode sequence is encountered.
 */
inline std::string convertWideToUtf8(const wchar_t* wide)
{
    return convertWideStringToUtf8(wide);
}

inline std::string convertWideToUtf8(const std::wstring& wide)
{
    return convertWideStringToUtf8(wide);
}

/**
 * Converts a UTF-8 encoded string to Windows wide string
 * Do not use this function for Windows file path conversion! Instead, use extras::convertCarboniteToWindowsPath(const
 * std::string& path) function for proper slashes handling and long file path support.
 *
 * @param utf8 Input string to convert, in UTF-8 encoding.
 * @note U+FFFD is returned if an invalid Unicode sequence is encountered.
 */
inline std::wstring convertUtf8ToWide(const char* utf8)
{
    return convertUtf8StringToWide(utf8);
}

inline std::wstring convertUtf8ToWide(const std::string& utf8)
{
    return convertUtf8StringToWide(utf8);
}

class LocaleWrapper
{
public:
    LocaleWrapper(const char* localeName)
    {
        locale = _create_locale(LC_ALL, localeName);
    }

    ~LocaleWrapper()
    {
        _free_locale(locale);
    }

    _locale_t getLocale()
    {
        return locale;
    }

private:
    _locale_t locale;
};

inline auto _getSystemDefaultLocale()
{
    static LocaleWrapper s_localeWrapper("");
    return s_localeWrapper.getLocale();
}

/**
 * Performs a case-insensitive comparison of wide strings in Unicode (Windows native) using system default locale.
 *
 * @param string1 First input string.
 * @param string2 Second input string.
 * @return < 0 if string1 less than string2,
 *         0 if string1 identical to string2,
 *         > 0 if string1 greater than string2,
 *         INT_MAX on error.
 */
inline int compareWideStringsCaseInsensitive(const wchar_t* string1, const wchar_t* string2)
{
    return _wcsicmp_l(string1, string2, _getSystemDefaultLocale());
}

inline int compareWideStringsCaseInsensitive(const std::wstring& string1, const std::wstring& string2)
{
    return compareWideStringsCaseInsensitive(string1.c_str(), string2.c_str());
}

/**
 * Converts wide string in Unicode (Windows native) to uppercase using system default locale.
 *
 * @param string Input string.
 * @return Uppercase string.
 */
inline std::wstring convertWideStringToUppercase(const std::wstring& string)
{
    std::wstring result = string;
    _wcsupr_s_l(&result[0], result.size() + 1, _getSystemDefaultLocale());
    return result;
}

/**
 * Converts wide string in Unicode (Windows native) to uppercase using system default locale, in-place version.
 *
 * @param string Input and output string.
 */
inline void convertWideStringToUppercaseInPlace(std::wstring& string)
{
    _wcsupr_s_l(&string[0], string.size() + 1, _getSystemDefaultLocale());
}

/**
 * Converts wide string in Unicode (Windows native) to lowercase using system default locale.
 *
 * @param string Input string.
 * @return Lowercase string.
 */
inline std::wstring convertWideStringToLowercase(const std::wstring& string)
{
    std::wstring result = string;
    _wcslwr_s_l(&result[0], result.size() + 1, _getSystemDefaultLocale());
    return result;
}

/**
 * Converts wide string in Unicode (Windows native) to lowercase using system default locale, in-place version.
 *
 * @param string Input and output string.
 */
inline void convertWideStringToLowercaseInPlace(std::wstring& string)
{
    _wcslwr_s_l(&string[0], string.size() + 1, _getSystemDefaultLocale());
}

#endif
} // namespace extras
} // namespace carb
