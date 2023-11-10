// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Helper class to manage a unique process.
 */
#pragma once

#include "../core/Omni.h"

#include <string>

#if OMNI_PLATFORM_WINDOWS
#    include "../../carb/CarbWindows.h"
#    include "../../carb/extras/WindowsPath.h"
#else
#    include <unistd.h>
#    include <sys/types.h>
#    include <sys/stat.h>
#    include <fcntl.h>
#endif


namespace omni
{
/** common namespace for extra helper functions and classes. */
namespace extras
{

/** Helper class to manage a unique app.  A unique app is one that is intended to only
 *  run a single instance of it at any given time.  This contains helper functions to handle
 *  common tasks that are intended to be used on both the unique app's side and on the host
 *  app side.  This contains two major sets of helper functions.
 *    * One set is to manage the uniqueness of the app itself.  These can either be called
 *      entirely from within the unique app process after launch to determine if another
 *      instance of the app is already running and that the new one should exit immediately.
 *      Alternatively, the launching host app could also first check if the unique app process
 *      is already running before deciding to launch it in the first place.
 *    * The other set is to manage notifying the unique app process when it should exit
 *      naturally.  These functions setup a signal that a host app is still running that
 *      the unique app can then poll on.  The host app is responsible for setting its
 *      'running' signal early on its launch.  The unique app process will then periodically
 *      poll to see if any host apps are still running.
 *
 *  Note that a 'connection' to the unique app is not actually a connection in a communication
 *  sense.  This is more akin to a reference count on an object that the operating system will
 *  manage regardless of how the host app exits or stops running.  The 'connection' can exist
 *  for the host app before the unique app is even launched (in fact, this behavior is preferred
 *  since it simplifies polling and avoids the possibility of an unintentional early exit).
 *  The host app's 'connection' to the unique app does not require any actual communication, just
 *  that the connection object remain open until it is either explicitly closed or the host app
 *  process exits.
 */
class UniqueApp
{
public:
    /** Constructor: creates a new unique app object with default settings.
     *
     *  This creates a new object with all default settings.  Note that this will use the default
     *  guard name.  If the caller doesn't set a different guard name with @ref setGuardName(),
     *  the uniqueness of the app represented by this object may conflict with other apps that
     *  also use that name.
     */
    UniqueApp() = default;

    /** Constructor: creates a new unique app object with explicit settings.
     *
     *  @param[in] guardPath    The directory to store the various guard files in.  This may not
     *                          be `nullptr`.  This may be a relative or absolute path.  If this
     *                          is an empty string, the current directory will be used instead.
     *  @param[in] guardName    The prefix to use for the guard objects.  This may not be
     *                          `nullptr`.  If this is an empty string, the default prefix will
     *                          be used.  This should be a name that is unique to the app being
     *                          launched.  However, all instances trying to launch that single
     *                          unique app must use the same guard name.
     *
     *  This creates a new object with explicit settings for the guard path and guard names.
     *  This is a convenience shortcut for creating an object with default settings, then
     *  changing the guard path and guard name manually using @ref setGuardPath() and
     *  @ref setGuardName().
     */
    UniqueApp(const char* guardPath, const char* guardName)
    {
        setGuardPath(guardPath);
        setGuardName(guardName);
    }

    ~UniqueApp()
    {
        // Note: We are *intentionally* leaking any created guard objects here.  If either of
        //       them were to be closed on destruction of the object, undesirable effects would
        //       result:
        //          * If the launch guard was created, closing it would allow other instances
        //            of the unique app to successfully launch.
        //          * If this process 'connected' to the unique app process, closing the exit
        //            guard object would remove its reference and could allow the unique app to
        //            exit prematurely thinking all of its 'clients' had exited already.
        //
        //       An alternative to this would require forcing all callers to store the created
        //       object at a global level where it would live for the duration of the process.
        //       While this is certainly possible, enforcing that would be difficult at best.
    }

    /** Sets the path to put the guard file(s) in.
     *
     *  @param[in] path     The directory to store the various guard files in.  This may not
     *                      be `nullptr`.  This may be a relative or absolute path.  If this is
     *                      an empty string, the current directory will be used instead.
     *  @returns no return value.
     *
     *  @thread_safety This call is not thread safe.  It is the caller's responsibility to
     *                 ensure this call is protected.
     */
    void setGuardPath(const char* path)
    {
        if (path[0] == 0 || path == nullptr)
            path = ".";

        m_guardPath = path;
    }

    /** Sets the name for the guard file(s).
     *
     *  @param[in] name     The prefix to use for the guard objects.  This may not be `nullptr`.
     *                      If this is an empty string, the default prefix will be used.  This
     *                      should be a name that is unique to the app being launched.  However,
     *                      all instances trying to launch that single unique app must use the
     *                      same guard name.
     *  @returns no return value.
     *
     *  @thread_safety This call is not thread safe.  It is the caller's responsibility to
     *                 ensure this call is protected.
     */
    void setGuardName(const char* name)
    {
        if (name[0] == 0 || name == nullptr)
            name = kDefaultNamePrefix;

        m_guardName = name;
    }

    /** Creates the run guard object for the unique app.
     *
     *  @returns `true` if the unique app launch guard was newly created successfully.  Returns
     *           `false` if the launch guard could not be created or it was already created by
     *           another process.
     *
     *  This creates the run guard object that is used to determine if the unique app is
     *  already running.  This is intended to be called exactly once from the unique app's
     *  process early during its startup.  The guard object that is created will exist for the
     *  remaining lifetime of the unique app's process.  Once the process that calls this exits,
     *  the object will be cleaned up by the operating system.
     *
     *  The @ref checkLaunchGuard() function is intended to be used to determine if this
     *  object still exists.  It may be called either from within the unique app itself
     *  to determine if new launches of it should just exit, or it may called from a host app
     *  to determine whether to launch the unique app in the first place.  This function
     *  however is intended to be called from the unique app itself and acts as both the
     *  guard object creation and a check for its existence.
     *
     *  @thread_safety This call is not thread safe.  It is the caller's responsibility to
     *                 ensure calls to it are protected.  However, since this is intended
     *                 to work between multiple processes, thread safety is not necessarily the
     *                 main concern and a race condition may still be possible.  This is intended
     *                 to only be called once from the creating process.
     */
    bool createLaunchGuard()
    {
        FileHandle handle;

        // this process has already created the launch guard object -> nothing to do => succeed.
        if (m_launchGuard != kBadFileHandle)
            return true;

#if OMNI_PLATFORM_WINDOWS
        std::string name = m_guardName + kLaunchLockExtension;

        handle = CreateEventA(nullptr, false, false, name.c_str());

        if (handle == nullptr)
            return false;

        if (GetLastError() == CARBWIN_ERROR_ALREADY_EXISTS)
        {
            CloseHandle(handle);
            return false;
        }
#else
        handle = _openFile(_getGuardName(kLaunchLockExtension).c_str());

        if (handle == kBadFileHandle)
            return false;

        if (!_lockFile(handle, LockType::eExclusive))
        {
            close(handle);
            return false;
        }
#endif

        // save the exit guard event so we can destroy it later.
        m_launchGuard = handle;

        // intentionally 'leak' the guard handle here.  This will keep the handle open for the
        // entire remaining duration of the process.  This is important because the existence
        // of the guard object is what is used by other host apps to determine if the unique
        // app is already running.  Once the unique app process exits (or it makes a call to
        // @ref destroyLaunchGuard()), the OS will automatically close the guard object.
        return true;
    }

    /** Destroys the locally created launch guard object.
     *
     *  @returns No return value.
     *
     *  This destroys the launch guard object that was most recently created with
     *  @ref createLaunchGuard().  The launch guard object can be recreated with another
     *  call to @ref createLaunchGuard() later.
     *
     *  @thread_safety This call is not thread safe.  It is the caller's responsibility to
     *                 ensure any calls are protected.  However, note that since this is a
     *                 global object potentially used for interprocess operations, there may
     *                 still be a possible race condition with its use.
     */
    void destroyLaunchGuard()
    {
        FileHandle fp = m_launchGuard;

        m_launchGuard = kBadFileHandle;
        _closeFile(fp);
    }

    /** Tests whether the unique app is already running.
     *
     *  @returns `true` if the unique app is currently running.  Returns `false` if the
     *           unique app is not running or if the guard object could not be accessed.
     *
     *  This tests whether the unique app is currently running.  This is done by trying
     *  to detect whether the guard object exists in the system.  When the unique app process
     *  exits (naturally or otherwise), the operating system will remove the guard object
     *  automatically.
     *
     *  This call differs from @ref createLaunchGuard() in that this will not create the guard
     *  object if it does not already exist.  This is intended to be used from host apps before
     *  launching the unique app instead of checking for uniqueness from the unique app itself.
     *
     *  @thread_safety This call is technically thread safe on Windows.  On Linux it is thread
     *                 safe as long as @ref setGuardPath() is not called concurrently.  However,
     *                 since it is meant to test for an object that is potentially owned by
     *                 another process there may still be race conditions that could arise.
     */
    bool checkLaunchGuard()
    {
#if OMNI_PLATFORM_WINDOWS
        HANDLE event;
        DWORD error;
        std::string name = m_guardName + kLaunchLockExtension;

        event = CreateEventA(nullptr, false, false, name.c_str());

        // failed to create the event handle (?!?) => fail.
        if (event == nullptr)
            return false;

        error = GetLastError();
        CloseHandle(event);

        return error == CARBWIN_ERROR_ALREADY_EXISTS;
#else
        FileHandle fp;
        bool success;

        fp = _openFile(_getGuardName(kLaunchLockExtension).c_str());

        if (fp == kBadFileHandle)
            return false;

        success = _lockFile(fp, LockType::eExclusive, LockAction::eTest);
        _closeFile(fp);
        return !success;
#endif
    }

    /** Notifies the unique app that a host app is running.
     *
     *  @returns `true` if the unique app was successfully notified of the new running
     *           host app.  Returns `false` if the notification either couldn't be sent or
     *           could not be completed.
     *
     *  This lets the unique app know that the calling host app is still running.  This
     *  is done by adding a shared lock reference to a marker file that the unique app
     *  can poll on periodically.  The operating system will automatically remove the lock
     *  reference(s) for this call once the calling process exits (naturally or otherwise).
     *
     *  This is intended to be called only once by any given host app.  However, it may be
     *  called multiple times without much issue.  The only downside to calling it multiple
     *  times would be that extra handles (Windows) or file descriptors (Linux) will be
     *  consumed for each call.
     *
     *  @thread_safety This call is not thread safe.  It is the caller's responsibility to
     *                 ensure calls to it are protected.  However, since it is meant to
     *                 operate between processes, there may still be unavoidable race conditions
     *                 that could arise.
     */
    bool connectClientProcess()
    {
        bool success;
        FileHandle fp;

        // this object has already 'connected' to the unique app -> nothing to do => succeed.
        if (m_exitGuard != kBadFileHandle)
            return true;

        fp = _openFile(_getGuardName(kExitLockExtension).c_str());

        // failed to open the guard file (?!?) => fail.
        if (fp == kBadFileHandle)
            return false;

        // grab a shared lock to the file.  This will allow all clients to still also grab a
        // shared lock but will prevent the unique app from grabbing its exclusive lock
        // that it uses to determine whether any client apps are still 'connected'.
        success = _lockFile(fp, LockType::eShared);

        if (!success)
            _closeFile(fp);

        // save the exit guard handle in case we need to explicitly disconnect this app later.
        else
            m_exitGuard = fp;

        // intentionally 'leak' the file handle here.  Since the file lock is associated with
        // the file handle, we can't close it until the process exits otherwise the unique app
        // will think this client has 'disconnected'.  If we leak the handle, the OS will take
        // care of closing the handle when the process exits and that will automatically remove
        // this process's file lock.
        return success;
    }

    /** 'Disconnect' the calling process from the exit guard.
     *
     *  @returns No return value.
     *
     *  This closes the calling process's reference to the exit guard file.  This will allow
     *  the exit guard for a host app process to be explicitly cleaned up before exit if
     *  needed.
     *
     *  @thread_safety This call is not thread safe.  It is the caller's responsibility to
     *                 ensure calls to it are protected.
     */
    void disconnectClientProcess()
    {
        FileHandle fp = m_exitGuard;

        m_exitGuard = kBadFileHandle;
        _closeFile(fp);
    }

    /** Tests whether all 'connected' host apps have exited.
     *
     *  @returns `true` if all connected host apps have exited (naturally or otherwise).  Returns
     *           `false` if at least one host app is still running.
     *
     *  This tests whether all 'connected' host apps have exited.  A host app is considered to
     *  be 'connected' if it had called @ref connectClientProcess() that had succeeded.
     *  Once a host app exits, all of its 'connections' to the unique app are automatically
     *  cleaned up by the operating system.
     *
     *  This is intended to be called from the unique app periodically to determine if it
     *  should exit itself.  The expectation is that this would be called once per minute or
     *  so to check whether it should be shut down.  When this does succeed, the unique app
     *  should perform any final cleanup tasks then exit itself.
     *
     *  @thread_safety This call is not thread safe.  It is the caller's responsibility to ensure
     *                 calls to it are protected.  However, since it is meant to operate between
     *                 processes, there may still be unavoidable race conditions that could arise.
     */
    bool haveAllClientsExited()
    {
        bool success;
        FileHandle fp;
        std::string path;

        path = _getGuardName(kExitLockExtension);
        fp = _openFile(path.c_str());

        // failed to open the guard file (?!?) => fail.
        if (fp == kBadFileHandle)
            return false;

        success = _lockFile(fp, LockType::eExclusive, LockAction::eTest);
        _closeFile(fp);

        // the file lock was successfully acquired -> no more clients are 'connected' => delete
        //   the lock file as a final cleanup step.
        if (success)
            _deleteFile(path.c_str());

        // all the clients have 'disconnected' when we're able to successfully grab an exclusive
        // lock on the file.  This means that all of the clients have exited and the OS has
        // released their shared locks on the file thus allowing us to grab an exclusive lock.
        return success;
    }

private:
    /** Extension of the name for the launch guard locks. */
    static constexpr const char* kLaunchLockExtension = ".lock";

    /** Extension of the name for the exit guard locks. */
    static constexpr const char* kExitLockExtension = ".exit";

    /** Default prefix for the lock guards. */
    static constexpr const char* kDefaultNamePrefix = "nvidia-unique-app";

#if OMNI_PLATFORM_WINDOWS
    using FileHandle = HANDLE;
    static constexpr FileHandle kBadFileHandle = CARBWIN_INVALID_HANDLE_VALUE;
#else
    /** Platform specific type for a file handle. */
    using FileHandle = int;

    /** Platform specific value for a failed file open operation. */
    static constexpr FileHandle kBadFileHandle = -1;
#endif

    /** Names for the type of lock to apply or test. */
    enum class LockType
    {
        /** A shared (read) lock.  Multiple shared locks may succeed simultaneously on the same
         *  file.  If at least one shared lock exists on a file, it will prevent anything else
         *  from acquiring an exclusive lock on the same file.
         */
        eShared,

        /** An exclusive (write) lock.  Only a single exclusive lock may exist on a file at any
         *  given time.  The file must be completely unlocked in order for an exclusive lock to
         *  succeed (unless a process's existing shared lock is being upgraded to an exclusive
         *  lock).  If the file has any shared locks on it from another process, an exclusive
         *  lock cannot be acquired.
         */
        eExclusive,
    };

    /** Names for the action to take when locking a file. */
    enum class LockAction
    {
        eSet, ///< attempt to acquire a lock on the file.
        eTest, ///< test whether a lock is immediately possible on a file.
    };


    /** Creates the full name of the guard object for a given extension.
     *
     *  @param[in] extension    The extension to use on the guard name.  This should either be
     *                          @a kLaunchLockExtension or @a kExitLockExtension.
     *  @returns A string representing the full name and path of the guard object to create or
     *           test.
     */
    std::string _getGuardName(const char* extension) const
    {
        return m_guardPath + "/" + m_guardName + extension;
    }

    /** Opens a guard file for read and write.
     *
     *  @param[in] filename     The name and path of the file to open.  This may not be `nullptr`
     *                          or an empty string.  The file may already exist.  If the file
     *                          does not exist, it will be created.
     *  @returns A handle to the opened file.  This may be passed to _closeFile() or _lockFile()
     *           as needed.  When the handle is no longer necessary, it should be closed using
     *           _closeFile().  Returns @a kBadFileHandle if the file could not be opened.
     */
    static FileHandle _openFile(const char* filename)
    {
#if OMNI_PLATFORM_WINDOWS
        std::wstring pathW = carb::extras::convertCarboniteToWindowsPath(filename);
        return CreateFileW(pathW.c_str(), CARBWIN_GENERIC_READ | CARBWIN_GENERIC_WRITE,
                           CARBWIN_FILE_SHARE_READ | CARBWIN_FILE_SHARE_WRITE, nullptr, CARBWIN_OPEN_ALWAYS, 0, nullptr);
#else
        return open(filename, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IROTH | S_IRGRP);
#endif
    }

    /** Closes a guard file opened with _openFile().
     *
     *  @param[in] fp   The file handle to close.  This may be kBadFileHandle.
     *  @returns No return value.
     */
    static void _closeFile(FileHandle fp)
    {
#if OMNI_PLATFORM_WINDOWS
        CloseHandle(fp);
#else
        close(fp);
#endif
    }

    /** Deletes a file on the file system.
     *
     *  @param[in] filename     The name and path of the file to delete.  This may not be
     *                          `nullptr` or an empty string.  The file will only be deleted
     *                          if all open handles to it have been closed.  Depending on the
     *                          platform, the file may still exist and be able to be opened
     *                          again if an open file handle to it still exists.
     *  @returns No return value.
     */
    static void _deleteFile(const char* filename)
    {
#if OMNI_PLATFORM_WINDOWS
        std::wstring pathW = carb::extras::convertCarboniteToWindowsPath(filename);
        DeleteFileW(pathW.c_str());
#else
        unlink(filename);
#endif
    }

    /** Attempts to lock a file.
     *
     *  @param[in] fp       The handle to the file to attempt to lock.  This must be a valid
     *                      file handle returned by a call to _openFile().  This may not be
     *                      @a kBadFileHandle.
     *  @param[in] type     The type of lock to grab.
     *  @param[in] action   Whether to set the lock or test it.  Setting the lock will grab
     *                      a new lock reference to the file.  Testing it will just check if
     *                      the requested type of lock can be immediately grabbed without
     *                      changing the file's lock state.
     *  @returns `true` if the lock is successfully acquired in 'set' mode or if the lock is
     *           immediately available to be acquired in 'test' mode.  Returns `false` if the
     *           lock could not be acquired in 'set' mode or if the lock was not immediately
     *           available to be acquired in 'test' mode.  Also returns `false` if an error
     *           occurred.
     */
    static bool _lockFile(FileHandle fp, LockType type, LockAction action = LockAction::eSet)
    {
#if OMNI_PLATFORM_WINDOWS
        BOOL success;
        CARBWIN_OVERLAPPED ov = {};
        DWORD flags = CARBWIN_LOCKFILE_FAIL_IMMEDIATELY;


        if (type == LockType::eExclusive)
            flags |= CARBWIN_LOCKFILE_EXCLUSIVE_LOCK;

        success = LockFileEx(fp, flags, 0, 1, 0, reinterpret_cast<LPOVERLAPPED>(&ov));

        if (action == LockAction::eTest)
            UnlockFileEx(fp, 0, 1, 0, reinterpret_cast<LPOVERLAPPED>(&ov));

        return success;
#else
        int result;
        struct flock fl;


        fl.l_type = (type == LockType::eExclusive ? F_WRLCK : F_RDLCK);
        fl.l_whence = SEEK_SET;
        fl.l_start = 0;
        fl.l_len = 1;
        fl.l_pid = 0;
        result = fcntl(fp, (action == LockAction::eTest ? F_GETLK : F_SETLK), &fl);

        if (result != 0)
            return false;

        if (action == LockAction::eTest)
            return fl.l_type == F_UNLCK;

        return true;
#endif
    }


    /** The path to use to store the guard files.  This may either be a relative or absolute
     *  path and will most often be specified by the caller.  This defaults to the current
     *  directory.
     */
    std::string m_guardPath = ".";

    /** The name to use for the guard files.  This should be unique to the app being launched.
     *  There is no formatting or syntax requirement for this aside from being valid on the
     *  file system for the calling platform.  This defaults to @a kDefaultNamePrefix.
     */
    std::string m_guardName = kDefaultNamePrefix;

    /** The last created launch guard object.  This is used to prevent multiple launch guard
     *  objects from being created from this object for a single unique app process.
     */
    FileHandle m_launchGuard = kBadFileHandle;

    /** The last created exit guard object.  This is used to prevent multiple 'connections'
     *  to the unique app from being created from this object for a single host app process.
     */
    FileHandle m_exitGuard = kBadFileHandle;
};

} // namespace extras
} // namespace omni
