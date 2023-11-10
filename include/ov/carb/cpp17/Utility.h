// Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

#pragma once

#include "../Defines.h"

namespace carb
{

namespace cpp17
{

struct in_place_t
{
    explicit in_place_t() = default;
};
static constexpr in_place_t in_place{};

template <class>
struct in_place_type_t
{
    explicit in_place_type_t() = default;
};

template <class T>
static constexpr in_place_type_t<T> in_place_type{};

template <std::size_t I>
struct in_place_index_t
{
    explicit in_place_index_t() = default;
};

template <std::size_t I>
static constexpr in_place_index_t<I> in_place_index{};

} // namespace cpp17
} // namespace carb
