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
#ifndef PXR_IMAGING_HD_ST_FALLBACK_LIGHTING_SHADER_H
#define PXR_IMAGING_HD_ST_FALLBACK_LIGHTING_SHADER_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hdSt/lightingShader.h"

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class HioGlslfx;

/// \class HdSt_FallbackLightingShader
///
/// A shader that provides fallback lighting behavior.
///
class HdSt_FallbackLightingShader : public HdStLightingShader
{
public:
    HDST_API
    HdSt_FallbackLightingShader();
    HDST_API
    ~HdSt_FallbackLightingShader() override;

    // HdStShaderCode overrides
    HDST_API
    ID ComputeHash() const override;
    HDST_API
    std::string GetSource(TfToken const &shaderStageKey) const override;
    HDST_API
    void BindResources(int program,
                       HdSt_ResourceBinder const &binder) override;
    HDST_API
    void UnbindResources(int program,
                         HdSt_ResourceBinder const &binder) override;
    HDST_API
    void AddBindings(HdBindingRequestVector *customBindings) override;

    // HdStLightingShader overrides
    HDST_API
    void SetCamera(GfMatrix4d const &worldToViewMatrix,
                   GfMatrix4d const &projectionMatrix) override;

private:
    std::unique_ptr<HioGlslfx> _glslfx;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_IMAGING_HD_ST_FALLBACK_LIGHTING_SHADER_H
