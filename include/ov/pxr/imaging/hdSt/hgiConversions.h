//
// Copyright 2019 Pixar
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
#ifndef PXR_IMAGING_HD_ST_HGI_CONVERSIONS_H
#define PXR_IMAGING_HD_ST_HGI_CONVERSIONS_H

#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hd/enums.h"
#include "pxr/imaging/hd/types.h"
#include "pxr/imaging/hgi/types.h"
#include "pxr/imaging/hgi/enums.h"

PXR_NAMESPACE_OPEN_SCOPE

///
/// \class HdStHgiConversions
///
/// Converts from Hd types to Hgi types
///
class HdStHgiConversions
{
public:

    HDST_API
    static HgiFormat GetHgiFormat(HdFormat hdFormat);

    HDST_API
    static HgiFormat GetHgiVertexFormat(HdType hdType);

    HDST_API
    static HgiSamplerAddressMode GetHgiSamplerAddressMode(HdWrap hdWrap);

    HDST_API
    static HgiSamplerFilter GetHgiMagFilter(HdMagFilter hdMagFilter);

    /// The HdMinFilter translates into two Hgi enums for
    /// HgiSamplerDesc::minFilter and HgiSamplerDesc::mipFilter.
    ///
    HDST_API
    static void GetHgiMinAndMipFilter(
        HdMinFilter hdMinFilter,
        HgiSamplerFilter *hgiSamplerFilter, HgiMipFilter *hgiMipFilter);

    HDST_API
    static HgiBorderColor GetHgiBorderColor(HdBorderColor hdBorderColor);

    HDST_API
    static HgiCompareFunction GetHgiCompareFunction(
        HdCompareFunction hdCompareFunc);

    HDST_API
    static HgiStencilOp GetHgiStencilOp(HdStencilOp hdStencilOp);
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
