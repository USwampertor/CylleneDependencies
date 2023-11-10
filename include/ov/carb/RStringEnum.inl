// Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

// This file is purposely missing #pragma once or any sort of include guard as it is included multiple times.

//! @file
//!
//! @brief Registered String utility enum values. See carb::RString for more info.
#if !defined(RSTRINGENUM_FROM_RSTRING_H) && !defined(RSTRINGENUM_FROM_RSTRING_INL)
#    error This file may only be included from RString.h or RString.inl.
#endif

#if (defined(__INTELLISENSE__) && defined(RSTRINGENUM_FROM_RSTRING_H)) || !defined(ENTRY) || defined(DOXYGEN_BUILD)
namespace carb
{

//! The maximum number of static RString values. Values over this amount are guaranteed to be dynamic.
constexpr size_t kMaxStaticRString = 500;

#    ifndef DOXYGEN_SHOULD_SKIP_THIS
#        define ENTRY(index, name) RS_##name = index,
#        define EMPTY_ENTRY(index, name) name = index,
#        define BUILDING_ENUM 1
#    endif

//! Enum values for pre-defined registered strings.
enum class eRString : unsigned
{
#else
#    define BUILDING_ENUM 0
#endif

    // clang-format off
// For step 4 in the Increasing Version checklist in RStringInternals.inl, copy the block below into the saved-off
// version of RStringInternals.inl
// vvvvvvvvvv

EMPTY_ENTRY(0, Empty) //!< Default static registered string for unassigned RString values. Specifically missing the
                        //!< RS_ prefix because the string does not match the enum name in case RS_Empty is added
                        //!< later.
ENTRY(1, RString) //!< Static registered string describing the RString class.
ENTRY(2, carb) //!< Static registered string describing the carb namespace.
ENTRY(3, omni) //!< Static registered string describing the omni namespace.
ENTRY(4, Carbonite) //!< Static registered string "Carbonite".
ENTRY(5, Omniverse) //!< Static registered string "Omniverse".
ENTRY(6, None) //!< Static registered string "None".
ENTRY(7, null) //!< Static registered string "null".
ENTRY(8, bool) //!< Static registered string "bool".
ENTRY(9, uint8) //!< Static registered string "uint8".
ENTRY(10, uint16) //!< Static registered string "uint16".
ENTRY(11, uint32) //!< Static registered string "uint32".
ENTRY(12, uint64) //!< Static registered string "uint64".
ENTRY(13, int8) //!< Static registered string "int8".
ENTRY(14, int16) //!< Static registered string "int16".
ENTRY(15, int32) //!< Static registered string "int32".
ENTRY(16, int64) //!< Static registered string "int64".
ENTRY(17, float) //!< Static registered string "float".
ENTRY(18, double) //!< Static registered string "double".
ENTRY(19, string) //!< Static registered string "string".
ENTRY(20, charptr) //!< Static registered string "charptr".
ENTRY(21, dictionary) //!< Static registered string "dictionary".
ENTRY(22, variant_pair) //!< Static registered string "variant_pair".
ENTRY(23, variant_array) //!< Static registered string "variant_array".
ENTRY(24, RStringU) //!< Static registered string "RStringU".
ENTRY(25, RStringKey) //!< Static registered string "RStringKey".
ENTRY(26, RStringUKey) //!< Static registered string "RStringUKey".
ENTRY(27, variant_map) //!< Static registered string "variant_map".

// ^^^^^^^^^^
// For step 4 in the Increasing Version checklist in RStringInternals.inl, copy the block above into the saved-off
// version of RStringInternals.inl
// clang-format on

#if BUILDING_ENUM
    RS_Max //!< Must be the last value.
};

static_assert(unsigned(eRString::RS_Max) <= kMaxStaticRString, "Too many static RString values!");
#    undef ENTRY
#    undef EMPTY_ENTRY
} // namespace carb
#endif
#undef BUILDING_ENUM
