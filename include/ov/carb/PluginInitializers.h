// Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Utilities to ease the creation of Carbonite plugins.
#pragma once

#include "Defines.h"

namespace carb
{
#ifndef DOXYGEN_BUILD
namespace detail
{
inline bool& initialized() noexcept
{
    static bool init = false;
    return init;
}
} // namespace detail
#endif
struct Framework;
namespace logging
{
void registerLoggingForClient() noexcept;
void deregisterLoggingForClient() noexcept;
} // namespace logging
namespace profiler
{
void registerProfilerForClient() noexcept;
void deregisterProfilerForClient() noexcept;
} // namespace profiler
namespace assert
{
void registerAssertForClient() noexcept;
void deregisterAssertForClient() noexcept;
} // namespace assert
namespace l10n
{
void registerLocalizationForClient() noexcept;
void deregisterLocalizationForClient() noexcept;
} // namespace l10n

/**
 * Function called automatically at plugin startup to initialize utilities within each plugin.
 */
inline void pluginInitialize()
{
    if (detail::initialized())
        return;
    carb::detail::initialized() = true;

    logging::registerLoggingForClient();
    profiler::registerProfilerForClient();
    assert::registerAssertForClient();
    l10n::registerLocalizationForClient();
}

/**
 * Function called automatically at plugin shutdown to de-initialize utilities within each plugin.
 */
inline void pluginDeinitialize()
{
    if (!detail::initialized())
        return;
    carb::detail::initialized() = false;

    assert::deregisterAssertForClient();
    profiler::deregisterProfilerForClient();
    logging::deregisterLoggingForClient();
    l10n::deregisterLocalizationForClient();
}

} // namespace carb
