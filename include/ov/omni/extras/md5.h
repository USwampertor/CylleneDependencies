// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once
#include <tuple>
#include <carb/Defines.h>

#ifdef _MSC_VER
// Can't undo this at the end of this file when doing static compilation, the compiler complains
#    pragma warning(disable : 4307)
#endif

namespace MD5
{
constexpr uint32_t K[64] = { 0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613,
                             0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193,
                             0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d,
                             0x02441453, 0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
                             0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122,
                             0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
                             0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244,
                             0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                             0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb,
                             0xeb86d391 };

constexpr uint32_t S[64] = { 7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 5,  9,  14, 20, 5,  9,
                             14, 20, 5,  9,  14, 20, 5,  9,  14, 20, 4,  11, 16, 23, 4,  11, 16, 23, 4,  11, 16, 23,
                             4,  11, 16, 23, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21 };

struct digest
{
    uint8_t u8[16];
};

static constexpr size_t kDigestStringSize = sizeof(digest) * 2;
struct DigestString
{
    char s[kDigestStringSize];
};

struct Data
{
    uint32_t A0 = 0x67452301;
    uint32_t B0 = 0xefcdab89;
    uint32_t C0 = 0x98badcfe;
    uint32_t D0 = 0x10325476;
    static constexpr uint32_t kBlocksize = 64;
    uint8_t buffer[kBlocksize] = {};
};

constexpr uint32_t rotate_left(uint32_t x, int n)
{
    return (x << n) | (x >> (32 - n));
}

constexpr uint32_t packUint32(const uint8_t block[Data::kBlocksize], uint32_t index)
{
    uint32_t u32 = 0;
    for (uint32_t i = 0; i < 4; i++)
    {
        u32 |= uint32_t(block[index + i]) << (8 * i);
    }
    return u32;
}

constexpr void processBlock(Data& data, const uint8_t block[Data::kBlocksize])
{
    uint32_t A = data.A0;
    uint32_t B = data.B0;
    uint32_t C = data.C0;
    uint32_t D = data.D0;

    for (uint32_t i = 0; i < 64; i++)
    {
        uint32_t F = 0;
        uint32_t g = 0;
        switch (i / 16)
        {
            case 0:
                F = (B & C) | ((~B) & D);
                g = i;
                break;
            case 1:
                F = (D & B) | ((~D) & C);
                g = (5 * i + 1) % 16;
                break;
            case 2:
                F = B ^ C ^ D;
                g = (3 * i + 5) % 16;
                break;
            case 3:
                F = C ^ (B | (~D));
                g = (7 * i) % 16;
                break;
        }

        // constexpr won't allow us to do pointer cast
        uint32_t Mg = packUint32(block, g * 4);
        F = F + A + K[i] + Mg;
        A = D;
        D = C;
        C = B;
        B = B + rotate_left(F, S[i]);
    }

    data.A0 += A;
    data.B0 += B;
    data.C0 += C;
    data.D0 += D;
}

constexpr void memcopyConst(uint8_t dst[], const uint8_t src[], size_t count)
{
    for (size_t i = 0; i < count; i++)
        dst[i] = src[i];
}

template <typename T>
constexpr void unpackUint(uint8_t dst[], T u)
{
    for (uint32_t i = 0; i < sizeof(T); i++)
        dst[i] = 0xff & (u >> (i * 8));
}

constexpr void processLastBlock(Data& data, const uint8_t str[], size_t strLen, size_t blockIndex)
{
    auto lastChunkSize = strLen % Data::kBlocksize;
    lastChunkSize = (!lastChunkSize && strLen) ? Data::kBlocksize : lastChunkSize;
    // We need at least 9 available bytes - 1 for 0x80 and 8 bytes for the length
    bool needExtraBlock = (Data::kBlocksize - lastChunkSize) < (sizeof(size_t) + 1);
    bool lastBitInExtraBlock = (lastChunkSize == Data::kBlocksize);

    {
        uint8_t msg[Data::kBlocksize]{};
        memcopyConst(msg, &str[blockIndex * Data::kBlocksize], lastChunkSize);

        if (!lastBitInExtraBlock)
            msg[lastChunkSize] = 0x80;
        if (!needExtraBlock)
            unpackUint(&msg[Data::kBlocksize - 8], strLen * 8);
        processBlock(data, msg);
    }

    if (needExtraBlock)
    {
        uint8_t msg[Data::kBlocksize]{};
        if (lastBitInExtraBlock)
            msg[0] = 0x80;
        unpackUint(&msg[Data::kBlocksize - 8], strLen * 8);
        processBlock(data, msg);
    }
}

constexpr digest createDigest(Data& data)
{
    digest u128 = {};
    unpackUint(&u128.u8[0], data.A0);
    unpackUint(&u128.u8[4], data.B0);
    unpackUint(&u128.u8[8], data.C0);
    unpackUint(&u128.u8[12], data.D0);
    return u128;
}

constexpr digest run(const uint8_t src[], size_t count)
{
    Data data;
    // Compute the number of blocks we need to process. We may need to add a block for padding
    size_t blockCount = 1;
    if (count)
    {
        blockCount = (count + Data::kBlocksize - 1) / Data::kBlocksize;

        for (size_t i = 0; i < blockCount - 1; i++)
            processBlock(data, &src[i * Data::kBlocksize]);
    }

    processLastBlock(data, src, count, blockCount - 1);
    return createDigest(data);
}

template <size_t lengthWithNull>
constexpr static inline digest run(const char (&str)[lengthWithNull])
{
    // Can't do pointer cast at compile time, need to create a copy
    uint8_t unsignedStr[lengthWithNull] = {};
    for (size_t i = 0; i < lengthWithNull; i++)
        unsignedStr[i] = (uint8_t)str[i];

    constexpr size_t length = lengthWithNull - 1;
    return run(unsignedStr, length);
}

// TODO: Move the rest of the MD5 implementation details into the detail namespace in a non-functional change.
namespace detail
{
constexpr static inline char nibbleToHex(uint8_t n)
{
    n &= 0xf;
    const char base = n < 10 ? '0' : ('a' - 10);
    return base + n;
}
} // namespace detail

constexpr static inline DigestString getDigestString(const MD5::digest& d)
{
    DigestString s = {};

    for (uint32_t i = 0; i < carb::countOf(d.u8); i++)
    {
        const uint32_t offset = i * 2;
        s.s[offset + 0] = detail::nibbleToHex(d.u8[i] >> 4);
        s.s[offset + 1] = detail::nibbleToHex(d.u8[i]);
    }

    return s;
}
} // namespace MD5
