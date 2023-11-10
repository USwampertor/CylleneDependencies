// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Provides functionality to force a symbol to be linked to a module
 *         instead of the optimizer potentially removing it out.
 */
#pragma once

#include "../core/Platform.h"

#if OMNI_PLATFORM_WINDOWS
// see below for why we unfortunately either need this or to forward declare SetLastError()
// directly here instead of using something like strerror_s() on Windows.
#    include "../../carb/CarbWindows.h"
#endif


namespace omni
{
/** common namespace for extra helper functions and classes. */
namespace extras
{

/** Helper class to force the linking of a C++ symbol.  This is done by having the symbol's
 *  address be passed to a function in another module.  Since, at link time, the linker
 *  doesn't know what the other module's function will do with the symbol, it can't discard
 *  the symbol.  This is useful for ensuring that debug-only or initializer-only symbols
 *  do not get unintentionally eliminated from the module they are present in.
 *
 *  Note that since this does not have any data members, it shouldn't occupy any space in the
 *  module's data section.  It will however generate a C++ static initializer and shutdown
 *  destructor.
 *
 *  This class should not be used directly, but instead through the @ref OMNI_FORCE_SYMBOL_LINK
 *  macro.
 */
class ForceSymbolLink
{
public:
    /** Constructor: passes an argument's value to a system library.
     *
     *  @param[in] ptr  The address to pass on to a system library call.  This prevents the
     *                  linker from being able to discard the symbol as unused or unreferenced.
     *                  This value is not accessed as a pointer in any way so any value is
     *                  acceptable.
     *  @returns No return value.
     */
    ForceSymbolLink(void* ptr)
    {
#if OMNI_PLATFORM_WINDOWS
        // On Windows, we unfortunately can't call into something like strerror_s() to
        // accomplish this task because the CRT is statically linked to the module that
        // will be using this.  That would make the function we're passing the symbol
        // to local and therefore the symbol will still be discardable.  Instead, we'll
        // pass the address to SetLastError() which will always be available from 'kernel32'.
        SetLastError(static_cast<DWORD>(reinterpret_cast<uintptr_t>(ptr)));
#else
        char* str;
        CARB_UNUSED(str);
        str = strerror(static_cast<int>(reinterpret_cast<uintptr_t>(ptr)));
#endif
    }
};

} // namespace extras
} // namespace omni


/** Helper to force a symbol to be linked.
 *
 *  @param[in] symbol_  The symbol that must be linked to the calling module.  This must be a
 *                      valid symbol name including any required namespaces given the calling
 *                      location.
 *  @param[in] tag_     A single token name to give to the symbol that forces the linking.
 *                      This is used purely for debugging purposes to give an identifiable name
 *                      to the symbol that is used to force linking of @p symbol_.  This must
 *                      be a valid C++ single token name (ie: only contains [A-Za-z0-9_] and
 *                      must not start with a number).
 *  @returns No return value.
 *
 *  @remarks This is used to ensure an unused symbol is linked into a module.  This is done
 *           by tricking the linker into thinking the symbol is not discardable because its
 *           address is being passed to a function in another module.  This is useful for
 *           example to ensure symbols that are meant purely for debugger use are not discarded
 *           from the module.  Similarly, it can be used on symbols that are only meant to
 *           anchor C++ object initializers but are unreferenced otherwise.
 */
#define OMNI_FORCE_SYMBOL_LINK(symbol_, tag_)                                                                          \
    static omni::extras::ForceSymbolLink CARB_JOIN(                                                                    \
        g_forceLink##tag_##_, CARB_JOIN(CARB_JOIN(__COUNTER__, _), __LINE__))(&(symbol_))
