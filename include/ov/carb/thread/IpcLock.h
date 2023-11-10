// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"

#if CARB_PLATFORM_WINDOWS
#    include "../CarbWindows.h"
#elif CARB_POSIX
#    include <sys/stat.h>

#    include <errno.h>
#    include <fcntl.h>
#    include <semaphore.h>
#    include <string.h>
#else
CARB_UNSUPPORTED_PLATFORM();
#endif


namespace carb
{
namespace thread
{

/** defines an implementation of an intra-process lock.  This lock will only be functional within
 *  the threads of the process that creates it.  The lock implementation will always be recursive.
 *  When a lock with the given name is created, it will initially be unlocked.
 */

#if CARB_PLATFORM_WINDOWS
/** defines an implementation of an inter-process lock.  These locks are given a unique name
 *  to allow other processes to open a lock of the same name.  The name may be any ASCII string
 *  that does not contain the slash character ('/').  The name may be limited to an implementation
 *  defined length.  Lock names should be less than 250 characters in general.  The name of the
 *  lock will be removed from the system when all processes that use that name destroy their
 *  objects.  When a lock with the given name is created, it will initially be unlocked.
 */
class IpcLock
{
public:
    IpcLock(const char* name)
    {
        m_mutex = CreateMutexA(nullptr, CARBWIN_FALSE, name);
        CARB_FATAL_UNLESS(m_mutex != nullptr, "CreateMutex() failed: %u", ::GetLastError());
    }

    ~IpcLock()
    {
        CloseHandle(m_mutex);
    }

    void lock()
    {
        WaitForSingleObject(m_mutex, CARBWIN_INFINITE);
    }

    void unlock()
    {
        ReleaseMutex(m_mutex);
    }

    bool try_lock()
    {
        return WaitForSingleObject(m_mutex, 0) == CARBWIN_WAIT_OBJECT_0;
    }

private:
    HANDLE m_mutex;
};

#elif CARB_POSIX
/** defines an implementation of an inter-process lock.  These locks are given a unique name
 *  to allow other processes to open a lock of the same name.  The name may be any string that
 *  does not contain the slash character ('/').  The name may be limited to an implementation
 *  defined length.  Lock names should be less than 250 characters in general.  The name of the
 *  lock will be removed from the system when all processes that use that name destroy their
 *  objects.  When a lock with the given name is created, it will initially be unlocked.
 */
class IpcLock
{
public:
    IpcLock(const char* name)
    {
        // create the name for the semaphore and remove all slashes within it (slashes are not
        // allowed after the first character and the first character must always be a slash).
        snprintf(m_name, CARB_COUNTOF(m_name) - 4, "/%s", name);

        for (size_t i = 1; m_name[i] != 0; i++)
        {
            if (m_name[i] == '/')
                m_name[i] = '_';
        }

        // create the named semaphore.
        m_semaphore = sem_open(m_name, O_CREAT | O_RDWR, 0644, 1);
        CARB_ASSERT(m_semaphore != SEM_FAILED);
    }

    ~IpcLock()
    {
        sem_close(m_semaphore);
        sem_unlink(m_name);
    }

    void lock()
    {
        int ret;


        // keep trying the wait operation as long as we get interrupted by a signal.
        do
        {
            ret = sem_wait(m_semaphore);
            // Oddly enough, on Windows Subsystem for Linux, sem_wait can fail with ETIMEDOUT. Handle that case here
        } while (ret == -1 && (errno == EINTR || errno == ETIMEDOUT));
    }

    void unlock()
    {
        sem_post(m_semaphore);
    }

    bool try_lock()
    {
        // keep trying the wait operation as long as we get interrupted by a signal.
        int ret = CARB_RETRY_EINTR(sem_trywait(m_semaphore));

        // if the lock was acquired, the return value will always be zero.  If if failed either
        // due to a non-signal error or because it would block, 'ret' will be -1.  If the call
        // was valid but would block, 'errno' is set to EAGAIN.
        return ret == 0;
    }

private:
    sem_t* m_semaphore;
    char m_name[NAME_MAX + 10];
};
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

} // namespace thread
} // namespace carb
