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
//! @brief Carbonite Robin-hood Unordered Set container.
#pragma once

#include "RobinHoodImpl.h"

namespace carb
{
namespace container
{

/**
 * Implements an Unordered Set, that is: a container that contains a set of keys that all must be unique. There is no
 * defined order to the set of keys.
 *
 * \copydetails detail::RobinHood
 *
 * @warning This container is similar to, but not a drop-in replacement for `std::unordered_set` due to differences in
 * iterator invalidation and memory layout.
 *
 * Iterator/reference/pointer invalidation (note differences from `std::unordered_set`):
 * Operation | Invalidates
 * --------- | -----------
 * All read operations | Never
 * `clear`, `rehash`, `reserve`, `operator=`, `insert`, `emplace` | Always
 * `erase` | Only the element removed
 * `swap` | All iterators, no pointers/references
 *
 * @tparam Key The key type
 * @tparam Hasher A functor to use as a hashing function for \c Key
 * @tparam Equals A functor to use to compare two \c Key values for equality
 * @tparam LoadFactorMax100 The load factor to use for the table. This value must be in the range `[10, 100]` and
 * represents the percentage of entries in the hash table that will be filled before resizing. Open-addressing hash maps
 * with 100% usage have better memory usage but worse performance since they need "gaps" in the hash table to terminate
 * runs.
 */
template <class Key, class Hasher = std::hash<Key>, class Equals = std::equal_to<Key>, size_t LoadFactorMax100 = 80>
class RHUnorderedSet
    : public detail::RobinHood<LoadFactorMax100, Key, const Key, detail::Identity<Key, const Key>, Hasher, Equals>
{
    using Base = detail::RobinHood<LoadFactorMax100, Key, const Key, detail::Identity<Key, const Key>, Hasher, Equals>;

public:
    //! The key type
    using key_type = typename Base::key_type;
    //! The value type (effectively `const key_type`)
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
    constexpr RHUnorderedSet() noexcept = default;

    /**
     * Copy constructor. Copies elements from another container.
     *
     * @note \c *this may have a different \ref capacity() than \p other.
     * @param other The other container to copy entries from.
     */
    RHUnorderedSet(const RHUnorderedSet& other) : Base(other)
    {
    }

    /**
     * Move constructor. Moves elements from another container.
     *
     * @note No move constructors on contained elements are invoked. \p other will be \ref empty() after this operation.
     * @param other The other container to move entries from.
     */
    RHUnorderedSet(RHUnorderedSet&& other) : Base(std::move(other))
    {
    }

    /**
     * Destructor. Destroys all contained elements and frees memory.
     */
    ~RHUnorderedSet() = default;

    /**
     * Copy-assign operator. Destroys all currently stored elements and copies elements from another container.
     *
     * @param other The other container to copy entries from.
     * @returns \c *this
     */
    RHUnorderedSet& operator=(const RHUnorderedSet& other)
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
    RHUnorderedSet& operator=(RHUnorderedSet&& other)
    {
        Base::operator=(std::move(other));
        return *this;
    }

    /**
     * Inserts an element into the container.
     *
     * If insertion is successful, all iterators, references and pointers are invalidated.
     *
     * @param value The value to insert by copying.
     * @returns A \c pair consisting of an iterator to the inserted element (or the existing element that prevented the
     * insertion) and a \c bool that will be \c true if insertion took place or \c false if insertion did \a not take
     * place.
     */
    std::pair<iterator, bool> insert(const value_type& value)
    {
        return this->insert_unique(value);
    }

    /**
     * Inserts an element into the container.
     *
     * If insertion is successful, all iterators, references and pointers are invalidated.
     *
     * @param value The value to insert by moving.
     * @returns A \c pair consisting of an iterator to the inserted element (or the existing element that prevented the
     * insertion) and a \c bool that will be \c true if insertion took place or \c false if insertion did \a not take
     * place.
     */
    std::pair<iterator, bool> insert(value_type&& value)
    {
        return this->insert_unique(std::move(value));
    }

    /**
     * Inserts an element into the container. Only participates in overload resolution if
     * `std::is_constructible_v<value_type, P&&>` is true.
     *
     * If insertion is successful, all iterators, references and pointers are invalidated.
     *
     * @param value The value to insert by constructing via `std::forward<P>(value)`.
     * @returns A \c pair consisting of an iterator to the inserted element (or the existing element that prevented the
     * insertion) and a \c bool that will be \c true if insertion took place or \c false if insertion did \a not take
     * place.
     */
    template <class P>
    std::pair<iterator, bool> insert(std::enable_if_t<std::is_constructible<value_type, P&&>::value, P&&> value)
    {
        return insert(value_type{ std::forward<P>(value) });
    }

    /**
     * Constructs an element in-place.
     *
     * If insertion is successful, all iterators, references and pointers are invalidated.
     *
     * @param args The arguments to pass to the \c value_type constructor.
     * @returns A \c pair consisting of an iterator to the inserted element (or the existing element that prevented the
     * insertion) and a \c bool that will be \c true if insertion took place or \c false if insertion did \a not take
     * place.
     */
    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args)
    {
        // The value is the key, so just construct the item here
        return insert(value_type{ std::forward<Args>(args)... });
    }

    /**
     * Removes elements with the given key.
     *
     * References, pointers and iterators to the erase element are invalidated. All other iterators, pointers and
     * references remain valid.
     *
     * @param key the key value of elements to remove
     * @returns the number of elements removed (either 1 or 0).
     */
    size_type erase(const key_type& key)
    {
        auto vt = this->internal_find(key);
        if (vt)
        {
            this->internal_erase(vt);
            return 1;
        }
        return 0;
    }

    /**
     * Returns the number of elements matching the specified key.
     *
     * @param key The key to check for.
     * @returns The number of elements with the given key (either 1 or 0).
     */
    size_t count(const key_type& key) const
    {
        return !!this->internal_find(key);
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
