// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include <omni/core/IObject.h>

namespace omni
{
namespace ext
{

OMNI_DECLARE_INTERFACE(IExtensionData);

//! Information about an extension.
class IExtensionData_abi : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.ext.IExtensionData")>
{
protected:
    //! The extension id.  For example: omni.example.greet.
    //!
    //! The memory returned is valid for the lifetime of this object.
    virtual OMNI_ATTR("c_str, not_null") const char* getId_abi() noexcept = 0;

    //! The directory which contains the extension.  For example:
    //! c:/users/ncournia/dev/kit/kit/_build/windows-x86_64/debug/exts/omni.example.greet.
    //!
    //! The memory returned is valid for the lifetime of this object.
    virtual OMNI_ATTR("c_str, not_null") const char* getDirectory_abi() noexcept = 0;
};

} // namespace ext
} // namespace omni

#include "IExtensionData.gen.h"
