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
//! @brief Carbonite Robin-hood Unordered Map container.
#pragma once

#include "RobinHoodImpl.h"

namespace carb
{
namespace container
{

/**
 * Implements an Unordered Map, that is: a container that contains a mapping of keys to values where all keys must be
 * unique. There is no defined order to the set of keys.
 *
 * \copydetails detail::RobinHood
 *
 * @warning This container is similar to, but not a drop-in replacement for `std::unordered_map` due to differences in
 * iterator invalidation and memory layout.
 *
 * Iterator/reference/pointer invalidation (note differences from `std::unordered_map`):
 * Operation | Invalidates
 * --------- | -----------
 * All read operations: Never
 * `clear`, `rehash`, `reserve`, `operator=`, `insert`, `emplace`, `try_emplace`, `operator[]` | Always
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
class RHUnorderedMap
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
    constexpr RHUnorderedMap() noexcept = default;

    /**
     * Copy constructor. Copies elements from another container.
     *
     * @note \c *this may have a different \ref carb::container::detail::RobinHood::capacity()
     *       than \p other.
     * @param other The other container to copy entries from.
     */
    RHUnorderedMap(const RHUnorderedMap& other) : Base(other)
    {
    }

    /**
     * Move constructor. Moves elements from another container.
     *
     * @note No move constructors on contained elements are invoked. \p other will be
     *       \ref carb::container::detail::RobinHood::empty() after this operation.
     * @param other The other container to move entries from.
     */
    RHUnorderedMap(RHUnorderedMap&& other) : Base(std::move(other))
    {
    }

    /**
     * Destructor. Destroys all contained elements and frees memory.
     */
    ~RHUnorderedMap() = default;

    /**
     * Copy-assign operator. Destroys all currently stored elements and copies elements from another container.
     *
     * @param other The other container to copy entries from.
     * @returns \c *this
     */
    RHUnorderedMap& operator=(const RHUnorderedMap& other)
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
    RHUnorderedMap& operator=(RHUnorderedMap&& other)
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
        // We need the key, so just construct the item here
        return insert(value_type{ std::forward<Args>(args)... });
    }

    /**
     * Inserts in-place if the key does not exist; does nothing if the key already exists.
     *
     * Inserts a new element into the container with key \p key and value constructed with \p args if there is no
     * element with the key in the container. If the key does not exist and the insert succeeds, constructs `value_type`
     * as `value_type{std::piecewise_construct, std::forward_as_tuple(key),
     * std::forward_as_tuple(std::forward<Args>(args)...}`.
     *
     * @param key The key used to look up existing and insert if not found.
     * @param args The args used to construct \ref mapped_type.
     * @returns A \c pair consisting of an iterator to the inserted element (or the existing element that prevented the
     * insertion) and a \c bool that will be \c true if insertion took place or \c false if insertion did \a not take
     * place.
     */
    template <class... Args>
    std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args)
    {
        auto result = this->internal_insert(key);
        if (result.second)
            new (result.first) value_type(std::piecewise_construct, std::forward_as_tuple(key),
                                          std::forward_as_tuple(std::forward<Args>(args)...));
        return std::make_pair(this->make_iter(result.first), result.second);
    }

    /**
     * Inserts in-place if the key does not exist; does nothing if the key already exists.
     *
     * Inserts a new element into the container with key \p key and value constructed with \p args if there is no
     * element with the key in the container. If the key does not exist and the insert succeeds, constructs `value_type`
     * as `value_type{std::piecewise_construct, std::forward_as_tuple(std::move(key)),
     * std::forward_as_tuple(std::forward<Args>(args)...}`.
     *
     * @param key The key used to look up existing and insert if not found.
     * @param args The args used to construct \ref mapped_type.
     * @returns A \c pair consisting of an iterator to the inserted element (or the existing element that prevented the
     * insertion) and a \c bool that will be \c true if insertion took place or \c false if insertion did \a not take
     * place.
     */
    template <class... Args>
    std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args)
    {
        auto result = this->internal_insert(key);
        if (result.second)
            new (result.first) value_type(std::piecewise_construct, std::forward_as_tuple(std::move(key)),
                                          std::forward_as_tuple(std::forward<Args>(args)...));
        return std::make_pair(this->make_iter(result.first), result.second);
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

#if CARB_EXCEPTIONS_ENABLED || defined(DOXYGEN_BUILD)
    /**
     * Access specified element with bounds checking.
     *
     * This function is only available if exceptions are enabled.
     *
     * @param key The key of the element to find.
     * @returns a reference to the mapped value of the element with key equivalent to \p key.
     * @throws std::out_of_range if no such element exists.
     */
    mapped_type& at(const key_type& key)
    {
        auto vt = this->internal_find(key);
        if (vt)
            return vt->second;
        throw std::out_of_range("key not found");
    }

    /**
     * Access specified element with bounds checking.
     *
     * This function is only available if exceptions are enabled.
     *
     * @param key The key of the element to find.
     * @returns a reference to the mapped value of the element with key equivalent to \p key.
     * @throws std::out_of_range if no such element exists.
     */
    const mapped_type& at(const key_type& key) const
    {
        auto vt = this->internal_find(key);
        if (vt)
            return vt->second;
        throw std::out_of_range("key not found");
    }
#endif

    /**
     * Returns a reference to a value that is mapped to the given key, performing an insertion if such key does not
     * already exist.
     *
     * If \p key does not exist, inserts a \c value_type constructed in-place from
     * `std::piecewise_construct, std::forward_as_tuple(key), std::tuple<>()`.
     *
     * \ref key_type must be \a CopyConstructible and \ref mapped_type must be \a DefaultConstructible.
     * @param key the key of the element to find or insert
     * @returns a reference to the \ref mapped_type mapped to \p key.
     */
    mapped_type& operator[](const key_type& key)
    {
        auto result = this->internal_insert(key);
        if (result.second)
            new (result.first) value_type(std::piecewise_construct, std::forward_as_tuple(key), std::tuple<>());
        return result.first->second;
    }

    /**
     * Returns a reference to a value that is mapped to the given key, performing an insertion if such key does not
     * already exist.
     *
     * If \p key does not exist, inserts a \c value_type constructed in-place from
     * `std::piecewise_construct, std::forward_as_tuple(std::move(key)), std::tuple<>()`.
     *
     * \ref key_type must be \a CopyConstructible and \ref mapped_type must be \a DefaultConstructible.
     * @param key the key of the element to find or insert
     * @returns a reference to the \ref mapped_type mapped to \p key.
     */
    mapped_type& operator[](key_type&& key)
    {
        auto result = this->internal_insert(key);
        if (result.second)
            new (result.first) value_type(std::piecewise_construct, std::forward_as_tuple(key), std::tuple<>());
        return result.first->second;
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
