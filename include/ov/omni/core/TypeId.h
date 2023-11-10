// Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief Helper functions and macros for generating type identifiers.
#pragma once

#include "../../carb/Defines.h"

#include <cstdint>

namespace omni
{
namespace core
{

//! Base type for an interface type identifier.
using TypeId = uint64_t;

//! Returns the type id of the given type name at compile time.
//!
//! See omni::core::typeId() for a version of this macro that is evaluated at runtime.
#define OMNI_TYPE_ID(str_) CARB_HASH_STRING(str_)

//! Returns the type id of the given type name at run time.
//!
//! If possible, use OMNI_TYPE_ID(), which is evaluated at compile time.
inline TypeId typeId(const char* str)
{
    // currently, the user must choose between calling typeId() or OMNI_TYPE_ID() to map a string to an id.  the user's
    // decision should be based on if the input string is known at compile time or not.
    //
    // we should be able to have a single constexpr function, typeId(), that will determine if the input string is
    // constant or not.  the compiler would then be able to evaluate the string correctly at compile time or runtime.
    // this would make the user's life easier.
    //
    // unfortunately, MSVC incorrectly warns when it detects that an unsigned expression will overflow in a constexpr.
    // overflowing an unsigned value is well-defined and actually by design in our hashing algorithm.
    //
    // disabling this warning is even more of a pain, as MSVC doesn't allow you to disable the warning at the
    // overflowing expression.  rather, you must disable the warning at each call site of the constexpr.
    //
    // this bug is fixed in late 2019.  since customers may be on older compilers, we're stuck with this workaround.
    //
    // https://developercommunity.visualstudio.com/content/problem/211134/unsigned-integer-overflows-in-constexpr-functionsa.html
    // https://stackoverflow.com/questions/57342279/how-can-i-suppress-constexpr-warnings-in-msvc
    return carb::hashString(str);
}

} // namespace core
} // namespace omni
