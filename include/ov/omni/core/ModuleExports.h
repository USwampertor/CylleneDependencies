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
//! @brief Helpers for defining a plugin's @ref omni::core::ModuleExports table.
#pragma once

#include "IObject.h"

#include "../../carb/Interface.h"

#include <algorithm>
#include <cstring>
#include <type_traits>

namespace omni
{

namespace log
{
class ILog;
}

namespace structuredlog
{
class IStructuredLog;

//! Registration function to install a schema with the structured logging system.
//!
//! @param[in] log  A pointer to the global singleton structured logging system to install
//!                 the schema in.
//! @returns `true` if the schema is successfully installed or was already installed.
//! @returns `false` if the schema could not be installed.  This may be caused by a lack of
//!          available memory, or too many events have been registered in the system.
using SchemaAddFn = bool (*)(IStructuredLog* log);
} // namespace structuredlog

namespace core
{

//! Unique type name for @ref omni::core::ModuleExportEntryOnModuleLoad.
constexpr const char* const kModuleExportEntryTypeOnModuleLoad = "omniOnModuleLoad";

//! Unique type name for @ref omni::core::ModuleExportEntryOnModuleStarted.
constexpr const char* const kModuleExportEntryTypeOnModuleStarted = "omniOnModuleStarted";

//! Unique type name for @ref omni::core::ModuleExportEntryOnModuleCanUnload.
constexpr const char* const kModuleExportEntryTypeOnModuleCanUnload = "omniOnModuleCanUnload";

//! Unique type name for @ref omni::core::ModuleExportEntryOnModuleUnload.
constexpr const char* const kModuleExportEntryTypeOnModuleUnload = "omniOnModuleUnload";

//! Unique type name for @ref omni::core::ModuleExportEntryITypeFactory.
constexpr const char* const kModuleExportEntryTypeITypeFactory = "omniITypeFactory";

//! Unique type name for @ref omni::core::ModuleExportEntryILog.
constexpr const char* const kModuleExportEntryTypeILog = "omniILog";

//! Unique type name for @ref omni::core::ModuleExportEntryLogChannel.
constexpr const char* const kModuleExportEntryTypeLogChannel = "omniLogChannel";

//! Unique type name for @ref omni::core::ModuleExportEntryIStructuredLog.
constexpr const char* const kModuleExportEntryTypeIStructuredLog = "omniIStructuredLog";

//! Unique type name for @ref omni::core::ModuleExportEntrySchema.
constexpr const char* const kModuleExportEntryTypeSchema = "omniSchema";

//! Unique type name for @ref omni::core::ModuleExportEntryCarbClientName.
constexpr const char* const kModuleExportEntryTypeCarbClientName = "carbClientName";

//! Unique type name for @ref omni::core::ModuleExportEntryCarbFramework.
constexpr const char* const kModuleExportEntryTypeCarbFramework = "carbFramework";

//! Unique type name for @ref omni::core::ModuleExportEntryCarbIAssert.
constexpr const char* const kModuleExportEntryTypeCarbIAssert = "carbIAssert";

//! Unique type name for @ref omni::core::ModuleExportEntryCarbILogging.
constexpr const char* const kModuleExportEntryTypeCarbILogging = "carbILogging";

//! Unique type name for @ref omni::core::ModuleExportEntryCarbIProfiler.
constexpr const char* const kModuleExportEntryTypeCarbIProfiler = "carbIProfiler";

//! Unique type name for @ref omni::core::ModuleExportEntryCarbIL10n.
constexpr const char* const kModuleExportEntryTypeCarbIL10n = "carbIL10n";

//! Unique type name for @ref omni::core::ModuleExportEntryGetModuleDependencies.
constexpr const char* const kModuleExportEntryTypeGetModuleDependencies = "omniGetModuleDependecies";

//! Per @ref omni::core::ModuleExportEntry flags.
using ModuleExportEntryFlag = uint32_t;
constexpr ModuleExportEntryFlag fModuleExportEntryFlagNone = 0; //!< No flags.

//! Fail module load if entry could not be populated.
constexpr ModuleExportEntryFlag fModuleExportEntryFlagRequired = (1 << 0);

//! Helper macro for defining an entry (i.e. @ref omni::core::ModuleExportEntry) in the export table (i.e. @ref
//! omni::core::ModuleExports).
//!
//! Implementation detail.  Not intended for use outside of omni/core/ModuleExports.h.
#define OMNI_MODULE_EXPORT_ENTRY_BEGIN(name_)                                                                          \
    struct name_                                                                                                       \
    {                                                                                                                  \
        /** <b>Unique</b> type name describing the entry. */                                                           \
        const char* type;                                                                                              \
        /** Special flags for the entry (ex: required). */                                                             \
        ModuleExportEntryFlag flags;                                                                                   \
        /** Size of the entry in bytes (including the header). */                                                      \
        uint32_t byteCount;                                                                                            \
                                                                                                                       \
        /** Constructor */                                                                                             \
        name_(const char* t, ModuleExportEntryFlag f)                                                                  \
        {                                                                                                              \
            type = t;                                                                                                  \
            flags = f;                                                                                                 \
            byteCount = sizeof(*this);                                                                                 \
        };

//! Helper macro for defining an entry in the export table.
//!
//! Implementation detail.  Not intended for use outside of omni/core/ModuleExports.h.
#define OMNI_MODULE_EXPORT_ENTRY_END(name_)                                                                            \
    }                                                                                                                  \
    ;                                                                                                                  \
    static_assert(std::is_standard_layout<name_>::value, #name_ " must be a standard layout type for ABI safety");

//! Define an entry in @ref omni::core::ModuleExports.
//!
//! Use @ref OMNI_MODULE_EXPORT_ENTRY_BEGIN and @ref OMNI_MODULE_EXPORT_ENTRY_END to define an new entry type.
//!
//! Each entry type must have a unique @p type (which is a `string`).
//!
//! @ref OMNI_MODULE_EXPORT_ENTRY_BEGIN defines the header of the entry.  Note, the use of macros vs. inheritance is to
//! ensure the resulting entry is @rstref{ABI-safe <abi-compatibility>} (e.g. meets `std::is_standard_layout`) since
//! these entries will be passed across DLL boundaries.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntry)
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntry)
static_assert(sizeof(ModuleExportEntry) == (8 + sizeof(void*)), "unexpected ModuleExportEntry size");

struct InterfaceImplementation;

//! Called to load interface implementation registration information.
//!
//! This function is called @ref omni::core::ModuleGetExportsFn.
//!
//! This function will never be called concurrently with any other function in the module.
//!
//! The module author can assume that the module's static initialization has occurred by the time this function is
//! called.
//!
//! The author should perform any implementation initialization in this function and return @ref kResultSuccess.  If
//! initialization fails, an error message should be logged (via @ref OMNI_LOG_ERROR) and an appropriate error code
//! should be returned.
//!
//! Due to potential race conditions, the module will not have access to the @ref omni::core::ITypeFactory during this
//! call but will have access to the logging and profiling systems.  If @ref omni::core::ITypeFactory access is needed
//! during initialization, lazy initialization is suggested (i.e. perform initialization during the first call to
//! `createFn`).
//!
//! The memory pointed to by @p *out must remain valid until the next call to this function.
using OnModuleLoadFn = Result(const InterfaceImplementation** out, uint32_t* outCount);

//! @ref omni::core::ModuleExports entry to register a function to advertise the interface implementations available in
//! the plugin.
//!
//! Use the helper @ref OMNI_MODULE_ON_MODULE_LOAD to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntryOnModuleLoad)
OnModuleLoadFn* onModuleLoad; //!< Module's unload function.
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntryOnModuleLoad)

//! Registers the plugin's function who is responsible for advertising the available interface implementations in the
//! plugin.
//!
//! @param exp_ The @ref omni::core::ModuleExports table in which the entry should be added.
//!
//! @param fn_ The plugin's @ref omni::core::OnModuleLoadFn who is responsible for advertising the plugin's interface
//! implementations.
#define OMNI_MODULE_ON_MODULE_LOAD(exp_, fn_) OMNI_RETURN_IF_FAILED(exp_->addOnModuleLoad(fn_))

//! This function will be called after the module is fully registered. It is called after @ref
//! omni::core::OnModuleLoadFn successfully returns.
//!
//! This function will not be called again until after @ref OnModuleUnloadFn has completed and the module has been fully
//! unloaded and reloaded.
//!
//! The owning @ref omni::core::ITypeFactory can be safely accessed in this function.
//!
//! A interface implementation's `createFn` can be called concurrently with this function.
//!
//! An interface implementation's `createFn` can be called before this function is called, as such:
//!
//!  - Move critical module initialization to @ref omni::core::OnModuleLoadFn.
//!
//!  - If some initialization cannot be performed in @ref omni::core::OnModuleLoadFn (due to @ref
//!    omni::core::ITypeFactory not being accessible), perform lazy initialization in `createFn` (in a thread-safe
//!    manner).
using OnModuleStartedFn = void();

//! @ref omni::core::ModuleExports entry to register a function to be called after the plugin has loaded.
//!
//! Use the helper @ref OMNI_MODULE_ON_MODULE_STARTED to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntryOnModuleStarted)
OnModuleStartedFn* onModuleStarted; //!< Module function to call once the module is loaded.
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntryOnModuleStarted)

//! Registers the plugin's function that will be called once the plugin is loaded.  See
//! @ref omni::core::OnModuleStartedFn for threading consideration with this function.
//!
//! @param exp_ The @ref omni::core::ModuleExports table in which the entry should be added.
//!
//! @param fn_ The plugin's @ref omni::core::OnModuleStartedFn to be called after the plugin is loaded.
#define OMNI_MODULE_ON_MODULE_STARTED(exp_, fn_) OMNI_RETURN_IF_FAILED(exp_->addOnModuleStarted(fn_))

//! Called to determine if the module can be unloaded.
//!
//! Return `true` if it is safe to unload the module. It is up to the module to determine what "safe" means, though in
//! general, it is expected that "safe" means that none of the objects created from the module's `createFn`'s are still
//! alive.
//!
//! This function will never be called while another thread is calling one of this module's `createFn` functions, during
//! @ref omni::core::OnModuleLoadFn, or during @ref omni::core::OnModuleStartedFn.
//!
//! @ref omni::core::OnModuleCanUnloadFn <u>must not</u> access the owning @ref omni::core::ITypeFactory.  @ref
//! omni::core::ITypeFactory is unable to prevent this, thus, if @ref omni::core::OnModuleCanUnloadFn does access @ref
//! omni::core::ITypeFactory (either directly or indirectly) there are no safety guards in place and undefined behavior
//! will result.
//!
//! If the module returns `true` from this function, @ref omni::core::OnModuleUnloadFn will be called.  If `false` is
//! returned, @ref omni::core::OnModuleUnloadFn will not be called.
//!
//! If this function returns `false`, it may be called again.  If `true` is returned, the module will be unloaded and
//! this function will not be called until the module is loaded again (if ever).
using OnModuleCanUnloadFn = bool();

//! @ref omni::core::ModuleExports entry to register a function to determine if the module can be unloaded.
//!
//! Use the helper @ref OMNI_MODULE_ON_MODULE_CAN_UNLOAD to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntryOnModuleCanUnload)
OnModuleCanUnloadFn* onModuleCanUnload; //!< Module function to call to see if the module can be unloaded.
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntryOnModuleCanUnload)

//! Registers the plugin's function that determines if the plugin can be unloaded. See @ref
//! omni::core::OnModuleCanUnloadFn for details.
//!
//! @param exp_ The @ref omni::core::ModuleExports table in which the entry should be added.
//!
//! @param fn_ The plugin's @ref omni::core::OnModuleCanUnloadFn to be called after the plugin is loaded.
#define OMNI_MODULE_ON_MODULE_CAN_UNLOAD(exp_, fn_) OMNI_RETURN_IF_FAILED(exp_->addOnModuleCanUnload(fn_))

//! Called when the module is about to be unloaded.
//!
//! This function is called after @ref OnModuleCanUnloadFn returns `true`.
//!
//! The module is expected to clean-up any external references to code within the module. For example, unregistering
//! asset types.
//!
//! Any registered implementations from this module will have already been unregistered by the time this function is
//! called.
//!
//! This function must never fail.
//!
//! It is safe to access the owning @ref omni::core::ITypeFactory.
//!
//! Attempting to load the module within @ref OnModuleLoadFn may result in deadlock.  It is safe for other threads to
//! attempt the load the module during @ref OnModuleUnloadFn, however, it is not safe for @ref OnModuleUnloadFn to
//! attempt to load the module.
//!
//! No other module functions will be called while this function is active.
//!
//! @ref omni::core::ITypeFactory implements the following unload pseudo-code:
//!
//! @code{.cpp}
//!  if (module->canUnload()) {
//!      factory->unregisterModuleTypes(module);
//!      module->onUnload();
//!      os->unloadDll(module);
//!  }
//! @endcode
using OnModuleUnloadFn = void();

//! @ref omni::core::ModuleExports entry to register a function to be called when the plugin is unloaded.
//!
//! Use the helper @ref OMNI_MODULE_ON_MODULE_UNLOAD to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntryOnModuleUnload)
OnModuleUnloadFn* onModuleUnload; //!< Module function to call to clean-up the module during unload.
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntryOnModuleUnload)

//! Registers the plugin's function who is responsible for cleaning up the plugin when the plugin is being unloaded.
//!
//! @param exp_ The @ref omni::core::ModuleExports table in which the entry should be added.
//!
//! @param fn_ The plugin's @ref omni::core::OnModuleUnloadFn who is responsible for cleaning up the plugin.
#define OMNI_MODULE_ON_MODULE_UNLOAD(exp_, fn_) OMNI_RETURN_IF_FAILED(exp_->addOnModuleUnload(fn_))

//! Forward declaration for omni::core::ITypeFactory.
OMNI_DECLARE_INTERFACE(ITypeFactory)

//! @ref omni::core::ModuleExports entry to access @ref omni::core::ITypeFactory.
//!
//! Use the helper @ref OMNI_MODULE_SET_EXPORTS to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntryITypeFactory)
ITypeFactory** typeFactory; //!< Pointer to the module's type factory pointer.
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntryITypeFactory)

//! @ref omni::core::ModuleExports entry to access @ref omni::log::ILog.
//!
//! Use the helper @ref OMNI_MODULE_SET_EXPORTS to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntryILog)
log::ILog** log; //!< Pointer to the module's log pointer.
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntryILog)

//! @ref omni::core::ModuleExports entry to add a logging channel.
//!
//! Use the helper @ref OMNI_MODULE_ADD_LOG_CHANNEL to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntryLogChannel)
const char* name; //!< Name of the channel.
int32_t* level; //!< Pointer to module memory where the channel's logging level is stored.
const char* description; //!< Description of the channel (for humans).
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntryLogChannel)

//! Adds a log channel to the logging system.  The channel will be removed when the module is unloaded.
//!
//! @p name_ and @p level_ must not be `nullptr`.
//!
//! The given pointers must remain valid for the lifetime of the module.
//!
//! Rather than calling this macro, use @ref OMNI_LOG_ADD_CHANNEL to both declare a channel and add it to the @ref
//! omni::core::ModuleExports table.
//!
//! @param exp_ The @ref omni::core::ModuleExports table in which the entry should be added.
//!
//! @param name_ The name of the channel.  Must not be `nullptr`.
//!
//! @param level_ Pointer to plugin memory where the logging system can store the channel's logging threshold. Shouldn't
//! be `nullptr`.
//!
//! @param description_ Description of the channel.  Useful for debugging and UIs.  Must not be `nullptr`.
#define OMNI_MODULE_ADD_LOG_CHANNEL(exp_, name_, level_, description_)                                                 \
    OMNI_RETURN_IF_FAILED(exp_->addLogChannel(name_, level_, description_))

//! @ref omni::core::ModuleExports entry to interop with @ref g_carbClientName.
//!
//! Use the helper @ref OMNI_MODULE_SET_EXPORTS to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntryCarbClientName)
const char* clientName; //!< The client name
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntryCarbClientName)

//! Requires that the owning @ref omni::core::ITypeFactory provides a Carbonite client name: @ref g_carbClientName.
//!
//! By default, the owning @ref omni::core::ITypeFactory will try to populate the module's @ref g_carbClientName, but
//! will silently fail if it cannot.  This macro tells the @ref omni::core::ITypeFactory to fail the module's load.
#define OMNI_MODULE_REQUIRE_CARB_CLIENT_NAME(exp_)                                                                     \
    OMNI_RETURN_IF_FAILED(out->requireExport(omni::core::kModuleExportEntryTypeCarbClientName))

//! @ref omni::core::ModuleExports entry to access @ref omni::structuredlog::IStructuredLog.
//!
//! Use the helper @ref OMNI_MODULE_SET_EXPORTS to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntryIStructuredLog)
omni::structuredlog::IStructuredLog** structuredLog; //!< Pointer to module structured log pointer.
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntryIStructuredLog)

//! @ref omni::core::ModuleExports entry to add a new structured logging schema to be registered.
//!
//! Use the helper @ref OMNI_MODULE_ADD_STRUCTURED_LOG_SCHEMA() to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntrySchema)
omni::structuredlog::SchemaAddFn schemaAddFn; //!< the schema registration function to run after core startup.
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntrySchema)

//! adds a new schema to be registered after core startup.
//!
//! @p fn_ must not be `nullptr`.
//!
//! This does not need to be called directly.  All the schemas included in a module will be
//! implicitly added by @ref OMNI_MODULE_SET_EXPORTS().
//!
//! @param exp_ The @ref omni::core::ModuleExports table in which the schema should be added.
//! @param fn_  The schema registration function that will be stored.  This will be executed
//!             once the core's startup has completed.  This must not be nullptr.
#define OMNI_MODULE_ADD_STRUCTURED_LOG_SCHEMA(exp_, fn_) OMNI_RETURN_IF_FAILED(exp_->addStructuredLogSchema(fn_))

} // namespace core
} // namespace omni

namespace carb
{
struct Framework;

namespace assert
{
struct IAssert;
} // namespace assert

namespace logging
{
struct ILogging;
} // namespace logging

namespace profiler
{
struct IProfiler;
} // namespace profiler

namespace l10n
{
struct IL10n;
struct LanguageTable;
struct LanguageIdentifier;
} // namespace l10n

} // namespace carb

namespace omni
{
namespace core
{

#ifndef DOXYGEN_BUILD
namespace detail
{
//! Carbonite logging callback.
using CarbLogFn = void (*)(const char* source,
                           int32_t level,
                           const char* fileName,
                           const char* functionName,
                           int lineNumber,
                           const char* fmt,
                           ...);

//! Carbonite logging threshold callback.
using CarbLogLevelFn = void(int32_t);

//! Carbonite localization callback.
using CarbLocalizeStringFn = const char*(CARB_ABI*)(const carb::l10n::LanguageTable* table,
                                                    uint64_t id,
                                                    const carb::l10n::LanguageIdentifier* language);
} // namespace detail
#endif

//! @ref omni::core::ModuleExports entry to interop with @ref carb::Framework.
//!
//! Use the helper @ref OMNI_MODULE_SET_EXPORTS to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntryCarbFramework)
carb::Framework** framework; //!< Pointer to the module's @ref g_carbFramework pointer.
carb::Version version; //!< Version of the @ref carb::Framework the module expects.
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntryCarbFramework)

//! Requires that the owning @ref omni::core::ITypeFactory provides a Carbonite @ref carb::Framework @ref
//! g_carbFramework.
//!
//! By default, the owning @ref omni::core::ITypeFactory will try to populate the module's @ref g_carbFramework, but
//! will silently fail if it cannot.  This macro tells the @ref omni::core::ITypeFactory to fail the module's load.
#define OMNI_MODULE_REQUIRE_CARB_FRAMEWORK(exp_)                                                                       \
    OMNI_RETURN_IF_FAILED(out->requireExport(omni::core::kModuleExportEntryTypeCarbFramework))

//! @ref omni::core::ModuleExports entry to interop with @ref carb::assert::IAssert.
//!
//! Use the helper @ref OMNI_MODULE_SET_EXPORTS to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntryCarbIAssert)
carb::assert::IAssert** assert; //!< Pointer to the module's @ref g_carbAssert pointer.
carb::InterfaceDesc interfaceDesc; //!< Required version of @ref carb::assert::IAssert.
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntryCarbIAssert)

//! Requires that the owning @ref omni::core::ITypeFactory provides a Carbonite @ref carb::assert::IAssert @ref
//! g_carbAssert.
//!
//! By default, the owning @ref omni::core::ITypeFactory will try to populate the module's @ref g_carbAssert, but will
//! silently fail if it cannot.  This macro tells the @ref omni::core::ITypeFactory to fail the module's load.
#define OMNI_MODULE_REQUIRE_CARB_IASSERT(exp_)                                                                         \
    OMNI_RETURN_IF_FAILED(out->requireExport(omni::core::kModuleExportEntryTypeCarbIAssert))

//! @ref omni::core::ModuleExports entry to interop with @ref carb::logging::ILogging.
//!
//! Use the helper @ref OMNI_MODULE_SET_EXPORTS to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntryCarbILogging)
carb::logging::ILogging** logging; //!< Pointer to the module's @ref g_carbLogging pointer.
detail::CarbLogFn* logFn; //!< Pointer to the module's @ref g_carbLogFn function pointer.
detail::CarbLogLevelFn* logLevelFn; //!< Pointer to a module function which can set the log level.
int32_t* logLevel; //!< Pointer to module memory where the logging threshold is stored.
carb::InterfaceDesc interfaceDesc; //!< Required version of @ref carb::logging::ILogging.
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntryCarbILogging)

//! Requires that the owning @ref omni::core::ITypeFactory provides a Carbonite @ref carb::logging::ILogging @ref
//! g_carbLogging.
//!
//! By default, the owning @ref omni::core::ITypeFactory will try to populate the module's @ref g_carbLogging, but will
//! silently fail if it cannot.  This macro tells the @ref omni::core::ITypeFactory to fail the module's load.
#define OMNI_MODULE_REQUIRE_CARB_ILOGGING(exp_)                                                                        \
    OMNI_RETURN_IF_FAILED(out->requireExport(omni::core::kModuleExportEntryTypeCarbILogging))

//! @ref omni::core::ModuleExports entry to interop with @ref carb::profiler::IProfiler.
//!
//! Use the helper @ref OMNI_MODULE_SET_EXPORTS to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntryCarbIProfiler)
carb::profiler::IProfiler** profiler; //!< Pointer to the module's @ref g_carbProfiler.
carb::InterfaceDesc interfaceDesc; //!< Required version of @ref carb::profiler::IProfiler.
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntryCarbIProfiler)

//! Requires that the owning @ref omni::core::ITypeFactory provides a Carbonite @ref carb::profiler::IProfiler @ref
//! g_carbProfiler.
//!
//! By default, the owning @ref omni::core::ITypeFactory will try to populate the module's @ref g_carbProfiler, but will
//! silently fail if it cannot.  This macro tells the @ref omni::core::ITypeFactory to fail the module's load.
#define OMNI_MODULE_REQUIRE_CARB_IPROFILER(exp_)                                                                       \
    OMNI_RETURN_IF_FAILED(out->requireExport(omni::core::kModuleExportEntryTypeCarbIProfiler))

//! @ref omni::core::ModuleExports entry to interop with @ref carb::l10n::IL10n.
//!
//! Use the helper @ref OMNI_MODULE_SET_EXPORTS to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntryCarbIL10n)
carb::l10n::IL10n** localization; //!< Pointer to the module's @ref g_carbLocalization.
detail::CarbLocalizeStringFn* localizationFn; //!< Pointer to the module's @ref g_localizationFn function pointer.
carb::InterfaceDesc interfaceDesc; //!< Required version of @ref carb::l10n::IL10n.
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntryCarbIL10n)

//! Called to get dependencies from the module.
using GetModuleDependenciesFn = Result(carb::InterfaceDesc** out, size_t* outCount);

//! @ref omni::core::ModuleExports entry to register a function to advertise the interface implementations available in
//! the plugin.
//!
//! Use the helper @ref OMNI_MODULE_GET_MODULE_DEPENDENCIES to add this entry.
OMNI_MODULE_EXPORT_ENTRY_BEGIN(ModuleExportEntryGetModuleDependencies)
GetModuleDependenciesFn* getModuleDependencies; //!< Module's dependencies information function.
OMNI_MODULE_EXPORT_ENTRY_END(ModuleExportEntryGetModuleDependencies)

//! Registers the function responsible for advertising the plugin's interface dependencies.
//!
//! @param exp_ The @ref omni::core::ModuleExports table in which the entry should be added.
//!
//! @param fn_ The plugin's @ref omni::core::GetModuleDependenciesFn responsible for advertising
//! plugin's interface dependencies.
#define OMNI_MODULE_GET_MODULE_DEPENDENCIES(exp_, fn_) OMNI_RETURN_IF_FAILED(exp_->addGetModuleDependencies(fn_))

//! Requires that the owning @ref omni::core::ITypeFactory provides a Carbonite @ref carb::l10n::IL10n @ref
//! g_carbLocalization.
//!
//! By default, the owning @ref omni::core::ITypeFactory will try to populate the module's @ref g_carbLocalization, but
//! will silently fail if it cannot.  This macro tells the @ref omni::core::ITypeFactory to fail the module's load.
#define OMNI_MODULE_REQUIRE_CARB_IL10N(exp_)                                                                           \
    OMNI_RETURN_IF_FAILED(out->requireExport(omni::core::kModuleExportEntryTypeCarbIL10n))

//! Magic number for sanity checking of @ref omni::core::ModuleExports.
constexpr uint16_t kModuleExportsMagic = 0x766e; // { 'n', 'v' }

//! Binary layout of @ref omni::core::ModuleExports.  This should be incremented if the fields in @ref
//! omni::core::ModuleExports change.
//!
//! Great care must be taken when changing this version, as it may prevent existing modules from loading without
//! recompilation.
constexpr uint16_t kModuleExportsVersion = 1;

//! Entities exported by a module for both use and population by @ref omni::core::ITypeFactory.
//!
//! Rather than a fixed data structure to communicate which functions a DLL exports, Omniverse modules use a data driven
//! approach to convey both what functionality the module brings to the table and the needs of the module in order to
//! operate correctly.
//!
//! The data members in this structure, while public, should be treated as opaque.  Hiding the data members (i.e. making
//! them private) would violate C++11's "standard layout" requirements, thus making this struct not
//! @rstref{ABI safe <abi-compatibility>}.
//!
//! Avoid calling methods of this struct directly.  Rather call the helper macros.  For example, call @ref
//! OMNI_MODULE_ON_MODULE_LOAD() rather than @ref ModuleExports::addOnModuleLoad(). Calling the former allows future
//! implementations leeway to make your awesome-futuristic interface compatible with older <i>carb.dll</i>'s.
//!
//! Unless otherwise noted, pointers provided to this object are expected to be valid for the lifetime of the module.
//!
//!@see @oni_overview for an overview of plugin loading (explicit module loading).
struct ModuleExports
{
    //! Magic number.  Used for sanity checking.  Should be kModuleExportsMagic.
    uint16_t magic;

    //! Version of this structure.  Changing this will break most modules.
    //!
    //! Version 1 of this structure defines a key/value database of module capabilities and requirements.
    //!
    //! Adding or removing a key from this database does not warrant a version bump.  Rather a version bump is required
    //! if:
    //!
    //! - Any field in this struct change its meaning.
    //! - Fields are removed from this struct (hint: never remove a field, rather, deprecate it).
    //!
    //! The "keys" in the key/value pairs are designed such that a "key" has a known value. A "key"'s meaning can never
    //! change. If a change is desired, a new key is created.
    uint16_t version;

    //! Size of this structure.  Here the size is `sizeof(ModuleExports)` + any extra space allocated at the end of this
    //! struct for @ref ModuleExportEntry's.
    uint32_t byteCount;

    //! Pointer to the first byte of the first @ref ModuleExportEntry.
    uint8_t* exportsBegin;

    //! Pointer to the byte after the end of the last @ref ModuleExportEntry.  The module is expected to update this
    //! field.
    uint8_t* exportsEnd;

    //! Returns @ref kResultSuccess if the given version is supported, an error otherwise.
    //!
    //! This method is called from the module.
    Result checkVersion(uint16_t moduleMagic, uint16_t moduleVersion)
    {
        // we can't log here, since we're to early in the module load process for logging to be available. pass back the
        // magic number and version we were expecting so omni::core::ITypeFactory can print an appropriate message if
        // the checks below fail.
        std::swap(magic, moduleMagic);
        std::swap(version, moduleVersion);

        if (magic != moduleMagic)
        {
            return kResultVersionParseError;
        }

        if (version != moduleVersion)
        {
            return kResultVersionCheckFailure;
        }

        return kResultSuccess;
    }

    //! Adds the given export entry.  Return `false` if there is not enough space.
    Result add(const ModuleExportEntry* entry)
    {
        uint32_t neededSize = uint32_t(exportsEnd - exportsBegin) + sizeof(ModuleExports) + entry->byteCount;
        if (neededSize > byteCount)
        {
            return kResultInsufficientBuffer;
        }

        std::memcpy(exportsEnd, entry, entry->byteCount);
        exportsEnd += entry->byteCount;

        return kResultSuccess;
    }

    //! Returns a pointer to the first entry of the given type.  Return `nullptr` if no such entry exists.
    ModuleExportEntry* find(const char* type)
    {
        if (!type)
        {
            return nullptr;
        }

        uint8_t* p = exportsBegin;
        while (p < exportsEnd)
        {
            auto entry = reinterpret_cast<ModuleExportEntry*>(p);
            if (0 == strcmp(type, entry->type))
            {
                return entry;
            }

            p += entry->byteCount;
        }

        return nullptr;
    }

    //! Finds the first entry of the given type and sets it as "required".  Returns an error if no such entry could be
    //! found.
    Result requireExport(const char* type)
    {
        auto entry = find(type);
        if (entry)
        {
            entry->flags |= fModuleExportEntryFlagRequired;
            return kResultSuccess;
        }

        return kResultNotFound;
    }

    //! See @ref OMNI_MODULE_ON_MODULE_LOAD.
    Result addOnModuleLoad(OnModuleLoadFn* fn, ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntryOnModuleLoad entry{ kModuleExportEntryTypeOnModuleLoad, flags };
        entry.onModuleLoad = fn;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }

    //! See @ref OMNI_MODULE_ON_MODULE_STARTED.
    Result addOnModuleStarted(OnModuleStartedFn* fn, ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntryOnModuleStarted entry{ kModuleExportEntryTypeOnModuleStarted, flags };
        entry.onModuleStarted = fn;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }

    //! See @ref OMNI_MODULE_ON_MODULE_CAN_UNLOAD.
    Result addOnModuleCanUnload(OnModuleCanUnloadFn* fn, ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntryOnModuleCanUnload entry{ kModuleExportEntryTypeOnModuleCanUnload, flags };
        entry.onModuleCanUnload = fn;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }

    //! See @ref OMNI_MODULE_ON_MODULE_UNLOAD.
    Result addOnModuleUnload(OnModuleUnloadFn* fn, ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntryOnModuleUnload entry{ kModuleExportEntryTypeOnModuleUnload, flags };
        entry.onModuleUnload = fn;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }

    //! See @ref OMNI_MODULE_SET_EXPORTS.
    Result addITypeFactory(ITypeFactory** typeFactory, ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntryITypeFactory entry{ kModuleExportEntryTypeITypeFactory, flags };
        entry.typeFactory = typeFactory;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }

    //! See @ref OMNI_MODULE_SET_EXPORTS.
    Result addILog(log::ILog** log, ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntryILog entry{ kModuleExportEntryTypeILog, flags };
        entry.log = log;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }

    //! See @ref OMNI_LOG_ADD_CHANNEL.
    Result addLogChannel(const char* channelName,
                         int32_t* level,
                         const char* description,
                         ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntryLogChannel entry{ kModuleExportEntryTypeLogChannel, flags };
        entry.name = channelName;
        entry.level = level;
        entry.description = description;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }

    //! See @ref OMNI_MODULE_SET_EXPORTS.
    Result addIStructuredLog(omni::structuredlog::IStructuredLog** strucLog,
                             ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntryIStructuredLog entry{ kModuleExportEntryTypeIStructuredLog, flags };
        entry.structuredLog = strucLog;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }

    //! See @ref OMNI_MODULE_ADD_STRUCTURED_LOG_SCHEMA().
    Result addStructuredLogSchema(omni::structuredlog::SchemaAddFn fn,
                                  ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntrySchema entry{ kModuleExportEntryTypeSchema, flags };
        entry.schemaAddFn = fn;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }

    //! See @ref OMNI_MODULE_SET_EXPORTS.
    Result addCarbClientName(const char* clientName, ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntryCarbClientName entry{ kModuleExportEntryTypeCarbClientName, flags };
        entry.clientName = clientName;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }

    //! See @ref OMNI_MODULE_SET_EXPORTS.
    Result addCarbFramework(carb::Framework** carbFramework,
                            const carb::Version& ver,
                            ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntryCarbFramework entry{ kModuleExportEntryTypeCarbFramework, flags };
        entry.framework = carbFramework;
        entry.version = ver;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }

    //! See @ref OMNI_MODULE_SET_EXPORTS.
    Result addCarbIAssert(carb::assert::IAssert** assert,
                          const carb::InterfaceDesc& interfaceDesc,
                          ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntryCarbIAssert entry{ kModuleExportEntryTypeCarbIAssert, flags };
        entry.assert = assert;
        entry.interfaceDesc = interfaceDesc;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }

    //! See @ref OMNI_MODULE_SET_EXPORTS.
    Result addCarbILogging(carb::logging::ILogging** logging,
                           detail::CarbLogFn* logFn,
                           detail::CarbLogLevelFn* logLevelFn,
                           int32_t* logLevel,
                           const carb::InterfaceDesc& interfaceDesc,
                           ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntryCarbILogging entry{ kModuleExportEntryTypeCarbILogging, flags };
        entry.logging = logging;
        entry.logFn = logFn;
        entry.logLevelFn = logLevelFn;
        entry.logLevel = logLevel;
        entry.interfaceDesc = interfaceDesc;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }

    //! See @ref OMNI_MODULE_SET_EXPORTS.
    Result addCarbIProfiler(carb::profiler::IProfiler** profiler,
                            const carb::InterfaceDesc& interfaceDesc,
                            ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntryCarbIProfiler entry{ kModuleExportEntryTypeCarbIProfiler, flags };
        entry.profiler = profiler;
        entry.interfaceDesc = interfaceDesc;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }

    //! See @ref OMNI_MODULE_SET_EXPORTS.
    Result addCarbIL10n(carb::l10n::IL10n** localization,
                        detail::CarbLocalizeStringFn* localizationFn,
                        const carb::InterfaceDesc& interfaceDesc,
                        ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntryCarbIL10n entry{ kModuleExportEntryTypeCarbIL10n, flags };
        entry.localization = localization;
        entry.localizationFn = localizationFn;
        entry.interfaceDesc = interfaceDesc;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }

    //! See @ref OMNI_MODULE_GET_MODULE_DEPENDENCIES.
    Result addGetModuleDependencies(GetModuleDependenciesFn* fn, ModuleExportEntryFlag flags = fModuleExportEntryFlagNone)
    {
        ModuleExportEntryGetModuleDependencies entry{ kModuleExportEntryTypeGetModuleDependencies, flags };
        entry.getModuleDependencies = fn;
        return add(reinterpret_cast<ModuleExportEntry*>(&entry));
    }
};

static_assert(std::is_standard_layout<ModuleExports>::value,
              "ModuleExports must be must be standard layout for ABI safety");
static_assert(sizeof(ModuleExports) == (8 + (2 * sizeof(void*))),
              "unexpected ModuleExports size. do not change ModuleExports?");

//! Type of @ref kModuleGetExportsName.  See @ref omniModuleGetExports.
using ModuleGetExportsFn = Result(ModuleExports* out);

//! Name of the module's exported function that is of type @ref omni::core::ModuleGetExportsFn.  See @ref
//! omniModuleGetExports.
constexpr const char* const kModuleGetExportsName = "omniModuleGetExports";

} // namespace core
} // namespace omni

#ifdef DOXYGEN_BUILD
//! @brief Main entry point into a module.  Returns the list of capabilities and requirements for the module.
//!
//! This is the first function called in a module by @ref omni::core::ITypeFactory.  Information is passed between the
//! module and the @ref omni::core::ITypeFactory via @ref omni::core::ModuleExports.
//!
//! @ref omni::core::ModuleExports is an @rstref{ABI-safe <abi-compatibility>} table used to store the module's
//! requirements.  For example, the module may require that the Carbonite @ref carb::Framework is present.
//!
//! This table is also used to communicate capabilities of the module.  For example, the module is able to denote which
//! logging channels it wishes to register.
//!
//! See @oni_overview for more details on how this function is used.
omni::core::Result omniModuleGetExports(omni::core::ModuleExports* out);
#endif
