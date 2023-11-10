// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

// NOTE: This comment is left for historical purposes, but is no longer accurate. The `g_carbAssert` global variable is
// weakly-linked now, so it no longer requires linking against carb.dll.

#if 0
// Include this file (near the top of your includes list) when compiling a DLL which does not depend on any Carbonite
// interfaces.
//
// This file solves the following problem: some inline code in carb::extras uses CARB_ASSERT, which causes a dependency
// on g_carbAssert.  When compiling code as a DLL for implicit linking (i.e. not a module/plugin), the linker will not
// be able to find g_carbAssert.  The DLL can define g_carbAssert, but no one is likely to set it to a valid value. The
// result is a crash.
//
// This file redefines the CARB_ASSERT macros to the OMNI_ASSERT macros.  The OMNI_ASSERT macros do not depend on global
// variables.
#    define CARB_ASSERT OMNI_ASSERT
#    define CARB_ASSERT_ENABLED OMNI_ASSERT_ENABLED
#    define CARB_CHECK OMNI_CHECK
#    define CARB_CHECK_ENABLED OMNI_CHECK_ENABLED
#    define CARB_FATAL_UNLESS OMNI_FATAL_UNLESS
#endif

#include "../../carb/Defines.h"

#include "Assert.h"
