// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "Logger.h"

namespace carb
{
namespace logging
{

enum class OutputStream
{
    eDefault,
    eStderr,
};

// Can be used by setFileConfiguration
const char* const kKeepSameFile = (const char*)size_t(-1);

/**
 * Describes the configuration for logging to a file for setFileConfiguration
 *
 * @note Do not rearrange below members as it disrupts @rstref{ABI compatibility <abi-compatibility>}; add members at
 * the bottom.
 */
struct LogFileConfiguration
{
    /// Size of the struct used for versioning. Adding members to this struct will change the size and therefore act as
    /// a version for the struct.
    size_t size{ sizeof(LogFileConfiguration) };

    /// Indicates whether opening the file should append to it. If false, file is overwritten.
    ///
    /// @note Setting (boolean): "/log/fileAppend"
    /// @note Default = false
    bool append{ false };
};

/**
 * The default logger provided by the Framework. It is quite flexible and you can use multiple
 * instances if you want different configurations for different output destinations. It can
 * also be safely called from multiple threads.
 *
 * @see ILogging::getDefaultLogger
 * @see ILogging::createStandardLogger
 * @see ILogging::destroyStandardLogger
 */
struct StandardLogger : public Logger
{
    /**
     * Includes or excludes the filename of where the log message came from. A new StandardLogger
     * will by default exclude this information.
     *
     * @param instance The instance of the StandardLogger interface being used
     * @param included Whether the filename information should be included in the log message
     */
    void(CARB_ABI* setFilenameIncluded)(StandardLogger* instance, bool included);

    /**
     * Includes or excludes the line number of where the log message came from. A new StandardLogger
     * will by default exclude this information.
     *
     * @param included Whether the line number information should be included in the log message
     */
    void(CARB_ABI* setLineNumberIncluded)(StandardLogger* instance, bool included);

    /**
     * Includes or excludes the function name of where the log message came from. A new StandardLogger
     * will by default exclude this information.
     *
     * @param included Whether the function name information should be included in the log message
     */
    void(CARB_ABI* setFunctionNameIncluded)(StandardLogger* instance, bool included);

    /**
     * Includes or excludes the timestamp of when the log message was issued. A new StandardLogger
     * will by default exclude this information. The time is in UTC format.
     *
     * @param instance The instance of the StandardLogger interface being used
     * @param included Whether the timestamp information should be included in the log message
     */
    void(CARB_ABI* setTimestampIncluded)(StandardLogger* instance, bool included);

    /**
     * Includes or excludes the id of a thread from which the log message was issued. A new StandardLogger
     * will by default exclude this information.
     *
     * @param instance The instance of the StandardLogger interface being used
     * @param included Whether the thread id should be included in the log message
     */
    void(CARB_ABI* setThreadIdIncluded)(StandardLogger* instance, bool included);

    /**
     * Includes or excludes the source (module) of where the log message came from. A new StandardLogger
     * will by default include this information.
     *
     * @param instance The instance of the StandardLogger interface being used
     * @param included Whether the source (module) information should be included in the log message
     */
    void(CARB_ABI* setSourceIncluded)(StandardLogger* instance, bool included);

    /**
     * Enables (or disables) standard stream output (stdout and stderr) for the logger. Error messages are written
     * to stderr, all other messages to stdout. A new FrameworkLogger will have this output enabled.
     *
     * @param instance The instance of the StandardLogger interface being used
     * @param enabled Whether log output should go to standard streams (stdout and stderr)
     */
    void(CARB_ABI* setStandardStreamOutput)(StandardLogger* instance, bool enabled);

    /**
     * (Windows only) Enables (or disables) debug console output for the logger via `OutputDebugStringW()`. By default,
     * debug output is only supplied if a debugger is attached (via `IsDebuggerPresent()`). Calling this with @p enabled
     * as `true` will always produce debug output which is useful for non-debugger tools such as SysInternals DebugView.
     *
     * @param instance The instance of the StandardLogger interface being used
     * @param enabled Whether log output should be sent to the debug console.
     */
    void(CARB_ABI* setDebugConsoleOutput)(StandardLogger* instance, bool enabled);

    /** sets the path to the log file to open.
     *
     *  @param[in] instance     the standard logger instance to set the log file path for.  This
     *                          may not be nullptr.
     *  @param[in] filePath     the local file path to write the log file to.  This may be a
     *                          relative or absolute path.  Relative paths will be resolved
     *                          relative to the process's current working directory at the time
     *                          of the call.  This may be nullptr to not write to a log file at
     *                          all or to close the current log file.  If a log file was
     *                          previously open during this call, it will be closed first.  If
     *                          nullptr is passed in here, logging to a file will effectively
     *                          be disabled.  This path must be UTF-8 encoded.  See the remarks
     *                          below for more information on formatting of the log file path.
     *  @returns no return value.
     *
     *  @remarks This sets the path to the log file to write to for the given instance of a
     *           standard logger object.  The log file name may contain the string "${pid}" to
     *           have the process ID inserted in its place.  By default, a new standard logger
     *           will disable logging to a file.
     *
     *  @note Setting the log file name with this function will preserve the previous log file
     *        configuration.  If the configuration needs to changes as well (ie: change the
     *        'append' state of the log file), setFileConfiguration() should be used instead.
     */
    void(CARB_ABI* setFileOutput)(StandardLogger* instance, const char* filePath);

    /**
     * Enables flushing on every log message to file specified severity or higher.
     * A new StandardLogger will have this set to flush starting from kLevelVerbose, so that file logging will be
     * reliable out of the box. The idea is that file logging will be used for debugging purposes by default, with a
     * price of significant performance penalty.
     *
     * @param instance The instance of the StandardLogger interface being used
     * @param level The starting log level to flush file log output at.
     */
    void(CARB_ABI* setFileOuputFlushLevel)(StandardLogger* instance, int32_t level);

    /**
     * Enables flushing of stdout after each message is printed to it.
     * By default, this option will be disabled.  The default behavior will be to only flush stdout just before
     * writing a message to stderr.
     *
     * @param instance The instance of the StandardLogger interface to modify.
     * @param enabled Set to true to cause stdout to be flushed after each message is written.  Set to false to
     *        use the default behavior of only flushing stdout before writing to stderr.
     */
    void(CARB_ABI* setFlushStandardStreamOutput)(StandardLogger* instance, bool enabled);

    /**
     *  Enables a high resolution time index to be printed with each message.
     *  By default, this option is disabled (ie: no time index printed).  When enabled, the current time index
     *  (since the first message was printed) will be printed with each message.  The time index may be in
     *  milliseconds, microseconds, or nanoseconds depending on the string @p units.  The printing of the time
     *  index may be enabled at the same time as the timestamp.
     *
     *  @param[in] instance the instance of the StandardLogger interface to modify.
     *  @param[in] units    the units that the time index should be printed in.  This can be one of the following
     *                      supported unit names:
     *      * nullptr, "", or "none": the time index printing is disabled (default state).
     *      * "ms", "milli", or "milliseconds": print the time index in milliseconds.
     *      * "us", "µs", "micro", or "microseconds": print the time index in microseconds.
     *      * "ns", "nano", or "nanoseconds": print the time index in nanoseconds.
     */
    void(CARB_ABI* setElapsedTimeUnits)(StandardLogger* instance, const char* units);

    /**
     * Includes or excludes the id of the process from which the log message was issued. A new StandardLogger
     * will by default exclude this information.
     *
     * @param instance The instance of the StandardLogger interface being used
     * @param enabled Whether the process id should be included in the log message
     */
    void(CARB_ABI* setProcessIdIncluded)(StandardLogger* inst, bool enabled);

    /**
     *  sets the process group ID for the logger.  If a non-zero identifier is given, inter-process
     *  locking will be enabled on both the log file and the stdout/stderr streams.  This will prevent
     *  simultaneous messages from multiple processes in the logs from becoming interleaved within
     *  each other.  If a zero identifier is given, inter-process locking will be disabled.
     *
     *  @param[in] inst     the instance of the StandardLogger interface to modify.
     *  @param[in] id       an arbitrary process group identifier to set.
     */
    void(CARB_ABI* setMultiProcessGroupId)(StandardLogger* inst, int32_t id);

    /**
     * Enables (or disables) color codes output for the logger. A new StandardLogger will have this output enabled
     * unless the output is piped to a file, in which case this will be disabled.
     *
     * @param instance The instance of the StandardLogger interface being used
     * @param enabled Whether log output should include color codes
     */
    void(CARB_ABI* setColorOutputIncluded)(StandardLogger* instance, bool enabled);

    /**
     * Specify the output stream that logging should go to.
     * By default, messages are sent to stdout and errors are sent to stderr.
     *
     *  @param[in] inst         The instance of the StandardLogger interface to modify.
     *  @param[in] outputStream The output stream setting to use.
     *                          If this is OutputStream::eStderr, all logging
     *                          output will be sent to stderr.
     *                          If this is OutputStream::eDefault, the default
     *                          logging behavior will be used.
     */
    void(CARB_ABI* setOutputStream)(StandardLogger* inst, OutputStream outputStream);

    /**
     * Sets the log level threshold for the messages going to the standard stream. Messages below this threshold will be
     * dropped.
     *
     * @param level The log level to set.
     */
    void(CARB_ABI* setStandardStreamOutputLevelThreshold)(StandardLogger* inst, int32_t level);

    /**
     * Sets the log level threshold for the messages going to the debug console output. Messages below this threshold
     * will be dropped.
     *
     * @param level The log level to set.
     */
    void(CARB_ABI* setDebugConsoleOutputLevelThreshold)(StandardLogger* inst, int32_t level);

    /**
     * Sets the log level threshold for the messages going to the file output. Messages below this threshold
     * will be dropped.
     *
     * @param level The log level to set.
     */
    void(CARB_ABI* setFileOutputLevelThreshold)(StandardLogger* inst, int32_t level);

    /**
     * Sets the file path and configuration for file logging. If nullptr is provided the file logging is disabled. A new
     * StandardLogger will by default disable file output.
     *
     * @param instance The instance of the StandardLogger interface being used
     * @param filePath The local file path to write to or nullptr, if you want to disable logging to file.
     *  Parameter is encoded as UTF8 character string with forward slashes as path separator. The path
     *  should include the extension .log but this is not a requirement. If a relative path is provided
     *  it is interpreted to be relative to the current working directory for the application. Can be kKeepSameFile to
     *  keep logging to the same file but set a new LogFileConfiguration.
     * @param config The LogFileConfiguration structure with parameters to use for the file configuration. Required.
     */
    void(CARB_ABI* setFileConfiguration)(StandardLogger* inst, const char* filePath, const LogFileConfiguration* config);

    /**
     * Returns the file path (in buffer) and configuration for file logging.
     *
     * @param instance The instance of the StandardLogger interface being used
     * @param buffer The buffer that will receive the UTF-8 file name that is being logged to. May be nullptr.
     * @param bufferSize The maximum number of bytes available in \ref buffer.
     * @param config The LogFileConfiguration to receive the current configuration. May be nullptr.
     * @returns If successful, the number of non-NUL bytes written to buffer. If not successful, contains the required
     * size of a buffer to receive the filename (not including the NUL terminator).
     */
    size_t(CARB_ABI* getFileConfiguration)(StandardLogger* inst,
                                           char* buffer,
                                           size_t bufferSize,
                                           LogFileConfiguration* config);

    /**
     * Pauses file logging (and closes the file) until resumeFileLogging() is called.
     *
     * @note This is a counted call. Each call to pauseFileLogging() must have a matching call to resumeFileLogging()
     *
     * @param inst The instance of the StandardLogger interface being used
     */
    void(CARB_ABI* pauseFileLogging)(StandardLogger* inst);

    /**
     * Resumes file logging (potentially reopening the file)
     *
     * @note This is a counted call. Each call to pauseFileLogging() must have a matching call to resumeFileLogging()
     *
     * @param inst The instance of the StandardLogger interface being used
     */
    void(CARB_ABI* resumeFileLogging)(StandardLogger* inst);


    /**
     * Forces the logger to use ANSI escape code's to annotate the log with color.
     *
     * By default, on Windows ANSI escape codes will never be used, rather the Console API will be used to place
     * colors in a console. Linux uses the isatty() to determine if the terminal supports ANSI escape codes. However,
     * the isatty check doesn't work in all cases. One notable case where this doesn't work is running a process in a
     * CI/CD that returns false from isatty() yet still supports ANSI escape codes.
     *
     * See: https://en.wikipedia.org/wiki/ANSI_escape_code for more information about ANSI escape codes.
     *
     * @param inst The instance of the StandardLogger interface being used
     * @param forceAnsiColor if true forces terminal to use ANSI escape codes for color
     */
    void(CARB_ABI* setForceAnsiColor)(StandardLogger* inst, bool forceAnsiColor);
};

/**
 * A class that pauses logging to a file when constructed and resumes logging to a file when destroyed.
 */
class ScopedFilePause
{
public:
    ScopedFilePause(StandardLogger* inst) : m_inst(inst)
    {
        m_inst->pauseFileLogging(m_inst);
    }
    ~ScopedFilePause()
    {
        m_inst->resumeFileLogging(m_inst);
    }

    CARB_PREVENT_COPY_AND_MOVE(ScopedFilePause);

private:
    StandardLogger* m_inst;
};
} // namespace logging
} // namespace carb
