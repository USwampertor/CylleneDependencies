//
// Copyright 2016 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#ifndef PXR_USD_IMAGING_USD_IMAGING_POINTS_ADAPTER_H
#define PXR_USD_IMAGING_USD_IMAGING_POINTS_ADAPTER_H

/// \file usdImaging/pointsAdapter.h

#include "pxr/pxr.h"
#include "pxr/usdImaging/usdImaging/api.h"
#include "pxr/usdImaging/usdImaging/primAdapter.h"
#include "pxr/usdImaging/usdImaging/gprimAdapter.h"

PXR_NAMESPACE_OPEN_SCOPE


/// \class UsdImagingPointsAdapter
///
/// Delegate support for UsdGeomPoints.
///
class UsdImagingPointsAdapter : public UsdImagingGprimAdapter 
{
public:
    using BaseAdapter = UsdImagingGprimAdapter;

    UsdImagingPointsAdapter()
        : UsdImagingGprimAdapter()
    {}

    USDIMAGING_API
    ~UsdImagingPointsAdapter() override;

    // ---------------------------------------------------------------------- //
    /// \name Scene Index Support
    // ---------------------------------------------------------------------- //

    USDIMAGING_API
    TfTokenVector GetImagingSubprims() override;

    USDIMAGING_API
    TfToken GetImagingSubprimType(TfToken const& subprim) override;

    USDIMAGING_API
    HdContainerDataSourceHandle GetImagingSubprimData(
            TfToken const& subprim,
            UsdPrim const& prim,
            const UsdImagingDataSourceStageGlobals &stageGlobals) override;

    // ---------------------------------------------------------------------- //
    /// \name Initialization
    // ---------------------------------------------------------------------- //
    
    USDIMAGING_API
    SdfPath Populate(
        UsdPrim const& prim,
        UsdImagingIndexProxy* index,
        UsdImagingInstancerContext const* instancerContext = nullptr) override;

    USDIMAGING_API
    bool IsSupported(UsdImagingIndexProxy const* index) const override;

    // ---------------------------------------------------------------------- //
    /// \name Parallel Setup and Resolve
    // ---------------------------------------------------------------------- //

    /// Thread Safe.
    USDIMAGING_API
    void TrackVariability(
        UsdPrim const& prim,
        SdfPath const& cachePath,
        HdDirtyBits* timeVaryingBits,
        UsdImagingInstancerContext const* instancerContext = nullptr,
        // #nv begin fast-updates
        // If checkVariabilty is false, this method
        // only populates the value cache with initial values.
        bool checkVariability = true)
        // nv end
            const override;

    /// Thread Safe.
    USDIMAGING_API
    void UpdateForTime(
        UsdPrim const& prim,
        SdfPath const& cachePath, 
        UsdTimeCode time,
        HdDirtyBits requestedBits,
        UsdImagingInstancerContext const* instancerContext = nullptr) 
            const override;

    // ---------------------------------------------------------------------- //
    /// \name Change Processing
    // ---------------------------------------------------------------------- //

    USDIMAGING_API
    HdDirtyBits ProcessPropertyChange(UsdPrim const& prim,
                                      SdfPath const& cachePath,
                                      TfToken const& propertyName) override;

    // ---------------------------------------------------------------------- //
    /// \name Data access
    // ---------------------------------------------------------------------- //

    USDIMAGING_API
    VtValue Get(UsdPrim const& prim,
                SdfPath const& cachePath,
                TfToken const& key,
                UsdTimeCode time,
                VtIntArray *outIndices) const override;

protected:
    USDIMAGING_API
    bool _IsBuiltinPrimvar(TfToken const& primvarName) const override;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_USD_IMAGING_POINTS_ADAPTER_H
