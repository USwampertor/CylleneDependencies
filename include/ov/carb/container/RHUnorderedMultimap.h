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
//! @brief Carbonite Robin-hood Unordered Multi-map container.
#pragma once

#include "RobinHoodImpl.h"

namespace carb
{
namespace container
{

/**
 * Implements an Unordered Multimap, that is: a container that contains a mapping of keys to values where keys may be
 * inserted multiple times, each creating a new element. There is no defined order to the set of keys.
 *
 * \copydetails detail::RobinHood
 *
 * @warning This container is similar to, but not a drop-in replacement for `std::unordered_multimap` due to differences
 * in iterator invalidation and memory layout.
 *
 * Iterator/reference/pointer invalidation (note differences from `std::unordered_multimap`):
 * Operation | Invalidates
 * --------- | -----------
 * All read operations | Never
 * `clear`, `rehash`, `reserve`, `operator=`, `insert`, `emplace` | Always
 * `erase` | Only the element removed
 * `swap` | All iterators, no pointers/references
 *
 * @tparam Key The key type
 * @tparam Value The mapped type to be associated with `Key`
 * @tparam Hasher A functor to use as a hashing function for \c Key
 * @tparam Equals A functor to use to compare two \c Key values for equality
 * @tparam LoadFactorMax100 The load factor to use for the table. This value must be in the range `[10, 100]` and
 * represents the percentage of entries in the hash table that will be filled before resizing. Open-addressing hash maps
 * with 100% usage have better memory usage but worse performance since they need "gaps" in the hash table to terminate
 * runs.
 */
template <class Key, class Value, class Hasher = std::hash<Key>, class Equals = std::equal_to<Key>, size_t LoadFactorMax100 = 80>
class RHUnorderedMultimap
    : public detail::
          RobinHood<LoadFactorMax100, Key, std::pair<const Key, Value>, detail::Select1st<Key, std::pair<const Key, Value>>, Hasher, Equals>
{
    using Base = detail::
        RobinHood<LoadFactorMax100, Key, std::pair<const Key, Value>, detail::Select1st<Key, std::pair<const Key, Value>>, Hasher, Equals>;

public:
    //! The key type
    using key_type = typename Base::key_type;
    //! The mapped value type
    using mapped_type = Value;
    //! The value type (effectively `std::pair<const key_type, mapped_type>`)
    using value_type = typename Base::value_type;
    //! Unsigned integer type (typically \c size_t)
    using size_type = typename Base::size_type;
    //! Signed integer type (typically \c ptrdiff_t)
    using difference_type = typename Base::difference_type;
    //! The hash function
    using hasher = typename Base::hasher;
    //! The key-equals function
    using key_equal = typename Base::key_equal;
    //! \c value_type&
    using reference = typename Base::reference;
    //! `const value_type&`
    using const_reference = typename Base::const_reference;
    //! \c value_type*
    using pointer = typename Base::pointer;
    //! `const value_type*`
    using const_pointer = typename Base::const_pointer;
    //! A \a LegacyForwardIterator to \c value_type
    using iterator = typename Base::iterator;
    //! A \a LegacyForwardIterator to `const value_type`
    using const_iterator = typename Base::const_iterator;
    //! A \a LegacyForwardIterator to \c value_type that proceeds to the next matching key when incremented.
    using find_iterator = typename Base::find_iterator;
    //! A \a LegacyForwardIterator to `const value_type` that proceeds to the next matching key when incremented.
    using const_find_iterator = typename Base::const_find_iterator;

    /**
     * Constructs empty container.
     */
    constexpr RHUnorderedMultimap() noexcept = default;

    /**
     * Copy constructor. Copies elements from another container.
     *
     * @note \c *this may have a different \ref carb::container::detail::RobinHood::capacity() than \p other.
     * @param other The other container to copy entries from.
     */
    RHUnorderedMultimap(const RHUnorderedMultimap& other) : Base(other)
    {
    }

    /**
     * Move constructor. Moves elements from another container.
     *
     * @note No move constructors on contained elements are invoked. \p other will be
     *       \ref carb::container::detail::RobinHood::empty() after this operation.
     * @param other The other container to move entries from.
     */
    RHUnorderedMultimap(RHUnorderedMultimap&& other) : Base(std::move(other))
    {
    }

    /**
     * Destructor. Destroys all contained elements and frees memory.
     */
    ~RHUnorderedMultimap() = default;

    /**
     * Copy-assign operator. Destroys all currently stored elements and copies elements from another container.
     *
     * @param other The other container to copy entries from.
     * @returns \c *this
     */
    RHUnorderedMultimap& operator=(const RHUnorderedMultimap& other)
    {
        Base::operator=(other);
        return *this;
    }

    /**
     * Move-assign operator. Effectively swaps with another container.
     *
     * @param other The other container to copy entries from.
     * @returns \c *this
     */
    RHUnorderedMultimap& operator=(RHUnorderedMultimap&& other)
    {
        Base::operator=(std::move(other));
        return *this;
    }

    /**
     * Inserts an element into the container.
     *
     * All iterators, references and pointers are invalidated.
     *
     * @param value The value to insert by copying.
     * @returns an \c iterator to the inserted element.
     */
    iterator insert(const value_type& value)
    {
        return this->insert_multi(value);
    }

    /**
     * Inserts an element into the container.
     *
     * All iterators, references and pointers are invalidated.
     *
     * @param value The value to insert by moving.
     * @returns an \c iterator to the inserted element.
     */
    iterator insert(value_type&& value)
    {
        return this->insert_multi(std::move(value));
    }

    /**
     * Inserts an element into the container. Only participates in overload resolution if
     * `std::is_constructible_v<value_type, P&&>` is true.
     *
     * All iterators, references and pointers are invalidated.
     *
     * @param value The value to insert by constructing via `std::forward<P>(value)`.
     * @returns an \c iterator to the inserted element.
     */
    template <class P>
    iterator insert(std::enable_if_t<std::is_constructible<value_type, P&&>::value, P&&> value)
    {
        return insert(value_type{ std::forward<P>(value) });
    }

    /**
     * Constructs an element in-place.
     *
     * All iterators, references and pointers are invalidated.
     *
     * @param args The arguments to pass to the \c value_type constructor.
     * @returns an \c iterator to the inserted element.
     */
    template <class... Args>
    iterator emplace(Args&&... args)
    {
        // We need the key, so just construct the item here
        return insert(value_type{ std::forward<Args>(args)... });
    }

    /**
     * Removes elements with the given key.
     *
     * References, pointers and iterators to the erase element are invalidated. All other iterators, pointers and
     * references remain valid.
     *
     * @param key the key value of elements to remove
     * @returns the number of elements removed.
     */
    size_type erase(const key_type& key)
    {
        size_t count = 0;
        auto vt = this->internal_find(key);
        decltype(vt) next;
        for (; vt; vt = next)
        {
            next = this->_findnext(vt);
            this->internal_erase(vt);
            ++count;
        }
        return count;
    }

    /**
     * Returns the number of elements matching the specified key.
     *
     * @param key The key to check for.
     * @returns The number of elements with the given key.
     */
    size_t count(const key_type& key) const
    {
        return this->internal_count_multi(key);
    }

#ifndef DOXYGEN_BUILD
    using Base::begin;
    using Base::cbegin;
    using Base::cend;
    using Base::end;

    using Base::capacity;
    using Base::empty;
    using Base::max_size;
    using Base::size;

    using Base::clear;
    using Base::erase;
    using Base::swap;

    using Base::contains;
    using Base::equal_range;
    using Base::find;

    using Base::rehash;
    using Base::reserve;
#endif
};

} // namespace container
} // namespace carb
