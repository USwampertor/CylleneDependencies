// Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "Defines.h"

#include "cpp17/TypeTraits.h"

#include <typeindex> // for std::hash
#include <ostream> // for std::basic_ostream

/**
 * Implements a strong type. `typedef` and `using` declarations do not declare a new type. `typedef int MyType` uses the
 * name `MyType` to refer to int; MyType and int are therefore interchangeable.
 *
 * CARB_STRONGTYPE(MyType, int) differs in that it creates an int-like structure named MyType which is type-safe. MyType
 * can be compared to `int` values, but cannot be implicitly assigned an int.
 */

#define CARB_STRONGTYPE(Name, T) using Name = ::carb::Strong<T, struct Name##Sig>

namespace carb
{

// clang-format off
template<class T, class Sig> class Strong final
{
private:
    T val;
public:
    using Type = T;
    constexpr Strong() : val{} {}
    constexpr explicit Strong(T&& val_) : val(std::forward<T>(val_)) {}
    constexpr Strong(const Strong& rhs) = default;
    Strong& operator=(const Strong& rhs) = default;
    constexpr Strong(Strong&& rhs) = default;
    Strong& operator=(Strong&& rhs) = default;
    const T& get() const { return val; }
    T& get() { return val; }
    /// Ensure that the underlying type matches expected; recommended for printf
    template <class U>
    U ensure() const { static_assert(std::is_same<T, U>::value, "Types are not the same"); return val; }
    explicit operator bool () const { return !!val; }
    bool operator == (const Strong& rhs) const { return val == rhs.val; }
    bool operator == (const T& rhs) const { return val == rhs; }
    bool operator != (const Strong& rhs) const { return val != rhs.val; }
    bool operator != (const T& rhs) const { return val != rhs; }
    bool operator < (const Strong& rhs) const { return val < rhs.val; }
    void swap(Strong& rhs) noexcept(noexcept(std::swap(val, rhs.val))) { std::swap(val, rhs.val); }
};
// clang-format on

template <class CharT, class Traits, class T, class Sig>
::std::basic_ostream<CharT, Traits>& operator<<(::std::basic_ostream<CharT, Traits>& o, const Strong<T, Sig>& s)
{
    o << s.get();
    return o;
}

// Swap can be specialized with ADL
template <class T, class Sig, typename = std::enable_if_t<carb::cpp17::is_swappable<T>::value, bool>>
void swap(Strong<T, Sig>& lhs, Strong<T, Sig>& rhs) noexcept(noexcept(lhs.swap(rhs)))
{
    lhs.swap(rhs);
}

} // namespace carb

// Specialization for std::hash
template <class T, class Sig>
struct std::hash<::carb::Strong<T, Sig>>
{
    size_t operator()(const ::carb::Strong<T, Sig>& v) const
    {
        return ::std::hash<T>{}(v.get());
    }
};
