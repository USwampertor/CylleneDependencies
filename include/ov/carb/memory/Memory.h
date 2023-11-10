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

#include "IMemoryTracker.h"

#if CARB_MEMORY_WORK_AS_PLUGIN
#    define CARB_MEMORY_GLOBALS() CARB_MEMORY_TRACKER_GLOBALS()

class MemoryInitializerScoped
{
public:
    MemoryInitializerScoped()
    {
        carb::memory::registerMemoryTrackerForClient();
    }
    ~MemoryInitializerScoped()
    {
        carb::memory::deregisterMemoryTrackerForClient();
    }
};
#endif

#if defined(CARB_MEMORY_TRACKER_MODE_REPLACE)
inline void* mallocWithRecord(size_t size)
{
    void* address = malloc(size);
    if (address)
    {
        carb::memory::IMemoryTracker* tracker = carb::memory::getMemoryTracker();
        if (tracker)
        {
            // Set allocationGroup to nullptr means using default allocation group(HEAP)
            tracker->recordAllocation(nullptr, address, size);
        }
    }
    return address;
}

inline void freeWithRecord(void* address)
{
    carb::memory::IMemoryTracker* tracker = carb::memory::getMemoryTracker();
    if (tracker)
    {
        // Set allocationGroup to nullptr means using default allocation group(HEAP)
        tracker->recordFree(nullptr, address);
    }
}

#    if CARB_PLATFORM_WINDOWS
inline void* operator new(size_t size) throw()
#    else
void* operator new(size_t size) throw()
#    endif
{
    return mallocWithRecord(size);
}

#    if CARB_PLATFORM_WINDOWS
inline void operator delete(void* address) throw()
#    else
void operator delete(void* address) throw()
#    endif
{
    freeWithRecord(address);
}

#    if CARB_PLATFORM_WINDOWS
inline void operator delete(void* address, unsigned long) throw()
#    else
void operator delete(void* address, unsigned long) throw()
#    endif
{
    freeWithRecord(address);
}

#    if CARB_PLATFORM_WINDOWS
inline void* operator new[](size_t size) throw()
#    else
void* operator new[](size_t size) throw()
#    endif
{
    return mallocWithRecord(size);
}

#    if CARB_PLATFORM_WINDOWS
inline void operator delete[](void* address) throw()
#    else
void operator delete[](void* address) throw()
#    endif
{
    freeWithRecord(address);
}

#    if CARB_PLATFORM_WINDOWS
inline void operator delete[](void* address, unsigned long) throw()
#    else
void operator delete[](void* address, unsigned long) throw()
#    endif
{
    freeWithRecord(address);
}

void* operator new(size_t size, const std::nothrow_t&)
{
    return mallocWithRecord(size);
}

void operator delete(void* address, const std::nothrow_t&)
{
    freeWithRecord(address);
}

void* operator new[](size_t size, const std::nothrow_t&)
{
    return mallocWithRecord(size);
}

void operator delete[](void* address, const std::nothrow_t&)
{
    freeWithRecord(address);
}
#endif

inline void* _carbMalloc(size_t size, va_list args)
{
    carb::memory::Context* context = va_arg(args, carb::memory::Context*);
    carb::memory::IMemoryTracker* tracker = carb::memory::getMemoryTracker();
    if (tracker && context)
        tracker->pushContext(*context);
#if defined(CARB_MEMORY_TRACKER_MODE_REPLACE)
    void* address = mallocWithRecord(size);
#else
    void* address = malloc(size);
#endif
    if (tracker && context)
        tracker->popContext();

    return address;
}

inline void* carbMalloc(size_t size, ...)
{
    va_list args;
    va_start(args, size);
    void* address = _carbMalloc(size, args);
    va_end(args);
    return address;
}

inline void carbFree(void* address)
{
#if defined(CARB_MEMORY_TRACKER_MODE_REPLACE)
    freeWithRecord(address);
#else
    free(address);
#endif
}

inline void* operator new(size_t size, const char* file, int line, ...)
{
    CARB_UNUSED(file);
    va_list args;
    va_start(args, line);
    void* address = _carbMalloc(size, args);
    va_end(args);
    return address;
}

inline void* operator new[](size_t size, const char* file, int line, ...)
{
    CARB_UNUSED(file);
    va_list args;
    va_start(args, line);
    void* address = _carbMalloc(size, args);
    va_end(args);
    return address;
}

#define NV_MALLOC(size, ...) carbMalloc(size, ##__VA_ARGS__)
#define NV_FREE(p) carbFree(p)
#define NV_NEW(...) new (__FILE__, __LINE__, ##__VA_ARGS__)
#define NV_DELETE delete
#define NV_DELETE_ARRAY delete[]
