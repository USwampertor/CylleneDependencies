// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "Types.h"

namespace carb
{

/**
 * Defines a resource format.
 */
enum class Format
{
    eUnknown,
    eR8_UNORM,
    eR8_SNORM,
    eR8_UINT,
    eR8_SINT,
    eRG8_UNORM,
    eRG8_SNORM,
    eRG8_UINT,
    eRG8_SINT,
    eBGRA8_UNORM,
    eBGRA8_SRGB,
    eRGBA8_UNORM,
    eRGBA8_SNORM,
    eRGBA8_UINT,
    eRGBA8_SINT,
    eRGBA8_SRGB,
    eR16_UNORM,
    eR16_SNORM,
    eR16_UINT,
    eR16_SINT,
    eR16_SFLOAT,
    eRG16_UNORM,
    eRG16_SNORM,
    eRG16_UINT,
    eRG16_SINT,
    eRG16_SFLOAT,
    eRGBA16_UNORM,
    eRGBA16_SNORM,
    eRGBA16_UINT,
    eRGBA16_SINT,
    eRGBA16_SFLOAT,
    eR32_UINT,
    eR32_SINT,
    eR32_SFLOAT,
    eRG32_UINT,
    eRG32_SINT,
    eRG32_SFLOAT,
    eRGB32_UINT,
    eRGB32_SINT,
    eRGB32_SFLOAT,
    eRGBA32_UINT,
    eRGBA32_SINT,
    eRGBA32_SFLOAT,
    eR10_G10_B10_A2_UNORM,
    eR10_G10_B10_A2_UINT,
    eR11_G11_B10_UFLOAT,
    eR9_G9_B9_E5_UFLOAT,
    eB5_G6_R5_UNORM,
    eB5_G5_R5_A1_UNORM,
    eBC1_RGBA_UNORM,
    eBC1_RGBA_SRGB,
    eBC2_RGBA_UNORM,
    eBC2_RGBA_SRGB,
    eBC3_RGBA_UNORM,
    eBC3_RGBA_SRGB,
    eBC4_R_UNORM,
    eBC4_R_SNORM,
    eBC5_RG_UNORM,
    eBC5_RG_SNORM,
    eBC6H_RGB_UFLOAT,
    eBC6H_RGB_SFLOAT,
    eBC7_RGBA_UNORM,
    eBC7_RGBA_SRGB,
    eD16_UNORM,
    eD24_UNORM_S8_UINT,
    eD32_SFLOAT,
    eD32_SFLOAT_S8_UINT_X24,
    // Formats for depth-stencil views
    eR24_UNORM_X8,
    eX24_R8_UINT,
    eX32_R8_UINT_X24,
    eR32_SFLOAT_X8_X24,
    // Formats for sampler-feedback
    eSAMPLER_FEEDBACK_MIN_MIP,
    eSAMPLER_FEEDBACK_MIP_REGION_USED,
    // Little-Endian Formats
    eABGR8_UNORM,
    eABGR8_SRGB,

    // Must be last
    eCount
};

/**
 * Defines a sampling count for a resource.
 */
enum class SampleCount
{
    e1x,
    e2x,
    e4x,
    e8x,
    e16x,
    e32x,
    e64x
};

/**
 * Defines the presentation mode for the rendering system.
 */
enum class PresentMode : uint8_t
{
    eNoTearing, //!< No tearing.
    eAllowTearing //!< Allow tearing.
};

/**
 * Defines a descriptor for clearing color values.
 */
union ClearColorValueDesc
{
    Color<float> rgba32f;
    Color<uint32_t> rgba32ui;
    Color<int32_t> rgba32i;
};

/**
 * Defines a descriptor for clearing depth-stencil values.
 */
struct ClearDepthStencilValueDesc
{
    float depth;
    uint32_t stencil;
};

enum class TextureGamma
{
    eDefault, ///< treat as linear for HDR formats, as sRGB for LDR formats (use e*_SRGB tex format or convert on load)
    eLinear, ///< treat as linear, leaves data unchanged
    eSRGB, ///< treat as sRGB, (use e*_SRGB texture format or convert on load)
    eCount
};

} // namespace carb
