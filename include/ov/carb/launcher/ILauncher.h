// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Simple external process launcher helper interface.
 */
#pragma once

#include "../Interface.h"

#if CARB_PLATFORM_LINUX
#    include <sys/prctl.h>
#    include <sys/signal.h>
#endif


namespace carb
{
/** Namespace for the Carbonite process launch helper interface. */
namespace launcher
{

// ****************************** structs, enums, and constants ***********************************
/** Opaque object used to represent a process that has been launched using the ILauncher
 *  interface.  A value of `nullptr` indicates an invalid process object.
 */
struct Process;

/** Base type for a process exit code.  Process exit codes differ between Windows and Linux - on
 *  Windows a process exit code is a DWORD, while on Linux it is an int.  This type should be able
 *  to successfully hold any value from either platform.
 */
using ExitCode = int64_t;

/** Base type for the identifier of a process.  This does not conform directly to the local
 *  definitions of a process ID for either Windows or Linux, but it should at least be large
 *  enough to properly contain either.
 */
using ProcessId = uint64_t;

/** Special value to indicate a bad process identifier.  This can be returned from
 *  ILauncher::getProcessId() if the given child process is no longer running.
 */
constexpr ProcessId kBadId = ~0ull;

/** Prototype for a stream read callback function.
 *
 *  @param[in] data     The buffer of data that was read from the child process's stream.  This
 *                      will never be `nullptr`.  Only the first @p bytes bytes of data in this
 *                      buffer will contain valid data.  Any data beyond that should be considered
 *                      undefined and not accessed.
 *  @param[in] bytes    The number of bytes of valid data in the @p data buffer.  This will never
 *                      be 0 as long as the connection to the child process is active.  When the
 *                      child process exits and the read thread has read all of the data from the
 *                      child process, one final callback will be performed passing 0 for the byte
 *                      count to indicate the end of the stream.  This count will not exceed the
 *                      buffer size specified in the original call to ILauncher::openProcess().
 *  @param[in] context  The context value that was originally passed to ILauncher::openProcess()
 *                      when the child process was created.
 *  @returns No return value.
 *
 *  @remarks This callback will be performed any time data is successfully read from one of the
 *           child process's output streams (ie: `stdout` or `stderr`).  The call will be performed
 *           on a worker thread that was created specifically for the child process.  This
 *           callback will be performed as soon after reading the data as possible.  The reader
 *           thread will remain in an efficient wait state while there is no data read to be
 *           read.  It is the callback's responsibility to ensure any shared resources that are
 *           accessed in the callback are appropriately protected from race conditions and
 *           general thread safety issues.
 *
 *  @remarks When reading from one of the child process' output streams, every effort will be
 *           taken to ensure the contents of at least one 'message' is delivered to the callback
 *           at a time.  A message can be thought of as the unit of data that was last written
 *           to the stream on the child process's side - for example, the output of a single call
 *           to fwrite() or fprintf().  However, there are a some caveats to this behavior that
 *           the callback and its owner need to be able to handle:
 *            * It is possible that depending on the size and arrival times of messages, multiple
 *              messages may be concatenated into a single callback call.  The callback needs to
 *              be able to handle this by being able to identify expected message ends and
 *              properly parse them out if needed.
 *            * If the current message or set of messages fills up the read buffer, the buffer
 *              as it is will be delivered to the callback with the last message truncated.  The
 *              remainder of the message will be sent in the next callback.  The callback needs
 *              to be able to handle this by either using a buffer size appropriate for the
 *              expected output of the child process, or by having the callback simply concatenate
 *              incoming data onto a data queue that is then processed elsewhere.
 *
 *  @remarks This callback should attempt to complete its task as quickly as possible to avoid
 *           blocking the read thread.  If the callback blocks or takes a long time to process
 *           it may result in blocking the child process's attempts to write to the stream.  The
 *           child process' thread will be effectively stopped until buffer space is freed up
 *           on the parent's read side.  It is best practice to have the callback simply queue
 *           up new data for later consumption on another thread in the parent process or to do
 *           a few simple string or data checks if searching for a specific incoming data message.
 *
 *  @remarks When the stream for this callback ends due to either the child or parent process
 *           closing it, one final callback will be performed.  The last callback will always
 *           have a @p bytes value of 0 in this case.  All other callbacks during the stream
 *           will have a non-zero @p bytes value.  Even in this final callback case however,
 *           a non-`nullptr` @p data buffer will still be provided.  Once the zero sized buffer
 *           has been delivered, the parent process can safely assume that the child process
 *           is done transmitting any data to the parent.
 */
using OnProcessReadFn = void (*)(const void* data, size_t bytes, void* context);

/** A default buffer size to use for reading from a child process's `stdout` or `stderr` streams. */
constexpr size_t kDefaultProcessBufferSize = 1ull << 17;


/** Launcher flags
 *  @{
 */
/** Base type for flags to the @ref carb::launcher::ILauncher::launchProcess function.  Valid flags for this
 *  type are the carb::launcher::fLaunchFlag* flags.
 */
using LauncherFlags = uint32_t;

/** Flag to indicate that the stdin stream for the child process should be opened and accessible
 *  on the side of the parent process.  If this flag is not present, any attempts to call
 *  ILauncher::writeProcessStdin() will fail immediately.  If this flag is present, the parent
 *  process may write information to the child process through its stdin stream.  The child
 *  process will be able to poll its stdin stream for input and read it.  If this is used, the
 *  child process will only be able to read input from the parent process.  If not used, the
 *  child process should assume that stdin cannot be read (though the actual behavior may
 *  differ by platform following native stdin inheritance rules).
 */
constexpr LauncherFlags fLaunchFlagOpenStdin = 0x00000001;

/** Flag to indicate that the new child process should be killed when the calling parent process
 *  exits.  If this flag is not present, the child process will only exit when it naturally exits
 *  on its own or is explicitly killed by another process.  If this flag is present, if the parent
 *  process exits in any way (ie: ends naturally, crashes, is killed, etc), the child process
 *  will also be killed.  Note that the child process will be killed without warning or any
 *  chance to clean up.  Any state in the child process that was not already saved to persistent
 *  storage will be lost.  Also, if the child process is in the middle of modifying persistent
 *  storage when it is killed, that resource may be left in an undefined state.
 *
 *  @note This flag is not supported on Mac.  It will be ignored if used on Mac and the child
 *        process(es) must be manually terminated by the parent process if necessary.
 */
constexpr LauncherFlags fLaunchFlagKillOnParentExit = 0x00000002;

/** When the @ref fLaunchFlagKillOnParentExit flag is also used, this indicates that the child
 *  process should be forcibly terminated instead of just being asked to exit when the parent
 *  process dies.  This flag is only used on Linux where there is the possibility of a child
 *  process catching and handling a SIGTERM signal.  If the child process generally installs
 *  a SIGTERM handler and doesn't exit as a result, this flag should be used to allow a SIGKILL
 *  to be sent instead (which can neither be caught nor ignored).  Generally, sending a SIGTERM
 *  is considered the 'correct' or 'appropriate' way to kill a process on Linux.  This flag is
 *  ignored on Windows.
 */
constexpr LauncherFlags fLaunchFlagForce = 0x00000004;

/** Flag to indicate that reading from the `stdout` or `stderr` streams of the child process should
 *  be handled as a byte stream.  Data will be delivered to the stream callback as soon as it is
 *  available.  The delivered bytes may only be a small portion of a complete message sent from
 *  the child process.  At least one byte will be sent when in this mode.  This is the default
 *  mode for all child processes.  This flag may not be combined with @ref fLaunchFlagMessageMode.
 *  When using this read mode, it is the callback's responsibility to process the incoming data
 *  and wait for any delimiters to arrive as is necessary for the task.  The message mode may not
 *  be changed after the child process has been launched.
 *
 *  @note [Windows] This mode is the only mode that is currently supported on Windows.  This may
 *        be fixed in a future version of the interface.
 */
constexpr LauncherFlags fLaunchFlagByteMode = 0x00000000;

/** Flag to indicate that reading from the `stdout` or `stderr` streams of the child process should
 *  be handled as a message stream.  Data will not be delivered to the stream callback until a
 *  complete message has been received.  A message is considered to be the contents of a single
 *  write call on the child's process side (up to a reasonable limit).  The data passed to the
 *  callbacks may be split into multiple messages if a very large message is sent in a single
 *  write call.  On Linux at least, this message limit is 4KB.  The message mode may not be
 *  changed after the child process has been launched.
 *
 *  @note [Windows] This mode is not currently supported on Windows.  This may be fixed in a
 *        future version of the interface.
 */
constexpr LauncherFlags fLaunchFlagMessageMode = 0x00000008;

/** Flag to indicate that the calling process's environment should not be inherited by the child
 *  process in addition to the new environment variables specified in the launch descriptor.  When
 *  no environment block is given in the descriptor, the default behavior is for the child
 *  process to inherit the parent's (ie: this calling process) environment block.  Similarly for
 *  when a non-empty environment block is specified in the launch descriptor - the environment
 *  block of the calling process will be prepended to the environment variables given in the
 *  launch descriptor.  However, when this flag is used, that will indicate that the new child
 *  process should only get the environment block that is explicitly given in the launch
 *  descriptor.
 */
constexpr LauncherFlags fLaunchFlagNoInheritEnv = 0x00000010;

/** Flag to indicate that the child process should still continue to be launched even if the
 *  environment block for it could not be created for any reason.  This flag is ignored if
 *  @ref LaunchDesc::env is `nullptr` or the block environment block object is successfully
 *  created.  The most common cause for failing to create the environment block is an out of
 *  memory situation or invalid UTF-8 codepoints being used in the given environment block.
 *  This flag is useful when the additional environment variables for the child process are
 *  optional to its functionality.
 */
constexpr LauncherFlags fLaunchFlagAllowBadEnv = 0x00000020;

/** Flag to indicate that the requested command should be launched as a script.  An attempt
 *  will be made to determine an appropriate command interpreter for it based on its file
 *  extension if no interpreter command is explicitly provided in @ref LaunchDesc::interpreter.
 *  When this flag is not present, the named command will be assumed to be a binary executable.
 *
 *  @note [Linux] This flag is not necessary when launching a script that contains a shebang
 *        on its first line.  The shebang indicates the command interpreter to be used when
 *        executing the script.  In this case, the script will also need to have its executable
 *        permission bit set for the current user.  If the shebang is missing however, this
 *        flag will be needed.
 */
constexpr LauncherFlags fLaunchFlagScript = 0x00000040;

/** Flags to indicate that the child process' standard output streams should be closed upon
 *  launch.  This is useful if the output from the child is not interesting or would otherwise
 *  garble up the parent process' standard streams log.  An alternative to this would be to
 *  create a dummy read function for both `stdout` and `stderr` where the parent process just drops
 *  all incoming messages from the child process.  This however also has its drawbacks since it
 *  would create one to two threads to listen for messages that would just otherwise be
 *  ignored.  If these flags are not used, the OS's default behavior of inheriting the parent
 *  process' standard streams will be used.  Each standard output stream can be disabled
 *  individually or together if needed depending on which flag(s) are used.
 *
 *  @note Any callbacks specified for @ref LaunchDesc::onReadStdout or
 *        @ref LaunchDesc::onReadStderr will be ignored if the corresponding flag(s) to disable
 *        those streams is also used.
 */
constexpr LauncherFlags fLaunchFlagNoStdOut = 0x00000080;

/** @copydoc fLaunchFlagNoStdOut */
constexpr LauncherFlags fLaunchFlagNoStdErr = 0x00000100;

/** @copydoc fLaunchFlagNoStdOut */
constexpr LauncherFlags fLaunchFlagNoStdStreams = fLaunchFlagNoStdOut | fLaunchFlagNoStdErr;

/** Flag to indicate that launching the child process should not fail if either of the log
 *  files fails to open for write.  This flag is ignored if the @ref LaunchDesc::stdoutLog
 *  and @ref LaunchDesc::stderrLog log filenames are `nullptr` or empty strings.  If either
 *  log file fails to open, the child process will still be launched but the default behavior
 *  of inheriting the parent's standard output streams will be used instead.  If this flag
 *  is not used, the default behavior is to fail the operation and not launch the child
 *  process.
 */
constexpr LauncherFlags fLaunchFlagAllowBadLog = 0x00000200;
/** @} */

/** Stream waiting flags
 *  @{
 */
/** Base type for flags to the @ref carb::launcher::ILauncher::waitForStreamEnd() function.
 *  Valid flags for this type are the carb::launcher::fWaitFlag* flags.
 */
using WaitFlags = uint32_t;

/** Flag to indicate that the `stdout` stream should be waited on.  The stream will be signaled
 *  when it is closed by either the child process and all of the data on the stream has been
 *  consumed, or by using the @ref fWaitFlagCloseStdOutStream flag.
 */
constexpr WaitFlags fWaitFlagStdOutStream = 0x00000001;

/** Flag to indicate that the `stderr` stream should be waited on.  The stream will be signaled
 *  when it is closed by either the child process and all of the data on the stream has been
 *  consumed, or by using the @ref fWaitFlagCloseStdErrStream flag.
 */
constexpr WaitFlags fWaitFlagStdErrStream = 0x00000002;

/** Flag to indicate that the `stdout` stream for a child should be closed before waiting on it.
 *  Note that doing so will truncate any remaining incoming data on the stream.  This is useful
 *  for closing the stream and exiting the reader thread when the parent process is no longer
 *  interested in the output of the child process (ie: the parent only wanted to wait on a
 *  successful startup signal from the child).  This flag has no effect if no read callback was
 *  provided for the `stdout` stream when the child process was launched.
 *
 *  @note [Linux] If this is used on a child process to close the parent process's end of the
 *        `stdout` stream, the child process will be terminated with SIGPIPE if it ever tries
 *        to write to `stdout` again.  This is the default handling of writing to a broken
 *        pipe or socket on Linux.  The only way around this default behavior is to ensure
 *        that the child process ignores SIGPIPE signals.  Alternatively, the parent could
 *        just wait for the child process to exit before destroying its process handle or
 *        closing the stream.
 */
constexpr WaitFlags fWaitFlagCloseStdOutStream = 0x00000004;

/** Flag to indicate that the `stderr` stream for a child should be closed before waiting on it.
 *  Note that doing so will truncate any remaining incoming data on the stream.  This is useful
 *  for closing the stream and exiting the reader thread when the parent process is no longer
 *  interested in the output of the child process (ie: the parent only wanted to wait on a
 *  successful startup signal from the child).  This flag has no effect if no read callback was
 *  provided for the `stderr` stream when the child process was launched.
 *
 *  @note [Linux] If this is used on a child process to close the parent process's end of the
 *        `stderr` stream, the child process will be terminated with SIGPIPE if it ever tries
 *        to write to `stderr` again.  This is the default handling of writing to a broken
 *        pipe or socket on Linux.  The only way around this default behavior is to ensure
 *        that the child process ignores SIGPIPE signals.  Alternatively, the parent could
 *        just wait for the child process to exit before destroying its process handle or
 *        closing the stream.
 */
constexpr WaitFlags fWaitFlagCloseStdErrStream = 0x00000008;

/** Flag to indicate that the wait should succeed when any of the flagged streams have been
 *  successfully waited on.  The default behavior is to wait for all flagged streams to be
 *  completed before returning or timing out.
 */
constexpr WaitFlags fWaitFlagAnyStream = 0x00000010;
/** @} */

/** Process kill flags.
 *  @{
 */
/** Base type for flags to the @ref carb::launcher::ILauncher::killProcess() function.  Valid flags for this
 *  type are the carb::launcher::fKillFlag* flags.
 */
using KillFlags = uint32_t;

/** Flag to indicate that any direct child processes of the process being terminated should
 *  also be terminated.  Note that this will not cause the full hierarchy of the process's
 *  ancestors to be terminated as well.  The caller should manage its process tree directly
 *  if multiple generations are to be terminated as well.
 */
constexpr KillFlags fKillFlagKillChildProcesses = 0x00000001;

/** Flag to indicate that a child process should be force killed.  This only has an effect
 *  on Linux where a SIGKILL signal will be sent to the process instead of SIGTERM.  This
 *  flag is ignored on Windows.  The potential issue with SIGTERM is that a process can
 *  trap and handle that signal in a manner other than terminating the process.  The SIGKILL
 *  signal however cannot be trapped and will always terminate the process.
 */
constexpr KillFlags fKillFlagForce = 0x00000002;

/** Flag to indicate that ILauncher::killProcess() or ILauncher::killProcessWithTimeout() calls
 *  should simply fail if a debugger is currently attached to the child process being terminated.
 *  The default behavior is to still attempt to kill the child process and wait for it to exit.
 *  On Linux, this will work as intended.  On Windows however, a process being debugged cannot
 *  be terminated without first detaching the debugger.  The attempt to terminate the child
 *  process will be queued for after the debugger has been attached.
 */
constexpr KillFlags fKillFlagFailOnDebugger = 0x00000004;

/** Flag to indicate that the ILauncher::killProcess() or ILauncher::killProcessWithTimeout()
 *  calls should not wait for the child process to fully exit before returning.  This allows
 *  calls to return more quickly, but could result in other functions such as
 *  ILauncher::isProcessActive() and ILauncher::getProcessExitCode() returning false results
 *  for a short period after ILauncher::killProcess() returns.  This period is usually only a
 *  few milliseconds, but may be inconsistent due to other system behavior and load.
 */
constexpr KillFlags fKillFlagSkipWait = 0x00000008;
/** @} */


/** Special value that can be passed to ILauncher::writeProcessStdin() for the @a bytes parameter
 *  to indicate that the input is a null terminated UTF-8 string.  When this special length value
 *  is used, it is the caller's responsibility to ensure the input string is actually null
 *  terminated.
 */
constexpr size_t kNullTerminated = ~0ull;

/** Special exit code to indicate that the process is still running and has not exited yet.
 *  This can be returned from ILauncher::waitProcessExit() and ILauncher::getProcessExitCode().
 */
constexpr ExitCode kStillActive = 0x8000000000000000ll;

/** Indicates an infinite timeout for use in the ILauncher::waitProcessExit() function in its
 *  @a timeout parameter.  The call will block until the requested process ends.
 */
constexpr uint64_t kInfiniteTimeout = ~0ull;

/** Return statuses for ILauncher::killProcessWithTimeout().  These indicate how the termination
 *  attempt completed.
 */
enum class KillStatus
{
    /** The child process was successfully terminated.  It will also have been confirmed to
     *  have exited fully if the @ref fKillFlagSkipWait flag was not used.  If the
     *  @ref fKillFlagSkipWait flag is used in the call, this status only indicates that the
     *  signal to terminate the child process was successfully sent.  The child process will
     *  exit at some point in the near future.  If a very short timeout was used in a call to
     *  ILauncher::killProcessWithTimeout(), and the child process had exited within that
     *  period, this will be returned, otherwise @ref KillStatus::eWaitFailed will be returned.
     *  In most situations, the time period between termination and when the child process fully
     *  exits will only be a few milliseconds.  However, within that time period, calls to
     *  functions such as ILauncher::isProcessActive() or ILauncher::getProcessExitCode() may
     *  return false results.
     */
    eSuccess,

    /** A debugger was attached to the child process at the time the termination attempt was
     *  made.  This will only be returned if the @ref fKillFlagFailOnDebugger flag is used
     *  and a debugger is attached to the child process.  No attempt to terminate the process
     *  will be made in this case.  A future call to ILauncher::killProcess() or
     *  ILauncher::killProcessWithTimeout() (once the debugger has been detached) will be
     *  needed to actually terminate the child process.
     */
    eDebuggerAttached,

    /** A debugger was attached to the child process and that prevented it from being terminated.
     *  This will only be returned on Windows.  A similar situation can still occur on Linux,
     *  except that on Linux the child process will be terminated successfully.  In that case,
     *  the debugger process will just be left in an invalid state where the only course of action
     *  is to detach from the terminated process.  Note that when this value is returned, the
     *  child process will be marked for termination by the system, but it will not actually be
     *  terminated until the debugger is detached from it.
     */
    eDebuggerFail,

    /** The attempt to signal the child process to terminate failed.  This can occur if the
     *  child process' handle is invalid or there is a permission problem.  This will not
     *  happen in most common situations.
     */
    eTerminateFailed,

    /** Waiting for the child process to exit failed or timed out.  When this is returned, the
     *  child process has still been successfully signaled to exit, but it didn't fully exit
     *  before the timeout expired.  This may still be viewed as a successful result however.
     *  This status code can be suppressed in successful cases with the @ref fKillFlagSkipWait
     *  flag.  That flag is especially useful when a zero timeout is desired but a successful
     *  result should still be returned.  If this value is returned, the caller is responsible
     *  for ensuring the child process successfully exits.  This state can be verified with
     *  calls such as ILauncher::isProcessActive(), ILauncher::getProcessExitCode(), and
     *  ILauncher::waitProcessExit().
     */
    eWaitFailed,

    /** An invalid parameter was passed into ILauncher::killProcessWithTimeout(). */
    eInvalidParameter,
};

/** Standard command interpreters for Windows and Linux.  These can be used in the launch
 *  descriptor's @ref LaunchDesc::interpreter value to override some default interpreter
 *  detection functionality.
 *
 *  @note These interpreter names are just 'safe' interpreters.  If a caller has additional
 *        knowledge of the functional requirements of a script (ie: requires python 3.6+,
 *        requires a specific install of python, requires additional options for the interpreter,
 *        etc), it is the caller's responsibility to ensure an appropriate interpreter path and
 *        command is provided in @ref LaunchDesc::interpreter.  If no interpreter path is given
 *        in the launch descriptor, one of these interpreters will be chosen based on the
 *        extension of the script file.
 *
 *  @note If you need to use `cmd /C`, you must use @ref kInterpreterShellScript so that ILauncher
 *        can properly quote your arguments, since `cmd /C` does not interpret a command argument
 *        in the way that almost every other interpreter does.
 */
#if CARB_PLATFORM_WINDOWS
constexpr const char* const kInterpreterShellScript = "cmd /C";
constexpr const char* const kInterpreterShellScript2 = "cmd /C";
#else
constexpr const char* const kInterpreterShellScript = "sh";

/** @copydoc kInterpreterShellScript */
constexpr const char* const kInterpreterShellScript2 = "bash";
#endif

/** Interpreter names for python scripts.  Using this command assumes that at least one version
 *  of python is installed locally on the system and is available through the system PATH
 *  variable.  It is the caller's responsibility to ensure that a global python instance is
 *  installed on the calling system before either using this interpreter string in a launch
 *  descriptor or attempting to run a python script with a `nullptr` interpreter.
 *
 *  To check if the global python interpreter is installed on the calling system, a call to
 *  ILauncher::launchProcess() with a @ref kInterpreterPythonCommand as the interpreter and
 *  the simple script "quit" can be used.  If ILauncher::launchProcess() succeeds, the global
 *  python interpreter is installed.  If it fails, it is not installed.  In the latter case,
 *  it is the caller's responsibility to either find or install an appropriate python interpreter
 *  before attempting to launch a python script.
 */
constexpr const char* const kInterpreterPythonScript = "python";

/** @copydoc kInterpreterPythonScript */
constexpr const char* const kInterpreterPythonCommand = "python -c";


/** Descriptor of the new process to be launched by ILauncher::openProcess().  This contains
 *  all the information needed to launch and communicate with the new child process.
 */
struct LaunchDesc
{
    /** A vector of command line arguments for the new process to launch.  This may not be
     *  `nullptr`.  The first argument in the vector must be the path to the executable to run.
     *  This may be a relative or absolute path.  All following arguments will be passed to the
     *  executable as command line arguments.  Each string must be UTF-8 encoded.  Note that if
     *  a relative path is used for the first argument, it must be given relative to the current
     *  working directory for the parent (ie: calling) process, not the path given by @ref path.
     *  The @ref path value will become the current working directory for the child process, not
     *  the path to find the executable image at.
     */
    const char* const* argv = nullptr;

    /** The total number of arguments in the @ref argv vector. */
    size_t argc = 0;

    /** The optional initial working directory of the new process.  If not specified, this will
     *  default to the calling process's current working directory.  Once the child process has
     *  been successfully started, its current working directory will be set to this path.  This
     *  will neither affect the current working directory of the parent process nor will it be
     *  used as the path to find the child process' executable.
     */
    const char* path = nullptr;

    /** Callback to be performed whenever data is successfully read from the child process's
     *  `stdout` or `stderr` streams.  These may be `nullptr` if nothing needs to be read from the
     *  child process's `stdout` or `stderr` streams.  Note that providing a callback here will
     *  spawn a new thread per callback to service read requests on the child process's `stdout`
     *  or `stderr` streams.  See @ref OnProcessReadFn for more information on how these callbacks
     *  should be implemented and what their responsibilities are.
     *
     *  @note [Linux] If this is non-`nullptr` and the parent process destroys its process handle
     *        for the child process before the child process exits, the child process will be
     *        terminated with SIGPIPE if it ever tries to write to the corresponding stream again
     *        This is the default Linux kernel behavior for writing to a broken pipe or socket.
     *        It is best practice to first wait for the child process to exit before destroying
     *        the process handle in the parent process.
     */
    OnProcessReadFn onReadStdout = nullptr;

    /** @copydoc onReadStdout */
    OnProcessReadFn onReadStderr = nullptr;

    /** The opaque context value to be passed to the read callbacks when they are performed.  This
     *  will never be accessed directly by the process object, just passed along to the callbacks.
     *  It is the responsibility of the callbacks to know how to appropriately interpret these
     *  values.  These values are ignored if both @ref onReadStdout and @ref onReadStderr are
     *  `nullptr`.
     */
    void* readStdoutContext = nullptr;

    /** @copydoc readStdoutContext */
    void* readStderrContext = nullptr;

    /** Flags to control the behavior of the new child process.  This is zero or more of the
     *  @ref LauncherFlags flags.
     */
    LauncherFlags flags = 0;

    /** A hint for the size of the buffer to use when reading from the child process's `stdout`
     *  and `stderr` streams.  This represents the maximum amount of data that can be read at
     *  once and returned through the onRead*() callbacks.  This is ignored if both
     *  @ref onReadStdout and @ref onReadStderr are `nullptr`.
     *
     *  Note that this buffer size is only a hint and may be adjusted internally to meet a
     *  reasonable minimum read size for the platform.
     */
    size_t bufferSize = kDefaultProcessBufferSize;

    /** A block of environment variables to pass to the child process.  If the @ref flags
     *  include the @ref fLaunchFlagNoInheritEnv flag, this environment block will be used
     *  explicitly if non-`nullptr`.  If that flag is not used, the calling process's current
     *  environment will be prepended to the block specified here.  Any environment variables
     *  specified in here will replace any variables in the calling process's environment
     *  block.  Each string in this block must be UTF-8 encoded.  If this is `nullptr`, the
     *  default behavior is for the child process to inherit the entire environment of the
     *  parent process.  If an empty non-`nullptr` environment block is specified here, the
     *  child process will be launched without any environment.
     */
    const char* const* env = nullptr;

    /** The number of environment variables specified in the environment block @ref env.  This
     *  may only be 0 if the environment block is empty or `nullptr`.
     */
    size_t envCount = 0;

    /** An optional command interpreter name to use when launching the new process.  This can be
     *  used when launching a script file (ie: shell script, python script, etc).  This must be
     *  `nullptr` if the command being launched is a binary executable.  If this is `nullptr`
     *  and a script file is being executed, an attempt will be made to determine the appropriate
     *  command interpreter based on the file extension of the first argument in @ref argv.
     *  The ILauncher::launchProcess() call may fail if this is `nullptr`, an appropriate
     *  interpreter could not be found for the named script, and the named script could not
     *  be launched directly on the calling platform (ie: a script using a shebang on its
     *  first line will internally specify its own command interpreter).  This value is ignored
     *  if the @ref fLaunchFlagScript flag is not present in @ref flags.
     *
     *  This can be one of the kInterpreter* names to use a standard command interpreter that
     *  is [presumably] installed on the system.  If a specific command interpreter is to be
     *  used instead, it is the caller's responsibility to set this appropriately.
     *
     *  @note This interpreter process will be launched with a search on the system's 'PATH'
     *        variable.  On Windows this is always the behavior for launching any process.
     *        However, on Linux a process must always be identified by its path (relative or
     *        absolute) except in the case of launching a command interpreter.
     */
    const char* interpreter = nullptr;

    /** Optional names of log files to redirect `stdout` and `stderr` output from the child process
     *  to in lieu of a callback.  The output from these streams won't be visible to the parent
     *  process (unless it is also reading from the log file).  If either of these are
     *  non-`nullptr` and not an empty string, and the callbacks (@ref onReadStdout or
     *  @ref onReadStderr) are also `nullptr`, the corresponding stream from the child process
     *  will be redirected to these files(s) on disk.  It is the caller's responsibility to ensure
     *  the requested filename is valid and writable.  If the log fails to open, launching the
     *  child process will be silently ignored or fail depending on whether the
     *  @ref fLaunchFlagAllowBadLog flag is used or not.  These logs will be ignored if any
     *  of the @ref fLaunchFlagNoStdStreams flags are used.  The log file corresponding to the
     *  flag(s) used will be disabled.
     *
     *  When a log is requested, it will be opened in a sharing mode.  The log will be opened
     *  with shared read and write permissions.  If the named log file already exists, it will
     *  always be truncated when opened.  It is the caller's responsibility to ensure the previous
     *  file is moved or renamed if its contents need to be preserved.  The log will also be
     *  opened in 'append' mode (ie: as if "a+" had been passed to fopen()) so that all new
     *  messages are written at the end of the file.  It is the child process's responsibility
     *  to ensure that all messages written to the log files are synchronized so that messages
     *  do not get incorrectly interleaved.  If both log files are given the same name, it is
     *  also the child process's responsibility to ensure writes to `stdout` and `stderr` are
     *  appropriately serialized.
     *
     *  Setting log file names here will behave roughly like shell redirection to a file.  The
     *  two streams can be independently specified (as in "> out.txt & err.txt"), or they can
     *  both be redirected to the same file (as in "&> log.txt" or > log.txt & log.txt").  The
     *  files will behave as expected in the child process - writing to `stdout` will be buffered
     *  while writing to `stderr` will be unbuffered.  If the named log file already exists, it
     *  will always be overwritten and truncated.
     *
     *  This filename can be either an absolute or relative path.  Relative paths will be opened
     *  relative to the current working directory at the time.  The caller is responsible for
     *  ensuring all path components leading up to the log filename exist before calling.  Note
     *  that the log file(s) will be created before the child process is launched.  If the child
     *  process fails to launch, it will be the caller's responsibility to clean up the log file
     *  if necessary.
     *
     *  @note if a non-`nullptr` callback is given for either `stdout` or `stderr`, that will
     *        override the log file named here.  Each log file will only be created if its
     *        corresponding callback is not specified.
     */
    const char* stdoutLog = nullptr;

    /** @copydoc stdoutLog */
    const char* stderrLog = nullptr;

    /** Reserved for future expansion.  This must be `nullptr`. */
    void* ext = nullptr;
};


// ********************************** interface declaration ***************************************
/** A simple process launcher helper interface.  This is responsible for creating child processes,
 *  tracking their lifetime, and communicating with the child processes.  All operations on this
 *  interface are thread safe within themselves.  It is the caller's responsibility to manage the
 *  lifetime of and any multi-threaded access to each process handle object that is returned from
 *  launchProcess().  For example, destroying a process handle object while another thread is
 *  operating on it will result in undefined behavior.
 *
 *  Linux notes:
 *    * If any other component of the software using this plugin sets the SIGCHLD handler back to
 *      default behavior (ie: SIG_DFL) or installs a SIGCHLD handler function, any child process
 *      that exits while the parent is still running will become a zombie process.  The only way
 *      to remove a zombie process in this case is to wait on it in the parent process with
 *      waitProcessExit() or getProcessExitCode() at some point after the child process has
 *      exited.  Explicitly killing the child process with killProcess() will also avoid creating
 *      a zombie process.
 *    * Setting the SIGCHLD handler to be ignored (ie: SIG_IGN) will completely break the ability
 *      to wait on the exit of any child process for this entire process.  Any call to wait on a
 *      child process such as waitProcessExit() or getProcessExitCode() will fail immediately.
 *      There is no way to detect or work around this situation on the side of this interface
 *      since signal handlers are process-wide and can be installed at any time by any component
 *      of the process.
 *    * The problem with leaving a zombie processes hanging around is that it continues to take
 *      up a slot in the kernel's process table.  If this process table fills up, the kernel
 *      will not be able to launch any new processes (or threads).  All of the zombie children
 *      of a process will be automatically cleaned up when the parent exits however.
 *    * If a read callback is provided for either `stdout` or `stderr` of the child process and
 *      the parent process destroys its process handle for the child process before the child
 *      process exits, the child process will be terminated with SIGPIPE if it ever tries to
 *      write to either stream again.  This is the default Linux kernel behavior for writing
 *      to a broken pipe or socket.  The only portable way around this is to ensure the child
 *      process will ignore SIGPIPE signals.  It is generally best practice to ensure the parent
 *      process waits on all child processes before destroying its process handle.  This also
 *      prevents the appearance of zombie processes as mentioned above.
 *
 *  Windows notes:
 *    * Reading from a child process's `stdout` or `stderr` streams will not necessarily be
 *      aligned to the end of a single 'message' (ie: the contents of a single write call on the
 *      child process's end of the stream).  This means that a partial message may be received in
 *      the callbacks registered during the launch of a child process.  It is left up to the
 *      caller to accumulate input from these streams until an appropriate delimiter has been
 *      reached and the received data can be fully parsed.  This may be fixed in a future version
 *      of this interface.
 */
struct ILauncher
{
    CARB_PLUGIN_INTERFACE("carb::launcher::ILauncher", 1, 3)

    /** Launches a new child process.
     *
     *  @param[in] desc     A descriptor of the child process to launch.  At least the
     *                      @ref LaunchDesc::argc and @ref LaunchDesc::argv members must be
     *                      filled in.  Default values on all other members are sufficient.
     *  @returns A new process object if the child process is successfully launched.  This must
     *           be destroyed with destroyProcessHandle() when it is no longer needed by the
     *           caller.  Note that closing the process object will not terminate the child
     *           process.  It simply means that the calling process can no longer communicate
     *           with or kill the child process.  If the child process needs to be killed first,
     *           it is the caller's responsibility to call kill() before destroying the handle.
     *  @returns `nullptr` if the new process could not be launched for any reason.  This may
     *           include insufficient permissions, failed memory or resource allocations, etc.
     *
     *  @remarks This attempts to launch a new child process.  The new process will be created
     *           and start to run before successful return here.  Depending on what flags and
     *           callbacks are provided in the launch descriptor, it may be possible for this
     *           process to communicate with the new child process.
     *
     *  @note On Linux, the child process's executable should not have the set-user-ID or set-
     *        group-ID capabilities set on it when using the @ref carb::launcher::fLaunchFlagKillOnParentExit
     *        flag.  These executables will clear the setting that allows the child process
     *        to receive a signal when the parent process exits for any reason (ie: exits
     *        gracefully, crashes, is externally terminated, etc).  Setting these capabilities
     *        on an executable file requires running something along the lines of:
     *          * @code{.sh}
     *            sudo setcap cap_setuid+ep <filename>
     *            sudo setcap cap_setgid+ep <filename>
     *            @endcode
     *
     *  @note Any built executable will, be default, not have any of these capabilities set.
     *        They will have to be explicitly set by a user, installer, or script.  These
     *        executables can be identified in an "ls -laF" output by either being highlighted
     *        in red (default bash color scheme), or by having the '-rwsr-xr-x' permissions
     *        (or similar) instead of '-rwxr-xr-x', or by passing the executable path to
     *        'capsh --print' to see what it's effective or permissive capabilities are.
     *
     *  @note If a set-user-ID or set-group-ID executable needs to be launched as a child process
     *        and the @ref fLaunchFlagKillOnParentExit flag is desired for it, the child process
     *        should call @code{.cpp}prctl(PR_SET_PDEATHSIG, SIGTERM)@endcode early in its main()
     *        function to regain this behavior.  The restoreParentDeathSignal() helper function
     *        is also offered here to make that call easier in Carbonite apps.  Note however that
     *        there will be a possible race condition in this case - if the parent exits before
     *        the child process calls prctl(), the child process will not be signaled.  Also note
     *        that unconditionally calling one of these functions in a child process will result
     *        in the @ref fLaunchFlagKillOnParentExit behavior being imposed on that child
     *        process regardless of whether the flag was used by the parent process.
     *
     *  @note Since we can neither guarantee the child process will be a Carbonite app nor that
     *        it will even be source under control of the same developer (ie: a command line
     *        tool), resetting this death signal is unfortunately not handled as a task in this
     *        interface.
     */
    Process*(CARB_ABI* launchProcess)(LaunchDesc& desc);

    /** Launches a detached child process.
     *
     *  @param[in] desc     A descriptor of the child process to launch.  At least the
     *                      @ref LaunchDesc::argc and @ref LaunchDesc::argv members must be
     *                      filled in.  Default values on all other members are sufficient.
     *                      Note that setting a read callback for `stdout` or `stderr` is
     *                      not allowed in a detached process.  The @ref LaunchDesc::onReadStderr
     *                      and @ref LaunchDesc::onReadStdout parameters will be cleared to
     *                      `nullptr` before launching the child process.  Callers may still
     *                      redirect `stdout` and `stderr` to log files however.
     *  @returns The process ID of the new child process if successfully launched.
     *  @retval kBadId if the new process could not be launched for any reason.  This may
     *           include insufficient permissions, failed memory or resource allocations, etc.
     *
     *  @remarks This is a convenience version of launchProcess() that launches a child process
     *           but does not return the process handle.  Instead the operating system's process
     *           ID for the new child process is returned.  This is intended to be used for
     *           situations where the parent process neither needs to communicate with the child
     *           process nor know when it has exited.  Using this should be reserved for child
     *           processes that manage their own lifetime and communication with the parent in
     *           another prearranged manner.  The returned process ID may be used in OS level
     *           process management functions, but is not useful to pass into any other ILauncher
     *           functions.
     */
    ProcessId launchProcessDetached(LaunchDesc& desc)
    {
        ProcessId id;
        Process* proc;

        // registering read callbacks is not allowed in a detached process since we'll be
        // immediately destroying the handle before return.  If there were set the child
        // process would be killed on Linux if it ever tried to write to one of its streams.
        // On both Windows and Linux having these as non-`nullptr` would also cause a lot
        // of unnecessary additional work to be done both during and after the launch.
        desc.onReadStderr = nullptr;
        desc.onReadStdout = nullptr;

        proc = launchProcess(desc);

        if (proc == nullptr)
            return kBadId;

        id = getProcessId(proc);
        destroyProcessHandle(proc);
        return id;
    }

    /** Destroys a process handle when it is no longer needed.
     *
     *  @param[in] process  The process handle to destroy.  This will no longer be valid upon
     *                      return.  This call will be silently ignored if `nullptr` is passed in.
     *  @returns No return value.
     *
     *  @remarks This destroys a process handle object that was previously returned by a call
     *           to launchProcess().  The process handle will no longer be valid upon return.
     *
     *  @note Calling this does *not* kill the child process the process handle refers to.
     *        It simply recovers the resources taken up by the process handle object.  In order
     *        to kill the child process, the caller must call killProcess() first.
     *
     *  @note [Linux] If the child process was started with a read callback for one or both of
     *        its standard streams, destroying the handle here will cause the child process to
     *        be terminated with SIGPIPE if it ever tries to write to any of the standard streams
     *        that were redirected to the read callbacks.  This is the default Linux kernel
     *        behavior for attempting to write to a broken pipe or socket.  The only portable
     *        way to work around this is to ensure the child process ignores SIGPIPE signals.
     *        However, the general best practice is for the parent process to wait for the
     *        child process to exit (or explicitly kill it) before destroying the process handle.
     */
    void(CARB_ABI* destroyProcessHandle)(Process* process);

    /** Retrieves the process identifier for a child process.
     *
     *  @param[in] process  The process handle object representing the child process to retrieve
     *                      the identifier for.  This may not be `nullptr`.
     *  @returns The integer identifier of the child process if it is still running.
     *  @retval kBadId if the child process if it is not running.
     *
     *  @remarks This retrieves the process identifier for a child process.  This can be used
     *           to gain more advanced platform specific access to the child process if needed,
     *           or simply for debugging or logging identification purposes.
     */
    ProcessId(CARB_ABI* getProcessId)(Process* process);

    /** Waits for a child process to exit.
     *
     *  @param[in] process  The process handle object representing the child process to wait
     *                      for.  This may not be `nullptr`.  This is returned from a previous
     *                      call to launchProcess().
     *  @param[in] timeout  The maximum time in milliseconds to wait for the child process to
     *                      exit.  This may be @ref kInfiniteTimeout to specify that this should
     *                      block until the child process exits.  This may be 0 to simply poll
     *                      whether the child process has exited or not.
     *  @returns The exit code of the child process if it successfully exited in some manner.
     *  @retval kStillActive if the wait timed out and the child process had not exited yet.
     *  @returns `EXIT_FAILURE` if @p process is `nullptr`.
     *
     *  @remarks This waits for a child process to exit and retrieves its exit code if the exit
     *           has occurred.  Note that this does not in any way signal the child process to
     *           exit.  That is left up to the caller since that method would be different for
     *           each child process.  This simply waits for the exit to occur.  If the child
     *           process doesn't exit within the allotted timeout period, it will remain running
     *           and the special @ref kStillActive exit code will be returned.
     *
     *  @note Despite the timeout value being a 64-bit value, on Windows this will only wait
     *        for up to ~49.7 days at a time (ie: 32 bits) for the child process to exit.  This
     *        is due to all the underlying timing functions only taking a 32-bit timeout value.
     *        If a longer wait is required, the wait will be repeated with the remaining time.
     *        However, if this process is blocking for that long waiting for the child process,
     *        that might not be the most desirable behavior and a redesign might be warranted.
     *
     *  @note [Linux] if the calling process sets a SIG_IGN signal handler for the SIGCHLD signal,
     *        this call will fail immediately regardless of the child process's current running
     *        state.  If a handler function is installed for SIGCHLD or that signal's handler is
     *        still set to its default behavior (ie: SIG_DFL), the child process will become a
     *        zombie process once it exits.  This will continue to occupy a slot in the kernel's
     *        process table until the child process is waited on by the parent process.  If the
     *        kernel's process table fills up with zombie processes, the system will no longer
     *        be able to create new processes or threads.
     *
     *  @note [Linux] It is considered an programming error to allow a child process to be leaked
     *        or to destroy the process handle without first waiting on the child process.  If
     *        the handle is destroyed without waiting for the child process to [successfully] exit
     *        first, a zombie process will be created.  The only exception to this is if the
     *        parent process exits before the child process.  In this case, any zombie child
     *        child processes will be inherited by the init (1) system process which will wait
     *        on them and recover their resources.
     *
     *  @note In general it is best practice to always wait on all child processes before
     *        destroying the process handle object.  This guarantees that any zombie children
     *        will be removed and that all resources will be cleaned up from the child process.
     */
    ExitCode(CARB_ABI* waitProcessExit)(Process* process, uint64_t timeout);

    /** Attempts to retrieve the exit code for a child process.
     *
     *  @param[in] process  The process handle object representing the child process to retrieve
     *                      the exit code for.  The child process may or may not still be running.
     *                      This may not be `nullptr`.
     *  @returns The exit code of the child process if it has already exited in some manner.
     *  @retval kStillActive if the child process is still running.
     *  @returns `EXIT_FAILURE` if @p process is `nullptr`.
     *
     *  @remarks This attempts to retrieve the exit code for a child process if it has exited.
     *           The exit code isn't set until the child process exits, is killed, or crashes.
     *           The special exit code @ref kStillActive will be returned if the child process
     *           is still running.
     *
     *  @note [Linux] see the notes about zombie processes in waitProcessExit().  These also
     *        apply to this function since it will also perform a short wait to see if the
     *        child process has exited.  If it has exited already, calling this will also
     *        effectively clean up the child zombie process.  However, if another component
     *        of this process has set the SIGCHLD signal handler to be ignored (ie: SIG_IGN),
     *        this call will also fail immediately.
     */
    ExitCode(CARB_ABI* getProcessExitCode)(Process* process);

    /** Writes a buffer of data to the stdin stream of a child process.
     *
     *  @param[in] process  The process handle object representing the child process to write
     *                      the data to.  This may not be `nullptr`.  The child process must
     *                      have been opened using the @ref fLaunchFlagOpenStdin flag in
     *                      order for this to succeed.
     *  @param[in] data     The buffer of data to write to the child process.
     *  @param[in] bytes    The total number of bytes to write to the child process or the
     *                      special value @ref kNullTerminated to indicate that the buffer
     *                      @p data is a null-terminated UTF-8 string.  If the latter value
     *                      is used, it is the caller's responsibility to ensure the data
     *                      buffer is indeed null terminated.
     *  @returns `true` if the buffer of data is successfully written to the child process's
     *           stdin stream.
     *  @returns `false` if the entire data buffer could not be written to the child process's
     *           stdin stream.  In this case, the child process's stdin stream will be left
     *           in an unknown state.  It is the responsibility of the caller and the child
     *           process to negotiate a way to resynchronize the stdin stream.
     *  @returns `false` if @p process is `nullptr`.
     *
     *  @remarks This attempts to write a buffer of data to a child process's stdin stream.
     *           This call will be ignored if the child process was not launched using the
     *           @ref fLaunchFlagOpenStdin flag or if the stdin handle for the process has
     *           since been closed with closeProcessStdin().  The entire buffer will be
     *           written to the child process's stdin stream.  If even one byte cannot be
     *           successfully written, this call will fail.  This can handle data buffers
     *           larger than 4GB if needed.  This will block until all data is written.
     *
     *  @remarks When the @ref kNullTerminated value is used in @p bytes, the data buffer is
     *           expected to contain a null terminated UTF-8 string.  The caller is responsible
     *           for ensuring this.  In the case of a null terminated string, all of the string
     *           up to but not including the null terminator will be sent to the child process.
     *           If the null terminator is to be sent as well, the caller must specify an
     *           explicit byte count.
     */
    bool(CARB_ABI* writeProcessStdin)(Process* process, const void* data, size_t bytes);

    /** Closes the stdin stream of a child process.
     *
     *  @param[in] process  The process handle object representing the child process to close
     *                      the stdin stream for.  This may not be `nullptr`.
     *  @returns No return value.
     *
     *  @remarks This closes the stdin stream of a child process.  This call will be ignored if
     *           the stdin stream for the child process either was never opened or if it has
     *           already been closed by a previous call to closeProcessStdin().  Closing the
     *           stdin stream only closes the side of the stream that is owned by the calling
     *           process.  It will not close the actual stream owned by the child process itself.
     *           Other processes may still retrieve the child process's stdin stream handle in
     *           other ways if needed to communicate with it.
     */
    void(CARB_ABI* closeProcessStdin)(Process* process);

    /** Kills a running child process.
     *
     *  @param[in] process  The process handle object representing the child process to kill.
     *                      This may not be `nullptr`.
     *  @param[in] flags    Flags to affect the behavior of this call.  This may be zero or
     *                      more of the @ref KillFlags flags.
     *  @returns No return value.
     *
     *  @remarks This kills a running child process.  If the process has already exited on its
     *           own, this call will be ignored.  The child process will be terminated without
     *           being given any chance for it to clean up any of its state or save any
     *           information to persistent storage.  This could result in corrupted data if
     *           the child process is in the middle of writing to persistent storage when it
     *           is terminated.  It is the caller's responsibility to ensure the child process
     *           is as safe as possible to terminate before calling.
     *
     *  @remarks On both Windows and Linux, this will set an exit code of either 137 if the
     *           @ref fKillFlagForce flag is used or 143 otherwise.  These values are the
     *           exit codes that are set in Linux by default for a terminated process.  They
     *           are 128 plus the signal code used to terminate the process.  In the case of
     *           a forced kill, this sends SIGKILL (9).  In the case of a normal kill, this
     *           sends SIGTERM (15).  On Windows, this behavior is simply mimicked.
     *
     *  @note On Linux, the @ref fKillFlagForce flag is available to cause the child process to
     *        be sent a SIGKILL signal instead of the default SIGTERM.  The difference between
     *        the two signals is that SIGTERM can be caught and handled by the process whereas
     *        SIGKILL cannot.  Having SIGTERM be sent to end the child process on Linux does
     *        allow for the possibility of a graceful shutdown for apps that handle the signal.
     *        On Linux, it is recommended that if a child process needs to be killed, it is
     *        first sent SIGTERM (by not using the @ref fKillFlagForce flag), then if after a
     *        short time (ie: 2-5 seconds, depending on the app's shutdown behavior) has not
     *        exited, kill it again using the @ref fKillFlagForce flag.
     */
    void(CARB_ABI* killProcess)(Process* process, KillFlags flags);

    /** Tests whether a process is still running.
     *
     *  @param[in] process  The process handle object representing the child process to query.
     *                      This may not be `nullptr`.
     *  @returns `true` if the given child process is still running.
     *  @returns `false` if the child process has exited.
     */
    inline bool isProcessActive(Process* process)
    {
        return getProcessExitCode(process) == kStillActive;
    }

    /** @copydoc killProcess().
     *
     *  @param[in] timeout  The time in milliseconds to wait for the child process to fully
     *                      exit.  This may be 0 to indicate that no wait should occur after
     *                      signaling the child process to terminate.  This may also be
     *                      @ref kInfiniteTimeout to wait indefinitely for the child process
     *                      to exit.
     *  @retval KillStatus::eSuccess if the child process is successfully terminated
     *           and was confirmed to have exited or the @ref fKillFlagSkipWait flag is used
     *           and the child process is successfully signaled to exit, or if the child process
     *           had already terminated on its own or was otherwise killed before this call.
     *  @retval KillStatus::eWaitFailed if the child process was successfully signaled to
     *           exit but the timeout for the wait for it to fully exit expired before it exited.
     *           This may still indicate successful termination, but it is left up to the caller
     *           to determine that.
     *  @returns Another @ref KillStatus code if the termination failed in any way.
     *
     *  @remarks This variant of killProcess() can be used to have more control over how the
     *           child process is terminated and whether it was successful or not.  The timeout
     *           allows the caller control over how long the call should wait for the child
     *           process to exit.  This can also be used in combination with other functions
     *           such as ILauncher::isDebuggerAttached() to figure out how and when a child
     *           process should be terminated.
     */
    KillStatus(CARB_ABI* killProcessWithTimeout)(Process* process, KillFlags flags, uint64_t timeout);

    /** Tests whether a debugger is currently attached to a child process.
     *
     *  @param[in] process  The child process to check the debugger status for.  This may not be
     *                      `nullptr`.  This may be a handle to a child process that has already
     *                      exited.
     *  @returns `true` if there is a debugger currently attached to the given child process.
     *  @returns `false` if the child process has exited or no debugger is currently attached to
     *           it.
     *  @returns `false` if @p process is `nullptr`.
     *
     *  @remarks This tests whether a debugger is currently attached to the given child process.
     *           On Windows at least, having a debugger attached to a child process will prevent
     *           it from being terminated with killProcess().  This can be queried to see if the
     *           debugger task has completed before attempting to kill it.
     */
    bool(CARB_ABI* isDebuggerAttached)(Process* process);

    /** Waits for one or more standard streams from a child process to end.
     *
     *  @param[in] process  The child process to wait on the standard streams for.  This may not
     *                      be `nullptr`.  This may be a handle to a child process that has
     *                      already exited.  This call will be ignored if the flagged stream(s)
     *                      were not opened for the given child process.
     *  @param[in] flags    Flags to control how the operation occurs and which stream(s) to wait
     *                      on.  At least one flag must be specified.  This may not be 0.  It is
     *                      not an error to specify flags for a stream that was not opened by the
     *                      child process.  In this case, the corresponding flag(s) will simply be
     *                      ignored.
     *  @param[in] timeout  The maximum amount of time in milliseconds to wait for the flagged
     *                      streams to end.  This may be @ref kInfiniteTimeout to wait infinitely
     *                      for the stream(s) to end.  A stream ends when it is closed by either
     *                      the child process (by it exiting or explicitly closing its standard
     *                      streams) or by the parent process closing it by using one of the
     *                      @ref fWaitFlagCloseStdOutStream or @ref fWaitFlagCloseStdErrStream
     *                      flags.  When a stream ends due to the child process closing it, all
     *                      of the pending data will have been consumed by the reader thread and
     *                      already delivered to the read callback function for that stream.
     *  @returns `true` if the wait operation specified by @p flags completed successfully.  This
     *           means that at least one, possibly both, streams have ended and been fully
     *           consumed.
     *  @returns `false` if the flagged streams did not end within the timeout period.
     *
     *  @remarks This is used to wait on one or more of the standard streams (`stdout` or `stderr`)
     *           from the child process to end.  This ensures that all data coming from the
     *           child process has been consumed and delivered to the reader callbacks.
     *
     *  @note This does not allow for waiting on the standard streams of child processes that
     *        are writing directly to log files (ie: using the @ref LaunchDesc::stdoutLog and
     *        @ref LaunchDesc::stderrLog parameters).  This only affects child processes that
     *        were launched with a read callback specified for `stdout`, `stderr`, or both.
     */
    bool(CARB_ABI* waitForStreamEnd)(Process* process, WaitFlags flags, uint64_t timeout);
};


/** Restores the parent death signal on set-user-ID and set-group-ID images.
 *
 *  @param[in] flags    Flags to indicate which signal should be sent on on parent process
 *                      exit.  This may be 0 to cause the child process to be asked to
 *                      gracefully terminate or @ref fKillFlagForce to forcibly terminate
 *                      the child process.  In the latter case, the child process will not
 *                      get any chance to clean up or release resources.
 *  @returns No return value.
 *
 *  @remarks This restores the parent death signal on Linux after executing an image file
 *           that has a set-user-ID or set-group-ID capability set on it.  Unfortunately,
 *           executing one of these image files has the result of clearing the parent
 *           death signal attribute that is set when using the @ref fLaunchFlagKillOnParentExit
 *           flag.  These capabilities are set on the image file with the "setcap" command
 *           line tool and require super user access.  The capabilities of an executable image
 *           file can be discovered with the "capsh --print" command line tool.  See the notes
 *           on ILauncher::launch() for more information.
 *
 *  @note This only needs to be called when a child process's image file has a set-user-ID
 *        or set-group-ID capability set on it on Linux and the @ref fLaunchFlagKillOnParentExit
 *        flag was used when launching the child process.  This is still safe to call on
 *        Windows and will simply do nothing.  Note that calling this when the child was not
 *        launched with the @ref fLaunchFlagKillOnParentExit flag will cause that behavior
 *        to be imposed on the child process.  Communicating the need for this call is left
 *        as an exercise for both the child and parent processes.
 */
inline void restoreParentDeathSignal(KillFlags flags = 0)
{
    CARB_UNUSED(flags);
#if CARB_PLATFORM_LINUX
    prctl(PR_SET_PDEATHSIG, (flags & fKillFlagForce) != 0 ? SIGKILL : SIGTERM);
#endif
}

} // namespace launcher
} // namespace carb
