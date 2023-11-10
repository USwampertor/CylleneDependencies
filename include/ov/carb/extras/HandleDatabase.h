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
//! @brief HandleDatabase definition file.
#pragma once

#include "../Defines.h"

#include "../container/LocklessStack.h"
#include "../logging/Log.h"
#include "../math/Util.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stack>
#include <vector>

CARB_IGNOREWARNING_MSC_WITH_PUSH(4201) // nonstandard extension used: nameless struct/union

namespace carb
{
namespace extras
{

template <class Mapped, class Handle, class Allocator>
struct HandleRef;

/**
 * Provides an OS-style mapping of a Handle to a Resource.
 *
 * Essentially HandleDatabase is a fast thread-safe reference-counted associative container.
 *
 * @thread_safety Unless otherwise mentioned, HandleDatabase functions may be called simultaneously from multiple
 * threads.
 *
 * A given Handle value is typically never reused, however, it is possible for a handle value to be reused if billions
 * of handles are requested.
 *
 * The HandleDatabase can store at most `(2^31)-1` items, or 2,147,483,647 items simultaneously, provided the memory
 * exists to support it.
 *
 * Implementation Details:
 *
 * HandleDatabase achieves its speed and thread-safety by having a fixed-size base array (`m_database`) where each
 * element is an array of size 2^(base array index). As such, these arrays double the size of the previous index array.
 * These base arrays are never freed, simplifying thread safety and allowing HandleDatabase to be mostly lockless.
 * This array-of-arrays forms a non-contiguous indexable array where the base array and offset can be computed from a
 * 32-bit index value (see `indexToBucketAndOffset`).
 *
 * The Handle is a 64-bit value composed of two 32-bit values. The least-significant 32 bits is the index value into the
 * array-of-arrays described above. The most-significant 32 bits is a lifecycle counter. Every time a new Mapped object
 * is constructed, the lifecycle counter is incremented. This value forms a contract between a handle and the Mapped
 * object at the index indicated by the Handle. If the lifecycle counter doesn't match, the Handle is invalid. As this
 * lifecycle counter is incremented, there is the possibility of rollover after 2^31 handles are generated at a given
 * index. The most significant bit will then be set as a rollover flag so that handleWasValid() continues to operate
 * correctly. The handleWasValid() function returns `true` for any Handle where the lifecycle counter at the given index
 * is greater-than-or-equal to the Handle's lifecycle counter, or if the rollover flag is set.
 *
 * @tparam Mapped The "value" type that is associated with a Handle.
 * @tparam Handle The "key" type. Must be an unsigned integer or pointer type and 64-bit. This is an opaque type that
 * cannot be dereferenced.
 * @tparam Allocator The allocator that will be used to allocate memory. Must be capable of allocating large contiguous
 * blocks of memory.
 */
template <class Mapped, class Handle, class Allocator = std::allocator<Mapped>>
class CARB_VIZ HandleDatabase
{
public:
    //! An alias to Mapped; the mapped value type.
    using MappedType = Mapped;

    //! An alias to Handle; the key type used to associate a mapped value type.
    using HandleType = Handle;

    //! An alias to Allocator; the allocator used to allocate memory.
    using AllocatorType = Allocator;

    //! An alias to the HandleRef for this HandleDatabase.
    using HandleRefType = HandleRef<Mapped, Handle, Allocator>;

    //! @a Deprecated: Use HandleRefType instead.
    using scoped_handle_ref_type = HandleRefType;

    /**
     * Constructor.
     * @thread_safety The constructor must complete in a single thread before any other functions may be called on
     * `*this`.
     */
    constexpr HandleDatabase() noexcept;

    /**
     * Destructor.
     * @thread_safety All other functions on `*this` must be finished before the destructor can be called.
     */
    ~HandleDatabase();

    /**
     * Checks to see if a handle is valid or was ever valid in the past.
     * @param handle The Handle to check.
     * @returns `true` if a @p handle is currently valid or was valid in the past and has since been released; `false`
     * otherwise.
     */
    bool handleWasValid(Handle handle) const;

    /**
     * Creates a new Mapped type with the given arguments.
     * @param args The arguments to pass to the Mapped constructor.
     * @returns A `std::pair` of the Handle that can uniquely identify the new Mapped type, and a pointer to the newly
     * constructed Mapped type.
     */
    template <class... Args>
    std::pair<Handle, Mapped*> createHandle(Args&&... args);

    /**
     * Attempts to find the Mapped type represented by @p handle.
     * @thread_safety Not thread-safe as the Mapped type could be destroyed if another thread calls release().
     * @param handle A handle previously returned from createHandle(). Invalid or previously-valid handles will merely
     * result in a `nullptr` result without an assert or any other side-effects.
     * @returns A pointer to a valid Mapped type if the handle was valid; `nullptr` otherwise.
     */
    Mapped* getValueFromHandle(Handle handle) const;

    /**
     * Retrieves the Handle representing @p mapped.
     * @warning Providing @p mapped that is no longer valid or was not returned from one of the HandleDatabase functions
     * is undefined.
     * @param mapped A Mapped type previously created with createHandle().
     * @returns The Handle representing @p mapped.
     */
    Handle getHandleFromValue(const Mapped* mapped) const;

    /**
     * Atomically attempts to add a reference for the given Handle.
     * @param handle A handle previously returned from createHandle(). Invalid or previously-valid handles will merely
     * result in a `nullptr` result without an assert or any other side-effects.
     * @returns A pointer to a valid Mapped type if a reference could be added; `nullptr` otherwise.
     */
    Mapped* tryAddRef(Handle handle);

    /**
     * Atomically adds a reference for the given Handle; fatal if @p handle is invalid.
     * @param handle A valid handle previously returned from createHandle(). Invalid or previously-valid handles will
     * result in `std::terminate()` being called.
     */
    void addRef(Handle handle);

    /**
     * Atomically adds a reference for the Handle representing @p mapped.
     * @warning Providing @p mapped that is no longer valid or was not returned from one of the HandleDatabase functions
     * is undefined.
     * @param mapped A Mapped type previously created with createHandle().
     */
    void addRef(Mapped* mapped);

    /**
     * Atomically releases a reference for the given Handle, potentially freeing the associated Mapped type.
     * @param handle A valid handle previously returned from createHandle(). Invalid or previously-valid handles will
     * result in an assert in Debug builds, but return `false` with no side effects in Release builds.
     * @returns `true` if the last reference was released and @p handle is no longer valid; `false` if Handle is not
     * valid or is previously-valid or a non-final reference was released.
     */
    bool release(Handle handle);

    /**
     * Atomically releases a reference for the Handle representing @p mapped.
     * @warning Provided @p mapped that is no longer valid or was not returned from one of the HandleDatabase functions
     * is undefined.
     * @param mapped A Mapped type previously created with createHandle().
     * @returns `true` if the last reference was released and @p mapped is no longer valid; `false` if a reference other
     * than the last reference was released.
     */
    bool release(Mapped* mapped);

    /**
     * Atomically releases a reference if and only if it's the last reference.
     * @param handle A valid handle previously returned from createHandle(). Invalid or previously-valid handles will
     * result in an assert in Debug builds, but return `false` with no side effects in Release builds.
     * @returns `true` if the last reference was released and @p handle is no longer valid; `false` otherwise.
     */
    bool releaseIfLastRef(Handle handle);

    /**
     * Atomically releases a reference if and only if it's the last reference.
     * @warning Provided @p mapped that is no longer valid or was not returned from one of the HandleDatabase functions
     * is undefined.
     * @param mapped A Mapped type previously created with createHandle().
     * @returns `true` if the last reference was released and @p mapped is no longer valid; `false` otherwise.
     */
    bool releaseIfLastRef(Mapped* mapped);

    /**
     * Attempts to atomically add a reference to @p handle, and returns a HandleRef to @p handle.
     * @param handle A handle previously returned from createHandle(). Invalid or previously-valid handles will merely
     * result in an empty HandleRef result without an assert or any other side-effects.
     * @returns If tryAddRef() would return a valid Mapped type for @p handle, then a HandleRef that manages the
     * reference is returned; otherwise an empty HandleRef is returned.
     */
    HandleRefType makeScopedRef(Handle handle);

    /**
     * Attempts to atomically add a reference to @p handle, and returns a HandleRef to @p handle.
     * @param handle A handle previously returned from createHandle(). Invalid or previously-valid handles will merely
     * result in an empty HandleRef result without an assert or any other side-effects.
     * @returns If tryAddRef() would return a valid Mapped type for @p handle, then a HandleRef that manages the
     * reference is returned; otherwise an empty HandleRef is returned.
     */
    const HandleRefType makeScopedRef(Handle handle) const;

    /**
     * Calls the provided `Func` invocable object for each valid handle and its associated mapped type.
     * @thread_safety This function is not safe to call if any other thread would be calling releaseIfLastRef() or
     * release() to release the last reference of a Handle. Other threads calling createHandle() when this function is
     * called may result in handles being skipped.
     * @param f An invocable object with signature `void(Handle, Mapped*)`.
     */
    template <class Func>
    void forEachHandle(Func&& f) const;

    /**
     * Iterates over all valid handles and sets their reference counts to zero, destroying the associated mapped type.
     * @thread_safety This function is NOT safe to call if any other thread is calling ANY other HandleDatabase function
     * except for clear() or handleWasValid().
     * @returns The number of handles that were released to zero.
     */
    size_t clear();

private:
    CARB_VIZ static constexpr uint32_t kBuckets = sizeof(uint32_t) * 8 - 1; ///< Number of buckets
    static constexpr uint32_t kMaxSize = (1U << kBuckets) - 1; ///< Maximum size ever

    // NOTE: Making *any* change to how rollover works should enable HANDLE_DATABASE_EXTENDED_TESTS in TestExtras.cpp
    static constexpr uint32_t kRolloverFlag = 0x80000000u;
    static constexpr uint32_t kLifecycleMask = 0x7fffffffu;

    // A struct for mapping type Handle into its basic components of index and lifecycle
    struct HandleUnion
    {
        union
        {
            Handle handle;
            struct
            {
                uint32_t index;
                uint32_t lifecycle;
            };
        };

        constexpr HandleUnion(Handle h) noexcept : handle(h)
        {
        }
        constexpr HandleUnion(uint32_t idx, uint32_t life) noexcept : index(idx), lifecycle(life & kLifecycleMask)
        {
        }
    };

    // A struct for tracking the refcount and lifecycle.
    struct alignas(uint64_t) Metadata
    {
        uint32_t refCount{ 0 };
        CARB_VIZ uint32_t lifecycle{ 0 };

        constexpr Metadata() noexcept = default;
    };

    struct HandleData
    {
        // This union will either be the free link and index, or the constructed mapped type
        union
        {
            struct
            {
                container::LocklessStackLink<HandleData> link;
                uint32_t index;
            };
            // This `val` is only constructed when the HandleData is in use
            Mapped val;
        };

        // Metadata, but the union allows us to access the refCount atomically, separately.
        union
        {
            CARB_VIZ std::atomic<Metadata> metadata;
            std::atomic_uint32_t refCount; // Must share memory address with metadata.refCount
        };

        constexpr HandleData(uint32_t idx) noexcept : index(idx), metadata{}
        {
        }

        // Empty destructor, required because of the union with non-trivial types
        ~HandleData() noexcept
        {
        }
    };
    static_assert(alignof(HandleData) >= alignof(Mapped),
                  "Invalid assumption: HandleData alignment should meet or exceed Mapped alignment");
    static_assert(sizeof(Handle) == sizeof(uint64_t), "Handle should be 64-bit");

    using HandleDataAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<HandleData>;
    using AllocatorTraits = std::allocator_traits<HandleDataAllocator>;

    static void indexToBucketAndOffset(uint32_t index, uint32_t& bucket, uint32_t& offset);
    HandleData* expandDatabase();
    HandleData* getHandleData(uint32_t index) const;
    HandleData* getHandleData(const Mapped* mapped) const;

    HandleData* getDbIndex(size_t index, std::memory_order = std::memory_order_seq_cst) const;

    // m_database is fixed size and written atomically in expandDatabase(). It never contracts and therefore
    // does not require a mutex
    CARB_VIZ std::atomic<HandleData*> m_database[kBuckets] = {};
    struct Members : HandleDataAllocator
    {
        container::LocklessStack<HandleData, &HandleData::link> m_free;
    } m_emo;
};

/**
 * A smart-reference class for a Handle associated with a HandleDatabase.
 *
 * When HandleRef is destroyed, the associated handle is released with the HandleDatabase.
 */
template <class Mapped, class Handle, class Allocator = std::allocator<Mapped>>
struct HandleRef
{
public:
    //! An alias to Mapped; the mapped value type.
    using MappedType = Mapped;

    //! An alias to Handle; the key type used to associate a mapped value type.
    using HandleType = Handle;

    //! An alias to Allocator; the allocator used to allocate memory.
    using AllocatorType = Allocator;

    //! The type specification for the associated HandleDatabase.
    using Database = HandleDatabase<Mapped, Handle, Allocator>;

    //! @a Deprecated: Use MappedType instead.
    using element_type = MappedType;
    //! @a Deprecated: Use HandleType instead.
    using handle_type = HandleType;

    /**
     * Default constructor. Produces an empty HandleRef.
     */
    HandleRef() = default;

    /**
     * Attempts to atomically reference a Handle from the given @p database.
     * @param database The HandleDatabase containing @p handle.
     * @param handle The handle previously returned from HandleDatabase::createHandle(). Providing an invalid or
     * previously-valid handle results in an empty HandleRef.
     */
    HandleRef(Database& database, Handle handle);

    /**
     * Destructor. If `*this` is non-empty, releases the associated Handle with the associated HandleDatabase.
     */
    ~HandleRef();

    /**
     * Move-constructor. Post-condition: @p other is an empty HandleRef.
     * @param other The other HandleRef to move a reference from.
     */
    HandleRef(HandleRef&& other);

    /**
     * Move-assign operator. Swaps state with @p other.
     * @param other The other HandleRef to move a reference from.
     */
    HandleRef& operator=(HandleRef&& other);

    // Specifically non-Copyable in order to reduce implicit usage.
    // Use the explicit `clone()` function if a copy is desired
    CARB_PREVENT_COPY(HandleRef);

    /**
     * Explicitly adds a reference to any referenced handle and returns a HandleRef.
     * @returns A HandleRef holding its own reference for the contained handle. May be empty if `*this` is empty.
     */
    HandleRef clone() const;

    /**
     * Dereferences and returns a pointer to the mapped type.
     * @warning Undefined behavior if `*this` is empty.
     * @returns A pointer to the mapped type.
     */
    Mapped* operator->();

    /**
     * Dereferences and returns a const pointer to the mapped type.
     * @warning Undefined behavior if `*this` is empty.
     * @returns A const pointer to the mapped type.
     */
    const Mapped* operator->() const;

    /**
     * Dereferences and returns a reference to the mapped type.
     * @warning Undefined behavior if `*this` is empty.
     * @returns A reference to the mapped type.
     */
    Mapped& operator*();

    /**
     * Dereferences and returns a const reference to the mapped type.
     * @warning Undefined behavior if `*this` is empty.
     * @returns A const reference to the mapped type.
     */
    const Mapped& operator*() const;

    /**
     * Returns a pointer to the mapped type, or `nullptr` if empty.
     * @returns A pointer to the mapped type or `nullptr` if empty.
     */
    Mapped* get();

    /**
     * Returns a const pointer to the mapped type, or `nullptr` if empty.
     * @returns A const pointer to the mapped type or `nullptr` if empty.
     */
    const Mapped* get() const;

    /**
     * Returns the Handle referenced by `*this`.
     * @returns The Handle referenced by `*this`.
     */
    Handle handle() const;

    /**
     * Tests if `*this` contains a valid reference.
     * @returns `true` if `*this` maintains a valid reference; `false` if `*this` is empty.
     */
    explicit operator bool() const;

    /**
     * Releases any associated reference and resets `*this` to empty.
     */
    void reset();

    /**
     * Swaps state with another HandleRef.
     */
    void swap(HandleRef& rhs);

private:
    Database* m_database{};
    Handle m_handle{};
    Mapped* m_mapped{};
};

/**
 * @a Deprecated: Use HandleRef instead.
 */
template <class Mapped, class Handle, class Allocator = std::allocator<Mapped>>
using ScopedHandleRef = HandleRef<Mapped, Handle, Allocator>;

template <class Mapped, class Handle, class Allocator>
inline constexpr HandleDatabase<Mapped, Handle, Allocator>::HandleDatabase() noexcept
{
}

template <class Mapped, class Handle, class Allocator>
inline HandleDatabase<Mapped, Handle, Allocator>::~HandleDatabase()
{
    // Make sure everything is removed from the free list before we destroy it
    m_emo.m_free.popAll();

    size_t leaks = 0;
    for (size_t i = 0; i < CARB_COUNTOF(m_database); i++)
    {
        HandleData* handleData = m_database[i].exchange(nullptr, std::memory_order_relaxed);
        if (!handleData)
            break;

        // Go through each entry and destruct any outstanding ones with a non-zero refcount
        size_t const kBucketSize = size_t(1) << i;
        for (size_t j = 0; j != kBucketSize; ++j)
        {
            Metadata meta = handleData[j].metadata.load(std::memory_order_relaxed);
            if (meta.refCount != 0)
            {
                handleData[j].val.~Mapped();
                ++leaks;
            }
            AllocatorTraits::destroy(m_emo, handleData + j);
        }

        AllocatorTraits::deallocate(m_emo, handleData, kBucketSize);
    }

    if (leaks != 0)
    {
        CARB_LOG_WARN("%s: had %zu outstanding handle(s) at shutdown", CARB_PRETTY_FUNCTION, leaks);
    }
}

template <class Mapped, class Handle, class Allocator>
inline auto HandleDatabase<Mapped, Handle, Allocator>::getHandleData(uint32_t index) const -> HandleData*
{
    uint32_t bucketIndex, offsetInBucket;
    indexToBucketAndOffset(index, bucketIndex, offsetInBucket);

    if (bucketIndex >= CARB_COUNTOF(m_database))
    {
        return nullptr;
    }

    auto bucket = getDbIndex(bucketIndex, std::memory_order_acquire);
    return bucket ? bucket + offsetInBucket : nullptr;
}

template <class Mapped, class Handle, class Allocator>
inline auto HandleDatabase<Mapped, Handle, Allocator>::getHandleData(const Mapped* mapped) const -> HandleData*
{
    // HandleData must have val as the first member. This would prevent us from casting
    // mapped to HandleData without using ugly pointer math otherwise.
    // static assert using offsetof() is not possible due to HandleData having a non-standard-layout type.
    auto pHandleData = reinterpret_cast<HandleData*>(const_cast<Mapped*>(mapped));
    CARB_ASSERT(reinterpret_cast<intptr_t>(&pHandleData->val) - reinterpret_cast<intptr_t>(pHandleData) == 0);
    return pHandleData;
}

template <class Mapped, class Handle, class Allocator>
inline auto HandleDatabase<Mapped, Handle, Allocator>::getDbIndex(size_t index, std::memory_order order) const
    -> HandleData*
{
    static HandleData* const kLocked = reinterpret_cast<HandleData*>(size_t(-1));
    CARB_ASSERT(index < kBuckets);

    // Spin briefly if the entry is locked
    HandleData* bucket = m_database[index].load(order);
    while (CARB_UNLIKELY(bucket == kLocked))
    {
        CARB_HARDWARE_PAUSE();
        bucket = m_database[index].load(order);
    }
    return bucket;
}

template <class Mapped, class Handle, class Allocator>
inline bool HandleDatabase<Mapped, Handle, Allocator>::handleWasValid(Handle handle) const
{
    HandleUnion hu(handle);
    HandleData* pHandleData = getHandleData(hu.index);

    if (pHandleData == nullptr)
    {
        return false;
    }

    Metadata meta = pHandleData->metadata.load(std::memory_order_acquire);

    // The zero lifecycle count isn't used
    if (hu.lifecycle == 0 || meta.lifecycle == 0)
    {
        return false;
    }

    // The kRolloverFlag value is always unset in hu, but possibly present in meta.lifecycle. However, this is still a
    // valid comparison because if the kRolloverFlag is set in meta.lifecycle, it means that every possible value has
    // been a valid handle at one point and the expression will always result in `true`.
    return hu.lifecycle <= meta.lifecycle;
}

template <class Mapped, class Handle, class Allocator>
template <class... Args>
inline std::pair<Handle, Mapped*> HandleDatabase<Mapped, Handle, Allocator>::createHandle(Args&&... args)
{
    HandleData* handleData = m_emo.m_free.pop();
    if (!handleData)
        handleData = expandDatabase();

    CARB_ASSERT(handleData);

    Metadata meta = handleData->metadata.load(std::memory_order_acquire);
    CARB_ASSERT(((void*)std::addressof(handleData->metadata) == (void*)std::addressof(handleData->refCount)) &&
                offsetof(Metadata, refCount) == 0);

    meta.refCount = 1;

    // Don't allow the 0 lifecycle.
    meta.lifecycle++;
    if ((meta.lifecycle & kLifecycleMask) == 0)
    {
        meta.lifecycle = 1 | kRolloverFlag;
    }

    uint32_t indexToAllocateFrom = handleData->index;
    Mapped* p = new (std::addressof(handleData->val)) Mapped(std::forward<Args>(args)...);
    handleData->metadata.store(meta, std::memory_order_release);

    HandleUnion hu(indexToAllocateFrom, meta.lifecycle);
    return std::make_pair(hu.handle, p);
}

template <class Mapped, class Handle, class Allocator>
inline Mapped* HandleDatabase<Mapped, Handle, Allocator>::tryAddRef(Handle handle)
{
    HandleUnion hu(handle);

    HandleData* pHandleData = getHandleData(hu.index);

    if (pHandleData == nullptr)
    {
        return nullptr;
    }

    Metadata meta = pHandleData->metadata.load(std::memory_order_acquire);
    for (;;)
    {
        if (meta.refCount == 0 || (meta.lifecycle & kLifecycleMask) != hu.lifecycle)
        {
            return nullptr;
        }

        Metadata desired = meta;
        desired.refCount++;
        if (pHandleData->metadata.compare_exchange_weak(
                meta, desired, std::memory_order_release, std::memory_order_relaxed))
        {
            return std::addressof(pHandleData->val);
        }
    }
}

template <class Mapped, class Handle, class Allocator>
inline void HandleDatabase<Mapped, Handle, Allocator>::addRef(Handle handle)
{
    Mapped* mapped = tryAddRef(handle);
    CARB_FATAL_UNLESS(mapped != nullptr, "Attempt to addRef released handle");
}

template <class Mapped, class Handle, class Allocator>
inline void HandleDatabase<Mapped, Handle, Allocator>::addRef(Mapped* mapped)
{
    HandleData* pHandleData = getHandleData(mapped);

    // We're working with the Mapped type here, so if it's not valid we're in UB.
    uint32_t prev = pHandleData->refCount.fetch_add(1, std::memory_order_relaxed);
    CARB_CHECK((prev + 1) > 1); // no resurrection and no rollover
}

template <class Mapped, class Handle, class Allocator>
inline bool HandleDatabase<Mapped, Handle, Allocator>::release(Handle handle)
{
    HandleUnion hu(handle);

    HandleData* pHandleData = getHandleData(hu.index);
    CARB_ASSERT(pHandleData);

    if (pHandleData == nullptr)
    {
        return false;
    }

    Metadata meta = pHandleData->metadata.load(std::memory_order_acquire);

    for (;;)
    {
        if (meta.refCount == 0 || (meta.lifecycle & kLifecycleMask) != hu.lifecycle)
        {
            CARB_ASSERT(0);
            return false;
        }
        Metadata desired = meta;
        bool released = --desired.refCount == 0;
        if (CARB_LIKELY(pHandleData->metadata.compare_exchange_weak(
                meta, desired, std::memory_order_release, std::memory_order_relaxed)))
        {
            if (released)
            {
                std::atomic_thread_fence(std::memory_order_acquire);
                pHandleData->val.~Mapped();
                pHandleData->index = hu.index;
                m_emo.m_free.push(pHandleData);
            }
            return released;
        }
    }
}

template <class Mapped, class Handle, class Allocator>
inline bool HandleDatabase<Mapped, Handle, Allocator>::release(Mapped* mapped)
{
    HandleData* pHandleData = getHandleData(mapped);

    // We're working with the Mapped type here, so if it's not valid we're in UB.
    uint32_t prev = pHandleData->refCount.fetch_sub(1, std::memory_order_release);
    CARB_CHECK(prev >= 1); // No underflow
    if (prev == 1)
    {
        // Released
        std::atomic_thread_fence(std::memory_order_acquire);
        pHandleData->val.~Mapped();
        pHandleData->index = HandleUnion(getHandleFromValue(mapped)).index;
        m_emo.m_free.push(pHandleData);
        return true;
    }
    return false;
}

template <class Mapped, class Handle, class Allocator>
inline bool HandleDatabase<Mapped, Handle, Allocator>::releaseIfLastRef(Handle handle)
{
    HandleUnion hu(handle);
    HandleData* pHandleData = getHandleData(hu.index);
    CARB_ASSERT(pHandleData);

    if (pHandleData == nullptr)
    {
        return false;
    }

    Metadata meta = pHandleData->metadata.load(std::memory_order_acquire);

    for (;;)
    {
        if (meta.refCount == 0 || (meta.lifecycle & kLifecycleMask) != hu.lifecycle)
        {
            CARB_ASSERT(0);
            return false;
        }
        if (meta.refCount > 1)
        {
            return false;
        }
        Metadata desired = meta;
        desired.refCount = 0;
        if (CARB_LIKELY(pHandleData->metadata.compare_exchange_weak(
                meta, desired, std::memory_order_release, std::memory_order_relaxed)))
        {
            std::atomic_thread_fence(std::memory_order_acquire);
            pHandleData->val.~Mapped();
            pHandleData->index = hu.index;
            m_emo.m_free.push(pHandleData);
            return true;
        }
    }
}

template <class Mapped, class Handle, class Allocator>
inline bool HandleDatabase<Mapped, Handle, Allocator>::releaseIfLastRef(Mapped* mapped)
{
    HandleData* pHandleData = getHandleData(mapped);

    // We're working with the Mapped type here, so if it's not valid we're in UB.
    uint32_t refCount = pHandleData->refCount.load(std::memory_order_acquire);
    for (;;)
    {
        CARB_CHECK(refCount != 0);
        if (refCount > 1)
        {
            return false;
        }
        if (CARB_LIKELY(pHandleData->refCount.compare_exchange_weak(
                refCount, 0, std::memory_order_release, std::memory_order_relaxed)))
        {
            std::atomic_thread_fence(std::memory_order_acquire);
            pHandleData->val.~Mapped();
            pHandleData->index = HandleUnion(getHandleFromValue(mapped)).index;
            m_emo.m_free.push(pHandleData);
            return true;
        }
    }
}

template <class Mapped, class Handle, class Allocator>
inline Mapped* HandleDatabase<Mapped, Handle, Allocator>::getValueFromHandle(Handle handle) const
{
    HandleUnion hu(handle);

    HandleData* pHandleData = getHandleData(hu.index);
    if (pHandleData == nullptr)
    {
        return nullptr;
    }

    Metadata meta = pHandleData->metadata.load(std::memory_order_acquire);
    return (meta.refCount != 0 && (meta.lifecycle & kLifecycleMask) == hu.lifecycle) ? std::addressof(pHandleData->val) :
                                                                                       nullptr;
}

template <class Mapped, class Handle, class Allocator>
inline Handle HandleDatabase<Mapped, Handle, Allocator>::getHandleFromValue(const Mapped* mapped) const
{
    // Resolve mapped to the handle data
    // We're working with the Mapped type here, so if it's not valid we're in UB.
    HandleData* pHandleData = getHandleData(mapped);

    // Start setting up the handle, but we don't know the index yet.
    uint32_t lifecycle = pHandleData->metadata.load(std::memory_order_acquire).lifecycle;

    // Find the index by searching all of the buckets.
    for (uint32_t i = 0; i < kBuckets; ++i)
    {
        HandleData* p = getDbIndex(i, std::memory_order_acquire);
        if (!p)
            break;

        size_t offset = size_t(pHandleData - p);
        if (offset < (size_t(1) << i))
        {
            // Found the index!
            uint32_t index = (uint32_t(1) << i) - 1 + uint32_t(offset);
            return HandleUnion(index, lifecycle).handle;
        }
    }

    // Given mapped doesn't exist in this HandleDatabase
    CARB_CHECK(0);
    return Handle{};
}

template <class Mapped, class Handle, class Allocator>
inline void HandleDatabase<Mapped, Handle, Allocator>::indexToBucketAndOffset(uint32_t index,
                                                                              uint32_t& bucket,
                                                                              uint32_t& offset)
{
    // Bucket id
    bucket = 31 - math::numLeadingZeroBits(index + 1);
    offset = index + 1 - (1 << CARB_MIN(31u, bucket));
}

template <class Mapped, class Handle, class Allocator>
inline auto HandleDatabase<Mapped, Handle, Allocator>::expandDatabase() -> HandleData*
{
    static HandleData* const kLocked = reinterpret_cast<HandleData*>(size_t(-1));

    // Scan (right now O(n), but this is rare and n is small) for the first empty database.
    size_t bucketToAllocateFrom;
    for (bucketToAllocateFrom = 0; bucketToAllocateFrom != CARB_COUNTOF(m_database); bucketToAllocateFrom++)
    {
        HandleData* mem = m_database[bucketToAllocateFrom].load(std::memory_order_acquire);
        for (;;)
        {
            while (mem == kLocked)
            {
                // Another thread is trying to allocate this. Try to pull from the free list until it stabilizes.
                mem = m_emo.m_free.pop();
                if (mem)
                    return mem;

                mem = m_database[bucketToAllocateFrom].load(std::memory_order_acquire);
            }
            if (!mem)
            {
                // Try to lock it
                if (m_database[bucketToAllocateFrom].compare_exchange_strong(
                        mem, kLocked, std::memory_order_release, std::memory_order_relaxed))
                {
                    // Successfully locked
                    break;
                }

                // Continue if another thread has locked it
                if (mem == kLocked)
                    continue;

                // Failed to lock, but it's not locked anymore, so see if there's a free entry now
                HandleData* hd = m_emo.m_free.pop();
                if (hd)
                    return hd;
            }

            CARB_ASSERT(mem != kLocked);
            break;
        }

        if (!mem)
            break;
    }
    CARB_FATAL_UNLESS(bucketToAllocateFrom < CARB_COUNTOF(m_database), "Out of handles!");

    uint32_t const allocateCount = 1 << bucketToAllocateFrom;
    uint32_t const offset = allocateCount - 1;

    HandleData* handleData = AllocatorTraits::allocate(m_emo, allocateCount);

    // Set the indexes
    for (uint32_t i = 0; i < allocateCount; ++i)
    {
        AllocatorTraits::construct(m_emo, handleData + i, offset + i);
    }

    // Add entries (after the first) to the free list
    m_emo.m_free.push(handleData + 1, handleData + allocateCount);

    // Overwrite the lock with the actual pointer now
    HandleData* expect = m_database[bucketToAllocateFrom].exchange(handleData, std::memory_order_release);
    CARB_ASSERT(expect == kLocked);
    CARB_UNUSED(expect);

    // Return the first entry that we reserved for the caller
    AllocatorTraits::construct(m_emo, handleData, offset);
    return handleData;
}

template <class Mapped, class Handle, class Allocator>
template <class Func>
inline void HandleDatabase<Mapped, Handle, Allocator>::forEachHandle(Func&& f) const
{
    for (uint32_t i = 0; i < kBuckets; ++i)
    {
        // We are done once we encounter a null pointer
        HandleData* data = getDbIndex(i, std::memory_order_acquire);
        if (!data)
            break;

        uint32_t const bucketSize = 1 << i;
        for (uint32_t j = 0; j != bucketSize; ++j)
        {
            Metadata meta = data[j].metadata.load(std::memory_order_acquire);
            if (meta.refCount != 0)
            {
                // Valid handle
                HandleUnion hu((bucketSize - 1) + j, meta.lifecycle);

                // Call the functor
                f(hu.handle, std::addressof(data[j].val));
            }
        }
    }
}

template <class Mapped, class Handle, class Allocator>
inline size_t HandleDatabase<Mapped, Handle, Allocator>::clear()
{
    size_t count = 0;

    for (uint32_t i = 0; i != kBuckets; ++i)
    {
        HandleData* data = getDbIndex(i, std::memory_order_acquire);
        if (!data)
            break;

        uint32_t const bucketSize = 1u << i;
        for (uint32_t j = 0; j != bucketSize; ++j)
        {
            if (data[j].refCount.exchange(0, std::memory_order_release) != 0)
            {
                // Successfully set the refcount to 0. Destroy and add to the free list
                std::atomic_thread_fence(std::memory_order_acquire);
                ++count;
                data[j].val.~Mapped();
                data[j].index = bucketSize - 1 + j;
                m_emo.m_free.push(std::addressof(data[j]));
            }
        }
    }

    return count;
}

template <class Mapped, class Handle, class Allocator>
inline auto HandleDatabase<Mapped, Handle, Allocator>::makeScopedRef(Handle handle) -> HandleRefType
{
    return HandleRef<Mapped, Handle, Allocator>(*this, handle);
}

template <class Mapped, class Handle, class Allocator>
inline auto HandleDatabase<Mapped, Handle, Allocator>::makeScopedRef(Handle handle) const -> const scoped_handle_ref_type
{
    return std::move(
        HandleRef<Mapped, Handle, Allocator>(const_cast<HandleDatabase<Mapped, Handle, Allocator>*>(*this), handle));
}

// HandleRef functions.
template <class Mapped, class Handle, class Allocator>
inline HandleRef<Mapped, Handle, Allocator>::HandleRef(Database& database, Handle handle)
    : m_mapped(database.tryAddRef(handle))
{
    if (m_mapped)
    {
        m_database = std::addressof(database);
        m_handle = handle;
    }
}

template <class Mapped, class Handle, class Allocator>
inline HandleRef<Mapped, Handle, Allocator>::~HandleRef()
{
    reset();
}

template <class Mapped, class Handle, class Allocator>
inline HandleRef<Mapped, Handle, Allocator>::HandleRef(HandleRef&& other)
    : m_database(std::exchange(other.m_database, nullptr)),
      m_handle(std::exchange(other.m_handle, Handle{})),
      m_mapped(std::exchange(other.m_mapped, nullptr))
{
}

template <class Mapped, class Handle, class Allocator>
inline auto HandleRef<Mapped, Handle, Allocator>::operator=(HandleRef&& other) -> HandleRef&
{
    swap(other);
    return *this;
}

template <class Mapped, class Handle, class Allocator>
inline auto HandleRef<Mapped, Handle, Allocator>::clone() const -> HandleRef
{
    HandleRef ret;
    ret.m_database = m_database;
    ret.m_handle = m_handle;
    ret.m_mapped = m_mapped;
    if (ret.m_mapped)
    {
        ret.m_database->addRef(ret.m_mapped);
    }
    return ret;
}

template <class Mapped, class Handle, class Allocator>
inline Mapped* HandleRef<Mapped, Handle, Allocator>::operator->()
{
    CARB_ASSERT(m_mapped);
    return m_mapped;
}

template <class Mapped, class Handle, class Allocator>
inline const Mapped* HandleRef<Mapped, Handle, Allocator>::operator->() const
{
    CARB_ASSERT(m_mapped);
    return m_mapped;
}

template <class Mapped, class Handle, class Allocator>
inline Mapped& HandleRef<Mapped, Handle, Allocator>::operator*()
{
    CARB_ASSERT(m_mapped);
    return *m_mapped;
}

template <class Mapped, class Handle, class Allocator>
inline const Mapped& HandleRef<Mapped, Handle, Allocator>::operator*() const
{
    CARB_ASSERT(m_mapped);
    return *m_mapped;
}

template <class Mapped, class Handle, class Allocator>
inline Mapped* HandleRef<Mapped, Handle, Allocator>::get()
{
    return m_mapped;
}

template <class Mapped, class Handle, class Allocator>
inline const Mapped* HandleRef<Mapped, Handle, Allocator>::get() const
{
    return m_mapped;
}

template <class Mapped, class Handle, class Allocator>
inline Handle HandleRef<Mapped, Handle, Allocator>::handle() const
{
    return m_handle;
}

template <class Mapped, class Handle, class Allocator>
inline HandleRef<Mapped, Handle, Allocator>::operator bool() const
{
    return m_mapped != nullptr;
}

template <class Mapped, class Handle, class Allocator>
inline void HandleRef<Mapped, Handle, Allocator>::reset()
{
    if (m_mapped)
    {
        m_database->release(m_handle);
    }
    m_database = nullptr;
    m_handle = handle_type();
    m_mapped = nullptr;
}

template <class Mapped, class Handle, class Allocator>
inline void HandleRef<Mapped, Handle, Allocator>::swap(HandleRef& rhs)
{
    std::swap(m_database, rhs.m_database);
    std::swap(m_handle, rhs.m_handle);
    std::swap(m_mapped, rhs.m_mapped);
}

// Global operators.
/**
 * Tests equality between HandleRef and `nullptr`.
 * @param ref The HandleRef to test.
 * @returns `true` if @p ref is empty; `false` otherwise.
 */
template <class Mapped, class Handle, class Allocator>
inline bool operator==(const HandleRef<Mapped, Handle, Allocator>& ref, std::nullptr_t)
{
    return ref.get() == nullptr;
}

/**
 * Tests equality between HandleRef and `nullptr`.
 * @param ref The HandleRef to test.
 * @returns `true` if @p ref is empty; `false` otherwise.
 */
template <class Mapped, class Handle, class Allocator>
inline bool operator==(std::nullptr_t, const HandleRef<Mapped, Handle, Allocator>& ref)
{
    return ref.get() == nullptr;
}

/**
 * Tests inequality between HandleRef and `nullptr`.
 * @param ref The HandleRef to test.
 * @returns `true` if @p ref is not empty; `false` otherwise.
 */
template <class Mapped, class Handle, class Allocator>
inline bool operator!=(const HandleRef<Mapped, Handle, Allocator>& ref, std::nullptr_t)
{
    return ref.get() != nullptr;
}

/**
 * Tests inequality between HandleRef and `nullptr`.
 * @param ref The HandleRef to test.
 * @returns `true` if @p ref is not empty; `false` otherwise.
 */
template <class Mapped, class Handle, class Allocator>
inline bool operator!=(std::nullptr_t, const HandleRef<Mapped, Handle, Allocator>& ref)
{
    return ref.get() != nullptr;
}

} // namespace extras
} // namespace carb

CARB_IGNOREWARNING_MSC_POP
