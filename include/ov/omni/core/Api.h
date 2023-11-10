// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief Helper macros to provide API calling convention tags.
#pragma once

#include "Platform.h"

#ifdef __cplusplus
//! Declares a "C" exported external symbol.  This uses the "C" name decoration style of
//! adding an underscore to the start of the exported name.
#    define OMNI_EXTERN_C extern "C"
#else
//! Declares a "C" exported external symbol.  This uses the "C" name decoration style of
//! adding an underscore to the start of the exported name.
#    define OMNI_EXTERN_C
#endif

//! \copydoc CARB_EXPORT
#define OMNI_EXPORT CARB_EXPORT

// Functions that wish to be exported from a .dll/.so should be decorated with OMNI_API.
//
// Functions related to modules, such omniGetModuleExports(), should be decorated with OMNI_MODULE_API.
#ifdef OMNI_COMPILE_AS_DYNAMIC_LIBRARY
#    if OMNI_PLATFORM_WINDOWS
//! Declares a symbol that is marked as externally exported.  The symbol will be exported
//! with C decorations.  On Windows, this is expected to be exported from the containing DLL.
//! On Linux, this is exported as having default visibility from the module instead of being
//! hidden.  This export tag should only be used when tagging exported symbols from within
//! omni.core itself.  Exported symbols in other modules (such as `omniGetModuleExports()`
//! functions in implementation libraries) should use @ref OMNI_MODULE_API instead.
#        define OMNI_API OMNI_EXTERN_C __declspec(dllexport)
#    elif OMNI_PLATFORM_LINUX || OMNI_PLATFORM_MACOS
//! Declares a symbol that is marked as externally exported.  The symbol will be exported
//! with C decorations.  On Windows, this is expected to be exported from the containing DLL.
//! On Linux, this is exported as having default visibility from the module instead of being
//! hidden.  This export tag should only be used when tagging exported symbols from within
//! omni.core itself.  Exported symbols in other modules (such as `omniGetModuleExports()`
//! functions in implementation libraries) should use @ref OMNI_MODULE_API instead.
#        define OMNI_API OMNI_EXTERN_C __attribute__((visibility("default")))
#    endif
#else
//! Declares a symbol that is marked as externally exported.  The symbol will be exported
//! with C decorations.  On Windows, this is expected to be exported from the containing DLL.
//! On Linux, this is exported as having default visibility from the module instead of being
//! hidden.  This export tag should only be used when tagging exported symbols from within
//! omni.core itself.  Exported symbols in other modules (such as `omniGetModuleExports()`
//! functions in implementation libraries) should use @ref OMNI_MODULE_API instead.
#    define OMNI_API OMNI_EXTERN_C
#endif

// Functions related to modules should be decorated with OMNI_MODULE_API. Currently, only omniGetModuleExports()
// qualifies.
#ifdef OMNI_COMPILE_AS_MODULE
#    ifdef OMNI_COMPILE_AS_DYNAMIC_LIBRARY
#        error "OMNI_COMPILE_AS_DYNAMIC_LIBRARY and OMNI_COMPILE_AS_MODULE cannot be both defined"
#    endif
#    if OMNI_PLATFORM_WINDOWS
//! Declares a function that is marked as externally exported.  The symbol will be exported
//! with C decorations.  On Windows, this is expected to be exported from the containing DLL.
//! On Linux, this is exported as having default visibility from the module instead of being
//! hidden.  This is intended for exported symbols in implementation libraries.
#        define OMNI_MODULE_API OMNI_EXTERN_C __declspec(dllexport)
#    elif OMNI_PLATFORM_LINUX || OMNI_PLATFORM_MACOS
//! Declares a function that is marked as externally exported.  The symbol will be exported
//! with C decorations.  On Windows, this is expected to be exported from the containing DLL.
//! On Linux, this is exported as having default visibility from the module instead of being
//! hidden.  This is intended for exported symbols in implementation libraries.
#        define OMNI_MODULE_API OMNI_EXTERN_C __attribute__((visibility("default")))
#    else
CARB_UNSUPPORTED_PLATFORM();
#    endif
#else
//! Declares a function that is marked as externally exported.  The symbol will be exported
//! with C decorations.  On Windows, this is expected to be exported from the containing DLL.
//! On Linux, this is exported as having default visibility from the module instead of being
//! hidden.  This is intended for exported symbols in implementation libraries.
#    define OMNI_MODULE_API OMNI_EXTERN_C
#endif
