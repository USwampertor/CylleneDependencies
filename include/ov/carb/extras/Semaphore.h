// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

#pragma once
#include "../cpp20/Semaphore.h"

namespace carb
{
namespace extras
{

class CARB_DEPRECATED("Deprecated: carb::extras::binary_semaphore has moved to carb::cpp20::binary_semaphore") binary_semaphore
    : public carb::cpp20::binary_semaphore
{
public:
    constexpr explicit binary_semaphore(ptrdiff_t desired) : carb::cpp20::binary_semaphore(desired)
    {
    }
};

template <ptrdiff_t least_max_value = carb::cpp20::detail::kSemaphoreValueMax>
class CARB_DEPRECATED("Deprecated: carb::extras::counting_semaphore has moved to carb::cpp20::counting_semaphore")
    counting_semaphore : public carb::cpp20::counting_semaphore<least_max_value>
{
public:
    constexpr explicit counting_semaphore(ptrdiff_t desired) : carb::cpp20::counting_semaphore<least_max_value>(desired)
    {
    }
};

} // namespace extras
} // namespace carb
