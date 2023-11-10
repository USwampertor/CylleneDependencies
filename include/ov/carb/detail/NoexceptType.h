// Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! \cond DEV
//! \file
//!
//! \brief Workaround for GCC's \c -Wnoexcept-type warning in pre-C++17.
#pragma once

#include "../Defines.h"

//! \def CARB_DETAIL_PUSH_IGNORE_NOEXCEPT_TYPE
//! Push a new layer of diagnostic checks which ignore GCC's \c noexcept-type warning. If this warning is not relevant
//! for the compiler configuration, this does nothing.
//!
//! GCC 7 before C++17 warns you about using a `R(*)(Args...) noexcept` as a type parameter because the mangling will
//! change in C++17. If you do not rely on mangling (as is the case of most inlined code), then this warning can be
//! safely ignored. The only case where an inlined function relies on mangling rules is if you rely on a function-local
//! static to be the same across shared object boundaries between two functions with different `noexcept` specifiers,
//! which is not common. See the GCC bug (https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80985) for additional details.
//!
//! Use of this macro should always be paired with \c CARB_DETAIL_POP_IGNORE_NOEXCEPT_TYPE.
//!
//! \code
//! void foo() noexcept;
//!
//! CARB_DETAIL_PUSH_IGNORE_NOEXCEPT_TYPE()
//! template <typename F>
//! void bar(F f) // <- error would occur here
//! {
//!     f();
//! }
//! CARB_DETAIL_POP_IGNORE_NOEXCEPT_TYPE()
//!
//! int main()
//! {
//!     bar(foo); // <- attempt to provide `F = void(*)() noexcept`
//! }
//! \endcode
//!
//! \def CARB_DETAIL_POP_IGNORE_NOEXCEPT_TYPE
//! The opposite of \c CARB_DETAIL_PUSH_IGNORE_NOEXCEPT_TYPE -- pops one layer of diagnostics if push added one. If
//! \c CARB_DETAIL_PUSH_IGNORE_NOEXCEPT_TYPE did nothing, then this macro will also do nothing.
#if __cplusplus < 201703L && CARB_COMPILER_GNUC && !CARB_TOOLCHAIN_CLANG && __GNUC__ == 7
#    define CARB_DETAIL_PUSH_IGNORE_NOEXCEPT_TYPE() CARB_IGNOREWARNING_GNUC_WITH_PUSH("-Wnoexcept-type")
#    define CARB_DETAIL_POP_IGNORE_NOEXCEPT_TYPE() CARB_IGNOREWARNING_GNUC_POP
#else
#    define CARB_DETAIL_PUSH_IGNORE_NOEXCEPT_TYPE()
#    define CARB_DETAIL_POP_IGNORE_NOEXCEPT_TYPE()
#endif

//! \endcond
