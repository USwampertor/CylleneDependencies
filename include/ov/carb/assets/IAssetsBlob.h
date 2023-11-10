// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief "Blob" (Binary Large OBject) asset type definition
#pragma once

#include "AssetsTypes.h"

namespace carb
{
namespace assets
{

//! An opaque type representing a binary large object. Use \ref IAssetsBlob to access the data.
//! Can be loaded with `IAssets::loadAsset<Blob>(...)`.
struct Blob;

/**
 * Defines an interface for managing assets that are loaded asynchronously.
 */
struct IAssetsBlob
{
    CARB_PLUGIN_INTERFACE("carb::assets::IAssetsBlob", 1, 0)

    /**
     * Gets the data from a blob.
     *
     * @param blob The blob to use.
     * @return The blob byte data.
     */
    const uint8_t*(CARB_ABI* getBlobData)(Blob* blob);

    /**
     * Gets the size of the blob in bytes.
     *
     * @param blob The blob to use.
     * @return The size of the blob in bytes.
     */
    size_t(CARB_ABI* getBlobSize)(Blob* blob);
};
} // namespace assets
} // namespace carb

CARB_ASSET(carb::assets::Blob, 1, 0)
