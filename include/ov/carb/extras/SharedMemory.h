// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Provides a helper class to manage a block of shared memory.
 */
#pragma once

#include "../Defines.h"

#include "../Framework.h"
#include "../cpp17/Optional.h"
#include "../extras/ScopeExit.h"
#include "../logging/Log.h"
#include "../process/Util.h"
#include "Base64.h"
#include "StringSafe.h"
#include "Unicode.h"

#include <cstddef>
#include <utility>
#if CARB_POSIX
#    include <sys/file.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <sys/syscall.h>
#    include <sys/types.h>

#    include <cerrno>
#    include <fcntl.h>
#    include <semaphore.h>
#    include <unistd.h>
#    if CARB_PLATFORM_MACOS
#        include <sys/posix_shm.h>
#    endif
#elif CARB_PLATFORM_WINDOWS
#    include "../CarbWindows.h"
#endif


/** Namespace for all low level Carbonite functionality. */
namespace carb
{
/** Common namespace for extra helper functions and classes. */
namespace extras
{

#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
namespace detail
{
#    if CARB_POSIX
constexpr int kAllReadWrite = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

inline constexpr const char* getGlobalSemaphoreName()
{
    // Don't change this as it is completely ABI breaking to do so.
    return "/carbonite-sharedmemory";
}

inline void probeSharedMemory()
{
    // Probe with a shm_open() call prior to locking the mutex. If the object compiling this does not link librt.so
    // then an abort can happen below, but while we have the semaphore locked. Since this is a system-wide
    // semaphore, it can leave this code unable to run in the future. Run shm_open here to make sure that it is
    // available; if not, an abort will occur but not while we have the system-wide semaphore locked.
    shm_open("", 0, 0);
}

class NamedSemaphore
{
public:
    NamedSemaphore(const char* name, bool unlinkOnClose = false) : m_name(name), m_unlinkOnClose(unlinkOnClose)
    {
        m_sema = sem_open(name, O_CREAT, carb::extras::detail::kAllReadWrite, 1);
        CARB_FATAL_UNLESS(m_sema, "Failed to create/open shared semaphore {%d/%s}", errno, strerror(errno));
#        if CARB_PLATFORM_LINUX
        // sem_open() is masked by umask(), so force the permissions with chmod().
        // NOTE: This assumes that named semaphores are under /dev/shm and are prefixed with sem. This is not ideal,
        //       but there does not appear to be any means to translate a sem_t* to a file descriptor (for fchmod())
        //       or a path.
        // NOTE: sem_open() is also affected by umask() on mac, but unfortunately semaphores on mac are not backed
        //       by the filesystem and can therefore not have their permissions modified after creation.
        size_t len = m_name.length() + 12;
        char* buf = CARB_STACK_ALLOC(char, len + 1);
        extras::formatString(buf, len + 1, "/dev/shm/sem.%s", name + 1); // Skip leading /
        chmod(buf, detail::kAllReadWrite);
#        endif
    }
    ~NamedSemaphore()
    {
        int result = sem_close(m_sema);
        CARB_ASSERT(result == 0, "Failed to close sema {%d/%s}", errno, strerror(errno));
        CARB_UNUSED(result);
        if (m_unlinkOnClose)
        {
            sem_unlink(m_name.c_str());
        }
    }

    bool try_lock()
    {
        int val = CARB_RETRY_EINTR(sem_trywait(m_sema));
        CARB_FATAL_UNLESS(val == 0 || errno == EAGAIN, "sem_trywait() failed {%d/%s}", errno, strerror(errno));
        return val == 0;
    }

    void lock()
    {
        int result;

#        if CARB_PLATFORM_LINUX
        auto printMessage = [](const char* format, ...) {
            va_list args;
            char buffer[1024];

            va_start(args, format);
            formatStringV(buffer, CARB_COUNTOF(buffer), format, args);
            va_end(args);

            if (g_carbLogFn && g_carbLogLevel <= logging::kLevelWarn)
            {
                CARB_LOG_WARN("%s", buffer);
            }

            else
            {
                fputs(buffer, stderr);
            }
        };
        constexpr int32_t kTimeoutInSeconds = 5;
        struct timespec abstime;
        clock_gettime(CLOCK_REALTIME, &abstime);
        abstime.tv_sec += kTimeoutInSeconds;

        // Since these are global semaphores and a process can crash with them in a bad state, wait for a period of time
        // then log so that we have an entry of what's wrong.
        result = CARB_RETRY_EINTR(sem_timedwait(m_sema, &abstime));
        CARB_FATAL_UNLESS(result == 0 || errno == ETIMEDOUT, "sem_timedwait() failed {%d/%s}", errno, strerror(errno));
        if (result == -1 && errno == ETIMEDOUT)
        {
            printMessage(
                "Waiting on global named semaphore %s has taken more than 5 seconds. It may be in a stuck state. "
                "You may have to delete /dev/shm/sem.%s and restart the application.",
                m_name.c_str(), m_name.c_str() + 1);

            CARB_FATAL_UNLESS(
                CARB_RETRY_EINTR(sem_wait(m_sema)) == 0, "sem_wait() failed {%d/%s}", errno, strerror(errno));
        }
#        elif CARB_PLATFORM_MACOS
        // mac doesn't support sem_timedwait() and doesn't offer any other named semaphore API
        // either.  For now we'll just do a blocking wait to attempt to acquire the semaphore.
        // If needed, we can add support for the brief wait before warning of a potential hang
        // like linux does.  It would go something along these lines:
        //  * spawn a thread or schedule a task to send a SIGUSR2 signal to this thread after
        //    the given timeout.
        //  * start an infinite wait on this thread.
        //  * if SIGUSR2 arrives on this thread and interrupts the infinite wait, print the
        //    hang warning message then drop into another infinite wait like linux does.
        //
        // Spawning a new thread for each wait operation here is likely a little heavy handed
        // though, especially if there ends up being a lot of contention on this semaphore.
        // The alternative would be to have a single shared thread that could handle timeouts
        // for multiple other threads.
        result = CARB_RETRY_EINTR(sem_wait(m_sema)); // CC-641 to add hang detection
        CARB_FATAL_UNLESS(result == 0, "sem_timedwait() failed {%d/%s}", errno, strerror(errno));
#        else
        CARB_UNSUPPORTED_PLATFORM();
#        endif
    }
    void unlock()
    {
        CARB_FATAL_UNLESS(CARB_RETRY_EINTR(sem_post(m_sema)) == 0, "sem_post() failed {%d/%s}", errno, strerror(errno));
    }

    CARB_PREVENT_COPY_AND_MOVE(NamedSemaphore);

private:
    sem_t* m_sema;
    std::string m_name;
    bool m_unlinkOnClose;
};
#    endif
} // namespace detail
#endif

/** A utility helper class to provide shared memory access to one or more processes.  The shared
 *  memory area is named so that it can be opened by another process or component using the same
 *  name.  Once created, views into the shared memory region can be created.  Each successfully
 *  created view will unmap the mapped region once the view object is deleted.  This object and
 *  any created view objects exist independently from each other - the ordering of destruction
 *  of each does not matter.  The shared memory region will exist in the system until the last
 *  reference to it is released through destruction.
 *
 *  A shared memory region must be mapped into a view before it can be accessed in memory.  A
 *  new view can be created with the createView() function as needed.  As long as one SharedMemory
 *  object still references the region, it can still be reopened by another call to open().  Each
 *  view object maps the region to a different location in memory.  The view's mapped address will
 *  remain valid as long as the view object exists.  Successfully creating a new view object
 *  requires that the mapping was successful and that the mapped memory is valid.  The view object
 *  can be wrapped in a smart pointer such as std::unique_ptr<> if needed to manage its lifetime.
 *  A view object should never be copied (byte-wise or otherwise).
 *
 *  A view can also be created starting at non-zero offsets into the shared memory region.  This
 *  is useful for only mapping a small view of a very large shared memory region into the memory
 *  space.  Multiple views of the same shared memory region may be created simultaneously.
 *
 *  When opening a shared memory region, an 'open token' must be acquired first.  This token can
 *  be retrieved from any other shared memory region object that either successfully created or
 *  opened the region using getOpenToken().  This token can then be transmitted to any other
 *  client through some means to be used to open the same region.  The open token data can be
 *  retrieved as a base64 encoded string for transmission to other processes.  The target process
 *  can then create its own local open token from the base64 string using the constructor for
 *  SharedMemory::OpenToken.  Open tokens can also be passed around within the same process or
 *  through an existing shared memory region as needed by copying, moving, or assigning it to
 *  another object.
 *
 *  @note On Linux, this requires that the host app be linked with the "rt" and "pthread" system
 *        libraries.  This can be done by adding 'links { "rt", "pthread" }' to the premake script
 *        or adding the "-lrt -lpthread" option to the build command line.
 */
class SharedMemory
{
public:
    /** An opaque token object used to open an existing SHM region or to retrieve from a newly
     *  created SHM region to pass to another client to open it.  A token is only valid if it
     *  contains data for a shared memory region that is currently open.  It will cause open()
     *  to fail if it is not valid however.
     */
    class OpenToken
    {
    public:
        /** Constructor: initializes an empty token. */
        OpenToken() : m_data(nullptr), m_base64(nullptr), m_size(0)
        {
        }

        /** Constructor: creates a new open token from a base64 encoded string.
         *
         *  @param[in] base64   A base64 encoded data blob containing the open token information.
         *                      This may be received through any means from another token.  This
         *                      string must be null terminated.  This may not be `nullptr` or an
         *                      empty string.
         *
         *  @remarks This will create a new open token from a base64 encoded data blob received
         *           from another open token.  Both the base64 and decoded representations will
         *           be stored.  Only the base64 representation will be able to be retrieved
         *           later.  No validation will be done on the token data until it is passed to
         *           SharedMemory::open().
         */
        OpenToken(const char* base64) : m_data(nullptr), m_base64(nullptr), m_size(0)
        {
            Base64 converter(Base64::Variant::eFilenameSafe);
            size_t size;
            size_t inSize;


            if (base64 == nullptr || base64[0] == 0)
                return;

            inSize = strlen(base64);
            m_base64 = new (std::nothrow) char[inSize + 1];

            if (m_base64 != nullptr)
                memcpy(m_base64, base64, (inSize + 1) * sizeof(char));

            size = converter.getDecodeOutputSize(inSize);
            m_data = new (std::nothrow) uint8_t[size];

            if (m_data != nullptr)
                m_size = converter.decode(base64, inSize, m_data, size);
        }

        /** Copy constructor: copies an open token from another one.
         *
         *  @param[in] token    The open token to be copied.  This does not necessarily need to
         *                      be a valid token.
         */
        OpenToken(const OpenToken& token) : m_data(nullptr), m_base64(nullptr), m_size(0)
        {
            *this = token;
        }

        /** Move constructor: moves an open token from another one to this one.
         *
         *  @param[inout] token The open token to be moved from.  The data in this token will
         *                      be stolen into this object and the source token will be cleared
         *                      out.
         */
        OpenToken(OpenToken&& token) : m_data(nullptr), m_base64(nullptr), m_size(0)
        {
            *this = std::move(token);
        }

        ~OpenToken()
        {
            clear();
        }

        /** Validity check operator.
         *
         *  @returns `true` if this object contains token data.
         *  @returns `false` if this object was not successfully constructed or does not contain
         *           any actual token data.
         */
        explicit operator bool() const
        {
            return isValid();
        }

        /** Validity check operator.
         *
         *  @returns `true` if this object does not contain any token data.
         *  @returns `false` if this object contains token data.
         *
         */
        bool operator!() const
        {
            return !isValid();
        }

        /** Token equality comparison operator.
         *
         *  @param[in] token    The token to compare this one to.
         *  @returns `true` if the other token contains the same data this one does.
         *  @returns `false` if the other token contains different data from this one.
         */
        bool operator==(const OpenToken& token) const
        {
            if (m_size == 0 && token.m_size == 0)
                return true;

            if (m_size != token.m_size)
                return false;

            if (m_data == nullptr || token.m_data == nullptr)
                return false;

            return memcmp(m_data, token.m_data, m_size) == 0;
        }

        /** Token inequality comparison operator.
         *
         *  @param[in] token    The token to compare this one to.
         *  @returns `true` if the other token contains different data from this one.
         *  @returns `false` if the other token contains the same data this one does.
         */
        bool operator!=(const OpenToken& token) const
        {
            return !(*this == token);
        }

        /** Copy assignment operator.
         *
         *  @param[in] token    The token to be copied.
         *  @returns A reference to this object.
         */
        OpenToken& operator=(const OpenToken& token)
        {
            if (this == &token)
                return *this;

            clear();

            if (token.m_data == nullptr)
                return *this;

            m_data = new (std::nothrow) uint8_t[token.m_size];

            if (m_data != nullptr)
            {
                memcpy(m_data, token.m_data, token.m_size);
                m_size = token.m_size;
            }

            return *this;
        }

        /** Move assignment operator.
         *
         *  @param[inout] token     The token to be moved from.  This source object will have its
         *                          data token into this object, and the source will be cleared.
         *  @returns A reference to this object.
         */
        OpenToken& operator=(OpenToken&& token)
        {
            if (this == &token)
                return *this;

            clear();

            m_size = token.m_size;
            m_data = token.m_data;
            m_base64 = token.m_base64;
            token.m_size = 0;
            token.m_data = nullptr;
            token.m_base64 = nullptr;
            return *this;
        }

        /** Retrieves the token data in base64 encoding.
         *
         *  @returns The token data encoded as a base64 string.  This will always be null
         *           terminated.  This token data string will be valid as long as this object
         *           still exists.  If the caller needs the data to persist, the returned
         *           string must be copied.
         *  @returns `nullptr` if this token doesn't contain any data or memory could not be
         *           allocated to hold the base64 encoded string.
         */
        const char* getBase64Token()
        {
            if (m_base64 != nullptr)
                return m_base64;

            if (m_size == 0)
                return nullptr;

            Base64 converter(Base64::Variant::eFilenameSafe);
            size_t size;


            size = converter.getEncodeOutputSize(m_size);
            m_base64 = new (std::nothrow) char[size];

            if (m_base64 == nullptr)
                return nullptr;

            converter.encode(m_data, m_size, m_base64, size);
            return m_base64;
        }

    protected:
        /** Constructor: initializes a new token with a specific data block.
         *
         *  @param[in] tokenData    The data to be copied into the new token.  This may not be
         *                          `nullptr`.
         *  @param[in] size         The size of the @p tokenData block in bytes.
         *  @returns No return value.
         */
        OpenToken(const void* tokenData, size_t size) : m_data(nullptr), m_base64(nullptr), m_size(0)
        {
            if (size == 0)
                return;

            m_data = new (std::nothrow) uint8_t[size];

            if (m_data != nullptr)
            {
                memcpy(m_data, tokenData, size);
                m_size = size;
            }
        }

        /** Tests if this token is valid.
         *
         *  @returns `true` if this token contains data.  Returns `false` otherwise.
         */
        bool isValid() const
        {
            return m_data != nullptr && m_size > 0;
        }

        /** Retrieves the specific token data from this object.
         *
         *  @returns The token data contained in this object.
         */
        uint8_t* getToken() const
        {
            return reinterpret_cast<uint8_t*>(m_data);
        }

        /** Retrieves the size in bytes of the token data in this object.
         *
         *  @returns The size of the token data in bytes.  Returns `0` if this object doesn't
         *           contain any token data.
         */
        size_t getSize() const
        {
            return m_size;
        }

        /** Clears the contents of this token object.
         *
         *  @returns No return value.
         *
         *  @remarks This clears out the contents of this object.  Upon return, the token will
         *           no longer be considered valid and can no longer be used to open an existing
         *           shared memory object.
         */
        void clear()
        {
            if (m_data != nullptr)
                delete[] m_data;

            if (m_base64 != nullptr)
                delete[] m_base64;

            m_size = 0;
            m_data = nullptr;
            m_base64 = nullptr;
        }

        /** The raw binary data for the token.  This will be `nullptr` if the token does not
         *  contain any data.
         */
        uint8_t* m_data;

        /** The base64 encoded version of this token's data.  This will be `nullptr` if no base64
         *  data has been retrieved from this object and it was created from a source other than
         *  a base64 string.
         */
        char* m_base64;

        /** The size of the binary data blob @ref m_data in bytes. */
        size_t m_size;

        friend class SharedMemory;
    };

    /** Flag to indicate that a unique region name should be generated from the given base name
     *  in create().  This will allow the call to be more likely to succeed even if a region
     *  with the same base name already exists in the system.  Note that using this flag will
     *  lead to a slightly longer open token being generated.
     */
    static constexpr uint32_t fCreateMakeUnique = 0x00000001;

    /**
     * Flag to indicate that failure should not be reported as an error log.
     */
    static constexpr uint32_t fQuiet = 0x00000002;

    /**
     * Flag to indicate that no mutexes should be locked during this operation.
     * @warning Use of this flag could have interprocess and thread safety issues! Use with utmost caution!
     * @note Currently only Linux is affected by this flag.
     */
    static constexpr uint32_t fNoMutexLock = 0x00000004;

    /** Names for the different ways a mapping region can be created and accessed. */
    enum class AccessMode
    {
        /** Use the default memory access mode for the mapping.  When this is specified to create
         *  a view, the same access permissions as were used when creating the shared memory
         *  region will be used.  For example, if the SHM region is created as read-only, creating
         *  a view with the default memory access will also be read-only.  The actual access mode
         *  that was granted can be retrieved from the View::getAccessMode() function after the
         *  view is created.
         */
        eDefault,

        /** Open or access the shared memory area as read-only.  This access mode cannot be used
         *  to create a new SHM area.  This can be used to create a view of any shared memory
         *  region however.
         */
        eReadOnly,

        /** Create, open, or access the shared memory area as read-write.  This access mode must
         *  be used when creating a new shared memory region.  It may also be used when opening
         *  a shared memory region, or creating a view of a shared memory region that was opened
         *  as read-write.  If this is used to create a view on a SHM region that was opened as
         *  read-only, the creation will fail.
         */
        eReadWrite,
    };

    /** Constructor: initializes a new shared memory manager object.  This will not point to any
     *               block of memory.
     */
    SharedMemory()
    {
        m_token = nullptr;
        m_access = AccessMode::eDefault;

        // collect the system page size and allocation granularity information.
#if CARB_PLATFORM_WINDOWS
        m_handle.handleWin32 = nullptr;
        CARBWIN_SYSTEM_INFO si;
        GetSystemInfo((LPSYSTEM_INFO)&si);
        m_pageSize = si.dwPageSize;
        m_allocationGranularity = si.dwAllocationGranularity;
#elif CARB_POSIX
        m_handle.handleFd = -1;
        m_refCount = SEM_FAILED;
        m_pageSize = getpagesize();
        m_allocationGranularity = m_pageSize;
#else
        CARB_UNSUPPORTED_PLATFORM();
#endif
    }

    ~SharedMemory()
    {
        close();
    }

    //! Result from createOrOpen().
    enum Result
    {
        eError, //!< An error occurred when attempting to create or open shared memory.
        eCreated, //!< The call to createOrOpen() created the shared memory by name.
        eOpened, //!< The call to createOrOpen() opened an existing shared memory by name.
    };

    /** Creates a new shared memory region.
     *
     *  @param[in] name     The name to give to the new shared memory region.  This may not be
     *                      `nullptr` or an empty string.  This must not contain a slash ('/') or
     *                      backslash ('\') character and must generally consist of filename safe
     *                      ASCII characters.  This will be used as the base name for the shared
     *                      memory region that is created.  This should be shorter than 250
     *                      characters for the most portable behavior.  Longer names are allowed
     *                      but will be silently truncated to a platform specific limit.  This
     *                      truncation may lead to unintentional failures if two region names
     *                      differ only by truncated characters.
     *  @param[in] size     The requested size of the shared memory region in bytes.  This may be
     *                      any size.  This will be silently rounded up to the next system
     *                      supported region size.  Upon creation, getSize() may be used to
     *                      retrieve the actual size of the newly created region.
     *  @param[in] flags    Flags to control behavior of creating the new shared memory region.
     *                      This may be 0 for default behavior, or may be @ref fCreateMakeUnique
     *                      to indicate that the name specified in @p name should be made into a
     *                      more unique string so that creating a new region with the same base
     *                      name as another region is more likely to succeed (note that it may
     *                      still fail for a host of other reasons).  This defaults to
     *                      @ref fCreateMakeUnique.
     *  @returns `true` if the new shared memory region is successfully created.  At this point,
     *           new views to the region may be created with createView().  When this region is
     *           no longer needed, it should be closed by either calling close() or by destroying
     *           this object.  This same region may be opened by another client by retrieving its
     *           open token and passing that to the other client.
     *  @returns `false` if the new shared memory region could not be created as a new region.  This
     *           can include failing because another shared memory region with the same name
     *           already exists or that the name was invalid (ie: contained invalid characters).
     *  @returns `false` if another shared memory region is currently open on this object.  In this
     *           case, close() must be called before attempting to create a new region.
     *
     *  @remarks This creates a new shared memory region in the system.  If successful, this new
     *           region will be guaranteed to be different from all other shared memory regions
     *           in the system.  The new region will always be opened for read and write access.
     *           New views into this shared memory region may be created upon successful creation
     *           using createView().  Any created view object will remain valid even if this
     *           object is closed or destroyed.  Note however that other clients will only be
     *           able to open this same shared memory region if are least one SharedMemory object
     *           still has the region open in any process in the system.  If all references to
     *           the region are closed, all existing views to it will remain valid, but no new
     *           clients will be able to open the same region by name.  This is a useful way to
     *           'lock down' a shared memory region once all expected client(s) have successfully
     *           opened it.
     *
     *  @note On Linux it is possible to have region creation fail if the @ref fCreateMakeUnique
     *        flag is not used because the region objects were leaked on the filesystem by another
     *        potentially crashed process (or the region was never closed).  This is also possible
     *        on Windows, however instead of failing to create the region, an existing region will
     *        be opened instead.  In both cases, this will be detected as a failure since a new
     *        unique region could not be created as expected.  It is best practice to always use
     *        the @ref fCreateMakeUnique flag here.  Similarly, it is always best practice to
     *        close all references to a region after all necessary views have been created if it
     *        is intended to persist for the lifetime of the process.  This will guarantee that
     *        all references to the region get released when the process exits (regardless of
     *        method).
     *
     *  @note On Linux, some malware scanners such as rkhunter check for large shared memory
     *        regions and flag them as potential root kits.  It is best practice to use a
     *        descriptive name for a shared memory region so that they can be easily dismissed
     *        as not a problem in a malware report log.
     */
    bool create(const char* name, size_t size, uint32_t flags = fCreateMakeUnique)
    {
        return createAndOrOpen(name, size, flags, false, true) == eCreated;
    }

    /**
     * Attempts to create a shared memory region, or if it could not be created, open an existing one by the same name.
     *
     * @see create() for more information.
     *
     * @note On Windows, the attempt to create or open is performed atomically within the operating system. On Linux,
     * however, this is not possible. Instead a first attempt is made to create the shared memory region, and if that
     * fails, an attempt is made to open the shared memory region.
     *
     * @note On Windows, if a mapping already exists under the given name and the requested @p size is larger than the
     * size of the existing mapping, the process to open the mapping is aborted and Result::eError is returned. On
     * Linux, if the requested @p size is larger than the size of the existing mapping, the mapping grows to accommodate
     * the requested @p size. It is not possible to grow an existing mapping on Windows.
     *
     * @param name The name of the shared memory region. @see create() for more information.
     * @param size The size of the shared memory region in bytes. @see create() for more information. If the region is
     * opened, this parameter has different behavior between Windows and Linux. See note above.
     * @param flags The flags provided to direct creation/opening of the shared memory region. @see create() for more
     * information. If the region was originally created by passing fCreateMakeUnique to create(), this function will
     * not be able to open the region as the name was decorated with a unique identifier. Instead use open() to open the
     * shared memory region using the OpenToken.
     * @returns A result for the operation.
     */
    Result createOrOpen(const char* name, size_t size, uint32_t flags = 0)
    {
        return createAndOrOpen(name, size, flags, true, true);
    }

    /**
     * Opens a shared memory region by name.
     *
     * @note On Windows, if a mapping already exists under the given name and the requested @p size is larger than the
     * size of the existing mapping, the process to open the mapping is aborted and Result::eError is returned. On
     * Linux, if the requested @p size is larger than the size of the existing mapping, the mapping grows to accommodate
     * the requested @p size. It is not possible to grow an existing mapping on Windows.
     *
     * @param name The name of the shared memory region. @see create() for more information.
     * @param size The required size of the shared memory region. This parameter has different behavior on Windows and
     * Linux. See note above.
     * @param flags The flags provided to direct opening the shared memory region.
     * @returns `true` if the region was found by name and opened successfully; `false` otherwise.
     */
    bool open(const char* name, size_t size, uint32_t flags = 0)
    {
        return createAndOrOpen(name, size, flags, true, false);
    }

    /** Opens a shared memory region by token.
     *
     *  @param[in] openToken    The open token to use when opening the shared memory region.  This
     *                          token contains all the information required to correctly open and
     *                          map the same shared memory region.  This open token is retrieved
     *                          from another SharedMemory object that has the region open.  This
     *                          is retrieved with getOpenToken().  The token may be encoded into
     *                          a base64 string for transmission to another process (ie: over a
     *                          socket, pipe, environment variable, command line, etc).  The
     *                          specific transmission method is left as an exercise for the
     *                          caller.  Once received, a new open token object may be created
     *                          in the target process by passing the base64 string to the
     *                          OpenToken() constructor.
     *  @param[in] access       The access mode to open the shared memory region in.  This will
     *                          default to @ref AccessMode::eReadWrite.
     *  @returns `true` if the shared memory region is successfully opened with the requested
     *           access permissions.  Once successfully opened, new views may be created and the
     *           open token may also be retrieved from here to pass on to yet another client.
     *           Note that at least one SharedMemory object must still have the region open in
     *           order for the region to be opened in another client with the same open token.
     *           This object must either be closed with close() or destroyed in order to ensure
     *           the region is destroyed properly once all views are unmapped.
     *  @returns `false` if the region could not not be opened.  This could include the open token
     *           being invalid or corrupt, the given region no longer being present in the system,
     *           or there being insufficient permission to access it from the calling process.
     */
    bool open(const OpenToken& openToken, AccessMode access = AccessMode::eDefault)
    {
        OpenTokenImpl* token;
        SharedHandle handle;
        std::string mappingName;


        if (m_token != nullptr)
        {
            CARB_LOG_ERROR(
                "the previous SHM region has not been closed yet.  Please close it before opening a new SHM region.");
            return false;
        }

        // not a valid open token => fail.
        if (!openToken.isValid())
            return false;

        token = reinterpret_cast<OpenTokenImpl*>(openToken.getToken());

        // make sure the token information seems valid.
        if (openToken.getSize() < offsetof(OpenTokenImpl, name) + token->nameLength + 1)
            return false;

        if (token->size == 0 || token->size % m_pageSize != 0)
            return false;

        if (access == AccessMode::eDefault)
            access = AccessMode::eReadWrite;

        token = reinterpret_cast<OpenTokenImpl*>(malloc(openToken.getSize()));

        if (token == nullptr)
        {
            CARB_LOG_ERROR("failed to allocate memory for the open token for this SHM region.");
            return false;
        }

        memcpy(token, openToken.getToken(), openToken.getSize());
        mappingName = getPlatformMappingName(token->name);

#if CARB_PLATFORM_WINDOWS
        std::wstring fname = carb::extras::convertUtf8ToWide(mappingName.c_str());


        handle.handleWin32 =
            OpenFileMappingW(getAccessModeFlags(access, FlagType::eFileFlags), CARBWIN_FALSE, fname.c_str());

        if (handle.handleWin32 == nullptr)
        {
            CARB_LOG_ERROR("failed to open a file mapping object with the name '%s' {error = %" PRIu32 "}", token->name,
                           GetLastError());
            free(token);
            return false;
        }
#elif CARB_POSIX
        // create the reference count object.  Note that this must already exist in the system
        // since we are expecting another process to have already created the region.
        if (!initRefCount(token->name, 0, true))
        {
            CARB_LOG_ERROR("failed to create the reference count object with the name '%s'.", token->name);
            free(token);
            return false;
        }

        handle.handleFd = shm_open(mappingName.c_str(), getAccessModeFlags(access, FlagType::eFileFlags), 0);

        // failed to open the SHM region => fail.
        if (handle.handleFd == -1)
        {
            CARB_LOG_ERROR("failed to open or create file mapping object with the name '%s' {errno = %d/%s}",
                           token->name, errno, strerror(errno));
            destroyRefCount(token->name);
            free(token);
            return false;
        }
#else
        CARB_UNSUPPORTED_PLATFORM();
#endif

        m_token = token;
        m_handle = handle;
        m_access = access;

        return true;
    }

    /** Represents a single mapped view into an open shared memory region.  The region will
     *  remain mapped in memory and valid as long as this object exists.  When this view
     *  object is destroyed, the region will be flushed and unmapped.  All view objects are
     *  still valid even after the shared memory object that created them have been closed
     *  or destroyed.
     *
     *  View objects may neither be copied nor manually constructed.  They may however be
     *  moved with either a move constructor or move assignment.  Note that for safety
     *  reasons, the moved view must be move assigned or move constructed immediately at
     *  declaration time.  This eliminates the possibility of an unmapped from being
     *  acquired and prevents the need to check its validity at any given time.  Once
     *  moved, the original view object will no longer be valid and should be immediately
     *  deleted.  The moved view will not be unmapped by deleting the original view object.
     */
    class View
    {
    public:
        /** Move constructor: moves another view object into this one.
         *
         *  @param[in] view     The other object to move into this one.
         */
        View(View&& view)
        {
            m_address = view.m_address;
            m_size = view.m_size;
            m_offset = view.m_offset;
            m_pageOffset = view.m_pageOffset;
            m_access = view.m_access;
            view.init();
        }

        ~View()
        {
            unmap();
        }

        /** Move assignment operator: moves another object into this one.
         *
         *  @param[in] view     The other object to move into this one.
         *  @returns A reference to this object.
         */
        View& operator=(View&& view)
        {
            if (this == &view)
                return *this;

            unmap();
            m_address = view.m_address;
            m_size = view.m_size;
            m_offset = view.m_offset;
            m_pageOffset = view.m_pageOffset;
            m_access = view.m_access;
            view.init();
            return *this;
        }

        /** Retrieves the mapped address of this view.
         *
         *  @returns The address of the mapped region.  This region will extend from this address
         *           through the following getSize() bytes minus 1.  This address will always be
         *           aligned to the system page size.
         */
        void* getAddress()
        {
            return reinterpret_cast<void*>(reinterpret_cast<uint8_t*>(m_address) + m_pageOffset);
        }

        /** Retrieves the size of this mapped view.
         *
         *  @returns The total size in bytes of this mapped view.  This region will extend from
         *           the mapped address returned from getAddress().  This will always be aligned
         *           to the system page size.  This size represents the number of bytes that are
         *           accessible in this view starting at the given offset (getOffset()) in the
         *           original shared memory region.
         */
        size_t getSize() const
        {
            return m_size;
        }

        /** Retrieves the offset of this view into the original mapping object.
         *
         *  @returns The offset of this view in bytes into the original shared memory region.
         *           This will always be aligned to the system page size.  If this is 0, this
         *           indicates that this view starts at the beginning of the shared memory
         *           region that created it.  If this is non-zero, this view only represents
         *           a portion of the original shared memory region.
         */
        size_t getOffset() const
        {
            return m_offset;
        }

        /** Retrieves the access mode that was used to create this view.
         *
         *  @returns The access mode used to create this view.  This will always be a valid memory
         *           access mode and will never be @ref AccessMode::eDefault.  This can be used
         *           to determine what the granted permission to the view is after creation if it
         *           originally asked for the @ref AccessMode::eDefault mode.
         */
        AccessMode getAccessMode() const
        {
            return m_access;
        }

    protected:
        // prevent new empty local declarations from being default constructed so that we don't
        // need to worry about constantly checking for invalid views.
        /** Constructor: protected default constructor.
         *  @remarks This initializes an empty view object.  This is protected to prevent new
         *           empty local declarations from being default constructed so that we don't
         *           need to worry about constantly checking for invalid views.
         */
        View()
        {
            init();
        }

        // remove these constructors and operators to prevent multiple copies of the same view
        // from being created and copied.  Doing so could cause other views to be invalidated
        // unintentionally if a mapping address is reused (which is common).
        View(const View&) = delete;
        View& operator=(const View&) = delete;
        View& operator=(const View*) = delete;

        /** Maps a view of a shared memory region into this object.
         *
         *  @param[in] handle       The handle to the open shared memory region.  This must be a
         *                          valid handle to the region.
         *  @param[in] offset       The offset in bytes into the shared memory region where the
         *                          new mapped view should start.  This must be aligned to the
         *                          system page size.
         *  @param[in] size         The size in bytes of the portion of the shared memory region
         *                          that should be mapped into the view.  This must not extend
         *                          past the end of the original shared memory region once added
         *                          to the offset.  This must be aligned to the system page size.
         *  @param[in] access       The requested memory access mode for the view.  This must not
         *                          be @ref AccessMode::eDefault.  This may not specify greater
         *                          permissions than the shared memory region itself allows (ie:
         *                          requesting read-write on a read-only shared memory region).
         *  @param[in] allocGran    The system allocation granularity in bytes.
         *  @returns `true` if the new view is successfully mapped.
         *  @returns `false` if the new view could not be mapped.
         */
        bool map(SharedHandle handle, size_t offset, size_t size, AccessMode access, size_t allocGran)
        {
            void* mapPtr = nullptr;


#if CARB_PLATFORM_WINDOWS
            size_t granOffset = offset & ~(allocGran - 1);

            m_pageOffset = offset - granOffset;
            mapPtr = MapViewOfFile(handle.handleWin32, getAccessModeFlags(access, FlagType::eFileFlags),
                                   static_cast<DWORD>(granOffset >> 32), static_cast<DWORD>(granOffset),
                                   static_cast<SIZE_T>(size + m_pageOffset));

            if (mapPtr == nullptr)
            {
                CARB_LOG_ERROR(
                    "failed to map %zu bytes from offset %zu {error = %" PRIu32 "}", size, offset, GetLastError());
                return false;
            }
#elif CARB_POSIX
            CARB_UNUSED(allocGran);
            m_pageOffset = 0;
            mapPtr = mmap(
                nullptr, size, getAccessModeFlags(access, FlagType::ePageFlags), MAP_SHARED, handle.handleFd, offset);

            if (mapPtr == MAP_FAILED)
            {
                CARB_LOG_ERROR(
                    "failed to map %zu bytes from offset %zu {errno = %d/%s}", size, offset, errno, strerror(errno));
                return false;
            }
#else
            CARB_UNSUPPORTED_PLATFORM();
#endif

            m_address = mapPtr;
            m_size = size;
            m_offset = offset;
            m_access = access;
            return true;
        }

        /** Unmaps this view from memory.
         *
         *  @returns No return value.
         *
         *  @remarks This unmaps this view from memory.  This should only be called on the
         *           destruction of this object.
         */
        void unmap()
        {
            if (m_address == nullptr)
                return;

#if CARB_PLATFORM_WINDOWS
            if (UnmapViewOfFile(m_address) == CARBWIN_FALSE)
                CARB_LOG_ERROR("failed to unmap the region at %p {error = %" PRIu32 "}", m_address, GetLastError());
#elif CARB_POSIX
            if (munmap(m_address, m_size) == -1)
                CARB_LOG_ERROR("failed to unmap the region at %p {errno = %d/%s}", m_address, errno, strerror(errno));
#else
            CARB_UNSUPPORTED_PLATFORM();
#endif
            init();
        }

        /** Initializes this object to an empty state. */
        void init()
        {
            m_address = nullptr;
            m_size = 0;
            m_offset = 0;
            m_pageOffset = 0;
            m_access = AccessMode::eDefault;
        }

        void* m_address; ///< The mapped address of this view.
        size_t m_size; ///< The size of this view in bytes.
        size_t m_offset; ///< The offset of this view in bytes into the shared memory region.
        size_t m_pageOffset; ///< The page offset in bytes from the start of the mapping.
        AccessMode m_access; ///< The granted access permissions for this view.

        // The SharedMemory object that creates this view needs to be able to call into map().
        friend class SharedMemory;
    };


    /** Creates a new view into this shared memory region.
     *
     *  @param[in] offset   The offset in bytes into the shared memory region where this view
     *                      should start.  This value should be aligned to the system page size.
     *                      If this is not aligned to the system page size, it will be aligned
     *                      to the start of the page that the requested offset is in.  The actual
     *                      mapped offset can be retrieved from the new view object with
     *                      getOffset() if the view is successfully mapped.  This defaults to 0
     *                      bytes into the shared memory region (ie: the start of the region).
     *  @param[in] size     The number of bytes of the shared memory region starting at the
     *                      byte offset specified by @p offset to map into the new view.  This
     *                      should be a multiple of the system page size.  If it is not a
     *                      multiple of the system page size, it will be rounded up to the
     *                      next page size during the mapping.  This size will also be clamped
     *                      to the size of the shared memory region (less the offset).  This
     *                      may be 0 to indicate that the remainder of the region starting at
     *                      the given offset should be mapped into the new view.  This defaults
     *                      to 0.
     *  @param[in] access   The access mode to use for the new view.  This can be set to
     *                      @ref AccessMode::eDefault to use the same permissions as were
     *                      originally used to open the shared memory region.  This will fail
     *                      if the requested mode attempts to grant greater permissions to the
     *                      shared memory region than were used to open it (ie: cannot create a
     *                      read-write view of a read-only shared memory region).  This defaults
     *                      to @ref AccessMode::eDefault.
     *  @returns A new view object representing the mapped view of the shared memory region.
     *           This must be deleted when the mapped region is no longer needed.
     *  @returns `nullptr` if the new view cannot be created or if an invalid set of parameters is
     *           passed in.
     *
     *  @remarks This creates a new view into this shared memory region.  The default behavior
     *           (with no parameters) is to map the entire shared memory region into the new view.
     *           Multiple views into the same shared memory region can be created simultaneously.
     *           It is safe to close the shared memory region or delete the object after a new
     *           view is successfully created.  In this case, the view object and the mapped
     *           memory will still remain valid as long as the view object still exists.  This
     *           shared memory object can similarly also safely close and open a new region
     *           while views on the previous region still exist.  The previous region will only
     *           be completely invalidated once all views are deleted and all other open
     *           references to the region are closed.
     *
     *  @note On Windows, the @p offset parameter should be additionally aligned to the system's
     *        allocation granularity (ie: getSystemAllocationGranularity()).  Under this object
     *        however, this additional alignment requirement is handled internally so that page
     *        alignment is provided to match Linux behavior.  However, this does mean that
     *        additional pages may be mapped into memory that are just transparently skipped by
     *        the view object.  When on Windows, it would be a best practice to align the view
     *        offsets to a multiple of the system allocation granularity (usually 64KB) instead
     *        of just the page size (usually 4KB).  This larger offset granularity behavior
     *        will also work properly on Linux.
     */
    View* createView(size_t offset = 0, size_t size = 0, AccessMode access = AccessMode::eDefault) const
    {
        View* view;


        // no SHM region is open -> nothing to do => fail.
        if (m_token == nullptr)
            return nullptr;

        // the requested offset is beyond the region => fail.
        if (offset >= m_token->size)
            return nullptr;

        if (access == AccessMode::eDefault)
            access = m_access;

        // attempting to map a read/write region on a read-only mapping => fail.
        else if (access == AccessMode::eReadWrite && m_access == AccessMode::eReadOnly)
            return nullptr;

        offset = alignPageFloor(offset);

        if (size == 0)
            size = m_token->size;

        if (offset + size > m_token->size)
            size = m_token->size - offset;

        view = new (std::nothrow) View();

        if (view == nullptr)
            return nullptr;

        if (!view->map(m_handle, offset, size, access, m_allocationGranularity))
        {
            delete view;
            return nullptr;
        }

        return view;
    }

    /** Closes this shared memory region.
     *
     *  @param forceUnlink @b Linux: If `true`, the shared memory region name is disassociated with the currently opened
     *    shared memory. If the shared memory is referenced by other processes (or other SharedMemory objects) it
     *    remains open and valid, but no additional SharedMemory instances will be able to open the same shared memory
     *    region by name. Attempts to open the shared memory by name will fail, and attempts to create the shared memory
     *    by name will create a new shared memory region. If `false`, the shared memory region will be unlinked when the
     *    final SharedMemory object using it has closed. @b Windows: this parameter is ignored.
     *
     *  @remarks This closes the currently open shared memory region on this object.  This will
     *           put the object back into a state where a new region can be opened.  This call
     *           will be ignored if no region is currently open.  This region can be closed even
     *           if views still exist on the current region.  The existing views will not be
     *           closed, invalidated, or unmapped by closing this shared memory region.
     *
     *  @note The current region will be automatically closed when this object is deleted.
     */
    void close(bool forceUnlink = false)
    {
        if (m_token == nullptr)
            return;

#if CARB_PLATFORM_WINDOWS
        CARB_UNUSED(forceUnlink);
        if (m_handle.handleWin32 != nullptr)
            CloseHandle(m_handle.handleWin32);

        m_handle.handleWin32 = nullptr;
#elif CARB_POSIX
        if (m_handle.handleFd != -1)
            ::close(m_handle.handleFd);

        m_handle.handleFd = -1;

        // check that all references to the SHM region have been released before unlinking
        // the named filesystem reference to it.  The reference count semaphore can also
        // be unlinked from the filesystem at this point.
        if (releaseRef() || forceUnlink)
        {
            std::string mappingName = getPlatformMappingName(m_token->name);
            shm_unlink(mappingName.c_str());
            destroyRefCount(m_token->name);
        }

        // close our local reference to the ref count semaphore.
        sem_close(m_refCount);
        m_refCount = SEM_FAILED;
#else
        CARB_UNSUPPORTED_PLATFORM();
#endif
        free(m_token);
        m_token = nullptr;
        m_access = AccessMode::eDefault;
    }

    /**
     * Indicates whether the SharedMemory object is currently open or not.
     *
     * @returns `true` if the SharedMemory object has a region open; `false` otherwise.
     */
    bool isOpen() const
    {
        return m_token != nullptr;
    }

    /** Retrieves the token used to open this same SHM region elsewhere.
     *
     *  @returns The open token object.  This token is valid as long as the SharedMemory object
     *           that returned it has not been closed.  If this object needs to persist, it should
     *           be copied to a caller owned buffer.
     *  @returns `nullptr` if no SHM region is currently open on this object.
     */
    OpenToken getOpenToken()
    {
        if (m_token == nullptr)
            return OpenToken();

        return OpenToken(m_token, offsetof(OpenTokenImpl, name) + m_token->nameLength + 1);
    }

    /** Retrieves the total size of the current shared memory region in bytes.
     *
     *  @returns The size in bytes of the currently open shared memory region.  Note that this
     *           will be aligned to the system page size if the original value passed into the
     *           open() call was not aligned.
     */
    size_t getSize() const
    {
        if (m_token == nullptr)
            return 0;

        return m_token->size;
    }

    /** The maximum access mode allowed for the current shared memory region.
     *
     *  @returns The memory access mode that the current shared memory region was created with.
     *           This will never be @ref AccessMode::eDefault when this SHM region is valid.
     *  @retval AccessMode::eDefault if no SHM region is currently open on this object.
     */
    AccessMode getAccessMode() const
    {
        if (m_token == nullptr)
            return AccessMode::eDefault;

        return m_access;
    }

    /** Retrieves the system memory page size.
     *
     *  @returns The size of the system's memory page size in bytes.
     */
    size_t getSystemPageSize() const
    {
        return m_pageSize;
    }

    /** Retrieves the system allocation granularity.
     *
     *  @returns The system's allocation granularity in bytes.
     */
    size_t getSystemAllocationGranularity() const
    {
        return m_allocationGranularity;
    }

private:
    /** An opaque token containing the information needed to open the SHM region that is created
     *  here.  Note that this structure is tightly packed to avoid having to transfer extra data
     *  between clients.  When a new token is returned, it will not include any extra padding
     *  space at the end of the object.
     */
    CARB_IGNOREWARNING_MSC_WITH_PUSH(4200) // nonstandard extension used: zero-sized array in struct/union
#pragma pack(push, 1)
    struct OpenTokenImpl
    {
        /** The creation time size of the SHM region in bytes.  Note that this is not strictly
         *  needed on Windows since the size of a mapping object can be retrieved at any time
         *  with NtQuerySection().  However, since that is an ntdll level function and has a
         *  slim possibility of changing in a future Windows version, we'll just explicitly
         *  pass over the creation time size when retrieving the token.
         */
        size_t size;

        /** The length of the @a name string in characters.  Note that this is limited to
         *  16 bits (ie: 65535 characters).  This is a system imposed limit on all named
         *  handle objects on Windows and is well beyond the supported length for filenames
         *  on Linux.  This is reduced in range to save space in the size of the tokens
         *  that need to be transmitted to other clients that will open the same region.
         *  Also note that this length is present in the token so that the token's size
         *  can be validated for safety and correctness in open().
         */
        uint16_t nameLength;

        /** The name that was used to create the SHM region that will be opened using this
         *  token.  This will always be null terminated and will contain exactly the number
         *  of characters specified in @a nameLength.
         */
        char name[0];
    };
#pragma pack(pop)
    CARB_IGNOREWARNING_MSC_POP

    /** Flags to indicate which type of flags to return from getAccessModeFlags(). */
    enum class FlagType
    {
        eFileFlags, ///< Retrieve the file mapping flags.
        ePageFlags, ///< Retrieve the page protection flags.
    };

#if CARB_POSIX
    struct SemLockGuard
    {
        sem_t* mutex_;

        SemLockGuard(sem_t* mutex) : mutex_(mutex)
        {
            CARB_FATAL_UNLESS(
                CARB_RETRY_EINTR(sem_wait(mutex_)) == 0, "sem_wait() failed {errno = %d/%s}", errno, strerror(errno));
        }
        ~SemLockGuard()
        {
            CARB_FATAL_UNLESS(
                CARB_RETRY_EINTR(sem_post(mutex_)) == 0, "sem_post() failed {errno = %d/%s}", errno, strerror(errno));
        }
    };
#endif

    size_t alignPageCeiling(size_t size) const
    {
        size_t pageSize = getSystemPageSize();
        return (size + (pageSize - 1)) & ~(pageSize - 1);
    }

    size_t alignPageFloor(size_t size) const
    {
        size_t pageSize = getSystemPageSize();
        return size & ~(pageSize - 1);
    }

    std::string getPlatformMappingName(const char* name, size_t maxLength = 0)
    {
        std::string fname;
#if CARB_PLATFORM_WINDOWS
        const char prefix[] = "Local\\";

        // all named handle objects have a hard undocumented name length limit of 64KB.  This is
        // due to all ntdll strings using a WORD value as the length in the UNICODE_STRING struct
        // that is always used for names at the ntdll level.  Any names that are beyond this
        // limit get silently truncated and used as-is.  This length limit includes the prefix.
        // Note that the name strings must still be null terminated even at the ntdll level.
        if (maxLength == 0)
            maxLength = (64 * 1024);

#elif CARB_POSIX
        const char prefix[] = "/";

        if (maxLength == 0)
        {
#    if CARB_PLATFORM_MACOS
            // Mac OS limits the SHM name to this.
            maxLength = PSHMNAMLEN;
#    else
            // This appears to be the specified length in POSIX.
            maxLength = NAME_MAX;
#    endif
        }
#else
        CARB_UNSUPPORTED_PLATFORM();
#endif
        fname = std::string(prefix) + name;

        if (fname.length() > maxLength)
        {
            fname.erase(fname.begin() + maxLength, fname.end());
        }

        return fname;
    }

    std::string makeUniqueName(const char* name)
    {
        std::string str = name;
        char buffer[256];

        // create a unique name be appending the process ID and a random number to the given name.
        // This should be sufficiently unique for our purposes.  This should only add 3-8 new
        // characters to the name.
        extras::formatString(buffer, CARB_COUNTOF(buffer), "%" OMNI_PRIxpid "-%x", this_process::getId(), rand());

        return std::string(str + buffer);
    }

    static constexpr uint32_t getAccessModeFlags(AccessMode access, FlagType type)
    {
        switch (access)
        {
            default:
            case AccessMode::eDefault:
            case AccessMode::eReadWrite:
#if CARB_PLATFORM_WINDOWS
                return type == FlagType::eFileFlags ? CARBWIN_FILE_MAP_ALL_ACCESS : CARBWIN_PAGE_READWRITE;
#elif CARB_POSIX
                return type == FlagType::eFileFlags ? O_RDWR : (PROT_READ | PROT_WRITE);
#else
                CARB_UNSUPPORTED_PLATFORM();
#endif

            case AccessMode::eReadOnly:
#if CARB_PLATFORM_WINDOWS
                return type == FlagType::eFileFlags ? CARBWIN_FILE_MAP_READ : CARBWIN_PAGE_READONLY;
#elif CARB_POSIX
                return type == FlagType::eFileFlags ? O_RDONLY : PROT_READ;
#else
                CARB_UNSUPPORTED_PLATFORM();
#endif
        }
    }

    Result createAndOrOpen(const char* name, size_t size, uint32_t flags, bool tryOpen, bool tryCreate)
    {
        std::string mappingName;
        std::string rawName;
        size_t extraSize = 0;
        OpenTokenImpl* token;
        SharedHandle handle;
        bool quiet = !!(flags & fQuiet);

        /****** check for bad calls and bad parameters ******/
        if (m_token != nullptr)
        {
            CARB_LOG_WARN(
                "the previous SHM region has not been closed yet.  Please close it before creating a new SHM region.");
            return eError;
        }

        // a valid name is needed => fail.
        if (name == nullptr || name[0] == 0)
            return eError;

        // can't create a zero-sized SHM region => fail.
        if (size == 0)
            return eError;

        // neither create nor open => fail.
        if (!tryOpen && !tryCreate)
            return eError;

        /****** create the named mapping object ******/
        bool const unique = (flags & fCreateMakeUnique) != 0;
        if (unique)
            rawName = makeUniqueName(name);

        else
            rawName = name;

        // get the platform-specific name for the region using the given name as a template.
        mappingName = getPlatformMappingName(rawName.c_str());

        // make sure the mapping size is aligned to the next system page size.
        size = alignPageCeiling(size);

        // create the open token that will be used for other clients.
        extraSize = rawName.length();
        token = reinterpret_cast<OpenTokenImpl*>(malloc(sizeof(OpenTokenImpl) + extraSize + 1));

        if (token == nullptr)
        {
            if (!quiet)
                CARB_LOG_ERROR("failed to create a new open token for the SHM region '%s'.", name);
            return eError;
        }

        // store the token information.
        token->size = size;
        token->nameLength = extraSize & 0xffff;
        memcpy(token->name, rawName.c_str(), sizeof(name[0]) * (token->nameLength + 1));

#if CARB_PLATFORM_WINDOWS
        std::wstring fname = carb::extras::convertUtf8ToWide(mappingName.c_str());

        if (!tryCreate)
            handle.handleWin32 = OpenFileMappingW(CARBWIN_PAGE_READWRITE, CARBWIN_FALSE, fname.c_str());
        else
            handle.handleWin32 =
                CreateFileMappingW(CARBWIN_INVALID_HANDLE_VALUE, nullptr, CARBWIN_PAGE_READWRITE,
                                   static_cast<DWORD>(size >> 32), static_cast<DWORD>(size), fname.c_str());

        // the handle was opened successfully => make sure it didn't open an existing object.
        if (handle.handleWin32 == nullptr || (!tryOpen && (GetLastError() == CARBWIN_ERROR_ALREADY_EXISTS)))
        {
            if (!quiet)
                CARB_LOG_ERROR("failed to create and/or open a file mapping object with the name '%s' {error = %" PRIu32
                               "}",
                               name, GetLastError());
            CloseHandle(handle.handleWin32);
            free(token);
            return eError;
        }
        bool const wasOpened = (GetLastError() == CARBWIN_ERROR_ALREADY_EXISTS);
        if (wasOpened)
        {
            // We need to use an undocumented function (NtQuerySection) to read the size of the mapping object.
            using PNtQuerySection = DWORD(__stdcall*)(HANDLE, int, PVOID, ULONG, PSIZE_T);
            static PNtQuerySection pNtQuerySection =
                (PNtQuerySection)::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "NtQuerySection");
            if (pNtQuerySection)
            {
                struct /*SECTION_BASIC_INFORMATION*/
                {
                    PVOID BaseAddress;
                    ULONG AllocationAttributes;
                    CARBWIN_LARGE_INTEGER MaximumSize;
                } sbi;
                SIZE_T read;
                if (pNtQuerySection(handle.handleWin32, 0 /*SectionBasicInformation*/, &sbi, sizeof(sbi), &read) >= 0)
                {
                    if (size > (size_t)sbi.MaximumSize.QuadPart)
                    {
                        if (!quiet)
                            CARB_LOG_ERROR("mapping with name '%s' was opened but existing size %" PRId64
                                           " is smaller than requested size %zu",
                                           name, sbi.MaximumSize.QuadPart, size);
                        CloseHandle(handle.handleWin32);
                        free(token);
                        return eError;
                    }
                }
            }
        }
#elif CARB_POSIX
        // See the function for an explanation of why this is needed.
        detail::probeSharedMemory();

        // Lock a mutex (named semaphore) while we attempt to initialize the ref-count and shared memory objects. For
        // uniquely-named objects we use a per-process semaphore, but for globally-named objects we use a global mutex.
        cpp17::optional<detail::NamedSemaphore> processMutex;
        cpp17::optional<std::lock_guard<detail::NamedSemaphore>> lock;
        if (!(flags & fNoMutexLock))
        {
            if (unique)
            {
                // Always get the current process ID since we could fork() and our process ID could change.
                // NOTE: Do not change this naming. Though PID is not a great unique identifier, it is considered an
                // ABI break to change this.
                std::string name = detail::getGlobalSemaphoreName();
                name += '-';
                name += std::to_string(this_process::getId());
                processMutex.emplace(name.c_str(), true);
                lock.emplace(processMutex.value());
            }
            else
            {
                lock.emplace(m_systemMutex);
            }
        }

        // create the reference count object.  Note that we don't make sure it's unique in the
        // system because another process may have crashed or leaked it.
        if (!tryCreate || !initRefCount(token->name, O_CREAT | O_EXCL, !tryOpen && !quiet))
        {
            // Couldn't create it exclusively. Something else must have created it, so just try to open existing.
            if (!tryOpen || !initRefCount(token->name, 0, !quiet))
            {
                if (!quiet)
                    CARB_LOG_ERROR(
                        "failed to create/open the reference count object for the new region with the name '%s'.",
                        token->name);
                free(token);
                return eError;
            }
        }

        handle.handleFd =
            tryCreate ? shm_open(mappingName.c_str(), O_RDWR | O_CREAT | O_EXCL, detail::kAllReadWrite) : -1;
        if (handle.handleFd != -1)
        {
            // We created the shared memory region. Since shm_open() is affected by the process umask, use fchmod() to
            // set the file permissions to all users.
            fchmod(handle.handleFd, detail::kAllReadWrite);
        }

        // failed to open the SHM region => fail.
        bool wasOpened = false;
        if (handle.handleFd == -1)
        {
            // Couldn't create exclusively. Perhaps it already exists to open.
            if (tryOpen)
            {
                handle.handleFd = shm_open(mappingName.c_str(), O_RDWR, 0);
            }
            if (handle.handleFd == -1)
            {
                if (!quiet)
                    CARB_LOG_ERROR("failed to create/open SHM region '%s' {errno = %d/%s}", name, errno, strerror(errno));
                destroyRefCount(token->name);
                free(token);
                return eError;
            }
            wasOpened = true;

            // If the region is too small, extend it while we have the semaphore locked.
            struct stat statbuf;
            if (fstat(handle.handleFd, &statbuf) == -1)
            {
                if (!quiet)
                    CARB_LOG_ERROR("failed to stat SHM region '%s' {errno = %d, %s}", name, errno, strerror(errno));
                ::close(handle.handleFd);
                free(token);
                return eError;
            }

            if (size > size_t(statbuf.st_size) && ftruncate(handle.handleFd, size) != 0)
            {
                if (!quiet)
                    CARB_LOG_ERROR("failed to grow the size of the SHM region '%s' from %zu to %zu bytes {errno = %d/%s}",
                                   name, size_t(statbuf.st_size), size, errno, strerror(errno));
                ::close(handle.handleFd);
                free(token);
                return eError;
            }
        }
        // set the size of the region by truncating the file while we have the semaphore locked.
        else if (ftruncate(handle.handleFd, size) != 0)
        {
            if (!quiet)
                CARB_LOG_ERROR("failed to set the size of the SHM region '%s' to %zu bytes {errno = %d/%s}", name, size,
                               errno, strerror(errno));
            ::close(handle.handleFd);
            shm_unlink(mappingName.c_str());
            destroyRefCount(token->name);
            free(token);
            return eError;
        }
#else
        CARB_UNSUPPORTED_PLATFORM();
#endif

        /****** save the values for the SHM region ******/
        m_token = token;
        m_handle = handle;
        m_access = AccessMode::eReadWrite;

        return wasOpened ? eOpened : eCreated;
    }

#if CARB_POSIX
    bool initRefCount(const char* name, int flags, bool logError)
    {
        // build the name for the semaphore.  This needs to start with a slash followed by up to
        // 250 non-slash ASCII characters.  This is the same format as for the SHM region, except
        // for the maximum length.  The name given here will need to be truncated if it is very
        // long.  The system adds "sem." to the name internally which explains why the limit is
        // not NAME_MAX.
        std::string mappingName = getPlatformMappingName(name, NAME_MAX - 4);

        // create the semaphore that will act as the IPC reference count for the SHM region.
        // Note that this will be created with an initial count of 0, not 1.  This is intentional
        // since we want the last reference to fail the wait operation so that we can atomically
        // detect the case where the region's file (and the semaphore's) should be unlinked.
        m_refCount = sem_open(mappingName.c_str(), flags, detail::kAllReadWrite, 0);

        if (m_refCount == SEM_FAILED)
        {
            if (logError)
            {
                CARB_LOG_ERROR("failed to create or open a semaphore named \"%s\" {errno = %d/%s}", mappingName.c_str(),
                               errno, strerror(errno));
            }
            return false;
        }
#    if CARB_PLATFORM_LINUX
        else if (flags & O_CREAT)
        {
            // sem_open() is masked by umask(), so force the permissions with chmod().
            // NOTE: This assumes that named semaphores are under /dev/shm and are prefixed with sem. This is not ideal,
            // but there does not appear to be any means to translate a sem_t* to a file descriptor (for fchmod()) or a
            // path.
            mappingName.replace(0, 1, "/dev/shm/sem.");
            chmod(mappingName.c_str(), detail::kAllReadWrite);
        }
#    endif

        // only add a new reference to the SHM region when opening the region, not when creating
        // it.  This will cause the last reference to fail the wait in releaseRef() to allow us
        // to atomically detect the destruction case.
        if ((flags & O_CREAT) == 0)
            CARB_RETRY_EINTR(sem_post(m_refCount));

        return true;
    }

    bool releaseRef()
    {
        int result;

        // waiting on the semaphore will decrement its count by one.  When it reaches zero, the
        // wait will block.  However since we're doing a 'try wait' here, that will fail with
        // errno set to EAGAIN instead of blocking.
        errno = -1;
        result = CARB_RETRY_EINTR(sem_trywait(m_refCount));
        return (result == -1 && errno == EAGAIN);
    }

    void destroyRefCount(const char* name)
    {
        std::string mappingName;

        mappingName = getPlatformMappingName(name, NAME_MAX - 4);
        sem_unlink(mappingName.c_str());
    }


    /** A semaphore used to handle a reference count to the SHM region itself.  This will allow
     *  the SHM region to only be unlinked from the file system once the reference count has
     *  reached zero.  This will leave the SHM region valid for other clients to open by name
     *  as long as at least one client still has it open.
     *
     *  @note This intentionally isn't used as mutex to protect access to a ref count in shared
     *        memory since the semaphore would be necessary anyway in that situation, along with
     *        wasting a full page of shared memory just to contain the ref count and semaphore
     *        object.  The semaphore itself already provides the required IPC safe counting
     *        mechanism that we need here.
     */
    sem_t* m_refCount;

    /**
     * A semaphore used as a system-wide shared mutex to synchronize the process of creating
     * ref-count objects and creating/opening existing shared memory objects.
     */
    detail::NamedSemaphore m_systemMutex{ detail::getGlobalSemaphoreName() };
#endif

    /** The information block used to open this mapping object.  This can be retrieved to
     *  open the same SHM region elsewhere.
     */
    OpenTokenImpl* m_token;

    /** The locally opened handle to the SHM region. */
    SharedHandle m_handle;

    /** The original access mode used when the SHM region was created or opened.  This will
     *  dictate the maximum allowed access permissions when mapping views of the region.
     */
    AccessMode m_access;

    /** The system page size in bytes.  Note that this unfortunately cannot be a static member
     *  because that would lead to either missing symbol or multiple definition link errors
     *  depending on whether the symbol were also defined outside the class or not.  We'll
     *  just retrieve it on construction instead.
     */
    size_t m_pageSize;

    /** The system allocation granularity in bytes.  Note that this unfortunately cannot be
     *  a static member because that would lead to either missing symbol or multiple
     *  definition link errors depending on whether the symbol were also defined outside
     *  the class or not.  We'll just retrieve it on construction instead.
     */
    size_t m_allocationGranularity;
};

} // namespace extras
} // namespace carb
