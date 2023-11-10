// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! \file
//! \brief Implementation of functionality from `<functional>` library added in C++17.

#pragma once

#include "TypeTraits.h"
#include "ImplInvoke.h"
#include "../detail/NoexceptType.h"

namespace carb
{
namespace cpp17
{

CARB_DETAIL_PUSH_IGNORE_NOEXCEPT_TYPE()

//! Invoke the function \a f with the given \a args pack.
//!
//! This is equivalent to the C++20 \c std::invoke function. It was originally added in C++17, but is marked as
//! \c constexpr per the C++20 Standard.
template <typename Func, typename... TArgs>
constexpr invoke_result_t<Func, TArgs...> invoke(Func&& f,
                                                 TArgs&&... args) noexcept(is_nothrow_invocable<Func, TArgs...>::value)
{
    return detail::invoke_impl<std::decay_t<Func>>::eval(std::forward<Func>(f), std::forward<TArgs>(args)...);
}

//! \cond DEV
namespace detail
{

// This is needed to handle calling a function which returns a non-void when `R` is void. The only difference is the
// lack of a return statement.
template <typename R, typename Func, typename... TArgs>
constexpr std::enable_if_t<is_void<R>::value> invoke_r_impl(Func&& f, TArgs&&... args) noexcept(
    is_nothrow_invocable_r<R, Func, TArgs...>::value)
{
    detail::invoke_impl<std::decay_t<Func>>::eval(std::forward<Func>(f), std::forward<TArgs>(args)...);
}

template <typename R, typename Func, typename... TArgs>
constexpr std::enable_if_t<!is_void<R>::value, R> invoke_r_impl(Func&& f, TArgs&&... args) noexcept(
    is_nothrow_invocable_r<R, Func, TArgs...>::value)
{
    return detail::invoke_impl<std::decay_t<Func>>::eval(std::forward<Func>(f), std::forward<TArgs>(args)...);
}

} // namespace detail
//! \endcond

//! Invoke the function with the given arguments with the explicit return type \c R. This follows the same rules as
//! \ref carb::cpp17::invoke().
//!
//! This is equivalent to the C++23 \c std::invoke_r function. It lives here because people would expect an \c invoke_r
//! function to live next to an \c invoke function.
template <typename R, typename Func, typename... TArgs>
constexpr R invoke_r(Func&& f, TArgs&&... args) noexcept(is_nothrow_invocable_r<R, Func, TArgs...>::value)
{
    return detail::invoke_r_impl<R>(std::forward<Func>(f), std::forward<TArgs>(args)...);
}

CARB_DETAIL_POP_IGNORE_NOEXCEPT_TYPE()

} // namespace cpp17
} // namespace carb
