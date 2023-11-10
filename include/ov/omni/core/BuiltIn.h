// Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Header file for Omni built-in interfaces.
#pragma once

#include "Api.h"

//! Used by omniGetBuiltInWithoutAcquire() to specify the desired interface.
//!
//! @warning Do not use omniGetBuiltInWithoutAcquire() directly. Instead use the referenced inline function for the
//! desired OmniBuiltIn enum value.
enum class OmniBuiltIn
{
    //! Returns a reference to ITypeFactory. Use omniGetTypeFactoryWithoutAcquire() inline function.
    eITypeFactory,

    //! Returns a reference to ILog. Use omniGetLogWithoutAcquire() inline function.
    eILog,

    //! Returns a reference to IStructuredLog. Use omniGetStructuredLogWithoutAcquire() inline function.
    eIStructuredLog,
};

//! Returns a built-in interface based on the given parameter.
//!
//! @warning This function should not be used. Instead, use the specific inline function for the desired OmniBuiltIn.
OMNI_API void* omniGetBuiltInWithoutAcquire(OmniBuiltIn);
