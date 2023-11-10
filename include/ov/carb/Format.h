// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include <cstring>
#include <sstream>

namespace carb
{

namespace fmt
{

namespace detail
{

inline void format(std::ostringstream& stream, const char* str)
{
    stream << str;
}

template <class Arg, class... Args>
inline void format(std::ostringstream& stream, const char* str, Arg&& arg, Args&&... args)
{
    const char* p = strstr(str, "{}");
    if (p)
    {
        stream.write(str, p - str);
        stream << arg;
        format(stream, p + 2, std::forward<Args>(args)...);
    }
    else
    {
        stream << str;
    }
}

} // namespace detail

/**
 * Formats a string similar to the {fmt} library (https://fmt.dev), but header-only and without requiring an external
 * library be included
 *
 * NOTE: This is not intended to be a full replacement for {fmt}. Only '{}' is supported (i.e. no non-positional
 * support). And any type can be formatted, but must be streamable (i.e. have an appropriate operator<<)
 *
 * Example: format("{}, {} and {}: {}", "Peter", "Paul", "Mary", 42) would produce the string "Peter, Paul and Mary: 42"
 * @param str The format string. Use '{}' to indicate where the next parameter would be inserted.
 * @returns The formatted string
 */
template <class... Args>
inline std::string format(const char* str, Args&&... args)
{
    std::ostringstream stream;
    detail::format(stream, str, std::forward<Args>(args)...);
    return stream.str();
}


} // namespace fmt
} // namespace carb
