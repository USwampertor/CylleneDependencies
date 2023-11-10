// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
// Implements a thread-safe (if used as directed) ring-buffer that can be used to store objects of various types and
// sizes. An age-old problem of ring-buffers is that they must copy the data in and out because data may wrap around and
// therefore not be contiguous. This implementation gets around that issue by using virtual memory to map the same page
// adjacently in memory. This uses double the address space without double the memory and allows pointers to be returned
// to the caller that automatically wrap around.
#pragma once

#include "../Defines.h"

#include "../CarbWindows.h"
#include "../cpp20/Atomic.h"
#include "../extras/Errors.h"
#include "../thread/Util.h"

#include <atomic>
#include <memory>
#include <utility>

#if CARB_POSIX
#    include <sys/mman.h>
#    include <sys/stat.h>

#    include <fcntl.h>
#    include <unistd.h>
#endif

#if CARB_PLATFORM_MACOS
#    include <sys/posix_shm.h>
#    define ftruncate64 ftruncate
#endif

namespace carb
{

namespace container
{

namespace detail
{

#if CARB_PLATFORM_WINDOWS
// RingBufferAllocator performs the memory-mapping trick listed above. First the requested size is rounded up to the
// system allocation granularity, then we search the address space for a place where we can map that much memory to two
// adjacent addresses.
class RingBufferAllocator
{
public:
    uint8_t* Allocate(size_t& size)
    {
        if (!size)
            size = 1;

        // Get the allocation granularity
        CARBWIN_SYSTEM_INFO info;
        memset(&info, 0, sizeof(info));
        ::GetSystemInfo(reinterpret_cast<SYSTEM_INFO*>(&info));

        // Round up to allocation granularity
        size += (info.dwAllocationGranularity - 1);
        size &= ~size_t(info.dwAllocationGranularity - 1);

        HANDLE mapping = ::CreateFileMappingW(
            CARBWIN_INVALID_HANDLE_VALUE, nullptr, CARBWIN_PAGE_READWRITE, (DWORD)(size >> 32), (DWORD)size, nullptr);
        CARB_FATAL_UNLESS(
            mapping != nullptr, "CreateFileMapping failed: %s", carb::extras::getLastWinApiErrorMessage().c_str());

        // Map to two adjacent pages so that writes across the boundary will wrap around automatically.
        for (;;)
        {
            // Try to reserve a block of memory large enough for both mappings
            uint8_t* search = (uint8_t*)::VirtualAlloc(nullptr, size * 2, CARBWIN_MEM_RESERVE, CARBWIN_PAGE_READWRITE);
            CARB_FATAL_UNLESS(search != nullptr, "Failed to find a mapping location");
            ::VirtualFree(search, 0, CARBWIN_MEM_RELEASE);

            uint8_t* where = (uint8_t*)::MapViewOfFileEx(mapping, CARBWIN_FILE_MAP_ALL_ACCESS, 0, 0, size, search);
            DWORD err1 = ::GetLastError();
            CARB_FATAL_UNLESS(where || search, "MapViewOfFileEx failed to find starting location: %s",
                              carb::extras::getLastWinApiErrorMessage().c_str());
            if (!where)
            {
                // Failed to map here; continue the search
                continue;
            }
            uint8_t* where2 = (uint8_t*)::MapViewOfFileEx(mapping, CARBWIN_FILE_MAP_ALL_ACCESS, 0, 0, size, where + size);
            DWORD err2 = ::GetLastError();
            CARB_FATAL_UNLESS(where2 == nullptr || where2 == (where + size),
                              "MapViewOfFileEx returned unexpected value: %s",
                              carb::extras::getLastWinApiErrorMessage().c_str());
            if (where2)
            {
                // We can close the mapping handle without affecting the mappings
                ::CloseHandle(mapping);
                return where;
            }

            // Failed to map in the expected location; unmap and try again
            ::UnmapViewOfFile(where);
            search = where + info.dwAllocationGranularity;
            CARB_FATAL_UNLESS(search < info.lpMaximumApplicationAddress, "Failed to find a mapping location");
            CARB_UNUSED(err1, err2);
        }
    }
    void Free(uint8_t* mem, size_t size)
    {
        ::UnmapViewOfFile(mem);
        ::UnmapViewOfFile(mem + size);
    }
};
#elif CARB_POSIX
class RingBufferAllocator
{
public:
    uint8_t* Allocate(size_t& size)
    {
        if (!size)
            size = 1;

        int const granularity = getpagesize();

        // Round up to allocation granularity
        size = (size + granularity - 1) & -(ptrdiff_t)granularity;

        // Create a memory "file" and size to the requested size

#    if 0
        // memfd_create would be the preferable way of doing this, but it doesn't appear to be available
        int fd = ::memfd_create("ringbuffer", 0u);
        CARB_FATAL_UNLESS(fd != -1, "memfd_create failed: %d/%s", errno, strerror(errno));
#    else

        // Fall back to creating a shared memory object. Linux doesn't appear to have a way to do this anonymously like
        // Windows does.
        char buffer[128];
#        if CARB_PLATFORM_MACOS
        // Mac OS limits the SHM name to PSHMNAMLEN.
        // POSIX seems to limit this to PATH_MAX.
        snprintf(buffer, CARB_MIN(PSHMNAMLEN + 1, sizeof(buffer)), "/carb-ring-%d-%p", getpid(), this);
#        else
        snprintf(buffer, CARB_MIN(PATH_MAX + 1, sizeof(buffer)), "/carb-ringbuffer-%d-%p", getpid(), this);
#        endif

        int fd = shm_open(buffer, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        CARB_FATAL_UNLESS(fd != -1, "shm_open failed: %d/%s", errno, strerror(errno));

        shm_unlink(buffer);
#    endif

        CARB_FATAL_UNLESS(ftruncate64(fd, size) != -1, "ftruncate failed: %d/%s", errno, strerror(errno));

        // Map to two adjacent pages so that writes across the boundary will wrap around automatically.

        // Try to map twice the address space and then re-map the top portion first
        uint8_t* search = (uint8_t*)mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (search != MAP_FAILED)
        {
            // Unmap the top half
            munmap(search + size, size);

            // Re-map as the same page as bottom
            uint8_t* mmap2 = (uint8_t*)mmap(search + size, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (mmap2 == (search + size))
            {
                // Success!
                // We no longer need the file descriptor around since the mmaps survive without it
                close(fd);
                return search;
            }

            // Failed or didn't take the hint for some reason
            if (mmap2 != MAP_FAILED)
                munmap(mmap2, size);

            munmap(search, size);
        }
        else
        {
            search = nullptr;
        }

        // Fall-back to search mode

        for (;;)
        {
            uint8_t* where = (uint8_t*)mmap(search, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            CARB_FATAL_UNLESS(
                where != MAP_FAILED || search, "mmap failed to find starting location: %d/%s", errno, strerror(errno));
            if (where == MAP_FAILED || (search && where != search))
            {
                // Failed to map here; continue the search
                CARB_FATAL_UNLESS(!where || munmap(where, size) == 0, "munmap failed: %d/%s", errno, strerror(errno));
                search += granularity;
                continue;
            }
            uint8_t* where2 = (uint8_t*)mmap(where + size, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (where2 != MAP_FAILED)
            {
                if (where2 == (where + size))
                {
                    // We can close the file descriptor without affecting the mappings
                    close(fd);
                    return where;
                }
                // Got a response, but not where we asked for
                CARB_FATAL_UNLESS(munmap(where2, size) == 0, "munmap failed: %d/%s", errno, strerror(errno));
                where2 = nullptr;
            }

            // Failed to map in the expected location. Unmap the first and try again.
            CARB_FATAL_UNLESS(munmap(where, size) == 0, "munmap failed: %d/%s", errno, strerror(errno));
            search = where + granularity;
            CARB_FATAL_UNLESS(search >= where, "Failed to find a mapping location");
        }
    }
    void Free(uint8_t* mem, size_t size)
    {
        munmap(mem, size);
        munmap(mem + size, size);
    }
};
#else
CARB_UNSUPPORTED_PLATFORM();
#endif

} // namespace detail

class RingBuffer : private detail::RingBufferAllocator
{
public:
    /**
     * The guaranteed minimum alignment returned by alloc().
     */
    constexpr static size_t kMinAlignment = sizeof(size_t);

    /**
     * The maximum alignment that can be requested by alloc().
     */
    constexpr static size_t kMaxAlignment = 4096;

    /**
     * Constructs the RingBuffer
     *
     * @param memSize The requested size in bytes. This will be rounded up to the system's allocation granularity.
     */
    RingBuffer(size_t memSize);
    ~RingBuffer();

    /**
     * Returns the storage capacity of the RingBuffer.
     *
     * This size will be greater than or equal to the size passed to the RingBuffer() constructor. Not all of this space
     * is usable by the application as some is used for internal record-keeping.
     *
     * @return size in bytes of the storage area of the RingBuffer
     */
    size_t capacity() const;

    /**
     * Returns the approximate used space of the RingBuffer.
     *
     * This is approximate due to other threads potentially changing the RingBuffer while this function is being called.
     *
     * @return the approximate size in bytes available to be read
     */
    size_t approx_used() const;

    /**
     * Returns the approximate available space of the RingBuffer.
     *
     * This is approximate due to other threads potentially changing the RingBuffer while this function is being called.
     * Also, some memory is used for record-keeping, so not all of this space is available to the application.
     *
     * @return the approximate size in bytes available to be written
     */
    size_t approx_available() const;

    /**
     * Allocates the requested size from the RingBuffer.
     *
     * The returned pointer is not available to be read() from the RingBuffer until commit() is called. If space is not
     * available, or the requested size exceed the total available memory of the RingBuffer, nullptr is returned.

     * @thread_safety may be called from multiple threads simultaneously.
     *
     * @param bytes The requested size in bytes to write to the RingBuffer
     * @param align The alignment required for the returned memory block. at least `sizeof(size_t)` alignment is
     * guaranteed. Must be a power of 2 and must not exceed \ref kMaxAlignment.
     * @return A pointer to memory of \ref bytes size to be written by the application. Use commit() to signal to the
     * RingBuffer that the memory is ready to be read(). nullptr is returned if the requested size is not available or
     * the requested alignment could not be granted.
     */
    void* alloc(size_t bytes, size_t align = 0);

    /**
     * Allocates the requested size from the RingBuffer, waiting until space is available.
     *
     * The returned pointer is not available to be read() from the RingBuffer until commit() is called. If space is not
     * available, \ref onWait is called and the function waits until space is available. If the requested size exceeds
     * the total available memory of the RingBuffer, or alignment is invalid, nullptr is returned. Note that in a
     * single-threaded environment, this function may deadlock.
     *
     * @thread_safety may be called from multiple threads simultaneously.
     *
     * @param bytes The requested size in bytes to write to the RingBuffer
     * @param onWait A function-like object that is called when waiting must occur
     * @param align The alignment required for the returned memory block. at least `sizeof(size_t)` alignment is
     * guaranteed. Must be a power of 2 and must not exceed \ref kMaxAlignment.
     * @return A pointer to memory of \ref bytes size to be written by the application. Use commit() to signal to the
     * RingBuffer that the memory is ready to be read(). nullptr is returned if the requested size exceeds the total
     * RingBuffer size or the requested alignment could not be granted.
     */
    template <class Func>
    void* allocWait(size_t bytes, Func&& onWait, size_t align = 0);

    /**
     * Commits memory returned by alloc() or allocWait() and makes it available to read().
     *
     * This must be called on every block of memory returned from alloc() and allocWait(), otherwise subsequent read()
     * operations will fail.
     *
     * @thread_safety may be called from multiple threads simultaneously.
     *
     * @param where The memory returned from alloc() that is ready to be committed.
     */
    void commit(void* where);

    /**
     * Peeks at next value from the RingBuffer without removing it, and calls \ref f() with the value.
     *
     * @thread_safety may be called from multiple threads simultaneously, and may be called while other threads are
     * writing via alloc() and commit(). May NOT be called while read() is called from another thread.
     *
     * @param f must be a function-like object with signature void(void* memory, size_t size) where memory is a pointer
     * to the memory block that was returned from alloc() and size is the size that was passed to alloc().
     * @return true if a value was successfully peeked and \ref f was called. false if no values are available to be
     * peeked or the next value has not been committed by commit() yet.
     */
    template <class Func>
    bool peek(Func&& f) const;

    /**
     * Reads a value from the RingBuffer and calls \ref f() with the value.
     *
     * @thread_safety may NOT be called from multiple threads simultaneously. Can be called while other threads are
     * writing via alloc() and commit(). Must also be serialized with peek().
     *
     * @param f must be a function-like object with signature void(void* memory, size_t size) where memory is a pointer
     * to the memory block that was returned from alloc() and size is the size that was passed to alloc().
     * @return true if a value was successfully read and \ref f was called. false if no values are available to be read
     * or the next value has not been committed by commit() yet.
     */
    template <class Func>
    bool read(Func&& f);

    /**
     * Reads all values from the RingBuffer and calls \ref f() on each value. Causes less contention with alloc() than
     * repeated read() calls because shared pointers are only updated at the end, but this also means that space in the
     * RingBuffer only becomes available right before readAll() returns.
     *
     * @thread_safety may NOT be called from multiple threads simultaneously. Can be called while other threads are
     * writing via alloc() and commit(). Must also be serialized with peek() and read().
     *
     * @param f must be a function-like object with signature bool(void* memory, size_t size) where memory is a pointer
     * to the memory block that was returned from alloc() and `size` is the size that was passed to alloc(). If \ref f
     * returns true, then the value is consumed and \ref f is called with the next value if one exists. If \ref f
     * returns false, then the value is not consumed (similar to peek()) and readAll() terminates.
     * @returns the count of times that \ref f returns true. If the next value to read is not committed, or no values
     * are available for read, 0 is returned.
     */
    template <class Func>
    size_t readAll(Func&& f);

    /**
     * Reads a value from the RingBuffer by copying it into the given buffer.
     *
     * @thread_safety may be called from multiple threads simultaneously. Can be called while other threads are writing
     * via alloc() and commit(). When multiple threads are reading simultaneously, a larger value (than \ref bufferSize)
     * may be spuriously returned and should be ignored by the application.
     *
     * When a single thread is calling readCopy(), \ref bufferSize can be 0 to determine the size of the next value to
     * be read. With multiple threads calling readCopy() another thread could read the value.
     *
     * @param buffer The memory buffer to write to.
     * @param bufferSize The size of \ref buffer and the maximum size of an object that can be read.
     * @return 0 if no values are available to be read or the next value hasn't been committed by commit() yet. If
     *  the return value is less-than or equal-to bufferSize, the next value has been read and copied into \ref buffer.
     *  If the return value is greater than bufferSize, the value has NOT been read and the return value indicates the
     *  minimum size of \ref bufferSize required to read the next object.
     */
    size_t readCopy(void* buffer, size_t bufferSize);

    /**
     * Returns if \ref mem belongs to the RingBuffer.
     *
     * @param mem The memory block to check.
     * @return true if \ref mem is owned by the RingBuffer; false otherwise.
     */
    bool isOwned(void* mem) const;

    /**
     * Helper function to write the given object to the RingBuffer.
     *
     * @thread_safety may be called from multiple threads simultaneously.
     *
     * @param t The object to write. This parameter is forwarded to the constructor of T.
     * @return true if the object was written; false if space could not be allocated for the object.
     */
    template <class T>
    bool writeObject(T&& t);

    /**
     * Helper function to write the given object to the RingBuffer, waiting until space is available to write.
     *
     * Note that in a single-threaded environment, this function may deadlock.
     *
     * @thread_safety may be called from multiple threads simultaneously.
     *
     * @param t The object to write. This parameter is forwarded to the constructor of T.
     * @param onWait The function-like object to call when waiting must occur.
     * @return true if the object was written; false if space could not be allocated for the object.
     */
    template <class T, class Func>
    bool writeObjectWait(T&& t, Func&& onWait);

    /**
     * Helper function to read an object from the RingBuffer.
     *
     * @thread_safety may NOT be called from multiple threads simultaneously. Can be called while other threads are
     * writing via alloc() and commit().
     *
     * @param t The object to populate (via std::move) with the next object in the RingBuffer.
     * @return true if the object was read; false if no object was available to read
     */
    template <class T>
    bool readObject(T& t);

    /**
     * Waits until data has been allocated from the RingBuffer. This is not as useful as waiting for committed data
     * (since data may not yet be available to read), but can be done simultaneously by multiple threads.
     *
     * @thread_safety may be called from multiple threads simultaneously.
     */
    void waitForAllocatedData();

    /**
     * Waits until data has been committed to the RingBuffer that is ready to be read.
     *
     * @thread_safety may NOT be called from multiple threads simultaneously. Can be called while other threads are
     * writing via alloc() and commit(). Use waitForAllocatedData() if multiple threads are necessary.
     */
    void waitForCommittedData();

private:
    struct Header
    {
        constexpr static uint32_t kCommitted = uint32_t(1) << 0;
        constexpr static uint32_t kPadding = uint32_t(1) << 1;

        carb::cpp20::atomic_uint32_t bytes;
        std::uint32_t requestedBytes;

        Header(uint32_t bytes_, uint32_t requestedBytes_, uint32_t flags = 0)
            : bytes(bytes_ | flags), requestedBytes(requestedBytes_)
        {
            CARB_ASSERT((bytes_ & (kCommitted | kPadding)) == 0);
        }
    };

    size_t constrain(size_t val) const
    {
        return val & (m_memorySize - 1);
    }

    template <class T>
    static T* alignForward(T* where, size_t align)
    {
        return reinterpret_cast<T*>((reinterpret_cast<size_t>(where) + align - 1) & -(ptrdiff_t)align);
    }

    static bool isPowerOf2(size_t val)
    {
        return (val & (val - 1)) == 0;
    }

    constexpr static size_t kCacheLineSize = 64;

    uint8_t* const m_memory;
    size_t const m_memorySize;
    carb::cpp20::atomic_size_t m_readPtr{ 0 };

    // Pad so that the write head/tail members are in a separate cache line
    size_t padding1[(kCacheLineSize - sizeof(uint8_t*) - sizeof(size_t) - sizeof(carb::cpp20::atomic_size_t)) /
                    sizeof(size_t)];

    // Use a two-phased write approach. The stable condition is where m_writeHead and m_writeTail are equal. However,
    // during alloc(), m_writeHead is moved first and the space between m_writeHead and m_writeTail is in flux and can
    // not be read. Once alloc() has written everything that it needs to, m_writeTail catches up to m_writeHead and the
    // RingBuffer is once again stable.
    carb::cpp20::atomic_size_t m_writeHead{ 0 };
    carb::cpp20::atomic_size_t m_writeTail{ 0 };

    // Pad out to a separate cache line
    size_t padding2[(kCacheLineSize - (2 * sizeof(carb::cpp20::atomic_size_t))) / sizeof(size_t)];
};

inline RingBuffer::RingBuffer(size_t memSize)
    : m_memory(detail::RingBufferAllocator::Allocate(memSize)), m_memorySize(memSize)
{
    CARB_UNUSED(padding1);
    CARB_UNUSED(padding2);
    static_assert(alignof(Header) <= kMinAlignment, "Invalid alignment assumption");
#if CARB_DEBUG
    // Test rollover
    m_readPtr = m_writeHead = m_writeTail = size_t(-ptrdiff_t(m_memorySize));
#endif
}

inline RingBuffer::~RingBuffer()
{
    // Nothing should be remaining in the ring buffer at destruction time
    CARB_ASSERT(approx_used() == 0);
    detail::RingBufferAllocator::Free(m_memory, m_memorySize);
}

inline size_t RingBuffer::capacity() const
{
    return m_memorySize;
}

inline size_t RingBuffer::approx_used() const
{
    size_t write = m_writeTail.load(std::memory_order_relaxed);
    size_t read = m_readPtr.load(std::memory_order_relaxed);
    ptrdiff_t diff = (write - read);
    return diff < 0 ? 0 : diff > ptrdiff_t(m_memorySize) ? m_memorySize : size_t(diff);
}

inline size_t RingBuffer::approx_available() const
{
    return capacity() - approx_used();
}

inline void* RingBuffer::alloc(size_t bytes, size_t align)
{
    // Will never have enough memory to allocate this
    if (bytes == 0 || (bytes + align) > (m_memorySize - sizeof(Header)))
        return nullptr;

    // Alignment greater than max is not allowed
    if (align > kMaxAlignment || !isPowerOf2(align))
    {
        return nullptr;
    }

    // Make sure bytes is aligned at least to kMinAlignment.
    size_t requestedBytes = bytes;
    bytes = (bytes + kMinAlignment - 1) & -(ptrdiff_t)kMinAlignment;
    if (align <= kMinAlignment)
        align = kMinAlignment;

    size_t const baseNeeded = bytes + sizeof(Header);

    size_t writeHead = m_writeHead.load(std::memory_order_acquire);
    size_t readPtr = m_readPtr.load(std::memory_order_acquire);

    uint8_t* currentMem;
    ptrdiff_t padding;
    size_t needed;

    for (;;)
    {
        currentMem = m_memory + constrain(writeHead) + sizeof(Header);
        padding = alignForward(currentMem, align) - currentMem;
        CARB_ASSERT((padding & ptrdiff_t(sizeof(Header) - 1)) == 0); // Must be aligned to sizeof(Header)
        needed = baseNeeded + padding;

        // Check if we have enough available to satisfy the request
        if (ptrdiff_t(needed) > ptrdiff_t(capacity() - (writeHead - readPtr)))
        {
            return nullptr;
        }

        // Update the pointer
        if (CARB_LIKELY(m_writeHead.compare_exchange_strong(
                writeHead, writeHead + needed, std::memory_order_acquire, std::memory_order_relaxed)))
            break;

        // Failed; writeHead was updated by the failed compare_exchange; refresh readPtr
        readPtr = m_readPtr.load(std::memory_order_acquire);
    }

    if (padding)
    {
        // Create the padding space Header if necessary
        new (currentMem - sizeof(Header)) Header(uint32_t(padding - sizeof(Header)), uint32_t(padding - sizeof(Header)),
                                                 Header::kPadding | Header::kCommitted);
        currentMem += padding;
    }

    // Create the header (with the kCommitted bit zero)
    new (currentMem - sizeof(Header)) Header(uint32_t(bytes), uint32_t(requestedBytes));

    // We have to wait until m_writeTail becomes our writeHead. This allows sequential writes to be properly ordered.
    size_t tail = m_writeTail.load(std::memory_order_relaxed);
    while (tail != writeHead)
    {
        m_writeTail.wait(tail, std::memory_order_relaxed);
        tail = m_writeTail.load(std::memory_order_relaxed);
    }
    m_writeTail.store(writeHead + needed, std::memory_order_release);
    m_writeTail.notify_all();

    return currentMem;
}

template <class Func>
inline void* RingBuffer::allocWait(size_t bytes, Func&& onWait, size_t align)
{
    // Will never have enough memory to allocate this
    if (bytes == 0 || (bytes + align) > (m_memorySize - sizeof(Header)))
    {
        return nullptr;
    }

    // Alignment greater than max is not allowed
    if (align > kMaxAlignment || !isPowerOf2(align))
    {
        CARB_ASSERT(0);
        return nullptr;
    }

    // Make sure bytes is aligned at least to kMinAlignment.
    size_t requestedBytes = bytes;
    bytes = (bytes + kMinAlignment - 1) & -(ptrdiff_t)kMinAlignment;

    size_t const baseNeeded = bytes + sizeof(Header);

    size_t writeHead;

    uint8_t* const baseMem = m_memory;
    uint8_t* currentMem;
    ptrdiff_t padding;
    size_t needed;

    if (align <= kMinAlignment)
    {
        // We don't need any padding, so just increment the pointer
        needed = baseNeeded;
        padding = 0;
        writeHead = m_writeHead.fetch_add(needed, std::memory_order_acquire);
        currentMem = baseMem + constrain(writeHead) + sizeof(Header);
    }
    else
    {
        // When specific alignment is required, we can't blindly increment the writeHead pointer, because the amount of
        // padding necessary is dependent on the current value of writeHead.
        writeHead = m_writeHead.load(std::memory_order_acquire);
        for (;;)
        {
            currentMem = baseMem + constrain(writeHead) + sizeof(Header);
            padding = alignForward(currentMem, align) - currentMem;
            CARB_ASSERT((padding & ptrdiff_t(sizeof(Header) - 1)) == 0); // Must be aligned to sizeof(Header)
            needed = baseNeeded + padding;

            // Update the pointer
            if (CARB_LIKELY(m_writeHead.compare_exchange_strong(
                    writeHead, writeHead + needed, std::memory_order_acquire, std::memory_order_relaxed)))
            {
                break;
            }

            // Failed; writeHead was updated by the failed compare_exchange. Loop and try again
        }
    }

    // If necessary, block until we have space to write
    size_t readPtr = m_readPtr.load(std::memory_order_acquire);
    if (CARB_UNLIKELY(ptrdiff_t(writeHead + needed - readPtr) > ptrdiff_t(m_memorySize)))
    {
        // We don't currently have capacity, so we need to wait
        onWait();

        // Wait for memory to be available
        do
        {
            m_readPtr.wait(readPtr, std::memory_order_relaxed);
            readPtr = m_readPtr.load(std::memory_order_relaxed);
        } while (ptrdiff_t(writeHead + needed - readPtr) > ptrdiff_t(m_memorySize));
    }

    if (padding)
    {
        // Create the padding space Header if necessary
        new (currentMem - sizeof(Header)) Header(uint32_t(padding - sizeof(Header)), uint32_t(padding - sizeof(Header)),
                                                 Header::kPadding | Header::kCommitted);
        currentMem += padding;
    }

    // Create the header (with the kCommitted bit zero)
    new (currentMem - sizeof(Header)) Header(uint32_t(bytes), uint32_t(requestedBytes));

    // We have to wait until m_writeTail becomes our writeHead. This allows sequential writes to be properly ordered.
    size_t tail = m_writeTail.load(std::memory_order_relaxed);
    while (tail != writeHead)
    {
        m_writeTail.wait(tail, std::memory_order_relaxed);
        tail = m_writeTail.load(std::memory_order_relaxed);
    }
    m_writeTail.store(writeHead + needed, std::memory_order_release);
    m_writeTail.notify_all();

    return currentMem;
}

inline void RingBuffer::commit(void* mem)
{
    CARB_ASSERT(isOwned(mem));

    Header* header = reinterpret_cast<Header*>(reinterpret_cast<uint8_t*>(mem) - sizeof(Header));
    uint32_t result = header->bytes.fetch_or(Header::kCommitted, std::memory_order_release);
    header->bytes.notify_one(); // notify the waiter in waitForCommittedData()
    CARB_ASSERT(!(result & Header::kCommitted)); // Shouldn't already be committed
    CARB_UNUSED(result);
}

template <class Func>
inline bool RingBuffer::peek(Func&& f) const
{
    size_t readPtr = m_readPtr.load(std::memory_order_acquire);
    size_t writePtr = m_writeTail.load(std::memory_order_acquire);

    for (;;)
    {
        // Any bytes to read?
        if (ptrdiff_t(writePtr - readPtr) <= 0)
            return false;

        uint8_t* offset = m_memory + constrain(readPtr);
        Header* header = reinterpret_cast<Header*>(offset);

        // Check if the next header has been committed
        uint32_t bytes = header->bytes.load(std::memory_order_acquire);
        if (!!(bytes & Header::kPadding))
        {
            // For padding, we're just going to skip it and look ahead
            CARB_ASSERT(!!(bytes & Header::kCommitted)); // Should be committed
            bytes &= ~(Header::kPadding | Header::kCommitted);
            readPtr += (bytes + sizeof(Header));
            continue;
        }

        // Must be committed
        if (!(bytes & Header::kCommitted))
            return false;

        bytes &= ~Header::kCommitted;

        // This may also indicate multiple threads calling read() simultaneously which is not allowed
        CARB_FATAL_UNLESS(
            (bytes + sizeof(Header)) <= (writePtr - readPtr), "RingBuffer internal error or memory corruption");

        // Call the function to handle the data
        f(offset + sizeof(Header), header->requestedBytes);

        return true;
    }
}

template <class Func>
inline bool RingBuffer::read(Func&& f)
{
    size_t readPtr = m_readPtr.load(std::memory_order_acquire);
    size_t writePtr = m_writeTail.load(std::memory_order_acquire);

    for (;;)
    {
        // Any bytes to read?
        if (ptrdiff_t(writePtr - readPtr) <= 0)
            return false;

        uint8_t* offset = m_memory + constrain(readPtr);
        Header* header = reinterpret_cast<Header*>(offset);

        // Check if the next header has been committed
        size_t bytes = header->bytes.load(std::memory_order_acquire);
        if (!!(bytes & Header::kPadding))
        {
            // Try to skip padding
            CARB_ASSERT(!!(bytes & Header::kCommitted)); // Should be committed
            bytes &= ~(Header::kPadding | Header::kCommitted);
            CARB_FATAL_UNLESS(m_readPtr.exchange(readPtr + bytes + sizeof(Header), std::memory_order_release) == readPtr,
                              "RingBuffer::read is not thread-safe; call from only one thread or use readCopy()");
            m_readPtr.notify_all();
            readPtr += (bytes + sizeof(Header));
            continue;
        }

        // Must be committed
        if (!(bytes & Header::kCommitted))
            return false;

        bytes &= ~Header::kCommitted;

        // This may also indicate multiple threads calling read() simultaneously which is not allowed
        CARB_FATAL_UNLESS(
            (bytes + sizeof(Header)) <= (writePtr - readPtr), "RingBuffer internal error or memory corruption");

        // Call the function to handle the data
        f(offset + sizeof(Header), header->requestedBytes);

        // Move the read pointer
        CARB_FATAL_UNLESS(m_readPtr.exchange(readPtr + bytes + sizeof(Header), std::memory_order_release) == readPtr,
                          "RingBuffer::read is not thread-safe; call from only one thread or use readCopy()");
        m_readPtr.notify_all();
        return true;
    }
}

template <class Func>
inline size_t RingBuffer::readAll(Func&& f)
{
    size_t origReadPtr = m_readPtr.load(std::memory_order_acquire);
    size_t writePtr = m_writeTail.load(std::memory_order_acquire);

    size_t count = 0;
    size_t readPtr = origReadPtr;

    for (;;)
    {
        // Any bytes to read?
        if (ptrdiff_t(writePtr - readPtr) <= 0)
        {
            break;
        }

        uint8_t* offset = m_memory + constrain(readPtr);
        Header* header = reinterpret_cast<Header*>(offset);

        // Check if the next header has been committed
        size_t bytes = header->bytes.load(std::memory_order_acquire);
        if (!!(bytes & Header::kPadding))
        {
            // Try to skip padding
            CARB_ASSERT(!!(bytes & Header::kCommitted)); // Should be committed
            bytes &= ~(Header::kPadding | Header::kCommitted);
            readPtr += (bytes + sizeof(Header));
            continue;
        }

        // Terminate iteration if we encounter a non-committed value
        if (!(bytes & Header::kCommitted))
            break;

        bytes &= ~Header::kCommitted;

        // This may also indicate multiple threads calling read() simultaneously which is not allowed
        CARB_FATAL_UNLESS(
            (bytes + sizeof(Header)) <= (writePtr - readPtr), "RingBuffer internal error or memory corruption");

        // Call the function to handle the data
        if (!f(offset + sizeof(Header), header->requestedBytes))
            break;

        ++count;

        // Advance the read pointer
        readPtr += (bytes + sizeof(Header));
    }

    if (readPtr != origReadPtr)
    {
        CARB_FATAL_UNLESS(m_readPtr.exchange(readPtr, std::memory_order_release) == origReadPtr,
                          "RingBuffer::readAll is not thread-safe; call from only one thread or use readCopy()");
        m_readPtr.notify_all();
    }
    return count;
}

inline size_t RingBuffer::readCopy(void* buffer, size_t bufSize)
{
    size_t readPtr = m_readPtr.load(std::memory_order_acquire);

    for (;;)
    {
        size_t writePtr = m_writeTail.load(std::memory_order_acquire);

        // Any bytes to read?
        if (ptrdiff_t(writePtr - readPtr) <= 0)
            return 0;

        uint8_t* offset = m_memory + constrain(readPtr);
        Header* header = reinterpret_cast<Header*>(offset);

        // Check if the next header has been committed
        size_t bytes = header->bytes.load(std::memory_order_acquire);
        if (!!(bytes & Header::kPadding))
        {
            // Try to skip padding
            CARB_ASSERT(!!(bytes & Header::kCommitted)); // Padding is always committed
            bytes &= ~(Header::kPadding | Header::kCommitted);

            // If we fail the compare_exchange, readPtr is re-loaded and we try again.
            if (CARB_LIKELY(m_readPtr.compare_exchange_strong(
                    readPtr, readPtr + sizeof(Header) + bytes, std::memory_order_release, std::memory_order_relaxed)))
                readPtr += (sizeof(Header) + bytes);
            m_readPtr.notify_all();
            continue;
        }

        // Must be committed
        if (!(bytes & Header::kCommitted))
            return 0;

        bytes &= ~Header::kCommitted;

        if ((bytes + sizeof(Header)) > (writePtr - readPtr))
        {
            // This *may* happen if another thread has advanced the read pointer, and another thread has written to the
            // block of data that we're currently trying to read from. It is incredibly rare, but we should refresh our
            // pointers and try again.
            readPtr = m_readPtr.load(std::memory_order_acquire);
            continue;
        }

        // Check if we have enough space to write. If not, return the size needed. NOTE: for the reasons listed in the
        // above comment, this may spuriously return.
        if (header->requestedBytes > bufSize)
            return header->requestedBytes;

        // Copy the data
        memcpy(buffer, offset + sizeof(Header), header->requestedBytes);

        // If we fail the compare_exchange, readPtr is re-loaded and we try again.
        if (CARB_LIKELY(m_readPtr.compare_exchange_strong(
                readPtr, readPtr + sizeof(Header) + bytes, std::memory_order_release, std::memory_order_relaxed)))
        {
            m_readPtr.notify_all();
            return header->requestedBytes;
        }
    }
}

inline bool RingBuffer::isOwned(void* mem) const
{
    return mem >= m_memory && mem < (m_memory + (2 * m_memorySize));
}

template <class T>
bool RingBuffer::writeObject(T&& obj)
{
    using Type = typename std::decay<T>::type;
    void* mem = this->alloc(sizeof(Type), alignof(Type));
    if (!mem)
        return false;

    new (mem) Type(std::forward<T>(obj));
    this->commit(mem);
    return true;
}

template <class T, class Func>
bool RingBuffer::writeObjectWait(T&& obj, Func&& onWait)
{
    using Type = typename std::decay<T>::type;
    void* mem = this->allocWait(sizeof(Type), std::forward<Func>(onWait), alignof(Type));
    if (!mem)
        return false;

    new (mem) Type(std::forward<T>(obj));
    this->commit(mem);
    return true;
}

template <class T>
bool RingBuffer::readObject(T& obj)
{
    return this->read([&obj](void* mem, size_t size) {
        using Type = typename std::decay<T>::type;
        CARB_UNUSED(size);
        CARB_ASSERT(size >= sizeof(Type));
        CARB_ASSERT((reinterpret_cast<size_t>(mem) & (alignof(Type) - 1)) == 0);
        Type* p = reinterpret_cast<Type*>(mem);
        obj = std::move(*p);
        p->~Type();
    });
}

inline void RingBuffer::waitForAllocatedData()
{
    size_t readPtr = m_readPtr.load(std::memory_order_acquire);
    size_t writePtr = m_writeTail.load(std::memory_order_acquire);

    while (ptrdiff_t(writePtr - readPtr) <= 0)
    {
        m_writeTail.wait(writePtr, std::memory_order_relaxed);
        writePtr = m_writeTail.load(std::memory_order_relaxed);
    }
}

inline void RingBuffer::waitForCommittedData()
{
    size_t readPtr = m_readPtr.load(std::memory_order_acquire);
    size_t writePtr = m_writeTail.load(std::memory_order_acquire);

    for (;;)
    {
        // Any bytes to read?
        if (ptrdiff_t(writePtr - readPtr) <= 0)
        {
            m_writeTail.wait(writePtr);
            writePtr = m_writeTail.load(std::memory_order_relaxed);
            continue;
        }

        uint8_t* offset = m_memory + constrain(readPtr);
        Header* header = reinterpret_cast<Header*>(offset);

        // Check if the next header has been committed
        uint32_t bytes = header->bytes.load(std::memory_order_acquire);
        if (!!(bytes & Header::kPadding))
        {
            // For padding, we're just going to skip it and look ahead
            CARB_ASSERT(!!(bytes & Header::kCommitted)); // Should be committed
            bytes &= ~(Header::kPadding | Header::kCommitted);

            // Skip the header
            readPtr += (bytes + sizeof(Header));
            offset = m_memory + constrain(readPtr);
            header = reinterpret_cast<Header*>(offset);
        }

        // Must be committed
        if (!(bytes & Header::kCommitted))
        {
            // Wait for commit
            header->bytes.wait(bytes, std::memory_order_relaxed);
        }

        // Bytes are now available
        return;
    }
}

} // namespace container

} // namespace carb
