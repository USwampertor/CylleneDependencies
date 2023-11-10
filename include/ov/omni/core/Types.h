// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief Common data structs and types.
#pragma once

#include "OmniAttr.h"
#include "../../carb/Types.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS
CARB_IGNOREWARNING_MSC_WITH_PUSH(4201) // nonstandard extension used: nameless struct/union
#endif

namespace omni
{
namespace core
{

/** Helper struct to represent a single 2-space vector of unsigned integers.  Each member
 *  of the struct can be accessed in multiple ways including an array and direct accessors
 *  known by multiple names.  Objects of this struct are guaranteed to be only as large as
 *  two 32-bit unsigned integers.
 */
union OMNI_ATTR("vec") UInt2
{
    /** Access to the value members in this object as an array. */
    OMNI_ATTR("no_py") uint32_t data[2]; // must be first for proper { } initialization

    /** Structure of unions containing the possible names of the first and second values
     *  in this object.
     */
    struct
    {
        /** Names for the first data member in this object.  This can be used to access
         *  the value treating it as a Cartesian coordinate (`x`), texture coordinate
         *  (`u`, `s`), or a dimensional size (`w`).  These are all just different names
         *  for the same value that can be used to help with the semantics of an access.
         */
        union
        {
            /** Provides access to the first data member as a Cartesian X coordinate. */
            OMNI_ATTR("init_arg") uint32_t x;
            uint32_t u; ///< Provides access to the first data member as a U texture coordinate.
            uint32_t s; ///< Provides access to the first data member as an S texture coordinate.
            uint32_t w; ///< Provides access to the first data member as a width value.
        };

        /** Names for the second data member in this object.  This can be used to access
         *  the value treating it as a Cartesian coordinate (`y`), texture coordinate
         *  (`v`, `t`), or a dimensional size (`h`).  These are all just different names
         *  for the same value that can be used to help with the semantics of an access.
         */
        union
        {
            /** Provides access to the first data member as a Cartesian Y coordinate. */
            OMNI_ATTR("init_arg") uint32_t y;
            uint32_t v; ///< Provides access to the first data member as a V texture coordinate.
            uint32_t t; ///< Provides access to the first data member as an T texture coordinate.
            uint32_t h; ///< Provides access to the first data member as a height value.
        };
    };
};

static_assert(sizeof(UInt2) == (sizeof(uint32_t) * 2), "unexpected UInt2 size");

/** Helper struct to represent a single 2-space vector of signed integers.  Each member of
 *  the struct can be accessed in multiple ways including an array and direct accessors known
 *  by multiple names.  Objects of this struct are guaranteed to be only as large as two
 *  32-bit signed integers.
 */
union OMNI_ATTR("vec") Int2
{
    /** Access to the value members in this object as an array. */
    OMNI_ATTR("no_py") int32_t data[2]; // must be first for proper { } initialization

    /** Structure of unions containing the possible names of the first and second values
     *  in this object.
     */
    struct
    {
        /** Names for the first data member in this object.  This can be used to access
         *  the value treating it as a Cartesian coordinate (`x`), texture coordinate
         *  (`u`, `s`), or a dimensional size (`w`).  These are all just different names
         *  for the same value that can be used to help with the semantics of an access.
         */
        union
        {
            /** Provides access to the first data member as a Cartesian X coordinate. */
            OMNI_ATTR("init_arg") int32_t x;
            int32_t u; ///< Provides access to the first data member as a U texture coordinate.
            int32_t s; ///< Provides access to the first data member as an S texture coordinate.
            int32_t w; ///< Provides access to the first data member as a width value.
        };

        /** Names for the second data member in this object.  This can be used to access
         *  the value treating it as a Cartesian coordinate (`y`), texture coordinate
         *  (`v`, `t`), or a dimensional size (`h`).  These are all just different names
         *  for the same value that can be used to help with the semantics of an access.
         */
        union
        {
            /** Provides access to the first data member as a Cartesian Y coordinate. */
            OMNI_ATTR("init_arg") int32_t y;
            int32_t v; ///< Provides access to the first data member as a V texture coordinate.
            int32_t t; ///< Provides access to the first data member as an T texture coordinate.
            int32_t h; ///< Provides access to the first data member as a height value.
        };
    };
};

static_assert(sizeof(Int2) == (sizeof(int32_t) * 2), "unexpected Int2 size");

/** Helper struct to represent a single 2-space vector of floating point values.  Each member of
 *  the struct can be accessed in multiple ways including an array and direct accessors known
 *  by multiple names.  Objects of this struct are guaranteed to be only as large as two
 *  32-bit floating point values.
 */
union OMNI_ATTR("vec") Float2
{
    /** Access to the value members in this object as an array. */
    OMNI_ATTR("no_py") float data[2]; // must be first for proper { } initialization

    /** Structure of unions containing the possible names of the first and second values
     *  in this object.
     */
    struct
    {
        /** Names for the first data member in this object.  This can be used to access
         *  the value treating it as a Cartesian coordinate (`x`), texture coordinate
         *  (`u`, `s`), or a dimensional size (`w`).  These are all just different names
         *  for the same value that can be used to help with the semantics of an access.
         */
        union
        {
            /** Provides access to the first data member as a Cartesian X coordinate. */
            OMNI_ATTR("init_arg") float x;
            float u; ///< Provides access to the first data member as a U texture coordinate.
            float s; ///< Provides access to the first data member as an S texture coordinate.
            float w; ///< Provides access to the first data member as a width value.
        };

        /** Names for the second data member in this object.  This can be used to access
         *  the value treating it as a Cartesian coordinate (`y`), texture coordinate
         *  (`v`, `t`), or a dimensional size (`h`).  These are all just different names
         *  for the same value that can be used to help with the semantics of an access.
         */
        union
        {
            /** Provides access to the first data member as a Cartesian Y coordinate. */
            OMNI_ATTR("init_arg") float y;
            float v; ///< Provides access to the first data member as a V texture coordinate.
            float t; ///< Provides access to the first data member as an T texture coordinate.
            float h; ///< Provides access to the first data member as a height value.
        };
    };
};

static_assert(sizeof(Float2) == (sizeof(float) * 2), "unexpected Float2 size");

} // namespace core
} // namespace omni

#ifndef DOXYGEN_SHOULD_SKIP_THIS
CARB_IGNOREWARNING_MSC_POP
#endif

#include "Types.gen.h"
