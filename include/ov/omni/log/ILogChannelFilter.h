// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief Provides a base class for a filter for log channel patterns.
#pragma once

#include "../core/IObject.h"
#include "../str/IReadOnlyCString.h"
#include "ILog.h"
#include "../extras/OutArrayUtils.h"

namespace omni
{
//! Namespace for logging functionality.
namespace log
{

class ILogChannelFilter_abi;
class ILogChannelFilter;
//! Read-only object to encapsulate a channel filter's pattern and effects.
//!
//! A channel filter is a pattern matcher.  If a channel's name matches the pattern, the filter can set both the
//! channel's enabled flag and/or level.
class ILogChannelFilter_abi : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.log.ILogChannelFilter")>
{
protected:
    //! Returns the channels pattern.  The returned memory is valid for the lifetime of this object.
    //!
    //! This method is thread safe.
    virtual OMNI_ATTR("c_str, not_null") const char* getFilter_abi() noexcept = 0;

    //! Returns the desired enabled state for this filter.
    //!
    //! All parameters must not be nullptr.
    //!
    //! If *isUsed is false after calling this method, *isEnabled and *behavior should not be used.
    //!
    //! This method is thread safe.
    virtual void getEnabled_abi(OMNI_ATTR("out, not_null") bool* isEnabled,
                                OMNI_ATTR("out, not_null") SettingBehavior* behavior,
                                OMNI_ATTR("out, not_null") bool* isUsed) noexcept = 0;

    //! Returns the desired level for this filter.
    //!
    //! All parameters must not be nullptr.
    //!
    //! If *isUsed is false after calling this method, *level and *behavior should not be used.
    //!
    //! This method is thread safe.
    virtual void getLevel_abi(OMNI_ATTR("out, not_null") Level* level,
                              OMNI_ATTR("out, not_null") SettingBehavior* behavior,
                              OMNI_ATTR("out, not_null") bool* isUsed) noexcept = 0;

    //! Given a channel name, returns if the channel name matches the filter's pattern.
    //!
    //! The matching algorithm used is implementation specific (e.g. regex, glob, etc).
    //!
    //! This method is thread safe.
    virtual bool isMatch_abi(OMNI_ATTR("c_str, not_null") const char* channel) noexcept = 0;
};

} // namespace log
} // namespace omni

#define OMNI_BIND_INCLUDE_INTERFACE_DECL
#include "ILogChannelFilter.gen.h"

//! \copydoc omni::log::ILogChannelFilter_abi
class omni::log::ILogChannelFilter : public omni::core::Generated<omni::log::ILogChannelFilter_abi>
{
};

#define OMNI_BIND_INCLUDE_INTERFACE_IMPL
#include "ILogChannelFilter.gen.h"
