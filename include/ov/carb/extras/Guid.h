// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"

#include <cstdio>
#include <string>

CARB_IGNOREWARNING_MSC_WITH_PUSH(4996) // disable complaint about _sscanf etc. on Windows, which hurts portability

namespace carb
{
namespace extras
{
/**
 * A Global Unique Identifier (GUID)
 */
struct Guid
{
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
};

/**
 * Converts string to GUID.
 *
 * @param src   A reference to the source string to be converted to GUID.  This string may
 *              optionally be surrounded with curly braces (ie: "{}").
 * @param guid  A pointer to the output Guid.
 * @returns nothing
 */
inline bool stringToGuid(const std::string& src, Guid* guid)
{
    const char* fmtString = "%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx";


    if (src[0] == '{')
        fmtString = "{%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx}";

    return sscanf(src.c_str(), fmtString, &guid->data1, &guid->data2, &guid->data3, &guid->data4[0], &guid->data4[1],
                  &guid->data4[2], &guid->data4[3], &guid->data4[4], &guid->data4[5], &guid->data4[6],
                  &guid->data4[7]) == 11;
}

/**
 * Converts GUID to string format.
 *
 * @param guid  A pointer to the input GUID to be converted to string.
 * @param src   A reference to the output string.
 * @returns nothing
 */
inline void guidToString(const Guid* guid, std::string& strDst)
{
    char guidCstr[39]; // 32 chars + 4 dashes + 1 null termination
    snprintf(guidCstr, sizeof(guidCstr), "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", guid->data1, guid->data2,
             guid->data3, guid->data4[0], guid->data4[1], guid->data4[2], guid->data4[3], guid->data4[4],
             guid->data4[5], guid->data4[6], guid->data4[7]);

    strDst = guidCstr;
}

constexpr bool operator==(const Guid& lhs, const Guid& rhs)
{

    return lhs.data1 == rhs.data1 && lhs.data2 == rhs.data2 && lhs.data3 == rhs.data3 && lhs.data4[0] == rhs.data4[0] &&
           lhs.data4[1] == rhs.data4[1] && lhs.data4[2] == rhs.data4[2] && lhs.data4[3] == rhs.data4[3] &&
           lhs.data4[4] == rhs.data4[4] && lhs.data4[5] == rhs.data4[5] && lhs.data4[6] == rhs.data4[6] &&
           lhs.data4[7] == rhs.data4[7];
}

} // namespace extras
} // namespace carb

namespace std
{

template <>
struct hash<carb::extras::Guid>
{
    using argument_type = carb::extras::Guid;
    using result_type = std::size_t;
    result_type operator()(argument_type const& s) const noexcept
    {
        // split into two 64 bit words
        uint64_t a = s.data1 | (uint64_t(s.data2) << 32) | (uint64_t(s.data3) << 48);
        uint64_t b = s.data4[0] | (uint64_t(s.data4[1]) << 8) | (uint64_t(s.data4[2]) << 16) |
                     (uint64_t(s.data4[3]) << 24) | (uint64_t(s.data4[4]) << 32) | (uint64_t(s.data4[5]) << 40) |
                     (uint64_t(s.data4[6]) << 48) | (uint64_t(s.data4[6]) << 56);
        // xor together
        return a ^ b;
    }
};

inline std::string to_string(const carb::extras::Guid& s)
{
    std::string str;
    guidToString(&s, str);
    return str;
}

} // namespace std


#define CARB_IS_EQUAL_GUID(rguid1, rguid2) (!memcmp(&(rguid1), &(rguid2), sizeof(carb::extras::Guid)))

CARB_IGNOREWARNING_MSC_POP
