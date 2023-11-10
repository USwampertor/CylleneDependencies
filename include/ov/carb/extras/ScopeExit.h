// Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"

#include "../cpp17/Exception.h"

#include <type_traits>
#include <utility>

//
// Inspired from Andrei Alexandrescu CppCon 2015 Talk: Declarative Control Flow
//  https://github.com/CppCon/CppCon2015/tree/master/Presentations/
//  https://www.youtube.com/watch?v=WjTrfoiB0MQ
//

// lambda executed on scope leaving scope
#define CARB_SCOPE_EXIT                                                                                                \
    const auto CARB_ANONYMOUS_VAR(SCOPE_EXIT_STATE) = ::carb::extras::detail::ScopeGuardExit{} + [&]()
// lambda executed when leaving scope because an exception was thrown
#define CARB_SCOPE_EXCEPT                                                                                              \
    const auto CARB_ANONYMOUS_VAR(SCOPE_EXCEPT_STATE) = ::carb::extras::detail::ScopeGuardOnFail{} + [&]()
// lambda executed when leaving scope and no exception was thrown
#define CARB_SCOPE_NOEXCEPT                                                                                            \
    const auto CARB_ANONYMOUS_VAR(SCOPE_EXCEPT_STATE) = ::carb::extras::detail::ScopeGuardOnSuccess{} + [&]()

namespace carb
{

namespace extras
{

namespace detail
{

template <typename Fn>
class ScopeGuard final
{
    CARB_PREVENT_COPY(ScopeGuard);

public:
    explicit ScopeGuard(Fn&& fn) : m_fn(std::move(fn))
    {
    }

    ScopeGuard(ScopeGuard&&) = default;
    ScopeGuard& operator=(ScopeGuard&&) = default;

    ~ScopeGuard()
    {
        m_fn();
    }

private:
    Fn m_fn;
};

enum class ScopeGuardExit
{
};

template <typename Fn>
ScopeGuard<typename std::decay_t<Fn>> operator+(ScopeGuardExit, Fn&& fn)
{
    return ScopeGuard<typename std::decay_t<Fn>>(std::forward<Fn>(fn));
}

class UncaughtExceptionCounter final
{
    CARB_PREVENT_COPY(UncaughtExceptionCounter);

    int m_exceptionCount;

public:
    UncaughtExceptionCounter() noexcept : m_exceptionCount(::carb::cpp17::uncaught_exceptions())
    {
    }

    ~UncaughtExceptionCounter() = default;

    UncaughtExceptionCounter(UncaughtExceptionCounter&&) = default;
    UncaughtExceptionCounter& operator=(UncaughtExceptionCounter&&) = default;

    bool isNewUncaughtException() const noexcept
    {
        return ::carb::cpp17::uncaught_exceptions() > m_exceptionCount;
    }
};

template <typename Fn, bool execOnException>
class ScopeGuardForNewException
{
    CARB_PREVENT_COPY(ScopeGuardForNewException);

    Fn m_fn;
    UncaughtExceptionCounter m_ec;

public:
    explicit ScopeGuardForNewException(Fn&& fn) : m_fn(std::move(fn))
    {
    }

    ~ScopeGuardForNewException() noexcept(execOnException)
    {
        if (execOnException == m_ec.isNewUncaughtException())
        {
            m_fn();
        }
    }

    ScopeGuardForNewException(ScopeGuardForNewException&&) = default;
    ScopeGuardForNewException& operator=(ScopeGuardForNewException&&) = default;
};

enum class ScopeGuardOnFail
{
};

template <typename Fn>
ScopeGuardForNewException<typename std::decay_t<Fn>, true> operator+(ScopeGuardOnFail, Fn&& fn)
{
    return ScopeGuardForNewException<typename std::decay_t<Fn>, true>(std::forward<Fn>(fn));
}

enum class ScopeGuardOnSuccess
{
};

template <typename Fn>
ScopeGuardForNewException<typename std::decay_t<Fn>, false> operator+(ScopeGuardOnSuccess, Fn&& fn)
{
    return ScopeGuardForNewException<typename std::decay_t<Fn>, false>(std::forward<Fn>(fn));
}

} // namespace detail

} // namespace extras

} // namespace carb
