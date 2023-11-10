// Copyright (c) 2018-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"

#include "FreeListAllocator.h"

#include <cstring>

namespace carb
{
namespace extras
{

/**
 * Defines arena allocator where allocations are made very quickly, but that deallocations
 * can only be performed in reverse order, or with the client code knowing a previous deallocation (say with
 * deallocateAllFrom), automatically deallocates everything after it.
 *
 * @note Though named "ArenaAllocator", this is really more of a block/sequential allocator, not an arena allocator. If
 * you desire to actually use an arena, see @ref carb::memory::ArenaAllocator. This class may be renamed in the future.
 *
 * It works by allocating large blocks and then cutting out smaller pieces as requested. If a piece of memory is
 * deallocated, it either MUST be in reverse allocation order OR the subsequent allocations are implicitly
 * deallocated too, and therefore accessing their memory is now undefined behavior. Allocations are made
 * contiguously from the current block. If there is no space in the current block, the
 * next block (which is unused) if available is checked. If that works, an allocation is made from the next block.
 * If not a new block is allocated that can hold at least the allocation with required alignment.
 *
 * All memory allocated can be deallocated very quickly and without a client having to track any memory.
 * All memory allocated will be freed on destruction - or with reset.
 *
 * A memory arena can have requests larger than the block size. When that happens they will just be allocated
 * from the heap. As such 'oversized blocks' are seen as unusual and potentially wasteful they are deallocated
 * when deallocateAll is called, whereas regular size blocks will remain allocated for fast subsequent allocation.
 *
 * It is intentional that blocks information is stored separately from the allocations that store the
 * user data. This is so that alignment permitting, block allocations sizes can be passed directly to underlying
 * allocator. For large power of 2 backing allocations this might mean a page/pages directly allocated by the OS for
 * example. Also means better cache coherency when traversing blocks -> as generally they will be contiguous in memory.
 *
 * Also note that allocateUnaligned can be used for slightly faster aligned allocations. All blocks allocated internally
 * are aligned to the blockAlignment passed to the constructor. If subsequent allocations (of any type) sizes are of
 * that alignment or larger then no alignment fixing is required (because allocations are contiguous) and so
 * 'allocateUnaligned' will return allocations of blockAlignment alignment.
 */
class ArenaAllocator
{
public:
    /**
     * The minimum alignment of the backing memory allocator.
     */
    static const size_t kMinAlignment = sizeof(void*);

    /**
     * Constructor.
     */
    ArenaAllocator()
    {
        // Mark as invalid so any alloc call will fail
        m_blockAlignment = 0;
        m_blockSize = 0;

        // Set up as empty
        m_blocks = nullptr;
        _setCurrentBlock(nullptr);
        m_blockFreeList.initialize(sizeof(Block), sizeof(void*), 16);
    }

    /**
     * Constructor.
     *
     * @param blockSize The minimum block size (in bytes).
     * @param blockAlignment The power 2 block alignment that all blocks must have.
     */
    explicit ArenaAllocator(size_t blockSize, size_t blockAlignment = kMinAlignment)
    {
        _initialize(blockSize, blockAlignment);
    }

    /**
     * ArenaAllocator is not copy-constructible.
     */
    ArenaAllocator(const ArenaAllocator& rhs) = delete;

    /**
     * ArenaAllocator is not copyable.
     */
    void operator=(const ArenaAllocator& rhs) = delete;

    /**
     * Destructor.
     */
    ~ArenaAllocator()
    {
        _deallocateBlocks();
    }

    /**
     * Initialize the arena with specified block size and alignment.
     *
     * If the arena has been previously initialized will free and deallocate all memory.
     *
     * @param blockSize The minimum block size (in bytes).
     * @param blockAlignment The power 2 block alignment that all blocks must have.
     */
    void initialize(size_t blockSize, size_t blockAlignment = kMinAlignment);

    /**
     * Determines if an allocation is consistent with an allocation from this arena.
     * The test cannot say definitively if this was such an allocation, because the exact details
     * of each allocation is not kept.
     *
     * @param alloc The start of the allocation
     * @param sizeInBytes The size of the allocation
     * @return true if allocation could have been from this Arena
     */
    bool isValid(const void* alloc, size_t sizeInBytes) const;

    /**
     * Allocate some memory of at least size bytes without having any specific alignment.
     *
     * Can be used for slightly faster *aligned* allocations if caveats in class description are met. The
     * Unaligned, means the method will not enforce alignment - but a client call to allocateUnaligned can control
     * subsequent allocations alignments via it's size.
     *
     * @param size The size of the allocation requested (in bytes and must be > 0).
     * @return The allocation. Can be nullptr if backing allocator was not able to request required memory
     */
    void* allocate(size_t size);

    /**
     * Allocate some aligned memory of at least size bytes
     *
     * @param size Size of allocation wanted (must be > 0).
     * @param alignment Alignment of allocation - must be a power of 2.
     * @return The allocation (or nullptr if unable to allocate). Will be at least 'alignment' alignment or better.
     */
    void* allocateAligned(size_t size, size_t alignment);

    /**
     * Allocates a null terminated string.
     *
     * @param str A null-terminated string
     * @return A copy of the string held on the arena
     */
    const char* allocateString(const char* str);

    /**
     * Allocates a null terminated string.
     *
     * @param chars Pointer to first character
     * @param charCount The amount of characters NOT including terminating 0.
     * @return A copy of the string held on the arena.
     */
    const char* allocateString(const char* chars, size_t charCount);

    /**
     * Allocate an element of the specified type.
     *
     * Note: Constructor for type is not executed.
     *
     * @return An allocation with correct alignment/size for the specified type.
     */
    template <typename T>
    T* allocate();

    /**
     * Allocate an array of a specified type.
     *
     * Note: Constructor for type is not executed.
     *
     * @param count The number of elements in the array
     * @return Returns an allocation with correct alignment/size for the specified type.
     */
    template <typename T>
    T* allocateArray(size_t count);

    /**
     * Allocate an array of a specified type.
     *
     * @param count The number of elements in the array
     * @return An allocation with correct alignment/size for the specified type. Contents is all zeroed.
     */
    template <typename T>
    T* allocateArray(size_t count, bool zeroMemory);

    /**
     * Allocate an array of a specified type, and copy array passed into it.
     *
     * @param arr. The array to copy from.
     * @param count. The number of elements in the array.
     * @return An allocation with correct alignment/size for the specified type. Contents is all zeroed.
     */
    template <typename T>
    T* allocateArrayAndCopy(const T* arr, size_t count);

    /**
     * Deallocate the last allocation.
     *
     * If data is not from the last allocation then the behavior is undefined.
     *
     * @param data The data allocation to free.
     */
    void deallocateLast(void* data);

    /**
     * Deallocate this allocation and all remaining after it.
     *
     * @param dataStart The data allocation to free from and all remaining.
     */
    void deallocateAllFrom(void* dataStart);

    /**
     * Deallocates all allocated memory. That backing memory will generally not be released so
     * subsequent allocation will be fast, and from the same memory. Note though that 'oversize' blocks
     * will be deallocated.
     */
    void deallocateAll();

    /**
     * Resets to the initial state when constructed (and all backing memory will be deallocated)
     */
    void reset();

    /**
     * Adjusts such that the next allocate will be at least to the block alignment.
     */
    void adjustToBlockAlignment();

    /**
     * Gets the block alignment that is passed at initialization otherwise 0  an invalid block alignment.
     *
     * @return the block alignment
     */
    size_t getBlockAlignment() const;

private:
    struct Block
    {
        Block* m_next;
        uint8_t* m_alloc;
        uint8_t* m_start;
        uint8_t* m_end;
    };

    void _initialize(size_t blockSize, size_t blockAlignment)
    {
        // Alignment must be a power of 2
        CARB_ASSERT(((blockAlignment - 1) & blockAlignment) == 0);

        // Must be at least sizeof(void*) in size, as that is the minimum the backing allocator will be
        blockAlignment = (blockAlignment < kMinAlignment) ? kMinAlignment : blockAlignment;

        // If alignment required is larger then the backing allocators then
        // make larger to ensure when alignment correction takes place it will be aligned
        if (blockAlignment > kMinAlignment)
        {
            blockSize += blockAlignment;
        }

        m_blockSize = blockSize;
        m_blockAlignment = blockAlignment;
        m_blocks = nullptr;
        _setCurrentBlock(nullptr);
        m_blockFreeList.initialize(sizeof(Block), sizeof(void*), 16);
    }

    void* _allocateAligned(size_t size, size_t alignment)
    {
        CARB_ASSERT(size);

        // Can't be space in the current block -> so we can either place in next, or in a new block
        _newCurrentBlock(size, alignment);
        uint8_t* const current = m_current;

        // If everything has gone to plan, must be space here...
        CARB_ASSERT(current + size <= m_end);

        m_current = current + size;

        return current;
    }

    void _deallocateBlocks()
    {
        Block* currentBlock = m_blocks;

        while (currentBlock)
        {
            // Deallocate the block
            CARB_FREE(currentBlock->m_alloc);
            // next block
            currentBlock = currentBlock->m_next;
        }
        // Can deallocate all blocks to
        m_blockFreeList.deallocateAll();
    }

    void _setCurrentBlock(Block* block)
    {
        if (block)
        {
            m_end = block->m_end;
            m_start = block->m_start;
            m_current = m_start;
        }
        else
        {
            m_start = nullptr;
            m_end = nullptr;
            m_current = nullptr;
        }
        m_currentBlock = block;
    }

    Block* _newCurrentBlock(size_t size, size_t alignment)
    {
        // Make sure init has been called (or has been set up in parameterized constructor)
        CARB_ASSERT(m_blockSize > 0);
        // Alignment must be a power of 2
        CARB_ASSERT(((alignment - 1) & alignment) == 0);

        // Alignment must at a minimum be block alignment (such if reused the constraints hold)
        alignment = (alignment < m_blockAlignment) ? m_blockAlignment : alignment;

        const size_t alignMask = alignment - 1;

        // First try the next block (if there is one)
        {
            Block* next = m_currentBlock ? m_currentBlock->m_next : m_blocks;
            if (next)
            {
                // Align could be done from the actual allocation start, but doing so would mean a pointer which
                // didn't hit the constraint of being between start/end
                // So have to align conservatively using start
                uint8_t* memory = (uint8_t*)((size_t(next->m_start) + alignMask) & ~alignMask);

                // Check if can fit block in
                if (memory + size <= next->m_end)
                {
                    _setCurrentBlock(next);
                    return next;
                }
            }
        }

        // The size of the block must be at least large enough to take into account alignment
        size_t allocSize = (alignment <= kMinAlignment) ? size : (size + alignment);

        // The minimum block size should be at least m_blockSize
        allocSize = (allocSize < m_blockSize) ? m_blockSize : allocSize;

        // Allocate block
        Block* block = (Block*)m_blockFreeList.allocate();
        if (!block)
        {
            return nullptr;
        }
        // Allocate the memory
        uint8_t* alloc = (uint8_t*)CARB_MALLOC(allocSize);
        if (!alloc)
        {
            m_blockFreeList.deallocate(block);
            return nullptr;
        }
        // Do the alignment on the allocation
        uint8_t* const start = (uint8_t*)((size_t(alloc) + alignMask) & ~alignMask);

        // Setup the block
        block->m_alloc = alloc;
        block->m_start = start;
        block->m_end = alloc + allocSize;
        block->m_next = nullptr;

        // Insert block into list
        if (m_currentBlock)
        {
            // Insert after current block
            block->m_next = m_currentBlock->m_next;
            m_currentBlock->m_next = block;
        }
        else
        {
            // Add to start of the list of the blocks
            block->m_next = m_blocks;
            m_blocks = block;
        }
        _setCurrentBlock(block);
        return block;
    }

    Block* _findBlock(const void* alloc, Block* endBlock = nullptr) const
    {
        const uint8_t* ptr = (const uint8_t*)alloc;

        Block* block = m_blocks;
        while (block != endBlock)
        {
            if (ptr >= block->m_start && ptr < block->m_end)
            {
                return block;
            }
            block = block->m_next;
        }
        return nullptr;
    }

    Block* _findPreviousBlock(Block* block)
    {
        Block* currentBlock = m_blocks;
        while (currentBlock)
        {
            if (currentBlock->m_next == block)
            {
                return currentBlock;
            }
            currentBlock = currentBlock->m_next;
        }
        return nullptr;
    }

    uint8_t* m_start;
    uint8_t* m_end;
    uint8_t* m_current;
    size_t m_blockSize;
    size_t m_blockAlignment;
    Block* m_blocks;
    Block* m_currentBlock;
    FreeListAllocator m_blockFreeList;
};


inline void ArenaAllocator::initialize(size_t blockSize, size_t blockAlignment)
{
    _deallocateBlocks();
    m_blockFreeList.reset();
    _initialize(blockSize, blockAlignment);
}

inline bool ArenaAllocator::isValid(const void* data, size_t size) const
{
    CARB_ASSERT(size);

    uint8_t* ptr = (uint8_t*)data;
    // Is it in current
    if (ptr >= m_start && ptr + size <= m_current)
    {
        return true;
    }
    // Is it in a previous block?
    Block* block = _findBlock(data, m_currentBlock);
    return block && (ptr >= block->m_start && (ptr + size) <= block->m_end);
}

inline void* ArenaAllocator::allocate(size_t size)
{
    // Align with the minimum alignment
    const size_t alignMask = kMinAlignment - 1;
    uint8_t* mem = (uint8_t*)((size_t(m_current) + alignMask) & ~alignMask);

    if (mem + size <= m_end)
    {
        m_current = mem + size;
        return mem;
    }
    else
    {
        return _allocateAligned(size, kMinAlignment);
    }
}

inline void* ArenaAllocator::allocateAligned(size_t size, size_t alignment)
{
    // Alignment must be a power of 2
    CARB_ASSERT(((alignment - 1) & alignment) == 0);

    // Align the pointer
    const size_t alignMask = alignment - 1;
    uint8_t* memory = (uint8_t*)((size_t(m_current) + alignMask) & ~alignMask);

    if (memory + size <= m_end)
    {
        m_current = memory + size;
        return memory;
    }
    else
    {
        return _allocateAligned(size, alignment);
    }
}

inline const char* ArenaAllocator::allocateString(const char* str)
{
    size_t size = ::strlen(str);
    if (size == 0)
    {
        return "";
    }
    char* dst = (char*)allocate(size + 1);
    std::memcpy(dst, str, size + 1);
    return dst;
}

inline const char* ArenaAllocator::allocateString(const char* chars, size_t charsCount)
{
    if (charsCount == 0)
    {
        return "";
    }
    char* dst = (char*)allocate(charsCount + 1);
    std::memcpy(dst, chars, charsCount);

    // Add null-terminating zero
    dst[charsCount] = 0;
    return dst;
}

template <typename T>
T* ArenaAllocator::allocate()
{
    return reinterpret_cast<T*>(allocateAligned(sizeof(T), CARB_ALIGN_OF(T)));
}

template <typename T>
T* ArenaAllocator::allocateArray(size_t count)
{
    return (count > 0) ? reinterpret_cast<T*>(allocateAligned(sizeof(T) * count, CARB_ALIGN_OF(T))) : nullptr;
}

template <typename T>
T* ArenaAllocator::allocateArray(size_t count, bool zeroMemory)
{
    if (count > 0)
    {
        const size_t totalSize = sizeof(T) * count;
        void* ptr = allocateAligned(totalSize, CARB_ALIGN_OF(T));
        if (zeroMemory)
        {
            std::memset(ptr, 0, totalSize);
        }
        return reinterpret_cast<T*>(ptr);
    }
    return nullptr;
}

template <typename T>
T* ArenaAllocator::allocateArrayAndCopy(const T* arr, size_t count)
{
    if (count > 0)
    {
        const size_t totalSize = sizeof(T) * count;
        void* ptr = allocateAligned(totalSize, CARB_ALIGN_OF(T));
        std::memcpy(ptr, arr, totalSize);
        return reinterpret_cast<T*>(ptr);
    }
    return nullptr;
}

inline void ArenaAllocator::deallocateLast(void* data)
{
    // See if it's in current block
    uint8_t* ptr = (uint8_t*)data;
    if (ptr >= m_start && ptr < m_current)
    {
        // Then just go back
        m_current = ptr;
    }
    else
    {
        // Only called if not in the current block. Therefore can only be in previous
        Block* prevBlock = _findPreviousBlock(m_currentBlock);
        if (prevBlock == nullptr || (!(ptr >= prevBlock->m_start && ptr < prevBlock->m_end)))
        {
            CARB_ASSERT(!"Allocation not found");
            return;
        }

        // Make the previous block the current
        _setCurrentBlock(prevBlock);
        // Make the current the alloc freed
        m_current = ptr;
    }
}

inline void ArenaAllocator::deallocateAllFrom(void* data)
{
    // See if it's in current block, and is allocated (ie < m_cur)
    uint8_t* ptr = (uint8_t*)data;
    if (ptr >= m_start && ptr < m_current)
    {
        // If it's in current block, then just go back
        m_current = ptr;
        return;
    }

    // Search all blocks prior to current block
    Block* block = _findBlock(data, m_currentBlock);
    CARB_ASSERT(block);
    if (!block)
    {
        return;
    }
    // Make this current block
    _setCurrentBlock(block);

    // Move the pointer to the allocations position
    m_current = ptr;
}

inline void ArenaAllocator::deallocateAll()
{
    Block** prev = &m_blocks;
    Block* block = m_blocks;

    while (block)
    {
        if (size_t(block->m_end - block->m_alloc) > m_blockSize)
        {
            // Oversized block so we need to free it and remove from the list
            Block* nextBlock = block->m_next;
            *prev = nextBlock;

            // Free the backing memory
            CARB_FREE(block->m_alloc);

            // Free the block
            m_blockFreeList.deallocate(block);

            // prev stays the same, now working on next tho
            block = nextBlock;
        }
        else
        {
            // Onto next
            prev = &block->m_next;
            block = block->m_next;
        }
    }

    // Make the first current (if any)
    _setCurrentBlock(m_blocks);
}

inline void ArenaAllocator::reset()
{
    _deallocateBlocks();
    m_blocks = nullptr;
    _setCurrentBlock(nullptr);
}

inline void ArenaAllocator::adjustToBlockAlignment()
{
    const size_t alignMask = m_blockAlignment - 1;
    uint8_t* ptr = (uint8_t*)((size_t(m_current) + alignMask) & ~alignMask);

    // Alignment might push beyond end of block... if so allocate a new block
    // This test could be avoided if we aligned m_end, but depending on block alignment that might waste some space
    if (ptr > m_end)
    {
        // We'll need a new block to make this alignment
        _newCurrentBlock(0, m_blockAlignment);
    }
    else
    {
        // Set the position
        m_current = ptr;
    }

    CARB_ASSERT(size_t(m_current) & alignMask);
}

inline size_t ArenaAllocator::getBlockAlignment() const
{
    return m_blockAlignment;
}
} // namespace extras
} // namespace carb
