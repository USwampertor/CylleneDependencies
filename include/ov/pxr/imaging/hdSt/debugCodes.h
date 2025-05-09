
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
#ifndef PXR_IMAGING_HD_ST_DEBUG_CODES_H
#define PXR_IMAGING_HD_ST_DEBUG_CODES_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/version.h"
#include "pxr/base/tf/debug.h"

PXR_NAMESPACE_OPEN_SCOPE


TF_DEBUG_CODES(
    HDST_DRAW,
    HDST_DRAW_BATCH,
    HDST_FORCE_DRAW_BATCH_REBUILD,
    HDST_DRAW_ITEM_GATHER,
    HDST_DRAWITEMS_CACHE,
    HDST_DISABLE_FRUSTUM_CULLING,
    HDST_DISABLE_MULTITHREADED_CULLING,
    HDST_DUMP_GLSLFX_CONFIG,
    HDST_DUMP_FAILING_SHADER_SOURCE,
    HDST_DUMP_FAILING_SHADER_SOURCEFILE,
    HDST_DUMP_SHADER_SOURCE,
    HDST_DUMP_SHADER_SOURCEFILE,
    HDST_MATERIAL_ADDED,
    HDST_MATERIAL_REMOVED
);


PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_IMAGING_HD_ST_DEBUG_CODES_H
