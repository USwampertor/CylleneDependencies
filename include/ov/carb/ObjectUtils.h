// Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Helper utilities for Carbonite objects (carb::IObject).
#pragma once

#include "IObject.h"

#include <atomic>


namespace carb
{

/**
 * Default handler for carb::IObject reaching zero references, which calls `delete`. Can be specialized for specific
 * types.
 * @param ptr The object to delete.
 */
template <class T>
void deleteHandler(T* ptr)
{
    delete ptr;
}

} // namespace carb


/**
 * Helper macro to implement default behavior of carb::IObject interface functions IObject::addRef() and
 * IObject::release().
 *
 * Example usage:
 * @code
 * class Foo : public IObject
 * {
 *      CARB_IOBJECT_IMPL
 *
 * public:
 *      ...
 * };
 * @endcode
 */
#define CARB_IOBJECT_IMPL                                                                                              \
public:                                                                                                                \
    /**                                                                                                                \
     * Atomically adds one to the reference count.                                                                     \
     * @returns The current reference count after one was added, though this value may change before read if other     \
     * threads are also modifying the reference count. The return value is guaranteed to be non-zero.                  \
     */                                                                                                                \
    size_t addRef() override                                                                                           \
    {                                                                                                                  \
        size_t prev = m_refCount.fetch_add(1, std::memory_order_relaxed);                                              \
        CARB_ASSERT(prev != 0); /* resurrected item if this occurs */                                                  \
        return prev + 1;                                                                                               \
    }                                                                                                                  \
                                                                                                                       \
    /**                                                                                                                \
     * Atomically subtracts one from the reference count. If the result is zero, carb::deleteHandler() is called for   \
     * `this`.                                                                                                         \
     * @returns The current reference count after one was subtracted. If zero is returned, carb::deleteHandler() was   \
     * called for `this`.                                                                                              \
     */                                                                                                                \
    size_t release() override                                                                                          \
    {                                                                                                                  \
        size_t prev = m_refCount.fetch_sub(1, std::memory_order_release);                                              \
        CARB_ASSERT(prev != 0); /* double release if this occurs */                                                    \
        if (prev == 1)                                                                                                 \
        {                                                                                                              \
            std::atomic_thread_fence(std::memory_order_acquire);                                                       \
            carb::deleteHandler(this);                                                                                 \
        }                                                                                                              \
        return prev - 1;                                                                                               \
    }                                                                                                                  \
                                                                                                                       \
private:                                                                                                               \
    std::atomic_size_t m_refCount{ 1 };
