// Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! \file
//! \brief Internal utilities for loading Carbonite functions at runtime.

#include "../Defines.h"

#if !defined(CARB_REQUIRE_LINKED) || defined(DOXYGEN_BUILD)
//! Changes how the `carbReallocate` symbol is acquired.
//!
//! If \c CARB_REQUIRE_LINKED is defined as 1 before this file is included, then Carbonite-provided functions (e.g.
//! `carbReallocate` and `omniGetErrorApi`) will be imported from \e carb.dll or \e libcarb.so and the binary must link
//! against the import library for the module. This can be useful for applications that want to use these functions
//! prior to initializing the framework.
//!
//! If not defined or defined as \c 0, the functions will be marked weakly-linked and the binary need not link against
//! the import library. Attempting to call these functions will dynamically attempt to find then in the already-loaded
//! \e carb module. If it cannot be found a warning will be thrown.
//!
//! \see CARB_DETAIL_DEFINE_DEFERRED_LOAD
//! \see carbReallocate
#    define CARB_REQUIRE_LINKED 0
#endif

//! Create a "deferred loader" function.
//!
//! \param fn_name_ The name of the function to define.
//! \param symbol_ The target function to attempt to load.
//! \param type_pack_ The type of the symbol to load as a parenthesized pack. For example, if the target \c symbol_
//!        refers to something like `int foo(char, double)`, this would be `(int (*)(char, double))`. The enclosing
//!        parenthesis are needed because these arguments contain spaces and commas.
#define CARB_DETAIL_DEFINE_DEFERRED_LOAD(fn_name_, symbol_, type_pack_)                                                \
    inline auto fn_name_()->CARB_DEPAREN(type_pack_)                                                                   \
    {                                                                                                                  \
        CARB_DETAIL_DEFINE_DEFERRED_LOAD_IMPL(symbol_, type_pack_);                                                    \
    }                                                                                                                  \
    static_assert(true, "follow with ;")

#if CARB_REQUIRE_LINKED || defined DOXYGEN_BUILD

//! \cond DEV
//! Implementation of the function body for \c CARB_DETAIL_DEFINE_DEFERRED_LOAD.
#    define CARB_DETAIL_DEFINE_DEFERRED_LOAD_IMPL(symbol_, type_pack_) return &symbol_
//! \endcond DEV

#elif CARB_PLATFORM_LINUX || CARB_PLATFORM_MACOS

#    define CARB_DETAIL_DEFINE_DEFERRED_LOAD_IMPL(symbol_, type_pack_)                                                 \
        auto impl = &symbol_;                                                                                          \
        CARB_FATAL_UNLESS(impl != nullptr, "Could not find `" CARB_STRINGIFY(                                          \
                                               symbol_) "` function -- make sure that libcarb.so is loaded");          \
        return impl

#elif CARB_PLATFORM_WINDOWS

#    include "../CarbWindows.h"

#    define CARB_DETAIL_DEFINE_DEFERRED_LOAD_IMPL(symbol_, type_pack_)                                                   \
        using TargetSymbolType = CARB_DEPAREN(type_pack_);                                                               \
        static TargetSymbolType cached_impl = nullptr;                                                                   \
        if (!cached_impl)                                                                                                \
        {                                                                                                                \
            HMODULE carb_dll_h = GetModuleHandleW(L"carb.dll");                                                          \
            CARB_FATAL_UNLESS(carb_dll_h != nullptr, "Could not find `carb.dll` module -- make sure that it is loaded"); \
            cached_impl = reinterpret_cast<TargetSymbolType>(GetProcAddress(carb_dll_h, CARB_STRINGIFY(symbol_)));       \
            CARB_FATAL_UNLESS(                                                                                           \
                cached_impl != nullptr,                                                                                  \
                "Could not find `" CARB_STRINGIFY(                                                                       \
                    symbol_) "` function at runtime -- #define CARB_REQUIRE_LINKED 1 before including this file");       \
        }                                                                                                                \
        return cached_impl

#else

#    define CARB_DETAIL_DEFINE_DEFERRED_LOAD_IMPL(symbol_, type_pack_) CARB_UNSUPPORTED_PLATFORM()

#endif
