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
#ifndef PXR_USD_IMAGING_USD_IMAGING_TOKENS_H
#define PXR_USD_IMAGING_USD_IMAGING_TOKENS_H

#include "pxr/pxr.h"
#include "pxr/usdImaging/usdImaging/api.h"
#include "pxr/base/tf/staticTokens.h"

PXR_NAMESPACE_OPEN_SCOPE


#define USDIMAGING_TOKENS   \
    ((collectionLightLink, "collection:lightLink")) \
    ((collectionShadowLink, "collection:shadowLink")) \
    ((infoSource, "info:source")) \
    (faceIndexPrimvar)      \
    (faceOffsetPrimvar)     \
    ((primvarsNormals, "primvars:normals")) \
    ((primvarsWidths,  "primvars:widths")) \
    (ptexFaceIndex)         \
    (ptexFaceOffset)        \
    (usdPopulatedPrimCount) \
    (usdVaryingExtent)      \
    (usdVaryingPrimvar)     \
    (usdVaryingTopology)    \
    (usdVaryingVisibility)  \
    (usdVaryingWidths)      \
    (usdVaryingNormals)     \
    (usdVaryingXform)       \
    (usdVaryingTexture)     \
    (uvPrimvar)             \
    (UsdPreviewSurface)     \
    (UsdUVTexture)          \
    (UsdPrimvarReader_float)\
    (UsdPrimvarReader_float2)\
    (UsdPrimvarReader_float3)\
    (UsdPrimvarReader_float4)\
    (UsdPrimvarReader_int)   \
    (UsdTransform2d)

TF_DECLARE_PUBLIC_TOKENS(UsdImagingTokens, USDIMAGING_API, USDIMAGING_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

#endif //PXR_USD_IMAGING_USD_IMAGING_TOKENS_H
