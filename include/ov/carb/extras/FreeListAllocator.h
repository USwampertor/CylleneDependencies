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

#include <cstring>

namespace carb
{
namespace extras
{

/**
 * Defines a free list that can allocate/deallocate fast and in any order from identically sized blocks.
 *
 * As long as every deallocation matches every allocation. Both allocation and deallocation are O(1) and
 * generally just a few instructions. The underlying memory allocator will allocate in large blocks,
 * with multiple elements amortizing a more costly large allocation against lots of fast small element allocations.
 */
class FreeListAllocator
{
public:
    static const size_t kMinimalAlignment = sizeof(void*);

    /**
     * Constructor.
     */
    FreeListAllocator()
        : m_top(nullptr),
          m_end(nullptr),
          m_activeBlocks(nullptr),
          m_freeBlocks(nullptr),
          m_freeElements(nullptr),
          m_elementSize(0),
          m_alignment(1),
          m_blockSize(0),
          m_blockAllocationSize(0)
    {
    }

    /**
     * Constructor.
     *
     * @param elementSize Size of an element(in bytes).
     * @param alignment Alignment for elements in bytes, must be a power of 2
     * @param elementsPerBlock The number of elements held in a single block
     */
    FreeListAllocator(size_t elementSize, size_t alignment, size_t elementsPerBlock)
    {
        _initialize(elementSize, alignment, elementsPerBlock);
    }

    /**
     * Destructor.
     */
    ~FreeListAllocator()
    {
        _deallocateBlocks(m_activeBlocks);
        _deallocateBlocks(m_freeBlocks);
    }

    /**
     * Initialize a free list allocator.
     *
     * If called on an already initialized heap, the heap will be deallocated.
     *
     * @param elementSize Size of an element in bytes
     * @param alignment Alignment for elements in bytes, must be a power of 2.
     * @param elementsPerBlock The amount of elements held in a single block
     */
    void initialize(size_t elementSize, size_t alignment, size_t elementsPerBlock);

    /**
     * Determines if the data incoming is a valid allocation.
     *
     * @param data Pointer to be checked if is a valid allocation.
     * @return true if the pointer is to a previously returned and active allocation.
     */
    bool isValid(const void* data) const;

    /**
     * Allocates an element.
     *
     * @return The element allocated.
     */
    void* allocate();

    /**
     * Deallocate a block that was previously allocated with allocate
     *
     * @param data Pointer previously gained by allocate.
     */
    void deallocate(void* data);

    /**
     * Deallocates all elements
     */
    void deallocateAll();

    /**
     * Resets and deallocates all blocks and frees any backing memory and resets memory blocks to initial state.
     */
    void reset();

    /**
     * Gets the element size.
     *
     * @return The size of an element (in bytes).
     */
    size_t getElementSize() const;

    /**
     * Gets the total size of each individual block allocation (in bytes).
     *
     * @return The size of each block (in bytes).
     */
    size_t getBlockSize() const;

    /**
     * Gets the allocation alignment (in bytes).
     *
     * @return Allocation alignment (in bytes).
     */
    size_t getAlignment() const;

private:
    struct Element
    {
        Element* m_next;
    };

    struct Block
    {
        Block* m_next;
        uint8_t* m_data;
    };

    FreeListAllocator(const FreeListAllocator& rhs) = delete;

    void operator=(const FreeListAllocator& rhs) = delete;

    void _initialize(size_t elementSize, size_t alignment, size_t elementsPerBlock)
    {
        // Alignment must be at least the size of a pointer, as when freed a pointer is stored in free blocks.
        m_alignment = (alignment < kMinimalAlignment) ? kMinimalAlignment : alignment;

        // Alignment must be a power of 2
        CARB_ASSERT(((m_alignment - 1) & m_alignment) == 0);

        // The elementSize must at least be the size of the alignment
        m_elementSize = (elementSize >= m_alignment) ? elementSize : m_alignment;

        // The elementSize must be an integral number of alignment/s
        CARB_ASSERT((m_elementSize & (m_alignment - 1)) == 0);

        m_blockSize = m_elementSize * elementsPerBlock;

        // Calculate the block size need, correcting for alignment
        const size_t alignedBlockSize = _calculateAlignedBlockSize(m_alignment);

        // Make the block struct size aligned
        m_blockAllocationSize = m_blockSize + alignedBlockSize;

        m_top = nullptr;
        m_end = nullptr;
        m_activeBlocks = nullptr;
        m_freeBlocks = nullptr;
        m_freeElements = nullptr;
    }

    void* _allocateBlock()
    {
        Block* block = m_freeBlocks;
        if (block)
        {
            // Remove from the free blocks
            m_freeBlocks = block->m_next;
        }
        else
        {
            block = (Block*)CARB_MALLOC(m_blockAllocationSize);
            if (!block)
            {
                return nullptr;
            }
            // Do the alignment
            {
                size_t fix = (size_t(block) + sizeof(Block) + m_alignment - 1) & ~(m_alignment - 1);
                block->m_data = (uint8_t*)fix;
            }
        }
        // Attach to the active blocks
        block->m_next = m_activeBlocks;
        m_activeBlocks = block;

        // Set up top and end
        m_end = block->m_data + m_blockSize;

        // Return the first element
        uint8_t* element = block->m_data;
        m_top = element + m_elementSize;

        return element;
    }

    void _deallocateBlocks(Block* block)
    {
        while (block)
        {
            Block* next = block->m_next;

            CARB_FREE(block);
            block = next;
        }
    }

    static size_t _calculateAlignedBlockSize(size_t align)
    {
        return (sizeof(Block) + align - 1) & ~(align - 1);
    }

    uint8_t* m_top;
    uint8_t* m_end;
    Block* m_activeBlocks;
    Block* m_freeBlocks;
    Element* m_freeElements;
    size_t m_elementSize;
    size_t m_alignment;
    size_t m_blockSize;
    size_t m_blockAllocationSize;
};

inline void FreeListAllocator::initialize(size_t elementSize, size_t alignment, size_t elementsPerBlock)
{
    _deallocateBlocks(m_activeBlocks);
    _deallocateBlocks(m_freeBlocks);

    _initialize(elementSize, alignment, elementsPerBlock);
}

inline bool FreeListAllocator::isValid(const void* data) const
{
    uint8_t* checkedData = (uint8_t*)data;
    Block* block = m_activeBlocks;
    while (block)
    {
        uint8_t* start = block->m_data;
        uint8_t* end = start + m_blockSize;

        if (checkedData >= start && checkedData < end)
        {
            // Check it's aligned correctly
            if ((checkedData - start) % m_elementSize)
            {
                return false;
            }
            // Non-allocated data is between top and end
            if (checkedData >= m_top && data < m_end)
            {
                return false;
            }
            // It can't be in the free list
            Element* element = m_freeElements;
            while (element)
            {
                if (element == (Element*)data)
                {
                    return false;
                }
                element = element->m_next;
            }
            return true;
        }
        block = block->m_next;
    }
    // It's not in an active block and therefore it cannot be a valid allocation
    return false;
}

inline void* FreeListAllocator::allocate()
{
    // Check if there are any previously deallocated elements, if so use these first
    {
        Element* element = m_freeElements;
        if (element)
        {
            m_freeElements = element->m_next;

            return element;
        }
    }
    // If there is no space on current block, then allocate a new block and return first element
    if (m_top >= m_end)
    {
        return _allocateBlock();
    }

    // We can use top
    void* data = (void*)m_top;
    m_top += m_elementSize;

    return data;
}

inline void FreeListAllocator::deallocate(void* data)
{
    Element* element = (Element*)data;
    element->m_next = m_freeElements;
    m_freeElements = element;
}

inline void FreeListAllocator::deallocateAll()
{
    Block* block = m_activeBlocks;
    if (block)
    {
        // Find the end block
        while (block->m_next)
        {
            block = block->m_next;
        }
        // Attach to the freeblocks
        block->m_next = m_freeBlocks;
        // The list is now all freelists
        m_freeBlocks = m_activeBlocks;
        // There are no active blocks
        m_activeBlocks = nullptr;
    }
    m_top = nullptr;
    m_end = nullptr;
}

inline void FreeListAllocator::reset()
{
    _deallocateBlocks(m_activeBlocks);
    _deallocateBlocks(m_freeBlocks);

    m_top = nullptr;
    m_end = nullptr;
    m_activeBlocks = nullptr;
    m_freeBlocks = nullptr;
    m_freeElements = nullptr;
}

inline size_t FreeListAllocator::getElementSize() const
{
    return m_elementSize;
}

inline size_t FreeListAllocator::getBlockSize() const
{
    return m_blockSize;
}

inline size_t FreeListAllocator::getAlignment() const
{
    return m_alignment;
}
} // namespace extras
} // namespace carb
