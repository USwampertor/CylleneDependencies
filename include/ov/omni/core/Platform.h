// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief Helper macros to detect the current platform.
#pragma once

#include "../../carb/Defines.h"

//! Set to `1` if compiling a Windows build.  Set to `0` otherwise.  This symbol will
//! always be defined even when not on a Windows build.  It can thus be used to pass
//! as parameters or in if-statements to modify behavior based on the platform.
#define OMNI_PLATFORM_WINDOWS CARB_PLATFORM_WINDOWS

//! Set to `1` if compiling a Linux build.  Set to `0` otherwise.  This symbol will
//! always be defined even when not on a Linux build.  It can thus be used to pass
//! as parameters or in if-statements to modify behavior based on the platform.
#define OMNI_PLATFORM_LINUX CARB_PLATFORM_LINUX

//! Set to `1` if compiling a MacOS build.  Set to `0` otherwise.  This symbol will
//! always be defined even when not on a MacOS build.  It can thus be used to pass
//! as parameters or in if-statements to modify behavior based on the platform.
#define OMNI_PLATFORM_MACOS CARB_PLATFORM_MACOS

/** @copydoc CARB_POSIX */
#define OMNI_POSIX CARB_POSIX

#if OMNI_PLATFORM_LINUX || OMNI_PLATFORM_MACOS || defined(DOXYGEN_BUILD)
//! Triggers a breakpoint.  If no debugger is attached, the program terminates.
#    define OMNI_BREAK_POINT() ::raise(SIGTRAP)
#elif OMNI_PLATFORM_WINDOWS
//! Triggers a breakpoint.  If no debugger is attached, the program terminates.
#    define OMNI_BREAK_POINT() ::__debugbreak()
#else
CARB_UNSUPPORTED_PLATFORM();
#endif
