// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief Implementation of select functions from C++20 `<bit>` library
#pragma once

#include "../cpp17/TypeTraits.h"
#include "../extras/CpuInfo.h"

#ifndef DOXYGEN_SHOULD_SKIP_THIS

// CARB_POPCNT is 1 if the compiler is targeting a CPU with AVX instructions or GCC reports popcnt is available. It is
// undefined at the bottom of the file.
#    if defined(__AVX__) /* MSC/GCC */ || defined(__POPCNT__) /* GCC */
#        define CARB_POPCNT 1
#    else
#        define CARB_POPCNT 0
#    endif

// CARB_LZCNT is 1 if the compiler is targeting a CPU with AVX2 instructions or GCC reports lzcnt is available. It is
// undefined at the bottom of the file.
#    if defined(__AVX2__) /* MSC/GCC */ || defined(__LZCNT__) /* GCC */
#        define CARB_LZCNT 1
#    else
#        define CARB_LZCNT 0
#    endif

#endif

#if CARB_COMPILER_MSC
extern "C"
{
    unsigned int __popcnt(unsigned int value);
    unsigned __int64 __popcnt64(unsigned __int64 value);
    unsigned char _BitScanReverse(unsigned long* _Index, unsigned long _Mask);
    unsigned char _BitScanReverse64(unsigned long* _Index, unsigned __int64 _Mask);
    unsigned char _BitScanForward(unsigned long* _Index, unsigned long _Mask);
    unsigned char _BitScanForward64(unsigned long* _Index, unsigned __int64 _Mask);
#    if CARB_LZCNT
    unsigned int __lzcnt(unsigned int);
    unsigned short __lzcnt16(unsigned short);
    unsigned __int64 __lzcnt64(unsigned __int64);
#    endif
}

#    pragma intrinsic(__popcnt)
#    pragma intrinsic(__popcnt64)
#    pragma intrinsic(_BitScanReverse)
#    pragma intrinsic(_BitScanReverse64)
#    pragma intrinsic(_BitScanForward)
#    pragma intrinsic(_BitScanForward64)
#    if CARB_LZCNT
#        pragma intrinsic(__lzcnt)
#        pragma intrinsic(__lzcnt16)
#        pragma intrinsic(__lzcnt64)
#    endif
#elif CARB_COMPILER_GNUC
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

namespace carb
{

//! Namespace for C++20 features using C++14 semantics.
namespace cpp20
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail
{

// Naive implementation of popcnt for CPUs without built in instructions.
template <class T, std::enable_if_t<std::is_unsigned<T>::value, bool> = true>
int popCountImpl(T val)
{
    int count = 0;
    while (val != 0)
    {
        ++count;
        val = val & (val - 1);
    }
    return count;
}

// The Helper class is specialized by type and size since many intrinsics have different names for different sizes.
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
    // popCount implementation for 1-4 bytes integers
    static int popCount(const T& val)
    {
#    if CARB_COMPILER_MSC
#        if !CARB_POPCNT // Skip the check if we know we have the instruction
        // Only use the intrinsic if it's supported on the CPU
        static extras::CpuInfo cpuInfo;
        if (!cpuInfo.popcntSupported())
        {
            return popCountImpl((Unsigned)val);
        }
        else
#        endif
        {
            return (int)__popcnt((unsigned long)(Unsigned)val);
        }
#    else
        return __builtin_popcount((unsigned long)(Unsigned)val);
#    endif
    }
    static constexpr void propagateHighBit(T& n)
    {
        n |= (n >> 1);
        n |= (n >> 2);
        n |= (n >> 4);
    }

    static int countl_zero(T val)
    {
#    if CARB_LZCNT
#        if CARB_COMPILER_MSC
        return int(__lzcnt16((unsigned short)(Unsigned)val)) - (16 - std::numeric_limits<T>::digits);
#        else
        return int(__builtin_ia32_lzcnt_u16((unsigned short)(Unsigned)val)) - (16 - std::numeric_limits<T>::digits);
#        endif
#    else
#        if CARB_COMPILER_MSC
        unsigned long index;
        constexpr static int diff = std::numeric_limits<unsigned long>::digits - std::numeric_limits<T>::digits;
        return _BitScanReverse(&index, (unsigned long)(Unsigned)val) ?
                   (std::numeric_limits<unsigned long>::digits - 1 - index - diff) :
                   std::numeric_limits<T>::digits;
#        else
        // According to docs, undefined if val == 0
        return val ? __builtin_clz((unsigned int)(Unsigned)val) - (32 - std::numeric_limits<T>::digits) :
                     std::numeric_limits<T>::digits;
#        endif
#    endif
    }

    static int countr_zero(T val)
    {
        if (val == 0)
        {
            return std::numeric_limits<T>::digits;
        }
        else
        {
#    if CARB_COMPILER_MSC
            unsigned long result;
            _BitScanForward(&result, (unsigned long)(Unsigned)val);
            return (int)result;
#    else
            return __builtin_ctz((unsigned int)(Unsigned)val);
#    endif
        }
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
    static constexpr void propagateHighBit(T& n)
    {
        Base::propagateHighBit(n);
        n |= (n >> 8);
    }
};

// Specialization for functions where sizeof(T) >= 4
template <class T>
class Helper<T, 4> : public Helper<T, 2>
{
public:
    using Base = Helper<T, 2>;
    using typename Base::Signed;
    using typename Base::Unsigned;
    static constexpr void propagateHighBit(T& n)
    {
        Base::propagateHighBit(n);
        n |= (n >> 16);
    }

#    if CARB_LZCNT
#        if CARB_COMPILER_MSC
    static int countl_zero(T val)
    {
        static_assert(std::numeric_limits<T>::digits == 32, "Invalid assumption");
        return int(__lzcnt((unsigned int)(Unsigned)val));
    }
#        else
    static int countl_zero(T val)
    {
        static_assert(std::numeric_limits<T>::digits == 32, "Invalid assumption");
        return int(__builtin_ia32_lzcnt_u32((unsigned int)(Unsigned)val));
    }
#        endif
#    endif
};

// Specialization for functions where sizeof(T) >= 8
template <class T>
class Helper<T, 8> : public Helper<T, 4>
{
public:
    using Base = Helper<T, 4>;
    using typename Base::Signed;
    using typename Base::Unsigned;

    // popCount implementation for 8 byte integers
    static int popCount(const T& val)
    {
        static_assert(sizeof(T) == sizeof(uint64_t), "Unexpected size");
#    if CARB_COMPILER_MSC
#        if !CARB_POPCNT // Skip the check if we know we have the instruction
        // Only use the intrinsic if it's supported on the CPU
        static extras::CpuInfo cpuInfo;
        if (!cpuInfo.popcntSupported())
        {
            return popCountImpl((Unsigned)val);
        }
        else
#        endif
        {
            return (int)__popcnt64((Unsigned)val);
        }
#    else
        return __builtin_popcountll((Unsigned)val);
#    endif
    }
    static constexpr void propagateHighBit(T& n)
    {
        Base::propagateHighBit(n);
        n |= (n >> 32);
    }

    static int countl_zero(T val)
    {
#    if CARB_LZCNT
#        if CARB_COMPILER_MSC
        return int(__lzcnt64((Unsigned)val));
#        else
        return int(__builtin_ia32_lzcnt_u64((Unsigned)val));
#        endif
#    else
#        if CARB_COMPILER_MSC
        unsigned long index;
        static_assert(sizeof(val) == sizeof(unsigned __int64), "Invalid assumption");
        return _BitScanReverse64(&index, val) ? std::numeric_limits<T>::digits - 1 - index :
                                                std::numeric_limits<T>::digits;
#        else
        // According to docs, undefined if val == 0
        return val ? __builtin_clzll((Unsigned)val) : std::numeric_limits<T>::digits;
#        endif
#    endif
    }

    static int countr_zero(T val)
    {
        if (val == 0)
        {
            return std::numeric_limits<T>::digits;
        }
        else
        {
#    if CARB_COMPILER_MSC
            unsigned long result;
            _BitScanForward64(&result, (Unsigned)val);
            return (int)result;
#    else
            return __builtin_ctzll((Unsigned)val);
#    endif
        }
    }
};

template <class U, class V>
struct SizeMatches : cpp17::bool_constant<sizeof(U) == sizeof(V)>
{
};

struct NontrivialDummyType
{
    constexpr NontrivialDummyType() noexcept
    {
    }
};
} // namespace detail
#endif

/**
 * Indicates the endianness of all scalar types for the current system.
 *
 * Endianness refers to byte ordering of scalar types larger than one byte. Take for example a 32-bit scalar with the
 * value "1".
 * On a little-endian system, the least-significant ("littlest") bytes are ordered first in memory. "1" would be
 * represented as:
 * @code{.txt}
 * 01 00 00 00
 * @endcode
 *
 * On a big-endian system, the most-significant ("biggest") bytes are ordered first in memory. "1" would be represented
 * as:
 * @code{.txt}
 * 00 00 00 01
 * @endcode
 */
enum class endian
{
#ifdef DOXYGEN_BUILD
    little = 0, //!< An implementation-defined value representing little-endian scalar types.
    big = 1, //!< An implementation-defined value representing big-endian scalar types.
    native = -1 //!< Will be either @ref endian::little or @ref endian::big depending on the target platform.
#elif CARB_COMPILER_GNUC
    little = __ORDER_LITTLE_ENDIAN__,
    big = __ORDER_BIG_ENDIAN__,
    native = __BYTE_ORDER__
#else
    little = 0,
    big = 1,
#    if CARB_X86_64 || CARB_AARCH64
    native = little
#    else
    CARB_UNSUPPORTED_PLATFORM();
#    endif
#endif
};

/**
 * Re-interprets the bits @p src as type `To`.
 *
 * @note The `To` and `From` types must exactly match size and both be trivially copyable.
 *
 * See: https://en.cppreference.com/w/cpp/numeric/bit_cast
 * @tparam To The object type to convert to.
 * @tparam From The (typically inferred) object type to convert from.
 * @param src The source object to reinterpret.
 * @returns The reinterpreted object.
 */
template <class To,
          class From,
          std::enable_if_t<detail::SizeMatches<To, From>::value && std::is_trivially_copyable<From>::value &&
                               std::is_trivially_copyable<To>::value,
                           bool> = false>
/* constexpr */ // Cannot be constexpr without compiler support
To bit_cast(const From& src) CARB_NOEXCEPT
{
    // This union allows us to bypass `dest`'s constructor and just trivially copy into it.
    union
    {
        detail::NontrivialDummyType dummy{};
        To dest;
    } u;
    std::memcpy(&u.dest, &src, sizeof(From));
    return u.dest;
}

/**
 * Checks if a given value is an integral power of 2
 * @see https://en.cppreference.com/w/cpp/numeric/has_single_bit
 * @tparam T An unsigned integral type
 * @param val An unsigned integral value
 * @returns \c true if \p val is not zero and has a single bit set (integral power of two); \c false otherwise.
 */
template <class T, std::enable_if_t<std::is_integral<T>::value && std::is_unsigned<T>::value, bool> = false>
constexpr bool has_single_bit(T val)
{
    return val != T(0) && (val & (val - 1)) == T(0);
}

/**
 * Finds the smallest integral power of two not less than the given value.
 * @see https://en.cppreference.com/w/cpp/numeric/bit_ceil
 * @tparam T An unsigned integral type
 * @param val An unsigned integral value
 * @returns The smallest integral power of two that is not smaller than \p val. Undefined if the resulting value is not
 *  representable in \c T.
 */
template <class T, std::enable_if_t<std::is_integral<T>::value && std::is_unsigned<T>::value, bool> = false>
constexpr T bit_ceil(T val) CARB_NOEXCEPT
{
    if (val <= 1)
        return T(1);

    // Yes, this could be implemented with a `nlz` instruction but cannot be constexpr without compiler support.
    --val;
    detail::Helper<T>::propagateHighBit(val);
    ++val;
    return val;
}

/**
 * Finds the largest integral power of two not greater than the given value.
 * @see https://en.cppreference.com/w/cpp/numeric/bit_floor
 * @tparam T An unsigned integral type
 * @param val An unsigned integral value
 * @returns The largest integral power of two not greater than \p val.
 */
template <class T, std::enable_if_t<std::is_integral<T>::value && std::is_unsigned<T>::value, bool> = false>
constexpr T bit_floor(T val) CARB_NOEXCEPT
{
    // Yes, this could be implemented with a `nlz` instruction but cannot be constexpr without compiler support.
    detail::Helper<T>::propagateHighBit(val);
    return val - (val >> 1);
}

/**
 * Returns the number of 1 bits in the value of x.
 *
 * @note Unlike std::popcount, this function is not constexpr. This is because the compiler intrinsics used to
 * implement this function are not constexpr until C++20, so it was decided to drop constexpr in favor of being able to
 * use compiler intrinsics.
 *
 * @note (Intel/AMD CPUs) This function will check to see if the CPU supports the `popcnt` instruction (Intel Nehalem
 * micro-architecture, 2008; AMD Jaguar micro-architecture, 2013), and if it is not supported, will use a fallback
 * function that is ~85% slower than the `popcnt` instruction. If the compiler indicates that the target CPU has that
 * instruction, the CPU support check can be skipped, saving about 20%. This is accomplished with command-line switches:
 * `/arch:AVX` (or higher) for Visual Studio or `-march=sandybridge` (or several other `-march` options) for GCC.
 *
 * @param[in] val The unsigned integer value to test.
 * @returns The number of 1 bits in the value of \p val.
 */
template <class T, std::enable_if_t<std::is_unsigned<T>::value, bool> = true>
int popcount(T val) CARB_NOEXCEPT
{
    return detail::Helper<T>::popCount(val);
}

/**
 * Returns the number of consecutive 0 bits in the value of val, starting from the most significant bit ("left").
 * @see https://en.cppreference.com/w/cpp/numeric/countl_zero
 *
 * @note Unlike std::countl_zero, this function is not constexpr. This is because the compiler intrinsics used to
 * implement this function are not constexpr until C++20, so it was decided to drop constexpr in favor of being able to
 * use compiler intrinsics.
 *
 * @note (Intel/AMD CPUs) To support backwards compatibility with older CPUs, by default this is implemented with a
 * `bsr` instruction (i386+), that is slightly less performant (~3%) than the more modern `lzcnt` instruction. This
 * function implementation will switch to using `lzcnt` if the compiler indicates that instruction is supported. On
 * Visual Studio this is provided by passing `/arch:AVX2` command-line switch, or on GCC with `-march=haswell` (or
 * several other
 * `-march` options). The `lzcnt` instruction was
 * <a href="https://en.wikipedia.org/wiki/X86_Bit_manipulation_instruction_set">introduced</a> on Intel's Haswell micro-
 * architecture and AMD's Jaguar and Piledriver micro-architectures.
 *
 * @tparam T An unsigned integral type
 * @param val An unsigned integral value
 * @returns The number of consecutive 0 bits in the provided value, starting from the least significant bit.
 */
template <class T, std::enable_if_t<std::is_integral<T>::value && std::is_unsigned<T>::value, bool> = false>
int countl_zero(T val) CARB_NOEXCEPT
{
    return detail::Helper<T>::countl_zero(val);
}

/**
 * Returns the number of consecutive 0 bits in the value of val, starting from the least significant bit ("right").
 * @see https://en.cppreference.com/w/cpp/numeric/countr_zero
 *
 * @note Unlike std::countr_zero, this function is not constexpr. This is because the compiler intrinsics used to
 * implement this function are not constexpr until C++20, so it was decided to drop constexpr in favor of being able to
 * use compiler intrinsics.
 *
 * @tparam T An unsigned integral type
 * @param val An unsigned integral value
 * @returns The number of consecutive 0 bits in the provided value, starting from the least significant bit.
 */
template <class T, std::enable_if_t<std::is_integral<T>::value && std::is_unsigned<T>::value, bool> = false>
int countr_zero(T val) CARB_NOEXCEPT
{
    return detail::Helper<T>::countr_zero(val);
}

} // namespace cpp20
} // namespace carb

#undef CARB_LZCNT
#undef CARB_POPCNT
