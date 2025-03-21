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
#ifndef PXR_IMAGING_HD_UNIT_TEST_NULL_RENDER_DELEGATE_H
#define PXR_IMAGING_HD_UNIT_TEST_NULL_RENDER_DELEGATE_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/renderDelegate.h"
#include "pxr/imaging/hd/instancer.h"

PXR_NAMESPACE_OPEN_SCOPE

class Hd_UnitTestNullRenderDelegate final : public HdRenderDelegate
{
public:
    HD_API
    Hd_UnitTestNullRenderDelegate() = default;
    HD_API
    ~Hd_UnitTestNullRenderDelegate() override = default;

    HD_API
    const TfTokenVector &GetSupportedRprimTypes() const override;

    HD_API
    const TfTokenVector &GetSupportedSprimTypes() const override;

    HD_API
    const TfTokenVector &GetSupportedBprimTypes() const override;

    HD_API
    HdRenderParam *GetRenderParam() const override;

    HD_API
    HdResourceRegistrySharedPtr GetResourceRegistry() const override;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Renderpass factory
    ///
    ////////////////////////////////////////////////////////////////////////////

    HD_API
    HdRenderPassSharedPtr CreateRenderPass(HdRenderIndex *index,
                HdRprimCollection const& collection) override;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Instancer Factory
    ///
    ////////////////////////////////////////////////////////////////////////////

    HD_API
    HdInstancer *CreateInstancer(HdSceneDelegate *delegate,
                                 SdfPath const& id) override;

    HD_API
    void DestroyInstancer(HdInstancer *instancer) override;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Prim Factories
    ///
    ////////////////////////////////////////////////////////////////////////////

    HD_API
    HdRprim *CreateRprim(TfToken const& typeId,
                                 SdfPath const& rprimId) override;

    HD_API
    void DestroyRprim(HdRprim *rPrim) override;

    HD_API
    HdSprim *CreateSprim(TfToken const& typeId,
                         SdfPath const& sprimId) override;

    HD_API
    HdSprim *CreateFallbackSprim(TfToken const& typeId) override;

    HD_API
    void DestroySprim(HdSprim *sprim) override;

    HD_API
    HdBprim *CreateBprim(TfToken const& typeId,
                         SdfPath const& bprimId) override;

    HD_API
    HdBprim *CreateFallbackBprim(TfToken const& typeId) override;

    HD_API
    void DestroyBprim(HdBprim *bprim) override;

    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Sync, Execute & Dispatch Hooks
    ///
    ////////////////////////////////////////////////////////////////////////////

    HD_API
    void CommitResources(HdChangeTracker *tracker) override;


    ////////////////////////////////////////////////////////////////////////////
    ///
    /// Commands API
    ///
    ////////////////////////////////////////////////////////////////////////////
    
    HdCommandDescriptors GetCommandDescriptors() const override;

    bool InvokeCommand(
        const TfToken &command,
        const HdCommandArgs &args = HdCommandArgs()) override;



private:
    static const TfTokenVector SUPPORTED_RPRIM_TYPES;
    static const TfTokenVector SUPPORTED_SPRIM_TYPES;
    static const TfTokenVector SUPPORTED_BPRIM_TYPES;

    Hd_UnitTestNullRenderDelegate(
                                const Hd_UnitTestNullRenderDelegate &) = delete;
    Hd_UnitTestNullRenderDelegate &operator =(
                                const Hd_UnitTestNullRenderDelegate &) = delete;

};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_IMAGING_HD_UNIT_TEST_NULL_RENDER_DELEGATE_H
