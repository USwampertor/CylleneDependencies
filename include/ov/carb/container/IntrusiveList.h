// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Carbonite intrusive list container.
#pragma once

#include "../Defines.h"

#include <iterator>

namespace carb
{

namespace container
{

/**
 * Defines a "link node" that \ref IntrusiveList will use for tracking data for the contained type.
 */
template <class T>
class IntrusiveListLink
{
public:
    //! The object that contains `*this`.
    using ValueType = T;

    //! Constructor.
    constexpr IntrusiveListLink() noexcept = default;

    //! Destructor. Asserts that the link is not contained in an @ref IntrusiveList.
    ~IntrusiveListLink()
    {
        // Shouldn't be contained at destruction time
        CARB_ASSERT(!isContained());
    }

    //! Reports whether this link object is inserted into am @ref IntrusiveList container.
    //! @returns `true` if this link object is present in an @ref IntrusiveList; `false` otherwise.
    bool isContained() const noexcept
    {
        return m_next != nullptr;
    }

private:
    template <class U, IntrusiveListLink<U> U::*V>
    friend class IntrusiveList;

    CARB_VIZ IntrusiveListLink* m_next{ nullptr };
    IntrusiveListLink* m_prev{ nullptr };

    CARB_PREVENT_COPY_AND_MOVE(IntrusiveListLink);

    constexpr IntrusiveListLink(IntrusiveListLink* init) noexcept : m_next(init), m_prev(init)
    {
    }
};

/**
 * IntrusiveList is very similar to std::list, but requires the tracking information to be contained within the stored
 * type `T`, rather than built around it. In other words, the tracking information is "intrusive" in the type `T` by way
 * of the @ref IntrusiveListLink type. IntrusiveList does no allocation of the `T` type; all allocation is done outside
 * of the context of IntrusiveList, which allows stored items to be on the stack, grouped with other items, etc.
 *
 * The impetus behind intrusive containers is specifically to allow the application to own the allocation patterns for a
 * type, but still be able to store them in a container. For `std::list`, everything goes through an `Allocator` type,
 * but in a real application some stored instances may be on the stack while others are on the heap, which makes using
 * `std::list` impractical. Furthermore, a stored type may wish to be removed from one list and inserted into another.
 * With `std::list`, this would require heap interaction to erase from one list and insert into another. With
 * IntrusiveList, this operation would not require any heap interaction and would be done very quickly (O(1)).
 *
 * Another example is a `std::list` of polymorphic types. For `std::list` the contained type would have to be a pointer
 * or pointer-like type which is an inefficient use of space, cache, etc.
 *
 * Since IntrusiveList doesn't require any form of `Allocator`, the allocation strategy is completely left up to the
 * application. This means that items could be allocated on the stack, pooled, or items mixed between stack and heap.
 *
 * IntrusiveList matches `std::list` with the following exceptions:
 * - The `iterator` and `initializer_list` constructor variants are not present in IntrusiveList.
 * - The `iterator` and `initializer_list` insert() variants are not present in IntrusiveList.
 * - IntrusiveList cannot be copied (though may still be moved).
 * - IntrusiveList does not have `erase()` to erase an item from the list, but instead has remove() which will remove an
 *   item from the container. It is up to the caller to manage the memory for the item.
 * - Likewise, clear() functions as a "remove all" and does not destroy items in the container.
 * - IntrusiveList does not have any emplace functions as it is not responsible for construction of items.
 * - iter_from_value() is a new function that translates an item contained in IntrusiveList into an iterator.
 *
 * Example:
 * @code{.cpp}
 *     // Given a class Waiter whose purpose is to wait until woken:
 *     class Waiter {
 *     public:
 *         void wait();
 *         IntrusiveListLink<Waiter> link;
 *     };
 *
 *     IntrusiveList<Waiter, &Waiter::link> list;
 *
 *     Waiter w;
 *     list.push_back(w);
 *     w.wait();
 *     list.remove(w);
 *
 *     // Since the Waiter instance is on the stack there is no heap used to track items in `list`.
 * @endcode
 *
 * Example 2:
 * @code{.cpp}
 *     // Intrusive list can be used to move items between multiple lists using the same link node without any heap
 *     // usage.
 *     class MyItem { public:
 *         // ...
 *         void process();
 *         IntrusiveListLink<MyItem> link;
 *     };
 *
 *     using MyItemList = IntrusiveList<MyItem, &MyItem::link>;
 *     MyItemList dirty;
 *     MyItemList clean;
 *
 *     MyItemList::iter;
 *     while (!dirty.empty())
 *     {
 *         MyItem& item = dirty.pop_front();
 *         item.process();
 *         clean.push_back(item);
 *     }
 * @endcode
 *
 * @tparam T The contained object type.
 * @tparam U A pointer-to-member of `T` that must be of type @ref IntrusiveListLink. This member will be used by the
 *     IntrusiveList for tracking the contained object.
 */
template <class T, IntrusiveListLink<T> T::*U>
class CARB_VIZ IntrusiveList
{
public:
    //! The type of the contained object.
    using ValueType = T;
    //! The type of the @ref IntrusiveListLink that must be a member of ValueType.
    using Link = IntrusiveListLink<T>;

    // Iterator support
    // clang-format off
    //! A LegacyBidirectionalIterator to `const ValueType`.
    //! @see <a href="https://en.cppreference.com/w/cpp/named_req/BidirectionalIterator">LegacyBidirectionalIterator</a>
    class const_iterator
    {
    public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = ValueType;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;
        const_iterator() noexcept = default;
        reference       operator *  () const                    { assertNotEnd(); return IntrusiveList::_value(*m_where); }
        pointer         operator -> () const                    { assertNotEnd(); return std::addressof(operator*()); }
        const_iterator& operator ++ () noexcept    /* ++iter */ { assertNotEnd(); incr(); return *this; }
        const_iterator  operator ++ (int) noexcept /* iter++ */ { assertNotEnd(); const_iterator i{ *this }; incr(); return i; }
        const_iterator& operator -- () noexcept    /* --iter */ { decr(); return *this; }
        const_iterator  operator -- (int) noexcept /* iter-- */ { const_iterator i{ *this }; decr(); return i; }
        bool operator == (const const_iterator& rhs) const noexcept { assertSameOwner(rhs); return m_where == rhs.m_where; }
        bool operator != (const const_iterator& rhs) const noexcept { assertSameOwner(rhs); return m_where != rhs.m_where; }
    protected:
        friend class IntrusiveList;
        Link* m_where{ nullptr };
#if CARB_ASSERT_ENABLED
        const IntrusiveList* m_owner{ nullptr };
        const_iterator(Link* where, const IntrusiveList* owner) : m_where(where), m_owner(owner) {}
        void assertOwner(const IntrusiveList* list) const noexcept
        {
            CARB_ASSERT(m_owner == list, "IntrusiveList iterator for invalid container");
        }
        void assertSameOwner(const const_iterator& rhs) const noexcept
        {
            CARB_ASSERT(m_owner == rhs.m_owner, "IntrusiveList iterators are from different containers");
        }
        void assertNotEnd() const noexcept
        {
            CARB_ASSERT(m_where != m_owner->_end(), "Invalid operation on IntrusiveList::end() iterator");
        }
#else
        const_iterator(Link* where, const IntrusiveList*) : m_where(where) {}
        void assertOwner(const IntrusiveList* list) const noexcept
        {
            CARB_UNUSED(list);
        }
        void assertSameOwner(const const_iterator& rhs) const noexcept
        {
            CARB_UNUSED(rhs);
        }
        void assertNotEnd() const noexcept {}
#endif // !CARB_ASSERT_ENABLED
        void incr() { m_where = m_where->m_next; }
        void decr() { m_where = m_where->m_prev; }
#endif // !DOXYGEN_SHOULD_SKIP_THIS
    };

    //! A LegacyBidirectionalIterator to `ValueType`.
    //! @see <a href="https://en.cppreference.com/w/cpp/named_req/BidirectionalIterator">LegacyBidirectionalIterator</a>
    class iterator : public const_iterator
    {
        using Base = const_iterator;
    public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = ValueType;
        using difference_type = ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;
        iterator() noexcept = default;
        reference operator *  () const                    { this->assertNotEnd(); return IntrusiveList::_value(*this->m_where); }
        pointer   operator -> () const                    { this->assertNotEnd(); return std::addressof(operator*()); }
        iterator& operator ++ () noexcept    /* ++iter */ { this->assertNotEnd(); this->incr(); return *this; }
        iterator  operator ++ (int) noexcept /* iter++ */ { this->assertNotEnd(); iterator i{ *this }; this->incr(); return i; }
        iterator& operator -- () noexcept    /* --iter */ { this->decr(); return *this; }
        iterator  operator -- (int) noexcept /* iter-- */ { iterator i{ *this }; this->decr(); return i; }
    protected:
        friend class IntrusiveList;
        iterator(Link* where, const IntrusiveList* owner) : Base(where, owner) {}
#endif
    };

    //! Helper definition for std::reverse_iterator<iterator>
    using reverse_iterator = std::reverse_iterator<iterator>;
    //! Helper definition for std::reverse_iterator<const_iterator>
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    // clang-format on

    CARB_PREVENT_COPY(IntrusiveList);

    //! Constructor. Initializes `*this` to be empty().
    constexpr IntrusiveList() : m_list(&m_list)
    {
    }

    //! Move-construct. Moves all entries to `*this` from @p other and leaves it empty.
    //! @param other Another IntrusiveList to move entries from.
    IntrusiveList(IntrusiveList&& other) noexcept : m_list(&m_list)
    {
        swap(other);
    }

    //! Destructor. Implies clear().
    ~IntrusiveList()
    {
        clear();
        m_list.m_next = m_list.m_prev = nullptr; // prevent the assert
    }

    //! Move-assign. Moves all entries from @p other and leaves @p other in a valid but possibly non-empty state.
    //! @param other Another IntrusiveList to move entries from.
    IntrusiveList& operator=(IntrusiveList&& other) noexcept
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

    // Forward iterator support
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

    // Reverse iterator support
    //! Returns a reverse_iterator to the beginning.
    //! @returns A reverse_iterator to the beginning.
    reverse_iterator rbegin() noexcept
    {
        return reverse_iterator(end());
    }

    //! Returns a reverse_iterator to the end.
    //! @returns A reverse_iterator to the end.
    reverse_iterator rend() noexcept
    {
        return reverse_iterator(begin());
    }

    //! Returns a const_reverse_iterator to the beginning.
    //! @returns A const_reverse_iterator to the beginning.
    const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }

    //! Returns a const_reverse_iterator to the end.
    //! @returns A const_reverse_iterator to the end.
    const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(cbegin());
    }

    //! Returns a const_reverse_iterator to the beginning.
    //! @returns A const_reverse_iterator to the beginning.
    const_reverse_iterator rbegin() const noexcept
    {
        return crbegin();
    }

    //! Returns a const_reverse_iterator to the end.
    //! @returns A const_reverse_iterator to the end.
    const_reverse_iterator rend() const noexcept
    {
        return crend();
    }

    //! Returns an iterator to the given value if it is contained in `*this`, otherwise returns `end()`. O(n).
    //! @param[in] value The value to check for.
    //! @returns An @ref iterator to @p value if contained in `*this`; end() otherwise.
    iterator locate(T& value) noexcept
    {
        Link* l = _link(value);
        if (!l->isContained())
            return end();

        Link* b = m_list.m_next;
        while (b != _end())
        {
            if (b == l)
                return iterator(l, this);
            b = b->m_next;
        }

        return end();
    }

    //! Returns an iterator to the given value if it is contained in `*this`, otherwise returns `end()`. O(n).
    //! @param[in] value The value to check for.
    //! @returns A @ref const_iterator to @p value if contained in `*this`; end() otherwise.
    const_iterator locate(T& value) const noexcept
    {
        Link* l = _link(value);
        if (!l->isContained())
            return end();

        Link* b = m_list.m_next;
        while (b != _end())
        {
            if (b == l)
                return const_iterator(l, this);
            b = b->m_next;
        }

        return end();
    }

#ifndef DOXYGEN_SHOULD_SKIP_THIS
    CARB_DEPRECATED("Use locate()") iterator find(T& value) noexcept
    {
        return locate(value);
    }
    CARB_DEPRECATED("Use locate()") const_iterator find(T& value) const noexcept
    {
        return locate(value);
    }
#endif

    //! Naively produces an @ref IntrusiveList::iterator for @p value within `*this`.
    //! @warning Undefined behavior results if @p value is not contained within `*this`. Use find() to safely check.
    //! @param value The value to convert.
    //! @returns An @ref iterator to @p value.
    iterator iter_from_value(T& value)
    {
        Link* l = _link(value);
        CARB_ASSERT(!l->isContained() || locate(value) != end());
        return iterator(l->isContained() ? l : _end(), this);
    }

    //! Naively produces an @ref IntrusiveList::iterator for @p value within `*this`.
    //! @warning Undefined behavior results if @p value is not contained within `*this`. Use find() to safely check.
    //! @param value The value to convert.
    //! @returns A @ref const_iterator to @p value.
    const_iterator iter_from_value(T& value) const
    {
        Link* l = _link(value);
        CARB_ASSERT(!l->isContained() || locate(value) != end());
        return const_iterator(l->isContained() ? l : _end(), this);
    }

    //! Accesses the first element.
    //! @warning Undefined behavior if `*this` is empty().
    //! @returns A reference to the first element.
    T& front()
    {
        CARB_ASSERT(!empty());
        return _value(*_head());
    }

    //! Accesses the first element.
    //! @warning Undefined behavior if `*this` is empty().
    //! @returns A const reference to the first element.
    const T& front() const
    {
        CARB_ASSERT(!empty());
        return _value(*_head());
    }

    //! Accesses the last element.
    //! @warning Undefined behavior if `*this` is empty().
    //! @returns A reference to the last element.
    T& back()
    {
        CARB_ASSERT(!empty());
        return _value(*_tail());
    }

    //! Accesses the last element.
    //! @warning Undefined behavior if `*this` is empty().
    //! @returns A const reference to the last element.
    const T& back() const
    {
        CARB_ASSERT(!empty());
        return _value(*_tail());
    }

    //! Inserts an element at the beginning of the list.
    //! @note Precondition: @p value must not be contained (via `U`) in `*this` or any other IntrusiveList.
    //! @param value The value to insert.
    //! @returns @p value for convenience.
    T& push_front(T& value)
    {
        Link* l = _link(value);
        CARB_ASSERT(!l->isContained());
        Link* const prev = _head();
        l->m_next = prev;
        l->m_prev = _end();
        m_list.m_next = l;
        prev->m_prev = l;
        ++m_size;
        return value;
    }

    //! Removes the first element.
    //! @note Precondition: `*this` must not be empty().
    //! @returns The prior first element in the list, which is now no longer contained in the list.
    T& pop_front()
    {
        CARB_ASSERT(!empty());
        Link* const head = _head();
        Link* const next = head->m_next;
        m_list.m_next = next;
        next->m_prev = _end();
        head->m_next = head->m_prev = nullptr;
        --m_size;
        return _value(*head);
    }

    //! Inserts an element at the end of the list.
    //! @note Precondition: @p value must not be contained (via `U`) in `*this` or any other IntrusiveList.
    //! @param value The value to insert.
    //! @returns @p value for convenience.
    T& push_back(T& value)
    {
        Link* l = _link(value);
        CARB_ASSERT(!l->isContained());
        Link* const prev = _tail();
        l->m_next = _end();
        l->m_prev = prev;
        prev->m_next = l;
        m_list.m_prev = l;
        ++m_size;
        return value;
    }

    //! Removes the last element.
    //! @note Precondition: `*this` must not be empty().
    //! @returns The prior last element in the list, which is now no longer contained in the list.
    T& pop_back()
    {
        CARB_ASSERT(!empty());
        Link* const tail = _tail();
        Link* const prev = tail->m_prev;
        m_list.m_prev = prev;
        prev->m_next = _end();
        tail->m_next = tail->m_prev = nullptr;
        --m_size;
        return _value(*tail);
    }

    //! Removes all elements from the list.
    //! @note Postcondition: `*this` is empty().
    void clear()
    {
        if (_head() != _end())
        {
            do
            {
                Link* p = _head();
                m_list.m_next = p->m_next;
                p->m_next = p->m_prev = nullptr;
            } while (_head() != _end());
            m_list.m_prev = _end();
            m_size = 0;
        }
    }

    //! Inserts an element before @p pos.
    //! @note Precondition: @p pos must be a valid const_iterator of `*this`; @p value must not be contained (via `U`)
    //!     in this or any other IntrusiveList.
    //! @param pos A @ref const_iterator indicating the insertion position. @p value will be inserted before @p pos.
    //! @param value The value to insert.
    //! @returns An @ref iterator to the newly-inserted @p value.
    iterator insert(const_iterator pos, T& value)
    {
        Link* l = _link(value);
        CARB_ASSERT(!l->isContained());
        l->m_prev = pos.m_where->m_prev;
        l->m_next = pos.m_where;
        l->m_prev->m_next = l;
        l->m_next->m_prev = l;
        ++m_size;
        return iterator(l, this);
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
        Link* next = pos.m_where->m_next;
        pos.m_where->m_prev->m_next = pos.m_where->m_next;
        pos.m_where->m_next->m_prev = pos.m_where->m_prev;
        pos.m_where->m_next = pos.m_where->m_prev = nullptr;
        --m_size;
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
            CARB_ASSERT(locate(value) != end());
            l->m_prev->m_next = l->m_next;
            l->m_next->m_prev = l->m_prev;
            l->m_next = l->m_prev = nullptr;
            --m_size;
        }
        return value;
    }

    //! Swaps the contents of `*this` with another IntrusiveList.
    //! @param other The other IntrusiveList to swap with.
    void swap(IntrusiveList& other) noexcept
    {
        if (this != std::addressof(other))
        {
            // Fix up the end iterators first.
            Link *&lhead = _head()->m_prev, *&ltail = _tail()->m_next;
            Link *&rhead = other._head()->m_prev, *&rtail = other._tail()->m_next;
            lhead = ltail = other._end();
            rhead = rtail = _end();

            // Now swap pointers
            std::swap(_end()->m_next, other._end()->m_next);
            std::swap(_end()->m_prev, other._end()->m_prev);
            std::swap(m_size, other.m_size);
        }
    }

    //! Merges two sorted lists.
    //! @note Precondition: `*this` and @p other must be sorted using @p comp.
    //! @note This operation is stable: for equivalent elements in the two lists elements from `*this` shall always
    //!     precede the elements from @p other. The order of equivalent elements within `*this` and @p other will not
    //!     change.
    //! @param[inout] other Another IntrusiveList to merge with. Must be sorted via `comp`. Will be empty after this
    //! call.
    //! @param comp The comparator predicate, such as `std::less`.
    template <class Compare>
    void merge(IntrusiveList& other, Compare comp)
    {
        if (this == std::addressof(other))
            return;

        if (!other.m_size)
            // Nothing to do
            return;

        // splice all of other's nodes onto the end of *this
        Link* const head = _end();
        Link* const otherHead = other._end();
        Link* const mid = otherHead->m_next;
        _splice(head, other, mid, otherHead, other.m_size);

        if (head->m_next != mid)
            _mergeSame(head->m_next, mid, head, comp);
    }

    //! Merges two sorted lists.
    //! @see merge(IntrusiveList& other, Compare comp)
    //! @param[inout] other Another IntrusiveList to merge with. Must be sorted via `comp`. Will be empty after this
    //! call.
    //! @param comp The comparator predicate, such as `std::less`.
    template <class Compare>
    void merge(IntrusiveList&& other, Compare comp)
    {
        merge(other, comp);
    }

    //! Merges two sorted lists via `std::less`.
    //! @see merge(IntrusiveList& other, Compare comp)
    //! @param[inout] other Another IntrusiveList to merge with. Must be sorted via `comp`. Will be empty after this
    //! call.
    void merge(IntrusiveList& other)
    {
        merge(other, std::less<ValueType>());
    }

    //! Merges two sorted lists via `std::less`.
    //! @see merge(IntrusiveList& other, Compare comp)
    //! @param[inout] other Another IntrusiveList to merge with. Must be sorted via `comp`. Will be empty after this
    //! call.
    void merge(IntrusiveList&& other)
    {
        merge(other);
    }

    //! Transfers elements from another IntrusiveList into `*this`.
    //! @note Precondition: @p pos must be a valid const_iterator of `*this`.
    //! @param pos The position before which to insert elements from @p other.
    //! @param other Another IntrusiveList to splice from. Will be empty after this call.
    void splice(const_iterator pos, IntrusiveList& other)
    {
        pos.assertOwner(this);
        if (this == std::addressof(other) || other.empty())
            return;

        _splice(pos.m_where, other, other.m_list.m_next, other._end(), other.m_size);
    }

    //! Transfers elements from another IntrusiveList into `*this`.
    //! @note Precondition: @p pos must be a valid const_iterator of `*this`.
    //! @param pos The position before which to insert elements from @p other.
    //! @param other Another IntrusiveList to splice from. Will be empty after this call.
    void splice(const_iterator pos, IntrusiveList&& other)
    {
        splice(pos, other);
    }

    //! Transfers an element from another IntrusiveList into `*this`.
    //! @note Precondition: @p pos must be a valid const_iterator of `*this`. @p it must be a valid iterator of @p other
    //!     and may not be `other.end()`.
    //! @param pos The position before which to insert the element from @p other.
    //! @param other The IntrusiveList that @p it is from.
    //! @param it An @ref iterator to an element from @p other. Will be removed from @p other and transferred to
    //! `*this`.
    void splice(const_iterator pos, IntrusiveList& other, iterator it)
    {
        pos.assertOwner(this);
        it.assertNotEnd();
        it.assertOwner(std::addressof(other));

        Link* const last = it.m_where->m_next;
        if (this != std::addressof(other) || (pos.m_where != it.m_where && pos.m_where != last))
            _splice(pos.m_where, other, it.m_where, last, 1);
    }

    //! Transfers an element from another IntrusiveList into `*this`.
    //! @note Precondition: @p pos must be a valid const_iterator of `*this`. @p it must be a valid iterator of @p other
    //!     and may not be `other.end()`.
    //! @param pos The position before which to insert the element from @p other.
    //! @param other The IntrusiveList that @p it is from.
    //! @param it An @ref iterator to an element from @p other. Will be removed from @p other and transferred to
    //! `*this`.
    void splice(const_iterator pos, IntrusiveList&& other, iterator it)
    {
        splice(pos, other, it);
    }

    //! Transfers a range of elements from another IntrusiveList into `*this`.
    //! @note Precondition: @p pos must be a valid const_iterator of `*this`. @p first and @p end must be a valid
    //!     iterator range of @p other. @p pos must not be in the range `[first, end)`.
    //! @param pos The position before which to insert the element(s) from @p other.
    //! @param other The IntrusiveList that @p first and @p end are from.
    //! @param first Combined with @p end describes a range of elements from @p other that will be moved to @p pos.
    //! @param end Combined with @p first describes a range of elements from @p other that will be moved to @p pos.
    void splice(const_iterator pos, IntrusiveList& other, const_iterator first, const_iterator end)
    {
        pos.assertOwner(this);
        first.assertOwner(std::addressof(other));
        end.assertOwner(std::addressof(other));

        if (first == end)
            return;

#if CARB_ASSERT_ENABLED
        if (pos.m_owner == first.m_owner)
        {
            // The behavior is undefined if pos is an iterator in the range [first, end); though we don't have an
            // efficient way of testing for that, so loop through and check
            for (const_iterator it = first; it != end; ++it)
                CARB_ASSERT(it != pos);
        }
#endif

        if (this != std::addressof(other))
        {
            size_t range = std::distance(first, end);
            CARB_ASSERT(other.m_size >= range);
            other.m_size -= range;
            m_size += range;
        }

        _splice(pos.m_where, first.m_where, end.m_where);
    }

    //! Transfers a range of elements from another IntrusiveList into `*this`.
    //! @note Precondition: @p pos must be a valid const_iterator of `*this`. @p first and @p end must be a valid
    //!     iterator range of @p other. @p pos must not be in the range `[first, end)`.
    //! @param pos The position before which to insert the element(s) from @p other.
    //! @param other The IntrusiveList that @p first and @p end are from.
    //! @param first Combined with @p end describes a range of elements from @p other that will be moved to @p pos.
    //! @param end Combined with @p first describes a range of elements from @p other that will be moved to @p pos.
    void splice(const_iterator pos, IntrusiveList&& other, const_iterator first, const_iterator end)
    {
        splice(pos, other, first, end);
    }

    //! Reverses the order of the elements.
    void reverse() noexcept
    {
        Link* end = _end();
        Link* n = end;

        for (;;)
        {
            Link* next = n->m_next;
            n->m_next = n->m_prev;
            n->m_prev = next;

            if (next == end)
                break;

            n = next;
        }
    }

    //! Sorts the contained elements by the specified comparator function.
    //! @param comp The comparator function, such as `std::less`.
    template <class Compare>
    void sort(Compare comp)
    {
        _sort(_end()->m_next, m_size, comp);
    }

    //! Sorts the contained elements by the specified comparator function.
    void sort()
    {
        sort(std::less<ValueType>());
    }

private:
    CARB_VIZ Link m_list;
    CARB_VIZ size_t m_size{ 0 };

    template <class Compare>
    static Link* _sort(Link*& first, size_t size, Compare comp)
    {
        switch (size)
        {
            case 0:
                return first;
            case 1:
                return first->m_next;
            default:
                break;
        }
        auto mid = _sort(first, size >> 1, comp);
        const auto last = _sort(mid, size - (size >> 1), comp);
        first = _mergeSame(first, mid, last, comp);
        return last;
    }

    template <class Compare>
    static Link* _mergeSame(Link* first, Link* mid, const Link* last, Compare comp)
    {
        // Merge the sorted ranges [first, mid) and [mid, last)
        // Returns the new beginning of the range (which won't be `first` if it was spliced elsewhere)
        Link* newfirst;
        if (comp(_value(*mid), _value(*first)))
            // mid will be spliced to the front of the range
            newfirst = mid;
        else
        {
            // Establish comp(mid, first) by skipping over elements from the first range already in position
            newfirst = first;
            do
            {
                first = first->m_next;
                if (first == mid)
                    return newfirst;
            } while (!comp(_value(*mid), _value(*first)));
        }

        // process one run splice
        for (;;)
        {
            auto runStart = mid;
            // find the end of the run of elements we need to splice from the second range into the first
            do
            {
                mid = mid->m_next;
            } while (mid != last && comp(_value(*mid), _value(*first)));

            // [runStart, mid) goes before first
            _splice(first, runStart, mid);
            if (mid == last)
                return newfirst;

            // Re-establish comp(mid, first) by skipping over elements from the first range already in position.
            do
            {
                first = first->m_next;
                if (first == mid)
                    return newfirst;
            } while (!comp(_value(*mid), _value(*first)));
        }
    }

    Link* _splice(Link* const where, IntrusiveList& other, Link* const first, Link* const last, size_t count)
    {
        if (this != std::addressof(other))
        {
            // Different list, need to fix up size
            m_size += count;
            other.m_size -= count;
        }
        return _splice(where, first, last);
    }

    static Link* _splice(Link* const before, Link* const first, Link* const last) noexcept
    {
        CARB_ASSERT(before != first && before != last && first != last);
        Link* const firstPrev = first->m_prev;
        firstPrev->m_next = last;
        Link* const lastPrev = last->m_prev;
        lastPrev->m_next = before;
        Link* const beforePrev = before->m_prev;
        beforePrev->m_next = first;

        before->m_prev = lastPrev;
        last->m_prev = firstPrev;
        first->m_prev = beforePrev;
        return last;
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
        return const_cast<Link*>(&m_list);
    }
};

} // namespace container

} // namespace carb
