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
#ifndef PXR_USD_IMAGING_USD_IMAGING_GL_DRAW_MODE_ADAPTER_H
#define PXR_USD_IMAGING_USD_IMAGING_GL_DRAW_MODE_ADAPTER_H

#include "pxr/pxr.h"
#include "pxr/usdImaging/usdImagingGL/api.h"
#include "pxr/usdImaging/usdImaging/primAdapter.h"

PXR_NAMESPACE_OPEN_SCOPE


/// \class UsdImagingGLDrawModeAdapter
///
/// Delegate support for the drawMode attribute on UsdGeomModelAPI.
///
class UsdImagingGLDrawModeAdapter : public UsdImagingPrimAdapter
{
public:
    using BaseAdapter = UsdImagingPrimAdapter;

    USDIMAGINGGL_API
    UsdImagingGLDrawModeAdapter();

    USDIMAGINGGL_API
    ~UsdImagingGLDrawModeAdapter() override;

    /// Called to populate the RenderIndex for this UsdPrim. The adapter is
    /// expected to create one or more Rprims in the render index using the
    /// given proxy.
    SdfPath Populate(
            UsdPrim const& prim,
            UsdImagingIndexProxy* index,
            UsdImagingInstancerContext const* instancerContext = NULL) override;

    // If the draw mode adapter is applied to a prim, it cuts off traversal of
    // that prim's subtree.
    bool ShouldCullChildren() const override;

    // Because draw mode can change usdImaging topology, we need to handle
    // render index compatibility at a later point than adapter lookup.
    bool IsSupported(UsdImagingIndexProxy const* index) const override;

    // Cards mode can be applied to instance prims, so we need to let the
    // UsdImagingInstanceAdapter know we want special handling.
    bool CanPopulateUsdInstance() const override;

    // ---------------------------------------------------------------------- //
    /// \name Parallel Setup and Resolve
    // ---------------------------------------------------------------------- //
    
    USDIMAGINGGL_API
    void TrackVariability(UsdPrim const& prim,
                          SdfPath const& cachePath,
                          HdDirtyBits* timeVaryingBits,
                          UsdImagingInstancerContext const* 
                              instancerContext = NULL,
                          // #nv begin fast-updates
                          // If checkVariabilty is false, this method
                          // only populates the value cache with initial values.
                          bool checkVariability = true) const override;
                         // nv end

    USDIMAGINGGL_API
    void UpdateForTime(UsdPrim const& prim,
                       SdfPath const& cachePath, 
                       UsdTimeCode time,
                       HdDirtyBits requestedBits,
                       UsdImagingInstancerContext const* 
                                   instancerContext = NULL) const override;

    // ---------------------------------------------------------------------- //
    /// \name Change Processing 
    // ---------------------------------------------------------------------- //

    USDIMAGINGGL_API
    HdDirtyBits ProcessPropertyChange(UsdPrim const& prim,
                                      SdfPath const& cachePath, 
                                      TfToken const& property) override;

    // Note: Resync/Remove are overridden so that we can resync/remove the
    // material and cards rprim together, since they are populated together.
    USDIMAGINGGL_API
    void ProcessPrimResync(SdfPath const& cachePath,
                           UsdImagingIndexProxy* index) override;

    USDIMAGINGGL_API
    void ProcessPrimRemoval(SdfPath const& cachePath,
                            UsdImagingIndexProxy* index) override;

    USDIMAGINGGL_API
    void MarkDirty(UsdPrim const& prim,
                   SdfPath const& cachePath,
                   HdDirtyBits dirty,
                   UsdImagingIndexProxy* index) override;

    USDIMAGINGGL_API
    void MarkTransformDirty(UsdPrim const& prim,
                            SdfPath const& cachePath,
                            UsdImagingIndexProxy* index) override;

    USDIMAGINGGL_API
    void MarkVisibilityDirty(UsdPrim const& prim,
                             SdfPath const& cachePath,
                             UsdImagingIndexProxy* index) override;

    USDIMAGINGGL_API
    void MarkMaterialDirty(UsdPrim const& prim,
                           SdfPath const& cachePath,
                           UsdImagingIndexProxy* index) override;

    // ---------------------------------------------------------------------- //
    /// \name Data access
    // ---------------------------------------------------------------------- //

    USDIMAGINGGL_API
    VtValue Get(UsdPrim const& prim,
                SdfPath const& cachePath,
                TfToken const& key,
                UsdTimeCode time,
                VtIntArray *outIndices) const override;

    USDIMAGINGGL_API
    HdCullStyle GetCullStyle(UsdPrim const& prim,
                             SdfPath const& cachePath,
                             UsdTimeCode time) const override;

    USDIMAGINGGL_API
    VtValue GetTopology(UsdPrim const& prim,
                        SdfPath const& cachePath,
                        UsdTimeCode time) const override;

    USDIMAGINGGL_API
    GfRange3d GetExtent(UsdPrim const& prim, 
                        SdfPath const& cachePath, 
                        UsdTimeCode time) const override;

    USDIMAGINGGL_API
    bool GetDoubleSided(UsdPrim const& prim, 
                        SdfPath const& cachePath, 
                        UsdTimeCode time) const override;

    USDIMAGINGGL_API
    GfMatrix4d GetTransform(UsdPrim const& prim, 
                            SdfPath const& cachePath,
                            UsdTimeCode time,
                            bool ignoreRootTransform = false) const override;

    USDIMAGINGGL_API
    SdfPath GetMaterialId(UsdPrim const& prim, 
                        SdfPath const& cachePath, 
                        UsdTimeCode time) const override;

    USDIMAGING_API
    VtValue GetMaterialResource(UsdPrim const& prim, 
                                SdfPath const& cachePath, 
                                UsdTimeCode time) const override;
  
protected:
    USDIMAGINGGL_API
    void _RemovePrim(SdfPath const& cachePath,
                     UsdImagingIndexProxy* index) override;

private:
    // For cards rendering, check if we're rendering any faces with 0 area;
    // if so, issue a warning.
    void _SanityCheckFaceSizes(SdfPath const& cachePath,
                               GfRange3d const& extents, uint8_t axes_mask) 
        const;

    void _ComputeGeometryData(UsdPrim const& prim,
                              SdfPath const& cachePath,
                              UsdTimeCode time,
                              TfToken const& drawMode,
                              VtValue* topology, 
                              VtValue* points, 
                              GfRange3d* extent,
                              VtValue* uv,
                              VtValue* assign) const;

    // Check whether the given cachePath is a path to the draw mode material.
    bool _IsMaterialPath(SdfPath const& path) const;

    // Check whether the given cachePath is a path to a draw mode texture.
    bool _IsTexturePath(SdfPath const& path) const;

    // Return true if prim has a time varying extent or extentsHint attribute.
    bool _HasVaryingExtent(UsdPrim const& prim) const;

    // Check if any of the cards texture attributes are marked as time-varying.
    void _CheckForTextureVariability(UsdPrim const& prim,
                                     HdDirtyBits dirtyBits,
                                     HdDirtyBits *timeVaryingBits) const;

    // Computes the extents of the given prim, using UsdGeomBBoxCache.
    // The extents are computed for purposes default/proxy/render.
    GfRange3d _ComputeExtent(UsdPrim const& prim,
                             const UsdTimeCode& timecode) const;

    // Generate geometry for "origin" draw mode.
    void _GenerateOriginGeometry(VtValue* topo, VtValue* points,
                                 GfRange3d const& extents) const;

    // Generate geometry for "bounds" draw mode.
    void _GenerateBoundsGeometry(VtValue* topo, VtValue* points,
                                 GfRange3d const& extents) const;

    // Generate geometry for "cards" draw mode, with cardGeometry "cross".
    void _GenerateCardsCrossGeometry(VtValue* topo, VtValue* points,
            GfRange3d const& extents, uint8_t axes_mask) const;

    // Generate geometry for "cards" draw mode, with cardGeometry "box".
    void _GenerateCardsBoxGeometry(VtValue* topo, VtValue* points,
            GfRange3d const& extents, uint8_t axes_mask) const;

    // Generate geometry for "cards" draw mode, with cardGeometry "fromTexture".
    void _GenerateCardsFromTextureGeometry(VtValue* topo, VtValue* points,
            VtValue* uv, VtValue* assign, GfRange3d* extents,
            UsdPrim const& prim) const;

    // Given an asset attribute pointing to a texture, pull the "worldtoscreen"
    // matrix out of image metadata.
    bool _GetMatrixFromImageMetadata(UsdAttribute const& attr, GfMatrix4d* mat)
        const;

    // Generate texture coordinates for cards "cross"/"box" mode.
    void _GenerateTextureCoordinates(VtValue* uv, VtValue* assign,
                                     uint8_t axes_mask) const;

    // Map from cachePath to what drawMode it was populated as.
    using _DrawModeMap = TfHashMap<SdfPath, TfToken, SdfPath::Hash>;
    _DrawModeMap _drawModeMap;

    // Map from cachePath (of gprim) to what material it's bound to.
    using _MaterialMap = TfHashMap<SdfPath, SdfPath, SdfPath::Hash>;
    _MaterialMap _materialMap;

    // The default value of model:drawModeColor, fetched from the schema
    // registry and stored for quick access...
    GfVec3f _schemaColor;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_USD_IMAGING_GL_DRAW_MODE_ADAPTER_H
