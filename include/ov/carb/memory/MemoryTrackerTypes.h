// Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
// ver: 0.1
//

#pragma once

#include <cstddef>
#include <cstdint>

namespace carb
{
namespace memory
{
/**
 * A context is a thin wrapper of a string pointer, as such it is up to the programmer
 * to ensure that the pointer is valid at the invocation.
 *
 * To minimize the possibility of error any API receiving the context should copy the
 * string rather than reference its pointer.
 */

class Context
{
public:
    explicit Context(const char* contextName) : m_contextName(contextName)
    {
    }

    const char* getContextName() const
    {
        return m_contextName;
    }

private:
    const char* m_contextName;
};

/**
 * An address space is a type of memory that the user wishes to track. Normal
 * allocation goes into the Global address space. This is used to track manual heaps, as
 * well as resources that behave like memory but are not directly tied to the memory
 * systems provided by the global heap. This can also be used to track an object who has
 * unique id for the life-time of the object. Example: OpenGL Texture Ids
 *
 * Examples include GPU resources and Object Pools.
 */
struct AllocationGroup;

#define DEFAULT_ALLOCATION_GROUP_NAME ""

/**
 * A bookmark is a point in time in the memory tracker, it allows the user to
 * create a view of the memory between a bookmark and now.
 */
struct Bookmark;

struct ReportFlag
{
    enum
    {
        eReportLeaks = 0x1, ///< Report any memory leaks as well.
        eSummary = 0x2, ///< Just a summary.
        eFull = eReportLeaks | eSummary,
    };
};
typedef uint32_t ReportFlags;

/**
 * This structure wraps up the data of the report.
 */
class Report;

/**
 * A Summary is a really simple report.
 */
struct Summary
{
    size_t allocationGroupCount;
    size_t allocationCount;
    size_t allocationBytes;
    size_t freeCount;
    size_t freeBytes;
};

enum class MemoryType
{
    eMalloc,
    eCalloc,
    eRealloc,
    eAlignedAlloc,
    eStrdup,
    eNew,
    eNewArray,
    eExternal,
    eMemalign, // Linux only
    eValloc, // Linux only
    ePosixMemalign, // Linux only
    eHeapAlloc,
    eHeapRealloc,
};

} // namespace memory
} // namespace carb
