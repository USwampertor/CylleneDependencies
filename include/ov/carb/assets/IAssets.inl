// Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
namespace carb
{
namespace assets
{

template <typename Type>
inline ScopedSnapshot<Type> IAssets::takeSnapshot(Id assetId)
{
    return ScopedSnapshot<Type>(this, assetId);
}

template <typename Type>
inline Id IAssets::loadAsset(carb::datasource::IDataSource* dataSource,
                             carb::datasource::Connection* connection,
                             const char* path,
                             Pool pool,
                             carb::tasking::Trackers trackers)
{
    carb::tasking::Tracker const* objects;
    size_t size;
    trackers.output(objects, size);
    return internalLoadAsset(dataSource, connection, path, pool, getAssetType<Type>(), nullptr, objects, size);
}

template <typename Type>
inline Id IAssets::loadAsset(carb::datasource::IDataSource* dataSource,
                             carb::datasource::Connection* connection,
                             const char* path,
                             Pool pool,
                             const LoadParameters& loadParameters,
                             carb::tasking::Trackers trackers)
{
    carb::tasking::Tracker const* objects;
    size_t size;
    trackers.output(objects, size);
    return internalLoadAsset(dataSource, connection, path, pool, getAssetType<Type>(), &loadParameters, objects, size);
}

template <typename Type>
inline void IAssets::registerAssetType(const LoaderDesc& loaderDesc, const AssetTypeParams& params)
{
    registerAssetTypeEx(getAssetType<Type>(), loaderDesc, params);
}

template <typename Type>
inline void IAssets::unregisterAssetType()
{
    unregisterAssetTypeEx(getAssetType<Type>());
}

} // namespace assets
} // namespace carb
