// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! \file
//! \brief Implementation of functionality from `<type_traits>` library added in C++17.

#pragma once

#include <functional>
#include <type_traits>

#include "../Defines.h"
#include "../detail/NoexceptType.h"
#include "ImplInvoke.h"

namespace carb
{
namespace cpp17
{

CARB_DETAIL_PUSH_IGNORE_NOEXCEPT_TYPE()

//! An integral constant with \c bool type and value \c B.
template <bool B>
using bool_constant = std::integral_constant<bool, B>;

//! \cond DEV
namespace detail
{

template <typename... B>
struct conjunction_impl;

template <>
struct conjunction_impl<> : std::true_type
{
};

template <typename B>
struct conjunction_impl<B> : B
{
};

template <typename BHead, typename... BRest>
struct conjunction_impl<BHead, BRest...> : std::conditional_t<bool(BHead::value), conjunction_impl<BRest...>, BHead>
{
};

} // namespace detail
//! \endcond

//! A conjunction is the logical \e and of all \c B traits.
//!
//! An empty list results in a \c true value. This meta-function is short-circuiting.
//!
//! \tparam B The series of traits to evaluate the \c value member of. Each \c B must have a member constant \c value
//!           which is convertible to \c bool. Use of carb::cpp17::bool_constant is helpful here.
template <typename... B>
struct conjunction : detail::conjunction_impl<B...>
{
};

//! \cond DEV
namespace detail
{

template <typename... B>
struct disjunction_impl;

template <>
struct disjunction_impl<> : std::false_type
{
};

template <typename B>
struct disjunction_impl<B> : B
{
};

template <typename BHead, typename... BRest>
struct disjunction_impl<BHead, BRest...> : std::conditional_t<bool(BHead::value), BHead, disjunction_impl<BRest...>>
{
};

} // namespace detail
//! \endcond

//! A disjunction is the logical \e or of all \c B traits.
//!
//! An empty list results in a \c false value. This metafunction is short-circuiting.
//!
//! \tparam B The series of traits to evaluate the \c value member of. Each \c B must have a member constant \c value
//!           which is convertible to \c bool. Use of \ref cpp17::bool_constant is helpful here.
template <typename... B>
struct disjunction : detail::disjunction_impl<B...>
{
};

//! A logical \e not of \c B trait.
//!
//! \tparam B The trait to evaluate the \c value member of. This must have a member constant \c value which is
//!           convertible to \c bool. Use of \ref cpp17::bool_constant is helpful here.
template <typename B>
struct negation : bool_constant<!bool(B::value)>
{
};

//! Void type
template <class...>
using void_t = void;

using std::is_convertible;
using std::is_void;

//! \cond DEV
namespace detail
{

// Base case matches where either `To` or `From` is void or if `is_convertible<From, To>` is false. The conversion in
// this case is only non-throwing if both `From` and `To` are `void`.
template <typename From, typename To, typename = void>
struct is_nothrow_convertible_impl : conjunction<is_void<From>, is_void<To>>
{
};

// If neither `From` nor `To` are void and `From` is convertible to `To`, then we test that such a conversion is
// non-throwing.
template <typename From, typename To>
struct is_nothrow_convertible_impl<
    From,
    To,
    std::enable_if_t<conjunction<negation<is_void<From>>, negation<is_void<To>>, is_convertible<From, To>>::value>>
{
    static void test(To) noexcept;

    static constexpr bool value = noexcept(test(std::declval<From>()));
};

} // namespace detail
//! \endcond

//! Determine if \c From can be implicitly-converted to \c To without throwing an exception.
//!
//! This is equivalent to the C++20 \c std::is_nothrow_convertible meta query. While added in C++20, it is required for
//! the C++17 \ref is_nothrow_invocable_r meta query.
template <typename From, typename To>
struct is_nothrow_convertible : bool_constant<detail::is_nothrow_convertible_impl<From, To>::value>
{
};

//! \cond DEV
namespace detail
{

template <class T>
struct IsSwappable;
template <class T>
struct IsNothrowSwappable;
template <class T, class U, class = void>
struct SwappableWithHelper : std::false_type
{
};
template <class T, class U>
struct SwappableWithHelper<T, U, void_t<decltype(swap(std::declval<T>(), std::declval<U>()))>> : std::true_type
{
};
template <class T, class U>
struct IsSwappableWith : bool_constant<conjunction<SwappableWithHelper<T, U>, SwappableWithHelper<U, T>>::value>
{
};
template <class T>
struct IsSwappable : IsSwappableWith<std::add_lvalue_reference_t<T>, std::add_lvalue_reference_t<T>>::type
{
};
using std::swap; // enable ADL
template <class T, class U>
struct SwapCannotThrow : bool_constant<noexcept(swap(std::declval<T>(), std::declval<U>()))&& noexcept(
                             swap(std::declval<U>(), std::declval<T>()))>
{
};
template <class T, class U>
struct IsNothrowSwappableWith : bool_constant<conjunction<IsSwappableWith<T, U>, SwapCannotThrow<T, U>>::value>
{
};
template <class T>
struct IsNothrowSwappable : IsNothrowSwappableWith<std::add_lvalue_reference_t<T>, std::add_lvalue_reference_t<T>>::type
{
};

} // namespace detail
//! \endcond

template <class T, class U>
struct is_swappable_with : detail::IsSwappableWith<T, U>::type
{
};
template <class T>
struct is_swappable : detail::IsSwappable<T>::type
{
};
template <class T, class U>
struct is_nothrow_swappable_with : detail::IsNothrowSwappableWith<T, U>::type
{
};
template <class T>
struct is_nothrow_swappable : detail::IsNothrowSwappable<T>::type
{
};

//! \cond DEV
namespace detail
{

// The base case is matched in cases where `invoke_uneval` is an invalid expression. The `Qualify` is always set to void
// by users.
template <typename Qualify, typename Func, typename... TArgs>
struct invoke_result_impl
{
};

template <typename Func, typename... TArgs>
struct invoke_result_impl<decltype(void(invoke_uneval(std::declval<Func>(), std::declval<TArgs>()...))), Func, TArgs...>
{
    using type = decltype(invoke_uneval(std::declval<Func>(), std::declval<TArgs>()...));
};

template <typename Qualify, typename Func, typename... TArgs>
struct is_invocable_impl : std::false_type
{
};

template <typename Func, typename... TArgs>
struct is_invocable_impl<void_t<typename invoke_result_impl<void, Func, TArgs...>::type>, Func, TArgs...> : std::true_type
{
};

template <typename Qualify, typename Func, typename... TArgs>
struct is_nothrow_invocable_impl : std::false_type
{
};

template <typename Func, typename... TArgs>
struct is_nothrow_invocable_impl<void_t<typename invoke_result_impl<void, Func, TArgs...>::type>, Func, TArgs...>
    : bool_constant<noexcept(invoke_uneval(std::declval<Func>(), std::declval<TArgs>()...))>
{
};

} // namespace detail
//! \endcond

//! Get the result type of calling \c Func with the \c TArgs pack.
//!
//! If \c Func is callable with the given \c TArgs pack, then this structure has a member typedef named \c type with the
//! return of that call. If \c Func is not callable, then the member typedef does not exist.
//!
//! \code
//! static_assert(std::is_same<int, typename invoke_result<int(*)(char), char>::type>::value);
//! \endcode
//!
//! This is equivalent to the C++17 \c std::invoke_result meta transformation.
//!
//! \see carb::cpp17::invoke_result_t
template <typename Func, typename... TArgs>
struct invoke_result : detail::invoke_result_impl<void, Func, TArgs...>
{
};

//! Helper for \ref carb::cpp17::invoke_result which accesses the \c type member.
//!
//! \code
//! // Get the proper return type and SFINAE-safe disqualify `foo` when `f(10)` is not valid.
//! template <typename Func>
//! invoke_result_t<Func, int> foo(Func&& f)
//! {
//!     return invoke(std::forward<Func>(f), 10);
//! }
//! \endcode
//!
//! This is equivalent to the C++ \c std::invoke_result_t helper typedef.
template <typename Func, typename... TArgs>
using invoke_result_t = typename invoke_result<Func, TArgs...>::type;

//! Check if the \c Func is invocable with the \c TArgs pack.
//!
//! If \c Func is callable with the given \c TArgs pack, then this structure will derive from \c true_type; otherwise,
//! it will be \c false_type. If \c value is \c true, then `invoke(func, args...)` is a valid expression.
//!
//! \code
//! static_assert(is_invocable<void(*)()>::value);
//! static_assert(!is_invocable<void(*)(int)>::value);
//! static_assert(is_invocable<void(*)(int), int>::value);
//! static_assert(is_invocable<void(*)(long), int>::value);
//! \endcode
//!
//! This is equivalent to the C++20 \c std::is_invocable meta query. The query was added in C++17, but this additionally
//! supports invoking a pointer to a `const&` member function on an rvalue reference.
template <typename Func, typename... TArgs>
struct is_invocable : detail::is_invocable_impl<void, Func, TArgs...>
{
};

//! Check if invoking \c Func with the \c TArgs pack will not throw.
//!
//! If \c Func called with the given \c TArgs pack is callable and marked \c noexcept, then this structure will derive
//! from \c true_type; otherwise, it will be \c false_type. If \c Func is not callable at all, then this will also be
//! \c false_type.
//!
//! This is equivalent to the C++17 \c is_nothrow_invocable meta query.
template <typename Func, typename... TArgs>
struct is_nothrow_invocable : detail::is_nothrow_invocable_impl<void, Func, TArgs...>
{
};

//! \cond DEV
namespace detail
{

template <typename Qualify, typename R, typename Func, typename... TArgs>
struct invocable_r_impl
{
    using invocable_t = std::false_type;

    using invocable_nothrow_t = std::false_type;
};

template <typename Func, typename... TArgs>
struct invocable_r_impl<std::enable_if_t<is_invocable<Func, TArgs...>::value>, void, Func, TArgs...>
{
    using invocable_t = std::true_type;

    using invocable_nothrow_t = is_nothrow_invocable<Func, TArgs...>;
};

// The is_void as part of the qualifier is to workaround an MSVC issue where it thinks this partial specialization and
// the one which explicitly lists `R` as void are equally-qualified.
template <typename R, typename Func, typename... TArgs>
struct invocable_r_impl<std::enable_if_t<is_invocable<Func, TArgs...>::value && !is_void<R>::value>, R, Func, TArgs...>
{
private:
    // Can't use declval for conversion checks, as it adds an rvalue ref to the type. We want to make sure the result of
    // a returned function can be converted.
    static invoke_result_t<Func, TArgs...> get_val() noexcept;

    template <typename Target>
    static void convert_to(Target) noexcept;

    template <typename TR, typename = decltype(convert_to<TR>(get_val()))>
    static std::true_type test(int) noexcept;

    template <typename TR>
    static std::false_type test(...) noexcept;

    template <typename TR, typename = decltype(convert_to<TR>(get_val()))>
    static bool_constant<noexcept(convert_to<TR>(get_val()))> test_nothrow(int) noexcept;

    template <typename TR>
    static std::false_type test_nothrow(...) noexcept;

public:
    using invocable_t = decltype(test<R>(0));

    using invocable_nothrow_t = decltype(test_nothrow<R>(0));
};

} // namespace detail
//! \endcond

//! Check if invoking \c Func with the \c TArgs pack will return \c R.
//!
//! Similar to \ref is_invocable, but additionally checks that the result type is convertible to \c R and that the
//! conversion does not bind a reference to a temporary object. If \c R is \c void, the result can be any type (as any
//! type can be converted to \c void by discarding it). If \c value is \c true, then `invoke_r<R>(func, args...)` is a
//! valid expression.
//!
//! This is equivalent to the C++23 definition of \c is_invocable_r. The function was originally added in C++17, but the
//! specification was altered in C++23 to avoid undefined behavior.
template <typename R, typename Func, typename... TArgs>
struct is_invocable_r : detail::invocable_r_impl<void, R, Func, TArgs...>::invocable_t
{
};

//! Check that invoking \c Func with the \c TArgs pack and converting it to \c R will not throw.
//!
//! This is equivalent to the C++23 definition of \c is_nothrow_invocable_r. The function was originally added in C++17,
//! but the specification was altered in C++23 to avoid undefined behavior.
template <typename R, typename Func, typename... TArgs>
struct is_nothrow_invocable_r : detail::invocable_r_impl<void, R, Func, TArgs...>::invocable_nothrow_t
{
};

CARB_DETAIL_POP_IGNORE_NOEXCEPT_TYPE()

} // namespace cpp17
} // namespace carb
