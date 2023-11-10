// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Utilities to ease the creation of Carbonite plugins.
#pragma once

#include "ClientUtils.h"
#include "PluginInitializers.h"
#include "PluginCoreUtils.h"

namespace omni
{
namespace structuredlog
{
void addModulesSchemas() noexcept;
}
} // namespace omni

//! Plugin helper macro to define boiler-plate code to register and unregister the plugin with various other components
//! in the system (e.g. logging channels, profiler, localization, etc.).
//!
//! Do not directly call this macro, rather call @ref CARB_PLUGIN_IMPL() which will call this macro for you.
#define CARB_DEFAULT_INITIALIZERS()                                                                                    \
    CARB_EXPORT void carbOnPluginPreStartup()                                                                          \
    {                                                                                                                  \
        carb::pluginInitialize();                                                                                      \
        omni::structuredlog::addModulesSchemas();                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    CARB_EXPORT void carbOnPluginPostShutdown()                                                                        \
    {                                                                                                                  \
        carb::pluginDeinitialize();                                                                                    \
    }

//! Main macro to declare a plugin implementation where multiple interface versions are not required.
//!
//! Authors of Carbonite plugins must use this macro in exactly one compilation unit for their plugin to generate code
//! expected by the Carbonite framework.
//!
//! @note Carbonite plugins can provide multiple versions of an interface to remain backwards compatible with apps and
//! modules that are built against earlier versions of plugins. In order to do this, see \ref CARB_PLUGIN_IMPL_EX.
//!
//! In particular, this macro:
//!
//! - Defines global variables, such as @ref g_carbFramework.
//!
//! - Registers a default logging channel with @ref omni::log::ILog.
//!
//! - Adds boiler-plate code for @oni_overview interop.
//!
//! - Adds boiler-plate code for plugin startup, shutdown, and registration. (See @carb_framework_overview for more
//!   information).
//!
//! This macro must be used in the global namespace. A @ref carb::PluginImplDesc must be provided as well as all
//! interfaces exported by this plugin. Each interface must be declared with @ref CARB_PLUGIN_INTERFACE. There must also
//! exist a @ref fillInterface(InterfaceType&) function for each interface type that is exported by this plugin.
//! A trailing semicolon is optional.
//!
//! Example:
//! @code{.cpp}
//! // Plugin Implementation Descriptor
//! const carb::PluginImplDesc kPluginImpl{ "carb.windowing-glfw.plugin", "Windowing (glfw).", "NVIDIA",
//!                                          carb::PluginHotReload::eDisabled, "dev" };
//!
//! // Generate boilerplate code
//! CARB_PLUGIN_IMPL(kPluginImpl, carb::windowing::IWindowing, carb::windowing::IGLContext)
//!
//! // Construct the carb::windowing::IWindowing interface
//! void fillInterface(carb::windowing::IWindowing& iface) { /* ... */ }
//!
//! // Construct the carb::windowing::IGLContext interface
//! void fillInterface(carb::windowing::IGLContext& iface) { /* ... */ }
//! @endcode
//!
//! See @carb_framework_overview and @carb_interfaces for more information on creating Carbonite plugins.
//!
//! @param impl The @ref carb::PluginImplDesc constant to be used as plugin description.
//!
//! @param ... One or more interface types to be implemented by the plugin. An interface is a `struct` or `class` with
//!   a use of @ref CARB_PLUGIN_INTERFACE() inside it. These interface types are constructed by a global function
//!   @ref fillInterface(InterfaceType&) that must exist in the plugin. See @ref fillInterface(InterfaceType&) for more
//!   information about interface construction and destruction.
#define CARB_PLUGIN_IMPL(impl, ...)                                                                                    \
    CARB_GLOBALS_EX(impl.name, impl.description)                                                                       \
    OMNI_MODULE_GLOBALS_FOR_PLUGIN()                                                                                   \
    CARB_PLUGIN_IMPL_WITH_INIT_0_5(impl, __VA_ARGS__) /* for backwards compatibility */                                \
    CARB_PLUGIN_IMPL_WITH_INIT(impl, __VA_ARGS__)                                                                      \
    CARB_DEFAULT_INITIALIZERS()

//! Main macro to declare a plugin implementation where multiple interface versions are required.
//!
//! Authors of Carbonite plugins must use this macro in exactly one compilation unit for their plugin to generate code
//! expected by the Carbonite framework.
//!
//! @note This implementation macro allows Carbonite plugins to provide multiple versions of an interface in order to
//! remain backwards compatible with apps and modules that are built against earlier versions of plugins. Every
//! interface exported by the plugin must have a @ref fillInterface(carb::Version*, void*) function.
//!
//! In particular, this macro:
//!
//! - Defines global variables, such as @ref g_carbFramework.
//!
//! - Registers a default logging channel with @ref omni::log::ILog.
//!
//! - Adds boiler-plate code for @oni_overview interop.
//!
//! - Adds boiler-plate code for plugin startup, shutdown, and registration. (See @carb_framework_overview for more
//!   information).
//!
//! This macro must be used in the global namespace. A @ref carb::PluginImplDesc must be provided as well as all
//! interfaces exported by this plugin. Each interface must be declared with @ref CARB_PLUGIN_INTERFACE. There must also
//! exist a @ref fillInterface(carb::Version*, void*) function for each interface type that is exported by this plugin.
//! A trailing semicolon is optional.
//!
//! Example:
//! @code{.cpp}
//! // Plugin Implementation Descriptor
//! const carb::PluginImplDesc kPluginImpl{ "carb.windowing-glfw.plugin", "Windowing (glfw).", "NVIDIA",
//!                                          carb::PluginHotReload::eDisabled, "dev" };
//!
//! // Generate boilerplate code
//! CARB_PLUGIN_IMPL_EX(kPluginImpl, carb::windowing::IWindowing, carb::windowing::IGLContext)
//!
//! // Construct the carb::windowing::IWindowing interface
//! template <> void fillInterface<carb::windowing::IWindowing>(carb::Version* v, void* iface) { /* ... */ }
//!
//! // Construct the carb::windowing::IGLContext interface
//! template <> void fillInterface<carb::windowing::IGLContext>(carb::Version* v, void* iface) { /* ... */ }
//! @endcode
//!
//! See @carb_framework_overview and @carb_interfaces for more information on creating Carbonite plugins.
//!
//! @param impl The @ref carb::PluginImplDesc constant to be used as plugin description.
//!
//! @param ... One or more interface types to be implemented by the plugin. An interface is a `struct` or `class` with
//!   a use of @ref CARB_PLUGIN_INTERFACE() inside it. These interface types are constructed by a global explicitly-
//!   specialized template function @ref fillInterface(carb::Version*, void*) that must exist in the plugin. See
//!   @ref fillInterface(carb::Version*, void*) for more information about interface construction and destruction.
#define CARB_PLUGIN_IMPL_EX(impl, ...)                                                                                 \
    CARB_GLOBALS_EX(impl.name, impl.description)                                                                       \
    OMNI_MODULE_GLOBALS_FOR_PLUGIN()                                                                                   \
    CARB_PLUGIN_IMPL_WITH_INIT_EX(impl, __VA_ARGS__)                                                                   \
    CARB_PLUGIN_IMPL_WITH_INIT_0_5_EX(impl, __VA_ARGS__) /* for backwards compatibility */                             \
    CARB_DEFAULT_INITIALIZERS()

/**
 * Macros to declare a plugin implementation dependencies.
 *
 * If a plugin lists an interface "A" as dependency it is guaranteed that `carb::Framework::acquireInterface<A>()` call
 * will return it, otherwise it can return `nullptr`. The Framework checks and resolves all dependencies before loading
 * the plugin. If the dependency cannot be loaded (i.e. no plugin satisfies the interface, or a circular load is
 * discovered) then the plugin will fail to load and `nullptr` will be returned from the
 * carb::Framework::acquireInterface() function.
 *
 * @note Circular dependencies can exist as long as they are not stated in the CARB_PLUGIN_IMPL_DEPS() macros. For
 * instance, assume plugins *Alpha*, *Beta*, and *Gamma*. *Alpha* is dependent on *Beta*; *Beta* is dependent on
 * *Gamma*. *Gamma* is dependent on *Alpha*, but cannot list *Alpha* in its CARB_PLUGIN_IMPL_DEPS() macro, nor
 * attempt to acquire and use it in *Gamma*'s carbOnPluginStartup() function. At a later point from within *Gamma*, the
 * desired interface from *Alpha* may be acquired and used. However, in terms of unload order, *Alpha* will be unloaded
 * first, followed by *Beta* and finally *Gamma*. In this case the *Gamma* carbOnPluginShutdown() function must account
 * for the fact that *Alpha* will already be unloaded.
 *
 * @param ... One or more interface types (e.g. `carb::settings::ISettings`) to list as dependencies for this plugin.
 */
#define CARB_PLUGIN_IMPL_DEPS(...)                                                                                     \
    template <typename... Types>                                                                                       \
    static void getPluginDepsTyped(struct carb::InterfaceDesc** deps, size_t* count)                                   \
    {                                                                                                                  \
        static carb::InterfaceDesc depends[] = { Types::getInterfaceDesc()... };                                       \
        *deps = depends;                                                                                               \
        *count = sizeof(depends) / sizeof(depends[0]);                                                                 \
    }                                                                                                                  \
                                                                                                                       \
    CARB_EXPORT void carbGetPluginDeps(struct carb::InterfaceDesc** deps, size_t* count)                               \
    {                                                                                                                  \
        getPluginDepsTyped<__VA_ARGS__>(deps, count);                                                                  \
    }

/**
 * Macro to declare a plugin without dependencies.
 *
 * Calling this macro is not required if there are no dependencies.  This macro exists to make your plugin more
 * readable.
 */
#define CARB_PLUGIN_IMPL_NO_DEPS()                                                                                     \
    CARB_EXPORT void carbGetPluginDeps(struct carb::InterfaceDesc** deps, size_t* count)                               \
    {                                                                                                                  \
        *deps = nullptr;                                                                                               \
        *count = 0;                                                                                                    \
    }

/**
 * Macro to declare a "minimal" plugin.
 *
 * Plugins in the Carbonite ecosystem tend to depend on other plugins.  For example, plugins often want to access
 * Carbonite's logging system via @ref carb::logging::ILogging.  When calling @ref CARB_PLUGIN_IMPL, boiler-plate code
 * is injected to ensure the plugin can use these "common" plugins.
 *
 * This macro avoids taking dependencies on these "common" plugins.  When calling this macro, only the "minimal" boiler
 * plate code is generated in order for the plugin to work. It's up to the developer to add additional code to make the
 * plugin compatible with any desired "common" plugin.
 *
 * Use of this macro is rare in Omniverse.
 */
#define CARB_PLUGIN_IMPL_MINIMAL(impl, ...)                                                                            \
    CARB_FRAMEWORK_GLOBALS(kPluginImpl.name)                                                                           \
    CARB_PLUGIN_IMPL_WITH_INIT(impl, __VA_ARGS__)
