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
//! @brief Type definitions for *carb.assets.plugin*
#pragma once

#include "../Defines.h"

#include "../extras/Hash.h"
#include "../Strong.h"

namespace carb
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace datasource
{
struct IDataSource;
struct Connection;
} // namespace datasource
#endif

namespace assets
{

/**
 * The reason a Snapshot was taken or not.
 */
enum class Reason
{
    eSuccess, //!< The asset was loaded, and the snapshot is valid.
    eInvalidHandle, //!< The asset handle was invalid, this may mean the asset was canceled.
    eInvalidType, //!< Although the asset may or may not have loaded, the snapshot type did not match the type the asset
                  //!< was loaded from. This should be construed as a programmer error.
    eFailed, //!< The asset was not loaded, because load asset failed. Notably loadAsset returned nullptr.
    eLoading, //!< The asset is still in the process of loading.
};

// The following are handles to detect incorrect usage patters such as using a handle
// after it has be destroyed.

//! An Asset ID, used to identify a particular asset.
CARB_STRONGTYPE(Id, size_t);

//! An Asset Pool, used to group assets together.
CARB_STRONGTYPE(Pool, size_t);

//! A snapshot, representing asset data at a given point in time.
CARB_STRONGTYPE(Snapshot, size_t);

//! C++ Type hash used to identify a C++ type. Typically a hash of a type name string.
using HashedType = uint64_t;

//! Used to identify an invalid asset id.
constexpr Id kInvalidAssetId{};

//! Used to identify an invalid pool.
constexpr Pool kInvalidPool{};

//! Used to identify an invalid snapshot.
constexpr Snapshot kInvalidSnapshot{};

//! A load context that exists for the duration of the load phase. A loader may create a subclass of this to maintain
//! context data about a type.
struct LoadContext
{
};

//! Parameters that can be passed into \ref IAssets::loadAsset(). Asset types can create a subclass of this to pass
//! asset-type-specific load parameters through the asset system.
struct LoadParameters
{
};

/**
 * Wrapper for an asset type that can be passed to various functions, identifies the asset as a hashed string
 * plus the version number, which can impact the LoadParameters structure.
 */
struct Type
{
    //! Constructor
    Type(HashedType hashedType, uint32_t majorVersion, uint32_t minorVersion)
        : hashedType(hashedType), majorVersion(majorVersion), minorVersion(minorVersion)
    {
    }
    uint64_t hashedType; //!< The hashed string name of the Type.
    uint32_t majorVersion; //!< The major version
    uint32_t minorVersion; //!< The minor version
};

//! A scoped snapshot, this automatically releases the snapshot when it goes out of scope.
template <class Type>
class ScopedSnapshot;

/**
 * The hash must be a strong (not necessarily cryptographically secure) 128-bit hash.
 * 128-bit hashes are chosen because the if the hash is well distributed, then the probability of a collision even given
 * a large set size is low. See: https://en.wikipedia.org/wiki/Birthday_problem#Probability_table
 *
 * For this reason when using hashing we do not do a deep compare on the objects.
 */
using AssetHash = extras::hash128_t;

/**
 * Defines a template for users to specialize their asset type as unique identifiers.
 *
 * The \ref CARB_ASSET macro specializes this function for registered asset types.
 *
 * @see CARB_ASSET
 *
 * Example:
 * ```cpp
 * CARB_ASSET(carb::imaging::Image, 1, 0)
 * ```
 */
template <typename T>
Type getAssetType();

/**
 * Determines if a currently processed load has been canceled.
 *
 * @note This callback is only valid within the scope of the \ref LoadAssetFn function
 *
 * This function is used by the \ref LoadAssetFn to determine if it can abort processing early.
 * Calling this function is optional. For longer or multi-stage loading processes, calling this function can be an
 * indicator as to whether any assets still exist which are interested in the data that \ref LoadAssetFn is processing.
 *
 * @param userData Must be the `isLoadCanceledUserData` provided to \ref LoadAssetFn.
 * @returns \c true if the load was canceled and should be aborted; \c false otherwise.
 */
using IsLoadCanceledFn = bool (*)(void* userData);

/**
 * Loader function (member of \ref LoaderDesc) used to construct an asset from raw data.
 *
 * Though the raw data for \p path has already been loaded from \p dataSource and \p connection, they are all provided
 * to this function in case additional data must be loaded.
 *
 * @note This function is called in Task Context (from carb::tasking::ITasking), therefore the called function is free
 * to await any fiber-aware I/O functions (i.e. sleep in a fiber safe manner) without bottlenecking the system.
 *
 * @param dataSource The datasource the asset is being loaded from.
 * @param connection The connection the asset is being loaded from.
 * @param path The path the file was loaded from.
 * @param data The data to be loaded.
 * @param size The size of the data (in bytes) to be loaded.
 * @param loadParameters Optional parameters passed from \ref IAssets::loadAsset() (Asset Type specific).
 * @param loadContext A context generated by \ref CreateContextFn, if one was provided.
 * @param isLoadCanceled A function that can be called periodically to determine if load should be canceled. This
 * function need only be called if the load process has multiple steps or lots of processing.
 * @param isLoadCanceledUserData User data that must be provided to \p isLoadCanceled when (if) it is called.
 * @return The loaded Asset, or \c nullptr if loading is aborted or an error occurred.
 */
using LoadAssetFn = void* (*)(carb::datasource::IDataSource* dataSource,
                              carb::datasource::Connection* connection,
                              const char* path,
                              const uint8_t* data,
                              size_t size,
                              const LoadParameters* loadParameters,
                              LoadContext* loadContext,
                              IsLoadCanceledFn isLoadCanceled,
                              void* isLoadCanceledUserData);

/**
 * Loader function (member of \ref LoaderDesc) used to unload an asset.
 *
 * @param asset The asset to be unloaded. This data was previously returned from \ref LoadAssetFn.
 */
using UnloadAssetFn = void (*)(void* asset);

/**
 * Loader function (member of \ref LoaderDesc) that informs the asset type that loading a file has started and creates
 * any load-specific context data necessary.
 *
 * @note This callback is optional, if it isn't provided, in future callbacks \c loadContext will be \c nullptr. If this
 * function is provided, \ref DestroyContextFn should also be provided to destroy the context.
 *
 * @note This function is called in Task Context (from carb::tasking::ITasking), therefore the called function is free
 * to await any fiber-aware I/O functions without bottlenecking the system.
 *
 * This function gives the programmer the option to do initial work that may be repetitive in the function calls
 * that load the file.
 *
 * The created context only exists during the loading of the asset, after the asset is loaded this context is destroyed
 * with \ref DestroyContextFn.
 *
 * @param dataSource The datasource the asset is being loaded from.
 * @param connection The connection the asset is being loaded from.
 * @param path The path of the file that is being loaded.
 * @param data The data read at the asset's URI path.
 * @param size The size of the data read in bytes.
 * @param loadParameters The load parameters passed to \ref IAssets::loadAsset(), or \c nullptr.
 * @returns A derivative of \ref LoadContext that is passed to \ref LoadAssetFn and \ref CreateDependenciesFn.
 */
using CreateContextFn = LoadContext* (*)(carb::datasource::IDataSource* dataSource,
                                         carb::datasource::Connection* connection,
                                         const char* path,
                                         const uint8_t* data,
                                         size_t size,
                                         const LoadParameters* loadParameters);

/**
 * Loader function (member of \ref LoaderDesc) that destroys the data created by \ref CreateContextFn.
 *
 * This function is optional, but always called if present, even if \p context is \c nullptr.
 *
 * @param context The context to destroy, previously created by \ref CreateContextFn.
 */
using DestroyContextFn = void (*)(LoadContext* context);

/**
 * Loader function (member of \ref LoaderDesc) that returns a string of the asset dependencies, that is, other files to
 * watch for changes.
 *
 * This function is optional, if it isn't provided, then only \p path (passed to \ref IAssets::loadAsset()) will be
 * monitored for changes.
 *
 * Many asset types, such as shaders, include additional files to generate their content. In this case it isn't just
 * the original file changing that requires the asset to be reloaded, if any dependent file changes, then the asset
 * has to be reloaded as well.
 *
 * Multiple dependencies must separated by the `|` character.
 *
 * @param dataSource The datasource the asset is being loaded from.
 * @param connection The connection the asset is being loaded from.
 * @param path The path of the file that is being loaded.
 * @param data The loaded data of the requested assets file.
 * @param size The size of the requested asset file.
 * @param loadParameters The load parameters provided to \ref IAssets::loadAsset().
 * @param context The context if any generated by \ref CreateContextFn.
 * @return A string containing dependencies to watch, delimited by `|`; \c nullptr may be returned to indicate no
 * dependencies. The returned pointer will be passed to \ref DestroyDependenciesFn to clean up the returned memory.
 */
using CreateDependenciesFn = const char* (*)(carb::datasource::IDataSource* dataSource,
                                             carb::datasource::Connection* connection,
                                             const char* path,
                                             const uint8_t* data,
                                             size_t size,
                                             const LoadParameters* loadParameters,
                                             LoadContext* context);

/**
 * Loader function (member of \ref LoaderDesc) that cleans up the previously returned value from
 * \ref CreateDependenciesFn.
 *
 * @note This function is required and called if and only if \ref CreateDependenciesFn is provided.
 *
 * @param dependencies The string generated by a previous call to \ref CreateDependenciesFn.
 * @param context The context if any generated by \ref CreateContextFn.
 */
using DestroyDependenciesFn = void (*)(const char* dependencies, LoadContext* context);

/**
 * Loader function (member of \ref LoaderDesc) that is called when a dependency changes.
 *
 * @param dataSource The datasource of the dependency that changed.
 * @param connection The connection of the dependency that changed.
 * @param path The path of the dependency that changed.
 */
using OnDependencyChangedFn = void (*)(carb::datasource::IDataSource* dataSource,
                                       carb::datasource::Connection* connection,
                                       const char* path);

/**
 * Loader function (member of \ref LoaderDesc) that hashes an asset's data, this is used to combine collisions in the
 * asset system.
 *
 * This function is optional; if not provided, the path of the loaded file is hashed.
 *
 * If two different files return the same hash, then the system will return a copy of the first
 * asset load. An example of where this is useful is programmatically generated shaders. In this
 * context this system ensures that only one unique shader is created from many sources that generate
 * the same shader.
 *
 * @param dataSource The datasource the asset is being loaded from.
 * @param connection The connection the asset is being loaded from.
 * @param path The path of the file that is being loaded.
 * @param data The data to be loaded.
 * @param size The size of the data (in bytes) to be loaded.
 * @param loadParameters The load parameters passed to \ref IAssets::loadAsset().
 * @param context A context generated by \ref CreateContextFn, if one was provided.
 * @return The hash of the asset.
 */
using HashAssetFn = AssetHash (*)(carb::datasource::IDataSource* dataSource,
                                  carb::datasource::Connection* connection,
                                  const char* path,
                                  const uint8_t* data,
                                  size_t size,
                                  const LoadParameters* loadParameters,
                                  LoadContext* context);

/**
 * Loader function (member of \ref LoaderDesc) that copies a \ref LoadParameters structure.
 *
 * @note This function is required for any types where a \ref LoadParameters derivative may be passed to
 * \ref IAssets::loadAsset().
 *
 * @param loadParameters The load parameters to copy.
 * @return The copied load parameters.
 */
using CreateLoadParametersFn = LoadParameters* (*)(const LoadParameters* loadParameters);

/**
 * Loader function (member of \ref LoaderDesc) that destroys a copied \ref LoadParameters structure.
 *
 * @note This function is required for any types where a \ref LoadParameters derivative may be passed to
 * \ref IAssets::loadAsset().
 *
 * @param loadParameters The load parameters to destroy.
 */
using DestroyLoadParametersFn = void (*)(LoadParameters* loadParameters);

/**
 * Loader function (member of \ref LoaderDesc) that hashes a LoadParameters structure.
 *
 * @note This function is required for any types where a \ref LoadParameters derivative may be passed to
 * \ref IAssets::loadAsset().
 *
 * @param loadParameters The load parameters to hash.
 * @returns The hashed value of the load parameters structure.
 *
 * @note Be aware of struct padding when hashing the load parameter data.
 *       Passing an entire parameter struct into a hash function may result in
 *       padding being hashed, which will cause undefined behavior.
 */
using HashLoadParametersFn = uint64_t (*)(const LoadParameters* loadParameters);

/**
 * Loader function (member of \ref LoaderDesc) that determines if two \ref LoadParameters derivatives are equal.
 *
 * @note This function is required for any types where a \ref LoadParameters derivative may be passed to
 * \ref IAssets::loadAsset().
 *
 * @param loadParametersA A \ref LoadParameters to compare.
 * @param loadParametersB A \ref LoadParameters to compare.
 * @return \c true if loadParametersA == loadParametersB; \c false otherwise.
 *
 * @note Avoid using \c memcmp() to compare parameters structs as padding within
 *       the struct could cause the comparison to unexpectedly fail.
 */
using LoadParametersEqualsFn = bool (*)(const LoadParameters* loadParametersA, const LoadParameters* loadParametersB);

/**
 * Defines the loader functions for an asset type.
 *
 * The following is the basic call order for loader functions (for clarity, parameters have been simplified).
 *
 * When an asset is being loaded for the first time, or reloaded:
 * ```cpp
 * context = createContext ? createContext() : nullptr;
 *
 * dependencies = createDependencies ? createDependencies() : nullptr;
 * if (dependencies)
 * {
 *     // dependencies are processed
 *     destroyDependencies(dependencies);
 * }
 *
 * hash = hashAsset();
 * // If the hash is already loaded then return that already loaded asset, otherwise:
 *
 * asset = loadAsset();
 * if (context)
 *     destroyContext(context);
 * ```
 *
 * When the asset is destroyed:
 * ```cpp
 * unloadAsset(asset)
 * ```
 *
 */
struct LoaderDesc
{
    LoadAssetFn loadAsset; //!< @see LoadAssetFn
    UnloadAssetFn unloadAsset; //!< @see UnloadAssetFn
    CreateLoadParametersFn createLoadParameters; //!< @see CreateLoadParametersFn
    DestroyLoadParametersFn destroyLoadParameters; //!< @see DestroyLoadParametersFn
    HashLoadParametersFn hashLoadParameters; //!< @see HashLoadParametersFn
    LoadParametersEqualsFn loadParametersEquals; //!< @see LoadParametersEqualsFn
    HashAssetFn hashAsset; //!< @see HashAssetFn
    CreateContextFn createContext; //!< @see CreateContextFn
    DestroyContextFn destroyContext; //!< @see DestroyContextFn
    CreateDependenciesFn createDependencies; //!< @see CreateDependenciesFn
    DestroyDependenciesFn destroyDependencies; //!< @see DestroyDependenciesFn
    OnDependencyChangedFn onDependencyChanged; //!< @see OnDependencyChangedFn
};

//! Parameters that describe an asset type's characteristics.
struct AssetTypeParams
{
    //! Must be initialized to `sizeof(AssetTypeParams)`. This is used as a version for future expansion of this
    //! `struct`.
    size_t sizeofThis;

    //! The maximum number of outstanding concurrent loads of this asset type. A value of \c 0 indicates "unlimited." A
    //! value of \c 1 indicates that loading is not thread-safe and only one asset may be loading at a given time. Any
    //! other value limits the number of loading assets for the given type. (default=`0`)
    uint32_t maxConcurrency;

    //! Specifies that the assets should automatically reload when the file or one of its dependencies changes. If this
    //! is false, you must call \ref IAssets::reloadAnyDirty() in order to manually trigger a reload. (default=`true`)
    bool autoReload;

    //! The amount of time in milliseconds to delay when automatically reloading an asset. This is because a reload is
    //! triggered immediately when a change is detected from the datasource. A filesystem for example can trigger
    //! multiple change notifications for a write to a file. This value gives a sliding window of changes so that all
    //! changes that happen within the window get condensed into a single reload. (default=`100` ms)
    uint32_t reloadDelayMs;

    /**
     * Returns the default values for \c AssetTypeParams.
     * @returns Default values.
     */
    constexpr static AssetTypeParams getDefault()
    {
        return AssetTypeParams{ sizeof(AssetTypeParams), 0, true, 100 };
    }
};

/**
 * Function to provide as a callback on asset changes.
 *
 * @see IAssets::subscribeToChangeEvent()
 *
 * @param assetId The asset ID of the asset that was modified.
 * @param userData The user data given when the subscription was created.
 */
using OnChangeEventFn = void (*)(Id assetId, void* userData);

} // namespace assets
} // namespace carb

#ifdef DOXYGEN_BUILD
/**
 * Registers an asset type.
 *
 * The version is used to protect the LoadParameter class as it's definition could change thereby causing undefined
 * behavior if the asset loader version doesn't match the users of that loader. Therefore, the version should be updated
 * any time the LoadParameters is update to avoid runtime issues.
 *
 * Note the additional two parameters which specify the version of the Asset, in this case
 * the imaging asset is version 1.0. This value should only need to be updated if the LoadParmeters structure is
 * updated.
 *
 * @param t The type of the asset to register
 * @param majorVersion The major version of the asset type.
 * @param minorVersion The minor version of the asset type.
 */
#    define CARB_ASSET(t, majorVersion, minorVersion)
#else
#    define CARB_ASSET(t, majorVersion, minorVersion)                                                                  \
        namespace carb                                                                                                 \
        {                                                                                                              \
        namespace assets                                                                                               \
        {                                                                                                              \
        template <>                                                                                                    \
        inline Type getAssetType<t>()                                                                                  \
        {                                                                                                              \
            return Type(CARB_HASH_TYPE(t), majorVersion, minorVersion);                                                \
        }                                                                                                              \
        }                                                                                                              \
        }
#endif
