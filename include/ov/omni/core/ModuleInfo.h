// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief Helper functions for collecting module information.
#pragma once

#include "../../carb/extras/Library.h"
#include "../../carb/Interface.h"
#include "IObject.h"

namespace omni
{
namespace core
{

//! Given an object, returns the name of the module (.dll/.exe) which contains the object's code.
//!
//! @param[in] obj  The object to retrieve the module name for.  This may not be `nullptr`.
//! @returns The name and path of the library that the given object's owning implementation
//!          comes from.  Returns an empty string if the object isn't bound to any particular
//!          library.
inline std::string getModuleFilename(omni::core::IObject* obj)
{
    // getLibraryFilename maps an address to a library.
    //
    // the first entry in IObject is the vtable pointer on both Windows and Linux. here we use the first virtual
    // function address as a function pointer to pass to getLibraryFilename.
    //
    // Fun fact: everyone loves pointer dereferencing gymnastics: ** **** ******* ****
    void** vtbl = *reinterpret_cast<void***>(obj);
    return carb::extras::getLibraryFilename(vtbl[0]);
}

} // namespace core
} // namespace omni


//! Provides a list of dependent interfaces for an ONI plugin.
//!
//! @param[in] ...  The list of fully qualified interface names that this plugin depends on.  This
//!                 should include any interfaces, Carbonite or ONI, that this plugin will attempt
//!                 to acquire or create.  This allows the Carbonite framework to verify that all
//!                 dependent modules or interfaces are available for a plugin when attempting to
//!                 load it, and allows for a more correct shutdown/unload order for plugins.
#define OMNI_PLUGIN_IMPL_DEPS(...)                                                                                     \
    template <typename... Types>                                                                                       \
    static void getPluginDepsTyped(struct carb::InterfaceDesc** deps, size_t* depsCount)                               \
    {                                                                                                                  \
        static carb::InterfaceDesc depends[] = { Types::getInterfaceDesc()... };                                       \
        *deps = depends;                                                                                               \
        *depsCount = sizeof...(Types);                                                                                 \
    }                                                                                                                  \
                                                                                                                       \
    omni::core::Result omniGetDependencies(carb::InterfaceDesc** deps, size_t* depsCount)                              \
    {                                                                                                                  \
        getPluginDepsTyped<__VA_ARGS__>(deps, depsCount);                                                              \
        return omni::core::kResultSuccess;                                                                             \
    }

//! Declares that the calling plugin has no dependencies on any other Carbonite or ONI interfaces.
//!
//! @remarks    This lets the Carbonite plugin manager know that the calling plugin does not
//!             expect to attempt to acquire any other Carbonite interfaces or create any other
//!             ONI objects.  This helps the plugin manager determine the most correct unload or
//!             shutdown order for all plugins.
#define OMNI_PLUGIN_IMPL_NODEPS()                                                                                      \
    omni::core::Result omniGetDependencies(carb::InterfaceDesc** deps, size_t* depsCount)                              \
    {                                                                                                                  \
        *deps = nullptr;                                                                                               \
        *depsCount = 0;                                                                                                \
        return omni::core::kResultSuccess;                                                                             \
    }
