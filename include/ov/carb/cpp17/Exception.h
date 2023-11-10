// Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"

#include <exception>

#if CARB_PLATFORM_LINUX
#    include <cxxabi.h>
#    include <cstring>
#elif CARB_PLATFORM_WINDOWS
#    include <cstring>
extern "C"
{
    void* __cdecl _getptd(void);
}
#endif

// __cpp_lib_uncaught_exceptions -> https://en.cppreference.com/w/User:D41D8CD98F/feature_testing_macros
// Visual Studio 14.0+ supports N4152 std::uncaught_exceptions() but doesn't define __cpp_lib_uncaught_exceptions
#if (defined(__cpp_lib_uncaught_exceptions) && __cpp_lib_uncaught_exceptions >= 201411) ||                             \
    (defined(_MSC_VER) && _MSC_VER >= 1900)
#    define CARB_HAS_UNCAUGHT_EXCEPTIONS 1
#else
#    define CARB_HAS_UNCAUGHT_EXCEPTIONS 0
#endif

namespace carb
{

namespace cpp17
{

namespace detail
{

#if CARB_PLATFORM_WINDOWS
inline void* getptd()
{
    return ::_getptd();
}
#endif

} // namespace detail

//
// various ways to backport this, mainly come from accessing the systems thread local data and knowing
// the exact offset the exception count is stored in.
//
// * https://beta.boost.org/doc/libs/develop/boost/core/uncaught_exceptions.hpp
// * https://github.com/facebook/folly/blob/master/folly/lang/UncaughtExceptions.h
// * https://github.com/bitwizeshift/BackportCpp/blob/master/include/bpstd/exception.hpp
//

inline int uncaught_exceptions() noexcept
{
#if CARB_HAS_UNCAUGHT_EXCEPTIONS
    return static_cast<int>(std::uncaught_exceptions());
#elif CARB_PLATFORM_LINUX
    using byte = unsigned char;
    int count{};
    // __cxa_eh_globals::uncaughtExceptions, x32 offset - 0x4, x64 - 0x8
    const auto* ptr = reinterpret_cast<const byte*>(::abi::__cxa_get_globals()) + sizeof(void*);
    std::memcpy(&count, ptr, sizeof(count));
    return count;
#elif CARB_PLATFORM_WINDOWS
    using byte = unsigned char;
    int count{};
    const auto offset = (sizeof(void*) == 8u ? 0x100 : 0x90);
    const auto* ptr = static_cast<const byte*>(carb::cpp17::detail::getptd()) + offset;
    // _tiddata::_ProcessingThrow, x32 offset - 0x90, x64 - 0x100
    std::memcpy(&count, ptr, sizeof(count));
    return count;
#else
    // return C++11/14 uncaught_exception() is basically broken for any nested exceptions
    // as only going from 0 exceptions to 1 exception will be detected
    return static_cast<int>(std::uncaught_exception());
#endif
}

} // namespace cpp17

} // namespace carb
