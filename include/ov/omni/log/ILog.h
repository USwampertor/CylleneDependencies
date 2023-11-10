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
//! @brief Fast, multi-channel logging.
#pragma once

#include "../core/Api.h"
#include "../core/BuiltIn.h"
#include "../core/IObject.h"
#include "../str/IReadOnlyCString.h"
#include "../log/LogChannel.h"
#include "../extras/OutArrayUtils.h"
#include "../../carb/thread/Util.h"

#include <cstring>
#include <vector>

//! Logs a message at @ref omni::log::Level::eVerbose level.
//!
//! The first argument can be either a channel or the format string.
//!
//! If the first argument is a channel, the second argument is the format string.  Channels can be created with the @ref
//! OMNI_LOG_ADD_CHANNEL macro.
//!
//! For example:
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_ADD_CHANNEL(kImageLoadChannel, "omni.image.load", "Messages when loading an image.");
//!
//!   ...
//!
//!   OMNI_LOG_VERBOSE(kImageLoadChannel, "I loaded a cool image: %s", imageFilename);
//!
//! @endcode
//!
//! If the first argument is the format string, the channel defined by the @ref OMNI_LOG_DEFAULT_CHANNEL is used.
//! Example usage:
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_VERBOSE("This message is going to the default channel: %s", "woo-hoo");
//!
//! @endcode
//!
//! The given format string uses the same tokens as the `printf` family of functions.
//!
//! The @ref omni::log::ILog used to log the message is the log returned by @ref omniGetLogWithoutAcquire().
//!
//! @see @ref OMNI_LOG_INFO, @ref OMNI_LOG_WARN, @ref OMNI_LOG_ERROR, @ref OMNI_LOG_FATAL.
//!
//! @thread_safety This macro is thread safe though may block if `omniGetLogWithAcquire()->isAsync()` is `false`.
#define OMNI_LOG_VERBOSE(channelOrFormat_, ...)                                                                        \
    OMNI_LOG_WRITE(channelOrFormat_, omni::log::Level::eVerbose, ##__VA_ARGS__)

//! Logs a message at @ref omni::log::Level::eInfo level.
//!
//! The first argument can be either a channel or the format string.
//!
//! If the first argument is a channel, the second argument is the format string.  Channels can be created with the @ref
//! OMNI_LOG_ADD_CHANNEL macro.
//!
//! For example:
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_ADD_CHANNEL(kImageLoadChannel, "omni.image.load", "Messages when loading an image.");
//!
//!   ...
//!
//!   OMNI_LOG_INFO(kImageLoadChannel, "I loaded a cool image: %s", imageFilename);
//!
//! @endcode
//!
//! If the first argument is the format string, the channel defined by the @ref OMNI_LOG_DEFAULT_CHANNEL is used.
//! Example usage:
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_INFO("This message is going to the default channel: %s", "woo-hoo");
//!
//! @endcode
//!
//! The given format string uses the same tokens as the `printf` family of functions.
//!
//! The @ref omni::log::ILog used to log the message is the log returned by @ref omniGetLogWithoutAcquire().
//!
//! @see @ref OMNI_LOG_VERBOSE, @ref OMNI_LOG_WARN, @ref OMNI_LOG_ERROR, @ref OMNI_LOG_FATAL.
//!
//! @thread_safety This macro is thread safe though may block if `omniGetLogWithAcquire()->isAsync()` is `false`.
#define OMNI_LOG_INFO(channelOrFormat_, ...) OMNI_LOG_WRITE(channelOrFormat_, omni::log::Level::eInfo, ##__VA_ARGS__)

//! Logs a message at @ref omni::log::Level::eWarn level.
//!
//! The first argument can be either a channel or the format string.
//!
//! If the first argument is a channel, the second argument is the format string.  Channels can be created with the @ref
//! OMNI_LOG_ADD_CHANNEL macro.
//!
//! For example:
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_ADD_CHANNEL(kImageLoadChannel, "omni.image.load", "Messages when loading an image.");
//!
//!   ...
//!
//!   OMNI_LOG_WARN(kImageLoadChannel, "I loaded a cool image: %s", imageFilename);
//!
//! @endcode
//!
//! If the first argument is the format string, the channel defined by the @ref OMNI_LOG_DEFAULT_CHANNEL is used.
//! Example usage:
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_WARN("This message is going to the default channel: %s", "woo-hoo");
//!
//! @endcode
//!
//! The given format string uses the same tokens as the `printf` family of functions.
//!
//! The @ref omni::log::ILog used to log the message is the log returned by @ref omniGetLogWithoutAcquire().
//!
//! @see @ref OMNI_LOG_VERBOSE, @ref OMNI_LOG_INFO, @ref OMNI_LOG_ERROR, @ref OMNI_LOG_FATAL.
//!
//! @thread_safety This macro is thread safe though may block if `omniGetLogWithAcquire()->isAsync()` is `false`.
#define OMNI_LOG_WARN(channelOrFormat_, ...) OMNI_LOG_WRITE(channelOrFormat_, omni::log::Level::eWarn, ##__VA_ARGS__)

//! Logs a message at @ref omni::log::Level::eError level.
//!
//! The first argument can be either a channel or the format string.
//!
//! If the first argument is a channel, the second argument is the format string.  Channels can be created with the @ref
//! OMNI_LOG_ADD_CHANNEL macro.
//!
//! For example:
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_ADD_CHANNEL(kImageLoadChannel, "omni.image.load", "Messages when loading an image.");
//!
//!   ...
//!
//!   OMNI_LOG_ERROR(kImageLoadChannel, "I loaded a cool image: %s", imageFilename);
//!
//! @endcode
//!
//! If the first argument is the format string, the channel defined by the @ref OMNI_LOG_DEFAULT_CHANNEL is used.
//! Example usage:
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_ERROR("This message is going to the default channel: %s", "woo-hoo");
//!
//! @endcode
//!
//! The given format string uses the same tokens as the `printf` family of functions.
//!
//! The @ref omni::log::ILog used to log the message is the log returned by @ref omniGetLogWithoutAcquire().
//!
//! @see @ref OMNI_LOG_VERBOSE, @ref OMNI_LOG_INFO, @ref OMNI_LOG_WARN, @ref OMNI_LOG_FATAL.
//!
//! @thread_safety This macro is thread safe though may block if `omniGetLogWithAcquire()->isAsync()` is `false`.
#define OMNI_LOG_ERROR(channelOrFormat_, ...) OMNI_LOG_WRITE(channelOrFormat_, omni::log::Level::eError, ##__VA_ARGS__)

//! Logs a message at @ref omni::log::Level::eFatal level.
//!
//! The first argument can be either a channel or the format string.
//!
//! If the first argument is a channel, the second argument is the format string.  Channels can be created with the @ref
//! OMNI_LOG_ADD_CHANNEL macro.
//!
//! For example:
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_ADD_CHANNEL(kImageLoadChannel, "omni.image.load", "Messages when loading an image.");
//!
//!   ...
//!
//!   OMNI_LOG_FATAL(kImageLoadChannel, "I loaded a cool image: %s", imageFilename);
//!
//! @endcode
//!
//! If the first argument is the format string, the channel defined by the @ref OMNI_LOG_DEFAULT_CHANNEL is used.
//! Example usage:
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_FATAL("This message is going to the default channel: %s", "woo-hoo");
//!
//! @endcode
//!
//! The given format string uses the same tokens as the `printf` family of functions.
//!
//! The @ref omni::log::ILog used to log the message is the log returned by @ref omniGetLogWithoutAcquire().
//!
//! @rst
//!
//! .. note:: This macro does not terminate the process, it just logs a message.
//!
//! @endrst
//!
//! @see @ref OMNI_LOG_VERBOSE, @ref OMNI_LOG_INFO, @ref OMNI_LOG_WARN, @ref OMNI_LOG_ERROR.
//!
//! @thread_safety This macro is thread safe though may block if `omniGetLogWithAcquire()->isAsync()` is `false`.
#define OMNI_LOG_FATAL(channelOrFormat_, ...) OMNI_LOG_WRITE(channelOrFormat_, omni::log::Level::eFatal, ##__VA_ARGS__)

//! Logs a message.
//!
//! The first argument can be either a channel or the format string.
//!
//! The second argument must be @ref omni::log::Level at which to log the message.
//!
//! If the first argument is a channel, the third argument is the format string.  Channels can be created with the @ref
//! OMNI_LOG_ADD_CHANNEL macro.
//!
//! For example:
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_ADD_CHANNEL(kImageLoadChannel, "omni.image.load", "Messages when loading an image.");
//!
//!   ...
//!
//!   OMNI_LOG_WRITE(kImageLoadChannel, omni::log::Level::eVerbose, "I loaded a cool image: %s", imageFilename);
//!
//! @endcode
//!
//! If the first argument is the format string, the channel defined by the @ref OMNI_LOG_DEFAULT_CHANNEL is used.
//! Example usage:
//!
//! @code{.cpp}
//!
//!   OMNI_LOG_WRITE("This message is going to the default channel: %s", omni::log::Level::eInfo, "woo-hoo");
//!
//! @endcode
//!
//! The given format string uses the same tokens as the `printf` family of functions.
//!
//! The @ref omni::log::ILog used to log the message is the log returned by @ref omniGetLogWithoutAcquire().
//!
//! Rather than using this function, consider using @ref OMNI_LOG_VERBOSE, @ref OMNI_LOG_INFO, @ref OMNI_LOG_WARN, @ref
//! OMNI_LOG_ERROR, @ref OMNI_LOG_FATAL.
//!
//! @thread_safety This macro is thread safe though may block if `omniGetLogWithAcquire()->isAsync()` is `false`.

//! Implementation detail.  Figure out if the first arg is a channel or a format string, check if the channel (or
//! default channel) is enabled, and only then evaluate the given arguments and call into the logger.
#define OMNI_LOG_WRITE(channelOrFormat_, level_, ...)                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        OMNI_LOG_VALIDATE_FORMAT(channelOrFormat_, ##__VA_ARGS__)                                                      \
        if (omni::log::detail::isChannelEnabled(level_, OMNI_LOG_DEFAULT_CHANNEL, channelOrFormat_))                   \
        {                                                                                                              \
            omni::log::ILog* log_ = omniGetLogWithoutAcquire();                                                        \
            if (log_)                                                                                                  \
            {                                                                                                          \
                omni::log::detail::writeLog(OMNI_LOG_DEFAULT_CHANNEL, channelOrFormat_, log_, level_, __FILE__,        \
                                            __func__, __LINE__, ##__VA_ARGS__);                                        \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)

namespace omni
{

//! Multi-channel logging.
namespace log
{

//! Defines if a log channel's setting should be respected or if the global logging system's settings should be used.
enum class OMNI_ATTR("prefix=e") SettingBehavior : uint32_t
{
    eInherit, //!< Use the log system's setting.
    eOverride //!< Use the setting defined by the log channel.
};

//! Reason for a channel update notification.
enum class OMNI_ATTR("prefix=e") ChannelUpdateReason : uint32_t
{
    eChannelAdded, //!< A channel was added.
    eChannelRemoved, //!< A channel was removed.
    eLevelUpdated, //!< The channel's level or level behavior was updated.
    eEnabledUpdated, //!< The channel's enabled flag or enabled behavior was updated.
    eDescriptionUpdated, //!< The channel's description was updated.
};

//! Severity of a message.
enum class OMNI_ATTR("prefix=e") Level : int32_t
{
    //! Verbose level, for detailed diagnostics messages.
    //!
    //! Expect to see some verbose messages on every frame under certain conditions.
    eVerbose = -2,

    //! Info level, this is for informational messages.
    //!
    //! They are usually triggered on state changes and typically we should not see the same
    //! message on every frame.
    eInfo = -1,

    //! Warning level, this is for warning messages.
    //!
    //! Something could be wrong but not necessarily an error.
    //! Therefore anything that could be a problem but cannot be determined to be an error
    //! should fall into this category. This is the default log level threshold, if nothing
    //! else was specified via configuration or startup arguments. This is also the reason
    //! why it has a value of 0 - the default is zero.
    eWarn = 0,

    //! Error level, this is for error messages.
    //!
    //! An error has occurred but the program can continue.
    eError = 1,

    //! Fatal level, this is for messages on unrecoverable errors.
    //!
    //! An error has occurred and the program cannot continue.
    //! After logging such a message the caller should take immediate action to exit the
    //! program or unload the module.
    eFatal = 2,

    //! Internal flag used to disable logging.
    eDisabled = 3,
};

class ILogMessageConsumer_abi;
class ILogMessageConsumer;

class ILogChannelUpdateConsumer_abi;
class ILogChannelUpdateConsumer;

class ILog_abi;
class ILog;

//! Consumes (listens for) log messages.
//!
//! @ref omni::log::ILogMessageConsumer is usually associated with an @ref omni::log::ILog instance.  Add a consumer to
//! an @ref omni::log::ILog object with @ref omni::log::ILog::addMessageConsumer().
class ILogMessageConsumer_abi
    : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.log.ILogMessageConsumer")>
{
protected:
    //! Receives a log message.
    //!
    //! Logging a message from this method results in undefined behavior.
    //!
    //! Accessing the owning @ref omni::log::ILog from this method will lead to undefined behavior.
    //!
    //! The memory pointed to by the provided pointers will remain valid only during the duration of this call.
    //!
    //! @thread_safety This method must be thread safe as the attached @ref omni::log::ILog may send messages to this
    //! object in parallel.
    virtual void onMessage_abi(OMNI_ATTR("c_str, not_null") const char* channel,
                               Level level,
                               OMNI_ATTR("c_str") const char* moduleName,
                               OMNI_ATTR("c_str") const char* fileName,
                               OMNI_ATTR("c_str") const char* functionName,
                               uint32_t lineNumber,
                               OMNI_ATTR("c_str, not_null") const char* msg,
                               carb::thread::ProcessId pid,
                               carb::thread::ThreadId tid,
                               uint64_t timestamp) noexcept = 0;
};

//! Consumes (listens for) state change to one or more @ref omni::log::ILog objects.
//!
//! Add this object to an omni::log::ILog via @ref omni::log::ILog::addChannelUpdateConsumer().
class ILogChannelUpdateConsumer_abi
    : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.log.ILogChannelUpdateConsumer")>
{
protected:
    //! Called when an attached @ref omni::log::ILog's state changes.
    //!
    //! Accessing the given omni::log::ILog from this method is safe.
    //!
    //! If @p name is @c nullptr, the change happened to the global log (i.e. not to a specific channel).
    //!
    //! @thread_safety
    //!   This method must be thread safe as the attached ILogs may send messages to this object in parallel.
    //!
    //!   Updates may come out-of-order and may be spurious.
    virtual void onChannelUpdate_abi(OMNI_ATTR("not_null") ILog* log,
                                     omni::str::IReadOnlyCString* name,
                                     ChannelUpdateReason reason) noexcept = 0;
};

//! Multi-channel logging interface which can write logs to multiple consumers.
//!
//! See the @rstref{Omniverse Logging Guide <carb_logging>} to better understand how logging works from both the user's
//! and developer's point-of-view.
//!
//! In practice, use of this interface is hidden to the user.  Most logging occurs via the following macros:
//!
//! - @ref OMNI_LOG_VERBOSE
//! - @ref OMNI_LOG_INFO
//! - @ref OMNI_LOG_WARN
//! - @ref OMNI_LOG_ERROR
//! - @ref OMNI_LOG_FATAL
//!
//! The macros above internally call @ref omniGetLogWithoutAcquire(), which returns an @ref omni::log::ILog pointer. See
//! @ref omniGetLogWithoutAcquire() for details on how to control which @ref omni::log::ILog pointer is returned.
//!
//! The logging interface defines two concepts: **log channels** and **log consumers**.
//!
//! **Log channels** are identified by a string and represent the idea of a logging "channel". Each channel has a:
//!
//! - Enabled/Disabled flag (see @ref omni::log::ILog::setChannelEnabled()).
//!
//! - Log level at which messages should be ignored (see @ref omni::log::ILog::setChannelLevel()).
//!
//! Each message logged is associated with a single log channel.
//!
//! Each time a message is logged, the channel's settings are checked to see if the message should be filtered out. If
//! the message is not filtered, the logging interface formats the message and passes it to each log message consumer.
//!
//! **Log consumers** (e.g. @ref omni::log::ILogMessageConsumer) are attached to the logging system via @ref
//! omni::log::ILog::addMessageConsumer().  Along with the formatted message, log consumers are passed a bevvy of
//! additional information, such as filename, line number, channel name, message level, etc. The consumer may choose to
//! perform additional filtering at this point. Eventually, it is up to the log consumer to "log" the message to its
//! backing store (e.g. `stdout`).
//!
//! The @ref omni::log::ILog interface itself has a global enable/disabled flag and log level. Each channel can choose
//! to respect the global flags (via @ref omni::log::SettingBehavior::eInherit) or override the global flags with their
//! own (via @ref omni::log::SettingBehavior::eOverride).
//!
//! With these settings, user have fine-grain control over which messages are filtered and where messages are logged.
//!
//! See @ref OMNI_LOG_ADD_CHANNEL() for information on creating and registering log channels.
//!
//! In order to support rich user experiences, the logging system also allows consumers to be notified of internal state
//! changes such as a channel being added, the logging level changing, etc.  See @ref
//! omni::log::ILog::addChannelUpdateConsumer() for details.
class ILog_abi : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.log.ILog")>
{
protected:
    //! Sends the supplied message to all registered @ref omni::log::ILogMessageConsumer objects.
    //!
    //! @param str Must be a `\0` terminated string.
    //!
    //! @param strCharCount The number of characters in @p str (including the terminating `\0`). If @p strCharCount is
    //! 0, its value will be computed by this method.
    virtual OMNI_ATTR("no_py") void log_abi(OMNI_ATTR("c_str, not_null") const char* channel,
                                            Level level,
                                            OMNI_ATTR("c_str") const char* moduleName,
                                            OMNI_ATTR("c_str") const char* fileName,
                                            OMNI_ATTR("c_str") const char* functionName,
                                            uint32_t lineNumber,
                                            OMNI_ATTR("c_str, not_null") const char* str,
                                            uint32_t strCharCount) noexcept = 0;

    //! Formats the supplied message and sends the result to all registered @ref omni::log::ILogMessageConsumer objects.
    virtual OMNI_ATTR("no_py") void logf_abi(OMNI_ATTR("c_str, not_null") const char* channel,
                                             Level level,
                                             OMNI_ATTR("c_str") const char* moduleName,
                                             OMNI_ATTR("c_str") const char* fileName,
                                             OMNI_ATTR("c_str") const char* functionName,
                                             uint32_t lineNumber,
                                             OMNI_ATTR("c_str, not_null") const char* format,
                                             va_list args) noexcept = 0;

    //! Adds the given log consumer to the internal list of log consumers.
    //!
    //! Each message is associated with a single log channel.  When a message is logged by a log channel, the message is
    //! checked to see if it should be filtered.  If not, it is given to the logging system (@ref omni::log::ILog) which
    //! eventually sends the message to each registered @ref omni::log::ILogMessageConsumer.
    //!
    //! A consumer can be registered a single time with a given @ref omni::log::ILog instance but can be registered with
    //! multiple @ref omni::log::ILog instances.
    //!
    //! Each message may be sent to registered consumers in parallel.
    //!
    //! Logging a message from a consumer callback will lead to undefined behavior.
    //!
    //! Calling @ref omni::log::ILog::addMessageConsumer() from @ref omni::log::ILogMessageConsumer::onMessage() will
    //! lead to undefined behavior.
    //!
    //! @thread_safety This method is thread safe.
    virtual OMNI_ATTR("consumer=onMessage_abi") void addMessageConsumer_abi(OMNI_ATTR("not_null")
                                                                                ILogMessageConsumer* consumer) noexcept = 0;

    //! Removes the given consumer from the internal consumer list.
    //!
    //! This method silently accepts `nullptr`.
    //!
    //! This method silently accepts consumers that have not been registered with this object.
    //!
    //! Calling @ref omni::log::ILog::removeMessageConsumer() from omni::log::ILogMessageConsumer::onMessage() will lead
    //! to undefined behavior.
    //!
    //! @thread_safety This method is thread safe.
    virtual void removeMessageConsumer_abi(ILogMessageConsumer* consumer) noexcept = 0;

    //! Returns the list of message consumers.
    //!
    //! This method operates in two modes: **query mode** or **get mode**.
    //!
    //! @p consumersCount must not be `nullptr` in both modes.
    //!
    //! **Query mode** is enabled when consumers is `nullptr`.  When in this mode, @p *consumersCount will be populated
    //! with the number of consumers in the log and @ref omni::core::kResultSuccess is returned.
    //!
    //! **Get mode** is enabled when consumers is not `nullptr`.  Upon entering the function, @p *consumersCount stores
    //! the number of entries in consumers.  If @p *consumersCount is less than the number of consumers in the log, @p
    //! *consumersCount is updated with the number of consumers in the log and @ref
    //! omni::core::kResultInsufficientBuffer is returned. If @p *consumersCount is greater or equal to the number of
    //! channels, @p *consumersCount is updated with the number of consumers in the log, consumers array is populated,
    //! and @ref omni::core::kResultSuccess is returned.
    //!
    //! If the @p consumers array is populated, each written entry in the array will have had @ref
    //! omni::core::IObject::acquire() called on it.
    //!
    //! Upon entering the function, the pointers in the @p consumers array must be `nullptr`.
    //!
    //! @returns Return values are as above.  Additional error codes may be returned (e.g. @ref
    //! omni::core::kResultInvalidArgument if @p consumersCount is `nullptr`).
    //!
    //! @thread_safety This method is thread safe.
    virtual OMNI_ATTR("no_api, no_py") omni::core::Result getMessageConsumers_abi( // disable omni.bind until OM-21202
        OMNI_ATTR("out, count=*consumersCount, *not_null") ILogMessageConsumer** consumers,
        OMNI_ATTR("in, out, not_null") uint32_t* consumersCount) noexcept = 0;

    //! Set the logging level of this object.
    //!
    //! By default log channels obey the logging level set on this object.  However, this behavior can be overridden
    //! with @ref omni::log::ILog::setChannelLevel().
    //!
    //! @thread_safety This method is thread safe.
    virtual void setLevel_abi(Level level) noexcept = 0;

    //! Returns the logging level of this object.
    //!
    //! @thread_safety This method is thread safe.
    virtual Level getLevel_abi() noexcept = 0;

    //! Set if the log is enabled/disabled.
    //!
    //! By default log channels obey the enable/disabled flag set on this object.  However, this behavior can be
    //! overridden with @ref omni::log::ILog::setChannelEnabled().
    //!
    //! @thread_safety This method is thread safe.
    virtual void setEnabled_abi(bool isEnabled) noexcept = 0;

    //! Returns if the log is enabled/disabled.
    //!
    //! @thread_safety This method is thread safe.
    virtual bool isEnabled_abi() noexcept = 0;

    //! Instructs the logging system to deliver all log messages to the logging backends asynchronously.
    //!
    //! This causes @ref omni::log::ILog::log() calls to be buffered so that @ref omni::log::ILog::log() may return as
    //! quickly as possible. A background thread then issues these buffered log messages to the registered Logger
    //! backend objects.
    //!
    //! @thread_safety This method is thread safe.
    //!
    //! @returns Returns the state of asynchronous logging before this method was called.
    virtual OMNI_ATTR("py_not_prop") bool setAsync_abi(bool logAsync) noexcept = 0;

    //! Returns `true` if asynchronous logging is enabled.
    //!
    //! @thread_safety This method is thread safe.
    virtual OMNI_ATTR("py_not_prop") bool isAsync_abi() noexcept = 0;

    //! Associates a log channel's id with a chunk of memory to store its settings.
    //!
    //! A log channel can be registered multiple times.  In fact, this is quite common, as a log channel's settings are
    //! usually stored per-module and a log channel may span multiple modules.
    //!
    //! When registering a channel via this API, the given setting's memory is updated.
    //!
    //! @param name Name of the channel. Copied by this method.
    //!
    //! @param level Pointer to where the channels level is stored.  The pointer must point to valid memory until @ref
    //! omni::log::ILog::removeChannel() is called.
    //!
    //! @param description The description of the channel.  Can be `nullptr`.  If not `nullptr`, and a description for
    //! the channel is already set and not empty, the given description is ignored.  Otherwise, the description is
    //! copied by this method.
    //!
    //! @thread_safety This method is thread safe.
    virtual OMNI_ATTR("no_py") void addChannel_abi(OMNI_ATTR("c_str, not_null") const char* name,
                                                   OMNI_ATTR("in, out, not_null") Level* level,
                                                   OMNI_ATTR("c_str") const char* description) noexcept = 0;

    //! Removes a log channel's settings memory.
    //!
    //! Use this method when unloading a module to prevent the log writing settings to unloaded memory.
    //!
    //! @thread_safety This method is thread safe.
    virtual OMNI_ATTR("no_py") void removeChannel_abi(OMNI_ATTR("c_str, not_null") const char* name,
                                                      OMNI_ATTR("in, out, not_null") Level* level) noexcept = 0;

    //! Returns the list of channels names.
    //!
    //! This method operates in two modes: **query mode** or **get mode**.
    //!
    //! @p namesCount must not be `nullptr` in both modes.
    //!
    //! **Query mode** is enabled when names is `nullptr`.  When in this mode, @p *namesCount will be populated with the
    //! number of channels in the log and @ref omni::core::kResultSuccess is returned.
    //!
    //! **Get mode** is enabled when names is not `nullptr`.  Upon entering the function, @p *namesCount stores the
    //! number of entries in names.  If @p *namesCount is less than the number of channels in the log, @p *namesCount is
    //! updated with the number of channels in the log and @ref omni::core::kResultInsufficientBuffer is returned.  If
    //! @p *namesCount is greater or equal to the number of channels, @p *namesCount is updated with the number of
    //! channels in the log, names array is populated, and @ref omni::core::kResultSuccess is returned.
    //!
    //! If the @p names array is populated, each written entry in the array will have had @ref
    //! omni::core::IObject::acquire() called on it.
    //!
    //! Upon entering the function, the pointers in the @p names array must be `nullptr`.
    //!
    //! @return Return values are as above.  Additional error codes may be returned (e.g. @ref
    //! omni::core::kResultInvalidArgument if @p namesCount is `nullptr`).
    //!
    //! @thread_safety This method is thread safe.
    virtual OMNI_ATTR("no_api, no_py") omni::core::Result getChannelNames_abi( // disable omni.bind until OM-21202
        OMNI_ATTR("out, count=*namesCount, *not_null") omni::str::IReadOnlyCString** names,
        OMNI_ATTR("in, out, not_null") uint32_t* namesCount) noexcept = 0;

    //! Sets the given channel's log level.
    //!
    //! If the channel has not yet been registered with @ref omni::log::ILog::addChannel(), the setting will be
    //! remembered and applied when the channel is eventually added.
    //!
    //! @thread_safety This method is thread safe.
    virtual void setChannelLevel_abi(OMNI_ATTR("c_str, not_null") const char* name,
                                     Level level,
                                     SettingBehavior behavior) noexcept = 0;

    //! Returns the given channel's logging level and override behavior.
    //!
    //! All parameters must be non-`nullptr`.
    //!
    //! If the given channel is not found, an @ref omni::core::kResultNotFound is returned.
    //!
    //! @return Returns @ref omni::core::kResultSuccess upon success, a failure code otherwise.
    //!
    //! @thread_safety This method is thread safe.
    virtual omni::core::Result getChannelLevel_abi(OMNI_ATTR("c_str, not_null") const char* name,
                                                   OMNI_ATTR("out, not_null") Level* outLevel,
                                                   OMNI_ATTR("out, not_null") SettingBehavior* outBehavior) noexcept = 0;

    //! Sets the given channel's enabled/disabled flag.
    //!
    //! If the channel has not yet been registered with @ref omni::log::ILog::addChannel(), the setting will be
    //! remembered and applied when the channel is eventually added.
    //!
    //! @thread_safety This method is thread safe.
    virtual void setChannelEnabled_abi(OMNI_ATTR("c_str, not_null") const char* name,
                                       bool isEnabled,
                                       SettingBehavior behavior) noexcept = 0;

    //! Returns the given channel's logging enabled state and override behavior.
    //!
    //! All parameters must be non-`nullptr`.
    //!
    //! If the given channel is not found, an @ref omni::core::kResultNotFound is returned.
    //!
    //! Return @ref omni::core::kResultSuccess upon success, a failure code otherwise.
    //!
    //! @thread_safety This method is thread safe.
    virtual omni::core::Result getChannelEnabled_abi(OMNI_ATTR("c_str, not_null") const char* name,
                                                     OMNI_ATTR("out, not_null") bool* outIsEnabled,
                                                     OMNI_ATTR("out, not_null")
                                                         SettingBehavior* outBehavior) noexcept = 0;

    //! Sets a channel's description.  If the channel does not exists, it is created.
    //!
    //! The given channel @p name and @p description must not be `nullptr`.
    //!
    //! The memory pointed to by @p description is copied by this method.
    //!
    //! If the channel already has a description, it is replaced.
    //!
    //! @thread_safety This method is thread safe.
    virtual void setChannelDescription_abi(OMNI_ATTR("c_str, not_null") const char* name,
                                           OMNI_ATTR("c_str, not_null") const char* description) noexcept = 0;

    //! Returns the given channel's description.
    //!
    //! All parameters must be non-`nullptr`.
    //!
    //! When calling this method, @p *outDescription must be `nullptr`.
    //!
    //! If the channel does not have a description set, @p *outDescription is set to `nullptr`.
    //!
    //! If @p *outDescription is set to non-`nullptr`, it will have @ref omni::core::IObject::acquire() called on it
    //! before it is passed back to the caller.
    //!
    //! If the given channel is not found, an @ref omni::core::kResultNotFound is returned.
    //!
    //! @return Returns @ref omni::core::kResultSuccess upon success, a failure code otherwise.
    //!
    //! @thread_safety This method is thread safe.
    virtual OMNI_ATTR("no_py") omni::core::Result getChannelDescription_abi( // OM-21456: disable omni.bind py bindings
        OMNI_ATTR("c_str, not_null") const char* name,
        OMNI_ATTR("out, not_null") omni::str::IReadOnlyCString** outDescription) noexcept = 0;

    //! Given a channel and a verbosity level, returns `true` if the channel is actively logging at the given level.
    //!
    //! Using the `OMNI_LOG_*` macros is preferred over this method, as those macros use a much more efficient method to
    //! filter messages.  However, the mechanics utilized by `OMNI_LOG_*` are not viable when binding to languages such
    //! as Python, thus this method's existence.
    //!
    //! @thread_safety This method is thread safe.
    virtual bool isLoggingAtLevel_abi(OMNI_ATTR("c_str, not_null") const char* name, Level level) noexcept = 0;

    //! Flush all queued messages to message consumers.
    //!
    //! If asynchronous logging is enabled (see @ref omni::log::ILog::setAsync), blocks until all pending messages have
    //! been delivered to message consumers.
    //!
    //! @thread_safety This method is thread safe.
    virtual void flush_abi() noexcept = 0;

    //! Adds the given channel updated consumer to the internal list of update consumers.
    //!
    //! Each time the state of the log changes, each update consumer is notified.
    //!
    //! A consumer can be registered a single time with a given @ref omni::log::ILog instance but can be registered with
    //! multiple @ref omni::log::ILog instances.
    //!
    //! Each message may be sent to registered consumers in parallel.
    //!
    //! It is safe to access @ref omni::log::ILog from the callback.
    //!
    //! @thread_safety This method is thread safe.
    virtual void OMNI_ATTR("consumer=onChannelUpdate_abi")
        addChannelUpdateConsumer_abi(OMNI_ATTR("not_null") ILogChannelUpdateConsumer* consumer) noexcept = 0;

    //! Removes the given consumer from the internal consumer list.
    //!
    //! This method silently accepts `nullptr`.
    //!
    //! This method silently accepts consumers that have not been registered with this object.
    //!
    //! Calling @ref omni::log::ILog::removeChannelUpdateConsumer() from @ref
    //! omni::log::ILogMessageConsumer::onMessage() will lead to undefined behavior.
    //!
    //! @thread_safety This method is thread safe.
    virtual void removeChannelUpdateConsumer_abi(ILogChannelUpdateConsumer* consumer) noexcept = 0;

    //! Returns the list of update consumers.
    //!
    //! This method operates in two modes: **query mode** or **get mode**.
    //!
    //! @p consumersCount must not be `nullptr` in both modes.
    //!
    //! **Query mode** is enabled when consumers is `nullptr`.  When in this mode, @p *consumersCount will be populated
    //! with the number of consumers in the log and @ref omni::core::kResultSuccess is returned.
    //!
    //! **Get mode** is enabled when consumers is not `nullptr`.  Upon entering the function, @p *consumersCount stores
    //! the number of entries in consumers.  If @p *consumersCount is less than the number of consumers in the log, @p
    //! *consumersCount is updated with the number of consumers in the log and @ref
    //! omni::core::kResultInsufficientBuffer is returned. If @p *consumersCount is greater or equal to the number of
    //! channels, @p *consumersCount is updated with the number of consumers in the log, consumers array is populated,
    //! and @ref omni::core::kResultSuccess is returned.
    //!
    //! If the @p consumers array is populated, each written entry in the array will have had @ref
    //! omni::core::IObject::acquire() called on it.
    //!
    //! Upon entering the function, the pointers in the @p consumers array must be `nullptr`.
    //!
    //! Return values are as above.  Additional error codes may be returned (e.g. @ref
    //! omni::core::kResultInvalidArgument if @p consumersCount is `nullptr`).
    //!
    //! @thread_safety This method is thread safe.
    virtual OMNI_ATTR("no_api, no_py") omni::core::Result getChannelUpdateConsumers_abi( // disable omni.bind until
                                                                                         // OM-21202
        OMNI_ATTR("out, count=*consumersCount, *not_null") ILogChannelUpdateConsumer** consumers,
        OMNI_ATTR("in, out, not_null") uint32_t* consumersCount) noexcept = 0;
};

} // namespace log
} // namespace omni

#define OMNI_BIND_INCLUDE_INTERFACE_DECL
#include "ILog.gen.h"

#ifdef OMNI_COMPILE_AS_DYNAMIC_LIBRARY
OMNI_API omni::log::ILog* omniGetLogWithoutAcquire();
#else
//! Returns the global log. omni::core::IObject::acquire() is **not** called on the returned pointer.
//!
//! The global omni::log::ILog instance can be configured by passing an omni::log::ILog to omniCoreStart().
//! If an instance is not provided, omniCreateLog() is called.
inline omni::log::ILog* omniGetLogWithoutAcquire()
{
    return static_cast<omni::log::ILog*>(omniGetBuiltInWithoutAcquire(OmniBuiltIn::eILog));
}
#endif

//! Instantiates a default implementation of @ref omni::log::ILog. @ref omni::core::IObject::acquire() is called on the
//! returned pointer.
OMNI_API omni::log::ILog* omniCreateLog();

// omniGetModuleFilename is also forward declared in omni/core/Omni.h.  Exhale doesn't like that.
#ifndef DOXYGEN_SHOULD_SKIP_THIS

//! Returns the module's name (e.g. "c:/foo/omni-glfw.dll"). The pointer returned is valid for the lifetime of the
//! module.
//!
//! The returned path will be delimited by '/' on all platforms.
OMNI_API const char* omniGetModuleFilename();

#endif

//! @copydoc omni::log::ILogMessageConsumer_abi
class omni::log::ILogMessageConsumer : public omni::core::Generated<omni::log::ILogMessageConsumer_abi>
{
};

//! @copydoc omni::log::ILogChannelUpdateConsumer_abi
class omni::log::ILogChannelUpdateConsumer : public omni::core::Generated<omni::log::ILogChannelUpdateConsumer_abi>
{
};

//! @copydoc omni::log::ILog_abi
class omni::log::ILog : public omni::core::Generated<omni::log::ILog_abi>
{
public:
    //! Returns a snapshot of the array of message consumers attached to the log.
    //!
    //! @thread_safety This method is thread safe.
    std::vector<omni::core::ObjectPtr<omni::log::ILogMessageConsumer>> getMessageConsumers() noexcept;

    //! Returns a snapshot of the array of channels attached to the log.
    //!
    //! @thread_safety This method is thread safe.
    std::vector<omni::core::ObjectPtr<omni::str::IReadOnlyCString>> getChannelNames() noexcept;

    //! Returns a snapshot of the array of update consumers attached to the log.
    //!
    //! @thread_safety This method is thread safe.
    std::vector<omni::core::ObjectPtr<omni::log::ILogChannelUpdateConsumer>> getChannelUpdateConsumers() noexcept;

    //! @copydoc omni::log::ILog_abi::setChannelEnabled_abi
    void setChannelEnabled(const omni::log::LogChannelData& channel,
                           bool isEnabled,
                           omni::log::SettingBehavior behavior) noexcept
    {
        setChannelEnabled(channel.name, isEnabled, behavior);
    }

    //! @copydoc omni::log::ILog_abi::getChannelEnabled_abi
    omni::core::Result getChannelEnabled(const omni::log::LogChannelData& channel,
                                         bool* outEnabled,
                                         omni::log::SettingBehavior* outBehavior) noexcept
    {
        return getChannelEnabled(channel.name, outEnabled, outBehavior);
    }

    // We must expose setChannelEnabled(const char*, ...) since setChannelEnabled(LogChannelData, ...) hides it.
    using omni::core::Generated<omni::log::ILog_abi>::setChannelEnabled;
    using omni::core::Generated<omni::log::ILog_abi>::getChannelEnabled;

    //! @copydoc omni::log::ILog_abi::setChannelLevel_abi
    void setChannelLevel(const omni::log::LogChannelData& channel,
                         omni::log::Level level,
                         omni::log::SettingBehavior behavior) noexcept
    {
        setChannelLevel(channel.name, level, behavior);
    }

    //! @copydoc omni::log::ILog_abi::getChannelLevel_abi
    omni::core::Result getChannelLevel(const omni::log::LogChannelData& channel,
                                       omni::log::Level* outLevel,
                                       omni::log::SettingBehavior* outBehavior) noexcept
    {
        return getChannelLevel(channel.name, outLevel, outBehavior);
    }

    // We must expose setChannelLevel(const char*, ...) since setChannelEnabled(LogChannelData, ...) hides it.
    using omni::core::Generated<omni::log::ILog_abi>::setChannelLevel;
    using omni::core::Generated<omni::log::ILog_abi>::getChannelLevel;

    //! @copydoc omni::log::ILog_abi::isLoggingAtLevel_abi
    bool isLoggingAtLevel(const omni::log::LogChannelData& channel, omni::log::Level level)
    {
        return isLoggingAtLevel(channel.name, level);
    }

    // We must expose isLoggingAtLevel(const char*, ...) since isLoggingAtLevel(LogChannelData, ...) hides it.
    using omni::core::Generated<omni::log::ILog_abi>::isLoggingAtLevel;
};

#define OMNI_BIND_INCLUDE_INTERFACE_IMPL
#include "ILog.gen.h"

namespace omni
{
namespace log
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS

namespace detail
{
#    if CARB_COMPILER_GNUC || CARB_TOOLCHAIN_CLANG
// clang sees compileTimeValidateFormat<>() as an unimplemented function (which it intentionally
// is) on mac and generates a warning.  We'll just silence that warning.
CARB_IGNOREWARNING_CLANG_WITH_PUSH("-Wundefined-internal")

// Utilizes an __attribute__ to validates the given fmt at compile time.  Does not produce any code. Works for GCC but
// not MSVC.
void compileTimeValidateFormat(const LogChannelData& channel, const char* fmt, ...) CARB_PRINTF_FUNCTION(2, 3);

// Utilizes an __attribute__ to validates the given fmt at compile time.  Does not produce any code. Works for GCC but
// not MSVC.
void compileTimeValidateFormat(const char* fmt, ...) CARB_PRINTF_FUNCTION(1, 2);

#        define OMNI_LOG_VALIDATE_FORMAT(...)                                                                          \
            if (false)                                                                                                 \
            {                                                                                                          \
                omni::log::detail::compileTimeValidateFormat(__VA_ARGS__);                                             \
            }

CARB_IGNOREWARNING_CLANG_POP
#    else
#        define OMNI_LOG_VALIDATE_FORMAT(...)
#    endif

// Implementation detail. Checks the message's level against the channel's level and logs as appropriate.
template <class... ArgList>
void writeLog(const LogChannelData& /*ignore*/,
              const LogChannelData& channel,
              ILog* log,
              Level level,
              const char* filename,
              const char* function,
              int32_t line,
              const char* fmt,
              ArgList&&... args)
{
    log->logf(
        channel.name, level, omniGetModuleFilename(), filename, function, line, fmt, std::forward<ArgList>(args)...);
}

// Implementation detail. Initiates logging to the default channel.
template <class... ArgList>
void writeLog(const LogChannelData& channel,
              const char* fmt,
              ILog* log,
              Level level,
              const char* filename,
              const char* function,
              int32_t line,
              ArgList&&... args)
{
    log->logf(
        channel.name, level, omniGetModuleFilename(), filename, function, line, fmt, std::forward<ArgList>(args)...);
}

// Implementation detail. Checks the message's level against the channel's level.
inline bool isChannelEnabled(Level level, const LogChannelData& /*ignore*/, const LogChannelData& channel)
{
    return (static_cast<Level>(channel.level) <= level);
}

// Implementation detail. Checks the message's level against the default channel
inline bool isChannelEnabled(Level level, const LogChannelData& channel, const char* /*fmt*/)
{
    return (static_cast<Level>(channel.level) <= level);
}

} // namespace detail

#endif

//! Registers known channels with the log returned by @ref omniGetLogWithoutAcquire().
//!
//! The @ref OMNI_LOG_ADD_CHANNEL macro adds the specified channel to a module specific list of channels.  This happens
//! pre-`main()` (i.e. during static initialization).
//!
//! Call this function, to iterate over the said list of channels and add them to the log.  This function should be
//! called per-module/application.
//!
//! Upon unloading a plugin, see @ref omni::log::removeModulesChannels to unregister channels registered with this
//! function.
//!
//! Carbonite plugin authors should consider using @ref CARB_PLUGIN_IMPL, which will call @ref
//! omni::log::addModulesChannels() and @ref omni::log::removeModulesChannels() for you.
//!
//! Omniverse Native Interface plugin authors do not need to call this function, logging channels are registered and
//! unregistered via @ref OMNI_MODULE_SET_EXPORTS.
//!
//! Python bindings authors should consider using @ref OMNI_PYTHON_GLOBALS, which will call @ref
//! omni::log::addModulesChannels() and @ref omni::log::removeModulesChannels() for you.
//!
//! Application author should consider using @ref OMNI_CORE_INIT() which will call @ref omni::log::addModulesChannels()
//! and @ref omni::log::removeModulesChannels() for you.
inline void addModulesChannels()
{
    auto log = omniGetLogWithoutAcquire();
    if (log)
    {
        for (auto channel = getModuleLogChannels(); channel; channel = channel->next)
        {
            log->addChannel(channel->name, reinterpret_cast<Level*>(&channel->level), channel->description);
        }
    }
}

//! Removes channels added by @ref omni::log::addModulesChannels().
//!
//! @see See @ref omni::log::addModulesChannels() to understand who should call this function.
inline void removeModulesChannels()
{
    auto log = omniGetLogWithoutAcquire();
    if (log)
    {
        for (auto channel = getModuleLogChannels(); channel; channel = channel->next)
        {
            log->removeChannel(channel->name, reinterpret_cast<Level*>(&channel->level));
        }
    }
}

} // namespace log
} // namespace omni

#ifndef DOXYGEN_SHOULD_SKIP_THIS

inline std::vector<omni::core::ObjectPtr<omni::log::ILogMessageConsumer>> omni::log::ILog::getMessageConsumers() noexcept
{
    std::vector<omni::core::ObjectPtr<omni::log::ILogMessageConsumer>> out;
    auto result = omni::extras::getOutArray<omni::log::ILogMessageConsumer*>(
        [this](omni::log::ILogMessageConsumer** consumers, uint32_t* consumersCount) // get func
        {
            std::memset(consumers, 0, sizeof(omni::log::ILogMessageConsumer*) * *consumersCount); // incoming ptrs
                                                                                                  // must be nullptr
            return this->getMessageConsumers_abi(consumers, consumersCount);
        },
        [&out](omni::log::ILogMessageConsumer** names, uint32_t namesCount) // fill func
        {
            out.reserve(namesCount);
            for (uint32_t i = 0; i < namesCount; ++i)
            {
                out.emplace_back(names[i], omni::core::kSteal);
            }
        });

    if (OMNI_FAILED(result))
    {
        OMNI_LOG_ERROR("unable to retrieve log channel settings consumers: 0x%08X", result);
    }

    return out;
}

inline std::vector<omni::core::ObjectPtr<omni::str::IReadOnlyCString>> omni::log::ILog::getChannelNames() noexcept
{
    std::vector<omni::core::ObjectPtr<omni::str::IReadOnlyCString>> out;
    auto result = omni::extras::getOutArray<omni::str::IReadOnlyCString*>(
        [this](omni::str::IReadOnlyCString** names, uint32_t* namesCount) // get func
        {
            std::memset(names, 0, sizeof(omni::str::IReadOnlyCString*) * *namesCount); // incoming ptrs must be nullptr
            return this->getChannelNames_abi(names, namesCount);
        },
        [&out](omni::str::IReadOnlyCString** names, uint32_t namesCount) // fill func
        {
            out.reserve(namesCount);
            for (uint32_t i = 0; i < namesCount; ++i)
            {
                out.emplace_back(names[i], omni::core::kSteal);
            }
        });

    if (OMNI_FAILED(result))
    {
        OMNI_LOG_ERROR("unable to retrieve log channel names: 0x%08X", result);
    }

    return out;
}

inline std::vector<omni::core::ObjectPtr<omni::log::ILogChannelUpdateConsumer>> omni::log::ILog::getChannelUpdateConsumers() noexcept
{
    std::vector<omni::core::ObjectPtr<omni::log::ILogChannelUpdateConsumer>> out;
    auto result = omni::extras::getOutArray<omni::log::ILogChannelUpdateConsumer*>(
        [this](omni::log::ILogChannelUpdateConsumer** consumers, uint32_t* consumersCount) // get func
        {
            std::memset(consumers, 0, sizeof(omni::log::ILogChannelUpdateConsumer*) * *consumersCount); // incoming
                                                                                                        // ptrs must
                                                                                                        // be nullptr
            return this->getChannelUpdateConsumers_abi(consumers, consumersCount);
        },
        [&out](omni::log::ILogChannelUpdateConsumer** names, uint32_t namesCount) // fill func
        {
            out.reserve(namesCount);
            for (uint32_t i = 0; i < namesCount; ++i)
            {
                out.emplace_back(names[i], omni::core::kSteal);
            }
        });

    if (OMNI_FAILED(result))
    {
        OMNI_LOG_ERROR("unable to retrieve log channel updated consumers: 0x%08X", result);
    }

    return out;
}

#endif
