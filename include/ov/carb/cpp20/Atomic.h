// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
// Implements the wait/notify functions from C++20-standard atomics for types that are 1, 2, 4 or 8 bytes. If this
// feature is not desired, or the type you're trying to atomic-wrap isn't supported, use std::atomic instead.
// See the following:
// https://en.cppreference.com/w/cpp/atomic/atomic/wait
// https://en.cppreference.com/w/cpp/atomic/atomic/notify_one
// https://en.cppreference.com/w/cpp/atomic/atomic/notify_all
#pragma once

#include "../thread/Futex.h"
#include "../thread/Util.h"

#include <type_traits>
#include <algorithm>

namespace carb
{

namespace cpp20
{

template <class T>
class atomic;

namespace detail
{

// C++20 adds fetch_add/fetch_sub and operator support for floating point types
template <class T>
class atomic_float_facade : public std::atomic<T>
{
    using Base = std::atomic<T>;
    static_assert(std::is_floating_point<T>::value, "");

public:
    atomic_float_facade() noexcept = default;
    constexpr atomic_float_facade(T desired) noexcept : Base(desired)
    {
    }
    atomic_float_facade(const atomic_float_facade&) = delete;

    using Base::operator=;

    T fetch_add(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        T temp = this->load(std::memory_order_relaxed);
        while (!this->compare_exchange_strong(temp, temp + arg, order, std::memory_order_relaxed))
        {
        }

        return temp;
    }
    T fetch_add(T arg, std::memory_order order = std::memory_order_seq_cst) volatile noexcept
    {
        T temp = this->load(std::memory_order_relaxed);
        while (!this->compare_exchange_strong(temp, temp + arg, order, std::memory_order_relaxed))
        {
        }

        return temp;
    }
    T fetch_sub(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        T temp = this->load(std::memory_order_relaxed);
        while (!this->compare_exchange_strong(temp, temp - arg, order, std::memory_order_relaxed))
        {
        }

        return temp;
    }
    T fetch_sub(T arg, std::memory_order order = std::memory_order_seq_cst) volatile noexcept
    {
        T temp = this->load(std::memory_order_relaxed);
        while (!this->compare_exchange_strong(temp, temp - arg, order, std::memory_order_relaxed))
        {
        }

        return temp;
    }

    T operator+=(T arg) noexcept
    {
        return this->fetch_add(arg) + arg;
    }
    T operator+=(T arg) volatile noexcept
    {
        return this->fetch_add(arg) + arg;
    }
    T operator-=(T arg) noexcept
    {
        return this->fetch_sub(arg) - arg;
    }
    T operator-=(T arg) volatile noexcept
    {
        return this->fetch_sub(arg) - arg;
    }
};

template <class T>
class atomic_ref_base
{
protected:
    using AtomicType = ::carb::cpp20::atomic<T>;
    AtomicType& m_ref;

public:
    using value_type = T;

    static constexpr bool is_always_lock_free = AtomicType::is_always_lock_free;

    static constexpr std::size_t required_alignment = alignof(T);

    explicit atomic_ref_base(T& obj) : m_ref(reinterpret_cast<AtomicType&>(obj))
    {
    }
    atomic_ref_base(const atomic_ref_base& ref) noexcept : m_ref(ref.m_ref)
    {
    }

    T operator=(T desired) const noexcept
    {
        m_ref.store(desired);
        return desired;
    }
    atomic_ref_base& operator=(const atomic_ref_base&) = delete;

    bool is_lock_free() const noexcept
    {
        return m_ref.is_lock_free();
    }

    void store(T desired, std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        m_ref.store(desired, order);
    }

    T load(std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return m_ref.load(order);
    }

    operator T() const noexcept
    {
        return load();
    }

    T exchange(T desired, std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return m_ref.exchange(desired, order);
    }

    bool compare_exchange_weak(T& expected, T desired, std::memory_order success, std::memory_order failure) const noexcept
    {
        return m_ref.compare_exchange_weak(expected, desired, success, failure);
    }
    bool compare_exchange_weak(T& expected, T desired, std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return m_ref.compare_exchange_weak(expected, desired, order);
    }
    bool compare_exchange_strong(T& expected, T desired, std::memory_order success, std::memory_order failure) const noexcept
    {
        return m_ref.compare_exchange_strong(expected, desired, success, failure);
    }
    bool compare_exchange_strong(T& expected, T desired, std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return m_ref.compare_exchange_strong(expected, desired, order);
    }

    void wait(T old, std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        m_ref.wait(old, order);
    }
    void wait(T old, std::memory_order order = std::memory_order_seq_cst) const volatile noexcept
    {
        m_ref.wait(old, order);
    }

    template <class Rep, class Period>
    bool wait_for(T old,
                  std::chrono::duration<Rep, Period> duration,
                  std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return m_ref.wait_for(old, duration, order);
    }
    template <class Rep, class Period>
    bool wait_for(T old,
                  std::chrono::duration<Rep, Period> duration,
                  std::memory_order order = std::memory_order_seq_cst) const volatile noexcept
    {
        return m_ref.wait_for(old, duration, order);
    }

    template <class Clock, class Duration>
    bool wait_until(T old,
                    std::chrono::time_point<Clock, Duration> time_point,
                    std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return m_ref.wait_until(old, time_point, order);
    }
    template <class Clock, class Duration>
    bool wait_until(T old,
                    std::chrono::time_point<Clock, Duration> time_point,
                    std::memory_order order = std::memory_order_seq_cst) const volatile noexcept
    {
        return m_ref.wait_until(old, time_point, order);
    }

    void notify_one() const noexcept
    {
        m_ref.notify_one();
    }
    void notify_one() const volatile noexcept
    {
        m_ref.notify_one();
    }
    void notify_all() const noexcept
    {
        m_ref.notify_all();
    }
    void notify_all() const volatile noexcept
    {
        m_ref.notify_all();
    }
};

template <class T>
class atomic_ref_pointer_facade : public atomic_ref_base<T>
{
    using Base = atomic_ref_base<T>;
    static_assert(std::is_pointer<T>::value, "");

public:
    using difference_type = std::ptrdiff_t;

    explicit atomic_ref_pointer_facade(T& ref) : Base(ref)
    {
    }
    atomic_ref_pointer_facade(const atomic_ref_pointer_facade& other) noexcept : Base(other)
    {
    }

    using Base::operator=;

    T fetch_add(std::ptrdiff_t arg, std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return this->m_ref.fetch_add(arg, order);
    }
    T fetch_sub(std::ptrdiff_t arg, std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return this->m_ref.fetch_sub(arg, order);
    }
    T operator++() const noexcept
    {
        return this->m_ref.fetch_add(1) + 1;
    }
    T operator++(int) const noexcept
    {
        return this->m_ref.fetch_add(1);
    }
    T operator--() const noexcept
    {
        return this->m_ref.fetch_sub(1) - 1;
    }
    T operator--(int) const noexcept
    {
        return this->m_ref.fetch_sub(1);
    }
    T operator+=(std::ptrdiff_t arg) const noexcept
    {
        return this->m_ref.fetch_add(arg) + arg;
    }
    T operator-=(std::ptrdiff_t arg) const noexcept
    {
        return this->m_ref.fetch_sub(arg) - arg;
    }
};

template <class T>
class atomic_ref_numeric_facade : public atomic_ref_base<T>
{
    using Base = atomic_ref_base<T>;
    static_assert(std::is_integral<T>::value || std::is_floating_point<T>::value, "");

public:
    using difference_type = T;

    explicit atomic_ref_numeric_facade(T& ref) : Base(ref)
    {
    }
    atomic_ref_numeric_facade(const atomic_ref_numeric_facade& other) noexcept : Base(other)
    {
    }

    using Base::operator=;

    T fetch_add(T arg, std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return this->m_ref.fetch_add(arg, order);
    }
    T fetch_sub(T arg, std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return this->m_ref.fetch_sub(arg, order);
    }
    T operator+=(T arg) const noexcept
    {
        return this->m_ref.fetch_add(arg) + arg;
    }
    T operator-=(T arg) const noexcept
    {
        return this->m_ref.fetch_sub(arg) - arg;
    }
};

template <class T>
class atomic_ref_integer_facade : public atomic_ref_numeric_facade<T>
{
    using Base = atomic_ref_numeric_facade<T>;
    static_assert(std::is_integral<T>::value, "");

public:
    explicit atomic_ref_integer_facade(T& ref) : Base(ref)
    {
    }
    atomic_ref_integer_facade(const atomic_ref_integer_facade& other) noexcept : Base(other)
    {
    }

    using Base::operator=;

    T fetch_and(T arg, std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return this->m_ref.fetch_and(arg, order);
    }
    T fetch_or(T arg, std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return this->m_ref.fetch_or(arg, order);
    }
    T fetch_xor(T arg, std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        return this->m_ref.fetch_xor(arg, order);
    }

    T operator++() const noexcept
    {
        return this->m_ref.fetch_add(T(1)) + T(1);
    }
    T operator++(int) const noexcept
    {
        return this->m_ref.fetch_add(T(1));
    }
    T operator--() const noexcept
    {
        return this->m_ref.fetch_sub(T(1)) - T(1);
    }
    T operator--(int) const noexcept
    {
        return this->m_ref.fetch_sub(T(1));
    }

    T operator&=(T arg) const noexcept
    {
        return this->m_ref.fetch_and(arg) & arg;
    }
    T operator|=(T arg) const noexcept
    {
        return this->m_ref.fetch_or(arg) | arg;
    }
    T operator^=(T arg) const noexcept
    {
        return this->m_ref.fetch_xor(arg) ^ arg;
    }
};

template <class T>
using SelectAtomicRefBase = std::conditional_t<
    std::is_pointer<T>::value,
    atomic_ref_pointer_facade<T>,
    std::conditional_t<std::is_integral<T>::value,
                       atomic_ref_integer_facade<T>,
                       std::conditional_t<std::is_floating_point<T>::value, atomic_ref_numeric_facade<T>, atomic_ref_base<T>>>>;

template <class T>
using SelectAtomicBase = std::conditional_t<std::is_floating_point<T>::value, atomic_float_facade<T>, std::atomic<T>>;

} // namespace detail

template <class T>
class atomic : public detail::SelectAtomicBase<T>
{
    using Base = detail::SelectAtomicBase<T>;

public:
    using value_type = T;

    atomic() noexcept = default;
    constexpr atomic(T desired) noexcept : Base(desired)
    {
    }

    static constexpr bool is_always_lock_free = sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8;

    using Base::operator=;
    using Base::operator T;

    CARB_PREVENT_COPY_AND_MOVE(atomic);

    // See https://en.cppreference.com/w/cpp/atomic/atomic/wait
    void wait(T old, std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        static_assert(is_always_lock_free, "Only supported for always-lock-free types");
        using I = thread::detail::to_integral_t<T>;
        for (;;)
        {
            if (this_thread::spinTryWait([&] {
                    return thread::detail::reinterpret_as<I>(this->load(order)) != thread::detail::reinterpret_as<I>(old);
                }))
            {
                break;
            }
            thread::futex::wait(*this, old);
        }
    }
    void wait(T old, std::memory_order order = std::memory_order_seq_cst) const volatile noexcept
    {
        static_assert(is_always_lock_free, "Only supported for always-lock-free types");
        using I = thread::detail::to_integral_t<T>;
        for (;;)
        {
            if (this_thread::spinTryWait([&] {
                    return thread::detail::reinterpret_as<I>(this->load(order)) != thread::detail::reinterpret_as<I>(old);
                }))
            {
                break;
            }
            thread::futex::wait(const_cast<atomic<T>&>(*this), old);
        }
    }
    // wait_for and wait_until are non-standard
    template <class Rep, class Period>
    bool wait_for(T old,
                  const std::chrono::duration<Rep, Period>& duration,
                  std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        // Since futex can spuriously wake up, calculate the end time so that we can handle the spurious wakeups without
        // shortening our wait time potentially significantly.
        return wait_until(old, std::chrono::steady_clock::now() + thread::detail::clampDuration(duration), order);
    }
    template <class Rep, class Period>
    bool wait_for(T old,
                  const std::chrono::duration<Rep, Period>& duration,
                  std::memory_order order = std::memory_order_seq_cst) const volatile noexcept
    {
        // Since futex can spuriously wake up, calculate the end time so that we can handle the spurious wakeups without
        // shortening our wait time potentially significantly.
        return wait_until(old, std::chrono::steady_clock::now() + thread::detail::clampDuration(duration), order);
    }
    template <class Clock, class Duration>
    bool wait_until(T old,
                    const std::chrono::time_point<Clock, Duration>& time_point,
                    std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        static_assert(is_always_lock_free, "Only supported for always-lock-free types");
        using I = thread::detail::to_integral_t<T>;
        for (;;)
        {
            if (this_thread::spinTryWait([&] {
                    return thread::detail::reinterpret_as<I>(this->load(order)) != thread::detail::reinterpret_as<I>(old);
                }))
            {
                return true;
            }
            if (!thread::futex::wait_until(*this, old, time_point))
            {
                return false;
            }
        }
    }
    template <class Clock, class Duration>
    bool wait_until(T old,
                    const std::chrono::time_point<Clock, Duration>& time_point,
                    std::memory_order order = std::memory_order_seq_cst) const volatile noexcept
    {
        static_assert(is_always_lock_free, "Only supported for always-lock-free types");
        using I = thread::detail::to_integral_t<T>;
        for (;;)
        {
            if (this_thread::spinTryWait([&] {
                    return thread::detail::reinterpret_as<I>(this->load(order)) != thread::detail::reinterpret_as<I>(old);
                }))
            {
                return true;
            }
            if (!thread::futex::wait_until(const_cast<atomic<T>&>(*this), old, time_point))
            {
                return false;
            }
        }
    }
    // See https://en.cppreference.com/w/cpp/atomic/atomic/notify_one
    void notify_one() noexcept
    {
        thread::futex::wake_one(*this);
    }
    void notify_one() volatile noexcept
    {
        thread::futex::wake_one(const_cast<atomic<T>&>(*this));
    }
    // See https://en.cppreference.com/w/cpp/atomic/atomic/notify_all
    void notify_all() noexcept
    {
        thread::futex::wake_all(*this);
    }
    void notify_all() volatile noexcept
    {
        thread::futex::wake_all(const_cast<atomic<T>&>(*this));
    }
};

template <class T>
class atomic_ref : public detail::SelectAtomicRefBase<T>
{
    using Base = detail::SelectAtomicRefBase<T>;

public:
    explicit atomic_ref(T& ref) : Base(ref)
    {
    }
    atomic_ref(const atomic_ref& other) noexcept : Base(other)
    {
    }
    using Base::operator=;
};

// Helper functions
// See https://en.cppreference.com/w/cpp/atomic/atomic_wait
template <class T>
inline void atomic_wait(const atomic<T>* object, typename atomic<T>::value_type old) noexcept
{
    object->wait(old);
}
template <class T>
inline void atomic_wait_explicit(const atomic<T>* object, typename atomic<T>::value_type old, std::memory_order order) noexcept
{
    object->wait(old, order);
}
// See https://en.cppreference.com/w/cpp/atomic/atomic_notify_one
template <class T>
inline void atomic_notify_one(atomic<T>* object)
{
    object->notify_one();
}
// See https://en.cppreference.com/w/cpp/atomic/atomic_notify_all
template <class T>
inline void atomic_notify_all(atomic<T>* object)
{
    object->notify_all();
}

using atomic_bool = atomic<bool>;
using atomic_char = atomic<char>;
using atomic_schar = atomic<signed char>;
using atomic_uchar = atomic<unsigned char>;
using atomic_short = atomic<short>;
using atomic_ushort = atomic<unsigned short>;
using atomic_int = atomic<int>;
using atomic_uint = atomic<unsigned int>;
using atomic_long = atomic<long>;
using atomic_ulong = atomic<unsigned long>;
using atomic_llong = atomic<long long>;
using atomic_ullong = atomic<unsigned long long>;
using atomic_char16_t = atomic<char16_t>;
using atomic_char32_t = atomic<char32_t>;
using atomic_wchar_t = atomic<wchar_t>;
using atomic_int8_t = atomic<int8_t>;
using atomic_uint8_t = atomic<uint8_t>;
using atomic_int16_t = atomic<int16_t>;
using atomic_uint16_t = atomic<uint16_t>;
using atomic_int32_t = atomic<int32_t>;
using atomic_uint32_t = atomic<uint32_t>;
using atomic_int64_t = atomic<int64_t>;
using atomic_uint64_t = atomic<uint64_t>;
using atomic_int_least8_t = atomic<int_least8_t>;
using atomic_uint_least8_t = atomic<uint_least8_t>;
using atomic_int_least16_t = atomic<int_least16_t>;
using atomic_uint_least16_t = atomic<uint_least16_t>;
using atomic_int_least32_t = atomic<int_least32_t>;
using atomic_uint_least32_t = atomic<uint_least32_t>;
using atomic_int_least64_t = atomic<int_least64_t>;
using atomic_uint_least64_t = atomic<uint_least64_t>;
using atomic_int_fast8_t = atomic<int_fast8_t>;
using atomic_uint_fast8_t = atomic<uint_fast8_t>;
using atomic_int_fast16_t = atomic<int_fast16_t>;
using atomic_uint_fast16_t = atomic<uint_fast16_t>;
using atomic_int_fast32_t = atomic<int_fast32_t>;
using atomic_uint_fast32_t = atomic<uint_fast32_t>;
using atomic_int_fast64_t = atomic<int_fast64_t>;
using atomic_uint_fast64_t = atomic<uint_fast64_t>;
using atomic_intptr_t = atomic<intptr_t>;
using atomic_uintptr_t = atomic<uintptr_t>;
using atomic_size_t = atomic<size_t>;
using atomic_ptrdiff_t = atomic<ptrdiff_t>;
using atomic_intmax_t = atomic<intmax_t>;
using atomic_uintmax_t = atomic<uintmax_t>;

} // namespace cpp20

} // namespace carb
