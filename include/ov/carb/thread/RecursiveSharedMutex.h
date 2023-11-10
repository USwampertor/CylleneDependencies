// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Recursive Shared Mutex implementation.
#pragma once

#include "SharedMutex.h"
#include "ThreadLocal.h"

#include <algorithm>
#include <vector>

namespace carb
{
namespace thread
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS
class recursive_shared_mutex;
namespace detail
{

using LockEntry = std::pair<recursive_shared_mutex*, ptrdiff_t>;
using LockList = std::vector<LockEntry>;

// TL;DR: Gymnastics to get around SIOF (Static Initialization Order Fiasco) with supported compilers
//
// For GCC this is pretty easy. The init_priority attribute allows us to specify a priority value to use for
// initialization order. For recursive_shared_mutex's lockList, we really only care that it's constructed before
// application initializers run.
//
// We have to jump through some hoops here for MSVC since this is a header-only class. MSVC does have pragma init_seg,
// BUT a given translation unit (i.e. cpp files) may have only one. Since this exists as a header-only class and we
// don't want to force linkage of a cpp file specifically for this, we can get around it by injecting our initializer
// function into the appropriate segment for initializer order at link time.
//
// This is a fairly good reference for the various C-Runtime initializer sections:
// https://gist.github.com/vaualbus/622099d88334fbba1d4ae703642c2956
//
// #pragma init_seg(lib) corresponds to section .CRT$XCL (the L seems to indicate `lib`). Ironically, C=compiler,
// L=lib, and U=user are also in alphabetical order and make nice delimiters between .CRT$XCA (__xc_a) and .CRT$XCZ
// (__xc_z).
#    if CARB_COMPILER_MSC

// If we just specified a variable of type carb::thread::ThreadLocal<LockList> (even allocating it into a specific
// custom section) the compiler will still try to instantiate it during the init_seg(user) order. To circumvent this
// behavior, we instead contain this variable inside `DataContainer`, but are careful to have the DataContainer()
// constructor well defined with zero side-effects. This is because constructLockList() will be called first (during the
// compiler's init_seg(lib) initialization order), which will construct the TLS member inside of DataContainer, but the
// DataContainer() constructor for lockListData runs after (during the compiler's init_seg(user) initialization order).

// clang-format off
// (for brevity)
struct DataContainer
{
    struct DummyType { constexpr DummyType() noexcept {} };
    union
    {
        DummyType empty;
        carb::thread::ThreadLocal<LockList> tls;
    };

    constexpr DataContainer() : empty() {}
    ~DataContainer() {}
} __declspec(selectany) lockListData;
// clang-format on

__declspec(selectany) bool constructed{ false };
inline carb::thread::ThreadLocal<LockList>& lockList()
{
    CARB_ASSERT(constructed);
    return lockListData.tls;
}

inline void constructLockList()
{
    // Construct the lock list and then register a function to destroy it at exit time
    CARB_ASSERT(!constructed);
    new (&lockListData.tls) carb::thread::ThreadLocal<LockList>();
    constructed = true;
    ::atexit([] {
        lockList().~ThreadLocal();
        constructed = false;
    });
}

extern "C"
{
    // Declare these so the linker knows to include them
    using CRTConstructor = void(__cdecl*)();
    extern CRTConstructor __xc_a[], __xc_z[];

    // Force the linker to include this symbol
#        pragma comment(linker, "/include:pConstructLockList")

    // Inject a pointer to our constructLockList() function at XCL, the same section that #pragma init_seg(lib) uses
#        pragma section(".CRT$XCL", long, read)
    __declspec(allocate(".CRT$XCL")) __declspec(selectany) CRTConstructor pConstructLockList = constructLockList;
}
#    else
// According to this GCC bug: https://gcc.gnu.org/bugzilla//show_bug.cgi?id=65115
// The default priority if init_priority is not specified is 65535. So we use one value lower than that.
#        define DEFAULT_INIT_PRIORITY (65535)
#        define LIBRARY_INIT_PRIORITY (DEFAULT_INIT_PRIORITY - 1)
struct Constructed
{
    bool constructed;
    constexpr Constructed() : constructed{ true }
    {
    }
    ~Constructed()
    {
        constructed = false;
    }
    explicit operator bool() const
    {
        return constructed;
    }
} constructed CARB_ATTRIBUTE(weak, init_priority(LIBRARY_INIT_PRIORITY));
carb::thread::ThreadLocal<LockList> lockListTls CARB_ATTRIBUTE(weak, init_priority(LIBRARY_INIT_PRIORITY));
inline carb::thread::ThreadLocal<LockList>& lockList()
{
    CARB_ASSERT(constructed);
    return lockListTls;
}
#    endif

} // namespace detail
#endif

/**
 * A recursive shared mutex. Similar to `std::shared_mutex` or carb::thread::shared_mutex, but can be used recursively.
 *
 * This primitive supports lock conversion: If a thread already holds one or more shared locks and attempts to take an
 * exclusive lock, the shared locks are released and the same number of exclusive locks are added. However, this is not
 * done atomically. @see recursive_shared_mutex::lock() for more info.
 *
 * A single thread-local storage entry is used to track the list of recursive_shared_mutex objects that a thread has
 * locked and their recursive lock depth. However, as this is a header-only class, all modules that use this class will
 * allocate their own thread-local storage entry.
 */
class recursive_shared_mutex : private carb::thread::shared_mutex
{
public:
    /**
     * Constructor.
     */
#if !CARB_DEBUG
    constexpr
#endif
        recursive_shared_mutex() = default;

    /**
     * Destructor.
     *
     * Debug builds assert that the mutex is not busy (locked) when destroyed.
     */
    ~recursive_shared_mutex() = default;

    /**
     * Blocks until an exclusive lock can be obtained.
     *
     * When this function returns, the calling thread exclusively owns the mutex. At some later point the calling thread
     * will need to call unlock() to release the exclusive lock.
     *
     * @note If the calling thread has taken shared locks on this mutex, all of the shared locks are converted to
     * exclusive locks.
     *
     * @warning If existing shared locks must be converted to exclusive locks, the mutex must convert these shared locks
     * to exclusive locks. In order to do this, it must first release all shared locks which potentially allows another
     * thread to gain exclusive access and modify the shared resource. Therefore, any time an exclusive lock is taken,
     * assume that the shared resource may have been modified, even if the calling thread held a shared lock before.
     */
    void lock();

    /**
     * Attempts to immediately take an exclusive lock, but will not block if one cannot be obtained.
     *
     * @note If the calling thread has taken shared locks on this mutex, `false` is returned and no attempt to convert
     * the locks is made. If the calling thread already has an exclusive lock on this mutex, `true` is always returned.
     *
     * @returns `true` if an exclusive lock could be obtained, and at some later point unlock() will need to be called
     * to release the lock. If an exclusive lock could not be obtained immediately, `false` is returned.
     */
    bool try_lock();

    /**
     * Releases either a single shared or exclusive lock on this mutex. Synonymous with unlock_shared().
     *
     * @note If the calling thread has recursively locked this mutex, unlock() will need to be called symmetrically for
     * each call to a successful locking function.
     *
     * @warning `std::terminate()` will be called if the calling thread does not have the mutex locked.
     */
    void unlock();

    /**
     * Blocks until a shared lock can be obtained.
     *
     * When this function returns, the calling thread has obtained a shared lock on the resources protected by the
     * mutex. At some later point the calling thread must call unlock_shared() to release the shared lock.
     *
     * @note If the calling thread already owns an exclusive lock, then calling lock_shared() will actually increase the
     * exclusive lock count.
     */
    void lock_shared();

    /**
     * Attempts to immediately take a shared lock, but will not block if one cannot be obtained.
     *
     * @note If the calling thread already owns an exclusive lock, then calling try_lock_shared() will always return
     * `true` and will actually increase the exclusive lock count.
     *
     * @returns `true` if a shared lock could be obtained, and at some later point unlock_shared() will need to be
     * called to release the lock. If a shared lock could not be obtained immediately, `false` is returned.
     */
    bool try_lock_shared();

    /**
     * Releases either a single shared or exclusive lock on this mutex. Synonymous with unlock().
     *
     * @note If the calling thread has recursively locked this mutex, unlock() or unlock_shared() will need to be called
     * symmetrically for each call to a successful locking function.
     *
     * @warning `std::terminate()` will be called if calling thread does not have the mutex locked.
     */
    void unlock_shared();

    /**
     * Returns true if the calling thread owns the lock.
     * @note Use \ref owns_lock_shared() or \ref owns_lock_exclusive() for a more specific check.
     * @returns `true` if the calling thread owns the lock, either exclusively or shared; `false` otherwise.
     */
    bool owns_lock() const;

    /**
     * Returns true if the calling thread owns a shared lock.
     * @returns `true` if the calling thread owns a shared lock; `false` otherwise.
     */
    bool owns_lock_shared() const;

    /**
     * Returns true if the calling thread owns an exclusive lock.
     * @returns `true` if the calling thread owns an exclusive lock; `false` otherwise.
     */
    bool owns_lock_exclusive() const;

private:
    const detail::LockEntry* hasLockEntry() const
    {
        auto& list = detail::lockList().get();
        auto iter = std::find_if(list.begin(), list.end(), [this](detail::LockEntry& e) { return e.first == this; });
        return iter == list.end() ? nullptr : std::addressof(*iter);
    }
    detail::LockEntry& lockEntry()
    {
        auto& list = detail::lockList().get();
        auto iter = std::find_if(list.begin(), list.end(), [this](detail::LockEntry& e) { return e.first == this; });
        if (iter == list.end())
            iter = (list.emplace_back(this, 0), list.end() - 1);
        return *iter;
    }
    void removeLockEntry(detail::LockEntry& e)
    {
        auto& list = detail::lockList().get();
        CARB_ASSERT(std::addressof(e) >= std::addressof(list.front()) && std::addressof(e) <= std::addressof(list.back()));
        e = list.back();
        list.pop_back();
    }
};

// Function implementations
inline void recursive_shared_mutex::lock()
{
    detail::LockEntry& e = lockEntry();
    if (e.second < 0)
    {
        // Already locked exclusively (negative lock count). Increase the negative count.
        --e.second;
    }
    else
    {
        if (e.second > 0)
        {
            // This thread already has shared locks for this lock. We need to convert to exclusive.
            shared_mutex::unlock_shared();
        }
        // Acquire the lock exclusively
        shared_mutex::lock();
        // Now inside the lock
        e.second = -(e.second + 1);
    }
}

inline bool recursive_shared_mutex::try_lock()
{
    detail::LockEntry& e = lockEntry();
    if (e.second < 0)
    {
        // Already locked exclusively (negative lock count). Increase the negative count.
        --e.second;
        return true;
    }
    else if (e.second == 0)
    {
        if (shared_mutex::try_lock())
        {
            // Inside the lock
            e.second = -1;
            return true;
        }
        // Lock failed
        removeLockEntry(e);
    }
    // Either we already have shared locks (that can't be converted to exclusive without releasing the lock and possibly
    // not being able to acquire it again) or the try_lock failed.
    return false;
}

inline void recursive_shared_mutex::unlock()
{
    detail::LockEntry& e = lockEntry();
    CARB_CHECK(e.second != 0);
    if (e.second > 0)
    {
        if (--e.second == 0)
        {
            shared_mutex::unlock_shared();
            removeLockEntry(e);
        }
    }
    else if (e.second < 0)
    {
        if (++e.second == 0)
        {
            shared_mutex::unlock();
            removeLockEntry(e);
        }
    }
    else
    {
        // unlock() without being locked!
        std::terminate();
    }
}

inline void recursive_shared_mutex::lock_shared()
{
    detail::LockEntry& e = lockEntry();
    if (e.second < 0)
    {
        // We already own an exclusive lock, which is stronger than shared. So just increase the exclusive lock.
        --e.second;
    }
    else
    {
        if (e.second == 0)
        {
            shared_mutex::lock_shared();
            // Now inside the lock
        }
        ++e.second;
    }
}

inline bool recursive_shared_mutex::try_lock_shared()
{
    detail::LockEntry& e = lockEntry();
    if (e.second < 0)
    {
        // We already own an exclusive lock, which is stronger than shared. So just increase the exclusive lock.
        --e.second;
        return true;
    }
    else if (e.second == 0 && !shared_mutex::try_lock_shared())
    {
        // Failed to get the shared lock
        removeLockEntry(e);
        return false;
    }
    ++e.second;
    return true;
}

inline void recursive_shared_mutex::unlock_shared()
{
    unlock();
}

inline bool recursive_shared_mutex::owns_lock() const
{
    auto entry = hasLockEntry();
    return entry ? entry->second != 0 : false;
}

inline bool recursive_shared_mutex::owns_lock_exclusive() const
{
    auto entry = hasLockEntry();
    return entry ? entry->second < 0 : false;
}

inline bool recursive_shared_mutex::owns_lock_shared() const
{
    auto entry = hasLockEntry();
    return entry ? entry->second > 0 : false;
}

} // namespace thread
} // namespace carb
