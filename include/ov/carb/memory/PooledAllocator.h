// Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../container/LocklessStack.h"
#include "../thread/Mutex.h"

#include <memory>

// Change this to 1 to enable pooled allocator leak checking. This captures a callstack and puts everything into an
// intrusive list.
#define CARB_POOLEDALLOC_LEAKCHECK 0
#if CARB_POOLEDALLOC_LEAKCHECK
#    include "../extras/Debugging.h"
#    include "../container/IntrusiveList.h"
#endif
#if CARB_DEBUG
#    include "../logging/Log.h"
#endif

namespace carb
{

namespace memory
{

/**
 * PooledAllocator implements the Allocator named requirements. It is thread-safe and (mostly) lockless. The given
 * Allocator type must be thread-safe as well. Memory is never returned to the given Allocator until destruction.
 *
 * @param T The type created by this PooledAllocator
 * @param Allocator The allocator to use for underlying memory allocation. Must be able to allocate many instances
 * contiguously.
 */
template <class T, class Allocator = std::allocator<T>>
class PooledAllocator
{
public:
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using void_pointer = void*;
    using const_void_pointer = const void*;
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    template <class U>
    struct rebind
    {
        using other = PooledAllocator<U>;
    };

    PooledAllocator() : m_emo(ValueInitFirst{}), m_debugName(CARB_PRETTY_FUNCTION)
    {
    }
    ~PooledAllocator()
    {
#if CARB_DEBUG
        // Leak checking
        size_type freeCount = 0;
        m_emo.second.forEach([&freeCount](MemBlock*) { ++freeCount; });
        size_t const totalCount =
            m_bucketCount ? (size_t(1) << (m_bucketCount + kBucketShift)) - (size_t(1) << kBucketShift) : 0;
        size_t const leaks = totalCount - freeCount;
        if (leaks != 0)
        {
            CARB_LOG_ERROR("%s: leaked %zu items", m_debugName, leaks);
        }
#endif

#if CARB_POOLEDALLOC_LEAKCHECK
        m_list.clear();
#endif

        // Deallocate everything
        m_emo.second.popAll();
        for (size_type i = 0; i != m_bucketCount; ++i)
        {
            m_emo.first().deallocate(m_buckets[i], size_t(1) << (i + kBucketShift));
        }
    }

    pointer allocate(size_type n = 1)
    {
        CARB_CHECK(n <= 1); // cannot allocate more than 1 item simultaneously

        MemBlock* p = m_emo.second.pop();
        p = p ? p : _expand();

#if CARB_POOLEDALLOC_LEAKCHECK
        size_t frames = extras::debugBacktrace(0, p->entry.callstack, CARB_COUNTOF(p->entry.callstack));
        memset(p->entry.callstack + frames, 0, sizeof(void*) * (CARB_COUNTOF(p->entry.callstack) - frames));

        new (&p->entry.link) decltype(p->entry.link){};

        std::lock_guard<carb::thread::mutex> g(m_listMutex);
        m_list.push_back(p->entry);
#endif

        return reinterpret_cast<pointer>(p);
    }
    pointer allocate(size_type n, const_void_pointer p)
    {
        pointer mem = p ? pointer(p) : allocate(n);
        return mem;
    }
    void deallocate(pointer p, size_type n = 1)
    {
        CARB_CHECK(n <= 1); // cannot free more than 1 item simultaneously

        MemBlock* mb = reinterpret_cast<MemBlock*>(p);

#if CARB_POOLEDALLOC_LEAKCHECK
        {
            std::lock_guard<carb::thread::mutex> g(m_listMutex);
            m_list.remove(mb->entry);
        }
#endif

        m_emo.second.push(mb);
    }

    size_type max_size() const
    {
        return 1;
    }

private:
    constexpr static size_type kBucketShift = 10; // First bucket contains 1<<10 items
    constexpr static size_type kAlignment = ::carb_max(alignof(T), alignof(carb::container::LocklessStackLink<void*>));

    struct alignas(kAlignment) PoolEntry
    {
        T obj;
#if CARB_POOLEDALLOC_LEAKCHECK
        void* callstack[32];
        carb::container::IntrusiveListLink<PoolEntry> link;
#endif
    };

    struct NontrivialDummyType
    {
        constexpr NontrivialDummyType() noexcept
        {
            // Avoid zero-initialization when value initialized
        }
    };

    struct alignas(kAlignment) MemBlock
    {
        union
        {
            NontrivialDummyType dummy{};
            PoolEntry entry;
            carb::container::LocklessStackLink<MemBlock> m_link;
        };

        ~MemBlock()
        {
        }
    };

#if CARB_POOLEDALLOC_LEAKCHECK
    carb::thread::mutex m_listMutex;
    carb::container::IntrusiveList<PoolEntry, &PoolEntry::link> m_list;
#endif

    // std::allocator<>::rebind<> has been deprecated in C++17 on mac.
    CARB_IGNOREWARNING_GNUC_WITH_PUSH("-Wdeprecated-declarations")
    using BaseAllocator = typename Allocator::template rebind<MemBlock>::other;
    CARB_IGNOREWARNING_GNUC_POP

    MemBlock* _expand()
    {
        std::lock_guard<Lock> g(m_mutex);

        // If we get the lock, first check to see if another thread populated the buckets first
        if (MemBlock* mb = m_emo.second.pop())
        {
            return mb;
        }

        size_t const bucket = m_bucketCount;
        size_t const allocationCount = size_t(1) << (bucket + kBucketShift);

        // Allocate from base. The underlying allocator may throw
        MemBlock* mem = m_emo.first().allocate(allocationCount);
        CARB_FATAL_UNLESS(mem, "PooledAllocator underlying allocation failed: Out of memory");
        // If any further exceptions are thrown, deallocate `mem`.
        CARB_SCOPE_EXCEPT
        {
            m_emo.first().deallocate(mem, allocationCount);
        };

        // Resize the number of buckets. This can throw if make_unique() fails.
        auto newBuckets = std::make_unique<MemBlock*[]>(m_bucketCount + 1);
        if (m_bucketCount++ > 0)
            memcpy(newBuckets.get(), m_buckets.get(), sizeof(MemBlock*) * (m_bucketCount - 1));
        m_buckets = std::move(newBuckets);

        // Populate the new bucket
        // Add entries (after the first) to the free list
        m_emo.second.push(mem + 1, mem + allocationCount);

        m_buckets[bucket] = mem;

        // Return the first entry that we reserved for the caller
        return mem;
    }

    using LocklessStack = carb::container::LocklessStack<MemBlock, &MemBlock::m_link>;
    EmptyMemberPair<BaseAllocator, LocklessStack> m_emo;

    using Lock = carb::thread::mutex;
    Lock m_mutex;
    std::unique_ptr<MemBlock* []> m_buckets {};
    size_t m_bucketCount{ 0 };
    const char* const m_debugName;
};

} // namespace memory
} // namespace carb
