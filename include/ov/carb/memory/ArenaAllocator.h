// Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Allocator that initially uses a memory arena first (typically on the stack) and then falls back to the heap.
#pragma once
#include "../Defines.h"

#include <memory>

namespace carb
{
namespace memory
{

/**
 * An allocator that initially allocates from a memory arena (typically on the stack) and falls back to another
 * allocator when that is exhausted.
 *
 * ArenaAllocator conforms to the C++ Named Requirement of <a
 * href="https://en.cppreference.com/w/cpp/named_req/Allocator">Allocator</a>.
 * @tparam T The type allocated by this allocator.
 * @tparam FallbackAllocator The Allocator that is used when the arena is exhausted.
 */
template <class T, class FallbackAllocator = std::allocator<T>>
class ArenaAllocator
{
public:
    //! \c T*
    using pointer = typename std::allocator_traits<FallbackAllocator>::pointer;
    //! `T const*`
    using const_pointer = typename std::allocator_traits<FallbackAllocator>::const_pointer;
    //! \c void*
    using void_pointer = typename std::allocator_traits<FallbackAllocator>::void_pointer;
    //! `void const*`
    using const_void_pointer = typename std::allocator_traits<FallbackAllocator>::const_void_pointer;
    //! \c T
    using value_type = typename std::allocator_traits<FallbackAllocator>::value_type;
    //! \c std::size_t
    using size_type = typename std::allocator_traits<FallbackAllocator>::size_type;
    //! \c std::ptrdiff_t
    using difference_type = typename std::allocator_traits<FallbackAllocator>::difference_type;

    //! Rebinds ArenaAllocator to a different type \c U
    template <class U>
    struct rebind
    {
        //! The rebound ArenaAllocator
        using other = ArenaAllocator<U, typename FallbackAllocator::template rebind<U>::other>;
    };

    /**
     * Default constructor. Only uses \c FallbackAllocator as no arena is given.
     */
    ArenaAllocator() : m_members(ValueInitFirst{}, nullptr), m_current(nullptr), m_end(nullptr)
    {
    }

    /**
     * Constructs \c ArenaAllocator with a specific \c FallbackAllocator. Only uses \c FallbackAllocator as no arena is
     * given.
     *
     * @param fallback A \c FallbackAllocator instance to copy.
     */
    explicit ArenaAllocator(const FallbackAllocator& fallback)
        : m_members(InitBoth{}, fallback, nullptr), m_current(nullptr), m_end(nullptr)
    {
    }

    /**
     * Constructs \c ArenaAllocator with an arena and optionally a specific \c FallbackAllocator.
     *
     * @warning It is the caller's responsibility to ensure that the given memory arena outlives \c *this and any other
     * \ref ArenaAllocator which it may be moved to.
     *
     * @param begin A pointer to the beginning of the arena.
     * @param end A pointer immediately past the end of the arena.
     * @param fallback A \c FallbackAllocator instance to copy.
     */
    ArenaAllocator(void* begin, void* end, const FallbackAllocator& fallback = FallbackAllocator())
        : m_members(InitBoth{}, fallback, static_cast<uint8_t*>(begin)),
          m_current(alignForward(m_members.second)),
          m_end(static_cast<uint8_t*>(end))
    {
    }

    /**
     * Move constructor: constructs \c ArenaAllocator by moving from a different \c ArenaAllocator.
     *
     * @param other The \c ArenaAllocator to copy from.
     */
    ArenaAllocator(ArenaAllocator&& other)
        : m_members(InitBoth{}, std::move(other.m_members.first()), other.m_members.second),
          m_current(other.m_current),
          m_end(other.m_end)
    {
        // Prevent `other` from allocating memory from the arena. By adding 1 we put it past the end which prevents
        // other->deallocate() from reclaiming the last allocation.
        other.m_current = other.m_end + 1;
    }

    /**
     * Copy constructor: constructs \c ArenaAllocator from a copy of a given \c ArenaAllocator.
     *
     * @note Even though \p other is passed via const-reference, the arena is transferred from \p other to `*this`.
     * Further allocations from \p other will defer to the FallbackAllocator.
     *
     * @param other The \c ArenaAllocator to copy from.
     */
    ArenaAllocator(const ArenaAllocator& other)
        : m_members(InitBoth{}, other.m_members.first(), other.m_members.second),
          m_current(other.m_current),
          m_end(other.m_end)
    {
        // Prevent `other` from allocating memory from the arena. By adding 1 we put it past the end which prevents
        // other->deallocate() from reclaiming the last allocation.
        other.m_current = other.m_end + 1;
    }

    /**
     * Copy constructor: constructs \c ArenaAllocator for type \c T from a copy of a given \c ArenaAllocator for type
     * \c U.
     *
     * @note This does not copy the arena; that is retained by the original allocator.
     *
     * @param other The \c ArenaAllocator to copy from.
     */
    template <class U, class UFallbackAllocator>
    ArenaAllocator(const ArenaAllocator<U, UFallbackAllocator>& other)
        : m_members(InitBoth{}, other.m_members.first(), other.m_members.second),
          m_current(other.m_end + 1),
          m_end(other.m_end)
    {
        // m_current is explicitly assigned to `other.m_end + 1` to prevent further allocations from the arena from
        // *this and to prevent this->deallocate() from reclaiming the last allocation.
    }

    /**
     * Allocates (but does not construct) memory for one or more instances of \c value_type.
     *
     * @param n The number of contiguous \c value_type instances to allocate. If the request cannot be serviced by the
     * arena, the \c FallbackAllocator is used.
     * @returns An uninitialized memory region that will fit \p n contiguous instances of \c value_type.
     * @throws Memory Any exception that would be thrown by \c FallbackAllocator.
     */
    pointer allocate(size_type n = 1)
    {
        if ((m_current + (sizeof(value_type) * n)) <= end())
        {
            pointer p = reinterpret_cast<pointer>(m_current);
            m_current += (sizeof(value_type) * n);
            return p;
        }
        return m_members.first().allocate(n);
    }

    /**
     * Deallocates (but does not destruct) memory for one or more instances of \c value_type.
     *
     * @note If the memory came from the arena, the memory will not be available for reuse unless the memory is the
     * most recent allocation from the arena.
     * @param in The pointer previously returned from \ref allocate().
     * @param n The same \c n value that was passed to \ref allocate() that produced \p in.
     */
    void deallocate(pointer in, size_type n = 1)
    {
        uint8_t* p = reinterpret_cast<uint8_t*>(in);
        if (p >= begin() && p < end())
        {
            if ((p + (sizeof(value_type) * n)) == m_current)
                m_current -= (sizeof(value_type) * n);
        }
        else
            m_members.first().deallocate(in, n);
    }

private:
    uint8_t* begin() const noexcept
    {
        return m_members.second;
    }
    uint8_t* end() const noexcept
    {
        return m_end;
    }

    static uint8_t* alignForward(void* p)
    {
        uint8_t* out = reinterpret_cast<uint8_t*>(p);
        constexpr static size_t align = alignof(value_type);
        size_t aligned = (size_t(out) + (align - 1)) & -(ptrdiff_t)align;
        return out + (aligned - size_t(out));
    }

    template <class U, class UFallbackAllocator>
    friend class ArenaAllocator;
    mutable EmptyMemberPair<FallbackAllocator, uint8_t* /*begin*/> m_members;
    mutable uint8_t* m_current;
    mutable uint8_t* m_end;
};

//! Equality operator
//! @param lhs An allocator to compare
//! @param rhs An allocator to compare
//! @returns \c true if \p lhs and \p rhs can deallocate each other's allocations.
template <class T, class U, class Allocator1, class Allocator2>
bool operator==(const ArenaAllocator<T, Allocator1>& lhs, const ArenaAllocator<U, Allocator2>& rhs)
{
    return (void*)lhs.m_members.second == (void*)rhs.m_members.second && lhs.m_members.first() == rhs.m_members.first();
}

//! Inequality operator
//! @param lhs An allocator to compare
//! @param rhs An allocator to compare
//! @returns the inverse of the equality operator.
template <class T, class U, class Allocator1, class Allocator2>
bool operator!=(const ArenaAllocator<T, Allocator1>& lhs, const ArenaAllocator<U, Allocator2>& rhs)
{
    return !(lhs == rhs);
}

} // namespace memory
} // namespace carb
