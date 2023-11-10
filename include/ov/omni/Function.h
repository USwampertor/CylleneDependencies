// Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! \file
//! \brief Implementation of \c omni::function.

#pragma once

#include <functional>

#include "detail/FunctionImpl.h"
#include "../carb/detail/NoexceptType.h"

namespace omni
{

//! Thrown when \c function::operator() is called but the function is unbound.
using std::bad_function_call;

template <typename Signature>
class function;

//! A polymorphic function wrapper which is compatible with \c std::function.
//!
//! For most usage, it is equivalent to the \c std::function API, but there are a few minor changes for ABI stability
//! and C interoperation.
//!
//! * The \c target and \c target_type functions are not available. These rely on RTTI, which is not required for
//!   Omniverse builds and is not ABI safe.
//! * No support for allocators, as these were removed in C++17. The \c carbReallocate function is used for all memory
//!   management needs.
//! * A local buffer for storing functors. While the C++ Standard encourages this, this implementation keeps the size
//!   and alignment fixed.
//! * An \c omni::function is trivially-relocatable.
template <typename TReturn, typename... TArgs>
class function<TReturn(TArgs...)> : private ::omni::detail::FunctionData
{
    using base_type = ::omni::detail::FunctionData;
    using traits = ::omni::detail::FunctionTraits<TReturn(TArgs...)>;

public:
    //! The return type of this function.
    using result_type = typename traits::result_type;

public:
    //! Create an unbound instance.
    function(std::nullptr_t) noexcept : base_type(nullptr)
    {
    }

    //! Create an unbound instance.
    function() noexcept : function(nullptr)
    {
    }

    //! Create an instance as a copy of \a other.
    //!
    //! If \a other was bound before the call, then the created instance will be bound to the same target.
    function(function const& other) = default;

    //! Create an instance by moving from \a other.
    //!
    //! If \a other was bound before the call, then the created instance will be bound to the same target. After this
    //! call, \a other will be unbound.
    function(function&& other) noexcept = default;

    //! Reset this function wrapper to unbound.
    function& operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    //! Copy the \a other instance into this one.
    //!
    //! If copying the function fails, both instances will remain unchanged.
    function& operator=(function const& other) = default;

    //! Move the \a other instance into this one.
    function& operator=(function&& other) noexcept = default;

    CARB_DETAIL_PUSH_IGNORE_NOEXCEPT_TYPE()
    //! The binding constructor for function, which wraps \a f for invocation.
    //!
    //! The created instance will have a "target type" (referred to as \c Target here) of \c std::decay_t<Target> which
    //! is responsible for invoking the function. The following descriptions use a common set of symbols to describe
    //! types:
    //!
    //! * \c UReturn -- A return type which is the same as or implicitly convertible from \c TReturn. If \c UReturn
    //!   is \c void, then \c TReturn must be implicitly discardable.
    //! * \c UArgs... -- An argument pack where all arguments are implicitly convertible to \c TArgs... .
    //! * \c UArg0Obj -- The object type of the possibly-dereferenced first type in the \c TArgs... pack. In other
    //!   words, if \c TArgs... was `{Object*, int}`, then \c UArg0Obj would be \c Object. Any cv-qualifiers follow the
    //!   dereference (e.g.: if \c TArg0 is `Object const*`, then \c UArg0Obj is `Object const`).
    //! * \c UArg1N... -- Same as \c UArgs... but do not include the first type in the pack.
    //!
    //! The \c Target can be one of the following:
    //!
    //! 1. A function pointer -- \c UReturn(*)(UArgs...)
    //! 2. A pointer to a member function -- `UReturn UArg0Obj::*(UArg1N...)`
    //! 3. A pointer to member data -- `UReturn UArg0Obj::*` when `sizeof(TArgs...) == 1`
    //! 4. Another `omni::function` -- `omni::function<UReturn(UArgs...)>` where \c UReturn and \c UArgs... are
    //!    different from \c TReturn and \c TArgs... (the exact same type uses a copy or move operation)
    //! 5. An `std::function` -- `std::function<UReturn(UArgs...)>`, but \c UReturn and \c UArgs... can all match
    //!    \c TReturn and \c TArgs...
    //! 6. A functor with \c operator() accepting \c UArgs... and returning \c UReturn
    //!
    //! If the \c Target does not match any of preceding possibilities, this function does not participate in overload
    //! resolution in a SFINAE-safe manner. If \c Target is non-copyable, then the program is ill-formed.
    //!
    //! For cases 1-5, if `f == nullptr`, then the function is initialized as unbound. Note the quirky case on #6, where
    //! the composite `omni::function<void()>{std::function<void()>{}}` will create an unbound function. But this is not
    //! commutative, as `std::function<void()>{omni::function<void()>{}}` will create a bound function which throws when
    //! called.
    template <typename F, typename Assigner = ::omni::detail::FunctionAssigner<traits, F>>
    function(F&& f) : base_type(Assigner{}, std::forward<F>(f))
    {
    }

    //! Bind this instance to \a f according to the rules in the binding constructor.
    //!
    //! If the process of binding a \c function to \a f fails, this instance will remain unchanged.
    template <typename F, typename Assigner = ::omni::detail::FunctionAssigner<traits, F>>
    function& operator=(F&& f)
    {
        return operator=(function{ std::forward<F>(f) });
    }
    CARB_DETAIL_POP_IGNORE_NOEXCEPT_TYPE()

    //! If this function has context it needs to destroy, then perform that cleanup.
    ~function() noexcept = default;

    //! Invoke this function with the provided \a args.
    //!
    //! If this function is not bound (see `operator bool() const`), the program will throw \c bad_function_call if
    //! exceptions are enabled or panic if they are not.
    TReturn operator()(TArgs... args) const
    {
        if (m_trampoline == nullptr)
            ::omni::detail::null_function_call();

        auto f = reinterpret_cast<TReturn (*)(::omni::detail::FunctionBuffer const*, TArgs...)>(m_trampoline);
        return f(&m_buffer, std::forward<TArgs>(args)...);
    }

    //! Check that this function is bound.
    explicit operator bool() const noexcept
    {
        return bool(m_trampoline);
    }

    //! Swap the contents of \a other with this one.
    void swap(function& other) noexcept
    {
        base_type::swap(other);
    }
};

//! Check if \a func is not activated.
template <typename TReturn, typename... TArgs>
bool operator==(function<TReturn(TArgs...)> const& func, std::nullptr_t) noexcept
{
    return !func;
}

//! Check if \a func is not activated.
template <typename TReturn, typename... TArgs>
bool operator==(std::nullptr_t, function<TReturn(TArgs...)> const& func) noexcept
{
    return !func;
}

//! Check if \a func is activated.
template <typename TReturn, typename... TArgs>
bool operator!=(function<TReturn(TArgs...)> const& func, std::nullptr_t) noexcept
{
    return static_cast<bool>(func);
}

//! Check if \a func is activated.
template <typename TReturn, typename... TArgs>
bool operator!=(std::nullptr_t, function<TReturn(TArgs...)> const& func) noexcept
{
    return static_cast<bool>(func);
}

//! Swap \a a and \a b by \c function::swap.
template <typename TReturn, typename... TArgs>
void swap(function<TReturn(TArgs...)>& a, function<TReturn(TArgs...)>& b) noexcept
{
    a.swap(b);
}

} // namespace omni
