// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
// --------- Warning: This is a build system generated file. ----------
//

//! @file
//!
//! @brief This file was generated by <i>omni.bind</i>.

#include <omni/core/Interface.h>
#include <omni/core/OmniAttr.h>
#include <omni/core/ResultError.h>

#include <functional>
#include <type_traits>
#include <utility>

#ifndef OMNI_BIND_INCLUDE_INTERFACE_IMPL


//! Hooks that can be defined by plugins to better understand how the plugin is being used by the extension system.
template <>
class omni::core::Generated<omni::ext::IExtensionHooks_abi> : public omni::ext::IExtensionHooks_abi
{
public:
    OMNI_PLUGIN_INTERFACE("omni::ext::IExtensionHooks")

    //! Called when an extension loads the plugin.
    //!
    //! If multiple extensions load the plugin, this method will be called multiple times.
    void onStartup(omni::core::ObjectParam<omni::ext::IExtensionData> ext) noexcept;

    //! Called when an extension that uses this plugin is unloaded.
    //!
    //! If multiple extension load the plugin, this method will be called multiple times.
    void onShutdown(omni::core::ObjectParam<omni::ext::IExtensionData> ext) noexcept;
};

#endif

#ifndef OMNI_BIND_INCLUDE_INTERFACE_DECL

inline void omni::core::Generated<omni::ext::IExtensionHooks_abi>::onStartup(
    omni::core::ObjectParam<omni::ext::IExtensionData> ext) noexcept
{
    onStartup_abi(ext.get());
}

inline void omni::core::Generated<omni::ext::IExtensionHooks_abi>::onShutdown(
    omni::core::ObjectParam<omni::ext::IExtensionData> ext) noexcept
{
    onShutdown_abi(ext.get());
}

#endif

#undef OMNI_BIND_INCLUDE_INTERFACE_DECL
#undef OMNI_BIND_INCLUDE_INTERFACE_IMPL
