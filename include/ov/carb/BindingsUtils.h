// Copyright (c) 2018-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Utilities for script bindings
#pragma once

#include "ClientUtils.h"
#include "Defines.h"
#include "Format.h"
#include "Framework.h"
#include "InterfaceUtils.h"
#include "ObjectUtils.h"
#include "assert/AssertUtils.h"
#include "logging/Log.h"
#include "profiler/Profile.h"

#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>

namespace carb
{

/**
 * Wraps an interface function into a `std::function<>`.
 *
 * @tparam InterfaceType The Carbonite interface type (i.e. `logging::ILogging`); can be inferred.
 * @tparam ReturnType The return type of @p p; can be inferred.
 * @tparam Args Arguments of @p p; can be inferred.
 * @param p The interface function to wrap.
 * @returns A `std::function<ReturnType(InterfaceType&, Args...)>` wrapper around @p p.
 */
template <typename InterfaceType, typename ReturnType, typename... Args>
auto wrapInterfaceFunction(ReturnType (*InterfaceType::*p)(Args...))
    -> std::function<ReturnType(InterfaceType&, Args...)>
{
    return [p](InterfaceType& c, Args... args) { return (c.*p)(args...); };
}

/**
 * Wraps an interface function into a `std::function<>`. This version captures the interface so that it does not need to
 * be passed to every invocation.
 *
 * @tparam InterfaceType The Carbonite interface type (i.e. `logging::ILogging`); can be inferred.
 * @tparam ReturnType The return type of @p p; can be inferred.
 * @tparam Args Arguments of @p p; can be inferred.
 * @param c The Carbonite interface to capture as part of the wrapper function.
 * @param p The interface function to wrap.
 * @returns A `std::function<ReturnType(Args...)>` wrapper around @p p.
 */
template <typename InterfaceType, typename ReturnType, typename... Args>
auto wrapInterfaceFunction(const InterfaceType* c, ReturnType (*InterfaceType::*p)(Args...))
    -> std::function<ReturnType(Args...)>
{
    return [c, p](Args... args) { return (c->*p)(args...); };
}

/**
 * A helper function for \ref Framework::tryAcquireInterface() that attempts to load plugins if not found.
 *
 * @tparam InterfaceType The interface to acquire (i.e. `assets::IAssets`). Must be specified and cannot be inferred.
 * @param pluginName An optional specific plugin to acquire the interface from. If `nullptr`, the default plugin for the
 * given InterfaceType is used.
 * @returns A pointer to the interface.
 * @throws std::runtime_error if the interface cannot be acquired and exceptions are enabled, otherwise this error
 * condition results in a \ref CARB_FATAL_UNLESS() assertion.
 */
template <typename InterfaceType>
InterfaceType* acquireInterfaceForBindings(const char* pluginName = nullptr)
{
    carb::Framework* framework = carb::getFramework();
    InterfaceType* iface = framework->tryAcquireInterface<InterfaceType>(pluginName);
    if (!iface)
    {
        // Try load plugins with default desc (all of them)
        carb::PluginLoadingDesc desc = carb::PluginLoadingDesc::getDefault();
        framework->loadPlugins(desc);
        iface = framework->tryAcquireInterface<InterfaceType>(pluginName);
        if (!iface)
        {
            // somehow this header gets picked up by code compiled by -fno-exceptions
#if !CARB_EXCEPTIONS_ENABLED
            OMNI_FATAL_UNLESS(iface, "Failed to acquire interface: '%s' (pluginName: '%s')",
                              InterfaceType::getInterfaceDesc().name, pluginName ? pluginName : "nullptr");
#else
            throw std::runtime_error(fmt::format("Failed to acquire interface: {} (pluginName: {})",
                                                 InterfaceType::getInterfaceDesc().name,
                                                 pluginName ? pluginName : "nullptr"));
#endif
        }
    }
    return iface;
}

/**
 * A helper function for \ref carb::getCachedInterface() that throws on error.
 *
 * @tparam InterfaceType The interface to acquire (i.e. `assets::IAssets`). Must be specified and cannot be inferred.
 * @returns A pointer to the interface.
 * @throws std::runtime_error if the interface cannot be acquired and exceptions are enabled, otherwise this error
 * condition results in a \ref CARB_FATAL_UNLESS() assertion.
 */
template <typename InterfaceType>
InterfaceType* getCachedInterfaceForBindings()
{
    InterfaceType* iface = carb::getCachedInterface<InterfaceType>();
    if (CARB_UNLIKELY(!iface))
    {
        // somehow this header gets picked up by code compiled by -fno-exceptions
#if !CARB_EXCEPTIONS_ENABLED
        OMNI_FATAL_UNLESS(iface, "Failed to acquire cached interface: '%s'", InterfaceType::getInterfaceDesc().name);
#else
        throw std::runtime_error(
            fmt::format("Failed to acquire cached interface: {}", InterfaceType::getInterfaceDesc().name));
#endif
    }
    return iface;
}

/**
 * Helper for \ref Framework::tryAcquireInterfaceFromLibrary() that throws on error.
 *
 * @tparam InterfaceType The interface to acquire (i.e. `assets::IAssets`). Must be specified and cannot be inferred.
 * @param libraryPath The library path to acquire the interface from. Must be specified. May be relative or absolute.
 * @returns A pointer to the interface.
 * @throws std::runtime_error if the interface cannot be acquired and exceptions are enabled, otherwise this error
 * condition results in a \ref CARB_FATAL_UNLESS() assertion.
 */
template <typename InterfaceType>
InterfaceType* acquireInterfaceFromLibraryForBindings(const char* libraryPath)
{
    carb::Framework* framework = carb::getFramework();
    InterfaceType* iface = framework->tryAcquireInterfaceFromLibrary<InterfaceType>(libraryPath);
    if (!iface)
    {
        // somehow this header gets picked up by code compiled by -fno-exceptions
#if !CARB_EXCEPTIONS_ENABLED
        OMNI_FATAL_UNLESS(
            "Failed to acquire interface: '%s' from: '%s')", InterfaceType::getInterfaceDesc().name, libraryPath);
#else
        throw std::runtime_error(fmt::format(
            "Failed to acquire interface: {} from: {})", InterfaceType::getInterfaceDesc().name, libraryPath));
#endif
    }
    return iface;
}

/**
 * Acquires the Carbonite Framework for a script binding.
 *
 * @note This is automatically called by \ref FrameworkInitializerForBindings::FrameworkInitializerForBindings() from
 * \ref CARB_BINDINGS().
 *
 * @param scriptLanguage The script language that this binding works with (i.e. "python"). This binding is registered
 * as via `carb::getFramework()->registerScriptBinding(BindingType::Binding, g_carbClientName, scriptLanguage)`.
 * @returns A pointer to the Carbonite \ref Framework, or `nullptr` on error (i.e. version mismatch).
 * @see Framework::registerScriptBinding()
 */
inline Framework* acquireFrameworkForBindings(const char* scriptLanguage)
{
    // Acquire framework and set into global variable
    // Is framework was previously invalid, we are the first who calling it and it will be created during acquire.
    // Register builtin plugin in that case
    const bool firstStart = !isFrameworkValid();
    Framework* f = acquireFramework(g_carbClientName);
    if (!f)
        return nullptr;
    g_carbFramework = f;

    // Register as binding for the given script language
    f->registerScriptBinding(BindingType::Binding, g_carbClientName, scriptLanguage);

    // Starting up logging
    if (firstStart)
        registerBuiltinLogging(f);
    logging::registerLoggingForClient();

    // Starting up filesystem and profiling
    if (firstStart)
    {
        registerBuiltinFileSystem(f);
        registerBuiltinAssert(f);
        registerBuiltinThreadUtil(f);
    }
    profiler::registerProfilerForClient();
    assert::registerAssertForClient();
    l10n::registerLocalizationForClient();
    return f;
}

/**
 * Releases the Carbonite Framework for a script binding.
 *
 * @note This is automatically called by the \ref FrameworkInitializerForBindings destructor from \ref CARB_BINDINGS().
 */
inline void releaseFrameworkForBindings()
{
    if (isFrameworkValid())
    {
        profiler::deregisterProfilerForClient();
        logging::deregisterLoggingForClient();
        assert::deregisterAssertForClient();
        l10n::deregisterLocalizationForClient();

        // Leave g_carbFramework intact here since the framework itself remains valid; we are just signaling our end of
        // using it. There may be some static destructors (i.e. CachedInterface) that still need to use it.
    }
    else
    {
        // The framework became invalid while we were loaded.
        g_carbFramework = nullptr;
    }
}

/**
 * A helper class used by \ref CARB_BINDINGS() to acquire and release the \ref Framework for a binding.
 */
class FrameworkInitializerForBindings
{
public:
    /**
     * Acquires the Carbonite \ref Framework for this binding module.
     *
     * @note Calls \ref acquireFrameworkForBindings() and \ref OMNI_CORE_START() if the ONI core is not already started.
     * @param scriptLanguage The script language that this binding works with.
     */
    FrameworkInitializerForBindings(const char* scriptLanguage = "python")
    {
        acquireFrameworkForBindings(scriptLanguage);

        m_thisModuleStartedOmniCore = !omniGetTypeFactoryWithoutAcquire();
        if (m_thisModuleStartedOmniCore)
        {
            // at this point, the core should already be started by the omniverse host executable (i.e. app).  however,
            // if we're in the python native interpreter, it will not automatically startup the core.  here we account
            // for this situation by checking if the core is started, and if not, start it.
            //
            // OMNI_CORE_START internally reference counts the start/stop calls, so one would think we could always make
            // this call (with a corresponding call to OMNI_CORE_STOP in the destructor).
            //
            // however, the Python interpreter doesn't like unloading .pyd files, meaning our destructor will not be
            // called.
            //
            // this shouldn't be an issue, unless the host expects to be able to load, unload, and then reload the core.
            // the result here would be the internal reference count would get confused, causing the core to never be
            // unloaded.
            //
            // we don't expect apps to reload the core, but our unit tests do.  so, here we only let python increment
            // the ref count if we think its the first entity to start the core (i.e. running in the interpreter).
            OMNI_CORE_START(nullptr);
        }

        omni::structuredlog::addModulesSchemas();
    }

    /**
     * Releases the Carbonite \ref Framework for this binding module.
     * @note Calls \ref OMNI_CORE_STOP() if the constructor initialized the ONI core, and
     * \ref releaseFrameworkForBindings().
     */
    ~FrameworkInitializerForBindings()
    {
        if (m_thisModuleStartedOmniCore)
        {
            OMNI_CORE_STOP_FOR_BINDINGS();
            m_thisModuleStartedOmniCore = false;
        }
        releaseFrameworkForBindings();
    }

    //! A boolean indicating whether the constructor called \ref OMNI_CORE_START().
    bool m_thisModuleStartedOmniCore;
};

/**
 * A helper function for combining two hash values.
 *
 * Effectively:
 * ```cpp
 * std::size_t res = 0;
 * using std::hash;
 * res = carb::hashCombine(res, hash<T1>{}(t1));
 * res = carb::hashCombine(res, hash<T2>{}(t2));
 * return res;
 * ```
 * @tparam T1 A type to hash.
 * @tparam T2 A type to hash.
 * @param t1 A value to hash.
 * @param t2 A value to hash.
 * @returns A hash combined from @p t1 and @p t2.
 * @see hashCombine()
 */
template <class T1, class T2>
inline size_t hashPair(T1 t1, T2 t2)
{
    std::size_t res = 0;
    using std::hash;
    res = carb::hashCombine(res, hash<T1>{}(t1));
    res = carb::hashCombine(res, hash<T2>{}(t2));
    return res;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
/**
 * Helper class to store and manage lifetime of `std::function<>` for script bindings.
 *
 * It allocates std::function copy on a heap, prolonging its lifetime. That allows passing std::function as user data
 * into interface subscription functions. You need to associate it with some key (provide key type with a template
 * KeyT), usually it is some kind of Subscription Id. When unsubscribing call ScriptCallbackRegistry::removeAndDestroy
 * with corresponding key. The usage:
 *
 * ```cpp
 * std::function<int(float, char)> myFunc;
 * static ScriptCallbackRegistry<size_t, int, float, char> s_registry;
 * std::function<int(float, char)>* myFuncCopy = s_registry.create(myFunc);
 * // myFuncCopy can now passed into C API as user data
 * s_registry.add(id, myFuncCopy);
 * // ...
 * s_registry.removeAndDestroy(id);
 * ```
 */
template <class KeyT, typename ReturnT, typename... Args>
class ScriptCallbackRegistry
{
public:
    using FuncT = std::function<ReturnT(Args...)>;

    static FuncT* create(const FuncT& f)
    {
        return new FuncT(f);
    }

    static void destroy(FuncT* f)
    {
        delete f;
    }

    void add(const KeyT& key, FuncT* ptr)
    {
        if (!m_map.insert({ key, ptr }).second)
        {
            CARB_LOG_ERROR("Scripting callback with that key already exists.");
        }
    }

    bool tryRemoveAndDestroy(const KeyT& key)
    {
        auto it = m_map.find(key);
        if (it != m_map.end())
        {
            destroy(it->second);
            m_map.erase(it);
            return true;
        }
        return false;
    }

    void removeAndDestroy(const KeyT& key)
    {
        if (!tryRemoveAndDestroy(key))
        {
            CARB_LOG_ERROR("Removing unknown scripting callback.");
        }
    }

private:
    std::unordered_map<KeyT, FuncT*> m_map;
};


template <typename ClassT, typename ObjectT, typename... Args>
auto wrapInStealObject(ObjectT* (ClassT::*f)(Args...))
{
    return [f](ClassT* c, Args... args) { return carb::stealObject<ObjectT>((c->*f)(args...)); };
}
#endif

} // namespace carb

/**
 * Declare a compilation unit as script language bindings.
 *
 * @param clientName The string to pass to CARB_GLOBALS which will be used as `g_carbClientName` for the module.
 * @param ... Arguments passed to \ref carb::FrameworkInitializerForBindings::FrameworkInitializerForBindings(),
 * typically the script language.
 */
#define CARB_BINDINGS(clientName, ...)                                                                                 \
    CARB_GLOBALS(clientName)                                                                                           \
    carb::FrameworkInitializerForBindings g_carbFrameworkInitializerForBindings{ __VA_ARGS__ };

/**
 * Declare a compilation unit as script language bindings.
 *
 * @param clientName_ The string to pass to CARB_GLOBALS_EX which will be used as `g_carbClientName` for the module.
 * @param desc_ The description passed to `omni::LogChannel` for the default log channel.
 * @param ... Arguments passed to \ref carb::FrameworkInitializerForBindings::FrameworkInitializerForBindings(),
 * typically the script language.
 */
#define CARB_BINDINGS_EX(clientName_, desc_, ...)                                                                      \
    CARB_GLOBALS_EX(clientName_, desc_)                                                                                \
    carb::FrameworkInitializerForBindings g_carbFrameworkInitializerForBindings{ __VA_ARGS__ };
