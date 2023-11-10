// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

#pragma once

#include <tuple>

#include "Functional.h"

namespace carb
{
namespace cpp17
{

namespace detail
{

template <class F, class Tuple, size_t... I>
constexpr decltype(auto) applyImpl(F&& f, Tuple&& t, std::index_sequence<I...>)
{
    return carb::cpp17::invoke(std::forward<F>(f), std::get<I>(std::forward<Tuple>(t))...);
}
} // namespace detail

template <class F, class Tuple>
constexpr decltype(auto) apply(F&& f, Tuple&& t)
{
    return detail::applyImpl(std::forward<F>(f), std::forward<Tuple>(t),
                             std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>{});
}

} // namespace cpp17
} // namespace carb
