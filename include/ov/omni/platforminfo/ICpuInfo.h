// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Helper interface to retrieve CPU info.
 */
#pragma once

#include "../core/IObject.h"


namespace omni
{
/** Platform and operating system info namespace. */
namespace platforminfo
{

/** Forward declaration of the API layer. */
class ICpuInfo;

/** CPU feature names.  Each feature name is used with ICpuInfo::isFeatureSupported() to
 *  determine if the requested CPU running on the calling system supports the feature.
 *  These feature flags mostly focus on the availability of specific instructions sets
 *  on the host CPU.
 */
enum class OMNI_ATTR("prefix=e") CpuFeature
{
    /** Intel specific features.  These names are largely labeled to match the mnemonics
     *  used in the Intel Instruction Set Programming Reference document from Intel.  For
     *  the most part, only the casing differs and '-' has been converted to an underscore.
     *
     *  @note Many of these features originated with Intel hardware and therefore have
     *        'X86' in their name.  However, many of these features are or can also be
     *        supported on AMD CPUs.  If an AMD CPU is detected, these feature names could
     *        still be valid and the related instructions usable.
     *
     *  @{
     */
    eX86Sse, ///< Intel SSE instructions are supported.
    eX86Sse2, ///< Intel SSE2 instructions are supported.
    eX86Sse3, ///< Intel SSE3 instructions are supported.
    eX86Ssse3, ///< Intel supplementary SSE3 instructions are supported.
    eX86Fma, ///< Fused multiply-add SIMD operations are supported.
    eX86Sse41, ///< Intel SSE4.1 instructions are supported.
    eX86Sse42, ///< Intel SSE4.2 instructions are supported.
    eX86Avx, ///< Intel AVX instructions are supported.
    eX86F16c, ///< 16-bit floating point conversion instructions are supported.
    eX86Popcnt, ///< Instruction for counting set bits are supported.
    eX86Tsc, ///< The `RDTSC` instruction is supported.
    eX86Mmx, ///< Intel MMX instructions are supported.
    eX86Avx2, ///< Intel AVX2 instructions are supported.
    eX86Avx512F, ///< The AVX-512 foundation instructions are supported.
    eX86Avx512Dq, ///< The AVX-512 double and quad word instructions are supported.
    eX86Avx512Ifma, ///< The AVX-512 integer fused multiply-add instructions are supported.
    eX86Avx512Pf, ///< The AVX-512 prefetch instructions are supported.
    eX86Avx512Er, ///< The AVX-512 exponential and reciprocal instructions are supported.
    eX86Avx512Cd, ///< The AVX-512 conflict detection instructions are supported.
    eX86Avx512Bw, ///< The AVX-512 byte and word instructions are supported.
    eX86Avx512Vl, ///< The AVX-512 vector length extensions instructions are supported.
    eX86Avx512_Vbmi, ///< The AVX-512 vector byte manipulation instructions are supported.
    eX86Avx512_Vbmi2, ///< The AVX-512 vector byte manipulation 2 instructions are supported.
    eX86Avx512_Vnni, ///< The AVX-512 vector neural network instructions are supported.
    eX86Avx512_Bitalg, ///< The AVX-512 bit algorithms instructions are supported.
    eX86Avx512_Vpopcntdq, ///< The AVX-512 vector population count instructions are supported.
    eX86Avx512_4Vnniw, ///< The AVX-512 word vector neural network instructions are supported.
    eX86Avx512_4fmaps, ///< The AVX-512 packed single fused multiply-add instructions are supported.
    eX86Avx512_Vp2intersect, ///< The AVX-512 vector pair intersection instructions are supported.
    eX86AvxVnni, ///< The AVX VEX-encoded versions of the neural network instructions are supported.
    eX86Avx512_Bf16, ///< The AVX-512 16-bit floating point vector NN instructions are supported.
    /** @} */

    /** AMD specific features.
     *  @{
     */
    eAmd3DNow, ///< The AMD 3DNow! instruction set is supported.
    eAmd3DNowExt, ///< The AMD 3DNow! extensions instruction set is supported.
    eAmdMmxExt, ///< The AMD MMX extensions instruction set is supported.
    /** @} */

    /** ARM specific features:
     *  @{
     */
    eArmAsimd, ///< The advanced SIMD instructions are supported.
    eArmNeon, ///< The ARM Neon instruction set is supported.
    eArmAtomics, ///< The ARMv8 atomic instructions are supported.
    eArmSha, ///< The SHA1 and SHA2 instruction sets are supported.
    eArmCrypto, ///< The ARM AES instructions are supported.
    eArmCrc32, ///< The ARM CRC32 instructions are supported.

    /** @} */
    eFeatureCount, /// Total number of features.  Not a valid feature name.
};

/** Interface to collect information about the CPUs installed in the calling system.  This
 *  can provide some basic information about the CPU(s) and get access to features that are
 *  supported by them.
 */
class ICpuInfo_abi : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.platforminfo.ICpuInfo")>
{
protected:
    /** Retrieves the total number of CPU packages installed on the system.
     *
     *  @returns The total number of CPU packages installed in the system.  A CPU package
     *           is a single physical CPU chip that is connected to a physical socket on
     *           the motherboard.
     *
     *  @remarks A system may have multiple CPUs installed if the motherboard supports it.
     *           At least in the Intel (and compatible) case, there are some restrictions
     *           to doing this - all CPUs must be in the same family, share the same core
     *           count, feature set, and bus speed.  Outside of that, the CPUs do not need
     *           to be identical.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual size_t getCpuPackageCount_abi() noexcept = 0;

    /** Retrieves the total number of physical cores across all CPUs in the system.
     *
     *  @returns The total number of physical cores across all CPUs in the system.  This includes
     *           the sum of all physical cores on all CPU packages.  This will not be zero.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual size_t getTotalPhysicalCoreCount_abi() noexcept = 0;

    /** Retrieves the total number of logical cores across all CPUs in the system.
     *
     *  @returns The total number of logical cores across all CPUs in the system.  This includes
     *           the sum of all logical cores on all CPU packages.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual size_t getTotalLogicalCoreCount_abi() noexcept = 0;

    /** Retrieves the number of physical cores per CPU package in the system.
     *
     *  @returns The total number of physical cores per CPU package.  Since all CPU packages
     *           must have the same core counts, this is a common value to all packages.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual size_t getPhysicalCoresPerPackage_abi() noexcept = 0;

    /** Retrieves the number of logical cores per CPU package in the system.
     *
     *  @returns The total number of logical cores per CPU package.  Since all CPU packages
     *           must have the same core counts, this is a common value to all packages.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual size_t getLogicalCoresPerPackage_abi() noexcept = 0;

    /** Checks if a requested feature is supported by the CPU(s) in the system.
     *
     *  @returns `true` if the requested feature is supported.  Returns `false` otherwise.
     *
     *  @remarks See @ref omni::platforminfo::CpuFeature for more information on the features
     *           that can be queried.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual bool isFeatureSupported_abi(CpuFeature feature) noexcept = 0;

    /** Retrieves the friendly name of a CPU in the system.
     *
     *  @param[in] cpuIndex     The zero based index of the CPU package to retrieve the name
     *                          for.  This should be less than the return value of
     *                          ICpuInfo::getCpuPackageCount().
     *  @returns The friendly name of the requested CPU package.  This string should be suitable
     *           for display to the user.  This will contain a rough outline of the processor
     *           model and architecture.  It may or may not contain the clock speed.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual const char* getPrettyName_abi(size_t cpuIndex) noexcept = 0;

    /** Retrieves the identifier of a CPU in the system.
     *
     *  @param[in] cpuIndex     The zero based index of the CPU package to retrieve the identifier
     *                          for.  This should be less than the return value of
     *                          ICpuInfo::getCpuPackageCount().
     *  @returns The identifier string of the requested CPU package.  This string should be
     *           suitable for display to the user.  This will contain information about the
     *           processor family, vendor, and architecture.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual const char* getIdentifier_abi(size_t cpuIndex) noexcept = 0;

    /** Retrieves the vendor string for a CPU package in the system.
     *
     *  @param[in] cpuIndex     The zero based index of the CPU package to retrieve the vendor
     *                          for.  This should be less than the return value of
     *                          ICpuInfo::getCpuPackageCount().
     *  @returns The name of the vendor as reported by the CPU itself.  This may be something
     *           along the lines of "GenuineIntel" or "AuthenticAMD" for x86_64 architectures,
     *           or the name of the CPU implementer for ARM architectures.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual const char* getVendor_abi(size_t cpuIndex) noexcept = 0;

    /** Note: the mask may be 0 if out of range of 64 bits. */
    /** Retrieves a bit mask for the processor cores in a CPU package in the system.
     *
     *  @param[in] cpuIndex     The zero based index of the CPU package to retrieve the identifier
     *                          for.  This should be less than the return value of
     *                          ICpuInfo::getCpuPackageCount().
     *  @returns A mask identifying which CPU cores the given CPU covers.  A set bit indicates
     *           a core that belongs to the given CPU.  A 0 bit indicates either a core from
     *           another package or a non-existent core.  This may also be 0 if more than 64
     *           cores are present in the system or they are out of range of a single 64-bit
     *           value.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual uint64_t getProcessorMask_abi(size_t cpuIndex) noexcept = 0;
};

} // namespace platforminfo
} // namespace omni

#define OMNI_BIND_INCLUDE_INTERFACE_DECL
#include "ICpuInfo.gen.h"

/** @copydoc omni::platforminfo::ICpuInfo_abi */
class omni::platforminfo::ICpuInfo : public omni::core::Generated<omni::platforminfo::ICpuInfo_abi>
{
};

#define OMNI_BIND_INCLUDE_INTERFACE_IMPL
#include "ICpuInfo.gen.h"
