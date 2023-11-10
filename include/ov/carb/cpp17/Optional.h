// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
// Implements std::optional from C++17 using C++14 paradigms.
// Heavily borrowed from MS STL: https://github.com/microsoft/STL/blob/master/stl/inc/optional
#pragma once

#include "../Defines.h"
#include "Utility.h"

#define CARB_IMPLOPTIONAL
#include "ImplOptional.h"
#undef CARB_IMPLOPTIONAL

namespace carb
{
//! Namespace for C++17 features using C++14 semantics.
namespace cpp17
{

struct nullopt_t
{
    struct Tag
    {
    };
    explicit constexpr nullopt_t(Tag)
    {
    }
};

static constexpr nullopt_t nullopt{ nullopt_t::Tag{} };

#if CARB_EXCEPTIONS_ENABLED
class bad_optional_access final : public std::exception
{
public:
    bad_optional_access() noexcept = default;
    bad_optional_access(const bad_optional_access&) noexcept = default;
    bad_optional_access& operator=(const bad_optional_access&) noexcept = default;
    virtual const char* what() const noexcept override
    {
        return "bad optional access";
    }
};
#endif

template <class T>
class CARB_VIZ optional : private detail::SelectHierarchy<detail::OptionalConstructor<T>, T>
{
    using BaseClass = detail::SelectHierarchy<detail::OptionalConstructor<T>, T>;

    static_assert(!std::is_same<std::remove_cv_t<T>, nullopt_t>::value &&
                      !std::is_same<std::remove_cv_t<T>, in_place_t>::value,
                  "T may not be nullopt_t or inplace_t");
    static_assert(std::is_object<T>::value && std::is_destructible<T>::value && !std::is_array<T>::value,
                  "T does not meet Cpp17Destructible requirements");

    // Essentially: !is_same(U, optional) && !is_same(U, in_place_t) && is_constructible(T from U)
    template <class U>
    using AllowDirectConversion = bool_constant<
        conjunction<negation<std::is_same<typename std::remove_reference_t<typename std::remove_cv_t<U>>, optional>>,
                    negation<std::is_same<typename std::remove_reference_t<typename std::remove_cv_t<U>>, in_place_t>>,
                    std::is_constructible<T, U>>::value>;

    // Essentially: !(is_same(T, U) || is_constructible(T from optional<U>) || is_convertible(optional<U> to T))
    template <class U>
    struct AllowUnwrapping : bool_constant<!disjunction<std::is_same<T, U>,
                                                        std::is_constructible<T, optional<U>&>,
                                                        std::is_constructible<T, const optional<U>&>,
                                                        std::is_constructible<T, const optional<U>>,
                                                        std::is_constructible<T, optional<U>>,
                                                        std::is_convertible<optional<U>&, T>,
                                                        std::is_convertible<const optional<U>&, T>,
                                                        std::is_convertible<const optional<U>, T>,
                                                        std::is_convertible<optional<U>, T>>::value>
    {
    };

    // Essentially: !(is_same(T, U) || is_assignable(T& from optional<U>))
    template <class U>
    struct AllowUnwrappingAssignment : bool_constant<!disjunction<std::is_same<T, U>,
                                                                  std::is_assignable<T&, optional<U>&>,
                                                                  std::is_assignable<T&, const optional<U>&>,
                                                                  std::is_assignable<T&, const optional<U>>,
                                                                  std::is_assignable<T&, optional<U>>>::value>
    {
    };

    [[noreturn]] static void onBadAccess()
    {
#if CARB_EXCEPTIONS_ENABLED
        throw bad_optional_access();
#else
        CARB_FATAL_UNLESS(0, "bad optional access");
#endif
    }

public:
    using value_type = T;

    constexpr optional() noexcept
    {
    }
    constexpr optional(nullopt_t) noexcept
    {
    }

    optional(const optional& other) : BaseClass(static_cast<const BaseClass&>(other))
    {
    }

    optional(optional&& other) noexcept(std::is_nothrow_move_constructible<T>::value)
        : BaseClass(static_cast<BaseClass&&>(std::move(other)))
    {
    }

    optional& operator=(const optional& other)
    {
        if (other)
            this->assign(*other);
        else
            reset();
        return *this;
    }

    optional& operator=(optional&& other) noexcept(
        std::is_nothrow_move_assignable<T>::value&& std::is_nothrow_move_constructible<T>::value)
    {
        if (other)
            this->assign(std::move(*other));
        else
            reset();
        return *this;
    }

    // The spec states that this is conditionally-explicit, which is a C++20 feature, so we have to work around it by
    // having two functions with SFINAE
    template <class U,
              typename std::enable_if_t<
                  conjunction<AllowUnwrapping<U>, std::is_constructible<T, const U&>, std::is_convertible<const U&, T>>::value,
                  int> = 0>
    optional(const optional<U>& other)
    {
        if (other)
            this->construct(*other);
    }
    template <
        class U,
        typename std::enable_if_t<
            conjunction<AllowUnwrapping<U>, std::is_constructible<T, const U&>, negation<std::is_convertible<const U&, T>>>::value,
            int> = 0>
    explicit optional(const optional<U>& other)
    {
        if (other)
            this->construct(*other);
    }

    // The spec states that this is conditionally-explicit, which is a C++20 feature, so we have to work around it by
    // having two functions with SFINAE
    template <
        class U,
        typename std::enable_if_t<conjunction<AllowUnwrapping<U>, std::is_constructible<T, U>, std::is_convertible<U, T>>::value,
                                  int> = 0>
    optional(optional<U>&& other)
    {
        if (other)
            this->construct(std::move(*other));
    }
    template <class U,
              typename std::enable_if_t<
                  conjunction<AllowUnwrapping<U>, std::is_constructible<T, U>, negation<std::is_convertible<U, T>>>::value,
                  int> = 0>
    explicit optional(optional<U>&& other)
    {
        if (other)
            this->construct(std::move(*other));
    }

    template <class... Args, typename std::enable_if_t<std::is_constructible<T, Args...>::value, int> = 0>
    optional(in_place_t, Args&&... args) : BaseClass(in_place, std::forward<Args>(args)...)
    {
    }

    template <class U,
              class... Args,
              typename std::enable_if_t<std::is_constructible<T, std::initializer_list<U>&, Args...>::value, int> = 0>
    optional(in_place_t, std::initializer_list<U> ilist, Args&&... args)
        : BaseClass(in_place, ilist, std::forward<Args>(args)...)
    {
    }

    // The spec states that this is conditionally-explicit, which is a C++20 feature, so we have to work around it by
    // having two functions with SFINAE
    template <class U = value_type,
              typename std::enable_if_t<conjunction<AllowDirectConversion<U>, std::is_convertible<U, T>>::value, int> = 0>
    constexpr optional(U&& value) : BaseClass(in_place, std::forward<U>(value))
    {
    }
    template <class U = value_type,
              typename std::enable_if_t<conjunction<AllowDirectConversion<U>, negation<std::is_convertible<U, T>>>::value, int> = 0>
    constexpr explicit optional(U&& value) : BaseClass(in_place, std::forward<U>(value))
    {
    }

    ~optional() = default;

    optional& operator=(nullopt_t) noexcept
    {
        reset();
        return *this;
    }

    template <class U = T,
              typename std::enable_if_t<
                  conjunction<negation<std::is_same<optional, typename std::remove_cv_t<typename std::remove_reference_t<U>>>>,
                              negation<conjunction<std::is_scalar<T>, std::is_same<T, typename std::decay_t<U>>>>,
                              std::is_constructible<T, U>,
                              std::is_assignable<T&, U>>::value,
                  int> = 0>
    optional& operator=(U&& value)
    {
        this->assign(std::forward<U>(value));
        return *this;
    }

    template <
        class U,
        typename std::enable_if_t<
            conjunction<AllowUnwrappingAssignment<U>, std::is_constructible<T, const U&>, std::is_assignable<T&, const U&>>::value,
            int> = 0>
    optional& operator=(const optional<U>& other)
    {
        if (other)
            this->assign(*other);
        else
            reset();
        return *this;
    }

    template <class U,
              typename std::enable_if_t<
                  conjunction<AllowUnwrappingAssignment<U>, std::is_constructible<T, U>, std::is_assignable<T&, U>>::value,
                  int> = 0>
    optional& operator=(optional<U>&& other)
    {
        if (other)
            this->assign(std::move(*other));
        else
            reset();
        return *this;
    }

    constexpr const T* operator->() const
    {
        return std::addressof(this->val());
    }
    constexpr T* operator->()
    {
        return std::addressof(this->val());
    }

    constexpr const T& operator*() const&
    {
        return this->val();
    }

    constexpr T& operator*() &
    {
        return this->val();
    }

    constexpr const T&& operator*() const&&
    {
        return std::move(this->val());
    }

    constexpr T&& operator*() &&
    {
        return std::move(this->val());
    }

    constexpr explicit operator bool() const noexcept
    {
        return this->hasValue;
    }
    constexpr bool has_value() const noexcept
    {
        return this->hasValue;
    }

    constexpr const T& value() const&
    {
        if (!this->hasValue)
            onBadAccess();
        return this->val();
    }

    constexpr T& value() &
    {
        if (!this->hasValue)
            onBadAccess();
        return this->val();
    }

    constexpr const T&& value() const&&
    {
        if (!this->hasValue)
            onBadAccess();
        return std::move(this->val());
    }

    constexpr T&& value() &&
    {
        if (!this->hasValue)
            onBadAccess();
        return std::move(this->val());
    }

    template <class U>
    constexpr typename std::remove_cv_t<T> value_or(U&& default_value) const&
    {
        static_assert(
            std::is_convertible<const T&, typename std::remove_cv_t<T>>::value,
            "The const overload of optional<T>::value_or() requires const T& to be convertible to std::remove_cv_t<T>");
        static_assert(std::is_convertible<U, T>::value, "optional<T>::value_or() requires U to be convertible to T");

        if (this->hasValue)
            return this->val();

        return static_cast<typename std::remove_cv_t<T>>(std::forward<U>(default_value));
    }
    template <class U>
    constexpr typename std::remove_cv_t<T> value_or(U&& default_value) &&
    {
        static_assert(
            std::is_convertible<T, typename std::remove_cv_t<T>>::value,
            "The rvalue overload of optional<T>::value_or() requires T to be convertible to std::remove_cv_t<T>");
        static_assert(std::is_convertible<U, T>::value, "optional<T>::value_or() requires U to be convertible to T");

        if (this->hasValue)
            return this->val();

        return static_cast<typename std::remove_cv_t<T>>(std::forward<U>(default_value));
    }

    void swap(optional& other) noexcept(std::is_nothrow_move_constructible<T>::value&& is_nothrow_swappable<T>::value)
    {
        static_assert(std::is_move_constructible<T>::value, "T must be move constructible");
        static_assert(!std::is_move_constructible<T>::value || is_swappable<T>::value, "T must be swappable");

        const bool engaged = this->hasValue;
        if (engaged == other.hasValue)
        {
            if (engaged)
            {
                using std::swap; // Enable ADL
                swap(**this, *other);
            }
        }
        else
        {
            optional& source = engaged ? *this : other;
            optional& target = engaged ? other : *this;
            target.construct(std::move(*source));
            source.reset();
        }
    }

    using BaseClass::reset;

    template <class... Args>
    T& emplace(Args&&... args)
    {
        reset();
        return this->construct(std::forward<Args>(args)...);
    }
    template <class U,
              class... Args,
              typename std::enable_if_t<std::is_constructible<T, std::initializer_list<U>&, Args...>::value, int> = 0>
    T& emplace(std::initializer_list<U> ilist, Args&&... args)
    {
        reset();
        return this->construct(ilist, std::forward<Args>(args)...);
    }
};

template <class T, class U>
constexpr bool operator==(const optional<T>& lhs, const optional<U>& rhs)
{
    const bool lhv = lhs.has_value();
    return lhv == rhs.has_value() && (!lhv || *lhs == *rhs);
}
template <class T, class U>
constexpr bool operator!=(const optional<T>& lhs, const optional<U>& rhs)
{
    const bool lhv = lhs.has_value();
    return lhv != rhs.has_value() || (lhv && *lhs != *rhs);
}
template <class T, class U>
constexpr bool operator<(const optional<T>& lhs, const optional<U>& rhs)
{
    return rhs.has_value() && (!lhs.has_value() || *lhs < *rhs);
}
template <class T, class U>
constexpr bool operator<=(const optional<T>& lhs, const optional<U>& rhs)
{
    return !lhs.has_value() || (rhs.has_value() && *lhs <= *rhs);
}
template <class T, class U>
constexpr bool operator>(const optional<T>& lhs, const optional<U>& rhs)
{
    return lhs.has_value() && (!rhs.has_value() || *lhs > *rhs);
}
template <class T, class U>
constexpr bool operator>=(const optional<T>& lhs, const optional<U>& rhs)
{
    return !rhs.has_value() || (lhs.has_value() && *lhs >= *rhs);
}

template <class T>
constexpr bool operator==(const optional<T>& opt, nullopt_t) noexcept
{
    return !opt.has_value();
}
template <class T>
constexpr bool operator==(nullopt_t, const optional<T>& opt) noexcept
{
    return !opt.has_value();
}
template <class T>
constexpr bool operator!=(const optional<T>& opt, nullopt_t) noexcept
{
    return opt.has_value();
}
template <class T>
constexpr bool operator!=(nullopt_t, const optional<T>& opt) noexcept
{
    return opt.has_value();
}
template <class T>
constexpr bool operator<(const optional<T>& opt, nullopt_t) noexcept
{
    CARB_UNUSED(opt);
    return false;
}
template <class T>
constexpr bool operator<(nullopt_t, const optional<T>& opt) noexcept
{
    return opt.has_value();
}
template <class T>
constexpr bool operator<=(const optional<T>& opt, nullopt_t) noexcept
{
    return !opt.has_value();
}
template <class T>
constexpr bool operator<=(nullopt_t, const optional<T>& opt) noexcept
{
    CARB_UNUSED(opt);
    return true;
}
template <class T>
constexpr bool operator>(const optional<T>& opt, nullopt_t) noexcept
{
    return opt.has_value();
}
template <class T>
constexpr bool operator>(nullopt_t, const optional<T>& opt) noexcept
{
    CARB_UNUSED(opt);
    return false;
}
template <class T>
constexpr bool operator>=(const optional<T>& opt, nullopt_t) noexcept
{
    CARB_UNUSED(opt);
    return true;
}
template <class T>
constexpr bool operator>=(nullopt_t, const optional<T>& opt) noexcept
{
    return !opt.has_value();
}

template <class T, class U, detail::EnableIfComparableWithEqual<T, U> = 0>
constexpr bool operator==(const optional<T>& opt, const U& value)
{
    return opt ? *opt == value : false;
}
template <class T, class U, detail::EnableIfComparableWithEqual<T, U> = 0>
constexpr bool operator==(const T& value, const optional<U>& opt)
{
    return opt ? *opt == value : false;
}
template <class T, class U, detail::EnableIfComparableWithNotEqual<T, U> = 0>
constexpr bool operator!=(const optional<T>& opt, const U& value)
{
    return opt ? *opt != value : true;
}
template <class T, class U, detail::EnableIfComparableWithNotEqual<T, U> = 0>
constexpr bool operator!=(const T& value, const optional<U>& opt)
{
    return opt ? *opt != value : true;
}
template <class T, class U, detail::EnableIfComparableWithLess<T, U> = 0>
constexpr bool operator<(const optional<T>& opt, const U& value)
{
    return opt ? *opt < value : true;
}
template <class T, class U, detail::EnableIfComparableWithLess<T, U> = 0>
constexpr bool operator<(const T& value, const optional<U>& opt)
{
    return opt ? value < *opt : false;
}
template <class T, class U, detail::EnableIfComparableWithLessEqual<T, U> = 0>
constexpr bool operator<=(const optional<T>& opt, const U& value)
{
    return opt ? *opt <= value : true;
}
template <class T, class U, detail::EnableIfComparableWithLessEqual<T, U> = 0>
constexpr bool operator<=(const T& value, const optional<U>& opt)
{
    return opt ? value <= *opt : false;
}
template <class T, class U, detail::EnableIfComparableWithGreater<T, U> = 0>
constexpr bool operator>(const optional<T>& opt, const U& value)
{
    return opt ? *opt > value : false;
}
template <class T, class U, detail::EnableIfComparableWithGreater<T, U> = 0>
constexpr bool operator>(const T& value, const optional<U>& opt)
{
    return opt ? value > *opt : true;
}
template <class T, class U, detail::EnableIfComparableWithGreaterEqual<T, U> = 0>
constexpr bool operator>=(const optional<T>& opt, const U& value)
{
    return opt ? *opt >= value : false;
}
template <class T, class U, detail::EnableIfComparableWithGreaterEqual<T, U> = 0>
constexpr bool operator>=(const T& value, const optional<U>& opt)
{
    return opt ? value >= *opt : true;
}

template <class T, typename std::enable_if_t<std::is_move_constructible<T>::value && is_swappable<T>::value, int> = 0>
void swap(optional<T>& lhs, optional<T>& rhs) noexcept(noexcept(lhs.swap(rhs)))
{
    lhs.swap(rhs);
}

template <class T>
constexpr optional<typename std::decay_t<T>> make_optional(T&& value)
{
    return optional<typename std::decay_t<T>>{ std::forward<T>(value) };
}
template <class T, class... Args>
constexpr optional<T> make_optional(Args&&... args)
{
    return optional<T>{ in_place, std::forward<Args>(args)... };
}
template <class T, class U, class... Args>
constexpr optional<T> make_optional(std::initializer_list<U> il, Args&&... args)
{
    return optional<T>{ in_place, il, std::forward<Args>(args)... };
}

} // namespace cpp17
} // namespace carb
