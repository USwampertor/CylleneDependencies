// Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Provides helper functions to manage and evaluate token strings.
 */
#pragma once

#include "../tokens/TokensUtils.h"


/** Namespace for all low level Carbonite functionality. */
namespace carb
{
/** Common namespace for extra helper functions and classes. */
namespace extras
{

/** Registers a new path string alias for replacement with resolvePathAliases().
 *
 *  @param[in] alias    The alias string to be replaced.  This should not contain the replacement
 *                      marker around the name "${}".
 *  @param[in] value    The value to replace the alias string with.
 *  @returns No return value.
 */
inline void registerPathAlias(const char* alias, const char* value)
{
    static carb::tokens::ITokens* tokens = carb::getFramework()->acquireInterface<carb::tokens::ITokens>();

    tokens->setValue(alias, value);
}

/** Unregisters a path string alias.
 *
 *  @param[in] alias    The alias string to be removed.  This should not contain the replacement
 *                      marker around the name "${}".  This should have been previously registered
 *                      with registerPathAlias().
 *  @returns No return value.
 */
inline void unregisterPathAlias(const char* alias)
{
    static carb::tokens::ITokens* tokens = carb::getFramework()->acquireInterface<carb::tokens::ITokens>();

    tokens->removeToken(alias);
}

/** Replaces path alias markers in a path with the full names.
 *
 *  @param[in] srcBuf The path to potentially replace path alias markers in.  Any path
 *                    alias markers in the string must be surrounded by "${" and "}" characters
 *                    (ie: "${exampleMarker}/file.txt").  The markers will only be replaced if a
 *                    path alias using the same marker name ("exampleMarker" in the previous
 *                    example) is currently registered.
 *  @returns A string containing the resolved path if path alias markers were replaced.
 *  @returns A string containing the original path if no path alias markers were found or
 *           replaced.
 *
 *  @remarks This resolves a path string by replacing path alias markers with their full
 *           paths.  Markers must be registered with registerPathAlias() in order to be
 *           replaced.  This will always return a new string, but not all of the markers
 *           may have been replaced in it is unregistered markers were found.
 *
 *  @remarks A replaced path will never end in a trailing path separator.  It is the caller's
 *           responsibility to ensure the marker(s) in the string are appropriately separated
 *           from other path components.  This behavior does however allow for path aliases
 *           to be used to construct multiple path names by piecing together different parts
 *           of a name.
 *
 *  @note This operation is always thread safe.
 */
inline std::string resolvePathAliases(const char* srcBuf)
{
    static carb::tokens::ITokens* tokens = carb::getFramework()->acquireInterface<carb::tokens::ITokens>();

    return carb::tokens::resolveString(tokens, srcBuf);
}

} // namespace extras
} // namespace carb
