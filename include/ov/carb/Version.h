// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Utilities for Carbonite version.
#pragma once

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <type_traits>

namespace carb
{

/**
 * Defines a version consisting of a major and minor version.
 */
struct Version
{
    uint32_t major; //!< The major version.
    uint32_t minor; //!< The minor version.
};
static_assert(std::is_standard_layout<Version>::value, "Invalid assumption");

/**
 * Less-than comparison operator.
 *
 * Compares two versions and reports true if the left version is lower than the right.
 *
 * @note The major and minor versions are compared independently. While the \a number `1.11` is less than the \a number
 *   `1.9`, \a version `1.11` is considered to be higher, so `Version{ 1, 9 } < Version{ 1, 11 }` would be `true`.
 * @param lhs The version on the left side of the operation
 * @param rhs The version on the right side of the operation
 * @returns `true` if \p lhs is a lower version than \p rhs; `false` otherwise.
 */
constexpr bool operator<(const Version& lhs, const Version& rhs) noexcept
{
    if (lhs.major == rhs.major)
    {
        return lhs.minor < rhs.minor;
    }
    return lhs.major < rhs.major;
}

/**
 * Less-than-or-equal comparison operator.
 *
 * Compares two versions and reports true if the left version is lower than or equal to the right.
 *
 * @note The major and minor versions are compared independently. While the \a number `1.11` is less than the \a number
 *   `1.9`, \a version `1.11` is considered to be higher, so `Version{ 1, 9 } <= Version{ 1, 11 }` would be `true`.
 * @param lhs The version on the left side of the operation
 * @param rhs The version on the right side of the operation
 * @returns `true` if \p lhs is a version that is lower than or equal to \p rhs; `false` otherwise.
 */
constexpr bool operator<=(const Version& lhs, const Version& rhs) noexcept
{
    if (lhs.major == rhs.major)
    {
        return lhs.minor <= rhs.minor;
    }
    return lhs.major < rhs.major;
}

/**
 * Equality operator.
 *
 * Compares two versions and reports true if the left version and the right version are equal.
 *
 * @param lhs The version on the left side of the operation
 * @param rhs The version on the right side of the operation
 * @returns `true` if \p lhs is equal to \p rhs; `false` otherwise.
 */
constexpr bool operator==(const Version& lhs, const Version& rhs) noexcept
{
    return lhs.major == rhs.major && lhs.minor == rhs.minor;
}

/**
 * Inequality operator.
 *
 * Compares two versions and reports true if the left version and the right version are not equal.
 *
 * @param lhs The version on the left side of the operation
 * @param rhs The version on the right side of the operation
 * @returns `true` if \p lhs is not equal to \p rhs; `false` otherwise.
 */
constexpr bool operator!=(const Version& lhs, const Version& rhs) noexcept
{
    return !(lhs == rhs);
}

/**
 * Checks two versions to see if they are semantically compatible.
 *
 * For more information on semantic versioning, see https://semver.org/.
 *
 * @warning A major version of `0` is considered to be the "development/experimental" version and `0.x` minor versions
 * may be but are not required to be compatible with each other. This function will consider \p minimum version `0.x` to
 * be semantically compatible to different \p candidate version `0.y`, but will emit a warning to `stderr` if a \p name
 * is provided.
 *
 * @param name An optional name that, if provided, will enable the warning message to `stderr` for `0.x` versions
 *   mentioned above.
 * @param minimum The minimum version required. This is typically the version being tested.
 * @param candidate The version offered. This is typically the version being tested against.
 * @retval true If \p minimum and \p candidate share the same major version and \p candidate has a minor version that is
 *   greater-than or equal to the minor version in \p minimum.
 * @retval false If \p minimum and \p candidate have different major versions or \p candidate has a minor version that
 *   is lower than the minor version requested in \p minimum.
 */
inline bool isVersionSemanticallyCompatible(const char* name, const Version& minimum, const Version& candidate)
{
    if (minimum.major != candidate.major)
    {
        return false;
    }
    else if (minimum.major == 0)
    {
        // Need to special case when major is equal but zero, then any difference in minor makes them
        // incompatible. See http://semver.org for details.
        // the case of version 0.x (major of 0), we are only going to "warn" the user of possible
        // incompatibility when a user asks for 0.x and we have an implementation 0.y (where y > x).
        // see https://nvidia-omniverse.atlassian.net/browse/CC-249
        if (minimum.minor > candidate.minor)
        {
            return false;
        }
        else if (minimum.minor < candidate.minor && name)
        {
            // using CARB_LOG maybe pointless, as logging may not be set up yet.
            fprintf(stderr,
                    "Warning: Possible version incompatibility. Attempting to load %s with version v%" PRIu32
                    ".%" PRIu32 " against v%" PRIu32 ".%" PRIu32 ".\n",
                    name, candidate.major, candidate.minor, minimum.major, minimum.minor);
        }
    }
    else if (minimum.minor > candidate.minor)
    {
        return false;
    }
    return true;
}

} // namespace carb
