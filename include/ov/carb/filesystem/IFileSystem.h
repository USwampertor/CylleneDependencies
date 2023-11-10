// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//! @brief Carbonite FileSystem interface definition file.
#pragma once

#include "../Defines.h"

#include "../Types.h"

#include <stdlib.h>
#include <string>
#include <time.h>
#include <vector>

namespace carb
{
//! Namespace for Carbonite FileSystem.
namespace filesystem
{

struct File;
struct IFileSystem;

//! Type definition of a subscription.
typedef uint32_t SubscriptionId;

//! Indicates an invalid subscription.
const SubscriptionId kInvalidSubscriptionId = 0;

/**
 * Defines the type of directory item.
 */
enum class DirectoryItemType
{
    eFile,
    eDirectory,
};

/**
 * Defines change action that is reported to callback function.
 */
enum class ChangeAction
{
    /// Reported when a file is created
    eCreated,

    /// Reported when a file is modified
    eModified,

    /// Reported when a file is deleted
    eDeleted,

    /// Generally reported when a file is renamed. However, due to OS limitations in how events are delivered, a rename
    /// may come through on rare occasion as separate eCreated and eDeleted events.
    /// These are supported on Windows and Linux but not Mac OS.
    eRenamed
};

/**
 * Defines the behavior requested by the callback function.
 */
enum class WalkAction : signed char
{
    /// Stops all iteration and causes forEachDirectoryItem[Recursive] to return immediately.
    eStop = -1,

    /// Skips the rest of the current directory and any remaining subdirectories of the current directory.
    eSkip,

    /// Continues iteration.
    eContinue,
};

/**
 * Information about a file
 */
struct FileInfo
{
    /// The type of this item: Directory or File.
    DirectoryItemType type;

    /// The time that this item was last modified.
    time_t modifiedTimestamp;

    /// The time that this item was created.
    time_t createdTimestamp;

    /// The size of this item in bytes.
    size_t size;

    /// Whether this item is a symlink. On Windows, this is a reparse point which also includes directory junctions.
    bool isSymlink;
};

/**
 * Defines struct to hold item info during directory walk.
 */
struct DirectoryItemInfo : public FileInfo
{
    /// The path to the file. Make a copy of the string if retention is desired after the callback.
    const char* path;
};

/**
 * fixed positions in a file that a file pointer can be moved relative to.
 */
enum class FileWhence
{
    eBegin, ///< beginning of the file.
    eCurrent, ///< current position in the file.
    eEnd, ///< end of the file.
};

/**
 * defines the potential states that an open file stream can be in.  These states are only valid
 * after an operation such as read, write, seek, etc complete.  The current state of the file
 * stream can be retrieved with getFileStatus().  Its return value will persist until another
 * operation on the stream completes.
 */
enum class FileStatus
{
    eOk, ///< the stream is valid and ready to be operated on.  No special state is set.
    eEof, ///< the stream has reached an end-of-file condition on the last operation.
    eError, ///< the stream has encountered an error on the last operation.
};

/** Base type for flags for the IFileSystem::makeCanonicalPathEx2() function. */
using CanonicalFlags = uint32_t;

/** Flag to indicate that the file must also exist in order for the function to succeed.  When
 *  this flag is used, the behavior will match IFileSystem::makeCanonicalPathEx2().
 */
constexpr CanonicalFlags fCanonicalFlagCheckExists = 0x01;

/**
 * Defines the callback function to use when listening to changes on file system.
 *
 * @param path The path for file system change.
 * @param action The change action that occurred.
 * @param userData The user data associated with the subscription to the change event.
 * @param newPath The path for the new name of the file. Used only for eRenamed action, otherwise it's nullptr
 */
typedef void (*OnChangeEventFn)(const char* path, ChangeAction action, void* userData, const char* newPath);

/**
 * Defines a file system for Carbonite.
 *
 * This interface provides a number of useful platform independent functions when working with files and folders in a
 * file system. All paths are in UTF-8 encoding using forward slash as path separator.
 *
 * On Windows, the maximum path of 32767 characters is supported. However, path components can't be longer than 255
 * characters.
 * Linux has a maximum filename length of 255 characters for most filesystems (including EXT4), and a
 * maximum path of 4096 characters.
 */
struct IFileSystem
{
    CARB_PLUGIN_INTERFACE("carb::filesystem::IFileSystem", 1, 2)

    /**
     * Returns the full path to the executable for this program.
     *
     * @return The full canonical path to the executable, including executable name and extension.
     *         This path will not change for the lifetime of the process.
     */
    const char*(CARB_ABI* getExecutablePath)();

    /**
     *  Returns the full path to the directory that contains the executable for this program.
     *
     *  @returns the full canonical path to the directory that contains the executable file.
     *           This will not include the executable filename itself.  This path will not
     *           change for the lifetime of the process.
     */
    const char*(CARB_ABI* getExecutableDirectoryPath)();

    /**
     *  Retrieves the full path to the 'app'.
     *
     *  @returns the buffer containing the application path string.  The contents of this buffer
     *           will be modified by any call to setAppDirectoryPath().  The buffer itself will
     *           persist for the lifetime of the framework.
     *
     *  @note Access to the application directory string is not thread safe.  It is the caller's
     *        responsibility to ensure the application path is not being modified from another
     *        thread while it is being retrieved.
     */
    const char*(CARB_ABI* getAppDirectoryPath)();

    /**
     *  Sets the full path to the 'app'.
     *
     *  @param[in] path the relative or absolute path to the 'app'.  If a relative path is used,
     *                  this will be resolved relative to the current working directory.
     *  @returns no return value.
     */
    void(CARB_ABI* setAppDirectoryPath)(const char* path);

    /**
     *  Returns the full path to the current working directory.
     *
     *  @returns the buffer containing the current working directory path string.  The contents
     *           of this buffer will be modified by any call to [gs]etCurrentDirectoryPath().
     *           The buffer itself will persist for the lifetime of the framework.
     *
     *  @note Retrieving the current working directory is not thread safe.  Since only a single
     *        working directory is maintained for each process, it could be getting changed from
     *        another thread while being retrieved.  It is the caller's responsibility to ensure
     *        that all access to the current working directory is safely serialized.
     */
    const char*(CARB_ABI* getCurrentDirectoryPath)();

    /**
     *  Sets the current working directory for the system.
     *
     *  @param[in] path the new current working directory path.  This may be a relative or
     *                  absolute path.  This must name a directory that already exists.  This
     *                  name must not exist as a file on the file system.  This may not be
     *                  nullptr.
     *  @returns true if the new working directory is successfully set.
     *  @returns false if the new working directory could not be set.
     *
     *  @note Setting or retrieving the current working directory is not thread safe.  Since
     *        the current working directory is global to the process, the caller is responsible
     *        for guaranteeing that the working directory will not change while attempting to
     *        retrieve it.
     */
    bool(CARB_ABI* setCurrentDirectoryPath)(const char* path);

    /**
     * Tests whether the path provided exists in the file system.
     *
     * @param path The absolute or relative path to test for existence. Relative paths are resolved from
     *  the current working directory (as returned from getCurrentDirectoryPath()).
     * @return true if and only if 'path' exists in the file system.
     */
    bool(CARB_ABI* exists)(const char* path);

    /**
     * Tests whether it's possible to write to file with the provided path.
     *
     * @param path The absolute or relative path to test for writability. Relative paths are resolved from
     *  the current working directory (as returned from getCurrentDirectoryPath()).
     * @return true if it's possible to write to this file.
     *
     * @note This accessibility check only answers the question of whether the user has
     *       _permission_ to write to the file, not that an open for write will always succeed.
     *       At least on Windows, it is still possible that another thread or process could
     *       have the file open without write sharing capabilities.  In this case, the caller
     *       should just do a test open of the file since that will answer the question of
     *       whether write sharing is currently allowed on the file.  On Linux there isn't any
     *       kernel enforced file sharing functionality so permission to the file should also
     *       imply the user will succeed to open it for write.
     */
    bool(CARB_ABI* isWritable)(const char* path);

    /**
     * Tests whether the path provided is a directory.
     *
     * @param path The absolute or relative path to test for existence. Relative paths are resolved from
     *  the current working directory (as returned from getCurrentDirectoryPath()).
     * @return true if and only if 'path' is a directory.
     */
    bool(CARB_ABI* isDirectory)(const char* path);

    /**
     * Use OS specific functions to build canonical path relative to the base root.
     *
     * The path must exist.
     *
     * If returned size is greater than passed bufferSize, then nothing is written to the buffer.
     * If returned size is 0, then canonical path failed to be built or doesn't exist.
     *
     * @param path  The absolute or relative path to canonicalize.
     * @param base  The base path to resolve relative path against.  This can be `nullptr`to use
     *              the working directory (as returned from getCurrentDirectoryPath()) to resolve
     *              the relative path.
     * @param buffer The buffer to write the canonical path to.  This may be `nullptr` if only
     *               the required size of the buffer is needed.
     * @param bufferSize The size of the buffer @p buffer in bytes.
     * @return The number of bytes written to the buffer @p buffer if the buffer is large enough.
     *         If the buffer is not large enough, nothing will be written to the buffer and the
     *         required size of the buffer in bytes will be returned.
     */
    size_t(CARB_ABI* makeCanonicalPathEx)(const char* path, const char* base, char* buffer, size_t bufferSize);

    /**
     * Helper function over makeCanonicalPathEx2() to use it with std::string.
     */
    std::string makeCanonicalPath(const char* path,
                                  const char* base = nullptr,
                                  CanonicalFlags flags = fCanonicalFlagCheckExists);

    /**
     *  Opens a file for reading in binary mode.
     *
     *  @param[in] path     The absolute or relative path for the file.  This may not be nullptr.
     *                      Relative paths are resolved from the current working directory (as
     *                      returned from getCurrentDirectoryPath()).
     *  @returns a new File object representing the opened file if the file exists and was able
     *           to be successfully opened for read.  This object must be passed to closeFile()
     *           when it is no longer needed.
     *  @returns nullptr if the named file does not exist in the file system or another error
     *           occurred (ie: insufficient permissions, allocation failure, etc).  A warning
     *           will be written to the default logger in this case.
     *
     *  @remarks This opens an existing file for reading.  If the file does not exist, this will
     *           fail.  A new file will never be created if the named file does not already exist.
     *           If a new file needs to be created, it must first be opened for write with
     *           openFileToWrite(), for read and write with openFileToReadWrite(), or for append
     *           with openFileToAppend().  The file pointer will initially be at the beginning of
     *           the file.  All reads will occur starting from the current file pointer position.
     */
    File*(CARB_ABI* openFileToRead)(const char* path);

    /**
     *  Opens a file for writing in binary mode.
     *
     *  @param[in] path     The absolute or relative path for the file.  This may not be nullptr.
     *                      Relative paths are resolved from the current working directory (as
     *                      returned from getCurrentDirectoryPath()).
     *  @returns a new File object representing the opened file if successful.  A new file will
     *           have been created if it previously did not exist.  This object must be passed
     *           to closeFile() when it is no longer needed.
     *  @returns nullptr if the named file could neither be created nor opened.  This may be
     *           the result of insufficient permissions to the file or an allocation failure.
     *           A warning will be written to the default logger in this case.
     *
     * @remarks This opens a file for writing.  If the file does not exist, it will be created.
     *          If the file does exist, it will always be truncated to an empty file.  The file
     *          pointer will initially be positioned at the beginning of the file.  All writes to
     *          the file will occur at the current file pointer position.  If the file needs
     *          to be opened for writing without truncating its contents, it should be opened
     *          either for append access (ie: openFileToAppend()) or for read/write access
     *          (ie: openFileToReadWrite()).
     */
    File*(CARB_ABI* openFileToWrite)(const char* path);

    /**
     *  Opens a file for appending in binary mode.
     *
     *  @param[in] path     The absolute or relative path for the file.  This may not be nullptr.
     *                      Relative paths are resolved from the current working directory (as
     *                      returned from getCurrentDirectoryPath()).
     *  @returns a new File object representing the opened file if successful.  A new file will
     *           have been created if it previously did not exist.  This object must be passed
     *           to closeFile() when it is no longer needed.
     *  @returns nullptr if the named file could neither be created nor opened.  This may be
     *           the result of insufficient permissions to the file or an allocation failure.
     *           A warning will be written to the default logger in this case.
     *
     *  @remarks This opens a file for appending.  If the file does not exist, it will always be
     *           created.  The file pointer is initially positioned at the end of the file.  All
     *           writes to the file will be performed at the end of the file regardless of the
     *           current file pointer position.  If random access writes are needed, the file
     *           should be opened for read/write access (ie: openFileToReadWrite()) instead.
     */
    File*(CARB_ABI* openFileToAppend)(const char* path);

    /**
     *  Closes a file returned by any of the openFileTo*() functions.
     *
     *  @param[in] file     the File object representing the file to be closed.  This object
     *                      will no longer be valid upon return and must not be used again.
     *                      This object would have been returned by a previous openFileTo*()
     *                      call.
     *  @returns no return value.
     *
     *  @remarks This closes a file that was previously opened by a call to the openFileTo*()
     *           functions.  The file object will be destroyed by this call and must not be
     *           used again.
     */
    void(CARB_ABI* closeFile)(File* file);

    /**
     * Gets the total size of the file.
     *
     * @param File object corresponding to an open file.
     * @return The total size of the file in bytes.
     */
    size_t(CARB_ABI* getFileSize)(File* file);

    /**
     * Gets the time of last modification to the file.
     *
     * @param file object corresponding to an open file.
     * @return The time this file was last modified.
     */
    time_t(CARB_ABI* getFileModTime)(File* file);

    /**
     * Gets the time of last modification to the file or directory item at path.
     *
     * @param path  The path to a file or directory item; relative paths are resolved from the current
     *              working directory (as returned from getCurrentDirectoryPath()).
     * @return The time the item at 'path' was last modified.
     */
    time_t(CARB_ABI* getModTime)(const char* path);

    /**
     * Gets the time of creation of the file.
     *
     * @param file object corresponding to an open file.
     * @return The time this file was created.
     */
    time_t(CARB_ABI* getFileCreateTime)(File* file);

    /**
     * Gets the time of creation of the file or directory item at path.
     *
     * @param path  The path to a file or directory item; relative paths are resolved from the current
     *              working directory (as returned from getCurrentDirectoryPath()).
     * @return The time the item at 'path' was created.
     */
    time_t(CARB_ABI* getCreateTime)(const char* path);

    /**
     * Reads a chunk of binary data from a file.
     *
     * @param file Object corresponding to an open file for reading in binary mode.
     * @param chunk Memory to read the binary data to, at least chunkSize bytes large.
     * @param chunkSize Number of bytes to read from file into 'chunk' memory area.
     * @return Number of bytes read, this can be less than requested 'chunkSize' when reading the last bytes of
     *  data. Will return 0 when all data has been read from the file.
     */
    size_t(CARB_ABI* readFileChunk)(File* file, void* chunk, size_t chunkSize);

    /**
     * Writes a chunk of binary data to a file.
     *
     * @param file An open file for writing in binary mode.
     * @param chunk The memory buffer to write to the file.
     * @param chunkSize Number of bytes from 'chunk' to write to the file.
     * @returns the number of bytes successfully written to the file.  This can be less than the
     *          requested @p chunkSize if an error occurs (ie: disk full).
     * @returns 0 if no data could be written to the file.
     */
    size_t(CARB_ABI* writeFileChunk)(File* file, const void* chunk, const size_t chunkSize);

    /**
     * Reads a line of character data from a text file (without including the line ending characters `\r` or `\n`).
     *
     * @note This function considers a `\n` by itself to be a line ending, as well as `\r\n`. A `\r` by itself is not
     *  considered a line ending. The line endings are consumed from the file stream but are not present in the result.
     * @note For @p maxLineSize of 0, `nullptr` is always returned without any change to the @p file read pointer. For
     *  @p maxLineSize of 1 when not at end-of-file, @p line will only contain a NUL terminator and if a line ending is
     *  at the start of the file stream it will be consumed.
     *
     * @param file A file returned from openFileToRead() or openFileToReadWrite().
     * @param line The string that will receive the read line. Unlike `fgets()`, the result will NOT end with any line
     *  ending characters (`\n` or `\r\n`), but they will be consumed from the file stream.
     * @param maxLineSize The maximum number of characters that can be read into @p line, including NUL terminator.
     *  If the buffer is exhausted before end-of-line is reached the buffer will be NUL terminated and thus still a
     *  proper C-style string but won't necessarily contain the full line from the file.
     * @return Returns @p line on each successful read, or `nullptr` if @p file is at end-of-file.
     */
    char*(CARB_ABI* readFileLine)(File* file, char* line, size_t maxLineSize);

    /**
     * Writes a line of characters to a text file.
     *
     * @param file An file returned from openFileToWrite() or openFileToAppend().
     * @param line The null-terminated string to write.  A newline will always be appended to the
     *        string in the file if it is successfully written.
     * @returns true if the string is successfully written to the file.
     * @returns false if the full string could not be written to the file.
     */
    bool(CARB_ABI* writeFileLine)(File* file, const char* line);

    /**
     * Flushes any unwritten data to the file.
     *
     * When a file is closed, either by calling closeFile or during program termination, all the associated buffers are
     * automatically flushed.
     *
     * @param file An open file for writing or appending.
     */
    void(CARB_ABI* flushFile)(File* file);

    /**
     *  Removes (deletes) a file.
     *
     *  @param[in] path  The path of the file to be removed.  This must not have any open file
     *                   objects on it otherwise the operation will fail.
     *  @returns true if the file was removed from the file system.
     *  @returns false if the file could not be removed.  This is often caused by either having
     *           the file still open by either the calling process or another process, or by
     *           not having sufficient permission to delete the file.
     */
    bool(CARB_ABI* removeFile)(const char* path);

    /**
     * Make a temporary directory.
     *
     * The directory is created under the system temporary directory area and will have a randomized name.
     *
     * @param pathBuffer The buffer that will receive the full path to the created directory.  This may not
     *                   be `nullptr`.
     * @param bufferSize The size of the buffer for storing the path.  This size also includes the null
     *                   terminator for the string.  If this is too small to store the output path.
     * @return `true` if the creation was successful and a path to the newly created temporary directory
     *         was returned in @p pathBuffer.  On success, the temporary directory is guaranteed to exist
     *         and be writable by the caller.  The caller is responsible for removing this directory when
     *         it is no longer needed.
     * @return `false` if the temporary directory could not be created for any reason.  In this case, the
     *         @p pathBuffer buffer will not be modified and its contents will be undefined.
     */
    bool(CARB_ABI* makeTempDirectory)(char* pathBuffer, size_t bufferSize);

    /** Make a single directory.
     *
     *  @param path  The path to the directory to create.  Relative paths will be resolved from the
     *               current working directory (as returned from getCurrentDirectoryPath()).  This
     *               may not be nullptr or an empty string.
     *  @return true if the path did not previously exist and the creation as a folder was successful.
     *  @return true if the path already existed as a directory.
     *  @return false if the path already existed as a non-directory entry.
     *  @return false if the path could not be created for a reason such as permission errors or an
     *          invalid path name being specified.
     *
     *  @remarks This attempts to make a single new directory entry.  All path components leading up
     *           to the new path must already exist for this to be expected to succeed.  The path
     *           may already exist and this call will still succeed.
     *
     *  @remarks Note that this operation is global to the system.  There is no control over what
     *           other threads or processes in the system may be simultaneously doing to the named
     *           path.  It is the caller's responsibility to gracefully handle any potential failures
     *           due to the action of another thread or process.
     *
     *  @note There is a possible race condition with another thread or process creating the same
     *        path simultaneously.  If this occurs, this call will still succeed in most cases.
     *        There is an additional rare possible race condition where the file or folder could
     *        also be deleted by an external thread or process after it also beat the calling thread
     *        to creating the path.  In this case, this call will fail.  For this to occur there
     *        would need to be the named path created then immediately destroyed externally.
     *
     *  @note This call itself is thread safe.  However, the operation it performs may race with
     *        other threads or processes in the system.  Since file system directories are global
     *        and shared by other processes, an external caller may create or delete the same
     *        directory as is requested here during the call.  There is unfortunately no way to
     *        prevent this or make it safer since the creators or deleters of the path may not
     *        even be local to the system (ie: a network share operation was requested).  The
     *        best a caller can do you be to guarantee its own threads do not simultaneously
     *        attempt to operate on the same path.
     */
    bool(CARB_ABI* makeDirectory)(const char* path);

    /** Make one or more directories.
     *
     *  @param path  The path to the directory to create.  Relative paths will be resolved from the
     *               current working directory (as returned from getCurrentDirectoryPath()).  This
     *               may not be nullptr or an empty string.
     *  @return true if the path did not previously exist and the creation as a folder was successful.
     *  @return true if the path already existed as a directory.
     *  @return false if the path already existed as a non-directory entry.
     *  @return false if the path could not be created for a reason such as permission errors or an
     *          invalid path name being specified.
     *
     *  @remarks This attempts to create one or more directories.  All components listed in the path
     *           will be created if they do not already exist.  If one of the path components already
     *           exists as a non-directory object, the operation will fail.  If creating any of the
     *           intermediate path components fails, the whole operation will fail.  If any of the
     *           components already exists as a directory, it will be ignored and continue with the
     *           operation.
     *
     *  @note This call itself is thread safe.  The operation itself may have a race condition with
     *        other threads or processes however.  Please see makeDirectory() for more information
     *        about these possible race conditions.
     */
    bool(CARB_ABI* makeDirectories)(const char* path);

    /**
     * Remove a directory.
     *
     * @param path  The path to the directory to remove; relative paths will be resolved from the
     *              current working directory (as returned from getCurrentDirectoryPath()).
     * @return true if the removal was successful, otherwise false.
     * @note This will never follow symbolic links. The symbolic link will be removed, but its target will not.
     * @note On Windows, it is neither possible to remove the current working directory nor any
     *       directory containing it.  This is because the Windows process holds an open handle
     *       to the current working directory without delete sharing permissions at all times.
     *       In order to remove the current working directory, the caller must first change the
     *       working directory to another valid path, then call removeDirectory().  On Linux,
     *       removing the current working directory is technically possible, however, doing so
     *       will leave the process in an undefined state since its working directory is no longer
     *       valid.  Changing away from the working directory before calling this is still a good
     *       idea even on Linux.
     */
    bool(CARB_ABI* removeDirectory)(const char* path);

    /**
     * Copy a file.
     *
     * @param from  The path to a file to copy; relative paths will be resolved from the current
     *              working directory (as returned from getCurrentDirectoryPath()).
     * @param to    The destination filename and path; relative paths will be resolved from the
     *              current working directory (as returned from getCurrentDirectoryPath()).
     */
    bool(CARB_ABI* copy)(const char* from, const char* to);

    /**
     * Moves (renames) a file or directory.
     *
     * @param from  The path to a file or directory to rename; relative paths will be resolved from
     *              the current working directory (as returned from getCurrentDirectoryPath()).
     * @param to    The destination path; relative paths will be resolved from the current working
     *              directory (as returned from getCurrentDirectoryPath()).
     */
    bool(CARB_ABI* move)(const char* from, const char* to);

    /**
     * User implemented callback function type for directory iteration.
     *
     * @param Info about a file. See `DirectoryItemInfo`
     * @param userData Any data that needs to be passed to the function for managing state across function calls, etc.
     * @return one of the `WalkAction` enum values to instruct forEachDirectoryItem[Recursive] on how to proceed.
     */
    typedef WalkAction (*OnDirectoryItemFn)(const DirectoryItemInfo* const info, void* userData);

    /**
     * Iterate through each item in the directory.
     *
     * @param path  The path to the directory; relative paths will be resolved from the current
     *              working directory (as returned from getCurrentDirectoryPath()).
     * @param onDirectoryItem The function to call for each directory item, see `OnDirectoryItemFn` type
     * @param userData The user data passed to the callback function for each item.
     */
    void(CARB_ABI* forEachDirectoryItem)(const char* path, OnDirectoryItemFn onDirectoryItem, void* userData);

    /**
     * Iterate through each item in the directory and recursive into subdirectories.
     *
     * @param path  The path to the directory; relative paths will be resolved from the current
     *              working directory (as returned from getCurrentDirectoryPath()).
     * @param onDirectoryItem The function to call for each directory item, see IFileSystem::DirectoryCallback type
     * @param userData The user data passed to the callback function for each item.
     * @note This will follow symbolic links.
     */
    void(CARB_ABI* forEachDirectoryItemRecursive)(const char* path, OnDirectoryItemFn onDirectoryItem, void* userData);

    /**
     * Subscribes to listen on change events on a path.
     *
     * @param path The path to subscribe to.
     *             This path must be an existing file or directory on disk.
     * @param onChangeEvent The callback function to be called when the events are fired.
     * @param userData The user data passed to the callback function for each item.
     * @return subscription id if the path was successfully subscribed to or nullptr otherwise.
     * @return kInvalidSubscriptionId if @p path does not exist.
     * @return kInvalidSubscriptionId if some other error occurred.
     *
     * @note The change subscriptions on directories are not recursive.
     *       If you watch a directory, you will not get change events for
     *       files/directories within subdirectories.
     *
     * @note Modifications to subdirectories (e.g. creating/deleting a file
     *       inside it) will not produce change events.
     *       OM-1071: there is a bug in the Windows implementation where deleting
     *       a file in a subdirectory produces a @ref ChangeAction::eModified event.
     *
     * @note If @p path points to a directory, deleting the directory will produce
     *       a @ref ChangeAction::eDeleted event and then no more events will be
     *       produced from the stream, even if the directory is created again.
     *       This behavior differs if @p path points to a file. Change events
     *       will continue to be produced for a file after it's been deleted if
     *       it's created again.
     *
     * @note Mac OS only appears to support ~512 subscriptions in a process
     *       (this value was determined experimentally).
     *       For subscription 513+, notifications are no longer sent.
     */
    SubscriptionId(CARB_ABI* subscribeToChangeEvents)(const char* path, OnChangeEventFn onChangeEvent, void* userData);

    /**
     * Unsubscribes from listening to change events on a path.
     *
     * @note It is safe to call this from within the callback passed to \ref subscribeToChangeEvents(). The function
     * will not return until the subscription callback is guaranteed to be exited by all other threads.
     *
     * @param subscription Subscription id
     */
    void(CARB_ABI* unsubscribeToChangeEvents)(SubscriptionId subscriptionId);

    /**
     *  retrieves the current file pointer position for an open file.
     *
     *  @param[in] file the file object to retrieve the current position for.  This may have been
     *                  opened for read or write.  Files that were opened for append will always
     *                  write at the end of the file regardless of the current file position.  The
     *                  file pointer's current position is typically unused or undefined in the
     *                  append case.
     *  @returns the current position in the file in bytes relative to the beginning.
     *  @returns -1 if the file's position could not be retrieved.
     *
     *  @remarks This retrieves the current location of the file pointer in a file that has been
     *           opened for read, write, or append.  The offset is always returned in bytes.  The
     *           current file position may be beyond the end of the file if the file pointer was
     *           recently placed beyond the end of the file.  However, this does not actually
     *           reflect the size of the file until at least one byte is written into it at the
     *           new position beyond the file's end.
     */
    int64_t(CARB_ABI* getFilePosition)(File* file);

    /**
     *  sets the new file pointer position for an open file.
     *
     *  @param[in] file the file object to set the current position for.  This may have been
     *                  opened for read or write.  Files that were opened for append will
     *                  always write at the end of the file regardless of the current file
     *                  position.  The file pointer's current position is typically unused or
     *                  undefined in the append case.
     *  @param[in] offsetFromWhence the new position for the file pointer relative to the
     *                              location specified in @p whence.  This value may be negative
     *                              only if @p whence is not FileWhence::eBegin.  This may specify
     *                              an index beyond the current end of the file when combined with
     *                              @p whence.
     *  @param[in] whence   the fixed location in the file to move the file pointer relative to.
     *  @returns true if the file position was successfully set.
     *  @returns false if the file position could not be set or was invalid.
     *
     *  @remarks This attempts to reposition the file pointer in an open file.  The new absolute
     *           position may not be negative once combined with @p whence.  If the new absolute
     *           position is beyond the current end of the file, the file will not be extended
     *           until at least one byte is written into the file at that new position or the file
     *           is truncated at the current position with truncateFileAtCurrentPosition().  When
     *           it is written to or truncated with a larger size than previous, the new space
     *           will be filled with zeros.  Note however, that if the file pointer is set beyond
     *           the end of the file, the getFilePosition() call will return that same position
     *           even though it is larger than the file currently is.
     */
    bool(CARB_ABI* setFilePosition)(File* file, int64_t offsetFromWhence, FileWhence whence);

    /**
     *  truncates a file at the current file position.
     *
     *  @param[in] file     the file to be truncated.  This must have been opened for write or
     *                      append.
     *  @returns true if the file was successfully truncated.
     *  @returns false if the file could not be truncated for any reason.
     *
     *  @remarks This truncates a file at the current file pointer position.  This can be used
     *           to extend a file without needing to write anything to it by opening the file,
     *           setting the file pointer to the desired size with setFilePointer(), then calling
     *           this function to set the new end of the file.  The new area of the file will be
     *           filled with zeros if it was extended.  If the file is being shortened, all data
     *           in the file beyond the current file pointer will be removed.
     */
    bool(CARB_ABI* truncateFileAtCurrentPosition)(File* file);

    /**
     *  helper functions to move to the beginning or end of an open file.
     *
     *  @param[in] file     the file stream to rewind or jump to the end of.
     *  @returns true if the file pointer is successfully returned to the beginning or end of the
     *           file.
     *  @returns false if the file pointer could not be repositioned.
     *
     *  @remarks These move the file pointer to the beginning or end of the file.  These are just
     *           convenience helper functions built on top of setFilePosition().
     */
    bool setFilePositionBegin(File* file);
    //! @copydoc setFilePositionBegin()
    bool setFilePositionEnd(File* file);

    /**
     *  opens the file for read and write in binary mode.
     *
     *  @param[in] path     the absolute or relative path to the file to open.  This may not be
     *                      nullptr.  Relative paths are resolved from the current working
     *                      directory (as returned from getCurrentDirectoryPath()).
     *  @returns a new open file stream object if the file is successfully opened.  This file
     *           object must be closed with closeFile() when it is no longer needed.
     *  @returns nullptr if the file could not be opened for any reason.  This can occur if the
     *           file could not be created or there are insufficient permissions to access the
     *           file, or an allocation failure occurred.  A warning will be written to the
     *           default logger in this case.
     *
     *  @remarks This opens a file for both read and write access.  If the file already exists, it
     *           is not truncated.  If the file does not exist, it will be created.  The file
     *           pointer is initially placed at the beginning of the file.  All writes to the file
     *           will occur at the current file pointer location.
     */
    File*(CARB_ABI* openFileToReadWrite)(const char* path);

    /**
     *  retrieves the current status of a file stream object.
     *
     *  @param[in] file     an open file stream to check the status of.
     *  @returns FileStatus::eOk if the file stream is still in a valid state and more read or
     *           write operation may potentially succeed.
     *  @returns FileStatus::eError if the file stream has encountered an error of any kind.
     *           This may include a partial write due to a full disk or a disk quota being
     *           reached.
     *  @returns FileStatus::eEof if a file stream opened for read has already read the last
     *           bytes in the file.  A future call to readFile*() will simply return 0 or
     *           nullptr from the same file position.
     *
     *  @remarks This retrieves the current status of a file stream object.  The status allows
     *           the caller to differentiate an error from an end-of-file condition for the
     *           last file operation.  The error condition on the file will be reset after
     *           each operation after being stored for later retrieval.  The file stream status
     *           value will remain valid until the next operation is performed on the file.
     *
     *  @note    As with all other file operations, retrieving this status is not thread safe
     *           and could change if another thread performs an unprotected operation on the
     *           same stream.  It is the caller's responsibility to ensure operations on the
     *           file stream are appropriately protected.
     *
     *  @note    The file status will not be modified by calls to getFileSize(), getFileModTime(),
     *           flushFile(), or getFilePosition().
     *
     */
    FileStatus(CARB_ABI* getFileStatus)(File* file);

    /**
     * fills the FileInfo struct with info about the given file.
     *
     * @param path The path to the file.
     * @param info The struct populated with info about the file.
     * @return true if information was gathered. false if an error occurs.
     */
    bool(CARB_ABI* getFileInfo)(const char* path, FileInfo* info);

    /**
     * Returns the current time of the file system.
     */
    time_t(CARB_ABI* getCurrentTime)();

    /**
     * Tests whether it's possible to read a file or directory.
     *
     * @param path  The absolute or relative path to test for readability.  Relative paths are
     *              resolved from the current working directory (as returned from the
     *              getCurrentDirectoryPath() function).  This may not be `nullptr` or an
     *              empty string.
     * @returns `true` if the given file or directory exists and is readable by the calling user.
     *          Returns `false` if the file or directory doesn't exist or the user does not have
     *          permission to read from it.  For a directory, readability represents permission
     *          to list the contents of the directory.
     *
     * @note This accessibility check only answers the question of whether the user has
     *       _permission_ to read the file, not that an open for read will always succeed.
     *       At least on Windows, it is still possible that another thread or process could
     *       have the file open without read sharing capabilities.  In this case, the caller
     *       should just do a test open of the file since that will answer the question of
     *       whether read sharing is currently allowed on the file.  On Linux there isn't any
     *       kernel enforced file sharing functionality so permission to the file should also
     *       imply the user will succeed to open it for read.
     */
    bool(CARB_ABI* isReadable)(const char* path);

    /**
     * Use OS specific functions to build canonical path relative to the base root.
     *
     * The path must exist.
     *
     * If returned size is greater than passed bufferSize, then nothing is written to the buffer.
     * If returned size is 0, then canonical path failed to be built or doesn't exist.
     *
     * @param path  The absolute or relative path to canonicalize.
     * @param base  The base path to resolve relative path against.  This can be `nullptr`to use
     *              the working directory (as returned from getCurrentDirectoryPath()) to resolve
     *              the relative path.
     * @param flags Flags to control the behavior of this operation.
     * @param buffer The buffer to write the canonical path to.  This may be `nullptr` if only
     *               the required size of the buffer is needed.
     * @param bufferSize The size of the buffer @p buffer in bytes.
     * @return The number of bytes written to the buffer @p buffer if the buffer is large enough.
     *         If the buffer is not large enough, nothing will be written to the buffer and the
     *         required size of the buffer in bytes will be returned.
     *
     * @note By default, this assumes that the requested file exists on the filesystem.  On
     *       Linux, the existence of the file will still be checked as a side effect of the
     *       operation.  On Windows however, no explicit check for the file existing in the
     *       filesystem will be performed unless the @ref fCanonicalFlagCheckExists is used.
     */
    size_t(CARB_ABI* makeCanonicalPathEx2)(
        const char* path, const char* base, CanonicalFlags flags, char* buffer, size_t bufferSize);
};

inline std::string IFileSystem::makeCanonicalPath(const char* path, const char* base, CanonicalFlags flags)
{
    std::vector<char> buf(1024);
    size_t bytes = this->makeCanonicalPathEx2(path, base, flags, buf.data(), buf.size());
    if (bytes > buf.size())
    {
        buf.resize(bytes);
        bytes = this->makeCanonicalPathEx2(path, base, flags, buf.data(), buf.size());
    }
    buf.resize(bytes);
    return buf.size() ? std::string(buf.data(), buf.size() - 1) : "";
}

inline bool IFileSystem::setFilePositionBegin(File* file)
{
    return setFilePosition(file, 0, FileWhence::eBegin);
}

inline bool IFileSystem::setFilePositionEnd(File* file)
{
    return setFilePosition(file, 0, FileWhence::eEnd);
}

} // namespace filesystem
} // namespace carb
