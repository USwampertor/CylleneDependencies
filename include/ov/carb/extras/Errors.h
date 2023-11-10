// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"

#include "../logging/Log.h"

#if CARB_PLATFORM_WINDOWS
#    include "../CarbWindows.h"
#    include "Unicode.h"

#    include <memory>
#elif CARB_POSIX
// Nothing needed for now
#else
CARB_UNSUPPORTED_PLATFORM();
#endif
#include <cerrno>
#include <string.h>
#include <string>

namespace carb
{
namespace extras
{

using ErrnoType = std::remove_reference<decltype(errno)>::type;

#if CARB_PLATFORM_WINDOWS
using WinApiErrorType = DWORD;
#endif

/**
 * Returns the last value of errno.
 *
 * @return the last value of errno.
 */
inline ErrnoType getLastErrno()
{
    return errno;
}

/**
 * Function translates the errno code into a text message.
 *
 * @param errorCode the error code from the errno
 * @return text message corresponding to the error code or an empty string if there is no error
 */
inline std::string convertErrnoToMessage(ErrnoType errorCode)
{
    if (!errorCode)
    {
        return std::string();
    }

    char buffer[1024];
    const size_t bufferSize = carb::countOf(buffer);

#if CARB_PLATFORM_WINDOWS
    const errno_t getTextError = strerror_s(buffer, bufferSize, errorCode);
    if (getTextError)
    {
        static_assert(std::is_same<errno_t, int>::value, "Incorrect format specifier for outputting an error code");

        CARB_LOG_ERROR("%s couldn't translate error code, `strerror_s` error code is '%i'", __func__, getTextError);
        return std::to_string(errorCode);
    }
    return buffer;

#elif CARB_PLATFORM_MACOS || CARB_PLATFORM_LINUX

    // strerror_r implementation switch
#    if CARB_PLATFORM_MACOS || ((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !_GNU_SOURCE)
    // XSI-compliant strerror_r
    int formattingResult = strerror_r(errorCode, buffer, bufferSize);
    if (formattingResult)
    {
        CARB_LOG_ERROR("%s couldn't translate error code, `strerror_r` error code is '%i'", __func__, formattingResult);
        return std::to_string(errorCode);
    }
    return buffer;

#    else
    // GNU-specific strerror_r
    // We always get some result in this implementation for a valid buffer
    const char* result = strerror_r(errorCode, buffer, bufferSize);
    return result;

#    endif // end of strerror_r implementation switch

#endif // end of platform switch
}

/**
 * Function translates the last errno error code into a text.
 *
 * @return text message corresponding to the last errno error code or an empty string if there were no error
 */
inline std::string getLastErrnoMessage()
{
    const ErrnoType errorCode = getLastErrno();
    return convertErrnoToMessage(errorCode);
}

///////////////////////////////////
/// Platform specific functions ///
///////////////////////////////////

#if CARB_PLATFORM_WINDOWS

/**
 * Returns the value of the GetLastError() Win API function
 *
 * @return the value of the GetLastError Win API function.
 */
inline WinApiErrorType getLastWinApiErrorCode()
{
    return ::GetLastError();
}

/**
 * Function translates the Win API error code into a text message.
 * Warning: texts of some Win API errors can contain special symbols thus
 * you should be careful if you want to use returned text as a parameter
 * that can influence formatting. I.e. `printf(convertWinApiErrorCodeToMessage().c_str());`
 * is NOT recommended. Ex. on Windows the error message for the ERROR_BAD_EXE_FORMAT error
 * will contain '%1' in the text: "%1 is not a valid Win32 application."
 * Also after this function call the value of the last Win API
 * error might be changed. If the error code is needed then use the getLastWinApiErrorCode
 * function beforehand.
 *
 * @param errorCode the code of the Win API error
 * @return text message corresponding to the error code or an empty string if there is no error
 */
inline std::string convertWinApiErrorCodeToMessage(WinApiErrorType errorCode)
{
    if (errorCode == CARBWIN_ERROR_SUCCESS)
    {
        return std::string();
    }

    LPWSTR resultMessageBuffer = nullptr;
    const DWORD kFormatFlags = CARBWIN_FORMAT_MESSAGE_ALLOCATE_BUFFER | CARBWIN_FORMAT_MESSAGE_FROM_SYSTEM |
                               CARBWIN_FORMAT_MESSAGE_IGNORE_INSERTS;

    const DWORD dwFormatResultCode = FormatMessageW(kFormatFlags, nullptr, errorCode,
                                                    CARBWIN_MAKELANGID(CARBWIN_LANG_NEUTRAL, CARBWIN_SUBLANG_DEFAULT),
                                                    reinterpret_cast<LPWSTR>(&resultMessageBuffer), 0, nullptr);
    if (dwFormatResultCode == 0)
    {
        const DWORD operationErrorCode = GetLastError();
        CARB_LOG_ERROR("%s couldn't translate error code {%" PRIu32 "}, `FormatMessage` error code is '%" PRIu32 "'",
                       __func__, errorCode, operationErrorCode);
        return std::to_string(errorCode);
    }

    assert(resultMessageBuffer);

    const auto localMemDeleter = [](LPWSTR str) {
        if (str)
        {
            ::LocalFree(str);
        }
    };
    std::unique_ptr<WCHAR, decltype(localMemDeleter)> systemBuffKeeper(resultMessageBuffer, localMemDeleter);

    const std::string result = carb::extras::convertWideToUtf8(resultMessageBuffer);

    return result;
}

/**
 * Function translates the last Win API error code into a text.
 * Warning: the same considerations as for the function `convertWinApiErrorCodeToMessage`
 * should be applied. Also after this function call the value of the last Win API
 * error might be changed. If the error code is needed then use the getLastWinApiErrorCode
 * function beforehand.
 *
 * @return text message corresponding to the last system error code or empty string if there were no error
 */
inline std::string getLastWinApiErrorMessage()
{
    const WinApiErrorType errorCode = getLastWinApiErrorCode();
    return convertWinApiErrorCodeToMessage(errorCode);
}

#endif // #if CARB_PLATFORM_WINDOWS

} // namespace extras
} // namespace carb
