// Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"

#include "../logging/Log.h"
#include "FreeListAllocator.h"

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
class MultiFreeListAllocator
{
public:
    struct AllocDesc
    {
        size_t elementSize;
        size_t elementsPerBlock;
    };

    /**
     * Constructor.
     */
    MultiFreeListAllocator()
    {
    }

    /**
     * Destructor.
     */
    ~MultiFreeListAllocator()
    {
        shutdown();
    }

    void startup(AllocDesc* allocatorDescs, size_t allocatorCount)
    {
        // First, count the free list allocator descriptors
        m_freeListAllocDescs = new FreeListAllocInternalDesc[allocatorCount];
        m_freeListAllocCount = static_cast<uint32_t>(allocatorCount);

        // Setup allocators
        for (uint32_t i = 0; i < m_freeListAllocCount; ++i)
        {
            size_t elemSize = CARB_ALIGNED_SIZE(allocatorDescs[i].elementSize, FreeListAllocator::kMinimalAlignment);
            size_t elemPerBlock = (allocatorDescs[i].elementsPerBlock == kElementsPerBlockAuto) ?
                                      kDefaultElementsPerBlock :
                                      allocatorDescs[i].elementsPerBlock;
            m_freeListAllocDescs[i].pureElemSize = elemSize;
            m_freeListAllocDescs[i].allocator.initialize(elemSize, 0, elemPerBlock);
        }
    }

    void shutdown()
    {
        delete[] m_freeListAllocDescs;
        m_freeListAllocDescs = nullptr;
        m_freeListAllocCount = 0;
    }

    void* allocate(size_t size)
    {
        uint8_t* originalChunk = nullptr;

        // Calculate total prefix size
        size_t prefixSize = sizeof(ChunkSizePrefixType) + sizeof(ChunkOffsetPrefixType);

        // Chunk size includes prefixes and the requested chunk size
        size_t originalChunkSize = prefixSize + size;

        // Get suitable allocator index based on the total chunk size
        uint32_t allocatorIndex = _getAllocatorIndexFromSize(originalChunkSize);
        if (allocatorIndex != kNoSuitableAllocator)
        {
            originalChunk = static_cast<uint8_t*>(m_freeListAllocDescs[allocatorIndex].allocator.allocate());
        }
        else
        {
            originalChunk = static_cast<uint8_t*>(CARB_MALLOC(originalChunkSize));
        }

        if (originalChunk == nullptr)
        {
            CARB_LOG_ERROR("Failed to allocate memory!");
            return nullptr;
        }

        size_t ptrOffset = prefixSize;

        // Calculate final chunk address, so that prefix could be inserted
        uint8_t* extChunk = ptrOffset + originalChunk;

        // Record chunk size requested from the underlying allocator
        ChunkSizePrefixType* chunkSizeMem = reinterpret_cast<ChunkSizePrefixType*>(extChunk) - 1;
        *chunkSizeMem = static_cast<ChunkSizePrefixType>(originalChunkSize);

        // Record the offset from the returned memory pointer to the allocated memory chunk
        ChunkOffsetPrefixType* chunkOffsetMem = reinterpret_cast<ChunkOffsetPrefixType*>(chunkSizeMem) - 1;
        *chunkOffsetMem = static_cast<ChunkOffsetPrefixType>(ptrOffset);

        return extChunk;
    }

    void* allocateAligned(size_t size, size_t alignment)
    {
        // With zero alignment, call regular allocation
        if (alignment == 0)
            return allocate(size);

        uint8_t* originalChunk = nullptr;

        // Calculate aligned memory slot size for prefixes
        size_t prefixSize = sizeof(ChunkSizePrefixType) + sizeof(ChunkOffsetPrefixType);
        size_t prefixSizeAligned = CARB_ALIGNED_SIZE(prefixSize, static_cast<uint32_t>(alignment));

        // Conservative chunk size includes prefixes, requested chunk size and alignment reserve
        size_t originalChunkAlignedSize = prefixSizeAligned + size + (alignment - 1);

        // Get suitable allocator index based on the total chunk size
        uint32_t allocatorIndex = _getAllocatorIndexFromSize(originalChunkAlignedSize);
        if (allocatorIndex != kNoSuitableAllocator)
        {
            originalChunk = static_cast<uint8_t*>(m_freeListAllocDescs[allocatorIndex].allocator.allocate());
        }
        else
        {
            originalChunk = static_cast<uint8_t*>(CARB_MALLOC(originalChunkAlignedSize));
        }

        if (originalChunk == nullptr)
        {
            CARB_LOG_ERROR("Failed to allocate memory!");
            return nullptr;
        }

        // Calculate final chunk address, so that prefix could be inserted
        uint8_t* alignedPrefixedChunk = CARB_ALIGN(originalChunk + prefixSize, alignment);
        size_t ptrOffset = alignedPrefixedChunk - originalChunk;

        // Make sure that we don't go out-of-bounds
        CARB_CHECK(ptrOffset + size <= originalChunkAlignedSize);

        // This is effectively equal to alignedPrefixedChunk
        uint8_t* extChunk = ptrOffset + originalChunk;

        // Record chunk size requested from the underlying allocator
        ChunkSizePrefixType* chunkSizeMem = reinterpret_cast<ChunkSizePrefixType*>(extChunk) - 1;
        *chunkSizeMem = static_cast<ChunkSizePrefixType>(originalChunkAlignedSize);

        // Record the offset from the returned memory pointer to the allocated memory chunk
        ChunkOffsetPrefixType* chunkOffsetMem = reinterpret_cast<ChunkOffsetPrefixType*>(chunkSizeMem) - 1;
        *chunkOffsetMem = static_cast<ChunkOffsetPrefixType>(ptrOffset);

        return extChunk;
    }

    void deallocate(void* memory)
    {
        if (!memory)
            return;

        uint8_t* extChunk = static_cast<uint8_t*>(memory);

        ChunkSizePrefixType* chunkSizeMem = reinterpret_cast<ChunkSizePrefixType*>(extChunk) - 1;
        size_t originalChunkSize = static_cast<size_t>(*chunkSizeMem);

        ChunkOffsetPrefixType* chunkOffsetMem = reinterpret_cast<ChunkOffsetPrefixType*>(chunkSizeMem) - 1;
        size_t offset = static_cast<size_t>(*chunkOffsetMem);

        uint8_t* originalChunk = reinterpret_cast<uint8_t*>(extChunk) - offset;

        uint32_t allocatorIndex = _getAllocatorIndexFromSize(originalChunkSize);
        if (allocatorIndex != kNoSuitableAllocator)
        {
            m_freeListAllocDescs[allocatorIndex].allocator.deallocate(originalChunk);
        }
        else
        {
            CARB_FREE(originalChunk);
        }
    }

private:
    struct FreeListAllocInternalDesc
    {
        FreeListAllocator allocator;
        size_t pureElemSize;
    };

    FreeListAllocInternalDesc* m_freeListAllocDescs = nullptr;
    uint32_t m_freeListAllocCount = 0;

    static const size_t kUnlimitedSize = 0;
    static const size_t kDefaultElementsPerBlock = 100;
    static const size_t kElementsPerBlockAuto = 0;

    using ChunkSizePrefixType = uint32_t;
    using ChunkOffsetPrefixType = uint32_t;
    const uint32_t kNoSuitableAllocator = (uint32_t)-1;

    uint32_t _getAllocatorIndexFromSize(size_t size)
    {
        for (uint32_t i = 0; i < m_freeListAllocCount; ++i)
        {
            if (m_freeListAllocDescs[i].pureElemSize >= size)
            {
                return i;
            }
        }
        return kNoSuitableAllocator;
    }

    static constexpr size_t _calculateAlignedSize(size_t size, size_t alignment)
    {
        return CARB_ALIGNED_SIZE(size, static_cast<uint32_t>(alignment));
    }
};
} // namespace extras
} // namespace carb
