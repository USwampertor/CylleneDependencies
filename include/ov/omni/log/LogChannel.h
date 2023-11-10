// Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Utilities for handling logging channels.
#pragma once

#include <cstdint>
#include <vector>

//! Given a channel name (as a string), declares a global variable to identify the channel.
//!
//! This macro must be called at global scope.
//!
//! This macro can be called multiple times in a module (think of it as a forward declaration).
//!
//! @ref OMNI_LOG_ADD_CHANNEL() must be called, only once, to define the properties of the channel (e.g. name,
//! description, etc.).
//!
//! For example, the following declares a channel identified by the `kImageChannel` variable:
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_DECLARE_CHANNEL(kImageLoadChannel);
//!
//! @endcode
//!
//! Later, we can log to that channel:
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_VERBOSE(kImageLoadChannel, "I loaded a cool image: %s", imageFilename);
//!
//! @endcode
//!
//! Above, `kImageChannel` is just a variable describing the channel.  To define the channel name (what the user sees)
//! use @ref OMNI_LOG_ADD_CHANNEL().
#define OMNI_LOG_DECLARE_CHANNEL(varName_) extern omni::log::LogChannelData varName_;

//! Defines the properties of a channel and adds it to a module specific list of channels.
//!
//! This macro must be called at global scope and is expected to run during static initialization.
//!
//! Example usage:
//!
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_ADD_CHANNEL(kImageLoadChannel, "omni.image.load", "Messages when loading an image.");
//!
//! @endcode
//!
//! For each module, this macro should only be called once per channel.
//!
//! To tell the log about a module's channels added during static initialization (i.e. by this macro), call
//! @ref omni::log::addModulesChannels().
//!
//! @ref OMNI_LOG_DECLARE_CHANNEL() does not have to be called before calling this macro.
#define OMNI_LOG_ADD_CHANNEL(varName_, channelName_, description_)                                                     \
    OMNI_LOG_DEFINE_CHANNEL_(varName_, channelName_, description_, true)

//! Defines the properties of a channel.
//!
//! This macro must be called at global scope.
//!
//! Example usage:
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_DEFINE_CHANNEL(kImageLoadChannel, "omni.image.load", "Messages when loading an image.");
//!
//! @endcode
//!
//! For each module, this macro should only be called once per channel.
//!
//! @ref OMNI_LOG_DECLARE_CHANNEL() does not have to be called before calling this macro.
//!
//! Use of this macro is rare (currently used only for unit testing) as it does not add the channel to the log.  See
//! @ref OMNI_LOG_ADD_CHANNEL for a more useful way to define a channel.  Note, per-channel, either call this macro or
//! @ref OMNI_LOG_ADD_CHANNEL once.  Never call both.
#define OMNI_LOG_DEFINE_CHANNEL(varName_, channelName_, description_)                                                  \
    OMNI_LOG_DEFINE_CHANNEL_(varName_, channelName_, description_, false)

//! Implementation detail.  Do not call.
#define OMNI_LOG_DEFINE_CHANNEL_(varName_, channelName_, description_, add_)                                           \
    omni::log::LogChannelData varName_{ channelName_, 0, description_, nullptr };                                      \
    omni::log::LogChannelRegistrar varName_##Registrar{ &varName_, add_ };

#ifndef OMNI_LOG_DEFAULT_CHANNEL

//! The default channel variable to use when no channel is supplied to the `OMNI_LOG_*` macros.
//!
//! By default, the channel variable to use is `kDefaultChannel`.
//!
//! See @rstref{Defining the Default Logging Channel <carb_logging_default_channel>} on how to use this `#define` in
//! practice.
#    define OMNI_LOG_DEFAULT_CHANNEL kDefaultChannel

#endif

namespace omni
{
namespace log
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS

//! Describes a channel.
//!
//! This is an implementation detail.
struct LogChannelData
{
    const char* const name;
    int32_t level;
    const char* const description;
    LogChannelData* next;
};

#endif

//! Returns a per-module list of channels defined during static initialization via @ref OMNI_LOG_ADD_CHANNEL.
//!
//! Call @ref omni::log::addModulesChannels() to iterate over this list and add the channels to the global log.
inline LogChannelData*& getModuleLogChannels() noexcept
{
    static LogChannelData* sHead = nullptr;
    return sHead;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

//! Template to allocate storage for a channel.
//!
//! Do not directly use this object.  Rather call @ref OMNI_LOG_ADD_CHANNEL().
class LogChannelRegistrar
{
public:
    LogChannelRegistrar(LogChannelData* data, bool add) noexcept
    {
        if (add)
        {
            data->next = getModuleLogChannels();
            getModuleLogChannels() = data;
        }
    }
};

#endif

} // namespace log
} // namespace omni

#ifndef DOXYGEN_SHOULD_SKIP_THIS

#    define OMNI_LOG_DECLARE_DEFAULT_CHANNEL_1(chan_) OMNI_LOG_DECLARE_CHANNEL(chan_)
#    define OMNI_LOG_DECLARE_DEFAULT_CHANNEL() OMNI_LOG_DECLARE_DEFAULT_CHANNEL_1(OMNI_LOG_DEFAULT_CHANNEL)

// forward declare the default channel
OMNI_LOG_DECLARE_DEFAULT_CHANNEL()

#endif
