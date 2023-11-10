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
#include <omni/ext/IExtensionData.h>

namespace omni
{
namespace ext
{

OMNI_DECLARE_INTERFACE(IExtensionHooks);

//! Hooks that can be defined by plugins to better understand how the plugin is being used by the extension system.
class IExtensionHooks_abi : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.ext.IExtensionHooks")>
{
protected:
    //! Called when an extension loads the plugin.
    //!
    //! If multiple extensions load the plugin, this method will be called multiple times.
    virtual void onStartup_abi(OMNI_ATTR("not_null") IExtensionData* ext) noexcept = 0;

    //! Called when an extension that uses this plugin is unloaded.
    //!
    //! If multiple extension load the plugin, this method will be called multiple times.
    virtual void onShutdown_abi(IExtensionData* ext) noexcept = 0;
};

} // namespace ext
} // namespace omni

#include "IExtensionHooks.gen.h"
