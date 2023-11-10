// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Provides an interface to allow for more detailed assertion failure dialogues.
 */
#pragma once

// NOTE: this interface is included and used in a very low level header.  No more headers than are
//       absolutely necessary should be included from here.
#include "../Interface.h"

#include <stdarg.h>

/** Namespace for all low level Carbonite functionality. */
namespace carb
{
/** Namespace for all assertion checking helpers and interfaces. */
namespace assert
{

/** Base type for the flags that control the behavior of CARB_ASSERT() and CARB_CHECK() failure reporting. */
using AssertFlags = uint32_t;

/** Flag to indicate that the assertion confirmation dialog should always be skipped for this
 *  process.  When set, the dialog will be assumed to always behave as though the button to
 *  break into the debugger had been pressed.  When clear, the dialog will show based on
 *  platform defaults.
 */
constexpr AssertFlags fAssertSkipDialog = 0x00000001;

/** Flag to indicate that the software breakpoint that is triggered on a failing assertion
 *  is to be ignored.  When set, the breakpoint will never be executed and execution will
 *  simply continue after reporting the failed assertion to the logs.  When clear, the
 *  breakpoint will be executed normally.
 */
constexpr AssertFlags fAssertSkipBreakpoint = 0x00000002;

/** Flag to indicate that the assertion should not produce any console output. When set, an
 *  assertion will not print anything to the console, but the failure count will increase.
 */
constexpr AssertFlags fAssertNoConsole = 0x00000004;


/** Interface to provide functionality to display assertion failures in greater detail.  This
 *  provides a way to encapsulate OS specific functionality for gathering additional information,
 *  displaying dialogues, and providing additional debugging functionality.  Without this interface,
 *  assertion failure reports are only really limited to log messages and software breakpoints.
 */
struct IAssert
{
    CARB_PLUGIN_INTERFACE("carb::assert::IAssert", 1, 0)

    /** Sets, clears, or retrieves the assertion behavioral flags.
     *
     *  @param[in] set      A mask of zero or more flags to enable.  This may be 0 to indicate
     *                      that no new flags should be set.  This is a combination of the
     *                      fAssert* flags.
     *  @param[in] clear    A mask of zero or more flags to disable.  This may be 0 to indicate
     *                      that no new flags should be cleared.  This is a combination of the
     *                      fAssert* flags.
     *  @returns The flags immediately before set/clear changes were applied.
     *
     *  @note This is always thread safe; changes are applied atomically. Also, set and clear can be 0
     *        to retrieve the current set of flags.
     */
    AssertFlags(CARB_ABI* setAssertionFlags)(AssertFlags set, AssertFlags clear);

    /** Retrieves the count of how many assertions have failed.
     *
     *  @returns The number of assertions that have failed in the calling process.
     *
     *  @remarks This retrieves the current number of assertion failures that have occurred in the
     *           calling process.  This may be combined with using the @ref fAssertSkipBreakpoint
     *           and @ref fAssertSkipDialog flags to allow certain assertions to continue in a
     *           'headless' mode whose behavior can then be monitored externally.
     */
    uint64_t(CARB_ABI* getAssertionFailureCount)();

    /** Reports the failure of an assertion condition to all applicable destinations.
     *
     *  @param[in] condition    The text describing the failed assertion condition.  This may not be
     *                          `nullptr`.
     *  @param[in] file         The name of the file that the assertion is in.  This may not be
     *                          `nullptr`.
     *  @param[in] func         The name of the function that the assertion is in.  This may not be
     *                          `nullptr`.
     *  @param[in] line         The line number that the assertion failed in.
     *  @param[in] fmt          A printf() style format string providing more information for the
     *                          failed assertion.  This may be `nullptr` if no additional information
     *                          is necessary or provided.
     *  @param[in] ...          Additional arguments required to fulfill the format string's needs.
     *                          Note that this is expected to be a single va_list object containing
     *                          the arguments.  For this reason, the @ref reportFailedAssertion()
     *                          variant is expected to be the one that is called instead of the 'V'
     *                          one being called directly.
     *  @returns `true` if a software breakpoint should be triggered.
     *  @returns `false` if the assertion should attempt to be ignored.
     *
     *  @remarks This handles displaying an assertion failure message to the user.  The failure
     *           message will be unconditionally written to the attached console and the debugger
     *           output window (if running on Windows under a debugger).  If no debugger is
     *           attached to the process, a simple message box will be shown to the user indicating
     *           that an assertion failure occurred and allowing them to choose how to proceed.
     *           If a debugger is attached, the default behavior is to break into the debugger
     *           unconditionally.
     *
     *  @note If running under the MSVC debugger, double clicking on the assertion failure text
     *        in the MSVC output window will jump to the source file and line containing the
     *        failed assertion.
     */
    bool(CARB_ABI* reportFailedAssertionV)(
        const char* condition, const char* file, const char* func, int32_t line, const char* fmt, ...);

    /** Reports the failure of an assertion condition to all applicable destinations.
     *
     *  @param[in] condition    The text describing the failed assertion condition.  This may not be
     *                          `nullptr`.
     *  @param[in] file         The name of the file that the assertion is in.  This may not be
     *                          `nullptr`.
     *  @param[in] func         The name of the function that the assertion is in.  This may not be
     *                          `nullptr`.
     *  @param[in] line         The line number that the assertion failed in.
     *  @param[in] fmt          A printf() style format string providing more information for the
     *                          failed assertion.  This may be `nullptr` if no additional information
     *                          is necessary or provided.
     *  @param[in] ...          Additional arguments required to fulfill the format string's needs.
     *  @returns `true` if a software breakpoint should be triggered.
     *  @returns `false` if the assertion should attempt to be ignored.
     *
     *  @remarks This handles displaying an assertion failure message to the user.  The failure
     *           message will be unconditionally written to the attached console and the debugger
     *           output window (if running on Windows under a debugger).  If no debugger is
     *           attached to the process, a simple message box will be shown to the user indicating
     *           that an assertion failure occurred and allowing them to choose how to proceed.
     *           If a debugger is attached, the default behavior is to break into the debugger
     *           unconditionally.
     *
     *  @note If running under the MSVC debugger, double clicking on the assertion failure text
     *        in the MSVC output window will jump to the source file and line containing the
     *        failed assertion.
     *
     *  @note This variant of the function is present to allow the @p fmt parameter to default
     *        to `nullptr` so that it can be used with a version of the CARB_ASSERT() macro that
     *        doesn't pass any message.
     */
    bool reportFailedAssertion(
        const char* condition, const char* file, const char* func, int32_t line, const char* fmt = nullptr, ...)
    {
        va_list args;
        bool result;

        va_start(args, fmt);
        result = reportFailedAssertionV(condition, file, func, line, fmt, &args);
        va_end(args);
        return result;
    }
};

} // namespace assert
} // namespace carb

/** Defines the global variable that holds the pointer to the IAssert implementation.  This will
 *  be `nullptr` if the IAssert interface is unavailable, disabled, or has not been initialized
 *  yet.  This variable is specific to each module.
 */
CARB_WEAKLINK CARB_HIDDEN carb::assert::IAssert* g_carbAssert;
