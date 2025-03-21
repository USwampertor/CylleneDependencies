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
#ifndef PXR_IMAGING_HD_VOLUME_H
#define PXR_IMAGING_HD_VOLUME_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/api.h"
#include "pxr/imaging/hd/version.h"
#include "pxr/imaging/hd/rprim.h"

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

using HdVolumePtrConstVector = std::vector<class HdVolume const *>;

/// \class HdVolume
///
/// Hd schema for a renderable volume primitive.
///
class HdVolume : public HdRprim
{
public:
    HD_API
    HdVolume(SdfPath const& id);

    HD_API
    ~HdVolume() override;

    HD_API
    TfTokenVector const & GetBuiltinPrimvarNames() const override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // PXR_IMAGING_HD_VOLUME_H
