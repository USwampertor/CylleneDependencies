// Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

#include "VariantUtils.h"
#include "Translator.inl"
#include "VariantUtils.inl"
#include "VariantTypes.inl"

namespace carb
{
namespace variant
{

inline VariantArrayPtr IVariant::createArray(const Variant* p, size_t count)
{
    return VariantArrayPtr{ internalCreateArray(p, count) };
}

inline VariantMapPtr IVariant::createMap()
{
    return VariantMapPtr{ internalCreateMap() };
}

} // namespace variant
} // namespace carb
