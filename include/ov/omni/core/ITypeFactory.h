// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief Provides the ITypeFactory interface declaration.
#pragma once

#include "../../carb/Defines.h"
#include "IObject.h"
#include "ModuleExports.h"
#include "Omni.h"
#include "Result.h"

#include <cstdint>
#include <vector>

namespace omni
{
namespace core
{

//! Forward declares that the ITypeFactory interface is present.
OMNI_DECLARE_INTERFACE(ITypeFactory);


//! Function called by ITypeFactory to instantiate an implementation.
//!
//! This "creation" function is one of the core principles behind @rstref{ABI safety <abi-compatibility>}.  By calling
//! this simple function, we're able to instantiate a complex implementation of an Omniverse interface. All of the
//! details to instantiate this implementation are hidden behind this function.  Since this function returns a pointer
//! to an interface (IObject), the caller is not exposed to any of the implementation details needed to instantiate the
//! interface.
using InterfaceImplementationCreateFn = IObject*();

//! Describes a mapping from a chunk of code (i.e. implementation) to one or more interfaces.
//!
//! Implementation are concrete classes that implement one or more interfaces.
//!
//! This data structure is essential to the Omniverse type system, as it maps type names (i.e. strings) to chunks of
//! code that can instantiate those types.  With this, the Omniverse type system is able to map interface type names to
//! implementations and implementation type names to specific implementations.
struct OMNI_ATTR("no_py") InterfaceImplementation
{
    //! Name of the implementation.  This must not be the name of an interface.
    const char* name;

    //! Function that instantiates the implementation.
    //!
    //! This function can be called concurrently on multiple threads
    InterfaceImplementationCreateFn* createFn;

    //! Implementations have versions.  By default, ITypeFactory will pick the implementation with the highest version
    //! number.  This behavior can be overridden (see ITypeFactory).
    //!
    //! This version number is not an "interface" version number. Interfaces are not versioned.  Implementations,
    //! however, can be versioned.  An implementation's version number is used by ITypeFactory to pick the best
    //! implementation when instantiating an interface.
    uint32_t version;

    //! List of interfaces, that when requested to be instantiated by ITypeFactory (e.g. omni::core::createType()),
    //! should instantiate this implementation. Not all implemented interfaces should be listed here, only those
    //! interfaces you wish to instantiate via omni::core::createType().
    //!
    //! Said differently, this is a list of interfaces, that when invoked with omni::core::createType(), should
    //! instantiate this implementation.
    //!
    //! Which interfaces should be listed here is subtle topic.  See @rstref{omniverse-native-interfaces} for
    //! more details.
    const char** interfacesImplemented;

    //! Number of interfaces implemented (size of interfacesImplemented).  Pro-tip: Use
    //! CARB_COUNTOF32(interfacesImplemented).
    uint32_t interfacesImplementedCount;
};

//! Base type for the flags used when registering plugins or implementations with the type
//! factory.  These are used to modify how the plugin or implementation is registered.  No
//! flags are currently defined.  These flags will all have the prefix `fTypeFactoryFlag`.
using TypeFactoryLoadFlags OMNI_ATTR("flag, prefix=fTypeFactoryFlag") = uint32_t;

//! Flag to indicate that no special change in behavior should be used when registering
//! a plugin or implementation.
constexpr TypeFactoryLoadFlags fTypeFactoryFlagNone = 0x0;

//! A mapping from type id's to implementations.
//!
//! This object maps type id's to concrete implementations.  The type id's can represent interface ids or implementation
//! ids.
//!
//! Register types with registerInterfaceImplementationsFromModule() and registerInterfaceImplementations().
//!
//! Instantiate types with omni::core::createType(). This is the primary way Omniverse applications are able to
//! instantiate concrete implementations of @rstref{ABI-safe <abi-compatibility>} interfaces.  See
//! omni::core::createType() for a helpful wrapper around omni::core::ITypeFactory::createType().
//!
//! In practice, there will be a single ITypeFactory active in the process space (accessible via
//! omniGetTypeFactoryWithoutAcquire()).  However, @ref omni::core::ITypeFactory is not inherently a singleton, and as
//! such multiple instantiations of the interface may exists.  This can be used to create private type trees.
//!
//! Unless otherwise noted, all methods in this interface are thread safe.
class ITypeFactory_abi : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.core.ITypeFactory")>
{
protected:
    //! Instantiates a concrete type.
    //!
    //! The given type id can be an interface or implementation id.
    //!
    //! If the id is an interface id, the following rules are followed:
    //!
    //! - If the application specified a default implementation, that implementation will be instantiated.
    //!
    //! - Otherwise, the first registered implementation of the interface is instantiated.  If multiple versions of the
    //!   implementation exist, the highest version is picked.
    //!
    //! - implVersion must be 0 since interfaces are not versioned (only implementations are versioned). If implVersion
    //!   is not 0, nullptr is returned.
    //!
    //! - If a default module name was provided by the app, the rules above will only be applied to implementations from
    //!   the specified default module.
    //!
    //! If the id is an implementation id, the followings rules apply:
    //!
    //! - If version is 0, the highest version of the implementation is returned.
    //!
    //! - If version is not 0, the returned object is the specified version of the implementation.  If such a version
    //!   does not exists, nullptr is returned. If multiple implementations exists with the same version, the
    //!   implementation registered first is instantiated.
    //!
    //! In both cases above, if moduleName given, the rules above are followed by only looking at implementations from
    //! the specified module.  If no match is found, nullptr is returned.
    //!
    //! If moduleName has not been loaded, it will be loaded and its implementations registered.
    //!
    //! If moduleName is nullptr, the rules above are applied across all loaded modules.
    //!
    //! This method is thread safe.
    virtual IObject* OMNI_ATTR("no_py")
        createType_abi(TypeId id, OMNI_ATTR("c_str") const char* moduleName, uint32_t implVersion) noexcept = 0;

    //! Registers types from the given module.
    //!
    //! If the module is currently loaded, it will not be reloaded and kResultSuccess is returned.
    //!
    //! Modules (e.g. .dll or .so) may contain one or many implementations of one or many interfaces. When registering a
    //! module with the type factory, a function, whose name is described by 'kModuleGetExportsName', is found and
    //! invoked.  Let's assume the exported function name is "omniModuleGetExports".
    //!
    //! "omniModuleGetExports" returns a key/value database of the module's capabilities and the module's requirements.
    //! Some things to note about this database:
    //!
    //!   - The module's requirements can be marked as optional.
    //!
    //!   - The module's capabilities can be ignored by ITypeFactory.
    //!
    //! These properties allow ITypeFactory and the module to find an intersection of desired functionality in a data
    //! driven manner.  If one party's required needs are not met, the module fails to load (e.g. an appropriate
    //! omni::core::Result is returned).
    //!
    //! It is expected the module has entries in the key/value database describing the functions ITypeFactory should
    //! call during the loading process.  The most important of these entries is the one defined by
    //! OMNI_MODULE_ON_MODULE_LOAD(), which points to the function ITypeFactory should call to get a list of
    //! implementations in the module.  ITypeFactory invokes exports from the module in the following pattern:
    //!
    //! .--------------------------------------------------------------------------------------------------------------.
    //! |                                                -> Time ->                                                    |
    //! |--------------------------------------------------------------------------------------------------------------|
    //! | omniModuleGetExports | onLoad (req.) | onStarted (optional) | onCanUnload (optional) | onUnload (optional)   |
    //! |                      |               | impl1->createFn      |                        |                       |
    //! |                      |               | impl2->createFn      |                        |                       |
    //! |                      |               | impl1->createFn      |                        |                       |
    //! \--------------------------------------------------------------------------------------------------------------/
    //!
    //! Above, functions in the same column can be called concurrently. It's up to the module to make sure such call
    //! patterns are thread safe within the module.
    //!
    //! onCanUnload and createFn can be called multiple times.  All other functions are called once during the lifecycle
    //! of a module.
    //!
    //! \see omni/core/ModuleExports.h.
    //! \see onModuleLoadFn
    //! \see onModuleStartedFn
    //! \see onModuleCanUnloadFn
    //! \see onModuleUnloadFn
    //!
    //!
    //! The module can be explicitly unloaded with unregisterInterfaceImplementationsFromModule().
    //!
    //! Upon destruction of this ITypeFactory, unregisterInterfaceImplementationsFromModule is called for each loaded
    //! module.  If the ITypeFactory destructor's call to unregisterInterfaceImplementationsFromModule fails to safely
    //! unload a module (via the module's onModuleCanUnload and onModuleUnload), an attempt will be made to
    //! forcefully/unsafely unload the module.
    //!
    //! The given module name must not be nullptr.
    //!
    //! This method is thread safe. Modules can be loaded in parallel.
    //!
    //! \returns Returns kResultSuccess if the module is loaded (either due to this function or a previous call).
    //! Otherwise, an error is returned.
    virtual Result registerInterfaceImplementationsFromModule_abi(OMNI_ATTR("c_str, not_null") const char* moduleName,
                                                                  TypeFactoryLoadFlags flags) noexcept = 0;

    //! Unregisters all types registered from the given module.
    //!
    //! Unregistering a module may fail if the module does not belief it can safely be unloaded. This is determined by
    //! OMNI_MODULE_ON_MODULE_CAN_UNLOAD().
    //!
    //! If unregistration does succeed, the given module will be unloaded from the process space.
    //!
    //! Upon destruction of this ITypeFactory, unregisterInterfaceImplementationsFromModule is called for each loaded
    //! module.  If the ITypeFactory destructor's call to unregisterInterfaceImplementationsFromModule fails to safely
    //! unload a module (via the module's onModuleCanUnload and onModuleUnload), an attempt will be made to
    //! forcefully/unsafely unload the module.
    //!
    //! The given module name must not be nullptr.
    //!
    //! This method is thread safe.
    //!
    //! \returns Returns kResultSuccess if the module wasn't already loaded or if this method successfully unloaded the
    //! module. Return an error code otherwise.
    virtual Result unregisterInterfaceImplementationsFromModule_abi(OMNI_ATTR("c_str, not_null")
                                                                        const char* moduleName) noexcept = 0;

    //! Register the list of types.
    //!
    //! Needed data from the "implementations" list is copied by this method.
    //!
    //! This method is thread safe.
    virtual OMNI_ATTR("no_py") void registerInterfaceImplementations_abi(
        OMNI_ATTR("in, count=implementationsCount, not_null") const InterfaceImplementation* implementations,
        uint32_t implementationsCount,
        TypeFactoryLoadFlags flags) noexcept = 0;

    //! Maps a type id back to its type name.
    //!
    //! The memory returned is valid for the lifetime of ITypeFactory
    //!
    //! Returns nullptr if id has never been registered. Types that have been registered, and then unregistered, will
    //! still have a valid string returned from this method.
    //!
    //! This method is thread safe.
    virtual const char* getTypeIdName_abi(TypeId id) noexcept = 0;

    //! Sets the implementation matching constraints for the given interface id.
    //!
    //! See omni::core::ITypeFactory_abi::createType_abi() for how these constraints are used.
    //!
    //! moduleName can be nullptr.
    //!
    //! if implVersion is 0 and implId is an implementation id, the implementation with the highest version is chosen.
    //!
    //! This method is thread safe.
    virtual void setInterfaceDefaults_abi(TypeId interfaceId,
                                          TypeId implId,
                                          OMNI_ATTR("c_str") const char* moduleName,
                                          uint32_t implVersion) noexcept = 0;

    //! Returns the implementation matching constraints for the given interface id.
    //!
    //! See omni::core::ITypeFactory_abi::createType_abi() for how these constraints are used.
    //!
    //! If the given output implementation id pointer (outImplid) is not nullptr, it will be populated with the default
    //! implementation id instantiated when the interface requested to be created.
    //!
    //! If the given output implementation version pointer (outImplVersion) is not nullptr, it will be populated with
    //! the default implementation version instantiated when the interface is requested to be created.
    //!
    //! If the output module name pointer (outModuleName) is not nullptr, it will be populated with the name of the
    //! module searched when trying to find an implementation of the interface.  If there is no current default module
    //! name, the output module name will be populated with the empty string. If the output module name's buffer size is
    //! insufficient to store the null terminated module name, kResultBufferInsufficient is returned and the module
    //! name's buffer size is updated with the needed buffer size.
    //!
    //! If the output module name is nullptr, the output module name buffer size (inOutModuleNameCount) will be
    //! populated with the size of the buffer needed to store the module name.
    //!
    //! The output module name buffer size pointer (inOutModuleNameCount) must not be nullptr.
    //!
    //! If the given interface id is not found, kResultNotFound is returned and the output implementation id (outImplId)
    //! and version (outImplVersion), if defined, are set to 0.  Additionally, the output module name (outModuleName),
    //! if defined, is set to the empty string.
    //!
    //! If kResultInsufficientBuffer and kResultNotFound are both flagged internally, kResultNotFound is returned.
    //!
    //! See omni::core::getInterfaceDefaults() for a C++ wrapper to this method.
    //!
    //! This method is thread safe.
    virtual OMNI_ATTR("no_py") Result
        getInterfaceDefaults_abi(TypeId interfaceId,
                                 OMNI_ATTR("out") TypeId* outImplId, // nullptr accepted
                                 OMNI_ATTR("out" /*count=*inOutModuleNameCount*/) char* outModuleName, // nullptr
                                                                                                       // accepted
                                 OMNI_ATTR("in, out, not_null") uint32_t* inOutModuleNameCount, // must not be null
                                 OMNI_ATTR("out") uint32_t* outImplVersion) noexcept = 0; // nullptr accepted
};

//! The version number of a @ref TypeFactoryArgs object being passed around.  This is used to
//! manage backward and forward compatibility checks when an implementation receives the
//! object.  Newer versions of a type factory implementation are expected to be able to handle
//! the layout and content of any older version of this object.
constexpr uint16_t kTypeFactoryArgsVersion = 1;

//! Arguments passed to omniCreateTypeFactory().
class TypeFactoryArgs
{
public:
    //! Version of this structure.  The version should be incremented only when removing/rearranging fields.  Adding
    //! fields (from the reserved space) is allowed without incrementing the version.
    uint16_t version;

    //! Size of this structure in bytes.
    uint16_t byteCount;

    //! Four bytes of intentional padding to ensure the following pointers are appropriately aligned and to
    //! force the size of this object to a known expected value.
    uint8_t padding[4];

    //! A pointer to the @ref omni::log::ILog implementation object that should be used by the core for all
    //! logging operations.  This may be `nullptr` to use the default internal implementation.
    omni::log::ILog* log;

    //! A pointer to the @ref omni::structuredlog::IStructuredLog implementation object that should be
    //! used by the core for all structured logging operations.  This may be `nullptr` to use the default
    //! implementation.
    omni::structuredlog::IStructuredLog* structuredLog;

    //! When adding fields, decrement this reserved space.  Be mindful of alignment (explicitly add padding fields if
    //! needed).
    void* reserved[13];

    TypeFactoryArgs()
    {
        std::memset(this, 0, sizeof(*this));
        version = kTypeFactoryArgsVersion;
        byteCount = sizeof(*this);
    }

    //! Constructor: initializes a new object explicitly referencing the override objects to use.
    //!
    //! @param[in] log_         The @ref omni::log::ILog object to use for all operations that go through
    //!                         `ILog`.  This may be `nullptr` to use the default internal implementation.
    //! @param[in] strucLog_    The @ref omni::structuredlog::IStructuredLog object to use for all operations
    //!                         that go through `IStructuredLog`.  This may be `nullptr` to use the default
    //!                         implementation.
    //! @returns No Return value.
    //!
    //! @remarks This initializes a new object with specific override objects.  There is currently no way
    //!          to specify that one of the override objects should be disabled completely - if a `nullptr`
    //!          object is passed in, the default implementation will be used instead.
    //!
    TypeFactoryArgs(omni::log::ILog* log_, omni::structuredlog::IStructuredLog* strucLog_) : TypeFactoryArgs()
    {
        log = log_;
        structuredLog = strucLog_;
    }
};

static_assert(std::is_standard_layout<TypeFactoryArgs>::value, "TypeFactoryArgs must be standard layout");
static_assert((8 + 15 * sizeof(void*)) == sizeof(TypeFactoryArgs), "TypeFactoryArgs has an unexpected size");

} // namespace core
} // namespace omni

#include "ITypeFactory.gen.h"

#ifdef OMNI_COMPILE_AS_DYNAMIC_LIBRARY
OMNI_API omni::core::ITypeFactory* omniGetTypeFactoryWithoutAcquire();
#else
//! Returns the global ITypeFactory. omni::core::IObject::acquire() is **not** called on the returned pointer.
//!
//! The global omni::core::ITypeFactory instance can be configured by passing an omni::core::ITypeFactory to
//! omniCoreStart(). If an instance is not provided, omniCreateTypeFactory() is called.
inline omni::core::ITypeFactory* omniGetTypeFactoryWithoutAcquire()
{
    return static_cast<omni::core::ITypeFactory*>(omniGetBuiltInWithoutAcquire(OmniBuiltIn::eITypeFactory));
}
#endif

//! Creates a default implementation of ITypeFactory.
//!
//! The given TypeFactoryArgs pointer will only be accessed during this call.
//!
//! nullptr is accepted.
OMNI_API omni::core::ITypeFactory* omniCreateTypeFactory(const omni::core::TypeFactoryArgs* args = nullptr);

// clang-format off
OMNI_DEFINE_INTERFACE_API(omni::core::ITypeFactory)
{
public:
    //! Instantiates an implementation of interface T.
    //!
    //! See omni::core::ITypeFactory_abi::createType_abi() for instantiation rules.
    template <typename T>
    inline ObjectPtr<T> createType(const char* moduleName = nullptr, uint32_t version = 0) noexcept
    {
        return createType<T>(T::kTypeId, moduleName, version);
    }

    //! Instantiates the given type and casts it to T.
    //!
    //! The given type id can be an implementation id.
    //!
    //! If the interface type T is not implemented by the type id, nullptr is returned.
    //!
    //! See omni::core::ITypeFactory_abi::createType_abi() for instantiation rules.
    template <typename T = IObject>
    inline ObjectPtr<T> createType(TypeId id, const char* moduleName = nullptr, uint32_t version = 0) noexcept
    {
        auto ptr = steal(createType_abi(id, moduleName, version));
        return ptr.template as<T>();
    }
};
// clang-format on

namespace omni
{
namespace core
{

//! Instantiates an implementation of interface T.
//!
//! See omni::core::ITypeFactory_abi::createType_abi() for instantiation rules.
template <typename T>
inline ObjectPtr<T> createType(const char* moduleName = nullptr, uint32_t version = 0) throw()
{
    return omniGetTypeFactoryWithoutAcquire()->createType<T>(moduleName, version);
}

//! Instantiates the given type and casts it to T.
//!
//! The given type id can be an implementation id.
//!
//! If the interface type T is not implemented by the type id, nullptr is returned.
//!
//! \see omni::core::ITypeFactory_abi::createType_abi() for instantiation rules.
template <typename T = IObject>
inline ObjectPtr<T> createType(TypeId id, const char* moduleName = nullptr, uint32_t version = 0) CARB_NOEXCEPT
{
    return omniGetTypeFactoryWithoutAcquire()->createType<T>(id, moduleName, version);
}

//! \see ITypeFactory::registerInterfaceImplementationsFromModule().
inline Result registerInterfaceImplementationsFromModule(const char* moduleName, // e.g. omni.scripting-python.dll
                                                         TypeFactoryLoadFlags flags = 0) CARB_NOEXCEPT
{
    return omniGetTypeFactoryWithoutAcquire()->registerInterfaceImplementationsFromModule(moduleName, flags);
}

//! \see ITypeFactory::registerInterfaceImplementations().
inline void registerInterfaceImplementations(const InterfaceImplementation* implementations,
                                             uint32_t implementationsCount,
                                             TypeFactoryLoadFlags flags = 0) CARB_NOEXCEPT
{
    return omniGetTypeFactoryWithoutAcquire()->registerInterfaceImplementations(
        implementations, implementationsCount, flags);
}

//! \see ITypeFactory::getTypeIdName().
inline const char* getTypeIdName(TypeId id) CARB_NOEXCEPT
{
    return omniGetTypeFactoryWithoutAcquire()->getTypeIdName(id);
}

//! \see ITypeFactory::setInterfaceDefaults().
template <typename T>
inline void setInterfaceDefaults(TypeId implId, const char* moduleName, uint32_t implVersion)
{
    omniGetTypeFactoryWithoutAcquire()->setInterfaceDefaults(T::kTypeId, implId, moduleName, implVersion);
}

//! Given an interface id (i.e. T), returns the preferred implementation (if any) instantiated when calling
//! omni::core::ITypeFactory::createType(), the preferred module (if any) searched when calling
//! omni::core::ITypeFactory::createType(), and the preferred implementation version number (if any) required
//! when calling omni::core::ITypeFactory::createType().
//!
//! \see omni::core::ITypeFactory::getInterfaceDefault().
//!
//! Unlike the ABI method, this method returns kResultTryAgain if another thread is actively changing the interface
//! defaults. This method internally retries multiple times to get the defaults, but will eventually give up with
//! kResultTryAgain.
//!
//! Unlike the ABI method, kResultInsufficientBuffer is never returned.
template <typename T>
inline Result getInterfaceDefaults(TypeId* implId, std::string* moduleName, uint32_t* implVersion)
{
    if (!moduleName)
    {
        uint32_t moduleNameCount = 0;
        Result result = omniGetTypeFactoryWithoutAcquire()->getInterfaceDefaults(
            T::kTypeId, implId, nullptr, &moduleNameCount, implVersion);
        return result;
    }
    else
    {
        // loop here in case the module name size changes between checking for the size and actually get the string.
        std::vector<char> buffer;
        for (unsigned i = 0; i < 4; ++i)
        {
            uint32_t moduleNameCount = uint32_t(buffer.size());
            Result result = omniGetTypeFactoryWithoutAcquire()->getInterfaceDefaults(
                T::kTypeId, implId, buffer.data(), &moduleNameCount, implVersion);
            if (kResultInsufficientBuffer != result)
            {
                *moduleName = buffer.data();
                return result;
            }
            else
            {
                buffer.resize(moduleNameCount);
            }
        }

        return kResultTryAgain;
    }
}

//! \see ITypeFactory::unregisterInterfaceImplementationsFromModule().
inline Result unregisterInterfaceImplementationsFromModule(const char* moduleName) CARB_NOEXCEPT
{
    return omniGetTypeFactoryWithoutAcquire()->unregisterInterfaceImplementationsFromModule(moduleName);
}

} // namespace core
} // namespace omni
