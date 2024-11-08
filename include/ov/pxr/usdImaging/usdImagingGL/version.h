//
// Copyright 2017 Pixar
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
#ifndef PXR_USD_IMAGING_USD_IMAGING_GL_VERSION_H
#define PXR_USD_IMAGING_USD_IMAGING_GL_VERSION_H

// 0 -> 1: added IDRenderColor decode and direct Rprim path fetching.
// 1 -> 2: added RenderParams::enableUsdDrawModes
// 2 -> 3: refactor picking API.
// 3 -> 4: Add "instancerContext" to new picking API.
// 4 -> 5: Use UsdImagingGLEngine::_GetSceneDelegate() instead of _delegate.
// 5 -> 6: Use UsdImagingGLEngine::_GetHdEngine() instead of _engine.
// 6 -> 7: Added UsdImagingGLEngine::_GetTaskController() and _IsUsingLegacyImpl()
// 7 -> 8: Added outHitNormal parameter to UsdImagingGLEngine::TestIntersection()
// 8 -> 9: Removed the "HydraDisabled" renderer (i.e. LegacyEngine).
#define USDIMAGINGGL_API_VERSION 9

#endif // PXR_USD_IMAGING_USD_IMAGING_GL_VERSION_H

