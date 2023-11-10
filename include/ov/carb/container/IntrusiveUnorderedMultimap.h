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
//! @brief Carbonite intrusive unordered multi-map container.
#pragma once

#include "../Defines.h"

#include "../cpp20/Bit.h"

#include <cmath>
#include <functional>
#include <iterator>
#include <memory>

namespace carb
{
namespace container
{

template <class Key, class T>
class IntrusiveUnorderedMultimapLink;

template <class Key, class T, IntrusiveUnorderedMultimapLink<Key, T> T::*U, class Hash, class Pred>
class IntrusiveUnorderedMultimap;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail
{

struct NontrivialDummyType
{
    constexpr NontrivialDummyType() noexcept
    {
    }
};
static_assert(!std::is_trivially_default_constructible<NontrivialDummyType>::value, "Invalid assumption");

template <class Key, class T>
class IntrusiveUnorderedMultimapLinkBase
{
public:
    using KeyType = const Key;
    using MappedType = T;
    using ValueType = std::pair<const Key, T&>;

    constexpr IntrusiveUnorderedMultimapLinkBase() noexcept = default;
    ~IntrusiveUnorderedMultimapLinkBase()
    {
        // Shouldn't be contained at destruction time
        CARB_ASSERT(!isContained());
    }

    bool isContained() const noexcept
    {
        return m_next != nullptr;
    }

private:
    template <class Key2, class U, IntrusiveUnorderedMultimapLink<Key2, U> U::*V, class Hash, class Pred>
    friend class ::carb::container::IntrusiveUnorderedMultimap;

    IntrusiveUnorderedMultimapLink<Key, T>* m_next{ nullptr };
    IntrusiveUnorderedMultimapLink<Key, T>* m_prev{ nullptr };

    CARB_PREVENT_COPY_AND_MOVE(IntrusiveUnorderedMultimapLinkBase);

    constexpr IntrusiveUnorderedMultimapLinkBase(IntrusiveUnorderedMultimapLink<Key, T>* init) noexcept
        : m_next(init), m_prev(init)
    {
    }
};

} // namespace detail
#endif

/**
 * Defines a "link node" that \ref IntrusiveUnorderedMultimap will use for tracking data for the contained type.
 */
template <class Key, class T>
class IntrusiveUnorderedMultimapLink : public detail::IntrusiveUnorderedMultimapLinkBase<Key, T>
{
    using Base = detail::IntrusiveUnorderedMultimapLinkBase<Key, T>;

public:
    //! Constructor.
    constexpr IntrusiveUnorderedMultimapLink() noexcept : empty{}
    {
    }

    //! Destructor. Asserts that the link is not contained in an @ref IntrusiveUnorderedMultimap.
    ~IntrusiveUnorderedMultimapLink()
    {
        // Shouldn't be contained at destruction time
        CARB_ASSERT(!this->isContained());
    }

private:
    template <class Key2, class U, IntrusiveUnorderedMultimapLink<Key2, U> U::*V, class Hash, class Pred>
    friend class IntrusiveUnorderedMultimap;

    union
    {
        detail::NontrivialDummyType empty;
        typename Base::ValueType value;
    };

    CARB_PREVENT_COPY_AND_MOVE(IntrusiveUnorderedMultimapLink);
};

/**
 * IntrusiveUnorderedMultimap is a closed-addressing hash table very similar to std::unordered_multimap, but requires
 * the tracking information to be contained within the stored type `T`, rather than built around it. In other words, the
 * tracking information is "intrusive" in the type `T` by way of the IntrusiveUnorderedMultimapLink type.
 * IntrusiveUnorderedMultimap does no allocation of the `T` type; all allocation is done outside of the context of
 * IntrusiveUnorderedMultimap, which allows stored items to be on the stack, grouped with other items, etc.
 *
 * The impetus behind intrusive containers is specifically to allow the application to own the allocation patterns for a
 * type, but still be able to store them in a container. For `std::unordered_multimap`, everything goes through an
 * `Allocator` type, but in a real application some stored instances may be on the stack while others are on the heap,
 * which makes using `std::unordered_multimap` impractical. Furthermore, a stored type may which to be removed from one
 * list and inserted into another. With `std::unordered_multimap`, this would require heap interaction to erase from one
 * list and insert into another. With IntrusiveUnorderedMultimap, this operation would not require any heap interaction
 * and would be done very quickly (O(1)).
 *
 * Another example is a `std::unordered_multimap` of polymorphic types. For `std::unordered_multimap` the mapped type
 * would have to be a pointer or pointer-like type which is an inefficient use of space, cache, etc. The
 * \c IntrusiveUnorderedMultimapLink can be part of the contained object which is a more efficient use of space.
 *
 * Since IntrusiveUnorderedMultimap doesn't require any form of `Allocator`, the allocation strategy is completely left
 * up to the application. This means that items could be allocated on the stack, pooled, or items mixed between stack
 * and heap.
 *
 * An intrusive unique-map (i.e. an intrusive equivalent to `std::unordered_map`) is impractical because allocation is
 * not the responsibility of the container. For `std::unordered_map::insert`, if a matching key already exists, a new
 * entry is not created and an iterator to the existing entry is returned. Conversely, with the intrusive container, the
 * insert function is being given an already-created object that cannot be destroyed. It is therefore up to the
 * application to ensure uniqueness if desired. Similarly, the existence of an intrusive (multi-)set is impractical
 * since a type `T` is required to be contained and a custom hasher/equality-predicate would have to be written to
 * support it--it would be simpler to use @ref carb::container::IntrusiveList.
 *
 * It is important to select a good hashing function in order to reduce collisions that may sap performance. Generally
 * \c std::hash is used which is typically based on the FNV-1a hash algorithm. Hash computation is only done for finding
 * the bucket that would contain an item. Once the bucket is selected, the \c Pred template parameter is used to compare
 * keys until a match is found. A truly ideal hash at the default load factor of \c 1.0 results in a single entry per
 * bucket; however, this is not always true in practice. Hash collisions cause multiple items to fall into the same
 * bucket, increasing the amount of work that must be done to find an item.
 *
 * Iterator invalidation mirrors that of `std::unordered_multimap`: rehashing invalidates iterators and may cause
 * elements to be rearranged into different buckets, but does not invalidate references or pointers to keys or the
 * mapped type.
 *
 * IntrusiveUnorderedMultimap matches `std::unordered_multimap` with the following exceptions:
 * - The `iterator` and `initializer_list` constructor variants are not present in IntrusiveUnorderedMultimap.
 * - The `iterator` and `initializer_list` insert() variants are not present in IntrusiveUnorderedMultimap.
 * - IntrusiveUnorderedMultimap cannot be copied (though may still be moved).
 * - IntrusiveUnorderedMultimap does not have `erase()` to erase an item from the list, but instead has remove() which
 *   will remove an item from the container. It is up to the caller to manage the memory for the item.
 * - Likewise, clear() functions as a "remove all" and does not destroy items in the container.
 * - iter_from_value() is a new function that translates an item contained in IntrusiveUnorderedMultimap into an
 *   iterator.
 * - `local_iterator` and `begin(size_type)`/`end(size_type)` are not implemented.
 *
 * Example:
 * @code{.cpp}
 *     class Subscription {
 *     public:
 *         IntrusiveUnorderedMultimapLink<std::string, Subscription> link;
 *         void notify();
 *     };
 *
 *     IntrusiveUnorderedMultimap<std::string, &Subscription::link> map;
 *
 *     Subscription sub;
 *     map.emplace("my subscription", sub);
 *
 *     // Notify all subscriptions:
 *     for (auto& entry : map)
 *         entry.second.notify();
 *
 *     map.remove("my subscription");
 * @endcode
 *
 * @tparam Key A key to associate with the mapped data.
 * @tparam T The mapped data, referred to by `Key`.
 * @tparam U A pointer-to-member of `T` that must be of type @ref IntrusiveUnorderedMultimapLink. This member will be
 *     used by the IntrusiveUnorderedMultimap for storing the key type and tracking the contained object.
 * @tparam Hash A class that is used to hash the key value. Better hashing functions produce better performance.
 * @tparam Pred A class that is used to equality-compare two key values.
 */
template <class Key, class T, IntrusiveUnorderedMultimapLink<Key, T> T::*U, class Hash = ::std::hash<Key>, class Pred = ::std::equal_to<Key>>
class IntrusiveUnorderedMultimap
{
    using BaseLink = detail::IntrusiveUnorderedMultimapLinkBase<Key, T>;

public:
    //! The type of the key; type `Key`
    using KeyType = const Key;
    //! The type of the mapped data; type `T`
    using MappedType = T;
    //! The type of the @ref IntrusiveUnorderedMultimapLink that must be a member of MappedType.
    using Link = IntrusiveUnorderedMultimapLink<Key, T>;
    //! Helper definition for `std::pair<const Key, T&>`.
    using ValueType = typename Link::ValueType;

    // Iterator support
    // clang-format off
    //! A LegacyForwardIterator to `const ValueType`.
    //! @see <a href="https://en.cppreference.com/w/cpp/named_req/ForwardIterator">LegacyForwardIterator</a>
    class const_iterator
    {
    public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
        using iterator_category = std::forward_iterator_tag;
        using value_type = ValueType;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;
        const_iterator() noexcept = default;
        reference       operator *  () const                    { assertNotEnd(); return m_where->value; }
        pointer         operator -> () const                    { assertNotEnd(); return std::addressof(operator*()); }
        const_iterator& operator ++ () noexcept    /* ++iter */ { assertNotEnd(); incr(); return *this; }
        const_iterator  operator ++ (int) noexcept /* iter++ */ { assertNotEnd(); const_iterator i{ *this }; incr(); return i; }
        bool operator == (const const_iterator& rhs) const noexcept { assertSameOwner(rhs); return m_where == rhs.m_where; }
        bool operator != (const const_iterator& rhs) const noexcept { assertSameOwner(rhs); return m_where != rhs.m_where; }
    protected:
        friend class IntrusiveUnorderedMultimap;
        Link* m_where{ nullptr };
#if CARB_ASSERT_ENABLED
        const IntrusiveUnorderedMultimap* m_owner{ nullptr };
        const_iterator(Link* where, const IntrusiveUnorderedMultimap* owner) noexcept : m_where(where), m_owner(owner) {}
        void assertOwner(const IntrusiveUnorderedMultimap* other) const noexcept
        {
            CARB_ASSERT(m_owner == other, "IntrusiveUnorderedMultimap iterator for invalid container");
        }
        void assertSameOwner(const const_iterator& rhs) const noexcept
        {
            CARB_ASSERT(m_owner == rhs.m_owner, "IntrusiveUnorderedMultimap iterators are from different containers");
        }
        void assertNotEnd() const noexcept
        {
            CARB_ASSERT(m_where != m_owner->_end(), "Invalid operation on IntrusiveUnorderedMultimap::end() iterator");
        }
#else
        const_iterator(Link* where, const IntrusiveUnorderedMultimap*) noexcept : m_where(where) {}
        void assertOwner(const IntrusiveUnorderedMultimap* other) const noexcept
        {
            CARB_UNUSED(other);
        }
        void assertSameOwner(const const_iterator& rhs) const noexcept
        {
            CARB_UNUSED(rhs);
        }
        void assertNotEnd() const noexcept {}
#endif // !CARB_ASSERT_ENABLED
        void incr() noexcept { m_where = m_where->m_next; }
#endif // !DOXYGEN_SHOULD_SKIP_THIS
    };

    //! A LegacyForwardIterator to `ValueType`.
    //! @see <a href="https://en.cppreference.com/w/cpp/named_req/ForwardIterator">LegacyForwardIterator</a>
    class iterator : public const_iterator
    {
        using Base = const_iterator;
    public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
        using iterator_category = std::forward_iterator_tag;
        using value_type = ValueType;
        using difference_type = ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;
        iterator() noexcept = default;
        reference operator *  () const                    { this->assertNotEnd(); return this->m_where->value; }
        pointer   operator -> () const                    { this->assertNotEnd(); return std::addressof(operator*()); }
        iterator& operator ++ () noexcept    /* ++iter */ { this->assertNotEnd(); this->incr(); return *this; }
        iterator  operator ++ (int) noexcept /* iter++ */ { this->assertNotEnd(); iterator i{ *this }; this->incr(); return i; }
    private:
        friend class IntrusiveUnorderedMultimap;
        iterator(Link* where, const IntrusiveUnorderedMultimap* owner) : Base(where, owner) {}
#endif
    };
    // clang-format on

    CARB_PREVENT_COPY(IntrusiveUnorderedMultimap);

    //! Constructor. Initializes `*this` to be empty().
    constexpr IntrusiveUnorderedMultimap() : m_list(_end())
    {
    }

    //! Move-construct. Moves all entries to `*this` from @p other and leaves it empty.
    //! @param other Another IntrusiveUnorderedMultimap to move entries from.
    IntrusiveUnorderedMultimap(IntrusiveUnorderedMultimap&& other) noexcept : m_list(_end())
    {
        swap(other);
    }

    //! Destructor. Implies clear().
    ~IntrusiveUnorderedMultimap()
    {
        clear();
        m_list.m_next = m_list.m_prev = nullptr; // Prevents the assert
    }

    //! Move-assign. Moves all entries from @p other and leaves @p other in a valid but possibly non-empty state.
    //! @param other Another IntrusiveUnorderedMultimap to move entries from.
    IntrusiveUnorderedMultimap& operator=(IntrusiveUnorderedMultimap&& other) noexcept
    {
        swap(other);
        return *this;
    }

    //! Checks whether the container is empty.
    //! @returns `true` if `*this` is empty; `false` otherwise.
    bool empty() const noexcept
    {
        return !m_size;
    }

    //! Returns the number of elements contained.
    //! @returns The number of elements contained.
    size_t size() const noexcept
    {
        return m_size;
    }

    //! Returns the maximum possible number of elements.
    //! @returns The maximum possible number of elements.
    size_t max_size() const noexcept
    {
        return size_t(-1);
    }

    // Iterator support
    //! Returns an iterator to the beginning.
    //! @returns An iterator to the beginning.
    iterator begin() noexcept
    {
        return iterator(_head(), this);
    }

    //! Returns an iterator to the end.
    //! @returns An iterator to the end.
    iterator end() noexcept
    {
        return iterator(_end(), this);
    }

    //! Returns a const_iterator to the beginning.
    //! @returns A const_iterator to the beginning.
    const_iterator cbegin() const noexcept
    {
        return const_iterator(_head(), this);
    }

    //! Returns a const_iterator to the end.
    //! @returns A const_iterator to the end.
    const_iterator cend() const noexcept
    {
        return const_iterator(_end(), this);
    }

    //! Returns a const_iterator to the beginning.
    //! @returns A const_iterator to the beginning.
    const_iterator begin() const noexcept
    {
        return cbegin();
    }

    //! Returns a const_iterator to the end.
    //! @returns A const_iterator to the end.
    const_iterator end() const noexcept
    {
        return cend();
    }

    //! Returns an iterator to the given value if it is contained in `*this`, otherwise returns `end()`. O(n).
    //! @param value The value to check for.
    //! @returns An @ref iterator to @p value if contained in `*this`; end() otherwise.
    iterator locate(T& value) noexcept
    {
        Link* l = _link(value);
        return iterator(l->isContained() ? _listfind(value) : _end(), this);
    }

    //! Returns an iterator to the given value if it is contained in `*this`, otherwise returns `end()`. O(n).
    //! @param value The value to check for.
    //! @returns A @ref const_iterator to @p value if contained in `*this`; end() otherwise.
    const_iterator locate(T& value) const noexcept
    {
        Link* l = _link(value);
        return const_iterator(l->isContained() ? _listfind(value) : _end(), this);
    }

    //! Naively produces an @ref iterator for @p value within `*this`.
    //! @warning Undefined behavior results if @p value is not contained within `*this`. Use locate() to safely check.
    //! @param value The value to convert.
    //! @returns An @ref iterator to @p value.
    iterator iter_from_value(T& value)
    {
        Link* l = _link(value);
        CARB_ASSERT(!l->isContained() || _listfind(value) != _end());
        return iterator(l->isContained() ? l : _end(), this);
    }

    //! Naively produces an @ref iterator for @p value within `*this`.
    //! @warning Undefined behavior results if @p value is not contained within `*this`. Use locate() to safely check.
    //! @param value The value to convert.
    //! @returns A @ref const_iterator to @p value.
    const_iterator iter_from_value(T& value) const
    {
        Link* l = _link(value);
        CARB_ASSERT(!l->isContained() || _listfind(value) != _end());
        return const_iterator(l->isContained() ? l : _end(), this);
    }

    //! Removes all elements.
    //! @note Postcondition: `*this` is empty().
    void clear()
    {
        if (_head() != _end())
        {
            do
            {
                Link* p = _head();
                p->value.~ValueType(); // Destruct the key
                m_list.m_next = p->m_next;
                p->m_next = p->m_prev = nullptr;
            } while (_head() != _end());
            m_list.m_prev = _end();
            m_size = 0;

            // Clear the buckets
            memset(m_buckets.get(), 0, sizeof(LinkPair) * m_bucketCount);
        }
    }

    //! Inserts an element.
    //! @note No uniqueness checking is performed; multiple values with the same `Key` may be inserted.
    //! @note Precondition: `value.second` must not be contained (via `U`) in this or any other
    //!     IntrusiveUnorderedMultimap.
    //! @param value The pair of `[key, value reference]` to insert.
    //! @returns An @ref iterator to the newly-inserted @p value.
    iterator insert(ValueType value)
    {
        T& val = value.second;
        Link* l = _link(val);
        CARB_ASSERT(!l->isContained());

        // Construct the key
        new (&l->value) ValueType(std::move(value));

        // Hash
        size_t const hash = Hash{}(l->value.first);

        ++m_size;

        // Find insertion point
        reserve(size());
        LinkPair& bucket = m_buckets[_bucket(hash)];
        if (bucket.first)
        {
            // Need to see if there's a matching value in the bucket so that we group all keys together
            Pred pred{};
            Link* const end = bucket.second->m_next;
            for (Link* p = bucket.first; p != end; p = p->m_next)
            {
                if (pred(l->value.first, p->value.first))
                {
                    // Match! Insert here.
                    l->m_prev = p->m_prev;
                    l->m_next = p;
                    l->m_prev->m_next = l;
                    l->m_next->m_prev = l;

                    if (p == bucket.first)
                    {
                        bucket.first = l;
                    }
                    return iterator(l, this);
                }
            }

            // Didn't find a match within the bucket. Just add to the end of the bucket
            l->m_prev = bucket.second;
            l->m_next = end;
            l->m_prev->m_next = l;
            l->m_next->m_prev = l;
            bucket.second = l;
        }
        else
        {
            // Insert at end of the list
            l->m_prev = _tail();
            l->m_next = _end();
            l->m_prev->m_next = l;
            l->m_next->m_prev = l;
            bucket.first = bucket.second = l;
        }

        return iterator(l, this);
    }

    //! Inserts an element while allowing key emplacement.
    //! @note No uniqueness checking is performed; multiple values with the same `Key` may be inserted.
    //! @note Precondition: The `second` member of the constructed `pair` must not be contained (via `U`) in this or any
    //!     other IntrusiveUnorderedMultimap.
    //! @param args Arguments passed to `std::pair<KeyType, MappedType>` to insert.
    //! @returns An @ref iterator to the newly-inserted value.
    template <class... Args>
    iterator emplace(Args&&... args)
    {
        return insert(ValueType{ std::forward<Args>(args)... });
    }

    //! Finds an element with a specific key.
    //! @param key The key that identifies an element.
    //! @returns An @ref iterator to the element, if found; end() otherwise.
    iterator find(const Key& key)
    {
        if (empty())
        {
            return end();
        }

        size_t const hash = Hash{}(key);
        LinkPair& pair = m_buckets[_bucket(hash)];
        if (!pair.first)
        {
            return end();
        }

        Pred pred{};
        for (Link* p = pair.first; p != pair.second->m_next; p = p->m_next)
        {
            if (pred(p->value.first, key))
            {
                return iterator(p, this);
            }
        }

        // Not found
        return end();
    }

    //! Finds an element with a specific key.
    //! @param key The key that identifies an element.
    //! @returns A @ref const_iterator to the element, if found; end() otherwise.
    const_iterator find(const Key& key) const
    {
        if (empty())
        {
            return cend();
        }

        size_t const hash = Hash{}(key);
        LinkPair& pair = m_buckets[_bucket(hash)];
        if (!pair.first)
        {
            return cend();
        }

        Pred pred{};
        Link* const bucketEnd = pair.second->m_next;
        for (Link* p = pair.first; p != bucketEnd; p = p->m_next)
        {
            if (pred(p->value.first, key))
            {
                return const_iterator(p, this);
            }
        }

        return cend();
    }

    //! Finds a range of elements matching the given key.
    //! @param key The key that identifies an element.
    //! @returns A `pair` of @ref iterator objects that define a range: the `first` iterator is the first item in the
    //!     range and the `second` iterator is immediately past the end of the range. If no elements exist with @p key,
    //!     `std::pair(end(), end())` is returned.
    std::pair<iterator, iterator> equal_range(const Key& key)
    {
        if (empty())
        {
            return std::make_pair(end(), end());
        }

        size_t const hash = Hash{}(key);
        LinkPair& pair = m_buckets[_bucket(hash)];
        if (!pair.first)
        {
            return std::make_pair(end(), end());
        }

        Pred pred{};
        Link* p = pair.first;
        Link* const bucketEnd = pair.second->m_next;
        for (; p != bucketEnd; p = p->m_next)
        {
            if (pred(p->value.first, key))
            {
                // Inner loop: terminates when no longer matches or bucket ends
                Link* first = p;
                p = p->m_next;
                for (; p != bucketEnd; p = p->m_next)
                {
                    if (!pred(p->value.first, key))
                    {
                        break;
                    }
                }
                return std::make_pair(iterator(first, this), iterator(p, this));
            }
        }
        return std::make_pair(end(), end());
    }

    //! Finds a range of elements matching the given key.
    //! @param key The key that identifies an element.
    //! @returns A `pair` of @ref const_iterator objects that define a range: the `first` const_iterator is the first
    //!     item in the range and the `second` const_iterator is immediately past the end of the range. If no elements
    //!     exist with @p key, `std::pair(end(), end())` is returned.
    std::pair<const_iterator, const_iterator> equal_range(const Key& key) const
    {
        if (empty())
        {
            return std::make_pair(cend(), cend());
        }

        size_t const hash = Hash{}(key);
        LinkPair& pair = m_buckets[_bucket(hash)];
        if (!pair.first)
        {
            return std::make_pair(cend(), cend());
        }

        Pred pred{};
        Link* p = pair.first;
        Link* const bucketEnd = pair.second->m_next;
        for (; p != bucketEnd; p = p->m_next)
        {
            if (pred(p->value.first, key))
            {
                // Inner loop: terminates when no longer matches or bucket ends
                Link* first = p;
                p = p->m_next;
                for (; p != bucketEnd; p = p->m_next)
                {
                    if (!pred(p->value.first, key))
                    {
                        break;
                    }
                }
                return std::make_pair(const_iterator(first, this), const_iterator(p, this));
            }
        }
        return std::make_pair(cend(), cend());
    }

    //! Returns the number of elements matching a specific key.
    //! @param key The key to search for.
    //! @returns The number of elements matching the given key.
    size_t count(const Key& key) const
    {
        if (empty())
        {
            return 0;
        }

        size_t const hash = Hash{}(key);
        LinkPair& pair = m_buckets[_bucket(hash)];
        if (!pair.first)
        {
            return 0;
        }

        Pred pred{};
        Link* p = pair.first;
        Link* const bucketEnd = pair.second->m_next;
        for (; p != bucketEnd; p = p->m_next)
        {
            if (pred(p->value.first, key))
            {
                // Inner loop: terminates when no longer matches or bucket ends
                size_t count = 1;
                p = p->m_next;
                for (; p != bucketEnd; p = p->m_next)
                {
                    if (!pred(p->value.first, key))
                    {
                        break;
                    }
                    ++count;
                }
                return count;
            }
        }
        return 0;
    }

    //! Removes an element by iterator.
    //! @note Precondition: @p pos must be a valid const_iterator of `*this` and may not be end().
    //! @param pos A @ref const_iterator to the element to remove.
    //! @returns A @ref iterator to the element immediately following @p pos, or end() if no elements followed it.
    iterator remove(const_iterator pos)
    {
        CARB_ASSERT(!empty());
        pos.assertNotEnd();
        pos.assertOwner(this);

        Link* l = pos.m_where;
        Link* next = l->m_next;

        // Fix up bucket if necessary
        LinkPair& pair = m_buckets[_bucket(Hash{}(l->value.first))];
        if (pair.first == l)
        {
            if (pair.second == l)
            {
                // Empty bucket now
                pair.first = pair.second = nullptr;
            }
            else
            {
                pair.first = next;
            }
        }
        else if (pair.second == l)
        {
            pair.second = l->m_prev;
        }

        l->m_prev->m_next = l->m_next;
        l->m_next->m_prev = l->m_prev;
        l->m_next = l->m_prev = nullptr;
        --m_size;

        // Destruct value
        l->value.~ValueType();
        return iterator(next, this);
    }

    //! Removes an element by reference.
    //! @note Precondition: @p value must be contained in `*this`.
    //! @param value The element to remove.
    //! @returns @p value for convenience.
    T& remove(T& value)
    {
        Link* l = _link(value);
        if (l->isContained())
        {
            CARB_ASSERT(!empty());
            CARB_ASSERT(_listfind(value) != _end());

            // Fix up bucket if necessary
            LinkPair& pair = m_buckets[_bucket(Hash{}(l->value.first))];
            if (pair.first == l)
            {
                if (pair.second == l)
                {
                    // Empty bucket now
                    pair.first = pair.second = nullptr;
                }
                else
                {
                    pair.first = l->m_next;
                }
            }
            else if (pair.second == l)
            {
                pair.second = l->m_prev;
            }

            l->m_prev->m_next = l->m_next;
            l->m_next->m_prev = l->m_prev;
            l->m_next = l->m_prev = nullptr;
            --m_size;

            // Destruct value
            l->value.~ValueType();
        }
        return value;
    }

    //! Removes all elements matching a specific key.
    //! @param key The key to search for.
    //! @returns The number of elements that were removed.
    size_t remove(const Key& key)
    {
        size_t count{ 0 };
        auto pair = equal_range(key);
        while (pair.first != pair.second)
        {
            remove(pair.first++);
            ++count;
        }
        return count;
    }

    //! Swaps the contents of `*this` with another IntrusiveUnorderedMultimap.
    //! @param other The other IntrusiveUnorderedMultimap to swap with.
    void swap(IntrusiveUnorderedMultimap& other) noexcept
    {
        if (this != std::addressof(other))
        {
            // Fix up the end iterators first
            Link *&lhead = _head()->m_prev, *&ltail = _tail()->m_next;
            Link *&rhead = other._head()->m_prev, *&rtail = other._tail()->m_next;
            lhead = ltail = other._end();
            rhead = rtail = _end();

            // Now swap everything else
            std::swap(m_buckets, other.m_buckets);
            std::swap(m_bucketCount, other.m_bucketCount);
            std::swap(_end()->m_next, other._end()->m_next);
            std::swap(_end()->m_prev, other._end()->m_prev);
            std::swap(m_size, other.m_size);
            std::swap(m_maxLoadFactor, other.m_maxLoadFactor);
        }
    }

    //! Returns the number of buckets.
    //! @returns The number of buckets.
    size_t bucket_count() const noexcept
    {
        return m_bucketCount;
    }

    //! Returns the maximum number of buckets.
    //! @returns The maximum number of buckets.
    size_t max_bucket_count() const noexcept
    {
        return size_t(-1);
    }

    //! Returns the bucket index for a specific key.
    //! @param key The key to hash.
    //! @returns A bucket index in the range `[0, bucket_count())`.
    size_t bucket(const Key& key) const
    {
        return _bucket(Hash{}(key));
    }

    //! Returns the average number of elements per bucket.
    //! @returns The average number of elements per bucket.
    float load_factor() const
    {
        return bucket_count() ? float(size()) / float(bucket_count()) : 0.f;
    }

    //! Returns the max load factor for `*this`.
    //! @returns The max load factor for `*this`. The default is 1.0.
    float max_load_factor() const noexcept
    {
        return m_maxLoadFactor;
    }

    //! Sets the maximum load factor for `*this`.
    //! @note Precondition: @p ml must be greater than 0.
    //! @note Changes do not take effect until the hash table is re-generated.
    //! @param ml The new maximum load factor for `*this`.
    void max_load_factor(float ml)
    {
        CARB_ASSERT(ml > 0.f);
        m_maxLoadFactor = ml;
    }

    //! Reserves space for at least the specified number of elements and re-generates the hash table.
    //! @param count The minimum number of elements to reserve space for.
    void reserve(size_t count)
    {
        rehash(size_t(std::ceil(count / max_load_factor())));
    }

    //! Reserves at least the specified number of buckets and re-generates the hash table.
    //! @param buckets The minimum number of buckets required.
    void rehash(size_t buckets)
    {
        if (buckets > m_bucketCount)
        {
            constexpr static size_t kMinBuckets(8);
            static_assert(carb::cpp20::has_single_bit(kMinBuckets), "Invalid assumption");
            buckets = carb::cpp20::bit_ceil(::carb_max(buckets, kMinBuckets));
            CARB_ASSERT(carb::cpp20::has_single_bit(buckets));
            m_buckets.reset(new LinkPair[buckets]);
            memset(m_buckets.get(), 0, sizeof(LinkPair) * buckets);
            m_bucketCount = buckets;

            // Walk through the list backwards and rehash everything. Things that have equal keys and are already
            // grouped together will remain so.
            Link* cur = _tail();
            m_list.m_prev = m_list.m_next = _end();

            Link* next;
            Hash hasher;
            for (; cur != _end(); cur = next)
            {
                next = cur->m_prev;

                LinkPair& bucket = m_buckets[_bucket(hasher(cur->value.first))];
                if (bucket.first)
                {
                    // Insert in front of whatever was in the bucket
                    cur->m_prev = bucket.first->m_prev;
                    cur->m_next = bucket.first;
                    cur->m_prev->m_next = cur;
                    cur->m_next->m_prev = cur;
                    bucket.first = cur;
                }
                else
                {
                    // Insert at the front of the list and the beginning of the bucket
                    cur->m_prev = _end();
                    cur->m_next = _head();
                    cur->m_prev->m_next = cur;
                    cur->m_next->m_prev = cur;
                    bucket.first = bucket.second = cur;
                }
            }
        }
    }

private:
    struct LinkPair
    {
        Link* first;
        Link* second;
    };
    std::unique_ptr<LinkPair[]> m_buckets{};
    size_t m_bucketCount{ 0 };
    BaseLink m_list;
    size_t m_size{ 0 };
    float m_maxLoadFactor{ 1.f };

    size_t _bucket(size_t hash) const
    {
        // bucket count is always a power of 2
        return hash & (m_bucketCount - 1);
    }

    Link* _listfind(T& value) const
    {
        Link* find = _link(value);
        Link* p = _head();
        for (; p != _end(); p = p->m_next)
        {
            if (p == find)
            {
                return p;
            }
        }
        return _end();
    }

    static Link* _link(T& value) noexcept
    {
        return std::addressof(value.*U);
    }

    static T& _value(Link& l) noexcept
    {
        // Need to calculate the offset of our link member which will allow adjusting the pointer to where T is.
        // This will not work if T uses virtual inheritance. Also, offsetof() cannot be used because we have a pointer
        // to the member
        size_t offset = size_t(reinterpret_cast<uint8_t*>(&(((T*)0)->*U)));
        return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(std::addressof(l)) - offset);
    }

    static const T& _value(const Link& l) noexcept
    {
        // Need to calculate the offset of our link member which will allow adjusting the pointer to where T is.
        // This will not work if T uses virtual inheritance. Also, offsetof() cannot be used because we have a pointer
        // to the member
        size_t offset = size_t(reinterpret_cast<uint8_t*>(&(((T*)0)->*U)));
        return *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(std::addressof(l)) - offset);
    }

    constexpr Link* _head() const noexcept
    {
        return const_cast<Link*>(m_list.m_next);
    }

    constexpr Link* _tail() const noexcept
    {
        return const_cast<Link*>(m_list.m_prev);
    }

    constexpr Link* _end() const noexcept
    {
        return static_cast<Link*>(const_cast<BaseLink*>(&m_list));
    }
};

} // namespace container

} // namespace carb
