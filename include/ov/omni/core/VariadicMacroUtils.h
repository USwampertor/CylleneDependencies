// Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

// this macro can be used to count the number of arguments, returning a user defined value for each count.
//
// some examples as to what you can do with this:
//
// - return the argument count
//
// - return 1 if the count is even, and 0 if the count is odd.
//
// - return "one", "two", "three", etc. based on the argument count
//
// note, if the argument list is empty, this macro counts the argument list as 1.  in short, this macro cannot detect an
// empty list.
//
// to call the macro, pass the argument list along with a reversed list of a mapping from the count to the desired
// value.  for example, to return the argument count:
//
// #define COUNT(...)
//     OMNI_VA_GET_ARG_64(__VA_ARGS__, 63, 62, 61, 60,
//          59, 58, 57, 56, 55, 54, 53, 52, 51, 50,
//          49, 48, 47, 46, 45, 44, 43, 42, 41, 40,
//          39, 38, 37, 36, 35, 34, 33, 32, 31, 30,
//          29, 28, 27, 26, 25, 24, 23, 22, 21, 20,
//          19, 18, 17, 16, 15, 14, 13, 12, 11, 10,
//          9,  8,  7,  6,  5,  4,  3,  2,  1,  0)
#define OMNI_VA_GET_ARG_64(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20,  \
                           _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38,   \
                           _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56,   \
                           _57, _58, _59, _60, _61, _62, _63, N, ...)                                                  \
    N


// needed in by MSVC's preprocessor to evaluate __VA_ARGS__.  harmless on other compilers.
#define OMNI_VA_EXPAND(x_) x_

// returns 1 if the argument list has fewer than two arguments (i.e. 1 or empty).  otherwise returns 0.
#define OMNI_VA_IS_FEWER_THAN_TWO(...)                                                                                 \
    OMNI_VA_EXPAND(OMNI_VA_GET_ARG_64(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   \
                                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, \
                                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /* one or 0 */, 1))

// counts the number of given arguments. due to the design of the pre-processor, if 0 arguments are given, a count of 1
// is incorrectly returned.
#define OMNI_VA_COUNT(...)                                                                                             \
    OMNI_VA_EXPAND(OMNI_VA_GET_ARG_64(__VA_ARGS__, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, \
                                      46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27,  \
                                      26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 20, 9, 8, 7, 6,  \
                                      5, 4, 3, 2, 1, 0))

// returns the first argument.  if the argument list is empty, nothing is returned.
//
// ("a", "b", "c") -> "a"
#define OMNI_VA_FIRST(...) OMNI_VA_FIRST_(OMNI_VA_IS_FEWER_THAN_TWO(__VA_ARGS__), __VA_ARGS__)
#define OMNI_VA_FIRST_(is_fewer_than_two_, ...) OMNI_VA_FIRST__(is_fewer_than_two_, __VA_ARGS__)
#define OMNI_VA_FIRST__(is_fewer_than_two_, ...) OMNI_VA_FIRST__##is_fewer_than_two_(__VA_ARGS__)
#define OMNI_VA_FIRST__0(...) OMNI_VA_EXPAND(OMNI_VA_FIRST___(__VA_ARGS__))
#define OMNI_VA_FIRST__1(...) __VA_ARGS__
#define OMNI_VA_FIRST___(first, ...) first

// removes the first argument from the argument list, returning the remaining arguments.
//
// if any arguments are returned a comma is prepended to the list.
//
// if no arguments are returned, no comma is added.
//
// ("a", "b", "c") -> , "b", "c"
// () ->
#define OMNI_VA_COMMA_WITHOUT_FIRST(...)                                                                               \
    OMNI_VA_COMMA_WITHOUT_FIRST_(OMNI_VA_IS_FEWER_THAN_TWO(__VA_ARGS__), __VA_ARGS__)
#define OMNI_VA_COMMA_WITHOUT_FIRST_(is_fewer_than_two_, ...)                                                          \
    OMNI_VA_COMMA_WITHOUT_FIRST__(is_fewer_than_two_, __VA_ARGS__)
#define OMNI_VA_COMMA_WITHOUT_FIRST__(is_fewer_than_two_, ...)                                                         \
    OMNI_VA_COMMA_WITHOUT_FIRST__##is_fewer_than_two_(__VA_ARGS__)
#define OMNI_VA_COMMA_WITHOUT_FIRST__0(...) OMNI_VA_EXPAND(OMNI_VA_COMMA_WITHOUT_FIRST___(__VA_ARGS__))
#define OMNI_VA_COMMA_WITHOUT_FIRST__1(...)
#define OMNI_VA_COMMA_WITHOUT_FIRST___(first, ...) , __VA_ARGS__

// returns the first argument from the argument list.  if the given list is empty, an empty string is returned.
//
// ("a", "b", "c") -> "a"
// () -> ""
#define OMNI_VA_FIRST_OR_EMPTY_STRING(...)                                                                             \
    OMNI_VA_FIRST_OR_EMPTY_STRING_(OMNI_VA_IS_FEWER_THAN_TWO(__VA_ARGS__), __VA_ARGS__)
#define OMNI_VA_FIRST_OR_EMPTY_STRING_(is_fewer_than_two_, ...)                                                        \
    OMNI_VA_FIRST_OR_EMPTY_STRING__(is_fewer_than_two_, __VA_ARGS__)
#define OMNI_VA_FIRST_OR_EMPTY_STRING__(is_fewer_than_two_, ...)                                                       \
    OMNI_VA_FIRST_OR_EMPTY_STRING__##is_fewer_than_two_(__VA_ARGS__)
#define OMNI_VA_FIRST_OR_EMPTY_STRING__0(...) OMNI_VA_EXPAND(OMNI_VA_FIRST_OR_EMPTY_STRING___(__VA_ARGS__))
#define OMNI_VA_FIRST_OR_EMPTY_STRING__1(...) " " __VA_ARGS__
#define OMNI_VA_FIRST_OR_EMPTY_STRING___(first, ...) first
