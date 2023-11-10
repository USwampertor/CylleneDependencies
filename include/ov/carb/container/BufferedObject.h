// Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief Defines the BufferedObject class
#pragma once

#include "../Defines.h"
#include "../cpp17/Utility.h"
#include "../cpp20/Atomic.h"

#include <array>
#include <cstdint>
#include <utility>

namespace carb
{

namespace container
{

/**
 * Lock-Free Asynchronous Buffer
 * Supports only 1 producer, 1 consumer
 *
 * BufferedObject is used when you have 1 producer and 1 consumer and both
 * the producer and consumer are operating at different frequencies.
 * The consumer only ever cares to see the latest data available.
 *
 * Examples:
 *
 *  BufferedObject<int> b;
 *  assert(b.front() == 0);
 *  b.emplace_back(42);
 *  assert(b.front() == 0);
 *  b.pop_front();
 *  assert(b.front() == 42);
 *
 *  BufferedObject<uint32_t> b{ in_place, 1U, 2U, 3U };
 *  assert(b.front() == 3U);
 *  b.pop_front(); // do nothing, as nothing was pushed
 *  assert(b.front() == 3U);
 *  b.push_back(42U);
 *  assert(b.front() == 3U);
 *  b.pop_front();
 *  assert(b.front() == 42U);
 */
template <typename T>
class BufferedObject final
{
    CARB_PREVENT_COPY_AND_MOVE(BufferedObject);

    enum Flags : uint8_t
    {
        eFlagsCreate = 0x06, // Field0=0, Field1=0, Field2=1, Field3=2
        eFlagsDestroy = 0x00,
        eDataAvailable = 0x40, // Field0=1
        eField0Mask = 0xc0,
        eField1Mask = 0x30,
        eField2Mask = 0x0c,
        eField3Mask = 0x03,
    };

public:
    /**
     * Create an async buffer from an array of 3 items using default ctors
     */
    BufferedObject() : m_flags(eFlagsCreate), m_buffer{}
    {
    }

    /**
     * Create an async buffer from an array of 3 items
     *
     * @param args arguments to forward to construct the elements of the buffer
     */
    template <typename... TArgs>
    constexpr explicit BufferedObject(carb::cpp17::in_place_t, TArgs&&... args)
        : m_flags(eFlagsCreate), m_buffer{ std::forward<TArgs>(args)... }
    {
    }

    /**
     * Destroy async buffer.
     */
    ~BufferedObject()
    {
        m_flags = eFlagsDestroy;
    }

    /**
     * Insert a new item into the container constructed in-place with the given args
     *
     * @param args arguments to forward to construct newly produced value and move it onto the buffer
     */
    template <typename... TArgs>
    void emplace_back(TArgs&&... args)
    {
        uint8_t flagsNow;
        uint8_t newFlags;

        // place item in buffer in producer index/slot
        m_buffer[(m_flags.load(std::memory_order_acquire) & eField1Mask) >> 4] = { std::forward<TArgs>(args)... };

        //
        // to produce a new value we need to:
        //   1. set field 0 in flags to 1 (using mask eNewDataMask)
        //   2. swap fields 1 and 2 ((flagsNow & eField2Mask) << 2) | ((flagsNow & eField1Mask) >> 2)
        //   3. leave field 3 unchanged (flagsNow & eField3Mask)
        //

        do
        {
            flagsNow = m_flags.load(std::memory_order_acquire);
            newFlags = eDataAvailable | ((flagsNow & eField2Mask) << 2) | ((flagsNow & eField1Mask) >> 2) |
                       (flagsNow & eField3Mask);
        } while (
            !m_flags.compare_exchange_strong(flagsNow, newFlags, std::memory_order_acq_rel, std::memory_order_relaxed));
    }

    /**
     * Insert a new item into the container by moving item
     *
     * @param item item to insert/move
     */
    void push_back(T&& item)
    {
        emplace_back(std::move(item));
    }

    /**
     * Insert a new item into the container by copying item
     *
     * @param item item to copy
     */
    void push_back(T const& item)
    {
        emplace_back(item);
    }

    /**
     * Get reference to the latest item.
     *
     * @return reference to latest item
     */
    T& front()
    {
        return m_buffer[m_flags.load(std::memory_order_acquire) & eField3Mask];
    }

    /**
     * Attempt to replace the front element of the container with a newly produced value.
     * If no new value was pushed/emplaced, this function does nothing.
     */
    void pop_front()
    {
        uint8_t flagsNow;
        uint8_t newFlags;

        //
        // to consume a new value:
        //   1. check if new data available bit is set, if not just return previous data
        //   2. remove the new data available bit
        //   3. swap fields 2 and 3 ((flagsNow & eField3Mask) << 2) | ((flagsNow & eField2Mask) >> 2)
        //   4. leave field 1 unchanged (flagsNow & eField1Mask)
        //

        do
        {
            flagsNow = m_flags.load(std::memory_order_acquire);

            // check new data available bit set first
            if ((flagsNow & eDataAvailable) == 0)
            {
                break;
            }

            newFlags = (flagsNow & eField1Mask) | ((flagsNow & eField3Mask) << 2) | ((flagsNow & eField2Mask) >> 2);
        } while (
            !m_flags.compare_exchange_strong(flagsNow, newFlags, std::memory_order_acq_rel, std::memory_order_relaxed));
    }

private:
    /*
     * 8-bits of flags (2-bits per field)
     *
     *   0  1  2  3
     * +--+--+--+--+
     * |00|00|00|00|
     * +--+--+--+--+
     *
     * Field 0: Is new data available? 0 == no, 1 == yes
     * Field 1: Index into m_buffer that new values are pushed/emplaced into via push_back()/emplace_back()
     * Field 2: Index into m_buffer that is the buffer between producer and consumer
     * Field 3: Index into m_buffer that represents front()
     *
     * When the producer pushes a new value to m_buffer[field1], it will then atomically swap
     * Field 1 and Field 2 and set Field 0 to 1 (to indicate new data is available)
     *
     * When the consumer calls front(), it just returns m_buffer[field3]. Since the producer
     * never changes field3 value, the consumer is safe to call front() without any locks, even
     * if the producer is pushing new values.
     *
     * When the consumer calls pop_front(), it will atomically swap
     * Field 3 and Field 2 and set Field 0 back to 0 (to indicate middle buffer was drained)
     *
     * Producer
     *   * only ever sets Field 0 to 1
     *   * only ever writes to m_buffer[field1]
     *   * only ever swaps Field 1 and Field 2
     *
     * Consumer
     *   * only ever sets Field 0 to 0
     *   * only ever reads to m_buffer[field3]
     *   * only ever swaps Field 2 and Field 3
     */
    std::atomic<uint8_t> m_flags;
    std::array<T, 3> m_buffer;
};

} // namespace container

} // namespace carb
