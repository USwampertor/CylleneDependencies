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
#ifndef PXR_USD_IMAGING_USD_IMAGING_PRIM_ADAPTER_H
#define PXR_USD_IMAGING_USD_IMAGING_PRIM_ADAPTER_H

/// \file usdImaging/primAdapter.h

#include "pxr/pxr.h"
#include "pxr/usdImaging/usdImaging/api.h"
#include "pxr/usdImaging/usdImaging/version.h"
#include "pxr/usdImaging/usdImaging/collectionCache.h"
#include "pxr/usdImaging/usdImaging/valueCache.h"
#include "pxr/usdImaging/usdImaging/resolvedAttributeCache.h"

#include "pxr/imaging/hd/changeTracker.h"
#include "pxr/imaging/hd/texture.h"
#include "pxr/imaging/hd/selection.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/timeCode.h"

#include "pxr/usd/usdShade/shader.h"

#include "pxr/base/tf/type.h"

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class UsdPrim;

// Forward declaration for nested class.
class UsdImagingDelegate;
class UsdImagingIndexProxy;
class UsdImagingInstancerContext;
class HdExtComputationContext;

using UsdImagingPrimAdapterSharedPtr = 
    std::shared_ptr<class UsdImagingPrimAdapter>;

/// \class UsdImagingPrimAdapter
///
/// Base class for all PrimAdapters.
///
class UsdImagingPrimAdapter 
            : public std::enable_shared_from_this<UsdImagingPrimAdapter> {
public:
    
    // ---------------------------------------------------------------------- //
    /// \name Initialization
    // ---------------------------------------------------------------------- //
 
    UsdImagingPrimAdapter()
    {}

    USDIMAGING_API
    virtual ~UsdImagingPrimAdapter();

    /// Called to populate the RenderIndex for this UsdPrim. The adapter is
    /// expected to create one or more prims in the render index using the
    /// given proxy.
    virtual SdfPath Populate(UsdPrim const& prim,
                UsdImagingIndexProxy* index,
                UsdImagingInstancerContext const* instancerContext = NULL) = 0;

    // Indicates whether population traversal should be pruned based on
    // prim-specific features (like whether it's imageable).
    USDIMAGING_API
    static bool ShouldCullSubtree(UsdPrim const& prim);

    // Indicates whether population traversal should be pruned based on
    // adapter-specific features (like whether the adapter is an instance
    // adapter, and wants to do its own population).
    USDIMAGING_API
    virtual bool ShouldCullChildren() const;

    // Indicates the adapter is a multiplexing adapter (e.g. PointInstancer),
    // potentially managing its children. This flag is used in nested
    // instancer cases to determine which adapter is assigned to which prim.
    USDIMAGING_API
    virtual bool IsInstancerAdapter() const;

    // Indicates whether this adapter can populate a master prim. By policy,
    // you can't directly instance a gprim, but you can directly instance proxy
    // objects (like cards). Note: masters don't have attributes, so an adapter
    // opting in here needs to check if prims it's populating are master prims,
    // and if so find a copy of the instancing prim.
    USDIMAGING_API
    virtual bool CanPopulateMaster() const;

    // ---------------------------------------------------------------------- //
    /// \name Parallel Setup and Resolve
    // ---------------------------------------------------------------------- //
 
    /// For the given \p prim, variability is detected and
    /// stored in \p timeVaryingBits. Initial values are cached into the value
    /// cache.
    ///
    /// This method is expected to be called from multiple threads.
    virtual void TrackVariability(UsdPrim const& prim,
                                  SdfPath const& cachePath,
                                  HdDirtyBits* timeVaryingBits,
                                  UsdImagingInstancerContext const* 
                                      instancerContext = NULL,
                                  // #nv begin fast-updates
                                  // If checkVariabilty is false, this method
                                  // only populates the value cache with initial values.
                                  bool checkVariability = true) const = 0;
                                  // nv end

    /// Populates the \p cache for the given \p prim, \p time and \p
    /// requestedBits.
    ///
    /// This method is expected to be called from multiple threads.
    virtual void UpdateForTime(UsdPrim const& prim,
                               SdfPath const& cachePath, 
                               UsdTimeCode time,
                               HdDirtyBits requestedBits,
                               UsdImagingInstancerContext const* 
                                   instancerContext = NULL) const = 0;

    // ---------------------------------------------------------------------- //
    /// \name Change Processing 
    // ---------------------------------------------------------------------- //

    /// Returns a bit mask of attributes to be updated, or
    /// HdChangeTracker::AllDirty if the entire prim must be resynchronized.
    ///
    /// \p changedFields contains a list of changed scene description fields
    /// for this prim. This may be empty in certain cases, like the addition
    /// of an inert prim spec for the given \p prim.
    ///
    /// The default implementation returns HdChangeTracker::AllDirty if any of
    /// the changed fields are plugin metadata fields, HdChangeTracker::Clean
    /// otherwise.
    USDIMAGING_API
    virtual HdDirtyBits ProcessPrimChange(UsdPrim const& prim,
                                          SdfPath const& cachePath,
                                          TfTokenVector const& changedFields);

    /// Returns a bit mask of attributes to be updated, or
    /// HdChangeTracker::AllDirty if the entire prim must be resynchronized.
    virtual HdDirtyBits ProcessPropertyChange(UsdPrim const& prim,
                                              SdfPath const& cachePath,
                                              TfToken const& propertyName) = 0;

    /// When a PrimResync event occurs, the prim may have been deleted entirely,
    /// adapter plug-ins should override this method to free any per-prim state
    /// that was accumulated in the adapter.
    USDIMAGING_API
    virtual void ProcessPrimResync(SdfPath const& cachePath,
                                   UsdImagingIndexProxy* index);

    /// Removes all associated Rprims and dependencies from the render index
    /// without scheduling them for repopulation. 
    USDIMAGING_API
    virtual void ProcessPrimRemoval(SdfPath const& cachePath,
                                   UsdImagingIndexProxy* index);

    //+NV_CHANGE FRZHANG
    USDIMAGING_API
    virtual void RefreshSkeletonQuery(const SdfPath& primPath, const TfToken& propertyName, UsdImagingIndexProxy* proxy);
    //-NV_CHANGE FRZHANG

    //+NV_CHANGE BRONG
    USDIMAGING_API
    virtual void RefreshAnimQuery(const SdfPath& primPath, const TfToken& propertyName, UsdImagingIndexProxy* proxy);
    //-NV_CHANGE BRONG

    virtual void MarkDirty(UsdPrim const& prim,
                           SdfPath const& cachePath,
                           HdDirtyBits dirty,
                           UsdImagingIndexProxy* index) = 0;

    USDIMAGING_API
    virtual void MarkRefineLevelDirty(UsdPrim const& prim,
                                      SdfPath const& cachePath,
                                      UsdImagingIndexProxy* index);

    USDIMAGING_API
    virtual void MarkReprDirty(UsdPrim const& prim,
                               SdfPath const& cachePath,
                               UsdImagingIndexProxy* index);

    USDIMAGING_API
    virtual void MarkCullStyleDirty(UsdPrim const& prim,
                                    SdfPath const& cachePath,
                                    UsdImagingIndexProxy* index);

    USDIMAGING_API
    virtual void MarkRenderTagDirty(UsdPrim const& prim,
                                    SdfPath const& cachePath,
                                    UsdImagingIndexProxy* index);

    USDIMAGING_API
    virtual void MarkTransformDirty(UsdPrim const& prim,
                                    SdfPath const& cachePath,
                                    UsdImagingIndexProxy* index);

    USDIMAGING_API
    virtual void MarkVisibilityDirty(UsdPrim const& prim,
                                     SdfPath const& cachePath,
                                     UsdImagingIndexProxy* index);

    USDIMAGING_API
    virtual void MarkMaterialDirty(UsdPrim const& prim,
                                   SdfPath const& cachePath,
                                   UsdImagingIndexProxy* index);

    USDIMAGING_API
    virtual void MarkWindowPolicyDirty(UsdPrim const& prim,
                                       SdfPath const& cachePath,
                                       UsdImagingIndexProxy* index);

    // ---------------------------------------------------------------------- //
    /// \name Computations 
    // ---------------------------------------------------------------------- //
    USDIMAGING_API
    virtual void InvokeComputation(SdfPath const& computationPath,
                                   HdExtComputationContext* context);

    // ---------------------------------------------------------------------- //
    /// \name Instancing
    // ---------------------------------------------------------------------- //

    /// Return an array of the categories used by each instance.
    USDIMAGING_API
    virtual std::vector<VtArray<TfToken>>
    GetInstanceCategories(UsdPrim const& prim);

    /// Sample the instancer transform for the given prim.
    /// \see HdSceneDelegate::SampleInstancerTransform()
    USDIMAGING_API
    virtual size_t
    SampleInstancerTransform(UsdPrim const& instancerPrim,
                             SdfPath const& instancerPath,
                             UsdTimeCode time,
                             size_t maxNumSamples,
                             float *sampleTimes,
                             GfMatrix4d *sampleValues);

    /// Sample the primvar for the given prim.
    /// \see HdSceneDelegate::SamplePrimvar()
    USDIMAGING_API
    virtual size_t
    SamplePrimvar(UsdPrim const& usdPrim,
                  SdfPath const& cachePath,
                  TfToken const& key,
                  UsdTimeCode time,
                  size_t maxNumSamples, 
                  float *sampleTimes,
                  VtValue *sampleValues);

    /// Get the subdiv tags for this prim.
    USDIMAGING_API
    virtual PxOsdSubdivTags GetSubdivTags(UsdPrim const& usdPrim,
                                          SdfPath const& cachePath,
                                          UsdTimeCode time) const;

    // ---------------------------------------------------------------------- //
    /// \name Nested instancing support
    // ---------------------------------------------------------------------- //

    // NOTE: This method is currently only used by PointInstancer
    // style instances, and not instanceable-references.

    /// Returns the transform of \p protoInstancerPath relative to
    /// \p instancerPath. \p instancerPath must be managed by this
    /// adapter.
    USDIMAGING_API
    virtual GfMatrix4d GetRelativeInstancerTransform(
        SdfPath const &instancerPath,
        SdfPath const &protoInstancerPath,
        UsdTimeCode time) const;

    // ---------------------------------------------------------------------- //
    /// \name Selection
    // ---------------------------------------------------------------------- //

    USDIMAGING_API
    virtual SdfPath GetScenePrimPath(SdfPath const& cachePath,
                                     int instanceIndex,
                                     HdInstancerContext *instancerCtx) const;

    // Add the given usdPrim to the HdSelection object, to mark it for
    // selection highlighting. cachePath is the path of the object referencing
    // this adapter.
    //
    // If an instance index is provided to Delegate::PopulateSelection, it's
    // interpreted as a hydra instance index and left unchanged (to make
    // picking/selection round-tripping work).  Otherwise, instance adapters
    // will build up a composite instance index range at each level.
    //
    // Consider:
    //   /World/A (2 instances)
    //           /B (2 instances)
    //             /C (gprim)
    // ... to select /World/A, instance 0, you want to select cartesian
    // coordinates (0, *) -> (0, 0) and (0, 1).  The flattened representation
    // of this is:
    //   index = coordinate[0] * instance_count[1] + coordinate[1]
    // Likewise, for one more nesting level you get:
    //   index = c[0] * count[1] * count[2] + c[1] * count[2] + c[2]
    // ... since the adapter for /World/A has no idea what count[1+] are,
    // this needs to be built up.  The delegate initially sets
    // parentInstanceIndices to [].  /World/A sets this to [0].  /World/A/B,
    // since it is selecting *, adds all possible instance indices:
    // 0 * 2 + 0 = 0, 0 * 2 + 1 = 1. /World/A/B/C is a gprim, and adds
    // instances [0,1] to its selection.
    USDIMAGING_API
    virtual bool PopulateSelection(
        HdSelection::HighlightMode const& highlightMode,
        SdfPath const &cachePath,
        UsdPrim const &usdPrim,
        int const hydraInstanceIndex,
        VtIntArray const &parentInstanceIndices,
        HdSelectionSharedPtr const &result) const;

    // ---------------------------------------------------------------------- //
    /// \name Texture resources
    // ---------------------------------------------------------------------- //

    USDIMAGING_API
    virtual HdTextureResource::ID
    GetTextureResourceID(UsdPrim const& usdPrim, SdfPath const &id,
                         UsdTimeCode time, size_t salt) const;

    USDIMAGING_API
    virtual HdTextureResourceSharedPtr
    GetTextureResource(UsdPrim const& usdPrim, SdfPath const &id,
                       UsdTimeCode time) const;

    // ---------------------------------------------------------------------- //
    /// \name Volume field information
    // ---------------------------------------------------------------------- //

    USDIMAGING_API
    virtual HdVolumeFieldDescriptorVector
    GetVolumeFieldDescriptors(UsdPrim const& usdPrim, SdfPath const &id,
                              UsdTimeCode time) const;

    // ---------------------------------------------------------------------- //
    /// \name Utilities 
    // ---------------------------------------------------------------------- //

    /// The root transform provided by the delegate. 
    USDIMAGING_API
    GfMatrix4d GetRootTransform() const;

    /// A thread-local XformCache provided by the delegate.
    USDIMAGING_API
    void SetDelegate(UsdImagingDelegate* delegate);

    // #nv begin #clean-property-invalidation
    /// A back-pointer to the scene delegate.
    USDIMAGING_API
    UsdImagingDelegate* GetDelegate() const;
    // nv end

    USDIMAGING_API
    bool IsChildPath(SdfPath const& path) const;
    
    /// Returns true if the given prim is visible, taking into account inherited
    /// visibility values. Inherited values are strongest, Usd has no notion of
    /// "super vis/invis".
    USDIMAGING_API
    bool GetVisible(UsdPrim const& prim, UsdTimeCode time) const;

    /// Returns the purpose token for \p prim. If an \p instancerContext is 
    /// provided and the prim doesn't have an explicitly authored or inherited 
    /// purpose, it may inherit the instancerContext's purpose if the instance
    /// has an explicit purpose.
    USDIMAGING_API
    TfToken GetPurpose(
        UsdPrim const& prim, 
        UsdImagingInstancerContext const* instancerContext) const;

    /// Returns the purpose token for \p prim, but only if it is inheritable 
    /// by child prims (i.e. it is an explicitly authored purpose on the prim
    /// itself or one of the prim's ancestors), otherwise it returns the empty 
    /// token.
    USDIMAGING_API
    TfToken GetInheritablePurpose(UsdPrim const& prim) const;

    /// Fetches the transform for the given prim at the given time from a
    /// pre-computed cache of prim transforms. Requesting transforms at
    /// incoherent times is currently inefficient.
    USDIMAGING_API
    GfMatrix4d GetTransform(UsdPrim const& prim, UsdTimeCode time,
                            bool ignoreRootTransform = false) const;

    /// Samples the transform for the given prim.
    USDIMAGING_API
    virtual size_t
    SampleTransform(UsdPrim const& prim,
                    SdfPath const& cachePath,
                    UsdTimeCode time,
                    size_t maxNumSamples, 
                    float *sampleTimes,
                    GfMatrix4d *sampleValues);

    /// Gets the material path for the given prim, walking up namespace if
    /// necessary.  
    USDIMAGING_API
    SdfPath GetMaterialUsdPath(UsdPrim const& prim) const; 

    /// Gets the model:drawMode attribute for the given prim, walking up
    /// the namespace if necessary.
    USDIMAGING_API
    TfToken GetModelDrawMode(UsdPrim const& prim);

    /// Computes the per-prototype instance indices for a UsdGeomPointInstancer.
    /// XXX: This needs to be defined on the base class, to have access to the
    /// delegate, but it's a clear violation of abstraction.  This call is only
    /// legal for prims of type UsdGeomPointInstancer; in other cases, the
    /// returned array will be empty and the computation will issue errors.
    USDIMAGING_API
    VtArray<VtIntArray> GetPerPrototypeIndices(UsdPrim const& prim,
                                               UsdTimeCode time) const;

    USDIMAGING_API
    virtual VtValue GetMaterialResource(UsdPrim const& prim,
        SdfPath const& cachePath,
        UsdTimeCode time) const;

    // ---------------------------------------------------------------------- //
    /// \name Render Index Compatibility
    // ---------------------------------------------------------------------- //

    /// Returns true if the adapter can be populated into the target index.
    virtual bool IsSupported(UsdImagingIndexProxy const* index) const {
        return true;
    }

protected:
    typedef UsdImagingValueCache::Key Keys;

    template <typename T>
    T _Get(UsdPrim const& prim, TfToken const& attrToken, 
           UsdTimeCode time) const {
        T value;
        prim.GetAttribute(attrToken).Get<T>(&value, time);
        return value;
    }

    template <typename T>
    void _GetPtr(UsdPrim const& prim, TfToken const& key, UsdTimeCode time,
                 T* out) const {
        prim.GetAttribute(key).Get<T>(out, time);
    }

    USDIMAGING_API
    UsdImagingValueCache* _GetValueCache() const;

    USDIMAGING_API
    UsdPrim _GetPrim(SdfPath const& usdPath) const;

    // Returns the prim adapter for the given \p prim, or an invalid pointer if
    // no adapter exists. If \p prim is an instance and \p ignoreInstancing
    // is \c true, the instancing adapter will be ignored and an adapter will
    // be looked up based on \p prim's type.
    USDIMAGING_API
    const UsdImagingPrimAdapterSharedPtr& 
    _GetPrimAdapter(UsdPrim const& prim, bool ignoreInstancing = false) const;

    USDIMAGING_API
    const UsdImagingPrimAdapterSharedPtr& 
    _GetAdapter(TfToken const& adapterKey) const;

    // XXX: Transitional API
    // Returns the instance proxy prim path for a USD-instanced prim, given the
    // instance chain leading to that prim. The paths are sorted from more to
    // less local; the first path is the prim path (possibly in master), then
    // instance paths (possibly in master); the last path is the prim or
    // instance path in the scene.
    USDIMAGING_API
    SdfPath _GetPrimPathFromInstancerChain(
                                    SdfPathVector const& instancerChain) const;

    USDIMAGING_API
    UsdTimeCode _GetTimeWithOffset(float offset) const;

    // Converts \p cachePath to the path in the render index.
    USDIMAGING_API
    SdfPath _ConvertCachePathToIndexPath(SdfPath const& cachePath) const;

    // Converts \p indexPath to the path in the USD stage
    USDIMAGING_API
    SdfPath _ConvertIndexPathToCachePath(SdfPath const& indexPath) const;

    // Returns the material binding purpose from the renderer delegate.
    USDIMAGING_API
    TfToken _GetMaterialBindingPurpose() const;

    // Returns the material context from the renderer delegate.
    USDIMAGING_API
    TfToken _GetMaterialNetworkSelector() const;

    // Returns true if render delegate wants primvars to be filtered based.
    // This will filter the primvars based on the bound material primvar needs.
    USDIMAGING_API
    bool _IsPrimvarFilteringNeeded() const;

    // Returns the shader source type from the render delegate.
    USDIMAGING_API
    TfTokenVector _GetShaderSourceTypes() const;

    // Returns \c true if \p usdPath is included in the scene delegate's
    // invised path list.
    USDIMAGING_API
    bool _IsInInvisedPaths(SdfPath const& usdPath) const;

    // Determines if an attribute is varying and if so, sets the given
    // \p dirtyFlag in the \p dirtyFlags and increments a perf counter. Returns
    // true if the attribute is varying.
    //
    // If \p exists is non-null, _IsVarying will store whether the attribute
    // was found.  If the attribute is not found, it counts as non-varying.
    USDIMAGING_API
    bool _IsVarying(UsdPrim prim, TfToken const& attrName, 
           HdDirtyBits dirtyFlag, TfToken const& perfToken,
           HdDirtyBits* dirtyFlags, bool isInherited,
           bool* exists = nullptr) const;

    // Determines if the prim's transform (CTM) is varying and if so, sets the 
    // given \p dirtyFlag in the \p dirtyFlags and increments a perf counter. 
    // Returns true if the prim's transform is varying.
    USDIMAGING_API
    bool _IsTransformVarying(UsdPrim prim,
                             HdDirtyBits dirtyFlag,
                             TfToken const& perfToken,
                             HdDirtyBits* dirtyFlags) const;

    // Convenience method for adding or updating a primvar descriptor.
    // Role defaults to empty token (none).
    USDIMAGING_API
    void _MergePrimvar(
        HdPrimvarDescriptorVector* vec,
        TfToken const& name,
        HdInterpolation interp,
        TfToken const& role = TfToken()) const;

    // Convenience method for removing a primvar descriptor.
    USDIMAGING_API
    void _RemovePrimvar(
        HdPrimvarDescriptorVector* vec,
        TfToken const& name) const;

    // Convenience method for computing a primvar. THe primvar will only be
    // added to the list in the valueCache if there is no primvar of the same
    // name already present.  Thus, "local" primvars should be merged before
    // inherited primvars.
    USDIMAGING_API
    void _ComputeAndMergePrimvar(UsdPrim const& prim,
                                 SdfPath const& cachePath,
                                 UsdGeomPrimvar const& primvar,
                                 UsdTimeCode time,
                                 UsdImagingValueCache* valueCache,
                                 HdInterpolation *interpOverride = nullptr)
                                 const;

    // Returns true if the property name has the "primvars:" prefix.
    USDIMAGING_API
    static bool _HasPrimvarsPrefix(TfToken const& propertyName);

    // Convenience methods to figure out what changed about the primvar and
    // return the appropriate dirty bit.
    // Caller can optionally pass in a dirty bit to set for primvar value
    // changes. This is useful for attributes that have a special dirty bit
    // such as normals and widths.
    //
    // Handle USD attributes that are treated as primvars by Hydra. This
    // requires the interpolation to be passed in, as well as the primvar
    // name passed to Hydra.
    USDIMAGING_API
    HdDirtyBits _ProcessNonPrefixedPrimvarPropertyChange(
        UsdPrim const& prim,
        SdfPath const& cachePath,
        TfToken const& propertyName,
        TfToken const& primvarName,
        HdInterpolation const& primvarInterp,
        HdDirtyBits valueChangeDirtyBit = HdChangeTracker::DirtyPrimvar) const;

    // Handle UsdGeomPrimvars that use the "primvars:" prefix, while also
    // accommodating inheritance.
    USDIMAGING_API
    HdDirtyBits _ProcessPrefixedPrimvarPropertyChange(
        UsdPrim const& prim,
        SdfPath const& cachePath,
        TfToken const& propertyName,
        HdDirtyBits valueChangeDirtyBit = HdChangeTracker::DirtyPrimvar,
        bool inherited = true) const;

    virtual void _RemovePrim(SdfPath const& cachePath,
                             UsdImagingIndexProxy* index) = 0;

    USDIMAGING_API
    UsdImaging_CollectionCache& _GetCollectionCache() const;

    USDIMAGING_API
    UsdImaging_CoordSysBindingStrategy::value_type
    _GetCoordSysBindings(UsdPrim const& prim) const;

    USDIMAGING_API
    UsdImaging_InheritedPrimvarStrategy::value_type
    _GetInheritedPrimvars(UsdPrim const& prim) const;

    USDIMAGING_API
    GfInterval _GetCurrentTimeSamplingInterval();

    USDIMAGING_API
    Usd_PrimFlagsConjunction _GetDisplayPredicate() const;

    USDIMAGING_API
    bool _DoesDelegateSupportCoordSys() const;

    //+NV_CHANGE FRZHANG
    USDIMAGING_API
    bool _UseNVGPUSkinningComputations() const;

    USDIMAGING_API
    bool _ShouldGenerateJointMesh() const;
    //-NV_CHANGE FRZHANG

    // Conversion functions between usd and hydra enums.
    USDIMAGING_API
    static HdInterpolation _UsdToHdInterpolation(TfToken const& usdInterp);
    USDIMAGING_API
    static TfToken _UsdToHdRole(TfToken const& usdRole);

private:
    UsdImagingDelegate* _delegate;
};

class UsdImagingPrimAdapterFactoryBase : public TfType::FactoryBase {
public:
    virtual UsdImagingPrimAdapterSharedPtr New() const = 0;
};

template <class T>
class UsdImagingPrimAdapterFactory : public UsdImagingPrimAdapterFactoryBase {
public:
    virtual UsdImagingPrimAdapterSharedPtr New() const
    {
        return std::make_shared<T>();
    }
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_USD_IMAGING_PRIM_ADAPTER_H
