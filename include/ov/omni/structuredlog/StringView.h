// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief A string_view wrapper to make telemetry calls easier.
 */
#pragma once
#include "../../carb/cpp17/StringView.h"
#include <string>

namespace omni
{
namespace structuredlog
{

/** An extension of carb::cpp17::basic_string_view that can handle nullptr and
 *  `std::basic_string` as inputs
 *
 *  @tparam CharT The char type pointed to by the view.
 *  @tparam Traits The type traits for @p CharT.
 */
template <class CharT, class Traits = std::char_traits<CharT>>
class BasicStringView : public carb::cpp17::basic_string_view<CharT, Traits>
{
public:
    constexpr BasicStringView() : base()
    {
    }

    /** Create a string view from a C string.
     *  @param[in] s The C string to create a view into.
     *               This original pointer continues to be used and nothing is copied.
     */
    constexpr BasicStringView(const CharT* s) : base(s, s == nullptr ? 0 : Traits::length(s))
    {
    }

    /** Create a string view from a char buffer
     *  @param[in] s The char buffer to create a view into.
     *               This does not need to be null terminated.
     *               This original pointer continues to be used and nothing is copied.
     *  @param[in] len The length of the view into @p s, in characters.
     */
    constexpr BasicStringView(const CharT* s, size_t len) : base(s, len)
    {
    }

    /** Create a string view from another string view.
     *  @param[in] other The other string view to create this string view from.
     */
    constexpr BasicStringView(const BasicStringView& other) noexcept = default;

    /** Create a string view from a std::basic_string.
     *  @param[in] s The string to create this string view for.
     */
    constexpr BasicStringView(const std::basic_string<CharT>& s) : base(s.c_str(), s.size())
    {
    }

private:
    using base = carb::cpp17::basic_string_view<CharT, Traits>;
};

/** String view for `char` strings. */
using StringView = BasicStringView<char>;

} // namespace structuredlog
} // namespace omni
