// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Main header for the Omniverse core.
#pragma once

#include "../../carb/extras/Library.h"
#include "../../carb/PluginInitializers.h"

#include "Api.h"
#include "BuiltIn.h"
#include "ITypeFactory.h"
#include "../log/ILog.h"
#include "../structuredlog/IStructuredLog.h"

//! Returns the module's name (e.g. "c:/foo/omni-glfw.dll"). The pointer returned is valid for the lifetime of the
//! module.
//!
//! The returned path will be delimited by '/' on all platforms.
OMNI_API const char* omniGetModuleFilename();

//! Returns the module's directory name (e.g. "c:/foo" for "c:/foo/omni-glfw.dll"). The pointer returned is valid for
//! the lifetime of the module.
//!
//! The returned path will be delimited by '/' on all platforms.
OMNI_API const char* omniGetModuleDirectory();

#if CARB_PLATFORM_WINDOWS || defined(DOXYGEN_BUILD)
//! Defines global symbols intended to be used to statically analyze whether a given plugin
//! is a debug or release build.  In a debug build, the `g_carbIsDebugConfig` symbol will
//! be present.  In a release build, the `g_carbIsReleaseConfig` symbol will be present.
//! These symbols are not intended to be used at runtime, but rather to be able to determine
//! the build configuration without having to load up the module in a process first.
//!
//! These symbols are only present on non-Windows builds.  They can be found with a command
//! line similar to this:
//!     `nm <modulePath> | grep g_carbIsDebugConfig` or
//!     `nm <modulePath> | grep g_carbIsReleaseConfig`
//!
//! On Windows, each module's "properties" window will list "(Debug)" in the "Product Name"
//! field of the "Details" tab for debug builds.
#    define OMNI_MODULE_GLOBALS_BUILD_CONFIG_SYMBOLS()
#else
#    if CARB_DEBUG
#        define OMNI_MODULE_GLOBALS_BUILD_CONFIG_SYMBOLS() CARB_HIDDEN int g_carbIsDebugConfig = 1
#    else
#        define OMNI_MODULE_GLOBALS_BUILD_CONFIG_SYMBOLS() CARB_HIDDEN int g_carbIsReleaseConfig = 1
#    endif
#endif

//! Defines functions centered around determining the current module's disk location.
//!
//! Internal macro to reduce code duplication.  Do not directly use.
#define OMNI_MODULE_DEFINE_LOCATION_FUNCTIONS()                                                                        \
    OMNI_API const char* omniGetModuleFilename()                                                                       \
    {                                                                                                                  \
        static std::string s_omniModuleFilename = carb::extras::getLibraryFilename((void*)(omniGetModuleFilename));    \
        return s_omniModuleFilename.c_str();                                                                           \
    }                                                                                                                  \
    OMNI_API const char* omniGetModuleDirectory()                                                                      \
    {                                                                                                                  \
        static std::string s_omniModuleDirectory = carb::extras::getLibraryDirectory((void*)(omniGetModuleDirectory)); \
        return s_omniModuleDirectory.c_str();                                                                          \
    }

//! Defines default implementations of global omni functions for a module.
//!
//! Internal macro to reduce code duplication.  Do not directly use.  Use @ref OMNI_MODULE_GLOBALS().
#define OMNI_MODULE_DEFINE_OMNI_FUNCTIONS()                                                                            \
    namespace                                                                                                          \
    {                                                                                                                  \
    ::omni::core::ITypeFactory* s_omniTypeFactory = nullptr;                                                           \
    ::omni::log::ILog* s_omniLog = nullptr;                                                                            \
    ::omni::structuredlog::IStructuredLog* s_omniStructuredLog = nullptr;                                              \
    }                                                                                                                  \
    OMNI_MODULE_DEFINE_LOCATION_FUNCTIONS()                                                                            \
    OMNI_MODULE_GLOBALS_BUILD_CONFIG_SYMBOLS();                                                                        \
    OMNI_API void* omniGetBuiltInWithoutAcquire(OmniBuiltIn type)                                                      \
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

//! Type of the `omniCarbStartup` function that is generated by \ref OMNI_MODULE_DEFINE_CARB_FUNCTIONS
using OmniCarbStartupFn = const char* (*)(carb::Framework*);

//! Type of the `omniCarbShutdown` function that is generated by \ref OMNI_MODULE_DEFINE_CARB_FUNCTIONS
using OmniCarbShutdownFn = void (*)();

//! Defines default implementations of global Carbonite functions for an Omni module.
//!
//! Internal macro to reduce code duplication.  Do not directly use.  Use @ref OMNI_MODULE_GLOBALS().
#define OMNI_MODULE_DEFINE_CARB_FUNCTIONS()                                                                            \
    OMNI_EXPORT const char* omniCarbStartup(carb::Framework* framework)                                                \
    {                                                                                                                  \
        g_carbFramework = framework;                                                                                   \
        carb::pluginInitialize();                                                                                      \
        return g_carbClientName;                                                                                       \
    }                                                                                                                  \
    OMNI_EXPORT void omniCarbShutdown()                                                                                \
    {                                                                                                                  \
        carb::pluginDeinitialize();                                                                                    \
    }

//! Implementation detail. Do not directly use.  Use @ref OMNI_GLOBALS_ADD_DEFAULT_CHANNEL.
#define OMNI_GLOBALS_ADD_DEFAULT_CHANNEL_1(chan_, name_, desc_) OMNI_LOG_ADD_CHANNEL(chan_, name_, desc_)

//! Adds the @p name_ as the default logging channel.
//!
//! It's unlikely user code will have to use this, as code like @ref CARB_GLOBALS_EX call this macro on behalf of most
//! clients.
#define OMNI_GLOBALS_ADD_DEFAULT_CHANNEL(name_, desc_)                                                                 \
    OMNI_GLOBALS_ADD_DEFAULT_CHANNEL_1(OMNI_LOG_DEFAULT_CHANNEL, name_, desc_)

//! Helper macro to declare globals needed by modules (i.e. plugins).
//!
//! Use with @ref OMNI_MODULE_SET_EXPORTS_WITHOUT_CARB().
//!
//! This macro is like @ref OMNI_MODULE_GLOBALS() but disables the interop between Carbonite interfaces and ONI.  This
//! macro is useful if you never plan on accessing Carbonite interfaces within your module.
#define OMNI_MODULE_GLOBALS_WITHOUT_CARB(name_, desc_)                                                                 \
    OMNI_MODULE_DEFINE_OMNI_FUNCTIONS()                                                                                \
    OMNI_GLOBALS_ADD_DEFAULT_CHANNEL(name_, desc_)

//! Helper macro to declare globals needed by modules (i.e. plugins).
//!
//! Use with @ref OMNI_MODULE_SET_EXPORTS().
#define OMNI_MODULE_GLOBALS(name_, desc_)                                                                              \
    OMNI_MODULE_DEFINE_OMNI_FUNCTIONS()                                                                                \
    OMNI_MODULE_DEFINE_CARB_FUNCTIONS()                                                                                \
    CARB_GLOBALS_EX(name_, desc_)

//! Helper macro to set known export fields in @ref omniModuleGetExports().
//!
//! Use this macro in conjunction with @ref OMNI_MODULE_GLOBALS_WITHOUT_CARB().
#define OMNI_MODULE_SET_EXPORTS_WITHOUT_CARB(out_)                                                                     \
    do                                                                                                                 \
    {                                                                                                                  \
        OMNI_RETURN_IF_FAILED(out_->checkVersion(omni::core::kModuleExportsMagic, omni::core::kModuleExportsVersion)); \
        OMNI_RETURN_IF_FAILED(out_->addITypeFactory(&s_omniTypeFactory));                                              \
        OMNI_RETURN_IF_FAILED(out_->addILog(&s_omniLog));                                                              \
        OMNI_RETURN_IF_FAILED(out_->addIStructuredLog(&s_omniStructuredLog));                                          \
        for (auto channel = omni::log::getModuleLogChannels(); channel; channel = channel->next)                       \
        {                                                                                                              \
            OMNI_MODULE_ADD_LOG_CHANNEL(out_, channel->name, &channel->level, channel->description);                   \
        }                                                                                                              \
        for (auto& schema : omni::structuredlog::getModuleSchemas())                                                   \
        {                                                                                                              \
            OMNI_MODULE_ADD_STRUCTURED_LOG_SCHEMA(out_, schema);                                                       \
        }                                                                                                              \
    } while (0)

//! Helper macro to set known export fields in @ref omniModuleGetExports().
//!
//! Use this macro in conjunction with @ref OMNI_MODULE_GLOBALS().
#define OMNI_MODULE_SET_EXPORTS(out_)                                                                                  \
    OMNI_MODULE_SET_EXPORTS_WITHOUT_CARB(out_);                                                                        \
    OMNI_MODULE_SET_CARB_EXPORTS(out_)

//! Helper macro to set known export fields in @ref omniModuleGetExports() related to Carbonite.
//!
//! Internal macro to reduce code duplication.  Do not directly use.  Use @ref OMNI_MODULE_SET_EXPORTS().
#define OMNI_MODULE_SET_CARB_EXPORTS(out_)                                                                             \
    OMNI_RETURN_IF_FAILED(out_->addCarbClientName(g_carbClientName));                                                  \
    OMNI_RETURN_IF_FAILED(out_->addCarbFramework(&g_carbFramework, carb::kFrameworkVersion));                          \
    OMNI_RETURN_IF_FAILED(out_->addCarbIAssert(&g_carbAssert, carb::assert::IAssert::getInterfaceDesc()));             \
    OMNI_RETURN_IF_FAILED(out_->addCarbILogging(&g_carbLogging, &g_carbLogFn,                                          \
                                                [](int32_t logLevel) { g_carbLogLevel = logLevel; }, &g_carbLogLevel,  \
                                                carb::logging::ILogging::getInterfaceDesc()));                         \
    OMNI_RETURN_IF_FAILED(out_->addCarbIProfiler(&g_carbProfiler, carb::profiler::IProfiler::getInterfaceDesc()));     \
    OMNI_RETURN_IF_FAILED(                                                                                             \
        out_->addCarbIL10n(&g_carbLocalization, &g_localizationFn, carb::l10n::IL10n::getInterfaceDesc()))

//! Helper macro to declare globals needed my the omni library when using the omni library in an application.
//!
//! \note Either this macro, or \ref CARB_GLOBALS, or \ref CARB_GLOBALS_EX must be specified in the global namespace
//! in exactly one compilation unit for a Carbonite Application.
//!
//! See @ref OMNI_CORE_INIT().
//! @param clientName The name of the client application. Must be unique with respect to any plugins loaded. Also is the
//! name of the default log channel.
//! @param clientDescription A description to use for the default log channel.
#define OMNI_APP_GLOBALS(clientName, clientDescription) CARB_GLOBALS_EX(clientName, clientDescription)

//! Helper macro to startup the Carbonite framework and Omni type factory.
//!
//! See @ref OMNI_CORE_INIT().
#define OMNI_CORE_START(args_)                                                                                         \
    omniCoreStart(args_);                                                                                              \
    omni::log::addModulesChannels();                                                                                   \
    omni::structuredlog::addModulesSchemas()

//! Helper macro to shutdown the Carbonite framework and Omni type factory.
//!
//! See @ref OMNI_CORE_INIT().
#define OMNI_CORE_STOP()                                                                                               \
    omni::log::removeModulesChannels();                                                                                \
    omniCoreStop()

//! Helper macro to shutdown the Carbonite framework and Omni type factory, for script bindings.
//!
//! See @ref OMNI_CORE_INIT().
#define OMNI_CORE_STOP_FOR_BINDINGS()                                                                                  \
    omni::log::removeModulesChannels();                                                                                \
    omniCoreStopForBindings()

//! Version of @ref OmniCoreStartArgs struct passed to @ref omniCoreStart.
//!
//! The version should be incremented only when removing/rearranging fields in @ref OmniCoreStartArgs. Adding fields
//! that default to `nullptr` or `0` (from the reserved space) is allowed without incrementing the version.
constexpr uint16_t kOmniCoreStartArgsVersion = 1;

//! Base type for the Omni core startup flags.
using OmniCoreStartFlags = uint32_t;

//! Flag to indicate that ILog usage should be disabled on startup instead of creating the
//! internal version or expecting that the caller to provide an implementation of the ILog
//! interface that has already been instantiated.
constexpr OmniCoreStartFlags fStartFlagDisableILog = 0x00000001;

//! Flag to indicate that IStructuredLog usage should be disabled on startup instead of
//! creating the internal version or expecting that the caller to provide an implementation
//! of the IStructuredLog interface that has already been instantiated.
constexpr OmniCoreStartFlags fStartFlagDisableIStructuredLog = 0x00000002;

//! Arguments passed to omniCoreStart().
class OmniCoreStartArgs
{
public:
    //! Version of this structure.  The version should be incremented only when removing/rearranging fields.  Adding
    //! fields (from the reserved space) is allowed without increment the version.
    //!
    //! This fields value should always be set to @ref kOmniCoreStartArgsVersion.
    uint16_t version;

    //! Size of this structure.
    uint16_t byteCount;

    //! flags to control the behavior of the Omni core startup.
    OmniCoreStartFlags flags;

    //! The type factory that will be returned by omniGetTypeFactoryWithoutAcquire().
    //!
    //! omniCoreStart will call acquire() on the given type factory.
    //!
    //! If the given parameter is nullptr, omniCreateTypeFactory() will be called.
    omni::core::ITypeFactory* typeFactory;

    //! The log that will be returned by omniGetLogWithoutAcquire().
    //!
    //! omniCoreStart will call acquire() on the given log.
    //!
    //! If the given parameter is nullptr, omniCreateLog() will be called.
    omni::log::ILog* log;

    //! The structured log object that will be returned by omniGetStructuredLogWithoutAcquire().
    //!
    //! omniCoreStart will call acquire on the given structured log object.
    //!
    //! If the given parameter is nullptr, the default implementation object will be used instead.
    omni::structuredlog::IStructuredLog* structuredLog;

    //! When adding fields, decrement this reserved space.  Be mindful of alignment (explicitly add padding fields if
    //! needed).
    void* reserved[12];

    //! Default constructor.
    OmniCoreStartArgs()
    {
        std::memset(this, 0, sizeof(*this));
        version = kOmniCoreStartArgsVersion;
        byteCount = sizeof(*this);
    }

    //! Constructor which accepts default implementations for the core.
    OmniCoreStartArgs(omni::core::ITypeFactory* factory_,
                      omni::log::ILog* log_ = nullptr,
                      omni::structuredlog::IStructuredLog* strucLog_ = nullptr)
        : OmniCoreStartArgs()
    {
        typeFactory = factory_;
        log = log_;
        structuredLog = strucLog_;
    }
};

static_assert(std::is_standard_layout<OmniCoreStartArgs>::value, "OmniCoreStartArgs must be standard layout");
static_assert((8 + 15 * sizeof(void*)) == sizeof(OmniCoreStartArgs), "OmniCoreStartArgs has an unexpected size");

//! Initializes the omni core library's internal data structures.
//!
//! nullptr is accepted, in which case the default behavior described in OmniCoreStartArgs is applied.
//!
//! See @ref OMNI_CORE_INIT().
OMNI_API void omniCoreStart(const OmniCoreStartArgs* args);

//! Tears down the omni core library's internal data structures.
//!
//! See @ref OMNI_CORE_INIT().
OMNI_API void omniCoreStop();

//! Tears down the omni core library's internal data structures for script bindings.
//!
//! See @ref OMNI_CORE_INIT().
OMNI_API void omniCoreStopForBindings();

//! Releases the structured log pointer.
//!
//! This should be called before unloading plugins so that the structured log plugin properly shuts down.
OMNI_API void omniReleaseStructuredLog();

#include "../../carb/ClientUtils.h"
