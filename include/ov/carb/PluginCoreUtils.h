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
//! @brief Utilities to ease the creation of Carbonite plugins.  Most code will include carb/PluginUtils.h
//! instead of this file.
#pragma once

#include "Defines.h"

#include "../omni/core/Api.h" // OMNI_API
#include "../omni/core/Omni.h"

#include <cstddef>
#include <cstdint>

//! See @ref carb::GetFrameworkVersionFn.  Required by plugins.
const char* const kCarbGetFrameworkVersionFnName = "carbGetFrameworkVersion";

//! See @ref carb::OnPluginRegisterFn.  Required by plugins.
const char* const kCarbOnPluginRegisterFnName = "carbOnPluginRegister";

//! See @ref carb::OnPluginRegisterExFn.  Required by plugins.
const char* const kCarbOnPluginRegisterExFnName = "carbOnPluginRegisterEx";

//! See @ref carb::OnPluginRegisterEx2Fn.  Required by plugins.
const char* const kCarbOnPluginRegisterEx2FnName = "carbOnPluginRegisterEx2";

//! See @ref carb::OnPluginPreStartupFn.  Optional for plugins.
const char* const kCarbOnPluginPreStartupFnName = "carbOnPluginPreStartup";

//! See @ref carb::OnPluginStartupFn.  Optional for plugins.
const char* const kCarbOnPluginStartupFnName = "carbOnPluginStartup";

//! See @ref carb::OnPluginStartupExFn.  Optional for plugins.
const char* const kCarbOnPluginStartupExFnName = "carbOnPluginStartupEx";

//! See @ref carb::OnPluginShutdownFn.  Optional for plugins.
const char* const kCarbOnPluginShutdownFnName = "carbOnPluginShutdown";

//! See @ref carb::OnPluginQuickShutdownFn.  Optional for plugins.
const char* const kCarbOnPluginQuickShutdownFnName = "carbOnPluginQuickShutdown";

//! See @ref carb::OnPluginPostShutdownFn.  Optional for plugins.
const char* const kCarbOnPluginPostShutdownFnName = "carbOnPluginPostShutdown";

//! See @ref carb::GetPluginDepsFn.  Optional for plugins.
const char* const kCarbGetPluginDepsFnName = "carbGetPluginDeps";

//! See @ref carb::OnReloadDependencyFn.  Optional for plugins.
const char* const kCarbOnReloadDependencyFnName = "carbOnReloadDependency";

namespace omni
{
namespace core
{
OMNI_DECLARE_INTERFACE(ITypeFactory) // forward declaration
}
namespace log
{
class ILog; // forward declaration
}
} // namespace omni

/// @cond DEV

/**
 * Helper macro to declare globals needed by Carbonite plugins.
 *
 * Do not directly use this macro.  Rather use @ref CARB_PLUGIN_IMPL() which will call it for you.
 */
#define OMNI_MODULE_GLOBALS_FOR_PLUGIN()                                                                               \
    namespace                                                                                                          \
    {                                                                                                                  \
    ::omni::core::ITypeFactory* s_omniTypeFactory = nullptr;                                                           \
    ::omni::log::ILog* s_omniLog = nullptr;                                                                            \
    ::omni::structuredlog::IStructuredLog* s_omniStructuredLog = nullptr;                                              \
    }                                                                                                                  \
    OMNI_MODULE_DEFINE_LOCATION_FUNCTIONS()                                                                            \
    OMNI_MODULE_GLOBALS_BUILD_CONFIG_SYMBOLS();                                                                        \
    OMNI_API void* omniGetBuiltInWithoutAcquire(::OmniBuiltIn type)                                                    \
    {                                                                                                                  \
        switch (type)                                                                                                  \
        {                                                                                                              \
            case ::OmniBuiltIn::eITypeFactory:                                                                         \
                return s_omniTypeFactory;                                                                              \
            case ::OmniBuiltIn::eILog:                                                                                 \
                return s_omniLog;                                                                                      \
            case ::OmniBuiltIn::eIStructuredLog:                                                                       \
                return s_omniStructuredLog;                                                                            \
            default:                                                                                                   \
                return nullptr;                                                                                        \
        }                                                                                                              \
    }

/**
 * Populates the Omniverse interfaces portion of @ref carb::PluginFrameworkDesc.
 *
 * Do not directly use this macro.  This macro is called by default by the @ref CARB_PLUGIN_IMPL_WITH_INIT() provided
 * version of @ref carb::OnPluginRegisterFn.
 */
#define OMNI_MODULE_SET_GLOBALS_FOR_PLUGIN(in_)                                                                        \
    s_omniTypeFactory = (in_)->omniTypeFactory;                                                                        \
    s_omniLog = (in_)->omniLog;                                                                                        \
    s_omniStructuredLog = (in_)->omniStructuredLog;

#ifndef DOXYGEN_SHOULD_SKIP_THIS

// FOR_EACH macro implementation, use as FOR_EACH(OTHER_MACRO, p0, p1, p2,)
#    define EXPAND(x) x
#    define FE_1(WHAT, X) EXPAND(WHAT(X))
#    define FE_2(WHAT, X, ...) EXPAND(WHAT(X) FE_1(WHAT, __VA_ARGS__))
#    define FE_3(WHAT, X, ...) EXPAND(WHAT(X) FE_2(WHAT, __VA_ARGS__))
#    define FE_4(WHAT, X, ...) EXPAND(WHAT(X) FE_3(WHAT, __VA_ARGS__))
#    define FE_5(WHAT, X, ...) EXPAND(WHAT(X) FE_4(WHAT, __VA_ARGS__))
#    define FE_6(WHAT, X, ...) EXPAND(WHAT(X) FE_5(WHAT, __VA_ARGS__))
#    define FE_7(WHAT, X, ...) EXPAND(WHAT(X) FE_6(WHAT, __VA_ARGS__))
#    define FE_8(WHAT, X, ...) EXPAND(WHAT(X) FE_7(WHAT, __VA_ARGS__))
#    define FE_9(WHAT, X, ...) EXPAND(WHAT(X) FE_8(WHAT, __VA_ARGS__))
#    define FE_10(WHAT, X, ...) EXPAND(WHAT(X) FE_9(WHAT, __VA_ARGS__))
#    define FE_11(WHAT, X, ...) EXPAND(WHAT(X) FE_10(WHAT, __VA_ARGS__))
#    define FE_12(WHAT, X, ...) EXPAND(WHAT(X) FE_11(WHAT, __VA_ARGS__))
#    define FE_13(WHAT, X, ...) EXPAND(WHAT(X) FE_12(WHAT, __VA_ARGS__))
#    define FE_14(WHAT, X, ...) EXPAND(WHAT(X) FE_13(WHAT, __VA_ARGS__))
#    define FE_15(WHAT, X, ...) EXPAND(WHAT(X) FE_14(WHAT, __VA_ARGS__))
#    define FE_16(WHAT, X, ...) EXPAND(WHAT(X) FE_15(WHAT, __VA_ARGS__))
#    define FE_17(WHAT, X, ...) EXPAND(WHAT(X) FE_16(WHAT, __VA_ARGS__))
#    define FE_18(WHAT, X, ...) EXPAND(WHAT(X) FE_17(WHAT, __VA_ARGS__))
#    define FE_19(WHAT, X, ...) EXPAND(WHAT(X) FE_18(WHAT, __VA_ARGS__))
#    define FE_20(WHAT, X, ...) EXPAND(WHAT(X) FE_19(WHAT, __VA_ARGS__))
#    define FE_21(WHAT, X, ...) EXPAND(WHAT(X) FE_20(WHAT, __VA_ARGS__))
#    define FE_22(WHAT, X, ...) EXPAND(WHAT(X) FE_21(WHAT, __VA_ARGS__))
#    define FE_23(WHAT, X, ...) EXPAND(WHAT(X) FE_22(WHAT, __VA_ARGS__))
#    define FE_24(WHAT, X, ...) EXPAND(WHAT(X) FE_23(WHAT, __VA_ARGS__))
#    define FE_25(WHAT, X, ...) EXPAND(WHAT(X) FE_24(WHAT, __VA_ARGS__))
#    define FE_26(WHAT, X, ...) EXPAND(WHAT(X) FE_25(WHAT, __VA_ARGS__))
#    define FE_27(WHAT, X, ...) EXPAND(WHAT(X) FE_26(WHAT, __VA_ARGS__))
#    define FE_28(WHAT, X, ...) EXPAND(WHAT(X) FE_27(WHAT, __VA_ARGS__))
#    define FE_29(WHAT, X, ...) EXPAND(WHAT(X) FE_28(WHAT, __VA_ARGS__))
#    define FE_30(WHAT, X, ...) EXPAND(WHAT(X) FE_29(WHAT, __VA_ARGS__))
#    define FE_31(WHAT, X, ...) EXPAND(WHAT(X) FE_30(WHAT, __VA_ARGS__))
#    define FE_32(WHAT, X, ...) EXPAND(WHAT(X) FE_31(WHAT, __VA_ARGS__))


//... repeat as needed
#    define GET_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21,  \
                      _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, NAME, ...)                                \
        NAME
#    define FOR_EACH(action, ...)                                                                                      \
        EXPAND(GET_MACRO(__VA_ARGS__, FE_32, FE_31, FE_30, FE_29, FE_28, FE_27, FE_26, FE_25, FE_24, FE_23, FE_22,     \
                         FE_21, FE_20, FE_19, FE_18, FE_17, FE_16, FE_15, FE_14, FE_13, FE_12, FE_11, FE_10, FE_9,     \
                         FE_8, FE_7, FE_6, FE_5, FE_4, FE_3, FE_2, FE_1)(action, __VA_ARGS__))


#    define DECLARE_FILL_FUNCTION(X) void fillInterface(X& iface);

// carbOnPluginRegisterEx2() was added with carbonite version 0.5 without changing the carbonite version number.
// Therefore, this exists only to support older carbonite version 0.5 instances that are not aware of
// carbOnPluginRegisterEx2. This macro can be safely removed when Framework version 0.5 is no longer supported.
static_assert(carb::kFrameworkVersion.major == 0, "Remove CARB_PLUGIN_IMPL_WITH_INIT_0_5");
#    define CARB_PLUGIN_IMPL_WITH_INIT_0_5(impl, ...)                                                                  \
        FOR_EACH(DECLARE_FILL_FUNCTION, __VA_ARGS__)                                                                   \
        template <typename T1>                                                                                         \
        void fillInterface0_5(carb::PluginRegistryEntry::Interface* interfaces)                                        \
        {                                                                                                              \
            interfaces[0].desc = T1::getInterfaceDesc();                                                               \
            static T1 s_pluginInterface{};                                                                             \
            fillInterface(s_pluginInterface);                                                                          \
            interfaces[0].ptr = &s_pluginInterface;                                                                    \
            interfaces[0].size = sizeof(T1);                                                                           \
        }                                                                                                              \
        template <typename T1, typename T2, typename... Types>                                                         \
        void fillInterface0_5(carb::PluginRegistryEntry::Interface* interfaces)                                        \
        {                                                                                                              \
            fillInterface0_5<T1>(interfaces);                                                                          \
            fillInterface0_5<T2, Types...>(interfaces + 1);                                                            \
        }                                                                                                              \
        template <typename... Types>                                                                                   \
        static void onPluginRegister0_5(carb::PluginFrameworkDesc* frameworkDesc, carb::PluginRegistryEntry* outEntry) \
        {                                                                                                              \
            static carb::PluginRegistryEntry::Interface s_interfaces[sizeof...(Types)] = {};                           \
            fillInterface0_5<Types...>(s_interfaces);                                                                  \
            outEntry->interfaces = s_interfaces;                                                                       \
            outEntry->interfaceCount = sizeof(s_interfaces) / sizeof(s_interfaces[0]);                                 \
            outEntry->implDesc = impl;                                                                                 \
                                                                                                                       \
            g_carbFramework = frameworkDesc->framework;                                                                \
            g_carbClientName = impl.name;                                                                              \
            OMNI_MODULE_SET_GLOBALS_FOR_PLUGIN(frameworkDesc)                                                          \
        }                                                                                                              \
        CARB_EXPORT void carbOnPluginRegisterEx(                                                                       \
            carb::PluginFrameworkDesc* frameworkDesc, carb::PluginRegistryEntry* outEntry)                             \
        {                                                                                                              \
            onPluginRegister0_5<__VA_ARGS__>(frameworkDesc, outEntry);                                                 \
        }

#    define CARB_PLUGIN_IMPL_WITH_INIT_0_5_EX(impl, ...)                                                                                                   \
        template <typename T1>                                                                                                                             \
        void fillInterface0_5(carb::PluginRegistryEntry::Interface* interfaces)                                                                            \
        {                                                                                                                                                  \
            interfaces->desc = T1::getInterfaceDesc();                                                                                                     \
            static void* s_pluginInterface[(sizeof(T1) / sizeof(void*)) + 1] = {};                                                                         \
            carb::Version ver = interfaces->desc.version;                                                                                                  \
            bool b = fillInterface<T1>(&ver, s_pluginInterface);                                                                                           \
            CARB_FATAL_UNLESS(b, "Failed to construct interface for type %s", interfaces->desc.name);                                                      \
            CARB_FATAL_UNLESS(                                                                                                                             \
                ver == interfaces->desc.version,                                                                                                           \
                "Interface %s constructor requested version %u.%u but got %u.%u (must match exactly as the interface declared itself to be this version)", \
                interfaces[0].desc.name, interfaces->desc.version.major, interfaces->desc.version.minor, ver.major,                                        \
                ver.minor);                                                                                                                                \
            interfaces->ptr = &s_pluginInterface;                                                                                                          \
            interfaces->size = sizeof(T1);                                                                                                                 \
        }                                                                                                                                                  \
        template <typename T1, typename T2, typename... Types>                                                                                             \
        void fillInterface0_5(carb::PluginRegistryEntry::Interface* interfaces)                                                                            \
        {                                                                                                                                                  \
            fillInterface0_5<T1>(interfaces);                                                                                                              \
            fillInterface0_5<T2, Types...>(interfaces + 1);                                                                                                \
        }                                                                                                                                                  \
        template <typename... Types>                                                                                                                       \
        static void onPluginRegister0_5(carb::PluginFrameworkDesc* frameworkDesc, carb::PluginRegistryEntry* outEntry)                                     \
        {                                                                                                                                                  \
            static carb::PluginRegistryEntry::Interface s_interfaces[sizeof...(Types)];                                                                    \
            fillInterface0_5<Types...>(s_interfaces);                                                                                                      \
            outEntry->interfaces = s_interfaces;                                                                                                           \
            outEntry->interfaceCount = sizeof(s_interfaces) / sizeof(s_interfaces[0]);                                                                     \
            outEntry->implDesc = impl;                                                                                                                     \
                                                                                                                                                           \
            g_carbFramework = frameworkDesc->framework;                                                                                                    \
            g_carbClientName = impl.name;                                                                                                                  \
            OMNI_MODULE_SET_GLOBALS_FOR_PLUGIN(frameworkDesc)                                                                                              \
        }                                                                                                                                                  \
        CARB_EXPORT void carbOnPluginRegisterEx(                                                                                                           \
            carb::PluginFrameworkDesc* frameworkDesc, carb::PluginRegistryEntry* outEntry)                                                                 \
        {                                                                                                                                                  \
            onPluginRegister0_5<__VA_ARGS__>(frameworkDesc, outEntry);                                                                                     \
        }

#endif // DOXYGEN_SHOULD_SKIP_THIS

/**
 * Defines boiler-plate code to declare the plugin's interfaces and registration code.
 *
 * Rather than directly calling this macro, consider calling @ref CARB_PLUGIN_IMPL which calls this macro for you.
 *
 * This macro does the following:
 *
 *  - Defines `carbGetFrameworkVersion` and `carbOnPluginRegisterEx2` functions.
 *
 *  - Sets the @ref g_carbFramework variable so @ref carb::getFramework() works.
 *
 *  - Sets the plugin client variable: @ref g_carbClientName.  The client name is used by @ref carb::Framework to
 *    create a graph of client inter-dependencies.
 *
 *  - Advertises to @ref carb::Framework the interfaces implemented by this plugin.
 *
 *  - Enables the usage of ONI (see @oni_overview) in the plugin.
 *
 * This macro must be defined in the global namespace.
 *
 * @param impl The PluginImplDesc constant to be used as plugin description.
 *
 * @param ... One or more interface types to be implemented by the plugin. An interface is a `struct` with
 * a call to @ref CARB_PLUGIN_INTERFACE() inside it. These interface types are constructed during plugin registration
 * (prior to the plugin startup) and destructed immediately after plugin shutdown. A global fillInterface() function
 * must exist and will be called immediately after instantiating the interface type. The interface types need not be
 * trivially constructed or destructed, but their constructors and destructors MUST NOT use any Carbonite framework
 * functions.
 */
#define CARB_PLUGIN_IMPL_WITH_INIT(impl, ...)                                                                          \
                                                                                                                       \
    /* Forward declare fill functions for every interface */                                                           \
    FOR_EACH(DECLARE_FILL_FUNCTION, __VA_ARGS__)                                                                       \
                                                                                                                       \
    template <typename T1>                                                                                             \
    void populate(carb::PluginRegistryEntry2::Interface2* iface)                                                       \
    {                                                                                                                  \
        iface->sizeofThisStruct = sizeof(carb::PluginRegistryEntry2::Interface2);                                      \
        iface->desc = T1::getInterfaceDesc();                                                                          \
        iface->size = sizeof(T1);                                                                                      \
        iface->align = alignof(T1);                                                                                    \
        iface->Constructor = [](void* p) { fillInterface(*new (p) T1); };                                              \
        iface->Destructor = [](void* p) { static_cast<T1*>(p)->~T1(); };                                               \
    }                                                                                                                  \
                                                                                                                       \
    template <typename T1, typename T2, typename... Types>                                                             \
    void populate(carb::PluginRegistryEntry2::Interface2* interfaces)                                                  \
    {                                                                                                                  \
        populate<T1>(interfaces);                                                                                      \
        populate<T2, Types...>(interfaces + 1);                                                                        \
    }                                                                                                                  \
                                                                                                                       \
    template <typename... Types>                                                                                       \
    static void registerPlugin(carb::PluginFrameworkDesc* frameworkDesc, carb::PluginRegistryEntry2* outEntry)         \
    {                                                                                                                  \
        outEntry->sizeofThisStruct = sizeof(carb::PluginRegistryEntry2);                                               \
        static carb::PluginRegistryEntry2::Interface2 s_interfaces[sizeof...(Types)] = {};                             \
        populate<Types...>(s_interfaces);                                                                              \
        outEntry->interfaces = s_interfaces;                                                                           \
        outEntry->interfaceCount = CARB_COUNTOF(s_interfaces);                                                         \
        outEntry->implDesc = impl;                                                                                     \
                                                                                                                       \
        g_carbFramework = frameworkDesc->framework;                                                                    \
        g_carbClientName = impl.name;                                                                                  \
        OMNI_MODULE_SET_GLOBALS_FOR_PLUGIN(frameworkDesc)                                                              \
    }                                                                                                                  \
                                                                                                                       \
    CARB_EXPORT void carbOnPluginRegisterEx2(                                                                          \
        carb::PluginFrameworkDesc* frameworkDesc, carb::PluginRegistryEntry2* outEntry)                                \
    {                                                                                                                  \
        registerPlugin<__VA_ARGS__>(frameworkDesc, outEntry);                                                          \
    }                                                                                                                  \
                                                                                                                       \
    CARB_EXPORT carb::Version carbGetFrameworkVersion()                                                                \
    {                                                                                                                  \
        return carb::kFrameworkVersion;                                                                                \
    }

#define CARB_PLUGIN_IMPL_WITH_INIT_EX(impl, ...)                                                                       \
    /* forward declare fillInterface function */                                                                       \
    template <class T1>                                                                                                \
    bool fillInterface(carb::Version*, void*);                                                                         \
    template <class T1>                                                                                                \
    void destroyInterface(carb::Version, void*)                                                                        \
    {                                                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    template <typename T1>                                                                                             \
    void populate(carb::PluginRegistryEntry2::Interface2* iface)                                                       \
    {                                                                                                                  \
        iface->sizeofThisStruct = sizeof(carb::PluginRegistryEntry2::Interface2);                                      \
        iface->desc = T1::getInterfaceDesc();                                                                          \
        iface->size = sizeof(T1);                                                                                      \
        iface->align = alignof(T1);                                                                                    \
        iface->Constructor = nullptr;                                                                                  \
        iface->Destructor = nullptr;                                                                                   \
        iface->VersionedConstructor = &fillInterface<T1>;                                                              \
        iface->VersionedDestructor = &destroyInterface<T1>;                                                            \
    }                                                                                                                  \
                                                                                                                       \
    template <typename T1, typename T2, typename... Types>                                                             \
    void populate(carb::PluginRegistryEntry2::Interface2* interfaces)                                                  \
    {                                                                                                                  \
        populate<T1>(interfaces);                                                                                      \
        populate<T2, Types...>(interfaces + 1);                                                                        \
    }                                                                                                                  \
                                                                                                                       \
    template <typename... Types>                                                                                       \
    static void registerPlugin(carb::PluginFrameworkDesc* frameworkDesc, carb::PluginRegistryEntry2* outEntry)         \
    {                                                                                                                  \
        outEntry->sizeofThisStruct = sizeof(carb::PluginRegistryEntry2);                                               \
        static carb::PluginRegistryEntry2::Interface2 s_interfaces[sizeof...(Types)] = {};                             \
        populate<Types...>(s_interfaces);                                                                              \
        outEntry->interfaces = s_interfaces;                                                                           \
        outEntry->interfaceCount = CARB_COUNTOF(s_interfaces);                                                         \
        outEntry->implDesc = impl;                                                                                     \
                                                                                                                       \
        g_carbFramework = frameworkDesc->framework;                                                                    \
        g_carbClientName = impl.name;                                                                                  \
        OMNI_MODULE_SET_GLOBALS_FOR_PLUGIN(frameworkDesc)                                                              \
    }                                                                                                                  \
                                                                                                                       \
    CARB_EXPORT void carbOnPluginRegisterEx2(                                                                          \
        carb::PluginFrameworkDesc* frameworkDesc, carb::PluginRegistryEntry2* outEntry)                                \
    {                                                                                                                  \
        registerPlugin<__VA_ARGS__>(frameworkDesc, outEntry);                                                          \
    }                                                                                                                  \
                                                                                                                       \
    CARB_EXPORT carb::Version carbGetFrameworkVersion()                                                                \
    {                                                                                                                  \
        return carb::kFrameworkVersion;                                                                                \
    }

/// @endcond

#if CARB_COMPILER_MSC
#    pragma section(".state", read, write)
#endif

/**
 * Macro to mark static and global variables to keep them when plugin is hot-reloaded.
 *
 * @rst
   .. deprecated:: 132.0
        Hot reloading support has been removed.  No replacement will be provided.  Note that any symbol
        this decorator is used on will generate a deprecation warning.
   @endrst
 */
#define CARB_STATE                                                                                                     \
    CARB_DEPRECATED("hot reload has been removed") CARB_DECLSPEC(allocate(".state")) CARB_ATTRIBUTE(section(".state"))


// The below is for documentation only
#ifdef DOXYGEN_BUILD
/**
 * An automatically-generated function exported by Carbonite plugins used to check the plugin's Framework compatibility.
 *
 * \note This function is automatically generated for each plugin by the \ref CARB_PLUGIN_IMPL macro. It is called by
 * the Framework when registering a plugin. This serves as documentation of this function only.
 *
 * The type of this function is \ref carb::GetFrameworkVersionFn and named \ref kCarbGetFrameworkVersionFnName.
 *
 * @returns The Framework version that the plugin was built against. The Framework uses this result to check if the
 * plugin is <a href="https://semver.org/">semantically compatible</a> with the Framework in order to continue loading.
 */
CARB_EXPORT carb::Version carbGetFrameworkVersion();

/**
 * An automatically-generated function exported by some Carbonite plugins (now deprecated).
 *
 * \note This function is automatically generated in some older plugins by the \ref CARB_PLUGIN_IMPL macro. It may be
 * called by the Framework when registering a plugin. This serves as documentation of this function only.
 *
 * \warning This function has been superseded by \ref carbOnPluginRegisterEx and \ref carbOnPluginRegisterEx2 in
 * Framework version 0.5. The Framework will look for and call the first available function from the following list:
 * \ref carbOnPluginRegisterEx2, \ref carbOnPluginRegisterEx, `carbOnPluginRegister` (this function).
 *
 * The type of this function is \ref carb::OnPluginRegisterFn and named \ref kCarbOnPluginRegisterFnName.
 *
 * Only plugins built with Framework versions prior to 0.5 export this function.
 *
 * @param framework The Framework will pass this function a pointer to itself when calling.
 * @param outEntry The plugin will populate this structure to inform the Framework about itself.
 */
CARB_EXPORT void carbOnPluginRegister(carb::Framework* framework, carb::PluginRegistryEntry* outEntry);

/**
 * An automatically-generated function exported by some Carbonite plugins (now deprecated).
 *
 * \note This function is automatically generated in some older plugins by the \ref CARB_PLUGIN_IMPL macro. It may be
 * called by the Framework when registering a plugin. This serves as documentation of this function only.
 *
 * \warning This function has been superseded by \ref carbOnPluginRegisterEx2 in Framework version 0.5. The Framework
 * will look for and call the first available function from the following list:
 * \ref carbOnPluginRegisterEx2, `carbOnPluginRegisterEx` (this function), \ref carbOnPluginRegister.
 *
 * The type of this function is \ref carb::OnPluginRegisterExFn and named \ref kCarbOnPluginRegisterExFnName.
 *
 * This function is generated for all plugins built against Framework 0.5. Since \ref carbOnPluginRegisterEx2 was added
 * to Framework version 0.5 without changing the Framework version (in Carbonite release v111.17), this function exists
 * and is exported in all plugins compatible with Framework version 0.5 to allow the plugins to load properly in earlier
 * editions of Framework version 0.5.
 *
 * @param frameworkDesc A description of the Framework provided by the Framework when it calls this function.
 * @param outEntry The plugin will populate this structure to inform the Framework about itself.
 */
CARB_EXPORT void carbOnPluginRegisterEx(carb::PluginFrameworkDesc* frameworkDesc, carb::PluginRegistryEntry* outEntry);

/**
 * An automatically-generated function exported by some Carbonite plugins.
 *
 * \note This function is automatically generated in plugins by the \ref CARB_PLUGIN_IMPL macro. It is called by the
 * Framework when registering a plugin. This serves as documentation of this function only.
 *
 * \note Older versions of this function exist in older plugins. This is the most current registration function as of
 * Framework version 0.5 and intended to be "future proof" for future Framework versions. The Framework will look for
 * and call the first available function from the following list:
 * `carbOnPluginRegisterEx2` (this function), \ref carbOnPluginRegisterEx, \ref carbOnPluginRegister.
 *
 * The type of this function is \ref carb::OnPluginRegisterEx2Fn and named \ref kCarbOnPluginRegisterEx2FnName.
 *
 * This function is generated for all plugins built against Carbonite v111.17 and later. Prior Carbonite releases with
 * Framework version 0.5 will use the \ref carbOnPluginRegisterEx function, whereas Carbonite releases v111.17 and later
 * will use this function to register a plugin.
 *
 * @param frameworkDesc A description of the Framework provided by the Framework when it calls this function.
 * @param outEntry The plugin will populate this structure to inform the Framework about itself.
 */
CARB_EXPORT void carbOnPluginRegisterEx2(carb::PluginFrameworkDesc* frameworkDesc, carb::PluginRegistryEntry2* outEntry);

/**
 * An automatically-generated function exported by Carbonite plugins.
 *
 * \note This function is automatically generated in plugins by the \ref CARB_DEFAULT_INITIALIZERS macro via the
 * \ref CARB_PLUGIN_IMPL macro. It is called by the Framework when starting the plugin (the first time an interface is
 * acquired from the plugin) prior to calling \ref carbOnPluginStartup. This serves as documentation of the generated
 * function only.
 *
 * This function starts up various Framework-provided subsystems for the plugin: logging, profiling, asserts,
 * localization and structured logging. The following functions are called in the plugin context:
 * - \ref carb::logging::registerLoggingForClient
 * - \ref carb::profiler::registerProfilerForClient
 * - \ref carb::assert::registerAssertForClient
 * - \ref carb::l10n::registerLocalizationForClient
 * - \ref omni::structuredlog::addModulesSchemas()
 *
 * The type of this function is \ref carb::OnPluginPreStartupFn and named \ref kCarbOnPluginPreStartupFnName.
 */
CARB_EXPORT void carbOnPluginPreStartup();

/**
 * An optional function that a plugin author can export from their plugin to start their plugin.
 *
 * The Framework will call this function after \ref carbOnPluginPreStartup when starting the plugin (the first time an
 * interface is acquired from the plugin). This serves as a guide for plugin authors.
 *
 * Providing this function is completely optional.
 *
 * Generally, if this function is provided, a \ref carbOnPluginShutdown should also be provided to cleanup any work
 * done by this function.
 *
 * This function is superseded by \ref carbOnPluginStartupEx, which allows startup to fail gracefully. If that function
 * does not exist, this function is called if it exists.
 *
 * Any interfaces declared as a dependency in \ref CARB_PLUGIN_IMPL_DEPS will be available to this plugin by the time
 * this function is called.
 *
 * This function is allowed to acquire interfaces and interact with the Framework normally (e.g. add hooks, etc.).
 * However, keep in mind that this function is called by the Framework when the Application or another plugin is trying
 * to acquire an interface from this plugin; actions that result in recursively starting the plugin will result in
 * failure to acquire the interface. However, your plugin is allowed to acquire other interfaces from itself in this
 * function.
 *
 * Once this function returns, the Framework considers your plugin as initialized.
 *
 * Typical things this function might do:
 * - Allocate memory and data structures for your plugin
 * - Load settings from \ref carb::settings::ISettings (if available)
 * - Start up libraries and subsystems
 *
 * The type of this function is \ref carb::OnPluginStartupFn and named \ref kCarbOnPluginStartupFnName.
 */
CARB_EXPORT void carbOnPluginStartup();

/**
 * An optional function that a plugin author can export from their plugin to start their plugin.
 *
 * The Framework will call this function after \ref carbOnPluginPreStartup when starting the plugin (the first time an
 * interface is acquired from the plugin). This serves as a guide for plugin authors.
 *
 * Providing this function is completely optional.
 *
 * Generally, if this function is provided, a \ref carbOnPluginShutdown should also be provided to cleanup any work
 * done by this function.
 *
 * This function supersedes \ref carbOnPluginStartup. The main difference is that this function allows the plugin to
 * indicate if startup fails (such as if a required subsystem fails to start) and allow the Framework to fail acquiring
 * an interface gracefully. If this function does not exist, \ref carbOnPluginStartup is called if it exists.
 *
 * Any interfaces declared as a dependency in \ref CARB_PLUGIN_IMPL_DEPS will be available to this plugin by the time
 * this function is called.
 *
 * This function is allowed to acquire interfaces and interact with the Framework normally (e.g. add hooks, etc.).
 * However, keep in mind that this function is called by the Framework when the Application or another plugin is trying
 * to acquire an interface from this plugin; actions that result in recursively starting the plugin will result in
 * failure to acquire the interface. However, your plugin is allowed to acquire other interfaces from itself in this
 * function.
 *
 * Once this function returns successfully, the Framework considers your plugin as initialized. If this function reports
 * failure, the plugin will be unloaded but remain registered. Attempting to acquire an interface from this plugin in
 * the future will reload the plugin and attempt to call this function again.
 *
 * Typical things this function might do:
 * - Allocate memory and data structures for your plugin
 * - Load settings from \ref carb::settings::ISettings (if available)
 * - Start up libraries and subsystems
 *
 * The type of this function is \ref carb::OnPluginStartupExFn and named \ref kCarbOnPluginStartupExFnName.
 * @returns `true` if the plugin started successfully; `false` otherwise.
 */
CARB_EXPORT bool carbOnPluginStartupEx();

/**
 * An optional function that a plugin author can export from their plugin to shutdown their plugin.
 *
 * The Framework will call this function when directed to unload a plugin, immediately before calling
 * \ref carbOnPluginPostShutdown and before requesting that the OS release the plugin library. This function will also
 * be called if \ref carb::Framework::unloadAllPlugins is called, but *not* if
 * \ref carb::quickReleaseFrameworkAndTerminate is called. This serves as a guide for plugin authors.
 *
 * This function is mutually exclusive with \ref carbOnPluginQuickShutdown; either this function or that one is called
 * depending on the shutdown type.
 *
 * Providing this function is completely optional.
 *
 * Generally, this function should be provided if \ref carbOnPluginStartup or \ref carbOnPluginStartupEx is provided in
 * order to clean up the work done in the startup function.
 *
 * Any interfaces declared as a dependency in \ref CARB_PLUGIN_IMPL_DEPS will still be available to this plugin when
 * this function is called.
 *
 * \warning This function should not attempt to acquire any interfaces that the plugin has not previously acquired.
 * In other words, only use interfaces in this function that the plugin has already acquired.
 *
 * During shutdown, if a circular reference exists between interfaces acquired by this plugin and those interfaces
 * (possibly indirectly) acquiring interfaces from this plugin, then it is possible that interfaces acquired by this
 * plugin may already be shut down. Using \ref carb::getCachedInterface or \ref carb::Framework::tryAcquireInterface may
 * result in an error log being issued. In this case, \ref carb::Framework::tryAcquireExistingInterface will only
 * acquire the interface if the plugin providing it is still started.
 *
 * Once this function returns successfully, the Framework considers your plugin as shut down and will typically proceed
 * to unload the library.
 *
 * Typical things this function might do:
 * - Deallocate memory and data structures for your plugin
 * - Report on leaked objects
 * - Shut down libraries and subsystems
 *
 * The type of this function is \ref carb::OnPluginShutdownFn and named \ref kCarbOnPluginShutdownFnName.
 */
CARB_EXPORT void carbOnPluginShutdown();

/**
 * An optional function that a plugin author can export from their plugin to quick-shutdown their plugin.
 *
 * The Framework will call this function for each plugin only when \ref carb::quickReleaseFrameworkAndTerminate is
 * called, in the unload order determined by the Framework. This serves as a guide for plugin authors.
 *
 * This function is mutually exclusive with \ref carbOnPluginShutdown; either this function or that one is called
 * depending on the shutdown type.
 *
 * Providing this function is completely optional.
 *
 * Since \ref carb::quickReleaseFrameworkAndTerminate will terminate the process without running static destructors or
 * closing files/connections/etc., this function should be provided to do the bare minimum of work as quickly as
 * possible to ensure data is written out and stable.
 *
 * Any interfaces declared as a dependency in \ref CARB_PLUGIN_IMPL_DEPS will still be available to this plugin when
 * this function is called.
 *
 * While this function can acquire new interfaces (unlike \ref carbOnPluginShutdown), it is generally undesired to do
 * so as that can be time-consuming and antithetical to quick shutdown.
 *
 * Typical things this function might do:
 * - Close network connections
 * - Commit database transactions
 * - Flush and close files open for write
 *
 * The type of this function is \ref carb::OnPluginQuickShutdownFn and named \ref kCarbOnPluginQuickShutdownFnName.
 */
CARB_EXPORT void carbOnPluginQuickShutdown();

/**
 * An automatically-generated function exported by Carbonite plugins.
 *
 * \note This function is automatically generated in plugins by the \ref CARB_DEFAULT_INITIALIZERS macro via the
 * \ref CARB_PLUGIN_IMPL macro. It is called by the Framework when shutting down the plugin immediately after calling
 * \ref carbOnPluginShutdown. This serves as documentation of the generated function only.
 *
 * This function shuts down various Framework-provided subsystems for the plugin: logging, profiling, asserts, and
 * localization. The following functions are called in the plugin context:
 * - \ref carb::assert::deregisterAssertForClient
 * - \ref carb::profiler::deregisterProfilerForClient
 * - \ref carb::logging::deregisterLoggingForClient
 * - \ref carb::l10n::deregisterLocalizationForClient
 *
 * The type of this function is \ref carb::OnPluginPostShutdownFn and named \ref kCarbOnPluginPostShutdownFnName.
 */
CARB_EXPORT void carbOnPluginPostShutdown();

/**
 * An automatically-generated function exported by some Carbonite plugins.
 *
 * \note This function is automatically generated in plugins by the \ref CARB_PLUGIN_IMPL_DEPS or
 * \ref CARB_PLUGIN_IMPL_NO_DEPS macros. It is called by the Framework when registering a plugin in order to determine
 * dependencies for the plugin. This serves as documentation of the generated function only.
 *
 * If neither of the above macros are used, this function is not generated for the plugin. The Framework considers this
 * function optional.
 *
 * The type of this function is \ref carb::GetPluginDepsFn and named \ref kCarbGetPluginDepsFnName.
 *
 * @param deps Assigned to static memory inside the plugin that is the array of interfaces the plugin is dependent on.
 * May be `nullptr` if there are no dependencies (in this case \p count must be `0`).
 * @param count Assigned to the number of items in the array of interfaces the plugin is dependent on.
 */
CARB_EXPORT void carbGetPluginDeps(struct carb::InterfaceDesc** deps, size_t* count);

/**
 * An optional function that a plugin author can export from their plugin to receive dependency reload notifications.
 *
 * When \ref carb::Framework::tryReloadPlugins is called, if a plugin is reloaded, any plugins which have acquired
 * interfaces from the reloading plugin will receive notifications before and after the plugin is reloaded via this
 * function. This serves as a guide for plugin authors.
 *
 * Providing this function is completely optional.
 *
 * Typical things this function might do (\p reloadState == `eBefore`):
 * - Release objects created from the interface
 * - Clear cached pointers to the interface
 *
 * Typical things this function might do (\p reloadState == `eAfter`):
 * - Update pointers to the new interface
 * - Reinstate objects
 *
 * The type of this function is \ref carb::OnReloadDependencyFn and named \ref kCarbOnReloadDependencyFnName.
 *
 * @param reloadState the callback phase
 * @param pluginInterface a pointer to the interface
 * @param desc a descriptor for the plugin
 */
CARB_EXPORT void carbOnReloadDependency(carb::PluginReloadState reloadState,
                                        void* pluginInterface,
                                        carb::PluginImplDesc desc);


//! A dummy type representing a Carbonite Interface for documentation purposes.
struct InterfaceType
{
    CARB_PLUGIN_INTERFACE("carb::InterfaceType", 1, 0);
};

/**
 * A required function that a plugin author must provide to construct an interface.
 *
 * @note This version of the function is required when using \ref CARB_PLUGIN_IMPL. When using \ref CARB_PLUGIN_IMPL_EX,
 * \ref fillInterface(carb::Version*, void*) is used instead.
 *
 * This function is called by the framework when the plugin is loaded, after calling \ref carbOnPluginRegisterEx2. The
 * plugin will fail to link if this function is not provided for all of the interfaces specified in the use of the
 * \ref CARB_PLUGIN_IMPL macro.
 *
 * @code{.cpp}
 * // Example
 * void fillInterface(carb::tasking::IFiberEvents& iface)
 * {
 *     using namespace carb::fibereventtest;
 *     iface =
 *     {
 *         notifyFiberStart,
 *         notifyFiberStop,
 *     };
 * }
 * @endcode
 *
 * @param iface This must be a reference to an interface struct exported by your plugin. The members of this struct must
 *   be set before returning. The underlying memory for `iface` is allocated and owned by the plugin and previously
 *   constructed with placement new. This type will be destructed by calling the destructor explicitly immediately
 *   before the plugin is unloaded.
 */
void fillInterface(InterfaceType& iface);

/**
 * A required function that a plugin author must provide to construct a requested version of an interface.
 *
 * @note This version of the function is required when using \ref CARB_PLUGIN_IMPL_EX. When using \ref CARB_PLUGIN_IMPL,
 * \ref fillInterface(InterfaceType&) is used instead.
 *
 * This function can be called at any time after \ref carbOnPluginRegisterEx2 is called by the framework when the plugin
 * is loaded. Generally this is called immediately for all interfaces for the version specified in their
 * \ref CARB_PLUGIN_INTERFACE declaration, and at later points when a different interface version is requested. The
 * plugin will fail to link if this function is not provided for all of the interfaces specified in the use of the
 * \ref CARB_PLUGIN_IMPL_EX macro.
 *
 * @warning This function **must** succeed (return `true`) with \p v unchanged when called with \p v equal to the
 * version specified in \ref CARB_PLUGIN_INTERFACE, otherwise the plugin will fail to register or load.
 *
 * @warning If the type `T` that you're explicitly specializing is a complex type, make sure to provide a
 *   \ref destroyInterface function to destruct the interface.
 *
 * @code{.cpp}
 * // Example
 * template <>
 * bool fillInterface<carb::stats::IStats>(carb::Version* v, void* iface)
 * {
 *     using namespace carb::stats;
 *     switch (v->major)
 *     {
 *         case IStats::getInterfaceDesc().version.major:
 *             *v = IStats::getInterfaceDesc().version;
 *             *static_cast<IStats*>(iface) = { addStat, removeStat, addValue, getValue, getCount };
 *             return true;
 *
 *         default:
 *             return false;
 *     }
 * }
 * @endcode
 *
 * @tparam T The interface type handled by this function. The generic class template for any type `T` is declared but
 *   not implemented by the \ref CARB_PLUGIN_IMPL_EX macro. This allows explicit specialization for the plugin author to
 *   provide the function for their types. For instance, if you are providing the function for `carb::IObject`, your
 *   function would be `template <> bool fillInterface<carb::IObject>(carb::Version* v, void* iface)`.
 * @param v When called, the framework provides the version requested. **The function must write the version constructed
 *   into this parameter before returning.** The version constructed does not need to be the same as (or even
 *   semantically compatible with) the version requested, but if a semantically compatible interface to the requested
 *   version is available it should be provided.
 * @param buf A memory buffer that is guaranteed to be at least as large as `sizeof(T)`, allocated and owned by the
 *   framework. For the initial allocation of an interface, this buffer is zeroed memory. If `T` is a POD type, the
 *   memory can be cast to `T*` and filled in, or placement new can be used to construct your type. If `T` is a more
 *   complex type, provide a \ref destroyInterface function to destruct the type.
 * @retval true to indicate that the type was constructed and \p v contains the version that was constructed, even if
 *   the version is different from the value that was in \p v when the function was called.
 * @retval false to indicate that the requested interface version could not be constructed.
 */
template <class T>
bool fillInterface(carb::Version* v, void* buf);

/**
 * An optional function that a plugin author can provide to destroy an interface.
 *
 * @note This function is only optionally required when using \ref CARB_PLUGIN_IMPL_EX. When using \ref CARB_PLUGIN_IMPL
 * the destruction of interfaces is handled through calling the type destructor explicitly. A plugin author need not
 * provide this function if `T` is a POD type.
 *
 * @warning This function is called immediately before the plugin is unloaded, and after \ref carbOnPluginShutdown. It
 *   **must not use** any Carbonite Framework functions. This function is not called in the event of shutdown via
 *   \ref carb::quickReleaseFrameworkAndTerminate.
 *
 * @tparam T The interface type handled by this function. The generic class template for any type `T` is declared and
 *   implemented to do nothing by the \ref CARB_PLUGIN_IMPL_EX macro, making this function optional. A plugin author may
 *   provide an explicit specialization in order to handle destruction of the interface type. For `carb::IObject` for
 *   example, this would be: `template <> void destroyInterface<carb::IObject>(carb::Version v, void* buf)`.
 * @param v The exact version that was 'returned' from \ref fillInterface(carb::Version*, void*).
 * @param buf The memory buffer that was passed to \ref fillInterface(carb::Version*, void*).
 */
template <class T>
void destroyInterface(Version v, void* buf);
#endif
