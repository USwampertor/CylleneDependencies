// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Carbonite math utility functions
#pragma once

#include "../cpp20/Bit.h"
#include "../Defines.h"

#include <array>

#if CARB_COMPILER_MSC
extern "C"
{
    unsigned char _BitScanReverse(unsigned long* _Index, unsigned long _Mask);
    unsigned char _BitScanReverse64(unsigned long* _Index, uint64_t _Mask);
    unsigned char _BitScanForward(unsigned long* _Index, unsigned long _Mask);
    unsigned char _BitScanForward64(unsigned long* _Index, uint64_t _Mask);
}

#    pragma intrinsic(_BitScanReverse)
#    pragma intrinsic(_BitScanReverse64)
#    pragma intrinsic(_BitScanForward)
#    pragma intrinsic(_BitScanForward64)
#elif CARB_COMPILER_GNUC
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

namespace carb
{

/** Namespace for various math helper functions. */
namespace math
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail
{

// The Helper class is specialized by type and size since many intrinsics have different names for different sizes. This
// allows implementation of a helper function in the Helper class for the minimum size supported. Since each size
// inherits from the smaller size, the minimum supported size will be selected. This also means that even though a
// function is implemented in the size==1 specialization, for instance, it must be understood that T may be larger than
// size 1.
template <class T, size_t Size = sizeof(T)>
class Helper;

// Specialization for functions where sizeof(T) >= 1
template <class T>
class Helper<T, 1>
{
public:
    static_assert(std::numeric_limits<T>::is_specialized, "Requires numeric type");
    using Signed = typename std::make_signed_t<T>;
    using Unsigned = typename std::make_unsigned_t<T>;

    static int numLeadingZeroBits(const T& val)
    {
#    if CARB_COMPILER_MSC
        unsigned long result;
        if (_BitScanReverse(&result, (unsigned long)(Unsigned)val))
            return int((sizeof(T) * 8 - 1) - result);
        return int(sizeof(T) * 8);
#    else
        constexpr size_t BitDiff = 8 * (sizeof(unsigned int) - sizeof(T));
        return val ? int(__builtin_clz((unsigned int)(Unsigned)val) - BitDiff) : int(sizeof(T) * 8);
#    endif
    }

    // BitScanForward implementation for 1-4 byte integers.
    static int bitScanForward(const T& val)
    {
#    if CARB_COMPILER_MSC
        unsigned long result;
        if (_BitScanForward(&result, (unsigned long)(Unsigned)val))
        {
            // BitScanForward returns the bit position zero-indexed, but we want to return the bit index + 1 to allow
            // returning 0 to indicate that no bit is set
            return (int)(result + 1);
        }
        return 0;
#    else
        return __builtin_ffs((unsigned int)(Unsigned)val);
#    endif
    }

    // BitScanReverse implementation for 1-4 byte integers.
    static int bitScanReverse(const T& val)
    {
#    if CARB_COMPILER_MSC
        unsigned long result;
        if (_BitScanReverse(&result, (unsigned long)(Unsigned)val))
        {
            // BitScanReverse returns the bit position zero-indexed, but we want to return the bit index + 1 to allow
            // returning 0 to indicate that no bit is set
            return (int)(result + 1);
        }
        return 0;
#    else
        // The most significant set bit is calculated from the total number of bits minus the number of leading zeroes
        return val ? int(int(sizeof(unsigned int) * 8) - __builtin_clz((unsigned int)(Unsigned)val)) : 0;
#    endif
    }
};

// Specialization for functions where sizeof(T) >= 2
template <class T>
class Helper<T, 2> : public Helper<T, 1>
{
public:
    using Base = Helper<T, 1>;
    using typename Base::Signed;
    using typename Base::Unsigned;
};

// Specialization for functions where sizeof(T) >= 4
template <class T>
class Helper<T, 4> : public Helper<T, 2>
{
public:
    using Base = Helper<T, 2>;
    using typename Base::Signed;
    using typename Base::Unsigned;
};

// Specialization for functions where sizeof(T) >= 8
template <class T>
class Helper<T, 8> : public Helper<T, 4>
{
public:
    using Base = Helper<T, 4>;
    using typename Base::Signed;
    using typename Base::Unsigned;
    static int numLeadingZeroBits(const T& val)
    {
#    if CARB_COMPILER_MSC
        unsigned long result;
        if (_BitScanReverse64(&result, (Unsigned)val))
            return int((sizeof(T) * 8 - 1) - result);
        return int(sizeof(T) * 8);
#    else
        constexpr int BitDiff = int(8 * (sizeof(uint64_t) - sizeof(T)));
        static_assert(BitDiff == 0, "Unexpected size");
        return val ? (__builtin_clzll((Unsigned)val) - BitDiff) : int(sizeof(T) * 8);
#    endif
    }

    // BitScanForward implementation for 8 byte integers
    static int bitScanForward(const T& val)
    {
        static_assert(sizeof(T) == sizeof(uint64_t), "Unexpected size");
#    if CARB_COMPILER_MSC
        unsigned long result;
        if (_BitScanForward64(&result, (Unsigned)val))
        {
            // BitScanForward returns the bit position zero-indexed, but we want to return the bit index + 1 to allow
            // returning 0 to indicate that no bit is set
            return (int)(result + 1);
        }
        return 0;
#    else
        return __builtin_ffsll((Unsigned)val);
#    endif
    }

    // BitScanReverse implementation for 8 byte integers
    static int bitScanReverse(const T& val)
    {
        static_assert(sizeof(T) == sizeof(uint64_t), "Unexpected size");
#    if CARB_COMPILER_MSC
        unsigned long result;
        if (_BitScanReverse64(&result, (Unsigned)val))
        {
            // BitScanReverse returns the bit position zero-indexed, but we want to return the bit index + 1 to allow
            // returning 0 to indicate that no bit is set
            return (int)(result + 1);
        }
        return 0;
#    else
        return val ? int(int(sizeof(uint64_t) * 8) - __builtin_clzll((Unsigned)val)) : 0;
#    endif
    }
};

} // namespace detail
#endif

/**
 * Returns whether the given integer value is a power of two.
 *
 * @param[in] val The non-zero integer value. Negative numbers are treated as unsigned values.
 * @returns true if the integer value is a power of two; false otherwise. Undefined behavior arises if \p val is zero.
 */
template <class T>
constexpr bool isPowerOf2(const T& val)
{
    // must be an integer type
    static_assert(std::is_integral<T>::value, "Requires integer type");
    CARB_ASSERT(val != 0); // 0 is undefined
    using Uns = typename std::make_unsigned_t<T>;
    return (Uns(val) & (Uns(val) - 1)) == 0;
}

/**
 * Returns the number of leading zero bits for an integer value.
 *
 * @param[in] val The integer value
 * @returns The number of most-significant zero bits. For a zero value, returns the number of bits for the type T.
 */
template <class T>
int numLeadingZeroBits(const T& val)
{
    // must be an integer type
    static_assert(std::is_integral<T>::value, "Requires integer type");
    return detail::Helper<T>::numLeadingZeroBits(val);
}

/**
 * Searches an integer value from least significant bit to most significant bit for the first set (1) bit.
 *
 * @param[in] val The integer value
 * @return One plus the bit position of the first set bit, or zero if \p val is zero.
 */
template <class T>
int bitScanForward(const T& val)
{
    // must be an integer type
    static_assert(std::numeric_limits<T>::is_integer, "Requires integer type");
    return detail::Helper<T>::bitScanForward(val);
}

/**
 * Searches an integer value from most significant bit to least significant bit for the first set (1) bit.
 *
 * @param[in] val The integer value
 * @return One plus the bit position of the first set bit, or zero if \p val is zero.
 */
template <class T>
int bitScanReverse(const T& val)
{
    // must be an integer type
    static_assert(std::numeric_limits<T>::is_integer, "Requires integer type");
    return detail::Helper<T>::bitScanReverse(val);
}

/**
 * Returns the number of set (1) bits in an integer value.
 *
 * @param[in] val The integer value
 * @return The number of set (1) bits.
 */
template <class T>
int popCount(const T& val)
{
    // must be an integer type
    static_assert(std::numeric_limits<T>::is_integer, "Requires integer type");
    return cpp20::popcount(static_cast<typename std::make_unsigned_t<T>>(val));
}

} // namespace math

} // namespace carb
