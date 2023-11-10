// Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"

// In plugin mode, memory tracker is required to be loaded/unloaded as other plugins
// In this mode, always track memory allocation/free after loaded
#define CARB_MEMORY_WORK_AS_PLUGIN 1

// JMK 2021-09-13: disabling carb.memory as it can lead to shutdown issues. The hooks are not added or removed in a
// thread-safe way, which means that other threads can be in a trampoline or hook function when they are removed. This
// leads to potential crashes at shutdown.
#ifndef CARB_MEMORY_TRACKER_ENABLED
// #    define CARB_MEMORY_TRACKER_ENABLED (CARB_DEBUG)
#    define CARB_MEMORY_TRACKER_ENABLED 0
#endif

// Option on work mode for Windows
// Set to 1: Hook windows heap API (only for Windows)
// Set to 0: Replace malloc/free, new/delete
// Linux always use replace mode
#define CARB_MEMORY_HOOK 1

#if CARB_PLATFORM_LINUX && CARB_MEMORY_TRACKER_ENABLED
#    define CARB_MEMORY_TRACKER_MODE_REPLACE
#elif CARB_PLATFORM_WINDOWS && CARB_MEMORY_TRACKER_ENABLED
#    if CARB_MEMORY_HOOK
#        define CARB_MEMORY_TRACKER_MODE_HOOK
#    else
#        define CARB_MEMORY_TRACKER_MODE_REPLACE
#    endif
#endif

// Option to add addition header before allocated memory
// See MemoryBlockHeader for header structure
#define CARB_MEMORY_ADD_HEADER 0

#if !CARB_MEMORY_ADD_HEADER
// If header not added, will verify the 6 of 8 bytes before allocated memory
// ---------------------------------
// | Y | Y | Y | Y | N | N | Y | Y | Allocated memory
// ---------------------------------
// Y means to verify, N means to ignore
// These 8 bytes should be part of heap chunk header
// During test, the 6 bytes will not changed before free while other 2 bytes may changed.
// Need investigate more for reason.
#    define CARB_MEMORY_VERIFY_HEAP_CHUNK_HEADER 1
#else
#    define CARB_MEMORY_VERIFY_HEAP_CHUNK_HEADER 0
#endif
