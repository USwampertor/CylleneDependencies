// Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//!
//! @brief UUIDv4 utilities
#pragma once

#include "../Defines.h"
#include "../cpp17/StringView.h"
#include "../cpp20/Bit.h"
#include "StringSafe.h"
#include "StringUtils.h"

#include <array>
#include <cctype>
#include <cinttypes>
#include <cstring>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <type_traits>


namespace carb
{
namespace extras
{

/**
 * UUIDv4 Unique Identifier (RFC 4122)
 */
class Uuid final
{
public:
    using value_type = std::array<uint8_t, 16>; ///< UUID raw bytes

    /**
     * Initialize empty UUID, 00000000-0000-0000-0000-000000000000
     */
    Uuid() noexcept : m_data{ 0U }
    {
    }

    /**
     * Convert a string to a Uuid using UUID format.
     *
     * Accepts the following formats:
     *   00000000-0000-0000-0000-000000000000
     *   {00000000-0000-0000-0000-000000000000}
     *   urn:uuid:00000000-0000-0000-0000-000000000000
     *
     * @param uuidStr string to try and convert to Uuid from
     */
    Uuid(const std::string& uuidStr) noexcept : m_data{ 0U }
    {
        std::array<uint8_t, 16> data{ 0U };
        carb::cpp17::string_view uuidView;

        if (uuidStr[0] == '{' && uuidStr.size() == 38)
        {
            uuidView = carb::cpp17::string_view(uuidStr.c_str(), uuidStr.size()).substr(1, uuidStr.size() - 2);
        }
        else if (startsWith(uuidStr.c_str(), "urn:uuid:") && uuidStr.size() == 45) // RFC 4122
        {
            uuidView = carb::cpp17::string_view(uuidStr.c_str(), uuidStr.size()).substr(9, uuidStr.size());
        }
        else if (uuidStr.size() == 36)
        {
            uuidView = carb::cpp17::string_view(uuidStr.c_str(), uuidStr.size());
        }

        if (!uuidView.empty())
        {
            CARB_ASSERT(uuidView.size() == 36);

            size_t i = 0;
            size_t j = 0;

            while (i < uuidView.size() - 1 && j < data.size())
            {
                if ((i == 8) || (i == 13) || (i == 18) || (i == 23))
                {
                    if (uuidView[i] == '-')
                    {
                        ++i;
                        continue;
                    }
                }

                if (std::isxdigit(uuidView[i]) && std::isxdigit(uuidView[i + 1]))
                {
                    char buf[3] = { uuidView[i], uuidView[i + 1], '\0' };
                    data[j++] = static_cast<uint8_t>(std::strtoul(buf, nullptr, 16));
                    i += 2;
                }
                else
                {
                    // error parsing, unknown character
                    break;
                }
            }

            // if we parsed the entire view and filled the entire array, copy it
            if (i == uuidView.size() && j == data.size())
            {
                m_data = data;
            }
        }
    }

    /**
     * Create UUIDv4 DCE compatible universally unique identifier.
     */
    static Uuid createV4() noexcept
    {
        Uuid uuidv4;
        std::random_device rd;

        for (size_t i = 0; i < uuidv4.m_data.size(); i += 4)
        {
            // use the entire 32-bits returned by random device
            uint32_t rdata = rd();
            uuidv4.m_data[i + 0] = (rdata >> 0) & 0xff;
            uuidv4.m_data[i + 1] = (rdata >> 8) & 0xff;
            uuidv4.m_data[i + 2] = (rdata >> 16) & 0xff;
            uuidv4.m_data[i + 3] = (rdata >> 24) & 0xff;
        }

        uuidv4.m_data[6] = (uuidv4.m_data[6] & 0x0f) | 0x40; // RFC 4122 for UUIDv4
        uuidv4.m_data[8] = (uuidv4.m_data[8] & 0x3f) | 0x80; // RFC 4122 for UUIDv4

        return uuidv4;
    }

    /**
     * Check if Uuid is empty
     *
     * @return true if Uuid is empty; otherwise, false
     */
    bool isEmpty() const noexcept
    {
        auto data = m_data.data();
        return *data == 0 && memcmp(data, data + 1, m_data.size() - 1) == 0;
    }

    /**
     * Access the binary data of the UUID
     *
     * @return array of UUID data
     */
    const value_type& data() const noexcept
    {
        return m_data;
    }

    /**
     * Compare two UUIDs for equality
     *
     * @param rhs right hand side of ==
     * @return true if uuids are equal; otherwise false
     */
    bool operator==(const Uuid& rhs) const noexcept
    {
        return !memcmp(m_data.data(), rhs.m_data.data(), m_data.size());
    }

    /**
     * Compare two UUIDs for inequality
     *
     * @param rhs right hand side of !=
     * @return true if uuids are not equal; otherwise false
     */
    bool operator!=(const Uuid& rhs) const noexcept
    {
        return !(*this == rhs);
    }

#ifndef DOXYGEN_SHOULD_SKIP_THIS
    /**
     * Convert Uuid to string.
     *
     * @return string of Uuid, empty string if failed to convert
     */
    friend std::string to_string(const Uuid& uuid) noexcept
    {
        // UUID format 00000000-0000-0000-0000-000000000000
        static constexpr char kFmtString[] =
            "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "-%02" SCNx8 "%02" SCNx8 "-%02" SCNx8 "%02" SCNx8
            "-%02" SCNx8 "%02" SCNx8 "-%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8;

        // 32 chars + 4 dashes + 1 null termination
        char strBuffer[37];

        formatString(strBuffer, CARB_COUNTOF(strBuffer), kFmtString, uuid.m_data[0], uuid.m_data[1], uuid.m_data[2],
                     uuid.m_data[3], uuid.m_data[4], uuid.m_data[5], uuid.m_data[6], uuid.m_data[7], uuid.m_data[8],
                     uuid.m_data[9], uuid.m_data[10], uuid.m_data[11], uuid.m_data[12], uuid.m_data[13],
                     uuid.m_data[14], uuid.m_data[15]);

        return std::string(strBuffer);
    }

    /**
     * Overload operator<< for easy use with std streams
     *
     * @param os output stream
     * @param uuid uuid to stream
     * @return output stream
     */
    friend std::ostream& operator<<(std::ostream& os, const Uuid& uuid)
    {
        return os << to_string(uuid);
    }
#endif // DOXYGEN_SHOULD_SKIP_THIS

private:
    value_type m_data;
};

static_assert(std::is_standard_layout<Uuid>::value, "Uuid must be standard layout");
static_assert(std::is_trivially_copyable<Uuid>::value, "Uuid must be trivially copyable");
static_assert(std::is_trivially_move_assignable<Uuid>::value, "Uuid must be move assignable");
static_assert(sizeof(Uuid) == 16, "Uuid must be exactly 16 bytes");

} // namespace extras
} // namespace carb

#ifndef DOXYGEN_SHOULD_SKIP_THIS

namespace std
{

template <>
struct hash<carb::extras::Uuid>
{
    using argument_type = carb::extras::Uuid;
    using result_type = std::size_t;

    result_type operator()(argument_type const& uuid) const noexcept
    {
        // uuid is random bytes, so just XOR
        // bit_cast() won't compile unless sizes match
        auto parts = carb::cpp20::bit_cast<std::array<result_type, 2>>(uuid.data());
        return parts[0] ^ parts[1];
    }
};

} // namespace std

#endif // DOXYGEN_SHOULD_SKIP_THIS
