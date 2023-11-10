// Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//! @brief Implementation of select components from C++20 `<type_traits>` library
#pragma once

#include "../cpp17/TypeTraits.h"

namespace carb
{
namespace cpp20
{

//! @copydoc carb::cpp17::is_nothrow_convertible
using carb::cpp17::is_nothrow_convertible;

//! Provides the member typedef type that names T (i.e. the identity transformation).
//! This can be used to establish non-deduced contexts in template argument deduction.
template <class T>
struct type_identity
{
    //! The identity transformation.
    using type = T;
};

//! Helper type for \ref type_identity
template <class T>
using type_identity_t = typename type_identity<T>::type;

} // namespace cpp20
} // namespace carb
