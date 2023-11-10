// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief C++17-compatible Shared Mutex implementation for C++14 or higher.
#pragma once

#include "../Defines.h"

#include <mutex>
#include <shared_mutex>

#if CARB_ASSERT_ENABLED
#    include <algorithm>
#    include <thread>
#    include <vector>
#endif

#if CARB_COMPILER_GNUC && (CARB_TEGRA || CARB_PLATFORM_LINUX)
#    include <pthread.h>
#endif
#if CARB_PLATFORM_WINDOWS
#    include "../CarbWindows.h"
#endif

namespace carb
{
namespace thread
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail
{
#    if CARB_PLATFORM_WINDOWS
class SharedMutexBase
{
protected:
    constexpr SharedMutexBase() noexcept = default;
    ~SharedMutexBase()
    {
        // NOTE: This assert can happen after main() has exited, since ExitProcess() kills all threads before running
        // any destructors. In this case, a thread could have the shared_mutex locked and be terminated, abandoning the
        // shared_mutex but leaving it in a busy state. This is not ideal. There is no easy way to determine if
        // ExitProcess() has been called and the program is in this state. Therefore, if this assert happens after
        // ExitProcess() has been called, ignore this assert.
        CARB_ASSERT(!m_lock.Ptr); // Destroyed while busy
    }

    void lockExclusive()
    {
        AcquireSRWLockExclusive((PSRWLOCK)&m_lock);
    }
    bool tryLockExclusive()
    {
        return !!TryAcquireSRWLockExclusive((PSRWLOCK)&m_lock);
    }
    void unlockExclusive()
    {
        ReleaseSRWLockExclusive((PSRWLOCK)&m_lock);
    }

    void lockShared()
    {
        AcquireSRWLockShared((PSRWLOCK)&m_lock);
    }
    bool tryLockShared()
    {
        return !!TryAcquireSRWLockShared((PSRWLOCK)&m_lock);
    }
    void unlockShared()
    {
        ReleaseSRWLockShared((PSRWLOCK)&m_lock);
    }

private:
    CARBWIN_SRWLOCK m_lock = CARBWIN_SRWLOCK_INIT;
};
#    else
class SharedMutexBase
{
protected:
    constexpr SharedMutexBase() noexcept = default;
    ~SharedMutexBase()
    {
        int result = pthread_rwlock_destroy(&m_lock);
        CARB_UNUSED(result);
        CARB_ASSERT(result == 0); // Destroyed while busy
    }

    void lockExclusive()
    {
        int result = pthread_rwlock_wrlock(&m_lock);
        CARB_CHECK(result == 0);
    }
    bool tryLockExclusive()
    {
        return pthread_rwlock_trywrlock(&m_lock) == 0;
    }
    void unlockExclusive()
    {
        int result = pthread_rwlock_unlock(&m_lock);
        CARB_CHECK(result == 0);
    }

    void lockShared()
    {
        int result = pthread_rwlock_rdlock(&m_lock);
        CARB_CHECK(result == 0);
    }
    bool tryLockShared()
    {
        return pthread_rwlock_tryrdlock(&m_lock) == 0;
    }
    void unlockShared()
    {
        int result = pthread_rwlock_unlock(&m_lock);
        CARB_CHECK(result == 0);
    }

private:
    pthread_rwlock_t m_lock = PTHREAD_RWLOCK_INITIALIZER;
};
#    endif
} // namespace detail
#endif

/**
 * A shared mutex implementation conforming to C++17's
 * [shared_mutex](https://en.cppreference.com/w/cpp/thread/shared_mutex).
 *
 * This implementation is non-recursive. See carb::thread::recursive_shared_mutex if a recursive shared_mutex is
 * desired.
 *
 * @note The underlying implementations are based on [Slim Reader/Writer (SRW)
 * Locks](https://docs.microsoft.com/en-us/windows/win32/sync/slim-reader-writer--srw--locks) for Windows, and [POSIX
 * read/write lock objects](https://linux.die.net/man/3/pthread_rwlock_init) on Linux.
 */
class shared_mutex : private detail::SharedMutexBase
{
public:
    /**
     * Constructor.
     */
#if !CARB_DEBUG
    constexpr
#endif
        shared_mutex() = default;

    /**
     * Destructor.
     *
     * Debug builds assert that the mutex is not busy (locked) when destroyed.
     */
    ~shared_mutex() = default;

    CARB_PREVENT_COPY(shared_mutex);

    /**
     * Blocks until an exclusive lock can be obtained.
     *
     * When this function returns, the calling thread exclusively owns the mutex. At some later point the calling thread
     * will need to call unlock() to release the exclusive lock.
     *
     * @warning Debug builds will assert that the calling thread does not already own the lock.
     */
    void lock();

    /**
     * Attempts to immediately take an exclusive lock, but will not block if one cannot be obtained.
     *
     * @warning Debug builds will assert that the calling thread does not already own the lock.
     * @returns `true` if an exclusive lock could be obtained, and at some later point unlock() will need to be called
     * to release the lock. If an exclusive lock could not be obtained immediately, `false` is returned.
     */
    bool try_lock();

    /**
     * Releases an exclusive lock held by the calling thread.
     *
     * @warning Debug builds will assert that the calling thread owns the lock exclusively.
     */
    void unlock();

    /**
     * Blocks until a shared lock can be obtained.
     *
     * When this function returns, the calling thread has obtained a shared lock on the resources protected by the
     * mutex. At some later point the calling thread must call unlock_shared() to release the shared lock.
     *
     * @warning Debug builds will assert that the calling thread does not already own the lock.
     */
    void lock_shared();

    /**
     * Attempts to immediately take a shared lock, but will not block if one cannot be obtained.
     *
     * @warning Debug builds will assert that the calling thread does not already own the lock.
     * @returns `true` if a shared lock could be obtained, and at some later point unlock_shared() will need to be
     * called to release the lock. If a shared lock could not be obtained immediately, `false` is returned.
     */
    bool try_lock_shared();

    /**
     * Releases a shared lock held by the calling thread.
     *
     * @warning Debug builds will assert that the calling thread owns a shared lock.
     */
    void unlock_shared();

private:
    using Base = detail::SharedMutexBase;
#if CARB_ASSERT_ENABLED
    using LockGuard = std::lock_guard<std::mutex>;
    mutable std::mutex m_ownerLock;
    std::vector<std::thread::id> m_owners;
    void addThread()
    {
        LockGuard g(m_ownerLock);
        m_owners.push_back(std::this_thread::get_id());
    }
    void removeThread()
    {
        LockGuard g(m_ownerLock);
        auto current = std::this_thread::get_id();
        auto iter = std::find(m_owners.begin(), m_owners.end(), current);
        if (iter != m_owners.end())
        {
            *iter = m_owners.back();
            m_owners.pop_back();
            return;
        }
        // Thread not found
        CARB_ASSERT(false);
    }
    void assertNotLockedByMe() const
    {
        LockGuard g(m_ownerLock);
        CARB_ASSERT(std::find(m_owners.begin(), m_owners.end(), std::this_thread::get_id()) == m_owners.end());
    }
#else
    inline void addThread()
    {
    }
    inline void removeThread()
    {
    }
    inline void assertNotLockedByMe() const
    {
    }
#endif
};

inline void shared_mutex::lock()
{
    assertNotLockedByMe();
    Base::lockExclusive();
    addThread();
}

inline bool shared_mutex::try_lock()
{
    assertNotLockedByMe();
    return Base::tryLockExclusive() ? (addThread(), true) : false;
}

inline void shared_mutex::unlock()
{
    removeThread();
    Base::unlockExclusive();
}

inline void shared_mutex::lock_shared()
{
    assertNotLockedByMe();
    Base::lockShared();
    addThread();
}

inline bool shared_mutex::try_lock_shared()
{
    assertNotLockedByMe();
    return Base::tryLockShared() ? (addThread(), true) : false;
}

inline void shared_mutex::unlock_shared()
{
    removeThread();
    Base::unlockShared();
}

/**
 * Alias for `std::shared_lock`.
 */
template <class Mutex>
using shared_lock = ::std::shared_lock<Mutex>;

} // namespace thread
} // namespace carb
