// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once
#include <omni/extras/StringHelpers.h>
#include <carb/RString.h>

#include <string>

namespace omni
{
namespace extras
{

/**
 * Version struct, follows Semantic Versioning 2.0. https://semver.org/
 * Use negative version numbers as wildcard (any version).
 */
struct SemanticVersion
{
    int32_t major;
    int32_t minor;
    int32_t patch;
    std::string prerelase;
    std::string build;

    SemanticVersion() : major{ -1 }, minor{ -1 }, patch{ -1 }, prerelase{ "" }, build{ "" }
    {
    }

    std::string toString(bool includeBuildMetadata = true) const
    {
        if (this->major < 0 && this->minor < 0 && this->patch < 0)
            return "";
        else
        {
            std::ostringstream ss;
            if (this->major >= 0 || this->minor >= 0 || this->patch >= 0)
                ss << this->major << "." << this->minor << "." << this->patch;
            if (!std::string(this->prerelase).empty())
                ss << "-" << this->prerelase;
            if (includeBuildMetadata && !std::string(this->build).empty())
                ss << "+" << this->build;
            return ss.str();
        }
    }

    void replaceNegativesWithZeros()
    {
        if (this->major < 0)
            this->major = 0;
        if (this->minor < 0)
            this->minor = 0;
        if (this->patch < 0)
            this->patch = 0;
    }

    bool isAllZeroes() const
    {
        return this->major == 0 && this->minor == 0 && this->patch == 0;
    }


    static const SemanticVersion getDefault()
    {
        static const SemanticVersion kDefault = SemanticVersion();
        return kDefault;
    }
};


inline bool isAnyVersion(const SemanticVersion& v)
{
    return v.major < 0 && v.minor < 0 && v.patch < 0;
}

inline bool prereleaseCmpLess(const char* x, const char* y)
{
    // SemVer prerelease part comparison according to: https://semver.org/#spec-item-11

    const size_t xLen = strlen(x);
    const size_t yLen = strlen(y);

    //  ("" < "") => false
    if (xLen == 0 && yLen == 0)
        return false;

    // ("abc" < "") => true
    if (yLen == 0)
        return true;

    // ("" < "abc") => false
    if (xLen == 0)
        return false;

    const char* xPartFrom = x;
    const char* yPartFrom = y;
    while (1)
    {
        // Find next '.'. Chunks are: [xPartFrom, xPartTo] and [yPartFrom, yPartTo]
        const char* xPartTo = strchr(xPartFrom, '.');
        xPartTo = xPartTo ? xPartTo : x + xLen;
        const char* yPartTo = strchr(yPartFrom, '.');
        yPartTo = yPartTo ? yPartTo : y + yLen;
        const size_t xPartLen = xPartTo - xPartFrom;
        const size_t yPartLen = yPartTo - yPartFrom;

        // Try to convert to integer until next '.'.
        char* end;
        const long xNum = strtol(xPartFrom, &end, 10);
        const bool xIsNum = (end == xPartTo);
        const long yNum = strtol(yPartFrom, &end, 10);
        const bool yIsNum = (end == yPartTo);


        // "123" < "abc"
        if (xIsNum && !yIsNum)
            return true;
        if (!xIsNum && yIsNum)
            return false;

        if (xIsNum && yIsNum)
        {
            // numerical comparison
            if (xNum != yNum)
                return xNum < yNum;
        }
        else
        {
            // string comparison until next nearest '.'
            const int res = strncmp(xPartFrom, yPartFrom, carb_min(xPartLen, yPartLen));

            // if different "zzz" < "abc", return:
            if (res != 0)
                return res < 0;

            // they are the same, but one longer? "abc" < "abcdef"
            if (xPartLen != yPartLen)
                return xPartLen < yPartLen;
        }

        // Go to the next `.` part
        xPartFrom = xPartTo + 1;
        yPartFrom = yPartTo + 1;

        // Reached the end of both? => they are equal
        if (xPartFrom == x + xLen + 1 && yPartFrom == y + yLen + 1)
            break;

        // x ended first? "abc.def" < "abc.def.xyz"
        if (xPartFrom == x + xLen + 1)
            return true;

        // y ended first?
        if (yPartFrom == y + yLen + 1)
            return false;
    }

    return false;
}

static bool prereleaseCmpLess(const std::string& lhs, const std::string& rhs)
{
    return prereleaseCmpLess(lhs.c_str(), rhs.c_str());
}


template <class VersionT>
constexpr bool versionsCmpLess(const VersionT& lhs, const VersionT& rhs)
{
    if (lhs.major != rhs.major)
        return lhs.major < rhs.major;

    if (lhs.minor != rhs.minor)
        return lhs.minor < rhs.minor;

    if (lhs.patch != rhs.patch)
        return lhs.patch < rhs.patch;

    return prereleaseCmpLess(lhs.prerelase, rhs.prerelase) == 1;
}


constexpr bool operator<(const SemanticVersion& lhs, const SemanticVersion& rhs)
{
    return versionsCmpLess(lhs, rhs);
}

constexpr bool operator==(const SemanticVersion& lhs, const SemanticVersion& rhs)
{
    // Notice that "build metadata" is ignored in version comparison
    return lhs.major == rhs.major && lhs.minor == rhs.minor && lhs.patch == rhs.patch && lhs.prerelase == rhs.prerelase;
}


inline bool stringToVersion(const std::string& str, SemanticVersion& outVersion)
{
    outVersion = SemanticVersion::getDefault();

    // [major].[minor].[patch]-[prerelase]+[build]
    auto versionAndBuild = extras::split(str, '+');

    // Only one `+` allowed in semver
    if (versionAndBuild.size() > 2)
        return false;

    auto versionParts = extras::split(versionAndBuild[0], '-', 2);

    // parse first part: [major].[minor].[patch]
    {
        auto parts = extras::split(versionParts[0], '.');

        if (parts.empty())
            return false;

        // 1.2.3.4 is not semver
        if (parts.size() > 3)
            return false;

        if (!extras::stringToInteger(parts[0], outVersion.major))
            return false;

        if (parts.size() > 1)
        {
            if (!extras::stringToInteger(parts[1], outVersion.minor))
                return false;
        }
        if (parts.size() > 2)
        {
            if (!extras::stringToInteger(parts[2], outVersion.patch))
                return false;
        }
    }

    // parse second part: [prerelase]+[build]
    if (versionParts.size() > 1)
        outVersion.prerelase = versionParts[1];

    if (versionAndBuild.size() > 1)
        outVersion.build = versionAndBuild[1];

    return true;
}

inline SemanticVersion stringToVersionOrDefault(const std::string& str)
{
    SemanticVersion v;
    if (!stringToVersion(str, v))
        v = SemanticVersion::getDefault();
    return v;
}

inline bool gitHashFromOmniverseVersion(const extras::SemanticVersion& version, std::string& outHash)
{
    // Omniverse Flow format: "{version}+{gitbranch}.{number}.{githash}.{build_location}"
    auto parts = extras::split(version.build, '.');
    outHash = "00000000";
    if (parts.size() > 2)
    {
        outHash = parts[2];
        return true;
    }
    return false;
}

} // namespace extras
} // namespace omni
