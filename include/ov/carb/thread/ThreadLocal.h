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
//! @brief Carbonite dynamic thread-local storage implementation.
#pragma once

#include "../Defines.h"

#include <atomic>
#include <type_traits>

#if CARB_POSIX
#    include <pthread.h>
#elif CARB_PLATFORM_WINDOWS
#    include "../CarbWindows.h"
#    include "SharedMutex.h"

#    include <map>
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

namespace carb
{
namespace thread
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail
{

using TlsDestructor = void (*)(void*);

inline std::mutex& tlsMutex()
{
    static std::mutex m;
    return m;
}

#    if CARB_POSIX
class ThreadLocalBase
{
    pthread_key_t m_key;

public:
    ThreadLocalBase(TlsDestructor destructor)
    {
        int res = pthread_key_create(&m_key, destructor);
        CARB_FATAL_UNLESS(res == 0, "pthread_key_create failed: %d/%s", res, strerror(res));
    }
    ~ThreadLocalBase()
    {
        pthread_key_delete(m_key);
    }

    // Not copyable or movable
    CARB_PREVENT_COPY_AND_MOVE(ThreadLocalBase);

    void* get() const
    {
        return pthread_getspecific(m_key);
    }
    void set(void* val) const
    {
        int res = pthread_setspecific(m_key, val);
        CARB_CHECK(res == 0, "pthread_setspecific failed with %d/%s for key %u", res, strerror(res), m_key);
    }
};
#    elif CARB_PLATFORM_WINDOWS
__declspec(selectany) CARBWIN_SRWLOCK mutex = CARBWIN_SRWLOCK_INIT;
__declspec(selectany) bool destructed = false;

class CARB_VIZ ThreadLocalBase
{
    CARB_VIZ DWORD m_key;

    class Destructors
    {
        using DestructorMap = std::map<DWORD, TlsDestructor>;
        DestructorMap m_map;

    public:
        Destructors() = default;
        ~Destructors()
        {
            AcquireSRWLockExclusive((PSRWLOCK)&mutex);
            destructed = true;
            // Destroy the map under the lock
            DestructorMap{}.swap(m_map);
            ReleaseSRWLockExclusive((PSRWLOCK)&mutex);
        }

        void add(DWORD slot, TlsDestructor fn)
        {
            // If there's no destructor, don't do anything
            if (!fn)
                return;

            AcquireSRWLockExclusive((PSRWLOCK)&mutex);
            m_map[slot] = fn;
            ReleaseSRWLockExclusive((PSRWLOCK)&mutex);
        }
        void remove(DWORD slot)
        {
            AcquireSRWLockExclusive((PSRWLOCK)&mutex);
            m_map.erase(slot);
            ReleaseSRWLockExclusive((PSRWLOCK)&mutex);
        }
        void call()
        {
            AcquireSRWLockShared((PSRWLOCK)&mutex);

            // It is possible for atexit destructors to run before other threads call destructors with thread-storage
            // duration.
            if (destructed)
            {
                ReleaseSRWLockShared((PSRWLOCK)&mutex);
                return;
            }

            // This mimics the process of destructors with pthread_key_create which will iterate multiple (up to
            // PTHREAD_DESTRUCTOR_ITERATIONS) times, which is typically 4.
            bool again;
            int iters = 0;
            const int kMaxIters = 4;
            do
            {
                again = false;
                for (auto& pair : m_map)
                {
                    if (void* val = ::TlsGetValue(pair.first))
                    {
                        // Set to nullptr and call destructor
                        ::TlsSetValue(pair.first, nullptr);
                        pair.second(val);
                        again = true;
                    }
                }
            } while (again && ++iters < kMaxIters);
            ReleaseSRWLockShared((PSRWLOCK)&mutex);
        }
    };

    static Destructors& destructors()
    {
        static Destructors d;
        return d;
    }

public:
    ThreadLocalBase(TlsDestructor destructor)
    {
        m_key = ::TlsAlloc();
        CARB_FATAL_UNLESS(m_key != CARBWIN_TLS_OUT_OF_INDEXES, "TlsAlloc() failed: %" PRIu32 "", ::GetLastError());
        destructors().add(m_key, destructor);
    }
    ~ThreadLocalBase()
    {
        destructors().remove(m_key);
        BOOL b = ::TlsFree(m_key);
        CARB_CHECK(!!b);
    }

    // Not copyable or movable
    CARB_PREVENT_COPY_AND_MOVE(ThreadLocalBase);

    void* get() const
    {
        return ::TlsGetValue(m_key);
    }
    void set(void* val) const
    {
        BOOL b = ::TlsSetValue(m_key, val);
        CARB_CHECK(!!b);
    }

    static void callDestructors(HINSTANCE, DWORD fdwReason, PVOID)
    {
        if (fdwReason == CARBWIN_DLL_THREAD_DETACH)
        {
            // Call for current thread
            destructors().call();
        }
    }
};

extern "C"
{
    // Hook the TLS destructors in the CRT
    // see crt/src/vcruntime/tlsdtor.cpp
    using TlsHookFunc = void(__stdcall*)(HINSTANCE, DWORD, PVOID);

    // Reference these so that the linker knows to include them
    extern DWORD _tls_used;
    extern TlsHookFunc __xl_a[], __xl_z[];

#        pragma comment(linker, "/include:pthread_thread_callback")
#        pragma section(".CRT$XLD", long, read)
    // Since this is a header file, the __declspec(selectany) enables weak linking so that the linker will throw away
    // all the duplicates and leave only one instance of pthread_thread_callback in the binary.
    // This is placed into the specific binary section used for TLS destructors
    __declspec(allocate(".CRT$XLD")) __declspec(selectany)
        TlsHookFunc pthread_thread_callback = carb::thread::detail::ThreadLocalBase::callDestructors;
}
#    else
CARB_UNSUPPORTED_PLATFORM();
#    endif

} // namespace detail
#endif

/**
 * Base template. See specializations for documentation.
 */
template <class T,
          bool Trivial = std::is_trivial<T>::value&& std::is_trivially_destructible<T>::value && (sizeof(T) <= sizeof(void*))>
class ThreadLocal
{
};

// Specializations
// Trivial and can fit within a pointer

/**
 * A class for declaring a dynamic thread-local variable (Trivial/Pointer/POD specialization).
 *
 * This is necessary since C++11 the `thread_local` storage class specifier can only be used at namespace scope. There
 * is no way to declare a `thread_local` variable at class scope unless it is static. ThreadLocal is dynamic.
 *
 * @note Most systems have a limit to the number of thread-local variables available. Each instance of ThreadLocal will
 * consume some of that storage. Therefore, if a class contains a non-static ThreadLocal member, each instance of that
 * class will consume another slot. Use sparingly.
 *
 * There are two specializations for ThreadLocal: a trivial version and a non-trivial version. The trivial version is
 * designed for types like plain-old-data that are pointer-sized or less. The non-trivial version supports classes and
 * large types. The heap is used for non-trivial ThreadLocal types, however these are lazy-initialize, so the per-thread
 * memory is only allocated when used for the first time. The type is automatically destructed (and the memory returned
 * to the heap) when a thread exits.
 *
 * On Windows, this class is implemented using [Thread Local Storage
 * APIs](https://docs.microsoft.com/en-us/windows/win32/procthread/thread-local-storage). On Linux, this class is
 * implemented using [pthread_key_t](https://linux.die.net/man/3/pthread_key_create).
 */
template <class T>
class ThreadLocal<T, true> : private detail::ThreadLocalBase
{
    struct Union
    {
        union
        {
            T t;
            void* p;
        };
        Union(void* p_) : p(p_)
        {
        }
        Union(std::nullptr_t, T t_) : t(t_)
        {
        }
    };

public:
    /**
     * Constructor. Allocates a thread-local storage slot from the operating system.
     */
    ThreadLocal() : ThreadLocalBase(nullptr)
    {
    }

    /**
     * Destructor. Returns the previously allocated thread-local storage slot to the operating system.
     */
    ~ThreadLocal() = default;

    /**
     * Returns the specific value of this ThreadLocal variable for this thread.
     *
     * @note If the calling thread has not yet set() a value for this thread-local storage, the value returned will be
     * default-initialized. For POD types this will be initialized to zeros.
     *
     * @returns A value previously set for the calling thread by set(), or a zero-initialized value if set() has not
     * been called by the calling thread.
     */
    T get() const
    {
        Union u(ThreadLocalBase::get());
        return u.t;
    }

    /**
     * Sets the specific value of this ThreadLocal variable for this thread.
     * @param t The specific value to set for this thread.
     */
    void set(T t)
    {
        Union u(nullptr, t);
        ThreadLocalBase::set(u.p);
    }

    /**
     * Alias for get().
     * @returns The value as if by get().
     */
    operator T()
    {
        return get();
    }

    /**
     * Alias for get().
     * @returns The value as if by get().
     */
    operator T() const
    {
        return get();
    }

    /**
     * Assignment operator and alias for set().
     * @returns `*this`
     */
    ThreadLocal& operator=(T t)
    {
        set(t);
        return *this;
    }

    /**
     * For types where `T` is a pointer, allows dereferencing the thread-local value.
     * @returns The value as if by get().
     */
    T operator->() const
    {
        static_assert(std::is_pointer<T>::value, "Requires pointer type");
        return get();
    }

    /**
     * For types where `T` is a pointer, allows dereferencing the thread-local value.
     * @returns The value as if by set().
     */
    auto operator*() const
    {
        static_assert(std::is_pointer<T>::value, "Requires pointer type");
        return *get();
    }

    /**
     * Tests the thread-local value for equality with @p rhs.
     *
     * @param rhs The parameter to compare against.
     * @returns `true` if the value stored for the calling thread (as if by get()) matches @p rhs; `false` otherwise.
     */
    bool operator==(const T& rhs) const
    {
        return get() == rhs;
    }

    /**
     * Tests the thread-local value for inequality with @p rhs.
     *
     * @param rhs The compare to compare against.
     * @returns `true` if the value stored for the calling thread (as if by get()) does not match @p rhs; `false`
     * otherwise.
     */
    bool operator!=(const T& rhs) const
    {
        return get() != rhs;
    }
};

// Non-trivial or needs more than a pointer (uses the heap)

/**
 * A class for declaring a dynamic thread-local variable (Large/Non-trivial specialization).
 *
 * This is necessary since C++11 the `thread_local` storage class specifier can only be used at namespace scope. There
 * is no way to declare a `thread_local` variable at class scope unless it is static. ThreadLocal is dynamic.
 *
 * @note Most systems have a limit to the number of thread-local variables available. Each instance of ThreadLocal will
 * consume some of that storage. Therefore, if a class contains a non-static ThreadLocal member, each instance of that
 * class will consume another slot. Use sparingly.
 *
 * There are two specializations for ThreadLocal: a trivial version and a non-trivial version. The trivial version is
 * designed for types like plain-old-data that are pointer-sized or less. The non-trivial version supports classes and
 * large types. The heap is used for non-trivial ThreadLocal types, however these are lazy-initialize, so the per-thread
 * memory is only allocated when used for the first time. The type is automatically destructed (and the memory returned
 * to the heap) when a thread exits.
 *
 * On Windows, this class is implemented using [Thread Local Storage
 * APIs](https://docs.microsoft.com/en-us/windows/win32/procthread/thread-local-storage). On Linux, this class is
 * implemented using [pthread_key_t](https://linux.die.net/man/3/pthread_key_create).
 */
template <class T>
class ThreadLocal<T, false> : private detail::ThreadLocalBase
{
public:
    /**
     * Constructor. Allocates a thread-local storage slot from the operating system.
     */
    ThreadLocal() : ThreadLocalBase(destructor), m_head(&m_head)
    {
        detail::tlsMutex(); // make sure this is constructed since we'll need it at shutdown
    }

    /**
     * Destructor. Returns the previously allocated thread-local storage slot to the operating system.
     */
    ~ThreadLocal()
    {
        // Delete all instances for threads created by this object
        ListNode n = m_head;
        m_head.next = m_head.prev = _end();
        while (n.next != _end())
        {
            Wrapper* w = reinterpret_cast<Wrapper*>(n.next);
            n.next = n.next->next;
            delete w;
        }
        // It would be very bad if a thread was using this while we're destroying it
        CARB_ASSERT(m_head.next == _end() && m_head.prev == _end());
    }

    /**
     * Returns the specific value of this ThreadLocal variable for this thread.
     *
     * @note If the calling thread has not yet set() a value for this thread-local storage, the value returned will be
     * default-constructed.
     *
     * @returns A value previously set for the calling thread by set(), or a default-constructed value if set() has not
     * been called by the calling thread.
     */
    T& get()
    {
        return *_get();
    }

    /**
     * Returns the specific value of this ThreadLocal variable for this thread.
     *
     * @note If the calling thread has not yet set() a value for this thread-local storage, the value returned will be
     * default-constructed.
     *
     * @returns A value previously set for the calling thread by set(), or a default-constructed value if set() has not
     * been called by the calling thread.
     */
    const T& get() const
    {
        return *_get();
    }

    /**
     * Sets the specific value of this ThreadLocal variable for this thread.
     *
     * @note If this is the first time set() has been called for this thread, the stored value is first default-
     * constructed and then @p t is copy-assigned to the thread-local value.
     * @param t The specific value to set for this thread.
     */
    void set(const T& t)
    {
        *_get() = t;
    }

    /**
     * Sets the specific value of this ThreadLocal variable for this thread.
     *
     * @note If this is the first time set() has been called for this thread, the stored value is first default-
     * constructed and then @p t is move-assigned to the thread-local value.
     * @param t The specific value to set for this thread.
     */
    void set(T&& t)
    {
        *_get() = std::move(t);
    }

    /**
     * Alias for get().
     * @returns The value as if by get().
     */
    operator T()
    {
        return get();
    }

    /**
     * Alias for get().
     * @returns The value as if by get().
     */
    operator T() const
    {
        return get();
    }

    /**
     * Assignment operator and alias for set().
     * @returns `*this`
     */
    ThreadLocal& operator=(const T& rhs)
    {
        set(rhs);
        return *this;
    }

    /**
     * Assignment operator and alias for set().
     * @returns `*this`
     */
    ThreadLocal& operator=(T&& rhs)
    {
        set(std::move(rhs));
        return *this;
    }

    /**
     * Pass-through support for operator->.
     * @returns the value of operator->.
     */
    auto operator->()
    {
        return get().operator->();
    }
    /**
     * Pass-through support for operator->.
     * @returns the value of operator->.
     */
    auto operator->() const
    {
        return get().operator->();
    }
    /**
     * Pass-through support for operator*.
     * @returns the value of operator*.
     */
    auto operator*()
    {
        return get().operator*();
    }
    /**
     * Pass-through support for operator*.
     * @returns the value of operator*.
     */
    auto operator*() const
    {
        return get().operator*();
    }
    /**
     * Pass-through support for operator[].
     * @param u The value to pass to operator[].
     * @returns the value of operator[].
     */
    template <class U>
    auto operator[](const U& u) const
    {
        return get().operator[](u);
    }

    /**
     * Tests the thread-local value for equality with @p rhs.
     *
     * @param rhs The parameter to compare against.
     * @returns `true` if the value stored for the calling thread (as if by get()) matches @p rhs; `false` otherwise.
     */
    bool operator==(const T& rhs) const
    {
        return get() == rhs;
    }

    /**
     * Tests the thread-local value for inequality with @p rhs.
     *
     * @param rhs The compare to compare against.
     * @returns `true` if the value stored for the calling thread (as if by get()) does not match @p rhs; `false`
     * otherwise.
     */
    bool operator!=(const T& rhs) const
    {
        return get() != rhs;
    }

private:
    struct ListNode
    {
        ListNode* next;
        ListNode* prev;
        ListNode() = default;
        ListNode(ListNode* init) : next(init), prev(init)
        {
        }
    };
    struct Wrapper : public ListNode
    {
        T t;
    };

    ListNode m_head;
    ListNode* _tail() const
    {
        return const_cast<ListNode*>(m_head.prev);
    }
    ListNode* _end() const
    {
        return const_cast<ListNode*>(&m_head);
    }

    static void destructor(void* p)
    {
        // Can't use offsetof because of "offsetof within non-standard-layout type 'Wrapper' is undefined"
        Wrapper* w = reinterpret_cast<Wrapper*>(reinterpret_cast<uint8_t*>(p) - size_t(&((Wrapper*)0)->t));
        {
            // Remove from the list
            std::lock_guard<std::mutex> g(detail::tlsMutex());
            w->next->prev = w->prev;
            w->prev->next = w->next;
        }
        delete w;
    }

    T* _get() const
    {
        T* p = reinterpret_cast<T*>(ThreadLocalBase::get());
        return p ? p : _create();
    }
    T* _create() const
    {
        Wrapper* w = new Wrapper;
        // Add to end of list
        {
            std::lock_guard<std::mutex> g(detail::tlsMutex());
            w->next = _end();
            w->prev = _tail();
            w->next->prev = w;
            w->prev->next = w;
        }
        T* p = std::addressof(w->t);
        ThreadLocalBase::set(p);
        return p;
    }
};

} // namespace thread
} // namespace carb
