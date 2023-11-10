// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief Provides templated helper functions to fill an arbitrary array of values.
//!
#pragma once

#include "../core/IObject.h" // for omni::core::Result

#include <type_traits>
#include <vector>

namespace omni
{
//! common namespace for extra helper functions and classes.
namespace extras
{

//! Fills the array given by outArray by calling fillFn.
//!
//! fillFn's signature is void(T* outArray, uint32_t outArrayCount).
//!
//! If outArrayCount is `nullptr`, kResultInvalidArgument is returned.
//!
//! If outArray is `nullptr`, *outArrayCount is populated with requiredAccount.
//!
//! If *outArrayCount is less than requiredCount, kResultInsufficientBuffer is returned.
//!
//! If the checks above pass, outArray is filled by the given function.  *outArrayCount is updated to requiredCount.
template <typename T, typename Callable, typename SizeType>
omni::core::Result fillOutArray(T* outArray, SizeType* outArrayCount, SizeType requiredCount, Callable&& fillFn)
{
    static_assert(std::is_unsigned<SizeType>::value, "SizeType must be an unsigned integer type!");

    if (!outArrayCount)
    {
        return omni::core::kResultInvalidArgument;
    }

    if (!outArray)
    {
        *outArrayCount = requiredCount;
        return omni::core::kResultSuccess;
    }

    if (*outArrayCount < requiredCount)
    {
        *outArrayCount = requiredCount;
        return omni::core::kResultInsufficientBuffer;
    }

    *outArrayCount = requiredCount;

    fillFn(outArray, requiredCount);

    return omni::core::kResultSuccess;
}

//! Retrieves an array of unknown size using @p getFn and passes it to @p fillFn.
//!
//! This utility is useful for transferring a raw array from the ABI to a modern C++ container.
//!
//! In order to avoid heap access, @p stackCount can be used to try to transfer the array from @p getFn to @p fillFn
//! using temporary stack storage.  @p stackCount is the number of T's that should be temporarily allocated on the
//! stack.  If
//! @p stackCount is inadequate, this function falls back to using the heap.  Care should be taken to not set @p
//! stackCount to a value that would cause a stack overflow.
//!
//! The source array may be dynamically growing in another thread, in which case this method will try @p maxRetryCount
//! times to allocate an array of the proper size and retrieve the values.  If this method exceeds the retry count,
//! @ref omni::core::kResultTryAgain is returned.
//!
//! @retval omni::core::kResultSuccess on success, an appropriate error code otherwise.
template <typename T, typename GetCallable, typename FillCallable>
omni::core::Result getOutArray(GetCallable&& getFn,
                               FillCallable&& fillFn,
                               uint32_t stackCount = (4096 / sizeof(T)),
                               uint32_t maxRetryCount = (UINT32_MAX - 1))
{
    // constructors won't run when we call alloca, make sure the type doesn't need a constructor to run.
    static_assert(std::is_trivially_default_constructible<T>::value, "T must be trivially default constructible");

    T* stack = CARB_STACK_ALLOC(T, stackCount);
    std::vector<T> heap;
    T* buffer = stack;
    uint32_t count = stackCount;

    omni::core::Result result = getFn(buffer, &count);
    uint32_t retryCount = maxRetryCount + 1;
    while (--retryCount)
    {
        switch (result)
        {
            case omni::core::kResultSuccess:
                if (buffer)
                {
                    fillFn(buffer, count);
                    return omni::core::kResultSuccess;
                }
                CARB_FALLTHROUGH; // (alloca returned nullptr, count is now correct and we should alloc on heap)
            case omni::core::kResultInsufficientBuffer:
                heap.resize(count);
                buffer = heap.data();
                result = getFn(buffer, &count);
                break;
            default:
                return result;
        }
    }

    return omni::core::kResultTryAgain;
}

} // namespace extras
} // namespace omni
