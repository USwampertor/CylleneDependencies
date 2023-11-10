// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief Helper macros to provide assertion checking macros.
#pragma once

#include "Platform.h"
#include "VariadicMacroUtils.h"

#include <array>
#include <cstdio>
#include <utility>

namespace omni
{
namespace core
{
#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail
{

template <size_t N>
constexpr std::integral_constant<size_t, N - 1> length(const char (&)[N])
{
    return {};
}

template <size_t N>
constexpr std::integral_constant<size_t, N - 1> length(std::array<char, N>)
{
    return {};
}

template <typename T>
using LengthType = decltype(length(std::declval<T>()));

template <typename ARRAY>
constexpr void constCopyTo(ARRAY& out, size_t dst, const char* in, size_t sz)
{
    if (sz)
    {
        out[dst] = *in;
        constCopyTo(out, dst + 1, in + 1, sz - 1);
    }
}

template <class T, std::size_t N, std::size_t... I>
constexpr std::array<std::remove_cv_t<T>, N> constToArrayImpl(T (&a)[N], std::index_sequence<I...>)
{
    return { { a[I]... } };
}

template <class T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> constToArray(T (&a)[N])
{
    return constToArrayImpl(a, std::make_index_sequence<N>{});
}

template <typename T>
constexpr const char* toChars(T& s)
{
    return s;
}

template <typename T, size_t N>
constexpr const char* toChars(const std::array<T, N>& s)
{
    return &(s[0]);
}

template <typename A, typename B, typename C>
constexpr std::array<char, LengthType<A>::value + LengthType<B>::value + LengthType<C>::value + 1> constConcat(
    const A& a, const B& b, const C& c)
{
    char o[LengthType<A>::value + LengthType<B>::value + LengthType<C>::value + 1]{};
    constCopyTo(o, 0, toChars(a), LengthType<A>::value);
    constCopyTo(o, LengthType<A>::value, toChars(b), LengthType<B>::value);
    constCopyTo(o, LengthType<A>::value + LengthType<B>::value, toChars(c), LengthType<C>::value);
    return constToArray(o);
}

template <typename A, typename B>
constexpr std::array<char, LengthType<A>::value + LengthType<B>::value + 1> constConcat(const A& a, const B& b)
{
    return constConcat(a, b, "");
}

template <typename A>
constexpr std::array<char, LengthType<A>::value + 1> constConcat(const A& a)
{
    return constConcat(a, "", "");
}

template <bool I>
struct Assertion
{
    constexpr static char Message[] = "Assertion (%s) failed:";
};

template <>
struct Assertion<false>
{
    constexpr static char Message[] = "Assertion (%s) failed.";
};

constexpr bool hasArg(const char*)
{
    return true;
}
constexpr bool hasArg()
{
    return false;
}

} // namespace detail
#endif
} // namespace core
} // namespace omni

//! This macro is surprisingly complex mainly because it accepts a variable number of arguments. If a single argument is
//! given, a message in the following form is printed:
//!
//!  "Assertion (myCondition) failed.\n"
//!
//! If multiple arguments are given, the message become more dynamic:
//!
//!  "Assertion (var == 1) failed: var == 2\n"
//!
//! Where the latter part of the message "var == 2" is provided by the caller.
//!
//! So, if multiple arguments are given, this macro must:
//!
//!  - End the first part of the message with a : instead of an .
//!
//!  - Concatenate the the fixed format string ("Assertion (%s)..."") with the user supplied format message.
//!
//!  - Add a newline.
//!
//! All of this at compile time.
//!
//! We use a couple of tricks to do this (all in portable C++).
//!
//! - Via the preprocessor, we can't detect if __VA_ARGS__ is empty (this is a preprocessor limitation). We can forward
//!   the __VA_ARGS__ to a constexpr (hasArg).  hasArg is an overloaded constexpr that will return true if an argument
//!   was supplied.  We can then use the result of this overload to select a template specialization (Assertion<>)
//!   containing a constexpr with our format string.
//!
//! - We need to concatenate the first part of the format string with the user provided portion and a newline.  We can't
//!   do this with the preprocess because the first part of the message is a constexpr, not a string literal. To get
//!   around this we use a constexpr (constConcat) to perform the concatenation.
//!
//! - The user format string may be empty.  We use the preprocessor's string concatenation in
//!   OMNI_VA_FIRST_OR_EMPTY_STRING to make sure a second argument is passed to the constexpr string concatenation
//!   function.
//!
//! - When passing the __VA_ARGS__ to fprintf, we need to elide the first argument (since it's the user supplied format
//!   we already concatenated) and provide a comma if any additional arguments were given. While the preprocessor cannot
//!   detect if __VA_ARGS__ is empty, it can detect if at least two arguments are supplied. OMNI_VA_COMMA_WITHOUT_FIRST
//!   uses this to determine if a comma should be added and to elide the first user supplied argument (the user's
//!   format string).
//!
//! Some of this code can be simplified with C++20's __VA_OPT__.

//! Checks if the given condition is true, if not, the given optional message is printed to stdout and the program is
//! terminated.
//!
//! Use this macro when an unrecoverable situation has been detected.
#define OMNI_FATAL_UNLESS(cond_, ...)                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!CARB_LIKELY(cond_))                                                                                       \
        {                                                                                                              \
            auto constexpr const failMsg_ = omni::core::detail::constConcat(                                           \
                omni::core::detail::Assertion<omni::core::detail::hasArg(OMNI_VA_FIRST(__VA_ARGS__))>::Message,        \
                OMNI_VA_FIRST_OR_EMPTY_STRING(__VA_ARGS__));                                                           \
            auto constexpr const fmt_ =                                                                                \
                omni::core::detail::constConcat(__FILE__ ":" CARB_STRINGIFY(__LINE__) ": ", failMsg_, "\n");           \
            std::fprintf(stderr, fmt_.data(), #cond_ OMNI_VA_COMMA_WITHOUT_FIRST(__VA_ARGS__));                        \
            OMNI_BREAK_POINT();                                                                                        \
        }                                                                                                              \
    } while (0)

//! Indicates whether runtime checking is enabled.  For the time being this is always set to `1`
//! indicating that the default implementation should not be overridden.  This may change in
//! the future.
#define OMNI_CHECK_ENABLED 1

//! Checks if the given condition is true, if not, the given optional message is printed to stdout and the program is
//! terminated.
//!
//! Unlike OMNI_ASSERT, this macro runs checks in release builds.
//!
//! Use this macro to when you fail to provide adequate test coverage.
#define OMNI_CHECK OMNI_FATAL_UNLESS

#if CARB_DEBUG
  //! Like std::assert. Basically OMNI_FATAL_UNLESS, but compiles to a no-op in debug builds.
#    define OMNI_ASSERT(cond, ...) OMNI_FATAL_UNLESS(cond, __VA_ARGS__)

//! Set to 1 to indicate that assertion checks are enabled.  Set to 0 if assertion checks will
//! just be ignored.  This value will always be defined regardless of the current mode.
#    define OMNI_ASSERT_ENABLED 1
#else
  //! Like std::assert. Basically OMNI_FATAL_UNLESS, but compiles to a no-op in debug builds.
#    define OMNI_ASSERT(cond, ...) ((void)0)

//! Set to 1 to indicate that assertion checks are enabled.  Set to 0 if assertion checks will
//! just be ignored.  This value will always be defined regardless of the current mode.
#    define OMNI_ASSERT_ENABLED 0
#endif
