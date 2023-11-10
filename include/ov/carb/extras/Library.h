// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Provides helper functions to handle library loading and management.
 */
#pragma once

#include "../Defines.h"
#include "Path.h"
#include "StringSafe.h"
#include "../../omni/extras/ScratchBuffer.h"

#if CARB_POSIX
#    if CARB_PLATFORM_LINUX
#        include <link.h>
#    elif CARB_PLATFORM_MACOS
#        include <mach-o/dyld.h>
#    endif
#    include <dlfcn.h>
#elif CARB_PLATFORM_WINDOWS
#    include "../CarbWindows.h"
#    include "Errors.h"
#    include "WindowsPath.h"
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

/** Namespace for all low level Carbonite functionality. */
namespace carb
{
/** Common namespace for extra helper functions and classes. */
namespace extras
{

/** Handle to a loaded library. */
#if CARB_POSIX || defined(DOXYGEN_BUILD)
//! A type representing a library handle.
using LibraryHandle = void*;
#elif CARB_PLATFORM_WINDOWS
using LibraryHandle = HMODULE;
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

/** A value representing an invalid library handle. */
constexpr LibraryHandle kInvalidLibraryHandle = {};

/** Base type for the flags to control how libraries are loaded. */
using LibraryFlags = uint32_t;

/** Flag to indicate that only the module's base name was given and that the full name should
 *  be constructed using createLibraryNameForModule() before attempting to load the library.
 *  When this flag is used, it is assumed that neither the library's prefix (if any) nor
 *  file extension are present in the given filename.  Path components leading up to the
 *  module name may be included as needed.  This flag is ignored if the filename is either
 *  `nullptr` or an empty string (ie: "").
 */
constexpr LibraryFlags fLibFlagMakeFullLibName = 0x00000001;

/** Flag to indicate that the library should be fully loaded and linked immediately.  This
 *  flag has no effect on Windows since it doesn't have the equivalent of Linux's lazy linker.
 *  This is equivalent to passing the `RTLD_NOW` flag to [dlopen](https://linux.die.net/man/3/dlopen) and will override
 *  any default lazy linking behavior.
 */
constexpr LibraryFlags fLibFlagNow = 0x00000002;

/** Flag to indicate that the symbols in the library being loaded should be linked to first
 *  and take precedence over global scope symbols of the same name from other libraries.  This
 *  is only available on Linux and is ignored on other platforms.  This is equivalent to passing
 *  the `RTLD_DEEPBIND` flag to [dlopen](https://linux.die.net/man/3/dlopen) on Linux.
 */
constexpr LibraryFlags fLibFlagDeepBind = 0x00000004;


/** The default library file extension for the current platform.
 *  This string will always begin with a dot ('.').
 *  The extension will always be lower case.
 *  This version is provided as a macro so it can be added to string literals.
 */
#if CARB_PLATFORM_WINDOWS
#    define CARB_LIBRARY_EXTENSION ".dll"
#elif CARB_PLATFORM_LINUX
#    define CARB_LIBRARY_EXTENSION ".so"
#elif CARB_PLATFORM_MACOS
#    define CARB_LIBRARY_EXTENSION ".dylib"
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

/** Retrieves the default library file extension for the current platform.
 *
 *  @returns The default library file extension for the platform.  This string will always
 *           begin with a dot ('.').  The extension will always be lower case.
 */
constexpr const char* getDefaultLibraryExtension()
{
    return CARB_LIBRARY_EXTENSION;
}


/** Retrieves the default library file prefix for the current platform.
 *  The prefix will always be lower case.
 *  This version is provided as a macro so it can be added to string literals.
 */
#if CARB_PLATFORM_WINDOWS
#    define CARB_LIBRARY_PREFIX ""
#elif CARB_PLATFORM_LINUX || CARB_PLATFORM_MACOS
#    define CARB_LIBRARY_PREFIX "lib"
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

/** Retrieves the default library file prefix for the current platform.
 *
 *  @returns The default library file prefix for the platform.  The prefix will always be
 *           lower case.
 */
constexpr const char* getDefaultLibraryPrefix()
{
    return CARB_LIBRARY_PREFIX;
}

/** A macro to build a library file's name as a string literal.
 *  @param name A string literal name for the library.
 *
 *  @remarks This will build the full platform-specific library file name;
 *           for example "carb" will be built into "carb.dll" on Windows,
 *           "libcarb.so" on Linux, etc.
 */
#define CARB_LIBRARY_GET_LITERAL_NAME(name) CARB_LIBRARY_PREFIX name CARB_LIBRARY_EXTENSION

/** Creates a full library name from a module's base name.
 *
 *  @param[in] baseName     The base name of the module to create the library name for.  This
 *                          base name should not include the file extension (ie: ".dll" or ".so")
 *                          and should not include any prefix (ie: "lib").  Path components may be
 *                          included and will also be present in the created filename.  This must
 *                          not be `nullptr`.
 *  @returns The name to use to load the named module on the current platform.  This will neither
 *           check for the existence of the named module nor will it verify that a prefix or
 *           file extension already exists in the base name.
 */
inline std::string createLibraryNameForModule(const char* baseName)
{
    const char* prefix;
    const char* ext;
    char* buffer;
    size_t len = 0;
    size_t pathLen = 0;
    const char* sep[2] = {};
    const char* name = baseName;

    if (baseName == nullptr || baseName[0] == 0)
        return {};

    sep[0] = strrchr(baseName, '/');
#if CARB_PLATFORM_WINDOWS
    // also handle mixed path separators on Windows.
    sep[1] = strrchr(baseName, '\\');

    if (sep[1] > sep[0])
        sep[0] = sep[1];
#endif

    if (sep[0] != nullptr)
    {
        pathLen = (sep[0] - baseName) + 1;
        name = sep[0] + 1;
        len += pathLen;
    }

    prefix = getDefaultLibraryPrefix();
    ext = getDefaultLibraryExtension();
    len += strlen(prefix) + strlen(ext);

    len += strlen(name) + 1;
    buffer = CARB_STACK_ALLOC(char, len);
    carb::extras::formatString(buffer, len, "%.*s%s%s%s", (int)pathLen, baseName, prefix, name, ext);
    return buffer;
}

/** Attempts to retrieve the address of a symbol from a loaded module.
 *
 *  @param[in] libHandle    The library to retrieve the symbol from.  This should be a handle
 *                          previously returned by loadLibrary().
 *  @param[in] name         The name of the symbol to retrieve the address of.  This must exactly
 *                          match the module's exported name for the symbol including case.
 *  @returns The address of the loaded symbol if successfully found in the requested module.
 *  @returns `nullptr` if the symbol was not found in the requested module.
 *
 *  @note The @p libHandle parameter may also be `nullptr` to achieve some special behavior when
 *        searching for symbols.  However, note that this special behavior differs between
 *        Windows and Linux.  On Windows, passing `nullptr` for the library handle will search
 *        for the symbol in the process's main module only.  Since most main executable modules
 *        don't export anything, this is likely to return `nullptr`.  On Linux however, passing
 *        `nullptr` for the library handle will search the process's entire symbol space starting
 *        from the main executable module.
 *
 *  @note Calling this on Linux with the handle of a library that has been unloaded from memory
 *        is considered undefined behavior.  It is the caller's responsibility to ensure the
 *        library handle is valid before attempting to call this.  On Windows, passing in an
 *        invalid library handle will technically fail gracefully, however it is still considered
 *        undefined behavior since another library could be reloaded into the same space in
 *        memory at a later time.
 */
template <typename T>
T getLibrarySymbol(LibraryHandle libHandle, const char* name)
{
#if CARB_PLATFORM_WINDOWS
    return reinterpret_cast<T>(::GetProcAddress(libHandle, name));
#elif CARB_POSIX
    if (libHandle == nullptr || name == nullptr)
        return nullptr;

    return reinterpret_cast<T>(::dlsym(libHandle, name));
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

std::string getLibraryFilenameByHandle(LibraryHandle handle); // forward declare

#ifndef DOXYGEN_BUILD
namespace detail
{
struct FreeString
{
    void operator()(char* p) noexcept
    {
        free(p);
    }
};

using UniqueCharPtr = std::unique_ptr<char, FreeString>;

#    if CARB_POSIX
struct FreePosixLib
{
    void operator()(void* p) noexcept
    {
        dlclose(p);
    }
};

using UniquePosixLib = std::unique_ptr<void, FreePosixLib>;
#    endif
} // namespace detail
#endif

// clang-format off
/** Attempts to load a named library into the calling process.
 *
 * This attempts to dynamically load a named library into the calling process. If the library is already loaded, the
 * reference count of the library is increased and a handle to it is returned. If the library was not already loaded,
 * it is dynamically loaded and a handle to it is returned; its reference count will be one. Each call to this function
 * should be balanced by a call to \ref unloadLibrary() when the library is no longer needed.
 *
 * Libraries that were loaded as part of the process's dependencies list will be 'pinned' and their reference count will
 * not be changed by attempting to load them. It is still safe to \ref unloadLibrary() on handles for those libraries.
 * 
 * \par Windows
 * This function calls [LoadLibraryEx](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw)
 * with `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` and `LOAD_LIBRARY_SEARCH_DEFAULT_DIRS` to load libraries. If this fails,
 * the default search path is instead used. If `ERROR_FILENAME_EXCED_RANGE` is returned, the `LoadLibraryEx` call is
 * repeated with the filepath preceded with a prefix to disable string parsing. If the module or a dependent module is
 * not found, `GetLastError()` typically reports `ERROR_MOD_NOT_FOUND`. In order to determine which DLL library is
 * missing there are a few options:
 *  * Enable [Show Loader Snaps](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/show-loader-snaps)
 *    with the *gflags* tool, which is available with
 *    [Debugging Tools for Windows](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/debugger-download-tools).
 *    Running the process in a debugger or with access to debug output will then show log messages from the Windows
 *    loader.
 *  * The same debug output can be enabled by setting the 32-bit value at `ntdll.dll!LdrpDebugFlags` to `0x1`. This
 *    value must be accessed in a debugger with symbols for *ntdll.dll*.
 *  * Alternately using a modern [Dependencies tool](https://github.com/lucasg/Dependencies) will reveal missing
 *    dependencies for a library without a debugger, although it may not show the actual error that led to `loadLibrary`
 *    failing.
 *
 * \par Linux
 * This function calls [dlopen](https://linux.die.net/man/3/dlopen) with the `RTLD_LAZY` flag (or `RTLD_NOW` if 
 * \ref fLibFlagNow is specified). If \ref fLibFlagDeepBind is specified, `RTLD_DEEPBIND` is also used. It is possible
 * for the underlying `dlopen()` call to succeed without a dependent library able to load. In this case, the dynamic
 * loader destroys the link map, so symbol lookup on the module will fail. This condition is detected and treated as an
 * error condition. To have dynamic loader debug output displayed, use the [LD_DEBUG](https://linux.die.net/man/8/ld.so)
 * environment variable set to a value such as `files`, `libs` or `all`. Typically the output is printed to *stdout*,
 * but the `LD_DEBUG_OUTPUT` can be set to the name of a file to instead log to the file. The
 * [ldd](https://linux.die.net/man/1/ldd) command-line tool can be used to print the dependent libraries required by
 * an executable or library.
 *
 * \par MacOSX
 * This function calls `dlopen` with the `RTLD_LAZY` flag (or `RTLD_NOW` if \ref fLibFlagNow is specified). To have the
 * dynamic loader debug output displayed, use the [DYLD_PRINT_LIBRARIES](https://developer.apple.com/library/archive/documentation/DeveloperTools/Conceptual/DynamicLibraries/100-Articles/LoggingDynamicLoaderEvents.html)
 * environment variable. The `otool` command-line tool with the `-L` option will print out the dependent libraries
 * required by an executable or library.
 * 
 * @note A library may fail to load because of a dependent library, but the error message may seem like the *requested*
 *   library is causing the failure.
 * 
 *  @param[in] libraryName  The name of the library to attempt to load.  This may be either a
 *                          relative or absolute path name.  This should be a UTF-8 encoded
 *                          path.  If the @ref fLibFlagMakeFullLibName flag is used, the library
 *                          name may omit any platform specific prefix or file extension.  The
 *                          appropriate platform specific name will be generated internally
 *                          using createLibraryNameForModule(). If `nullptr` is provided, the executable module is
 *                          retrieved instead. On Windows this handle need not be closed with \ref unloadLibrary() as
 *                          it does not increment the reference count, but on MacOSX and Linux it increments the
 *                          reference count and should therefore be passed to \ref unloadLibrary().
 *  @param[in] flags        Flags to control the behavior of the operation.  This defaults to 0.
 *  @returns A handle to the library if it was already loaded in the process.  This handle should
 *           be cleaned up by unloadLibrary() when it is no longer necessary. `nullptr` is returned if the library
 *           could not be found, or loading fails for some reason (use \ref getLastLoadLibraryError()).
 */
// clang-format on
inline LibraryHandle loadLibrary(const char* libraryName, LibraryFlags flags = 0)
{
    std::string fullLibName;


    // asked to construct a full library name => create the name and adjust the path as needed.
    if (libraryName != nullptr && libraryName[0] != '\0' && (flags & fLibFlagMakeFullLibName) != 0)
    {
        fullLibName = createLibraryNameForModule(libraryName);
        libraryName = fullLibName.c_str();
    }

#if CARB_PLATFORM_WINDOWS
    // retrieve the main executable module's handle.
    if (libraryName == nullptr)
        return ::GetModuleHandleW(nullptr);

    // retrieve the handle of a specific module.
    std::wstring widecharName = carb::extras::convertCarboniteToWindowsPath(libraryName);
    LibraryHandle handle =
        ::LoadLibraryExW(widecharName.c_str(), nullptr,
                         CARBWIN_LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | CARBWIN_LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

    // Although convertCarboniteToWindowsPath will ensure that a path over MAX_PATH has the long-path prefix,
    // LoadLibraryExW complains about strings slightly smaller than that. If we get that specific error then try
    // again with the long-path prefix.
    if (!handle && ::GetLastError() == CARBWIN_ERROR_FILENAME_EXCED_RANGE)
    {
        handle = ::LoadLibraryExW((L"\\\\?\\" + widecharName).c_str(), nullptr,
                                  CARBWIN_LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | CARBWIN_LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    }

    // failed to load the loading the module from the 'default search dirs' => attempt to load
    //   it with the default system search path.  Oddly enough, this is different from the search
    //   path provided by the flags used above - it includes the current working directory and
    //   the paths in $PATH.  Another possible reason for the above failing is that the library
    //   name was a relative path.  The CARBWIN_LOAD_LIBRARY_SEARCH_DEFAULT_DIRS flag used above
    //   requires that an absolute path be used.  To keep the behavior of this function on par
    //   with Linux's behavior, we'll attempt another load from the default paths instead.
    if (handle == nullptr)
    {
        handle = ::LoadLibraryExW(widecharName.c_str(), nullptr, 0);

        // As above, try again with the long-path prefix if we get a specific error response from LoadLibrary.
        if (!handle && ::GetLastError() == CARBWIN_ERROR_FILENAME_EXCED_RANGE)
        {
            handle = ::LoadLibraryExW((L"\\\\?\\" + widecharName).c_str(), nullptr, 0);
        }
    }
#elif CARB_POSIX
    int openFlags = RTLD_LAZY;

    if ((flags & fLibFlagNow) != 0)
        openFlags |= RTLD_NOW;

#    if CARB_PLATFORM_LINUX
    if ((flags & fLibFlagDeepBind) != 0)
        openFlags |= RTLD_DEEPBIND;
#    endif

    LibraryHandle handle = dlopen(libraryName, openFlags);

    // failed to get a module handle or load the module => check if this was a request to load the
    //   handle for the main executable module by its path name.
    if (handle == nullptr && libraryName != nullptr && libraryName[0] != 0)
    {
        detail::UniqueCharPtr path(realpath(libraryName, nullptr));
        if (path == nullptr)
        {
            // probably trying to load a library that doesn't exist
            CARB_LOG_INFO("realpath(%s) failed (errno = %d)", libraryName, errno);
            return nullptr;
        }

        std::string raw = getLibraryFilenameByHandle(nullptr);
        CARB_FATAL_UNLESS(!raw.empty(), "getLibraryFilenameByHandle(nullptr) failed");

        // use realpath() to ensure the paths can be compared
        detail::UniqueCharPtr path2(realpath(raw.c_str(), nullptr));
        CARB_FATAL_UNLESS(path2 != nullptr, "realpath(%s) failed (errno = %d)", raw.c_str(), errno);

        // the two names match => retrieve the main executable module's handle for return.
        if (strcmp(path.get(), path2.get()) == 0)
        {
            return dlopen(nullptr, openFlags);
        }
    }

#    if CARB_PLATFORM_LINUX
    if (handle != nullptr)
    {
        // Linux's dlopen() has a strange issue where it's possible to have the call succeed
        // even though one or more of the library's dependencies fail to load.  The dlopen()
        // call succeeds because there are still references on the handle despite the module's
        // link map having been destroyed (visible from the 'LD_DEBUG=all' output).
        // Unfortunately, if the link map is destroyed, any attempt to retrieve a symbol from
        // the library with dlsym() will fail.  This causes some very confusing and misleading
        // error messages or crashes (depending on usage) instead of just having the module load
        // fail.
        void* linkMap = nullptr;
        const char* errorMsg = dlerror();

        if (dlinfo(handle, RTLD_DI_LINKMAP, &linkMap) == -1 || linkMap == nullptr)
        {
            CARB_LOG_WARN("Library '%s' loaded with errors '%s' and no link map.  The likely cause of this is that ",
                          libraryName, errorMsg);
            CARB_LOG_WARN("a dependent library or symbol in the dependency chain is missing.  Use the environment ");
            CARB_LOG_WARN("variable 'LD_DEBUG=all' to diagnose.");

            // close the bad library handle.  Note that this may not actually unload the bad
            // library since it may still have multiple references on it (part of the failure
            // reason).  However, we can only safely clean up one reference here.
            dlclose(handle);
            return nullptr;
        }
    }
#    endif
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
    return handle;
}

/** Retrieves a string explaining the most recent library load failure cause.
 *
 *  @returns A string containing a message explaining the most recent failure from loadLibrary().
 */
inline std::string getLastLoadLibraryError()
{
#if CARB_PLATFORM_WINDOWS
    return carb::extras::getLastWinApiErrorMessage();
#else
    return dlerror();
#endif
}

/** Unloads a loaded library.
 *
 *  @param[in] libraryHandle    The handle to the library to unload.  This should have been
 *                              returned by a previous call to loadLibrary().  This may not be
 *                              `nullptr`.
 *  @returns No return value.
 *
 *  @remarks This unloads one reference to a library that was loaded by loadLibrary().  Note that
 *           this may not actually unload the library from the process if other references to it
 *           still exist.  When the last reference to an unpinned library is removed, that library
 *           will be removed from the process's memory space.
 *
 *  @note Once a library has been unloaded from memory, the handle to it cannot be safely used
 *        any more.  Attempting to use it may result in undefined behavior.  Effectively, as
 *        soon as a handle is passed in here, it should be discarded.  Even though the module may
 *        remain in memory, the handle should be treated as though it has been freed upon return
 *        from here.
 */
inline void unloadLibrary(LibraryHandle libraryHandle)
{
    if (libraryHandle)
    {
#if CARB_PLATFORM_WINDOWS
        if (!::FreeLibrary(libraryHandle))
        {
            DWORD err = ::GetLastError();
            CARB_LOG_WARN("FreeLibrary for handle %p failed with error: %d/%s", libraryHandle, err,
                          convertWinApiErrorCodeToMessage(err).c_str());
        }
#elif CARB_POSIX
        if (::dlclose(libraryHandle) != 0)
        {
            CARB_LOG_WARN("Closing library handle %p failed with error: %s", libraryHandle, dlerror());
        }
#else
        CARB_UNSUPPORTED_PLATFORM();
#endif
    }
}

/** Attempts to retrieve a library's handle by its filename.
 *
 * @warning This function does not increment a library's reference count, so it is possible (though unlikely) that
 * another thread could be unloading the library and the handle returned from this function is invalid. Only use this
 * function for debugging, or when you know that the returned handle will still be valid.
 *
 * @thread_safety This function is safe to call simultaneously from multiple threads, but only as long as another thread
 * is not attempting to unload the library found by this function.
 *
 *  @param[in] libraryName  The name of the library to retrieve the handle for.  This may either
 *                          be the full path for the module or just its base filename.  This may
 *                          be `nullptr` to retrieve the handle for the main executable module
 *                          for the calling process.  If the @ref fLibFlagMakeFullLibName flag is
 *                          used, the library name may omit any platform specific prefix or file
 *                          extension.  The appropriate platform specific name will be generated
 *                          internally using createLibraryNameForModule().
 *  @param[in] flags        Flags to control the behavior of the operation.  This defaults to 0.
 *  @returns The handle to the requested library if it is already loaded in the process.  Returns
 *           `nullptr` if the library is not loaded.  This will not load the library if it is not
 *           already present in the process.  This can be used to test if a library is already
 *           loaded.
 */
inline LibraryHandle getLibraryHandleByFilename(const char* libraryName, LibraryFlags flags = 0)
{
    std::string fullLibName;

    if (libraryName != nullptr && libraryName[0] != '\0' && (flags & fLibFlagMakeFullLibName) != 0)
    {
        fullLibName = createLibraryNameForModule(libraryName);
        libraryName = fullLibName.c_str();
    }

#if CARB_PLATFORM_WINDOWS
    if (libraryName == nullptr)
        return ::GetModuleHandleW(nullptr);

    std::wstring wideCharName = carb::extras::convertCarboniteToWindowsPath(libraryName);
    return GetModuleHandleW(wideCharName.c_str());
#else
    if (libraryName != nullptr && libraryName[0] == 0)
        return nullptr;

    // A successful dlopen() with RTLD_NOLOAD increments the reference count, so we dlclose() it to make sure that
    // the reference count stays the same. This function is inherently racy as another thread could be unloading the
    // library while we're trying to load it.
    // Note that we can't use UniquePosixLib here because it causes clang to think that we're returning a freed
    // pointer.
    void* handle = ::dlopen(libraryName, RTLD_LAZY | RTLD_NOLOAD);
    if (handle != nullptr) // dlclose(nullptr) crashes
    {
        dlclose(handle);
    }
    return handle;
#endif
}

/** Retrieves the path for a loaded library from its handle.
 *
 *  @param[in] handle   The handle to the library to retrieve the name for.  This may be `nullptr`
 *                      to retrieve the filename of the main executable module for the process.
 *  @returns A string containing the full path to the requested library.
 *  @returns An empty string if the module handle was invalid or the library is no longer loaded
 *           in the process.
 */
inline std::string getLibraryFilenameByHandle(LibraryHandle handle)
{
#if CARB_PLATFORM_WINDOWS
    omni::extras::ScratchBuffer<wchar_t, CARBWIN_MAX_PATH> path;
    // There's no way to verify the correct length, so we'll just double the buffer
    // size every attempt until it fits.
    for (;;)
    {
        DWORD res = GetModuleFileNameW(handle, path.data(), DWORD(path.size()));
        if (res == 0)
        {
            // CARB_LOG_ERROR("GetModuleFileNameW(%p) failed (%d)", handle, GetLastError());
            return "";
        }
        if (res < path.size())
        {
            break;
        }

        bool suc = path.resize(path.size() * 2);
        OMNI_FATAL_UNLESS(suc, "failed to allocate %zu bytes", path.size() * 2);
    }

    return carb::extras::convertWindowsToCarbonitePath(path.data());
#elif CARB_PLATFORM_LINUX
    struct link_map* map;

    // requested the filename for the main executable module => dlinfo() will succeed on this case
    //   but will give an empty string for the path.  To work around this, we'll simply read the
    //   path to the process's executable symlink.
    if (handle == nullptr)
    {
        detail::UniqueCharPtr path(realpath("/proc/self/exe", nullptr));
        CARB_FATAL_UNLESS(path != nullptr, "calling realpath(\"/proc/self/exe\") failed (%d)", errno);
        return path.get();
    }

    int res = dlinfo(handle, RTLD_DI_LINKMAP, &map);
    if (res != 0)
    {
        // CARB_LOG_ERROR("failed to retrieve the link map from library handle %p (%d)", handle, errno);
        return "";
    }

    // for some reason, the link map doesn't provide a filename for the main executable module.
    // This simply gets returned as an empty string.  If we get that case, we'll try getting
    // the main module's filename instead.
    if (!map->l_name || map->l_name[0] == '\0')
    {
        // first make sure the handle passed in is for our main executable module.
        auto binaryHandle = loadLibrary(nullptr);
        if (binaryHandle)
        {
            unloadLibrary(binaryHandle);
        }

        if (binaryHandle != handle)
        {
            // CARB_LOG_ERROR("library had no filename in the link map but was not the main module");
            return {};
        }

        // recursively call to get the main module's name
        return getLibraryFilenameByHandle(nullptr);
    }

    return map->l_name;
#elif CARB_PLATFORM_MACOS
    // dlopen(nullptr) gives a different (non-null) result than dlopen(path_to_exe), so
    // we need to test against it as well.
    if (handle == nullptr || detail::UniquePosixLib{ dlopen(nullptr, RTLD_LAZY | RTLD_NOLOAD) }.get() == handle)
    {
        omni::extras::ScratchBuffer<char, 4096> buffer;
        uint32_t len = buffer.size();
        int res = _NSGetExecutablePath(buffer.data(), &len);
        if (res != 0)
        {
            bool succ = buffer.resize(len);
            CARB_FATAL_UNLESS(succ, "failed to allocate %" PRIu32 " bytes", len);

            res = _NSGetExecutablePath(buffer.data(), &len);
            CARB_FATAL_UNLESS(res != 0, "_NSGetExecutablePath() failed");
        }

        detail::UniqueCharPtr path(realpath(buffer.data(), nullptr));
        CARB_FATAL_UNLESS(path != nullptr, "realpath(%s) failed (errno = %d)", buffer.data(), errno);
        return path.get();
    }

    // Look through all the currently loaded libraries for the our handle.
    for (uint32_t i = 0;; i++)
    {
        const char* name = _dyld_get_image_name(i);
        if (name == nullptr)
        {
            break;
        }

        // RTLD_NOLOAD is passed to avoid unnecessarily loading a library if it happened to be unloaded concurrently
        // with this call. UniquePosixLib is used to release the reference that dlopen adds if successful.
        if (detail::UniquePosixLib{ dlopen(name, RTLD_LAZY | RTLD_NOLOAD) }.get() == handle)
        {
            return name;
        }
    }

    return {};
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/** Retrieves the path for a loaded library from an address or symbol within it.
 *
 *  @param[in] symbolAddress    The address of the symbol to find the library file name for.
 *                              This may be a symbol returned from a previous call to
 *                              getLibrarySymbol() or another known symbol in the library.
 *                              This does not strictly need to be an exported symbol from the
 *                              library, just an address within that library's memory space.
 *  @returns A string containing the name and path of the library that the symbol belongs to if
 *           found.  This path components in this string will always be delimited by '/' and
 *           the string will always be UTF-8 encoded.
 *  @returns An empty string if the requested address is not a part of a loaded library.
 */
inline std::string getLibraryFilename(const void* symbolAddress)
{
#if CARB_PLATFORM_WINDOWS
    HMODULE hm = NULL;

    if (0 == GetModuleHandleExW(
                 CARBWIN_GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | CARBWIN_GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                 (LPCWSTR)symbolAddress, &hm))
    {
        return {};
    }

    return getLibraryFilenameByHandle(hm);
#elif CARB_PLATFORM_LINUX
    Dl_info info;
    struct link_map* lm;
    if (dladdr1(symbolAddress, &info, reinterpret_cast<void**>(&lm), RTLD_DL_LINKMAP))
    {
        if (info.dli_fname != nullptr && info.dli_fname[0] == '/')
            return info.dli_fname;

        else if (lm->l_name != nullptr && lm->l_name[0] == '/')
            return lm->l_name;

        else
        {
            // the main executable doesn't have a path set for it => retrieve it directly.  This
            //   seems to be the expected behavior for the link map for the main module.
            if (lm->l_name == nullptr || lm->l_name[0] == 0)
                return getLibraryFilenameByHandle(nullptr);

            // no info to retrieve the name from => fail.
            if (info.dli_fname == nullptr || info.dli_fname[0] == 0)
                return {};

            // if this process was launched using a relative path, the returned name from dladdr()
            // will also be a relative path => convert it to a fully qualified path before return.
            //   Note that for this to work properly, the working directory should not have
            //   changed since the process launched.  This is not necessarily a valid assumption,
            //   but since we have no control over that behavior here, it is the best we can do.
            //   Note that we took all possible efforts above to minimize the cases where this
            //   step will be needed however.
            detail::UniqueCharPtr path(realpath(info.dli_fname, nullptr));
            if (path == nullptr)
                return {};

            return path.get();
        }
    }

    return {};
#elif CARB_PLATFORM_MACOS
    Dl_info info;
    if (dladdr(symbolAddress, &info))
    {
        if (info.dli_fname == nullptr)
        {
            return getLibraryFilenameByHandle(nullptr);
        }
        else if (info.dli_fname[0] == '/')
        {
            // path is already absolute, just return it
            return info.dli_fname;
        }
        else
        {
            detail::UniqueCharPtr path(realpath(info.dli_fname, nullptr));
            CARB_FATAL_UNLESS(path != nullptr, "realpath(%s) failed (errno = %d)", info.dli_fname, errno);
            return path.get();
        }
    }

    return {};
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/**
 * Retrieves the handle for a loaded library from an address or symbol within it.
 *
 * @note The library is not referenced as if @ref loadLibrary() was called for it. Do not call @ref unloadLibrary() to
 *      as this handle does not need to be released.
 *
 * @param[in] symbolAddress The address of the symbol to find the library handle for. This may be a symbol returned from
 *      a previous call to @ref getLibrarySymbol() or another known symbol in the library. This does not strictly need
 *      to be an exported symbol from the library, just an address within that library's memory space.
 * @returns A @ref LibraryHandle representing the library that contains @p symbolAddress; @ref kInvalidLibraryHandle if
 *      the library containing the symbol could not be determined.
 */
inline LibraryHandle getLibraryHandle(const void* symbolAddress)
{
#if CARB_PLATFORM_WINDOWS
    HMODULE hm = NULL;

    if (0 == GetModuleHandleExW(
                 CARBWIN_GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | CARBWIN_GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                 (LPCWSTR)symbolAddress, &hm))
    {
        return kInvalidLibraryHandle;
    }

    return hm;
#elif CARB_POSIX
    std::string module = getLibraryFilename(symbolAddress);
    if (!module.empty())
    {
        // loadLibrary increments the reference count, so decrement it immediately after.
        auto handle = loadLibrary(module.c_str());
        if (handle != kInvalidLibraryHandle)
        {
            unloadLibrary(handle);
        }
        return handle;
    }

    return kInvalidLibraryHandle;
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/** Retrieves the parent directory of a library.
 *
 *  @param[in] handle   The handle to the loaded library to retrieve the directory for.  This
 *                      must be a handle that was previously returned from loadLibrary() and has
 *                      not yet been unloaded.  This may be `nullptr` to retrieve the directory of
 *                      the process's main executable.
 *  @returns A string containing the name and path of the directory that contains the library
 *           that the symbol belongs to if found.  This path components in this string will
 *           always be delimited by '/' and the string will always be UTF-8 encoded.  The trailing
 *           path separator will always be removed.
 *  @returns An empty string if the requested address is not a part of a loaded library.
 */
inline std::string getLibraryDirectoryByHandle(LibraryHandle handle)
{
    return carb::extras::getPathParent(getLibraryFilenameByHandle(handle));
}

/** Retrieves the parent directory of the library containing a given address or symbol.
 *
 *  @param[in] symbolAddress    The address of the symbol to find the library file name for.
 *                              This may be a symbol returned from a previous call to
 *                              getLibrarySymbol() or another known symbol in the library.
 *                              This does not strictly need to be an exported symbol from the
 *                              library, just an address within that library's memory space.
 *  @returns A string containing the name and path of the directory that contains the library
 *           that the symbol belongs to if found.  This path components in this string will
 *           always be delimited by '/' and the string will always be UTF-8 encoded.  The
 *           trailing path separator will always be removed.
 *  @returns An empty string if the requested address is not a part of a loaded library.
 */
inline std::string getLibraryDirectory(void* symbolAddress)
{
    return carb::extras::getPathParent(getLibraryFilename(symbolAddress));
}

} // namespace extras
} // namespace carb
