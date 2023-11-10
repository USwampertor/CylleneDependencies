// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief The L10n interface.
 */
#pragma once

#include "../Framework.h"
#include "IL10n.h"


/** Placeholder for global scope work that needs to be done for localization.  Do not call
 *  this directly.  This is called by @ref CARB_GLOBALS().
 */
#define CARB_LOCALIZATION_GLOBALS()


namespace carb
{
namespace l10n
{

/** Called during client initialization to obtain the globals needed for localization. */
inline void registerLocalizationForClient() noexcept
{
    g_carbLocalization = getFramework()->tryAcquireInterface<IL10n>();
    if (g_carbLocalization != nullptr)
        g_localizationFn = g_carbLocalization->getLocalizedStringFromHash;
}

/** Called during client shutdown to clear out the global state. */
inline void deregisterLocalizationForClient() noexcept
{
    g_carbLocalization = nullptr;
    g_localizationFn = carb::l10n::getLocalizedStringFromHashNoPlugin;
}

} // namespace l10n
} // namespace carb
