// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

#include "extras/Library.h"
#include "extras/SharedMemory.h"
#include "cpp20/Atomic.h"
#include "math/Util.h"
#include "memory/Util.h"
#include "extras/ScopeExit.h"
#include "thread/Util.h"

#if CARB_PLATFORM_WINDOWS
#    include "CarbWindows.h"
#else
#    include <sys/mman.h>
#    include <unistd.h>
#endif

#include <inttypes.h>

namespace carb
{

namespace detail
{

namespace rstring
{

// Make sure everything is packed to 8 bytes. This is important since different modules can link this code but all
// instances of the code have to work with the same virtual memory.
#pragma pack(push, 8)

CARB_IGNOREWARNING_MSC_WITH_PUSH(4200) // nonstandard extension used: zero-sized array in struct/union
struct Rec
{
    Rec* m_next;
    uint32_t m_stringId;
    uint32_t m_stringLen : 31;
    uint32_t m_authority : 1;

    size_t const m_uncasedHash;
    size_t m_hash{ 0 };
    char m_string[0]; // Actual size is m_stringLen + 1

    Rec(Rec* next, uint32_t stringId, uint32_t stringLen, bool authority, size_t uncasedHash, const char* s)
        : m_next(next), m_stringId(stringId), m_stringLen(stringLen), m_authority(authority), m_uncasedHash(uncasedHash)
    {
        memcpy(m_string, s, stringLen);
        m_string[stringLen] = '\0';
    }
};
CARB_IGNOREWARNING_MSC_POP

struct MemoryAlloc
{
    MemoryAlloc* m_next;
    size_t m_size;
};

enum LockState : uint8_t
{
    Unlocked,
    Locked,
    LockedMaybeWaiting,
};

constexpr eRString kNoHint = eRString(-1);

namespace versioned
{

// NOTE NOTE NOTE: These constants should only be used in the initializer data for struct Data, below. Since a different
// module could have constructed Data, we need to read the appropriate values out of the Data struct itself at runtime.
// version 1: initial release
// version 2: added memory add/remove handlers
// version 3: linked list of Internals structures for memory tracking
constexpr uint8_t Version = 3;

// Checklist for increasing Version:
// 1. In premake5.lua, add a `define_rstringversiontest(X)` call where X is your new version number.
// 2. Copy this file (RStringInternals.inl) to a new directory `X` under *source/tests/plugins/carb/rstringversiontest*
//     where `X` is your new version number.
// 3. In the new copy of RStringInternals.inl produced in step 2, change the `rstring` namespace to `rstring_X` where
//     `X` is your new version number.
// 4. In the new copy of RStringInternals.inl produced in step 2, remove the line that is:
//     #include "RStringEnum.inl"
//    and replace it with the EMPTY_ENTRY() and ENTRY() definitions currently present in RStringEnum.inl

// These values can ONLY be changed if the Version is changed
constexpr size_t kNumHashBuckets = 2 << 10; // ~2k
constexpr size_t kMaxEntries = 2 << 20; // ~2m
constexpr size_t kEntriesPerChunk = 16 << 10; // ~16k
constexpr size_t kNumChunks = kMaxEntries / kEntriesPerChunk; // 128
constexpr size_t kAllocSize = 64 << 10; // ~64k

static_assert(math::isPowerOf2(kNumHashBuckets), "Hash table bucket count must be power of 2");
static_assert(math::isPowerOf2(kAllocSize), "Alloc size must be power of 2");
static_assert(kMaxStaticRString < kEntriesPerChunk, "All static entries must fit within first chunk");

} // namespace versioned

using Bucket = Rec*;
using HashTable = Rec**;
using Chunk = Rec*;
using ChunkList = Chunk*;
using VisualizerType = ChunkList*;
class Internals;

// This is for *debugging only*. A Visual Studio visualizer exists in carb.natvis to allow looking up RString values
// via this variable. Note that each module (executable or shared library) will have its own instance of this variable,
// which is expected. This allows all modules that use RString to have a Visualizer variable present that Visual Studio
// can use for debugging purposes.
CARB_WEAKLINK VisualizerType volatile Visualizer;

using OnMemoryChange = void (*)(const void*, size_t, void*);

struct MemoryChangeNotifier
{
    OnMemoryChange callback;
    void* user;

    bool operator==(const MemoryChangeNotifier& rhs) const
    {
        return callback == rhs.callback && user == rhs.user;
    }
};

// This structure is mapped into memory and carefully versioned as each binary (DLL/EXE/shared object/etc) can open the
// memory mapping and manipulate the data. Therefore, everything that uses this structure must agree on the layout and
// size and changes to this must be done in a very careful manner.
struct Data
{
    uint8_t version{ versioned::Version }; // Byte offsets used: 0 - 1
    std::atomic<LockState> lock{ Unlocked }; // 1 - 2
    std::atomic_ushort initialized{ 0 }; // 2 - 4

    size_t const MaxEntries{ versioned::kMaxEntries }; // 8 - 16
    size_t const EntriesPerChunk{ versioned::kEntriesPerChunk }; // 16 - 24
    size_t const StaticEntries{ kMaxStaticRString }; // 24 - 32
    size_t AllocSize{ versioned::kAllocSize }; // 32 - 40

    size_t nextIndex{ 0 }; // 40 - 48
    MemoryAlloc* allocList{ nullptr }; // 48 - 56

    uint8_t* mem{ nullptr }; // 56 - 64
    uint8_t* memEnd{ nullptr }; // 64 - 72

    size_t const NumHashBuckets{ versioned::kNumHashBuckets }; // 72 - 80
    Bucket* hashTableBuckets{ nullptr }; // 80 - 88

    size_t const ChunkListSize{ versioned::kNumChunks }; // 88 - 96
    ChunkList* chunkLists{ nullptr }; // 96 - 104

    // Don't change this count; add a new member instead.
    char loadingModule[256]; // 104 - 360

    // Don't change this count; add a new member instead.
    MemoryChangeNotifier onMemoryChange[16]; // 360 - 616

    // Linked list of all Internals objects in various modules
    Internals* head; // 616 - 624
    Internals* tail; // 624 - 632

    // NOTE: Always add new members here!
};

// Size and member asserts
static_assert(sizeof(MemoryChangeNotifier) == 16, "sizeof(MemoryChangeNotifier) may not change");
static_assert(offsetof(Data, version) == 0, "Member location and size may not change");
static_assert(offsetof(Data, lock) == 1, "Member location and size may not change");
static_assert(offsetof(Data, initialized) == 2, "Member location and size may not change");
static_assert(offsetof(Data, MaxEntries) == 8, "Member location and size may not change");
static_assert(offsetof(Data, EntriesPerChunk) == 16, "Member location and size may not change");
static_assert(offsetof(Data, StaticEntries) == 24, "Member location and size may not change");
static_assert(offsetof(Data, AllocSize) == 32, "Member location and size may not change");
static_assert(offsetof(Data, nextIndex) == 40, "Member location and size may not change");
static_assert(offsetof(Data, allocList) == 48, "Member location and size may not change");
static_assert(offsetof(Data, mem) == 56, "Member location and size may not change");
static_assert(offsetof(Data, memEnd) == 64, "Member location and size may not change");
static_assert(offsetof(Data, NumHashBuckets) == 72, "Member location and size may not change");
static_assert(offsetof(Data, hashTableBuckets) == 80, "Member location and size may not change");
static_assert(offsetof(Data, ChunkListSize) == 88, "Member location and size may not change");
static_assert(offsetof(Data, chunkLists) == 96, "Member location and size may not change");
static_assert(offsetof(Data, loadingModule) == 104, "Member location and size may not change");
static_assert(offsetof(Data, onMemoryChange) == 360, "Member location and size may not change");
static_assert(offsetof(Data, head) == 616, "Member location and size may not change");
static_assert(offsetof(Data, tail) == 624, "Member location and size may not change");

// This may change if new members are added to the end
static_assert(sizeof(Data) == 632,
              "Please update this value to reflect new members. Make sure that version was"
              "increased and old versions accounted for. Existing members and their size may not be changed.");

inline bool casedEqual(const char* str1, const char* str2, size_t len)
{
    return std::memcmp(str1, str2, len) == 0;
}

inline bool uncasedEqual(const char* str1, const char* str2, size_t len)
{
    const char* const str1end = str1 + len;
    while (str1 != str1end)
    {
        if (carb::tolower(*(str1++)) != carb::tolower(*(str2++)))
            return false;
    }
    return true;
}

inline int casedCompare(const char* str1, size_t len1, const char* str2, size_t len2)
{
    int result = std::memcmp(str1, str2, ::carb_min(len1, len2));
    if (result == 0 && len1 != len2)
        return len1 < len2 ? -1 : 1;
    return result;
}

inline int uncasedCompare(const char* str1, size_t len1, const char* str2, size_t len2)
{
    size_t minlen = ::carb_min(len1, len2);
    for (; minlen != 0; --minlen, ++str1, ++str2)
    {
        signed char c1 = carb::tolower(*str1);
        signed char c2 = carb::tolower(*str2);
        int val = c1 - c2;
        if (val != 0)
            return val;
    }
    if (len1 != len2)
        return len1 < len2 ? -1 : 1;
    return 0;
}

class Internals
{
public:
    static Internals& get()
    {
        static Internals internals{};
        return internals;
    }

    const Rec* at(uint32_t stringId) const
    {
        CARB_UNUSED(m_unused);
        // Don't need to hold the mutex for this
        size_t chunkList = stringId / m_data->EntriesPerChunk;
        size_t chunkListOffset = stringId % m_data->EntriesPerChunk;
        if (chunkList < m_data->ChunkListSize && m_data->chunkLists[chunkList])
        {
            return m_data->chunkLists[chunkList][chunkListOffset];
        }
        return nullptr;
    }

    const Rec* operator[](uint32_t stringId) const
    {
        return at(stringId);
    }

    uint32_t findOrAdd(const char* str, bool uncased, RStringOp op)
    {
        if (!str || *str == '\0')
            return uint32_t(eRString::Empty);
        return findOrAdd(kNoHint, str, std::strlen(str), uncased, op);
    }

    uint32_t findOrAdd(const char* str, size_t len, bool uncased, RStringOp op)
    {
        if (!str || len == 0)
            return uint32_t(eRString::Empty);
        return findOrAdd(kNoHint, str, len, uncased, op);
    }

    uint32_t convertUncased(uint32_t stringId) const
    {
        const Rec* rec = at(stringId);
        if (rec)
        {
            // This is already the case-insensitive authority.
            if (rec->m_authority)
            {
                return stringId;
            }

            // The uncased record should always be found.
            rec = hashTableFind(rec->m_string, rec->m_stringLen, true, rec->m_uncasedHash);
            CARB_ASSERT(rec);
            return rec->m_stringId;
        }
        return uint32_t(eRString::Empty);
    }

    size_t getHash(uint32_t stringId)
    {
        Rec* rec = at(stringId);
        if (!rec)
            return 0;

        carb::cpp20::atomic_ref<size_t> hashRef(rec->m_hash);
        size_t hash = hashRef.load(std::memory_order_acquire);
        if (CARB_LIKELY(hash))
            return hash;

        // The hash for this registered string hasn't been computed yet. If multiple threads enter this function
        // simultaneously, they should all compute the same value, so it doesn't matter if it's written multiple times.
        hash = carb::hashString(rec->m_string);
        hashRef.store(hash, std::memory_order_release);
        return hash;
    }

    bool addMemoryNotifier(OnMemoryChange callback, void* user, bool callForCurrent)
    {
        if (m_data->version >= 2 && callback)
        {
            lockMutex();
            CARB_SCOPE_EXIT
            {
                unlockMutex();
            };

            size_t const kCount = CARB_COUNTOF(m_data->onMemoryChange);
            size_t i = 0;
            for (; i != kCount; ++i)
            {
                if (m_data->onMemoryChange[i].callback == nullptr)
                {
                    m_data->onMemoryChange[i] = { callback, user };
                    break;
                }
            }
            if (i == kCount)
            {
                // No empty slots
                return false;
            }

            if (callForCurrent)
            {
                callback(m_data, sizeof(*m_data), user);

                // Report all registered visualizer variables
                if (m_data->version >= 3)
                {
                    for (Internals* p = m_data->head; p; p = p->m_next)
                    {
                        callback(p->m_visualizer, sizeof(VisualizerType), user);
                    }
                }
                else
                {
                    // Report only our visualizer variable since we don't have a list to walk
                    callback(m_visualizer, sizeof(VisualizerType), user);
                }

                // Report all allocations in the list
                for (MemoryAlloc* alloc = m_data->allocList; alloc; alloc = alloc->m_next)
                {
                    callback(alloc, alloc->m_size, user);
                }

                // ChunkLists after the first one aren't included in the allocList, so add those separately
                for (size_t chunk = 1; chunk != m_data->ChunkListSize; ++chunk)
                {
                    if (!m_data->chunkLists[chunk])
                        break;
                    callback(m_data->chunkLists[chunk], sizeof(Chunk) * m_data->EntriesPerChunk, user);
                }
            }
        }
        return false;
    }

    void removeMemoryNotifier(OnMemoryChange callback, void* user)
    {
        if (m_data->version >= 2 && callback)
        {
            lockMutex();
            CARB_SCOPE_EXIT
            {
                unlockMutex();
            };

            auto const end =
                std::find(m_data->onMemoryChange, m_data->onMemoryChange + CARB_COUNTOF(m_data->onMemoryChange),
                          MemoryChangeNotifier{});
            // Remove matching entries and fill to the end with empty entries.
            std::fill(std::remove(m_data->onMemoryChange, end, MemoryChangeNotifier{ callback, user }), end,
                      MemoryChangeNotifier{});
        }
    }

    void notifyQuickShutdown()
    {
        // The process is about to call _exit(), so close our shared memory regions.
        m_data = nullptr;
        m_view.reset();
        m_shm.close(true); // force unlink of the shared memory
    }

    bool initializedByMe() const
    {
        return m_initializedByMe;
    }

private:
    static bool validate(Data* pData)
    {
        // Either a shared object with an older version of this code exists and created the RString mapping, or
        // an existing file was found on disk from a previous crash and we're reusing the pid. If it's the former,
        // we want to use it gracefully. If it's the latter, we're going to erase it and do the new method.
        bool valid;

        // If initialization is in progress, wait for it to finish. However, this could be completely garbage memory
        // so we're going to put a time limit on it. NOTE: This used to use cpp20::atomic::wait_for(), but this does not
        // work properly since different binaries map the shared memory to different addresses, and for the underlying
        // futex implementation to work properly the address must be unique. Therefore, we just spin for a bit.
        auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while ((valid = pData->initialized.load(std::memory_order_acquire)) == false &&
               std::chrono::steady_clock::now() < timeout)
            std::this_thread::yield();

        valid = valid && memory::testReadable(pData->mem);
        valid = valid && memory::testReadable(pData->hashTableBuckets);
        valid = valid && memory::testReadable(pData->chunkLists);

        if (valid)
        {
            // Walk the alloc list
            MemoryAlloc* alloc = pData->allocList;
            while (alloc)
            {
                valid = memory::testReadable(alloc);
                if (!valid)
                    break;
                alloc = alloc->m_next;
            }
        }

        if (valid)
        {
            // Walk the memory change notifier list
            if (pData->version >= 2)
            {
                auto const end = pData->onMemoryChange + CARB_COUNTOF(pData->onMemoryChange);
                for (auto p = pData->onMemoryChange; valid && p != end && p->callback; ++p)
                {
                    valid = memory::testReadable((const void*)p->callback);
                }
            }
        }

        if (valid)
        {
            // Walk the internals list
            if (pData->version >= 3)
            {
                Internals* p = pData->head;
                while (p)
                {
                    valid = memory::testReadable(p);
                    if (!valid)
                        break;
                    p = p->m_next;
                }
            }
        }

        return valid;
    }

    void init()
    {
        // We created the memory, so it's our responsibility to initialize it.
        m_initializedByMe = true;
        new (m_data) Data{};

        // Set the constructing module file name
        {
            const volatile void* addr = &Visualizer;
            auto libName = extras::getLibraryFilename(const_cast<void*>(addr));
            auto len = ::carb_min(CARB_COUNTOF(m_data->loadingModule) - 1, libName.size());
            memcpy(m_data->loadingModule, libName.data(), len);
            m_data->loadingModule[len] = '\0';
        }

        size_t const allocGranularity = m_shm.getSystemAllocationGranularity();
        CARB_ASSERT(math::isPowerOf2(allocGranularity));

        // Round AllocSize up to allocation granularity
        m_data->AllocSize = (m_data->AllocSize + allocGranularity - 1) & -ptrdiff_t(allocGranularity);

        // Allocate everything needed initially from one chunk. Reserve extra bytes that will be used as Rec memory
        // once rounded up to allocation granularity.
        size_t sizeNeeded = sizeof(MemoryAlloc) + (sizeof(Bucket) * versioned::kNumHashBuckets) +
                            (sizeof(ChunkList) * versioned::kNumChunks) + (sizeof(Rec*) * versioned::kEntriesPerChunk) +
                            sizeof(Rec) + 1;

        // Round up to allocation granularity.
        sizeNeeded = (sizeNeeded + allocGranularity - 1) & -ptrdiff_t(allocGranularity);

        void* mem = sysAlloc(sizeNeeded);
        CARB_FATAL_UNLESS(mem, "Failed to allocate system memory for RString space");
        // Don't need to notifyMemory() here because it's impossible that anything has registered at this point. Any
        // other threads will be waiting on m_data->initialized.

        uint8_t* bytes = static_cast<uint8_t*>(mem);
        m_data->memEnd = bytes + sizeNeeded;

        // Set all of the members
        m_data->allocList = new (bytes) MemoryAlloc{ m_data->allocList, sizeNeeded };
        bytes += sizeof(MemoryAlloc);
        m_data->hashTableBuckets = reinterpret_cast<Bucket*>(bytes);
        bytes += (sizeof(Bucket) * versioned::kNumHashBuckets);
        // Set our Visualizer for debugging
        Visualizer = m_data->chunkLists = reinterpret_cast<ChunkList*>(bytes);
        bytes += (sizeof(ChunkList) * versioned::kNumChunks);
        m_data->chunkLists[0] = reinterpret_cast<Chunk*>(bytes);
        bytes += (sizeof(Chunk) * versioned::kEntriesPerChunk);
        m_data->mem = bytes;
        CARB_ASSERT(size_t(m_data->memEnd - m_data->mem) > sizeof(Rec)); // Should at least be able to fit one Rec

        struct Entry
        {
            eRString enumVal;
            const char* str;
            size_t len;
        };
#define EMPTY_ENTRY(a, b) { eRString(a), "", 0 },
#define ENTRY(a, b) { eRString(a), #b, CARB_COUNTOF(#b) - 1 },
        static const Entry entries[] = {
#define RSTRINGENUM_FROM_RSTRING_INL
#include "RStringEnum.inl"
#undef RSTRINGENUM_FROM_RSTRING_INL
#undef ENTRY
#undef EMPTY_ENTRY
        };

        // Register the static strings
        for (size_t i = 0; i != CARB_COUNTOF(entries); ++i)
        {
            findOrAdd(entries[i].enumVal, entries[i].str, entries[i].len, false, RStringOp::eRegister);
        }

        m_data->nextIndex = kMaxStaticRString + 1;

        // Set up the linked list of Internal structures
        m_data->head = m_data->tail = this;
        m_next = m_prev = nullptr; // redundant

        // Last step: set initialized. Anyone spinning on `initialized` will wake.
        // NOTE: Older versions of RString used cpp20::atomic::wait and notify_all(), but this doesn't work properly
        // because each binary will map the shared memory at a different address and the underlying futex system
        // requires a unique address to work properly.
        auto old = m_data->initialized.exchange(1, std::memory_order_release);
        CARB_FATAL_UNLESS(old == 0, "Initialization of internal data already performed!");
    }

    Internals()
    {
        // Static checks
        static_assert(offsetof(Internals, m_version) == 0, "Member size and offset may not change");
        static_assert(sizeof(m_version) == 1, "Member size and offset may not change");
        static_assert(offsetof(Internals, m_initializedByMe) == 1, "Member size and offset may not change");
        static_assert(sizeof(m_initializedByMe) == 1, "Member size and offset may not change");
        static_assert(offsetof(Internals, m_next) == 8, "Member size and offset may not change");
        static_assert(sizeof(m_next) == 8, "Member size and offset may not change");
        static_assert(offsetof(Internals, m_prev) == 16, "Member size and offset may not change");
        static_assert(sizeof(m_prev) == 8, "Member size and offset may not change");
        static_assert(offsetof(Internals, m_visualizer) == 24, "Member size and offset may not change");
        static_assert(sizeof(m_visualizer) == 8, "Member size and offset may not change");

        process::ProcessId pid = this_process::getId();
        char name[256];
        // PID does not a good ID make. Ideally we would use this_process::getUniqueId(), but we cannot change the name
        // because we need to be backwards compatible with modules built with an old version of this code. Windows does
        // a good job of cleaning up named objects once all references to them expire. Since this is only used within
        // the context of a process, that means that we're very unlikely to run into a situation on Windows where the
        // data is invalid because we're reusing a PID. On Linux however, that's not the case. Linux doesn't
        // automatically clean up shared memory objects. This is unfortunately as it means that the shared memory region
        // that we opened could have been garbage from a different process that has crashed or did not shut down
        // cleanly. As the mapped region contains pointers that are only good when the process is running, we need to
        // validate the data and unlink it if it isn't valid. For Linux, to ensure that we have exclusive access, we're
        // also going to hold the same shared semaphore that SharedMemory uses.
        extras::formatString(name, CARB_COUNTOF(name), "carb-RStringInternals-%" OMNI_PRIpid, pid);

        auto result = extras::SharedMemory::Result::eOpened;
        uint32_t shmFlags = 0;
#if CARB_PLATFORM_LINUX
        // Keep the global semaphore locked the whole time this is occurring. This will allow us to check the old shm
        // region and create a new one under the lock.
        extras::detail::NamedSemaphore sema{ extras::detail::getGlobalSemaphoreName() };
        std::unique_lock<extras::detail::NamedSemaphore> guard(sema);

        shmFlags = extras::SharedMemory::fNoMutexLock;

        if (m_shm.open(name, sizeof(Data), shmFlags | extras::SharedMemory::fQuiet))
        {
            m_view.reset(m_shm.createView());
            if (!m_view)
            {
                guard.unlock(); // Don't crash with the global semaphore locked!
                CARB_FATAL_UNLESS(false, "Error while mapping shared memory %s", name);
            }

            m_data = static_cast<Data*>(m_view->getAddress());
            if (!validate(m_data))
            {
                m_data = nullptr;
                m_view.reset();
                m_shm.close(true); // Force unlink since nothing could successfully use this shm object

                // Should now be able to create a new one
                if (!m_shm.create(name, sizeof(Data), shmFlags))
                {
                    guard.unlock(); // Don't crash with the global semaphore locked!
                    CARB_FATAL_UNLESS(false, "Failed to create shared memory named %s", name);
                }
                result = extras::SharedMemory::Result::eCreated;
            }
        }
#endif

        if (!m_shm.isOpen())
        {
            result = m_shm.createOrOpen(name, sizeof(Data), shmFlags);
        }

#if CARB_PLATFORM_LINUX
        // The rest of initialization can proceed without the global lock.
        guard.unlock();
#endif

        CARB_FATAL_UNLESS(result != extras::SharedMemory::eError, "Error while opening shared memory %s", name);

        m_view.reset(m_shm.createView());
        CARB_FATAL_UNLESS(m_view, "Error while mapping shared memory %s", name);

        m_data = static_cast<Data*>(m_view->getAddress());

        if (result == extras::SharedMemory::eCreated)
        {
            // We created the shm object, so it's our job to initialize
            init();
        }
        else
        {
            // Wait until initialized is non-zero. NOTE: Previously this used a cpp20::atomic::wait(), but this will not
            // work properly because different binaries map the shared memory to different addresses, so `*this` of the
            // atomic variable varies and the underlying futex may not wake all waiters. Therefore, just spin and wait
            // for initialization.
            while (!m_data->initialized.load(std::memory_order_acquire))
                std::this_thread::yield();

            CARB_FATAL_UNLESS(
                m_data->StaticEntries >= kMaxStaticRString,
                "RString: version mismatch: this module expects static RString entries that the loading module (%s) is not aware of. Please re-build the loading module with the latest version of Carbonite.",
                m_data->loadingModule);

            lockMutex();
            // Set our Visualizer for debugging
            Visualizer = m_data->chunkLists;

            // Add ourself to the internal list if correct version. If we're working with an older version, next and
            // prev will remain nullptr.
            if (m_data->version >= 3)
            {
                m_prev = m_data->tail;
                if (m_prev)
                {
                    m_data->tail = m_prev->m_next = this;
                }
                else
                {
                    m_data->head = m_data->tail = this;
                }
            }

            // Notify about our visualizer
            notifyMemory(m_visualizer, sizeof(VisualizerType));
            unlockMutex();
        }
    }
    ~Internals()
    {
        // We should only get here if notifyQuickShutdown() was not called. This assert fires if it was.
        CARB_ASSERT(m_data);

        lockMutex();
        // Remove ourself from the linked list if we were registered into it
        if (m_data->version >= 3)
        {
            if (m_next)
                m_next->m_prev = m_prev;
            else
            {
                CARB_ASSERT(m_data->tail == this);
                m_data->tail = m_prev;
            }
            if (m_prev)
                m_prev->m_next = m_next;
            else
            {
                CARB_ASSERT(m_data->head == this);
                m_data->head = m_next;
            }
            m_next = m_prev = nullptr;
        }

        // Remove our Visualizer from memory registration
        notifyMemory(m_visualizer, 0);
        unlockMutex();

        // Close our reference to the mapping and shared memory, but leak any memory created with sysAlloc() since it
        // may still be in use by other modules within the process.
    }

    Rec* at(uint32_t stringId)
    {
        // Don't need to hold the mutex for this
        size_t chunkList = stringId / m_data->EntriesPerChunk;
        size_t chunkListOffset = stringId % m_data->EntriesPerChunk;
        if (chunkList < m_data->ChunkListSize && m_data->chunkLists[chunkList])
        {
            return m_data->chunkLists[chunkList][chunkListOffset];
        }
        return nullptr;
    }

    void* sysAlloc(size_t size)
    {
        // Allocate memory directly from the system. This is necessary as different modules can have different heaps and
        // we want memory that won't be affected when modules are unloaded.
#if CARB_PLATFORM_WINDOWS
        return ::VirtualAlloc(nullptr, size, CARBWIN_MEM_COMMIT | CARBWIN_MEM_RESERVE, CARBWIN_PAGE_READWRITE);
#else
        return ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
#endif
    }

    void sysFree(void* mem, size_t size)
    {
#if CARB_PLATFORM_WINDOWS
        CARB_UNUSED(size);
        ::VirtualFree(mem, 0, CARBWIN_MEM_RELEASE);
#else
        ::munmap(mem, size);
#endif
    }

    void notifyMemory(void* mem, size_t size)
    {
        // Assumes that the mutex is locked before calling
        if (m_data->version >= 2)
        {
            for (size_t i = 0; i != CARB_COUNTOF(m_data->onMemoryChange); ++i)
            {
                auto& onMemoryChange = m_data->onMemoryChange[i];
                if (!onMemoryChange.callback)
                    break;
                onMemoryChange.callback(mem, size, onMemoryChange.user);
            }
        }
    }

    void lockMutex()
    {
        LockState state = Unlocked;
        if (CARB_UNLIKELY(!m_data->lock.compare_exchange_strong(
                state, Locked, std::memory_order_acquire, std::memory_order_relaxed)))
        {
            if (state == LockedMaybeWaiting)
            {
                // Failed to lock and need to wait
                // NOTE: This used to use cpp20::atomic::wait(), but this does not work properly since each binary maps
                // the shared memory to a different address, and the underlying futex needs a unique address. Therefore,
                // we just spin with a backoff as a workaround. The `LockedMaybeWaiting` state is a remnant from that
                // era, but we maintain it for older versions of RString that may still expect it to be used.
                this_thread::spinWaitWithBackoff(
                    [&] { return m_data->lock.load(std::memory_order_acquire) != LockedMaybeWaiting; });
            }

            while (m_data->lock.exchange(LockedMaybeWaiting, std::memory_order_acquire) != Unlocked)
            {
                this_thread::spinWaitWithBackoff(
                    [&] { return m_data->lock.load(std::memory_order_acquire) != LockedMaybeWaiting; });
            }
        }
        // Now inside the lock.
    }

    void unlockMutex()
    {
        // Unlock the mutex. Older versions of RString used cpp20::atomic::notify_one(), but this doesn't work
        // properly for the reasons mentioned in lockMutex().
        m_data->lock.store(Unlocked, std::memory_order_release);
    }

    Rec* hashTableFind(const char* str, size_t len, bool uncased, size_t const uncasedHash) const
    {
        // Load from the bucket head with an atomic op because this is not called under the lock and another thread
        // could be modifying the table (under the lock) in findOrAdd(), below. This operation synchronizes-with the
        // store in findOrAdd().
        Rec* rec = carb::cpp20::atomic_ref<Rec*>(m_data->hashTableBuckets[uncasedHash & (m_data->NumHashBuckets - 1)])
                       .load(std::memory_order_acquire);
        for (; rec; rec = rec->m_next)
        {
            if (rec->m_uncasedHash == uncasedHash && len == rec->m_stringLen)
            {
                if ((!uncased && casedEqual(str, rec->m_string, len)) ||
                    (uncased && rec->m_authority && uncasedEqual(str, rec->m_string, len)))
                {
                    // Found in the hash table
                    return rec;
                }
            }
        }
        return nullptr;
    }

    uint32_t findOrAdd(eRString enumVal, const char* str, size_t const len, bool uncased, RStringOp op)
    {
        CARB_ASSERT(str);
        CARB_ASSERT(unsigned(enumVal) <= kMaxStaticRString || enumVal == kNoHint);

        size_t const uncasedHash = carb::hashLowercaseBuffer(str, len);

        // Check the hash-table for an existing entry. We don't need to lock to do this because nothing is ever deleted.
        // Also this can happen during initialization, but only in the thread/module constructing Data. All other
        // threads will be blocked on initializing or constructing the static Internals.
        Rec* rec = hashTableFind(str, len, uncased, uncasedHash);
        if (rec)
        {
            return rec->m_stringId;
        }

        // Not found in hash table. Bail if we're only doing a find.
        if (op == RStringOp::eFindExisting)
        {
            return uint32_t(eRString::Empty);
        }

        // Now need the lock. Make sure to unlock when we leave scope.
        lockMutex();
        CARB_SCOPE_EXIT
        {
            unlockMutex();
        };

        // Search the hash table again as it could have been inserted by a different thread under the lock, but we don't
        // expect this to be the case. So we do a broader search to see if there's a case-insensitive "authority"
        // already. If there isn't then the new one that we're adding will become the authority.
        Rec* authority = nullptr;
        Rec*& pBucketHead = m_data->hashTableBuckets[uncasedHash & (m_data->NumHashBuckets - 1)];
        for (rec = pBucketHead; rec; rec = rec->m_next)
        {
            if (rec->m_uncasedHash == uncasedHash && len == rec->m_stringLen)
            {
                // We're now looking for a case-insensitive authority, so do an case-insensitive check.
                if (uncasedEqual(str, rec->m_string, len))
                {
                    if (rec->m_authority)
                    {
                        CARB_ASSERT(!authority); // Should only be one.
                        authority = rec;
                        if (uncased)
                        {
                            // Unlikely case, but the one we wanted was added by another thread since we didn't find it
                            // earlier when not under the lock.
                            return rec->m_stringId;
                        }
                    }

                    if (!uncased && casedEqual(str, rec->m_string, len))
                    {
                        // Unlikely case, but the exact match we wanted was added by another thread since we didn't find
                        // it earlier when not under the lock.
                        return rec->m_stringId;
                    }
                }
            }
        }

        Rec** ppRec;
        size_t index = size_t(enumVal);
        if (enumVal == kNoHint)
        {
            index = m_data->nextIndex++;

            // Make sure that we have the chunklist for this index
            size_t chunkList = index / m_data->EntriesPerChunk;
            CARB_FATAL_UNLESS(chunkList < m_data->ChunkListSize, "Too many registered strings!");

            if (!m_data->chunkLists[chunkList])
            {
                // Allocate a chunk list
                const static size_t kAllocSize = sizeof(Chunk) * m_data->EntriesPerChunk;
                m_data->chunkLists[chunkList] = static_cast<ChunkList>(sysAlloc(kAllocSize));
                CARB_FATAL_UNLESS(m_data->chunkLists[chunkList], "Failed to allocate ChunkList!");

                notifyMemory(m_data->chunkLists[chunkList], kAllocSize);
            }

            size_t chunkListOffset = index % m_data->EntriesPerChunk;
            ppRec = &m_data->chunkLists[chunkList][chunkListOffset];
        }
        else
        {
            // Static strings always fit within the first chunk (static_assert'd above).
            ppRec = &m_data->chunkLists[0][index];
        }

        // Figure out how much space we need for the string. Rec ends with a zero-length string so allocate extra space
        // for the string and NUL terminator and round up to Rec's alignment.
        size_t sizeNeeded = sizeof(Rec) + len + 1;
        // Round up to Rec alignment
        sizeNeeded = (sizeNeeded + (alignof(Rec) - 1)) & -ptrdiff_t(alignof(Rec));

        // Can we fit in the current memory block? If not, we need to allocate a new block. Unfortunately, this means
        // that the bit of memory remaining in the current memory block is unused.
        if (size_t(m_data->memEnd - m_data->mem) < sizeNeeded)
        {
            CARB_FATAL_UNLESS(sizeNeeded < (m_data->AllocSize - sizeof(MemoryAlloc)),
                              "Trying to register massive string of size %zu!", len);
            uint8_t* bytes = static_cast<uint8_t*>(sysAlloc(m_data->AllocSize));
            CARB_FATAL_UNLESS(bytes, "Memory allocation failed");
            notifyMemory(bytes, m_data->AllocSize);
            m_data->memEnd = bytes + m_data->AllocSize;
            m_data->allocList = new (bytes) MemoryAlloc{ m_data->allocList, m_data->AllocSize };
            bytes += sizeof(MemoryAlloc);
            m_data->mem = bytes;
        }

        // If we don't have an existing authority, we want this new Rec to be the uncased authority.
        *ppRec = new (m_data->mem) Rec(pBucketHead, uint32_t(index), uint32_t(len), !authority, uncasedHash, str);
        m_data->mem += sizeNeeded;
        CARB_ASSERT(m_data->mem <= m_data->memEnd);

        // Add to the hash table. Do this with an atomic op even though we're under lock because other threads can be
        // walking the hash table without the lock. This operation synchronizes-with findHashTable().
        carb::cpp20::atomic_ref<Rec*>(pBucketHead).store(*ppRec, std::memory_order_release);
        return (*ppRec)->m_stringId;
    }

    // These members may not change and are tied to versioned::Version as they can be changed by other instances of
    // Internal in other modules.
    uint8_t m_version{ versioned::Version }; // 0-1
    bool m_initializedByMe{ false }; // 1-2
    uint8_t m_unused[6]{}; // 2-8
    Internals* m_next{ nullptr }; // 8-16
    Internals* m_prev{ nullptr }; // 16-24
    VisualizerType* m_visualizer{ const_cast<VisualizerType*>(&Visualizer) }; // 24-32
    // New versioned members must be added here.

    // These members can vary
    carb::extras::SharedMemory m_shm;
    std::unique_ptr<carb::extras::SharedMemory::View> m_view;
    Data* m_data;
};

#pragma pack(pop)

} // namespace rstring

} // namespace detail

} // namespace carb
