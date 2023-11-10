// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief DLL Boundary safe memory management functions
#pragma once

#include "Defines.h"
#include "Types.h"
#include "detail/DeferredLoad.h"

//! Internal function used by all other allocation functions.
//!
//! This function is the entry point into `carb.dll`/`libcarb.so` for @ref carb::allocate(), @ref carb::deallocate(),
//! and @ref carb::reallocate(). There are four modes to this function:
//! - If @p p is `nullptr` and @p size is `0`, no action is taken and `nullptr` is returned.
//! - If @p p is not `nullptr` and @p size is `0`, the given pointer is deallocated and `nullptr` is returned.
//! - If @p p is `nullptr` and @p size is non-zero, memory of the requested @p size and alignment specified by @p align
//!   is allocated and returned. If an allocation error occurs, `nullptr` is returned.
//! - If @p p is not `nullptr` and @p size is non-zero, the memory is reallocated and copied (as if by `std::memcpy`) to
//!   the new memory block, which is returned. If @p p can be resized in situ, the same @p p value is returned. If an
//!   error occurs, `nullptr` is returned.
//!
//! @note Using this function requires explicitly linking with `carb.dll`/`libcarb.so` if @ref CARB_REQUIRE_LINKED is
//! `1`. Otherwise, the caller must ensure that `carb.dll`/`libcarb.so` is already loaded before calling this function.
//! Use in situations where the Carbonite Framework is already loaded (i.e. plugins) does not require explicitly linking
//! against Carbonite as this function will be found dynamically at runtime.
//!
//! @warning Do not call this function directly. Instead call @ref carb::allocate(), @ref carb::deallocate(), or
//! @ref carb::reallocate()
//!
//! @see carb::allocate() carb::reallocate() carb::deallocate()
//! @param p The pointer to re-allocate or free. May be `nullptr`. See explanation above.
//! @param size The requested size of the memory region in bytes. See explanation above.
//! @param align The requested alignment of the memory region in bytes. Must be a power of two. See explanation above.
//! @returns Allocated memory, or `nullptr` upon deallocation, or `nullptr` on allocation when an error occurs.
#if CARB_REQUIRE_LINKED
CARB_DYNAMICLINK void* carbReallocate(void* p, size_t size, size_t align);
#else
CARB_DYNAMICLINK void* carbReallocate(void* p, size_t size, size_t align) CARB_ATTRIBUTE(weak);
#endif


namespace carb
{
//! \cond DEV
namespace detail
{

CARB_DETAIL_DEFINE_DEFERRED_LOAD(getCarbReallocate, carbReallocate, (void* (*)(void*, size_t, size_t)));

} // namespace detail
//! \endcond

//! Allocates a block of memory.
//!
//! @note Any plugin (or the executable) may @ref allocate the memory and a different plugin (or the executable) may
//! @ref deallocate or @ref reallocate it.
//!
//! @note If carb.dll/libcarb.so is not loaded, this function will always return `nullptr`.
//!
//! @param size The size of the memory block requested, in bytes. Specifying '0' will return a valid pointer that
//! can be passed to @ref deallocate but cannot be used to store any information.
//! @param align The minimum alignment (in bytes) of the memory block requested. Must be a power of two. Values less
//!     than `sizeof(size_t)` are ignored. `0` indicates to use default system alignment (typically
//!     `2 * sizeof(void*)`).
//! @returns A non-`nullptr` memory block of @p size bytes with minimum alignment @p align. If an error occurred,
//!     or memory could not be allocated, `nullptr` is returned. The memory is not initialized.
inline void* allocate(size_t size, size_t align = 0) noexcept
{
    if (auto impl = detail::getCarbReallocate())
        return impl(nullptr, size, align);
    else
        return nullptr;
}

//! Deallocates a block of memory previously allocated with @ref allocate().
//!
//! @note Any plugin (or the executable) may @ref allocate the memory and a different plugin (or the executable) may
//! @ref deallocate or @ref reallocate it.
//!
//! @note If carb.dll/libcarb.so is not loaded, this function will silently do nothing. Since @ref allocate would have
//! returned `nullptr` in this case, this function should never be called.
//!
//! @param p The block of memory previously returned from @ref allocate() or @ref reallocate(), or `nullptr`.
inline void deallocate(void* p) noexcept
{
    if (p)
    {
        if (auto impl = detail::getCarbReallocate())
            impl(p, 0, 0);
    }
}

//! Reallocates a block of memory previously allocated with @ref allocate().
//!
//! This function changes the size of the memory block pointed to by @p p to @p size bytes with @p align alignment.
//! The contents are unchanged from the start of the memory block up to the minimum of the old size and @p size. If
//! @p size is larger than the old size, the added memory is not initialized. If @p p is `nullptr`, the call is
//! equivalent to `allocate(size, align)`; if @p size is `0` and @p p is not `nullptr`, the call is equivalent to
//! `deallocate(p)`. Unless @p p is `nullptr`, it must have been retrieved by an earlier call to @ref allocate() or
//! @ref reallocate(). If the memory region was moved in order to resize it, @p p will be freed as with `deallocate(p)`.
//!
//! @note Any plugin (or the executable) may @ref allocate the memory and a different plugin (or the executable) may
//! @ref deallocate or @ref reallocate it.
//!
//! @note If carb.dll/libcarb.so is not loaded, this function will always return @p p without side-effects.
//!
//! @param p The block of memory previously returned from @ref allocate() or @ref reallocate() if resizing is
//!     resizing is desired. If `nullptr` is passed as this parameter, the call behaves as if
//!     `allocate(size, align)` was called.
//! @param size The size of the memory block requested, in bytes. See above for further explanation.
//! @param align The minimum alignment (in bytes) of the memory block requested. Must be a power of two. Values less
//!     than `sizeof(size_t)` are ignored. Changing the alignment from a previous allocation is undefined behavior.
//!     `0` indicates to use default system alignment (typically `2 * sizeof(void*)`).
//! @returns A pointer to a block of memory of @p size bytes with minimum alignment @p align, unless an error
//!     occurs in which case `nullptr` is returned. If @p p is `nullptr` and @p size is `0` then `nullptr` is also
//!     returned.
inline void* reallocate(void* p, size_t size, size_t align = 0) noexcept
{
    if (auto impl = detail::getCarbReallocate())
        return impl(p, size, align);
    else
        return p;
}
} // namespace carb
