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
//! @brief Interface definition for *carb.assets.plugin*
#pragma once

#include "../Interface.h"
#include "../Types.h"
#include "../datasource/IDataSource.h"
#include "../tasking/TaskingHelpers.h"
#include "AssetsTypes.h"

namespace carb
{

//! Namespace for *carb.assets.plugin* and related utilities.
namespace assets
{

/**
 * Defines an interface for managing assets that are loaded asynchronously.
 */
struct IAssets
{
    CARB_PLUGIN_INTERFACE("carb::assets::IAssets", 2, 0)

    /**
     * Creates an asset pool for managing and caching assets together.
     *
     * @param name The name of the pool.
     * @return The asset pool handle.
     */
    Pool(CARB_ABI* createPool)(const char* name);

    /**
     * Destroys an asset pool previously created with \ref createPool().
     *
     * @param pool The pool to destroy.
     */
    void(CARB_ABI* destroyPool)(Pool pool);

    /**
     * Gets basic statistics about a pool
     *
     * @note The resulting values from this function are transitive and the values may have changed by other threads
     * before the results can be read. They should be used for debugging purposes only.
     *
     * @param pool The pool to get stats about.
     * @param totalAssets Receives the number of assets in the pool.
     * @param assetsLoading Receives the number of assets currently loading.
     */
    void(CARB_ABI* poolStats)(Pool pool, int& totalAssets, int& assetsLoading);

    //! @private
    Id(CARB_ABI* internalLoadAsset)(carb::datasource::IDataSource* dataSource,
                                    carb::datasource::Connection* connection,
                                    const char* path,
                                    Pool pool,
                                    const Type& assetType,
                                    const LoadParameters* loadParameters,
                                    carb::tasking::Object const* trackers,
                                    size_t numTrackers);

    //! @private
    CARB_DEPRECATED("Use loadAsset<> instead.")
    Id loadAssetEx(carb::datasource::IDataSource* dataSource,
                   carb::datasource::Connection* connection,
                   const char* path,
                   Pool pool,
                   const Type& assetType,
                   const LoadParameters* loadParameters,
                   carb::tasking::Object const* trackers,
                   size_t numTrackers)
    {
        return internalLoadAsset(dataSource, connection, path, pool, assetType, loadParameters, trackers, numTrackers);
    }

    /**
     * Unloads an asset previously loaded with \ref loadAsset().
     *
     * If an asset that is currently loading is unloaded, this will attempt to cancel the load.
     *
     * @param assetId The asset to unload.
     */
    void(CARB_ABI* unloadAsset)(Id assetId);

    /**
     * Unloads all assets from the specified asset pool.
     *
     * If any assets in the pool are currently loading, this will attempt to cancel the load.
     *
     * @param pool The pool to clear the assets from.
     */
    void(CARB_ABI* unloadAssets)(Pool pool);

    /**
     * Pauses the current thread or task until the requested asset has finished loading.
     *
     * @param assetId The assetId to wait for.
     */
    void(CARB_ABI* yieldForAsset)(Id assetId);

    /**
     * Pauses the current thread or task until all assets in the given pool have finished loading.
     *
     * @param pool The pool containing the assets to wait for.
     */
    void(CARB_ABI* yieldForAssets)(Pool pool);

    /**
     * Registers a callback that will be notified when an asset changes.
     *
     * The callback occurs after the asset has changed. At the point of the callback, \ref acquireSnapshot() will return
     * the updated data.
     *
     * @note Only one callback can be registered for a given \p assetId. If the given \p assetId already has a callback
     * registered, it is revoked in favor of this new callback.
     *
     * @param assetId The asset to monitor for changes.
     * @param onChangeEvent The callback function to be called once the changes are made.
     * @param userData The user data associated with the callback.
     */
    void(CARB_ABI* subscribeToChangeEvent)(Id assetId, OnChangeEventFn onChangeEvent, void* userData);

    /**
     * Unsubscribes any asset change callbacks for a given asset previously registered with
     * \ref subscribeToChangeEvent().
     *
     * When this function returns, it is guaranteed that the previously registered \c onChangeEvent function is not
     * currently being called from another thread, and will not be called again.
     *
     * @param assetId The assetId to remove subscriptions for.
     */
    void(CARB_ABI* unsubscribeToChangeEvent)(Id assetId);

    /**
     * Acquires a \ref Snapshot of the asset of the given type.
     *
     * @note It is the caller's responsibility to release the snapshot via \ref releaseSnapshot() when finished with it.
     *
     * If an asset changes (i.e. a change event was issued for the given \p assetId), existing snapshots are not
     * updated; you will have to release existing snapshots and acquire a new snapshot to get the updated data.
     *
     * @param assetId The asset to take a snapshot of.
     * @param assetType The asset type being requested.
     * @param reason The reason the snapshot could, or couldn't be taken.
     *               While the snapshot is loading, it will return \ref Reason::eLoading;
     *               any other value returned means the snapshot has finished loading.
     *               It is recommended to use \ref carb::tasking::Trackers with \ref loadAsset() instead of polling this
     *               function until \ref Reason::eLoading is no longer returned.
     * @returns The snapshot handle for the asset at the present time.
     */
    Snapshot(CARB_ABI* acquireSnapshot)(Id assetId, const Type& assetType, Reason& reason);

    /**
     * Releases a snapshot of an asset previously returned by \ref acquireSnapshot().
     *
     * @param snapshot The snapshot to release.
     */
    void(CARB_ABI* releaseSnapshot)(Snapshot snapshot);

    /**
     * Gets the underlying data for the asset based on a snapshot.
     *
     * @param snapshot The snapshot of the asset to get the data from.
     * @returns The raw data of the asset at the snapshot. If the asset's \c Type has been unregistered then `nullptr`
     * is returned.
     */
    void*(CARB_ABI* getDataFromSnapshot)(Snapshot snapshot);

    /**
     * Forces all dirty (that is, assets with changed data) assets of a given \c Type to reload.
     *
     * This function is only necessary for registered types where \ref AssetTypeParams::autoReload is set to \c false.
     *
     * @param assetType The \c Type of asset to request a reload for.
     */
    void(CARB_ABI* reloadAnyDirty)(Type assetType);

    /**
     * Used to register a loader for a specific asset \c Type.
     *
     * @warning Typically \ref registerAssetType() should be used instead of this lower-level function.
     *
     * @param assetType The asset type to register.
     * @param desc The loader descriptor.
     * @param params The \ref AssetTypeParams settings for this assetType
     */
    void(CARB_ABI* registerAssetTypeEx)(const Type& assetType, const LoaderDesc& desc, const AssetTypeParams& params);

    /**
     * Unregisters a specific asset loader.
     *
     * @note Typically \ref unregisterAssetType() should be used instead of this lower-level function.
     *
     * @warning All data from any \ref Snapshot objects (i.e. from \ref acquireSnapshot()) is invalid after calling
     * this function. The \ref Snapshot handle remains valid but any attempts to retrieve data from the \ref Snapshot
     * (with \ref getDataFromSnapshot()) will return \c nullptr. Using a \ref ScopedSnapshot of the given \c Type after
     * calling this function produces undefined behavior.
     *
     * @note This function will attempt to cancel all loading tasks for this \p assetType and will wait for all loading
     * tasks to complete.
     *
     * @param assetType The asset type to unregister.
     */
    void(CARB_ABI* unregisterAssetTypeEx)(const Type& assetType);

    /**
     * Loads an asset of the given type. This overload uses default \ref LoadParameters.
     *
     * Events:
     *  - `Asset.BeginLoading` - Sent in the calling thread if asset load starts. Also sent by \ref reloadAnyDirty() or
     *    by a background thread if underlying data changes. Parameters:
     *     - `Path` (`const char*`) - the path to the asset.
     *     - `AssetId` (\ref Id) - the asset ID that is loading.
     *  - `Asset.EndLoading` - Sent from a background thread whenever asset load finishes, only for assets that have
     *    previously sent a `Asset.BeginLoading` event.
     *     - `Path` (`const char*`) - the path to the asset.
     *     - `AssetId` (\ref Id) - the asset ID that finished loading.
     *     - `Success` (bool) - whether the load was successful or not. If \c true, \ref acquireSnapshot() will acquire
     *       the new data for the asset.
     *
     * @tparam Type The type of the asset. A compile error will occur if \ref getAssetType() for the given type does not
     * resolve to a function. @see CARB_ASSET.
     * @param dataSource The data source to load from.
     * @param connection The connection (from the given data source) to load from.
     * @param path The asset path to load.
     * @param pool The pool to load the asset into.
     * @param trackers (Optional) Trackers that can be queried to see when the asset is done loading.
     * @returns A unique \ref Id that identifies this asset. The asset system internally de-duplicates based on path,
     * datasource, connection and \ref LoadParameters, so several different asset \ref Id results may reference the same
     * underlying asset.
     */
    template <typename Type>
    Id loadAsset(carb::datasource::IDataSource* dataSource,
                 carb::datasource::Connection* connection,
                 const char* path,
                 Pool pool,
                 carb::tasking::Trackers trackers = carb::tasking::Trackers{});

    /**
     * Loads an asset of the given type, with the given \ref LoadParameters.
     *
     * Events:
     *  - `Asset.BeginLoading` - Sent in the calling thread if asset load starts. Also sent by \ref reloadAnyDirty() or
     *    by a background thread if underlying data changes. Parameters:
     *     - `Path` (`const char*`) - the path to the asset.
     *     - `AssetId` (\ref Id) - the asset ID that is loading.
     *  - `Asset.EndLoading` - Sent from a background thread whenever asset load finishes, only for assets that have
     *    previously sent a `Asset.BeginLoading` event.
     *     - `Path` (`const char*`) - the path to the asset.
     *     - `AssetId` (\ref Id) - the asset ID that finished loading.
     *     - `Success` (bool) - whether the load was successful or not. If \c true, \ref acquireSnapshot() will acquire
     *       the new data for the asset.
     *
     * @tparam Type The type of the asset. A compile error will occur if \ref getAssetType() for the given type does not
     * resolve to a function. @see CARB_ASSET.
     * @param dataSource The data source to load from.
     * @param connection The connection (from the given data source) to load from.
     * @param path The asset path to load.
     * @param pool The pool to load the asset into.
     * @param loadParameters The \ref LoadParameters derived class containing information about how to load the asset.
     * @param trackers (Optional) Trackers that can be queried to see when the asset is done loading.
     * @returns A unique \ref Id that identifies this asset. The asset system internally de-duplicates based on path,
     * datasource, connection and \ref LoadParameters, so several different asset \ref Id results may reference the same
     * underlying asset.
     */
    template <typename Type>
    Id loadAsset(carb::datasource::IDataSource* dataSource,
                 carb::datasource::Connection* connection,
                 const char* path,
                 Pool pool,
                 const LoadParameters& loadParameters,
                 carb::tasking::Trackers trackers = carb::tasking::Trackers{});

    /**
     * Takes a snapshot of the asset in a RAII-style object.
     *
     * @tparam Type The asset type.
     * @param assetId The assetId to take a snapshot of.
     * @returns A RAII-style object that manages the snapshot of data for the object.
     */
    template <typename Type>
    ScopedSnapshot<Type> takeSnapshot(Id assetId);

    /**
     * Used to register a loader for a specific asset \c Type.
     *
     * @tparam Type The asset type to register.
     * @param loaderDesc The loader descriptor.
     * @param params The \ref AssetTypeParams settings for this assetType
     */
    template <typename Type>
    void registerAssetType(const LoaderDesc& loaderDesc, const AssetTypeParams& params = AssetTypeParams::getDefault());

    /**
     * Unregisters a specific asset loader.
     *
     * @note Typically \ref unregisterAssetType() should be used instead of this lower-level function.
     *
     * @warning All data from any \ref Snapshot objects (i.e. from \ref acquireSnapshot()) is invalid after calling
     * this function. The \ref Snapshot handle remains valid but any attempts to retrieve data from the \ref Snapshot
     * (with \ref getDataFromSnapshot()) will return \c nullptr. Using a \ref ScopedSnapshot of the given \c Type after
     * calling this function produces undefined behavior.
     *
     * @note This function will attempt to cancel all loading tasks for this \p assetType and will wait for all loading
     * tasks to complete.
     *
     * @tparam Type The asset type to unregister.
     */
    template <typename Type>
    void unregisterAssetType();
};
} // namespace assets
} // namespace carb

#include "IAssets.inl"
