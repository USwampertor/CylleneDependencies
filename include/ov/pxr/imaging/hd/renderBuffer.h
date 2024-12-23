//
// Copyright 2018 Pixar
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
#ifndef PXR_IMAGING_HD_RENDER_BUFFER_H
#define PXR_IMAGING_HD_RENDER_BUFFER_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/api.h"
#include "pxr/imaging/hd/bprim.h"

#include "pxr/base/gf/vec2i.h"

PXR_NAMESPACE_OPEN_SCOPE




/// \class HdRenderBuffer
///
/// A render buffer is a handle to a data resource that can be rendered into,
/// such as a 2d image for a draw target or auxiliary rendering output.
///
/// The render buffer can be used as an indexed prim, in which case it
/// communicates with the scene delegate to get buffer properties, or it can
/// be created out of band and supplied directly with allocation parameters.
///
/// Render buffers can be targeted by render passes.  They also contain
/// mapping functionality for reading and writing buffer data.
class HdRenderBuffer : public HdBprim
{
public:
    // change tracking for HdRenderBuffer
    enum DirtyBits : HdDirtyBits {
        Clean               = 0,
        DirtyDescription    = 1 << 0,
        AllDirty            = (DirtyDescription)
    };

    HD_API
    HdRenderBuffer(SdfPath const& id);
    HD_API
    ~HdRenderBuffer() override;

    // ---------------------------------------------------------------------- //
    /// \name Prim API
    // ---------------------------------------------------------------------- //

    /// Get initial invalidation state.
    HD_API
    HdDirtyBits GetInitialDirtyBitsMask() const override;

    /// Get allocation information from the scene delegate.
    HD_API
    void Sync(HdSceneDelegate *sceneDelegate,
              HdRenderParam *renderParam,
              HdDirtyBits *dirtyBits) override;

    /// Deallocate before deletion.
    HD_API
    void Finalize(HdRenderParam *renderParam) override;

    // ---------------------------------------------------------------------- //
    /// \name Renderbuffer API
    // ---------------------------------------------------------------------- //

    /// Allocate a buffer.  Can be called from Sync(), or directly.
    /// If the buffer has already been allocated, calling Allocate() again
    /// will destroy the old buffer and allocate a new one.
    ///
    /// A negative dimension or invalid format will cause an allocation error.
    /// If the requested buffer can't be allocated, the function will return
    /// false.
    virtual bool Allocate(GfVec3i const& dimensions,
                          HdFormat format,
                          bool multiSampled) = 0;

    /// Get the buffer's width.
    virtual unsigned int GetWidth() const = 0;
    /// Get the buffer's height.
    virtual unsigned int GetHeight() const = 0;
    /// Get the buffer's depth.
    virtual unsigned int GetDepth() const = 0;
    /// Get the buffer's per-pixel format.
    virtual HdFormat GetFormat() const = 0;
    /// Get whether the buffer is multisampled.
    virtual bool IsMultiSampled() const = 0;

    /// Map the buffer for reading.
    virtual void* Map() = 0;
    /// Unmap the buffer. It is no longer safe to read from the buffer.
    virtual void Unmap() = 0;
    /// Return whether the buffer is currently mapped by anybody.
    virtual bool IsMapped() const = 0;

    /// Resolve the buffer so that reads reflect the latest writes.
    ///
    /// Some buffer implementations may defer final processing of writes until
    /// a buffer is read, for efficiency; examples include OpenGL MSAA or
    /// multi-sampled raytraced buffers.
    virtual void Resolve() = 0;

    /// Return whether the buffer is converged (whether the renderer is
    /// still adding samples or not).
    virtual bool IsConverged() const = 0;

    /// This optional API returns a (type-erased) resource that backs this
    /// render buffer. For example, a render buffer implementation may allocate
    /// a gpu texture that holds the data of the buffer. This function allows
    /// other parts of Hydra, such as a HdTask to get access to this resource.
    virtual VtValue GetResource(bool multiSampled) const {return VtValue();}

protected:
    /// Deallocate the buffer, freeing any owned resources.
    virtual void _Deallocate() = 0;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_IMAGING_HD_RENDER_BUFFER_H
