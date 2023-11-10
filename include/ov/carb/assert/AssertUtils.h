// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Helper utility functions to modify assertion failure behavior.
 */
#pragma once

#include "../Framework.h"


/** Placeholder macro for any work that needs to be done at the global level for any IAssert
 *  related functionality.  Do not call this directly.
 */
#define CARB_ASSERT_GLOBALS()


/** Namespace for all low level Carbonite functionality. */
namespace carb
{
/** Namespace for all assertion checking helpers and interfaces. */
namespace assert
{

/** Registers the IAssert implementation with the calling module.
 *
 *  @returns No return value.
 *
 *  @remarks This will be called once during framework startup to attempt to register the
 *           IAssert interface implementation with the calling module.  Once the interface
 *           is acquired, it will be valid through @ref g_carbAssert until the framework
 *           is shutdown.
 */
inline void registerAssertForClient() noexcept
{
    g_carbAssert = getFramework()->tryAcquireInterface<IAssert>();
}

/** Removes the global instance of the IAssert interface.
 *
 *  @returns No return value.
 *
 *  @remarks This will be called during framework shutdown or module unload to clear out the
 *           IAssert interface pointer for the module.  Once cleared out, all functionality
 *           that makes use of it will be effectively disabled for that module.
 */
inline void deregisterAssertForClient() noexcept
{
    g_carbAssert = nullptr;
}

/** Disables the assertion failure dialog for the process.
 *
 *  @param[in] disable  `true` to disable the assertion dialog from appearing. `false` to enable
 *                      the assertion dialog.
 *
 *  @returns The previous value of the `disableDialog` flag, in other words: `true` if the dialog
 *           was previously disabled; `false` if the dialog was previously enabled.
 *
 *  @remarks By default this dialog only shows on Windows when not attached to a debugger.
 *           Disabling the dialog is useful for test apps or unit tests that want to run in a
 *           'headless' mode but that may still trigger assertions.  In this case, at least if
 *           the assertion fails and the process possibly crashes, it won't be stuck waiting
 *           on user input.
 */
inline bool disableDialog(bool disable)
{
    if (g_carbAssert)
    {
        return !!(g_carbAssert->setAssertionFlags(disable ? fAssertSkipDialog : 0, disable ? 0 : fAssertSkipDialog) &
                  fAssertSkipDialog);
    }
    return false;
}

/** Sets whether the software breakpoint for a failed assertion should be triggered.
 *
 *  @param[in] enabled  Set to `true` to trigger the software breakpoint when an assertion fails.
 *                      Set to `false` to skip over the software breakpoint and just continue
 *                      execution when an assertion fails.  The default behavior is to always
 *                      trigger the software breakpoint unless it is overridden by user input
 *                      on the assertion dialog (if enabled).
 *  @returns The previous value of the `useBreakpoint` flag, in other words: `true` if breakpoints
 *           were previously enabled; `false` if breakpoints were previously disabled.
 *
 *  @remarks This sets whether the software breakpoint for failed assertions should be triggered.
 *           This can be used to allow failed assertions to just continue execution instead of
 *           hitting the software breakpoint.  The assertion failure message is always still
 *           written to the log destinations.  This is useful for checking whether certain 'safe'
 *           assertions failed during execution - allow them to continue execution as normal,
 *           then periodically check the assertion failure count with getFailureCount().
 *           Note that this still won't prevent a crash after a 'fatal' assertion fails.
 */
inline bool useBreakpoint(bool enabled)
{
    if (g_carbAssert)
    {
        return !(g_carbAssert->setAssertionFlags(enabled ? 0 : fAssertSkipBreakpoint, enabled ? fAssertSkipBreakpoint : 0) &
                 fAssertSkipBreakpoint);
    }
    return true;
}

/** Sets whether a message should be printed out to the console on a failed assertion.
 *
 *  @param[in] enabled  Set to `true` to cause an assertion message to be printed to the console when
 *                      an assertion fails. Set to `false` to prevent the assertion message from showing.
 *                      The assertion message is shown by default.
 *  @return The previous value of the `showToConsole` flag, in other words: `true` if show-to-console was
 *          previously enabled; `false` if show-to-console was previously disabled.
 */
inline bool showToConsole(bool enabled)
{
    if (g_carbAssert)
    {
        return !(g_carbAssert->setAssertionFlags(enabled ? 0 : fAssertNoConsole, enabled ? fAssertNoConsole : 0) &
                 fAssertNoConsole);
    }
    return true;
}

/** Retrieves the current assertion failure count for the calling process.
 *
 *  @returns The total number of assertions that have failed in the calling process.  For most
 *           normally functioning apps, this should always be zero.  This can be used in unit
 *           tests to determine whether some 'safe' assertions failed without requiring manual
 *           verification or user input.  When used in unit tests, this should be combined with
 *           carbAssertUseBreakpoint() to ensure the normal execution isn't stopped if an
 *           assertion fails.
 */
inline uint64_t getFailureCount()
{
    return g_carbAssert ? g_carbAssert->getAssertionFailureCount() : 0;
}


} // namespace assert
} // namespace carb
