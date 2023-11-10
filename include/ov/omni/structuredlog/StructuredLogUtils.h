// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Utilities for structured log functionality.
 */
#pragma once
#include "../extras/UniqueApp.h"
#include "IStructuredLogSettings.h"

namespace omni
{
namespace structuredlog
{

/** Generate the 'dataschema' field for events of a schema.
 *  @param[in] clientName    The client name specified in the schema's metadata.
 *  @param[in] schemaVersion The version specified in the schema's metadata.
 *  @returns The 'dataschema' field that will be emitted with the event.
 */
inline std::string generateDataSchema(const char* clientName, const char* schemaVersion)
{
    return std::string(clientName) + '-' + schemaVersion;
}

} // namespace structuredlog
} // namespace omni
