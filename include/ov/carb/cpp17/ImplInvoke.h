// Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

#pragma once

#include "../Defines.h"
#include "../detail/NoexceptType.h"

#include <functional>
#include <type_traits>
#include <utility>

//! \file
//! Contains common utilities used by \c invoke (which lives in the \c functional header) and the \c invoke_result type
//! queries (which live in the \c type_traits header).

namespace carb
{
namespace cpp17
{
//! \cond DEV
namespace detail
{

CARB_DETAIL_PUSH_IGNORE_NOEXCEPT_TYPE()

template <typename T>
struct is_reference_wrapper : std::false_type
{
};

template <typename T>
struct is_reference_wrapper<std::reference_wrapper<T>> : std::true_type
{
};

// The interface of invoke_impl are the `eval` and `uneval` functions, which have the correct return type and noexcept
// specifiers for an expression `INVOKE(f, args...)` (this comes from the C++ concept of "Callable"). The two functions
// are identical, save for the `eval` function having a body while the `uneval` function does not. This matters for
// functions with declared-but-undefined return types. It is legal to ask type transformation questions about a function
// `R (*)(Args...)` when `R` is undefined, but not legal to evaluate it in any way.
//
// Base template: T is directly invocable -- a function pointer or object with operator()
template <typename T>
struct invoke_impl
{
    template <typename F, typename... TArgs>
    static constexpr auto eval(F&& f,
                               TArgs&&... args) noexcept(noexcept(std::forward<F>(f)(std::forward<TArgs>(args)...)))
        -> decltype(std::forward<F>(f)(std::forward<TArgs>(args)...))
    {
        return std::forward<F>(f)(std::forward<TArgs>(args)...);
    }
    template <typename F, typename... TArgs>
    static constexpr auto uneval(F&& f,
                                 TArgs&&... args) noexcept(noexcept(std::forward<F>(f)(std::forward<TArgs>(args)...)))
        -> decltype(std::forward<F>(f)(std::forward<TArgs>(args)...));
};

// Match the case where we want to invoke a member function.
template <typename TObject, typename TReturn>
struct invoke_impl<TReturn TObject::*>
{
    using Self = invoke_impl;

    template <bool B>
    using bool_constant = std::integral_constant<bool, B>;

#if CARB_COMPILER_GNUC == 1 && __cplusplus <= 201703L
    // WORKAROUND for pre-C++20: Calling a `const&` member function on an `&&` object through invoke is a C++20
    // extension. MSVC supports this, but GNUC-compatible compilers do not until C++20. To work around this, we change
    // the object's ref qualifier from `&&` to `const&` if we are attempting to call a `const&` member function.
    //
    // Note `move(x).f()` has always been allowed if `f` is `const&` qualified, the issue is `std::move(x).*(&T::foo)()`
    // is not allowed (this is a C++ specification bug, corrected in C++20). Further note that we can not do this for
    // member data selectors, because the ref qualifier carries through since C++17. When this workaround is no longer
    // needed (when C++20 is minimum), the `_is_cref_mem_fn` tests on the `_access` functions can be removed.
    template <typename UReturn, typename UObject, typename... UArgs>
    static std::true_type _test_is_cref_mem_fn(UReturn (UObject::*mem_fn)(UArgs...) const&);

    static std::false_type _test_is_cref_mem_fn(...);

    template <typename TRMem>
    using _is_cref_mem_fn = decltype(_test_is_cref_mem_fn(std::declval<std::decay_t<TRMem>>()));

    template <typename T, typename = std::enable_if_t<std::is_base_of<TObject, std::decay_t<T>>::value>>
    static constexpr auto _access(T&& x, std::false_type) noexcept -> std::add_rvalue_reference_t<T>
    {
        return std::forward<T>(x);
    }

    template <typename T, typename = std::enable_if_t<std::is_base_of<TObject, std::decay_t<T>>::value>>
    static constexpr auto _access(T const& x, std::true_type) noexcept -> T const&
    {
        return x;
    }

#else
    template <typename>
    using _is_cref_mem_fn = std::false_type;

    // Accessing the type should be done directly.
    template <typename T, bool M, typename = std::enable_if_t<std::is_base_of<TObject, std::decay_t<T>>::value>>
    static constexpr auto _access(T&& x, bool_constant<M>) noexcept -> std::add_rvalue_reference_t<T>
    {
        return std::forward<T>(x);
    }
#endif

    // T is a reference wrapper -- access goes through the `get` function.
    template <typename T, bool M, typename = std::enable_if_t<is_reference_wrapper<std::decay_t<T>>::value>>
    static constexpr auto _access(T&& x, bool_constant<M>) noexcept(noexcept(x.get())) -> decltype(x.get())
    {
        return x.get();
    }

    // Matches cases where a pointer or fancy pointer is passed in.
    template <typename TOriginal,
              bool M,
              typename T = std::decay_t<TOriginal>,
              typename = std::enable_if_t<!std::is_base_of<TObject, T>::value && !is_reference_wrapper<T>::value>>
    static constexpr auto _access(TOriginal&& x, bool_constant<M>) noexcept(noexcept(*std::forward<TOriginal>(x)))
        -> decltype(*std::forward<TOriginal>(x))
    {
        return *std::forward<TOriginal>(x);
    }

    template <typename T, typename... TArgs, typename TRMem, typename = std::enable_if_t<std::is_function<TRMem>::value>>
    static constexpr auto eval(TRMem TObject::*pmem, T&& x, TArgs&&... args) noexcept(noexcept(
        (Self::_access(std::forward<T>(x), _is_cref_mem_fn<decltype(pmem)>{}).*pmem)(std::forward<TArgs>(args)...)))
        -> decltype((Self::_access(std::forward<T>(x), _is_cref_mem_fn<decltype(pmem)>{}).*
                     pmem)(std::forward<TArgs>(args)...))
    {
        return (Self::_access(std::forward<T>(x), _is_cref_mem_fn<decltype(pmem)>{}).*pmem)(std::forward<TArgs>(args)...);
    }

    template <typename T, typename... TArgs, typename TRMem, typename = std::enable_if_t<std::is_function<TRMem>::value>>
    static constexpr auto uneval(TRMem TObject::*pmem, T&& x, TArgs&&... args) noexcept(noexcept(
        (Self::_access(std::forward<T>(x), _is_cref_mem_fn<decltype(pmem)>{}).*pmem)(std::forward<TArgs>(args)...)))
        -> decltype((Self::_access(std::forward<T>(x), _is_cref_mem_fn<decltype(pmem)>{}).*
                     pmem)(std::forward<TArgs>(args)...));

    template <typename T>
    static constexpr auto eval(TReturn TObject::*select,
                               T&& x) noexcept(noexcept(Self::_access(std::forward<T>(x), std::false_type{}).*select))
        -> decltype(Self::_access(std::forward<T>(x), std::false_type{}).*select)
    {
        return Self::_access(std::forward<T>(x), std::false_type{}).*select;
    }

    template <typename T>
    static constexpr auto uneval(TReturn TObject::*select,
                                 T&& x) noexcept(noexcept(Self::_access(std::forward<T>(x), std::false_type{}).*select))
        -> decltype(Self::_access(std::forward<T>(x), std::false_type{}).*select);
};

// Test invocation of `f(args...)` in an unevaluated context to get its return type. This is a SFINAE-safe error if the
// expression `f(args...)` is invalid.
template <typename F, typename... TArgs>
auto invoke_uneval(F&& f, TArgs&&... args) noexcept(
    noexcept(invoke_impl<std::decay_t<F>>::uneval(std::forward<F>(f), std::forward<TArgs>(args)...)))
    -> decltype(invoke_impl<std::decay_t<F>>::uneval(std::forward<F>(f), std::forward<TArgs>(args)...));

CARB_DETAIL_POP_IGNORE_NOEXCEPT_TYPE()

} // namespace detail
//! \endcond
} // namespace cpp17
} // namespace carb
