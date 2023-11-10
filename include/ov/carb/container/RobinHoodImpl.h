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
//! @brief Carbonite Robin-hood container generic implementation.
#pragma once

#include "../Defines.h"

#include <algorithm>
#include <iterator>

#include "../cpp20/Bit.h"

namespace carb
{
namespace container
{

namespace detail
{

#ifndef DOXYGEN_BUILD
template <class Key, class ValueType>
struct Select1st
{
    const Key& operator()(const ValueType& vt) const noexcept
    {
        return vt.first;
    }
};

template <class Key, class ValueType>
struct Identity
{
    const Key& operator()(const ValueType& vt) const noexcept
    {
        return vt;
    }
};
#endif

/**
 * Implements a "Robin Hood" open-addressing hash container that can either store keys alone or key/value pairs; this
 * template is not meant to be used directly--instead use \ref RHUnorderedSet, \ref RHUnorderedMap,
 * \ref RHUnorderedMultimap, or \ref RHUnorderedMultiset.
 *
 * \details
 * In an open-addressing ("OA") hash table, the contained items are stored in the buckets directly. Contrast this with
 * traditional hash tables that typically have a level of indirection: buckets point to the head of a linked-list that
 * contains every item that hashes to that bucket. Open-addressing hash tables are great for using contiguous memory,
 * whereas traditional hash tables have a separate allocation per node and fragment memory. However, OA hash tables have
 * a couple downsides: if a collision occurs on insertion, probing must happen until an open spot is found where the
 * item can be placed. For a find operation, probing must continue until an empty spot is reached to make sure that
 * all keys have been checked. When erasing an item, a "deleted" marker must be put in its place so that probing past
 * the key can continue. This system also gives advantage to earlier insertions and penalizes later collisions.
 *
 * The Robin Hood algorithm for open-addressing hashing was first postulated by Pedro Celis in 1986:
 * https://cs.uwaterloo.ca/research/tr/1986/CS-86-14.pdf. Simply put, it applies a level of fairness to locality of
 * items within the OA hash table. This is done by tracking the distance from an items ideal insertion point. Similarly
 * the distance-from-ideal can be easily computed for existing locations that are probed. Once a probed location for a
 * new item will cause the new item to be worse off (farther from ideal insertion) than the existing item, the new item
 * can "steal" the location from the existing item, which must then probe until it finds a location where it is worse
 * off than the existing item, and so on. This balancing of locality has beneficial side effects for finding and erasing
 * too: when searching for an item, once a location is reached where the item would be worse off than the existing item,
 * probing can cease with the knowledge that the item is not contained.
 *
 * OA hash tables cannot be direct drop-in replacements for closed-addressing hash containers such as
 * `std::unordered_map` as nearly every modification to the table can potentially invalidate any other iterator.
 *
 * Open-addressing hash tables may not be a good replacement for `std` unordered containers in cases where the key
 * and/or value is very large (though this may be mitigated somewhat by using indirection through `std::unique_ptr`).
 * Since OA hash tables must carry the size of each value_type, having a low load factor (or a high capacity() to size()
 * ratio) wastes a lot of memory, especially if the key/value pair is very large.
 *
 * It is important to keep OA hash tables as compact as possible, as operations like `clear()` and iterating over the
 * hash table are `O(n)` over `capacity()`, not `size()`. You can always ensure that the hash table is as compact as
 * possible by calling `rehash(0)`.
 *
 * Because of the nature of how elements are stored in this hash table, there are two iterator types: `iterator` and
 * `find_iterator` (both with `const` versions). These types can be compared with each other, but incrementing these
 * objects works differently. `iterator` and `const_iterator` traverse to the next item in the container, while
 * `find_iterator` and `const_find_iterator` will only traverse to the next item with the same key. In multi-key
 * containers, items with the same key may not necessarily be stored adjacently, so incrementing `iterator` may not
 * encounter the next item with the same key as the previous. For unique-key containers, incrementing a `find_iterator`
 * will always produce `end()` since keys are guaranteed to be unique.
 */
template <size_t LoadFactorMax100, class Key, class ValueType, class KeyFromValue, class Hasher, class Equals>
class CARB_VIZ RobinHood
{
public:
#ifndef DOXYGEN_BUILD
    using key_type = Key;
    using value_type = ValueType;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using hasher = Hasher;
    using key_equal = Equals;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

    static_assert(LoadFactorMax100 >= 10 && LoadFactorMax100 <= 100, "Load factor must be in range [10, 100]");

    // clang-format off
    class iter_base
    {
    public:
        constexpr iter_base() noexcept = default;
        bool operator == (const iter_base& other) const noexcept { CARB_ASSERT(owner == other.owner); return where == other.where; }
        bool operator != (const iter_base& other) const noexcept { CARB_ASSERT(owner == other.owner); return where != other.where; }
    protected:
        constexpr iter_base(const RobinHood* owner_, value_type* where_) noexcept : owner(owner_), where(where_) {}
        CARB_VIZ const RobinHood* owner{ nullptr };
        CARB_VIZ value_type* where{ nullptr };
    };

    class CARB_VIZ const_find_iterator : public iter_base
    {
        using Base = iter_base;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = RobinHood::value_type;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;
        constexpr               const_find_iterator() noexcept = default;
        reference               operator *  () const noexcept   { CARB_ASSERT(this->where); return *this->where; }
        pointer                 operator -> () const noexcept   { CARB_ASSERT(this->where); return this->where; }
        const_find_iterator&    operator ++ () noexcept         { CARB_ASSERT(this->where); incr(); return *this; }
        const_find_iterator     operator ++ (int) noexcept      { const_find_iterator i{ *this }; incr(); return i; }
    protected:
        friend class RobinHood;
        constexpr const_find_iterator(const RobinHood* owner_, value_type* where_) noexcept : Base{ owner_, where_ } {}
        void incr() { CARB_ASSERT(this->owner && this->where); this->where = this->owner->_findnext(this->where); }
    };
    class find_iterator : public const_find_iterator
    {
        using Base = const_find_iterator;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = RobinHood::value_type;
        using difference_type = ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;
        constexpr       find_iterator() noexcept = default;
        reference       operator *  () const noexcept { CARB_ASSERT(this->where); return *this->where; }
        pointer         operator -> () const noexcept { CARB_ASSERT(this->where); return this->where; }
        find_iterator&  operator ++ () noexcept       { CARB_ASSERT(this->where); this->incr(); return *this; }
        find_iterator   operator ++ (int) noexcept    { CARB_ASSERT(this->where); find_iterator i{ *this }; this->incr(); return i; }
    protected:
        friend class RobinHood;
        constexpr find_iterator(const RobinHood* owner_, value_type* where_) noexcept : Base(owner_, where_) {}
    };

    class CARB_VIZ const_iterator : public iter_base
    {
        using Base = iter_base;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = RobinHood::value_type;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;
        constexpr       const_iterator() noexcept = default;
        reference       operator *  () const noexcept   { CARB_ASSERT(this->where); return *this->where; }
        pointer         operator -> () const noexcept   { CARB_ASSERT(this->where); return this->where; }
        const_iterator& operator ++ () noexcept         { CARB_ASSERT(this->where); incr(); return *this; }
        const_iterator  operator ++ (int) noexcept      { const_iterator i{ *this }; incr(); return i; }
    protected:
        friend class RobinHood;
        constexpr const_iterator(const RobinHood* owner_, value_type* where_) noexcept : Base{ owner_, where_ } {}
        void incr() { CARB_ASSERT(this->owner && this->where); this->where = this->owner->_next(this->where); }
    };
    class iterator : public const_iterator
    {
        using Base = const_iterator;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = RobinHood::value_type;
        using difference_type = ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;
        constexpr iterator() noexcept = default;
        reference operator *  () const noexcept { CARB_ASSERT(this->where); return *this->where; }
        pointer   operator -> () const noexcept { CARB_ASSERT(this->where); return this->where; }
        iterator& operator ++ () noexcept       { CARB_ASSERT(this->where); this->incr(); return *this; }
        iterator  operator ++ (int) noexcept    { CARB_ASSERT(this->where); iterator i{ *this }; this->incr(); return i; }
    protected:
        friend class RobinHood;
        constexpr iterator(const RobinHood* owner_, value_type* where_) noexcept : Base(owner_, where_) {}
    };
    // clang-format on

    constexpr RobinHood() noexcept = default;

    RobinHood(const RobinHood& other)
    {
        // This effectively rebuilds `other` as *this, but it sizes *this as compactly as possible (whereas `other` may
        // have a high capacity():size() ratio).
        reserve(other.size());
        for (auto& entry : other)
        {
            auto result = internal_insert_multi(KeyFromValue{}(entry));
            new (result) value_type(entry); // copy
        }
    }

    RobinHood(RobinHood&& other)
    {
        std::swap(m_data, other.m_data);
    }

    ~RobinHood()
    {
        if (m_data.m_table)
        {
            if (!empty() && !std::is_trivially_destructible<value_type>::value)
            {
                for (size_t i = 0; i != m_data.m_tableSize; ++i)
                {
                    if (isHashValid(m_data.m_hashes[i]))
                        m_data.m_table[i].~value_type();
                }
            }
            std::free(std::exchange(m_data.m_table, nullptr));
            m_data.m_hashes = nullptr;
            m_data.m_size = m_data.m_tableSize = 0;
        }
    }

    RobinHood& operator=(const RobinHood& other)
    {
        clear();
        reserve(other.size());
        for (auto& entry : other)
        {
            auto result = internal_insert_multi(KeyFromValue{}(entry));
            new (result) value_type(entry);
        }
        return *this;
    }

    RobinHood& operator=(RobinHood&& other)
    {
        if (this != &other)
        {
            std::swap(m_data, other.m_data);
        }
        return *this;
    }
#endif

    /**
     * Creates an iterator to the first element in the container.
     *
     * @returns a `const_iterator` to the first element in the container. If the container is \ref empty() the iterator
     * will be equal to \ref cend().
     */
    const_iterator cbegin() const
    {
        if (CARB_UNLIKELY(empty()))
            return cend();

        auto const end = m_data.m_hashes + m_data.m_tableSize;
        for (auto e = m_data.m_hashes; e != end; ++e)
            if (isHashValid(*e))
                return { this, m_data.m_table + (e - m_data.m_hashes) };

        // Should never get here since we checked empty()
        CARB_ASSERT(0);
        return cend();
    }

    /**
     * Creates an iterator to the first element in the container.
     *
     * @returns an `iterator` to the first element in the container. If the container is \ref empty() the iterator will
     * be equal to \ref end().
     */
    iterator begin()
    {
        if (CARB_UNLIKELY(empty()))
            return end();

        auto const kEnd = m_data.m_hashes + m_data.m_tableSize;
        for (auto e = m_data.m_hashes; e != kEnd; ++e)
            if (isHashValid(*e))
                return { this, m_data.m_table + (e - m_data.m_hashes) };

        // Should never get here since we checked empty()
        CARB_ASSERT(0);
        return end();
    }

    /**
     * Creates an iterator to the first element in the container.
     *
     * @returns a `const_iterator` to the first element in the container. If the container is \ref empty() the iterator
     * will be equal to \ref end().
     */
    const_iterator begin() const
    {
        return cbegin();
    }

    /**
     * Creates an iterator to the past-the-end element in the container.
     *
     * @returns a `const_iterator` to the past-the-end element in the container. This iterator is a placeholder;
     * attempting to access it results in undefined behavior.
     */
    const_iterator cend() const
    {
        return { this, nullptr };
    }

    /**
     * Creates an iterator to the past-the-end element in the container.
     *
     * @returns an `iterator` to the past-the-end element in the container. This iterator is a placeholder;
     * attempting to access it results in undefined behavior.
     */
    iterator end()
    {
        return { this, nullptr };
    }

    /**
     * Creates an iterator to the past-the-end element in the container.
     *
     * @returns a `const_iterator` to the past-the-end element in the container. This iterator is a placeholder;
     * attempting to access it results in undefined behavior.
     */
    const_iterator end() const
    {
        return cend();
    }

    /**
     * Checks whether the container is empty.
     * @returns \c true if the container contains no elements; \c false otherwise.
     */
    bool empty() const noexcept
    {
        return !m_data.m_size;
    }

    /**
     * Returns the number of elements contained. O(1)
     * @returns the number of elements contained.
     */
    size_t size() const noexcept
    {
        return m_data.m_size;
    }

    /**
     * Returns the maximum possible number of elements. O(1)
     * @returns the maximum possible number of elements.
     */
    constexpr size_t max_size() const noexcept
    {
        return size_t(-1) & ~kDeletedBit;
    }

    /**
     * Returns the number of elements that can be stored with the current memory usage. This is based on the
     * `LoadFactorMax100` percentage and the current power-of-two memory allocation size. O(1)
     * @see reserve()
     * @returns the number of elements that can be stored with the current memory usage.
     */
    size_t capacity() const noexcept
    {
        // Handle case where Windows.h may have defined 'max'
#pragma push_macro("max")
#undef max
        if (CARB_LIKELY(m_data.m_tableSize <= (std::numeric_limits<size_t>::max() / 100)))
            return (m_data.m_tableSize * LoadFactorMax100) / 100;
#pragma pop_macro("max")
        // In the unlikely event of a huge table, switch operations to not overflow
        return (m_data.m_tableSize / 100) * LoadFactorMax100;
    }

    /**
     * Clears the contents. O(n) over \ref capacity()
     *
     * Erases all elements from the container. After this call \ref size() returns zero. Invalidates all iterators,
     * pointers and references to contained elements.
     *
     * @note This does not free the memory used by the container; to free the hash table memory, use `rehash(0)` after
     * this call.
     */
    void clear()
    {
        if (!empty())
        {
            size_t* const end = m_data.m_hashes + m_data.m_tableSize;
            for (size_t* e = m_data.m_hashes; e != end; ++e)
            {
                if (isHashValid(*e) && !std::is_trivially_destructible<value_type>::value)
                    m_data.m_table[e - m_data.m_hashes].~value_type();
                *e = kEmpty;
            }
            m_data.m_size = 0;
        }
    }

    /**
     * Swaps the contents of two containers. O(1)
     *
     * Exchanges the contents of \c *this with \p other. Will not invoke any move/copy/swap operations on the individual
     * elements.
     *
     * All iterators are invalidated for both containers. However, pointers and references to contained elements remain
     * valid.
     *
     * The \c Hasher and \c KeyEqual template parameters must be \a Swappable.
     * @param other The other container to swap with \c *this.
     */
    void swap(RobinHood& other)
    {
        std::swap(m_data, other.m_data);
    }

    /**
     * Removes the given element.
     *
     * References, pointers and iterators to the erased element are invalidated. All other iterators, pointers and
     * references remain valid.
     *
     * @param pos The \c iterator to the element to remove. This iterator must be valid and dereferenceable.
     * @returns the iterator immediately following \p pos.
     */
    iterator erase(const_iterator pos)
    {
        CARB_ASSERT(pos.owner == this);
        assertContained(pos.where);

        internal_erase(pos.where);
        return iterator{ this, _next(pos.where) };
    }

    /**
     * Removes the elements in the given range.
     *
     * The range `[first, last)` must be a valid range in `*this`. References, pointers and iterators to erased elements
     * are invalidated. All other iterators, pointers and references to other elements remain valid.
     *
     * @param first The start of the range of iterators to remove.
     * @param last The past-the-end iterator for the range to remove.
     * @returns \p last
     */
    iterator erase(const_iterator first, const_iterator last)
    {
        while (first != last)
            first = erase(first);
        return { this, first.where };
    }

    /**
     * Removes the given element.
     *
     * References, pointers and iterators to the erased element are invalidated. All other iterators, pointers and
     * references remain valid.
     *
     * @param pos The \c const_find_iterator to the element to remove. This iterator must be valid and dereferenceable.
     * @returns the \c find_iterator immediately following \p pos.
     */
    find_iterator erase(const_find_iterator pos)
    {
        CARB_ASSERT(pos.owner == this);
        assertContained(pos.where);

        find_iterator next{ this, _findnext(pos.where) };
        internal_erase(pos.where);
        return next;
    }

    /**
     * Removes the elements in the given range.
     *
     * The range `[first, last)` must be a valid range in `*this`. References, pointers and iterators to erased elements
     * are invalidated. All other iterators, pointers and references to other elements remain valid.
     *
     * @param first The start of the range of iterators to remove.
     * @param last The past-the-end iterator for the range to remove.
     * @returns \p last
     */
    find_iterator erase(const_find_iterator first, const_find_iterator last)
    {
        while (first != last)
            first = erase(first);
        return { this, first.where };
    }

    /**
     * Finds the first element with the specified key.
     *
     * @note \c find_iterator objects returned from this function will only iterate through elements with the same key;
     * they cannot be used to iterate through the entire container.
     *
     * @param key The key of the element(s) to search for.
     * @returns a \c find_iterator to the first element matching \p key, or \ref end() if no element was found matching
     * \p key.
     */
    find_iterator find(const key_type& key)
    {
        return { this, internal_find(key) };
    }

    /**
     * Finds the first element with the specified key.
     *
     * @note \c const_find_iterator objects returned from this function will only iterate through elements with the same
     * key; they cannot be used to iterate through the entire container.
     *
     * @param key The key of the element(s) to search for.
     * @returns a \c const_find_iterator to the first element matching \p key, or \ref cend() if no element was found
     * matching \p key.
     */
    const_find_iterator find(const key_type& key) const
    {
        return { this, internal_find(key) };
    }

    /**
     * Returns whether there is at least one element matching a given key in the container.
     *
     * @note This function can be faster than \c count() for multimap and multiset containers.
     * @param key The key of the element to search for.
     * @returns \c true if at least one element matching \p key exists in the container; \c false otherwise.
     */
    bool contains(const key_type& key) const
    {
        return !!internal_find(key);
    }

    /**
     * Returns a range containing all elements with the given key.
     *
     * @note \c find_iterator objects returned from this function will only iterate through elements with the same key;
     * they cannot be used to iterate through the entire container.
     *
     * @param key The key of the element(s) to search for.
     * @returns A \c pair containing a pair of iterators for the desired range. If there are no such elements, both
     * iterators will be \ref end().
     */
    std::pair<find_iterator, find_iterator> equal_range(const key_type& key)
    {
        auto vt = this->internal_find(key);
        find_iterator fend{ this, nullptr };
        return vt ? std::make_pair(find_iterator{ this, vt }, fend) : std::make_pair(fend, fend);
    }

    /**
     * Returns a range containing all elements with the given key.
     *
     * @note \c const_find_iterator objects returned from this function will only iterate through elements with the same
     * key; they cannot be used to iterate through the entire container.
     *
     * @param key The key of the element(s) to search for.
     * @returns A \c pair containing a pair of iterators for the desired range. If there are no such elements, both
     * iterators will be \ref end().
     */
    std::pair<const_find_iterator, const_find_iterator> equal_range(const key_type& key) const
    {
        auto vt = this->internal_find(key);
        const_find_iterator fend{ this, nullptr };
        return vt ? std::make_pair(const_find_iterator{ this, vt }, fend) : std::make_pair(fend, fend);
    }

    /**
     * Reserves space for at least the specified number of elements and regenerates the hash table.
     *
     * Sets \ref capacity() of \c *this to a value greater-than-or-equal-to \p n. If \ref capacity() already exceeds
     * \p n, nothing happens.
     *
     * If a rehash occurs, all iterators, pointers and references to existing elements are invalidated.
     *
     * @param n The desired minimum capacity of \c *this.
     */
    void reserve(size_t n)
    {
        if (n > capacity())
        {
            rehash(n);
        }
    }

    /**
     * Sets the capacity of the container to the lowest valid value greater-than-or-equal-to the given value, and
     * rehashes the container.
     *
     * If \p n is less-than \ref size(), \ref size() is used instead.
     *
     * The value is computed as if by `cpp20::bit_ceil(std::ceil(float(n * 100) / float(LoadFactorMax100)))` with a
     * minimum size of 8.
     *
     * If the container is empty and \p n is zero, the memory for the container is freed.
     *
     * After this function is called, all iterators, pointers and references to existing elements are invalidated.
     *
     * @param n The minimum capacity for the container. The actual size of the container may be larger than this.
     */
    void rehash(size_t n)
    {
        if (n < m_data.m_size)
            n = m_data.m_size;

        if (n == 0)
        {
            std::free(m_data.m_table);
            m_data.m_table = nullptr;
            m_data.m_hashes = nullptr;
            m_data.m_tableSize = 0;
            return;
        }

        ncvalue_type* const oldtable = m_data.m_table;
        size_t* const oldhashes = m_data.m_hashes;
        size_t* const oldend = oldhashes + m_data.m_tableSize;

        size_t minsize = (n * 100 + (LoadFactorMax100 - 1)) / LoadFactorMax100; // ceiling (round up)
        constexpr static auto kMinSize_ = kMinSize; // CC-1110
        size_t newsize = ::carb_max(kMinSize_, cpp20::bit_ceil(minsize)); // must be a power of 2

        m_data.m_tableSize = newsize;
        CARB_ASSERT(capacity() >= m_data.m_size);

        m_data.m_table =
            static_cast<ncvalue_type*>(std::malloc(m_data.m_tableSize * (sizeof(ncvalue_type) + sizeof(size_t))));
        m_data.m_hashes = reinterpret_cast<size_t*>(m_data.m_table + m_data.m_tableSize);
        size_t* const end = m_data.m_hashes + m_data.m_tableSize;

        // Initialize
        for (size_t* e = m_data.m_hashes; e != end; ++e)
            *e = kEmpty;

        for (size_t* e = oldhashes; e != oldend; ++e)
            if (isHashValid(*e))
            {
                auto result = internal_insert_multi2(*e, KeyFromValue{}(oldtable[e - oldhashes]));
                new (result) ncvalue_type(std::move(oldtable[e - oldhashes]));
                oldtable[e - oldhashes].~value_type();
            }

        std::free(oldtable);
    }

#ifndef DOXYGEN_BUILD
protected:
    constexpr static size_t kDeletedBit = size_t(1) << (8 * sizeof(size_t) - 1);
    constexpr static size_t kEmpty = size_t(-1) & ~kDeletedBit;
    constexpr static size_t kMinSize = 8; // Minimum hash table size
    static_assert(cpp20::has_single_bit(kMinSize), "Must be power of 2");

    using ncvalue_type = typename std::remove_const_t<value_type>;

    static constexpr bool isEmpty(size_t h) noexcept
    {
        return h == kEmpty;
    }
    static constexpr bool isDeleted(size_t h) noexcept
    {
        return !!(h & kDeletedBit);
    }
    static constexpr bool isHashValid(size_t h) noexcept
    {
        return !(isDeleted(h) | isEmpty(h));
    }

    struct Data : public Hasher, public Equals
    {
        CARB_VIZ ncvalue_type* m_table{ nullptr };
        CARB_VIZ size_t* m_hashes{ nullptr };
        CARB_VIZ size_t m_size{ 0 };
        CARB_VIZ size_t m_tableSize{ 0 };
    };

    size_t hash(const key_type& key) const noexcept
    {
        const Hasher& hasher = m_data;
        size_t h = size_t(hasher(key)) & ~kDeletedBit;
        return h ^ (h == kEmpty);
    }

    bool equals(const key_type& k1, const key_type& k2) const noexcept
    {
        const Equals& e = m_data;
        return e(k1, k2);
    }

    void assertContained(const value_type* v) const
    {
        CARB_UNUSED(v);
        CARB_ASSERT(v >= m_data.m_table && v < (m_data.m_table + m_data.m_tableSize));
    }

    const size_t* _end() const noexcept
    {
        return m_data.m_hashes + m_data.m_tableSize;
    }

    value_type* _next(value_type* prev) const
    {
        assertContained(prev);

        const size_t* e = m_data.m_hashes + (prev - m_data.m_table) + 1;
        for (; e != _end(); ++e)
            if (isHashValid(*e))
                return m_data.m_table + (e - m_data.m_hashes);

        return nullptr;
    }

    value_type* _findnext(value_type* prev) const
    {
        assertContained(prev);

        size_t const h = m_data.m_hashes[(prev - m_data.m_table)];
        CARB_ASSERT(isHashValid(h));
        key_type const& key = KeyFromValue{}(*prev);

        size_t const kMask = (m_data.m_tableSize - 1);
        size_t const start = (h & kMask); // starting index of the search. If we get back to this point, we're done.

        size_t index = ((prev - m_data.m_table) + 1) & kMask;
        size_t dist = (index - start) & kMask;

        for (; index != start; index = (index + 1) & kMask, ++dist)
        {
            size_t* e = m_data.m_hashes + index;

            if (isEmpty(*e))
            {
                return nullptr;
            }

            if (*e == h && equals(KeyFromValue{}(m_data.m_table[index]), key))
            {
                return m_data.m_table + index;
            }

            size_t entryDist = (index - *e) & kMask;
            if (dist > entryDist)
            {
                return nullptr;
            }
        }

        return nullptr;
    }

    constexpr iterator make_iter(value_type* where) const noexcept
    {
        return iterator{ this, where };
    }

    std::pair<iterator, bool> insert_unique(const value_type& value)
    {
        auto result = internal_insert(KeyFromValue{}(value));
        if (result.second)
            new (result.first) value_type(value);
        return std::make_pair(iterator{ this, result.first }, result.second);
    }

    std::pair<iterator, bool> insert_unique(value_type&& value)
    {
        auto result = internal_insert(KeyFromValue{}(value));
        if (result.second)
            new (result.first) value_type(std::move(value));
        return std::make_pair(iterator{ this, result.first }, result.second);
    }

    iterator insert_multi(const value_type& value)
    {
        return { this, new (internal_insert_multi(KeyFromValue{}(value))) value_type(value) };
    }

    iterator insert_multi(value_type&& value)
    {
        return { this, new (internal_insert_multi(KeyFromValue{}(value))) value_type(std::move(value)) };
    }

    ncvalue_type* internal_insert_multi(const key_type& key)
    {
        if (((m_data.m_size) + 1) >= capacity())
        {
            reserve(m_data.m_size + 1);
        }

        CARB_ASSERT(m_data.m_size < m_data.m_tableSize);
        size_t h = hash(key);
        ++m_data.m_size;
        return internal_insert_multi2(h, key);
    }

    ncvalue_type* internal_insert_multi2(size_t h, const key_type& key)
    {
        size_t const kMask = (m_data.m_tableSize - 1);
        size_t index = h & kMask;
        size_t last = (index - 1) & kMask;

        size_t dist = 0; // distance from desired slot

        size_t* e;
        for (;;)
        {
            e = m_data.m_hashes + index;

            if (isEmpty(*e))
            {
                *e = h;
                return m_data.m_table + index;
            }

            // Compute the distance of the existing item or deleted entry
            size_t const existingDist = (index - *e) & kMask;
            if (isDeleted(*e))
            {
                // The evicted item can only go into a deleted slot only if it's "fair": our distance-from-desired must
                // be same or worse than the existing deleted item.
                if (dist >= existingDist)
                {
                    // We can take a deleted entry that meets or exceeds our desired distance
                    *e = h;
                    return m_data.m_table + (e - m_data.m_hashes);
                }
            }
            else if (dist > existingDist)
            {
                // Our distance from desired now exceeds the current entry, so we'll take it and evict whatever was
                // previously there. Proceed to the next phase to find a spot for the evicted entry.
                dist = existingDist;
                break;
            }

            if (CARB_UNLIKELY(index == last))
            {
                // We reached the end without finding a valid spot, but there are deleted entries in the table. So
                // rebuild to remove all of the deleted entries and recursively call.
                rebuild();
                return internal_insert_multi2(h, key);
            }

            index = (index + 1) & kMask;
            ++dist;
        }

        // At this point, we have to evict an existing item in order to insert at a fair position. The slot that will
        // contain our new entry is pointed at by `orig`. Our caller will be responsible for initializing the value_type

        ncvalue_type* const orig = m_data.m_table + (e - m_data.m_hashes);
        std::swap(*e, h);
        ncvalue_type value(std::move(*orig));
        orig->~value_type(); // caller will need to reconstruct.

        // We are now taking the perspective of the evicted item. `h` is already the hash value for the evicted item, so
        // recompute `last`. `dist` is already the distance from desired for the evicted item as well.
        last = (h - 1) & kMask;

        // Start with the following index as it is the first candidate for the evicted item.
        index = (index + 1) & kMask;
        ++dist;

        for (;;)
        {
            e = m_data.m_hashes + index;

            if (isEmpty(*e))
            {
                // Found an empty slot that the evicted item can move into.
                *e = h;
                new (m_data.m_table + index) value_type(std::move(value));
                return orig;
            }

            size_t existingDist = (index - *e) & kMask;
            if (isDeleted(*e))
            {
                // The evicted item can only go into a deleted slot only if it's "fair": our distance-from-desired must
                // be same or worse than the existing deleted item.
                if (dist >= existingDist)
                {
                    *e = h;
                    new (m_data.m_table + index) value_type(std::move(value));
                    return orig;
                }
            }
            else if (dist > existingDist)
            {
                // For an existing item, we can swap it out with the previously evicted item if it is worse off than the
                // item at this location. It becomes the new evicted item and we continue traversal until we find a
                // suitable location for it.
                std::swap(*e, h);
                swapValue(value, m_data.m_table[e - m_data.m_hashes]);
                dist = existingDist;
                last = (h - 1) & kMask;
            }

            if (index == last)
            {
                // We're in a bad state. There are too many deleted items and we've walked the entire list trying to
                // find a location. Do a rebuild to remove all the deleted entries and re-hash everything then call
                // recursively.
                std::swap(m_data.m_hashes[orig - m_data.m_table], h);
                // Reconstruct the item at orig since it was previously deleted.
                new (orig) value_type(std::move(value));
                CARB_ASSERT(h == hash(key));
                rebuild();
                return internal_insert_multi2(h, key);
            }

            index = (index + 1) & kMask;
            ++dist;
        }
    }

    std::pair<ncvalue_type*, bool> internal_insert(const key_type& key)
    {
        if ((m_data.m_size + 1) >= capacity())
        {
            reserve(m_data.m_size + 1);
        }

        CARB_ASSERT(m_data.m_size < m_data.m_tableSize);
        size_t h = hash(key);
        auto result = internal_insert2(h, key);
        m_data.m_size += result.second;
        return result;
    }

    std::pair<ncvalue_type*, bool> internal_insert2(size_t h, const key_type& key)
    {
        size_t const kMask = (m_data.m_tableSize - 1);
        size_t index = h & kMask;
        size_t last = (index - 1) & kMask;

        size_t dist = 0; // distance from desired slot
        size_t* firstDeletedSlot = nullptr;

        size_t* e;
        for (;;)
        {
            e = m_data.m_hashes + index;

            if (isEmpty(*e))
            {
                *e = h;
                return std::make_pair(m_data.m_table + index, true);
            }

            if (*e == h && equals(KeyFromValue{}(m_data.m_table[index]), key))
            {
                return std::make_pair(m_data.m_table + index, false);
            }

            // Compute the distance of the existing item or deleted entry
            size_t const existingDist = (index - *e) & kMask;
            if (dist > existingDist)
            {
                // Our distance from desired now exceeds the current entry, so we'll take it. If it's deleted, we can
                // merely take it, but if something already exists there, we need to move it down the line.
                if (!firstDeletedSlot && isDeleted(*e))
                {
                    firstDeletedSlot = e;
                }

                if (firstDeletedSlot)
                {
                    // If we found a deleted slot, we can go into it
                    *firstDeletedSlot = h;
                    return std::make_pair(m_data.m_table + (firstDeletedSlot - m_data.m_hashes), true);
                }

                if (CARB_UNLIKELY(index == last))
                {
                    // We reached the end without finding a valid spot, but there are deleted entries in the table. So
                    // rebuild to remove all of the deleted entries and recursively call.
                    rebuild();
                    return internal_insert2(h, key);
                }

                // We break out and proceed to find a new location for the existing entry
                dist = existingDist;
                break;
            }
            else if (!firstDeletedSlot && dist == existingDist && isDeleted(*e))
            {
                firstDeletedSlot = e;
            }

            if (CARB_UNLIKELY(index == last))
            {
                // We reached the end without finding a valid spot. Rebuild to remove all deleted entries and call
                // recursively.
                rebuild();
                return internal_insert2(h, key);
            }

            index = (index + 1) & kMask;
            ++dist;
        }

        // At this point, we guarantee that we need to insert and we had to evict an existing item. The slot that will
        // contain our new entry is pointed at by `orig`. Our caller will be responsible for initializing the value_type

        ncvalue_type* const orig = m_data.m_table + (e - m_data.m_hashes);
        std::swap(*e, h);
        ncvalue_type value(std::move(*orig));
        orig->~value_type(); // caller will need to reconstruct.

        // We are now taking the perspective of the evicted item. `h` is already the hash value for the evicted item, so
        // recompute `last`. `dist` is already the distance from desired for the evicted item as well.
        last = (h - 1) & kMask;

        // Start with the following index as it is the first candidate for the evicted item.
        index = (index + 1) & kMask;
        ++dist;

        for (;;)
        {
            e = m_data.m_hashes + index;

            if (isEmpty(*e))
            {
                // Found an empty slot that the evicted item can move into.
                *e = h;
                new (m_data.m_table + index) value_type(std::move(value));
                return std::make_pair(orig, true);
            }

            size_t existingDist = (index - *e) & kMask;
            if (isDeleted(*e))
            {
                // The evicted item can only go into a deleted slot only if it's "fair": our distance-from-desired must
                // be same or worse than the existing deleted item.
                if (dist >= existingDist)
                {
                    *e = h;
                    new (m_data.m_table + index) value_type(std::move(value));
                    return std::make_pair(orig, true);
                }
            }
            else if (dist > existingDist)
            {
                // For an existing item, we can swap it out with the previously evicted item if it is worse off than the
                // item at this location. It becomes the new evicted item and we continue traversal until we find a
                // suitable location for it.
                std::swap(*e, h);
                swapValue(value, m_data.m_table[e - m_data.m_hashes]);
                dist = existingDist;
                last = (h - 1) & kMask;
            }

            if (index == last)
            {
                // We're in a bad state. There are too many deleted items and we've walked the entire list trying to
                // find a location. Do a rebuild to remove all the deleted entries and re-hash everything then call
                // recursively.
                std::swap(m_data.m_hashes[orig - m_data.m_table], h);
                // Reconstruct the item at orig since it was previously deleted.
                new (orig) value_type(std::move(value));
                CARB_ASSERT(h == hash(key));
                rebuild();
                return internal_insert2(h, key);
            }

            index = (index + 1) & kMask;
            ++dist;
        }
    }

    size_t internal_count_multi(const key_type& key) const
    {
        size_t count = 0;
        for (auto vt = internal_find(key); vt; vt = _findnext(vt))
            ++count;
        return count;
    }

    void internal_erase(value_type* value)
    {
        --m_data.m_size;
        value->~value_type();
        size_t index = value - m_data.m_table;
        size_t* h = m_data.m_hashes + index;
        // Set the deleted bit, but we retain most bits in the hash so that distance checks work properly.
        *h |= kDeletedBit;

        // If our next entry is kEmpty, then we can walk backwards and set everything to kEmpty. This will keep the
        // table a bit cleaner of deleted entries.
        size_t const kMask = m_data.m_tableSize - 1;
        if (isEmpty(m_data.m_hashes[(index + 1) & kMask]))
        {
            do
            {
                *h = kEmpty;
                index = (index - 1) & kMask;
                h = m_data.m_hashes + index;
            } while (isDeleted(*h));
        }
    }

    value_type* internal_find(const key_type& key) const
    {
        if (CARB_UNLIKELY(empty()))
            return nullptr;

        size_t const h = hash(key);
        size_t const kMask = size_t(m_data.m_tableSize - 1);
        size_t index = h & kMask;
        size_t dist = 0;

        for (;;)
        {
            size_t* e = m_data.m_hashes + index;

            if (isEmpty(*e))
                return nullptr;

            if (*e == h && equals(KeyFromValue{}(m_data.m_table[index]), key))
            {
                return m_data.m_table + index;
            }

            size_t entryDist = (index - *e) & kMask;
            if (dist > entryDist)
                return nullptr;

            // We do not need to check against the last entry here because distance keeps increasing. Eventually it will
            // be larger than the number of items in the map.

            ++dist;
            index = (index + 1) & kMask;
        }
    }

    static void swapValue(ncvalue_type& v1, ncvalue_type& v2) noexcept
    {
        // Cannot use std::swap because key is const, so work around it with move-construct/destruction
        ncvalue_type temp{ std::move(v1) };
        v1.~value_type();
        new (&v1) ncvalue_type(std::move(v2));
        v2.~value_type();
        new (&v2) ncvalue_type(std::move(temp));
    }

    // Similar to rehash except that it keeps the same table size
    void rebuild()
    {
        ncvalue_type* const oldtable = m_data.m_table;
        size_t* const oldhashes = m_data.m_hashes;
        size_t* const oldend = oldhashes + m_data.m_tableSize;

        m_data.m_table =
            static_cast<ncvalue_type*>(std::malloc(m_data.m_tableSize * (sizeof(ncvalue_type) + sizeof(size_t))));
        m_data.m_hashes = reinterpret_cast<size_t*>(m_data.m_table + m_data.m_tableSize);
        size_t* const end = m_data.m_hashes + m_data.m_tableSize;

        for (size_t* e = m_data.m_hashes; e != end; ++e)
            *e = kEmpty;

        if (CARB_LIKELY(m_data.m_size != 0))
        {
            for (size_t* e = oldhashes; e != oldend; ++e)
            {
                if (isHashValid(*e))
                {
                    auto result = internal_insert_multi2(*e, KeyFromValue{}(oldtable[e - oldhashes]));
                    new (result) ncvalue_type(std::move(oldtable[e - oldhashes]));
                    oldtable[e - oldhashes].~value_type();
                }
            }
        }

        std::free(oldtable);
    }
#endif

private:
    CARB_VIZ Data m_data;
};

/**
 * ADL swap function. Swaps between two containers that have the same template parameters.
 *
 * @param lhs The first container
 * @param rhs The second container.
 */
template <size_t LoadFactorMax100, class Key, class ValueType, class KeyFromValue, class Hasher, class Equals>
void swap(RobinHood<LoadFactorMax100, Key, ValueType, KeyFromValue, Hasher, Equals>& lhs,
          RobinHood<LoadFactorMax100, Key, ValueType, KeyFromValue, Hasher, Equals>& rhs)
{
    lhs.swap(rhs);
}

/**
 * Equality operator. Checks for equality between two containers.
 *
 * @param lhs The first container.
 * @param rhs The second container.
 * @returns \c true if the containers are equal, that is, the second container is a permutation of the first.
 */
template <size_t LoadFactorMax100_1, size_t LoadFactorMax100_2, class Key, class ValueType, class KeyFromValue, class Hasher1, class Hasher2, class Equals>
bool operator==(const RobinHood<LoadFactorMax100_1, Key, ValueType, KeyFromValue, Hasher1, Equals>& lhs,
                const RobinHood<LoadFactorMax100_2, Key, ValueType, KeyFromValue, Hasher2, Equals>& rhs)
{
    Equals equals{};
    return std::is_permutation(
        lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), [&equals](const ValueType& l, const ValueType& r) {
            KeyFromValue kfv{};
            return equals(kfv(l), kfv(r));
        });
}

/**
 * Inequality operator. Checks for inequality between two containers.
 *
 * @param lhs The first container.
 * @param rhs The second container.
 * @returns \c true if the containers are not equal, that is, the second container is not a permutation of the first.
 */
template <size_t LoadFactorMax100_1, size_t LoadFactorMax100_2, class Key, class ValueType, class KeyFromValue, class Hasher1, class Hasher2, class Equals>
bool operator!=(const RobinHood<LoadFactorMax100_1, Key, ValueType, KeyFromValue, Hasher1, Equals>& lhs,
                const RobinHood<LoadFactorMax100_2, Key, ValueType, KeyFromValue, Hasher2, Equals>& rhs)
{
    return !(lhs == rhs);
}

} // namespace detail
} // namespace container
} // namespace carb
