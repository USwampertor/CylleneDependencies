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
//! @brief Utilities for gathering information about the CPU
#pragma once

#include "../Defines.h"

#include <array>

#if CARB_X86_64
#    if CARB_COMPILER_MSC
extern "C"
{
    void __cpuid(int cpuInfo[4], int function_id);
}

#        pragma intrinsic(__cpuid)
#    elif CARB_COMPILER_GNUC
// for some strange reason, GCC's 'cpuid.h' header does not have an include guard of any
// kind.  This leads to multiple definition errors when the header is included twice in
// a translation unit.  To avoid this, we'll add our own external include guard here.
#        ifndef CARB_CPUID_H_INCLUDED
#            define CARB_CPUID_H_INCLUDED
#            include "cpuid.h"
#        endif
#    else
CARB_UNSUPPORTED_PLATFORM();
#    endif
#endif

namespace carb
{
namespace extras
{

/**
 * Helper class for gathering and querying CPU information for x86 and x64 CPUs
 *
 * On construction this class will query the CPU information provided by CPU ID and then provides helper methods for
 * querying information about the CPU.
 */
class CpuInfo
{
public:
    /**
     * Constructor
     */
    CpuInfo() : m_isValid(true)
    {
#if CARB_X86_64
#    if CARB_COMPILER_MSC
        __cpuid(reinterpret_cast<int32_t*>(m_data.data()), kInfoType);
#    else
        // if __get_cpuid returns 0, the cpu information is not valid.
        int result = __get_cpuid(kInfoType, &m_data[kEax], &m_data[kEbx], &m_data[kEcx], &m_data[kEdx]);
        if (result == 0)
        {
            m_isValid = false;
        }
#    endif
#else
        m_isValid = false;
#endif
    }

    /**
     * Checks if the popcnt instruction is supported
     *
     * @return True if popcnt is supported, false if it is not supported, or the CPU information is not valid.
     */
    bool popcntSupported()
    {
        return m_isValid && m_data[kEcx] & (1UL << kPopCountBit);
    }

private:
    static constexpr uint32_t kInfoType = 0x00000001;
    static constexpr uint8_t kEax = 0;
    static constexpr uint8_t kEbx = 1;
    static constexpr uint8_t kEcx = 2;
    static constexpr uint8_t kEdx = 3;
    static constexpr uint8_t kPopCountBit = 23;

    bool m_isValid;
    std::array<uint32_t, 4> m_data;
};
} // namespace extras
} // namespace carb
