// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#ifndef CARB_IMPLOPTIONAL
#    error This file should only be included from Optional.h
#endif

#include <utility>

#include "TypeTraits.h"

namespace carb
{
namespace cpp17
{
namespace detail
{

struct NontrivialDummyType
{
    constexpr NontrivialDummyType() noexcept
    {
        // Avoid zero-initialization when value initialized
    }
};
static_assert(!std::is_trivially_default_constructible<NontrivialDummyType>::value, "Invalid assumption");

// Default facade for trivial destruction of T
template <class T, bool = std::is_trivially_destructible<T>::value>
struct OptionalDestructor
{
    union
    {
        NontrivialDummyType empty;
        CARB_VIZ typename std::remove_const_t<T> value;
    };
    CARB_VIZ bool hasValue;

    constexpr OptionalDestructor() noexcept : empty{}, hasValue{ false }
    {
    }

    template <class... Args>
    constexpr explicit OptionalDestructor(in_place_t, Args&&... args)
        : value(std::forward<Args>(args)...), hasValue(true)
    {
    }

    // Cannot access anonymous union member `value` until C++17, so expose access here
    constexpr const T& val() const&
    {
        CARB_ASSERT(hasValue);
        return value;
    }

    constexpr T& val() &
    {
        CARB_ASSERT(hasValue);
        return value;
    }

    constexpr const T&& val() const&&
    {
        CARB_ASSERT(hasValue);
        return std::move(value);
    }

    constexpr T&& val() &&
    {
        CARB_ASSERT(hasValue);
        return std::move(value);
    }

    void reset() noexcept
    {
        // No need to destruct since trivially destructible
        hasValue = false;
    }
};

// Specialization for non-trivial destruction of T
template <class T>
struct OptionalDestructor<T, false>
{
    union
    {
        NontrivialDummyType empty;
        CARB_VIZ typename std::remove_const_t<T> value;
    };
    CARB_VIZ bool hasValue;

    ~OptionalDestructor() noexcept
    {
        if (hasValue)
        {
            value.~T();
        }
    }

    constexpr OptionalDestructor() noexcept : empty{}, hasValue{ false }
    {
    }

    template <class... Args>
    constexpr explicit OptionalDestructor(in_place_t, Args&&... args)
        : value(std::forward<Args>(args)...), hasValue(true)
    {
    }

    OptionalDestructor(const OptionalDestructor&) = default;
    OptionalDestructor(OptionalDestructor&&) = default;
    OptionalDestructor& operator=(const OptionalDestructor&) = default;
    OptionalDestructor& operator=(OptionalDestructor&&) = default;

    // Cannot access anonymous union member `value` until C++17, so expose access here
    const T& val() const&
    {
        CARB_ASSERT(hasValue);
        return value;
    }

    T& val() &
    {
        CARB_ASSERT(hasValue);
        return value;
    }

    constexpr const T&& val() const&&
    {
        CARB_ASSERT(hasValue);
        return std::move(value);
    }

    constexpr T&& val() &&
    {
        CARB_ASSERT(hasValue);
        return std::move(value);
    }

    void reset() noexcept
    {
        if (hasValue)
        {
            value.~T();
            hasValue = false;
        }
    }
};

template <class T>
struct OptionalConstructor : OptionalDestructor<T>
{
    using value_type = T;
    using OptionalDestructor<T>::OptionalDestructor;

    template <class... Args>
    T& construct(Args&&... args)
    {
        CARB_ASSERT(!this->hasValue);
        new (std::addressof(this->value)) decltype(this->value)(std::forward<Args>(args)...);
        this->hasValue = true;
        return this->value;
    }

    template <class U>
    void assign(U&& rhs)
    {
        if (this->hasValue)
        {
            this->value = std::forward<U>(rhs);
        }
        else
        {
            construct(std::forward<U>(rhs));
        }
    }

    template <class U>
    void constructFrom(U&& rhs) noexcept(std::is_nothrow_constructible<T, decltype((std::forward<U>(rhs).value))>::value)
    {
        if (rhs.hasValue)
        {
            construct(std::forward<U>(rhs).value);
        }
    }

    template <class U>
    void assignFrom(U&& rhs) noexcept(std::is_nothrow_constructible<T, decltype((std::forward<U>(rhs).value))>::value&&
                                          std::is_nothrow_assignable<T, decltype((std::forward<U>(rhs).value))>::value)
    {
        if (rhs.hasValue)
        {
            assign(std::forward<U>(rhs).value);
        }
        else
        {
            this->reset();
        }
    }
};

template <class Base>
struct NonTrivialCopy : Base
{
    using Base::Base;

    NonTrivialCopy() = default;
#if CARB_COMPILER_MSC // MSVC can evaluate the noexcept operator, but GCC errors when compiling it
    NonTrivialCopy(const NonTrivialCopy& from) noexcept(noexcept(Base::constructFrom(static_cast<const Base&>(from))))
#else // for GCC, use the same clause as Base::constructFrom
    NonTrivialCopy(const NonTrivialCopy& from) noexcept(
        std::is_nothrow_constructible<typename Base::value_type, decltype(from.value)>::value)
#endif
    {
        Base::constructFrom(static_cast<const Base&>(from));
    }
};

// If T is copy-constructible and not trivially copy-constructible, select NonTrivialCopy,
// otherwise use the base OptionalConstructor
template <class Base, class... Types>
using SelectCopy =
    typename std::conditional_t<conjunction<std::is_copy_constructible<Types>...,
                                            negation<conjunction<std::is_trivially_copy_constructible<Types>...>>>::value,
                                NonTrivialCopy<Base>,
                                Base>;

template <class Base, class... Types>
struct NonTrivialMove : SelectCopy<Base, Types...>
{
    using BaseClass = SelectCopy<Base, Types...>;
    using BaseClass::BaseClass;

    NonTrivialMove() = default;
    NonTrivialMove(const NonTrivialMove&) = default;
#if CARB_COMPILER_MSC // MSVC can evaluate the noexcept operator, but GCC errors when compiling it
    NonTrivialMove(NonTrivialMove&& from) noexcept(noexcept(BaseClass::constructFrom(static_cast<Base&&>(from))))
#else // for GCC, use the same clause as Base::constructFrom
    NonTrivialMove(NonTrivialMove&& from) noexcept(
        std::is_nothrow_constructible<typename Base::value_type, decltype(static_cast<Base&&>(from).value)>::value)
#endif
    {
        BaseClass::constructFrom(static_cast<Base&&>(from));
    }
    NonTrivialMove& operator=(const NonTrivialMove&) = default;
    NonTrivialMove& operator=(NonTrivialMove&&) = default;
};

// If T is move-constructible and not trivially move-constructible, select NonTrivialMove,
// otherwise use the selected Copy struct.
template <class Base, class... Types>
using SelectMove =
    typename std::conditional_t<conjunction<std::is_move_constructible<Types>...,
                                            negation<conjunction<std::is_trivially_move_constructible<Types>...>>>::value,
                                NonTrivialMove<Base, Types...>,
                                SelectCopy<Base, Types...>>;

template <class Base, class... Types>
struct NonTrivialCopyAssign : SelectMove<Base, Types...>
{
    using BaseClass = SelectMove<Base, Types...>;
    using BaseClass::BaseClass;

    NonTrivialCopyAssign() = default;
    NonTrivialCopyAssign(const NonTrivialCopyAssign&) = default;
    NonTrivialCopyAssign(NonTrivialCopyAssign&&) = default;

    NonTrivialCopyAssign& operator=(const NonTrivialCopyAssign& from) noexcept(
        noexcept(BaseClass::assignFrom(static_cast<const Base&>(from))))
    {
        BaseClass::assignFrom(static_cast<const Base&>(from));
        return *this;
    }
    NonTrivialCopyAssign& operator=(NonTrivialCopyAssign&&) = default;
};

template <class Base, class... Types>
struct DeletedCopyAssign : SelectMove<Base, Types...>
{
    using BaseClass = SelectMove<Base, Types...>;
    using BaseClass::BaseClass;

    DeletedCopyAssign() = default;
    DeletedCopyAssign(const DeletedCopyAssign&) = default;
    DeletedCopyAssign(DeletedCopyAssign&&) = default;
    DeletedCopyAssign& operator=(const DeletedCopyAssign&) = delete;
    DeletedCopyAssign& operator=(DeletedCopyAssign&&) = default;
};

// For selecting the proper copy-assign class, things get a bit more complicated:
// - If T is trivially destructible and trivially copy-constructible and trivially copy-assignable:
//   * We use the Move struct selected above
// - Otherwise, if T is copy-constructible and copy-assignable:
//   * We select the NonTrivialCopyAssign struct
// - If all else fails, the class is not copy-assignable, so select DeletedCopyAssign
template <class Base, class... Types>
using SelectCopyAssign = typename std::conditional_t<
    conjunction<std::is_trivially_destructible<Types>...,
                std::is_trivially_copy_constructible<Types>...,
                std::is_trivially_copy_assignable<Types>...>::value,
    SelectMove<Base, Types...>,
    typename std::conditional_t<conjunction<std::is_copy_constructible<Types>..., std::is_copy_assignable<Types>...>::value,
                                NonTrivialCopyAssign<Base, Types...>,
                                DeletedCopyAssign<Base, Types...>>>;

template <class Base, class... Types>
struct NonTrivialMoveAssign : SelectCopyAssign<Base, Types...>
{
    using BaseClass = SelectCopyAssign<Base, Types...>;
    using BaseClass::BaseClass;

    NonTrivialMoveAssign() = default;
    NonTrivialMoveAssign(const NonTrivialMoveAssign&) = default;
    NonTrivialMoveAssign(NonTrivialMoveAssign&&) = default;
    NonTrivialMoveAssign& operator=(const NonTrivialMoveAssign&) = default;

    NonTrivialMoveAssign& operator=(NonTrivialMoveAssign&& from) noexcept(
        noexcept(BaseClass::assignFrom(static_cast<const Base&&>(from))))
    {
        BaseClass::assignFrom(static_cast<Base&&>(from));
        return *this;
    }
};

template <class Base, class... Types>
struct DeletedMoveAssign : SelectCopyAssign<Base, Types...>
{
    using BaseClass = SelectCopyAssign<Base, Types...>;
    using BaseClass::BaseClass;

    DeletedMoveAssign() = default;
    DeletedMoveAssign(const DeletedMoveAssign&) = default;
    DeletedMoveAssign(DeletedMoveAssign&&) = default;
    DeletedMoveAssign& operator=(const DeletedMoveAssign&) = default;
    DeletedMoveAssign& operator=(DeletedMoveAssign&&) = delete;
};

// Selecting the proper move-assign struct is equally complicated:
// - If T is trivially destructible, trivially move-constructible and trivially move-assignable:
//   * We use the CopyAssign struct selected above
// - If T is move-constructible and move-assignable:
//   * We select the NonTrivialMoveAssign struct
// - If all else fails, T is not move-assignable, so select DeletedMoveAssign
template <class Base, class... Types>
using SelectMoveAssign = typename std::conditional_t<
    conjunction<std::is_trivially_destructible<Types>...,
                std::is_trivially_move_constructible<Types>...,
                std::is_trivially_move_assignable<Types>...>::value,
    SelectCopyAssign<Base, Types...>,
    typename std::conditional_t<conjunction<std::is_move_constructible<Types>..., std::is_move_assignable<Types>...>::value,
                                NonTrivialMoveAssign<Base, Types...>,
                                DeletedMoveAssign<Base, Types...>>>;

// An alias for constructing our struct hierarchy to wrap T
template <class Base, class... Types>
using SelectHierarchy = SelectMoveAssign<Base, Types...>;

// Helpers for determining which operators can be enabled
template <class T>
using EnableIfBoolConvertible = typename std::enable_if_t<std::is_convertible<T, bool>::value, int>;

template <class L, class R>
using EnableIfComparableWithEqual =
    EnableIfBoolConvertible<decltype(std::declval<const L&>() == std::declval<const R&>())>;

template <class L, class R>
using EnableIfComparableWithNotEqual =
    EnableIfBoolConvertible<decltype(std::declval<const L&>() != std::declval<const R&>())>;

template <class L, class R>
using EnableIfComparableWithLess =
    EnableIfBoolConvertible<decltype(std::declval<const L&>() < std::declval<const R&>())>;

template <class L, class R>
using EnableIfComparableWithGreater =
    EnableIfBoolConvertible<decltype(std::declval<const L&>() > std::declval<const R&>())>;

template <class L, class R>
using EnableIfComparableWithLessEqual =
    EnableIfBoolConvertible<decltype(std::declval<const L&>() <= std::declval<const R&>())>;

template <class L, class R>
using EnableIfComparableWithGreaterEqual =
    EnableIfBoolConvertible<decltype(std::declval<const L&>() >= std::declval<const R&>())>;

} // namespace detail
} // namespace cpp17
} // namespace carb
