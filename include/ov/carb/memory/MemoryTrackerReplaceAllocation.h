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

#if CARB_PLATFORM_WINDOWS && CARB_MEMORY_TRACKER_ENABLED && defined CARB_MEMORY_TRACKER_MODE_REPLACE
#    include <vcruntime_new.h>

#    pragma warning(push)
#    pragma warning(disable : 4595) // non-member operator new or delete functions may not be declared inline

/**
 * Replacement of the new/delete operator in C++
 */
inline void* operator new(size_t size)
{
    return malloc(size);
}

inline void operator delete(void* address)
{
    free(address);
}

inline void* operator new[](size_t size)
{
    return malloc(size);
}

inline void operator delete[](void* address)
{
    free(address);
}

/*
void* operator new(size_t size, const std::nothrow_t&)
{
    return malloc(size);
}

void operator delete(void* address, const std::nothrow_t&)
{
    free(address);
}

void* operator new[](size_t size, const std::nothrow_t&)
{
    return malloc(size);
}

void operator delete[](void* address, const std::nothrow_t&)
{
    free(address);
}*/
#    pragma warning(pop)
#endif
