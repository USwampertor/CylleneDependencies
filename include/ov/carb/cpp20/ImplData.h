// Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//! @brief Implementation of `std::data` from C++20
#pragma once

#include <cstddef>
#include <initializer_list>

namespace carb
{
namespace cpp20
{

//! Returns a pointer to the block of memory containing the elements of the range.
//! @tparam T The type of the array elements
//! @tparam N The size of the array
//! @param array An array
//! @returns array
template <class T, std::size_t N>
constexpr T* data(T (&array)[N]) noexcept
{
    return array;
}

//! Returns a pointer to the block of memory containing the elements of the range.
//! @tparam C The container type
//! @param c A container
//! @returns `c.data()`
template <class C>
constexpr auto data(C& c) -> decltype(c.data())
{
    return c.data();
}

//! Returns a pointer to the block of memory containing the elements of the range.
//! @tparam C The container type
//! @param c A container
//! @returns `c.data()`
template <class C>
constexpr auto data(const C& c) -> decltype(c.data())
{
    return c.data();
}

//! Returns a pointer to the block of memory containing the elements of the range.
//! @tparam E The type contained in the `std::initializer_list`
//! @param il An `std::initializer_list` of type E
//! @returns `il.begin()`
template <class E>
constexpr const E* data(std::initializer_list<E> il) noexcept
{
    return il.begin();
}

} // namespace cpp20
} // namespace carb
