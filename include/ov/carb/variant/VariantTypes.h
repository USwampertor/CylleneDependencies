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
//! @brief Type definitions for *carb.variant.plugin*
#pragma once

#include "../RString.h"
#include "../IObject.h"
#include "../../omni/String.h"
#include "../../omni/detail/PointerIterator.h"

namespace carb
{
namespace variant
{

//! @defgroup Types that are known (by default) to *carb.variant.plugin*.
//! @{
constexpr RString eNull{ carb::eRString::RS_null }; //!< Represents \c nullptr
constexpr RString eBool{ carb::eRString::RS_bool }; //!< Represents \c bool
constexpr RString eUInt8{ carb::eRString::RS_uint8 }; //!< Represents \c uint8_t or `unsigned char`
constexpr RString eUInt16{ carb::eRString::RS_uint16 }; //!< Represents \c uint16_t or `unsigned short`
constexpr RString eUInt32{ carb::eRString::RS_uint32 }; //!< Represents \c uint32_t or `unsigned int`
constexpr RString eUInt64{ carb::eRString::RS_uint64 }; //!< Represents \c uint64_t or `unsigned long long`
constexpr RString eInt8{ carb::eRString::RS_int8 }; //!< Represents \c int8_t or `signed char`
constexpr RString eInt16{ carb::eRString::RS_int16 }; //!< Represents \c int16_t or `short`
constexpr RString eInt32{ carb::eRString::RS_int32 }; //!< Represents \c int32_t or `int`
constexpr RString eInt64{ carb::eRString::RS_int64 }; //!< Represents \c int64_t or `long long`
constexpr RString eFloat{ carb::eRString::RS_float }; //!< Represents \c float
constexpr RString eDouble{ carb::eRString::RS_double }; //!< Represents \c double
constexpr RString eString{ carb::eRString::RS_string }; //!< Represents \c omni::string
constexpr RString eCharPtr{ carb::eRString::RS_charptr }; //!< Represents `char*` or `const char*`
constexpr RString eDictionary{ carb::eRString::RS_dictionary }; //!< Represents `dictionary::Item`.
constexpr RString eVariantPair{ carb::eRString::RS_variant_pair }; //!< Represents `std::pair<Variant, Variant>`
constexpr RString eVariantArray{ carb::eRString::RS_variant_array }; //!< Represents `VariantArray*`.
constexpr RString eVariantMap{ carb::eRString::RS_variant_map }; //!< Represents `VariantMap*`.
constexpr RString eRString{ carb::eRString::RS_RString }; //!< Represents \c RString
constexpr RString eRStringU{ carb::eRString::RS_RStringU }; //!< Represents \c RStringU
constexpr RString eRStringKey{ carb::eRString::RS_RStringKey }; //!< Represents \c RStringKey
constexpr RString eRStringUKey{ carb::eRString::RS_RStringUKey }; //!< Represents \c RStringUKey
//! @}

struct VTable;
class Variant;

/**
 * A standard-layout @rstref{ABI-safe <abi-compatibility>} struct for communicating variant data. This struct is filled
 * out by \ref Translator specializations.
 *
 * This class should generally not be used directly except by \ref Translator specializations. Instead use the
 * \ref Variant wrapper-class.
 *
 * @see \ref Translator, \ref Variant
 */
struct VariantData
{
    //! The v-table for this variant. Only empty variants are allowed a \c nullptr v-table. The v-table is used to
    //! provide functions for variant behavior and can be used as a type-identifier of sorts.
    const VTable* vtable;

    //! A generic pointer whose interpretation is based on the v-table and the \ref Translator specialization that
    //! created it.
    void* data;
};
static_assert(std::is_standard_layout<VariantData>::value, "");

//! A v-table definition for a variant type. Each registered type has a unique v-table pointer that is retrievable via
//! IVariant::getVTable(). Each entry in the v-table is a function with a default behavior if \c nullptr.
//! @note This class is applicable only to users of *carb.variant.plugin* that author a custom \ref Translator class.
//! @warning Functions in the v-table should not be called directly; the Variant wrapper class calls them through
//! various \ref traits functions.
//! @warning All functions require that `self->vtable->[function]` is equal to the function called.
struct VTable
{
    /**
     * A member used as version control. This member should be set to `sizeof(VTable)` for the version of the
     * v-table that a module is built against.
     */
    uint32_t sizeOf;

    /**
     * Indicates the type name of the v-table. Once registered with \ref IVariant::registerType(), this name can be used
     * to look up the type with \ref IVariant::getVTable().
     *
     * @warning This must be a unique name within the running process and may not match any of the built-in type names.
     */
    RString typeName;

    /**
     * Used to destroy the VariantData::data member. A \c nullptr destructor function indicates that no destruction
     * needs to take place.
     *
     * @param self The VariantData to destroy. Can assume that `self->vtable->Destructor` is the same as the function
     * called.
     */
    void (*Destructor)(VariantData* self) noexcept;

    /**
     * Called to create a functional copy of the given VariantData. A \c nullptr function indicates that VariantData can
     * be trivially copied.
     * @note The resulting VariantData need not have the same v-table as \p self.
     * @param self The VariantData to copy. Can assume that `self->vtable->Copy` is the same as the function called.
     * @returns A VariantData that is a functional copy of \p self. \ref traits::equals() should be \c true for \c *self
     * and the returned VariantData.
     */
    VariantData (*Copy)(const VariantData* self) noexcept;

    /**
     * Called to test equality of \c *self with (possibly different type) \c *other. A \c nullptr function indicates
     * that a trivial comparison of the \ref VariantData is performed (i.e. \c memcmp).
     * @warning Generally speaking, order should not matter: assuming that \c lhs and \c rhs are both
     * `const VariantData*` with non-null \c Equals, it should hold that `lhs->vtable->Equals(lhs, rhs)` should always
     * equal `rhs->vtable->Equals(rhs, lhs)` regardless of their respective v-tables.
     * @param self The VariantData performing the compare. Can assume that `self->vtable->Equals` is the same as the
     * function called.
     * @param other The same or a different VariantData to compare with \p self. May have a different \c vtable than
     * \p self.
     * @returns \c true if the types are equal; \c false otherwise.
     */
    bool (*Equals)(const VariantData* self, const VariantData* other) noexcept;

    /**
     * Called to render the \ref VariantData as a string. A \c nullptr function indicates that a string is produced that
     * contains "<vtable pointer>:<data pointer>".
     * @param self The VariantData to render. Can assume that `self->vtable->ToString` is the same as the function
     * called.
     * @returns A type-dependent printable string representing the type, useful for debugging.
     */
    omni::string (*ToString)(const VariantData* self) noexcept;

    /**
     * Called to attempt to convert \p self to a different type. A \c nullptr function is the same as returning false.
     * @warning If \c false is returned, \p out is in an undefined state. If and only if \c true is returned,
     * \ref traits::destruct() must be called at some later point on \c *out.
     * @note Generally speaking, \ref Equals() and \ref ConvertTo() should understand the same types.
     * @param self The VariantData performing the conversion. Can assume that `self->vtable->ConvertTo` is this same as
     * the function called.
     * @param newtype A v-table representing the type to attempt to convert to. If the function recognizes the given
     * v-table (which should not be the same as `self->vtable`) and can convert to that type then the function should
     * write to \p out and return \c true. If the v-table is not recognized, the function must return \c false.
     * @param out If \c true is returned from the function, this must be a valid \ref VariantData and must be later
     * destroyed with \ref traits::destruct(). If \c false is returned then the state of \p out is undefined. There is
     * no requirement that `out->vtable` matches \p newtype if \c true is returned; it must merely be valid.
     * @returns \c true if and only if \p out contains a valid converted representation of \p self; \c false otherwise.
     */
    bool (*ConvertTo)(const VariantData* self, const VTable* newtype, VariantData* out) noexcept;

    /**
     * Computes a hash of \p self. A \c nullptr function casts `self->data` to a `size_t` for use as a hash.
     * @param self The VariantData to hash. Can assume that `self->vtable->Hash` is the same as the function called.
     * @returns A value to use as a hash identifying \c *self.
     */
    size_t (*Hash)(const VariantData* self) noexcept;

    // Note to maintainers: adding new functions here does not necessarily require a version change for IVariant. Add a
    // `struct traits` function that performs a default behavior if the function is `nullptr` or if the `sizeOf` is less
    // than the offset of your new member. All calls to the v-table function should happen in the new `traits` function.
};
static_assert(std::is_standard_layout<VTable>::value, "");

/**
 * An array-of-variants type that can itself be contained in a \ref Variant.
 *
 * Similar in many respects to `std::vector`, but reference-counted and implemented within *carb.variant.plugin*.
 *
 * Created via \ref IVariant::createArray().
 */
class VariantArray : public IObject
{
public:
    //! A type conforming to RandomAccessIterator.
    using iterator = omni::detail::PointerIterator<Variant*, VariantArray>;

    //! A type conforming to RandomAccessIterator.
    using const_iterator = omni::detail::PointerIterator<const Variant*, VariantArray>;

    /**
     * Provides direct access to the underlying array.
     * @returns the beginning of the underlying array.
     */
    virtual Variant* data() noexcept = 0;

    //! @copydoc data()
    virtual const Variant* data() const noexcept = 0;

    /**
     * Returns the number of variants contained.
     * @returns the number of variants contained.
     */
    virtual size_t size() const noexcept = 0;

    /**
     * Adds a variant to the end of the array.
     * @param v The \ref Variant to add to the array.
     */
    virtual void push_back(Variant v) noexcept = 0;

    /**
     * Attempts to insert a variant at the given offset.
     *
     * The given @p offset must be in `[0, size()]`, otherwise \c false is returned.
     *
     * @warning This is an O(n) operation.
     *
     * @param offset The 0-based offset indicating where to insert. The elements at that position (and all subsequent
     * elements) will be pushed back to make room for @p v.
     * @param v The \ref Variant to insert.
     * @returns \c true if the given variant was inserted; \c false otherwise.
     */
    virtual bool insert(size_t offset, Variant v) noexcept = 0;

    /**
     * Attempts to erase the variant at the given offset.
     *
     * The given @p offset must be in `[0, size())`, otherwise \c false is returned.
     *
     * @warning This is an O(n) operation.
     *
     * @param offset The 0-based offset indicating which element to erase. The elements following that position will be
     * moved forward to fill in the gap removed at @p offset.
     * @returns \c true if the variant was erased; \c false otherwise.
     */
    virtual bool erase(size_t offset) noexcept = 0;

    /**
     * Pops the last element from the array.
     *
     * @returns \c true if the element was popped; \c false if the array is empty.
     */
    virtual bool pop_back() noexcept = 0;

    /**
     * Clears the existing array elements and assigns new elements.
     *
     * @param p The beginning of the new array elements.
     * @param count The number of array elements in the raw array @p p.
     */
    virtual void assign(const Variant* p, size_t count) noexcept = 0;

    /**
     * Reserves space for elements.
     *
     * @param count The number of elements to reserve space for, exactly. If this amount is less than the current space,
     * the request is ignored.
     */
    virtual void reserve(size_t count) noexcept = 0;

    /**
     * Changes the number of elements stored.
     *
     * @param count The number of elements to store in the array. Elements at the end of the array are added (as via
     * \ref Variant default construction) or removed so that following this call \ref size() matches @p count. Note that
     * resizing heuristics may be applied, so \ref capacity() following this call may be greater than @p count.
     */
    virtual void resize(size_t count) noexcept = 0;

    /**
     * Returns the number of elements that can be stored with the current allocated space.
     * @returns The number of elements that can be stored with the current allocated space.
     */
    virtual size_t capacity() const noexcept = 0;

    /**
     * Erases all elements from the array and leaves the array empty.
     */
    void clear() noexcept;

    /**
     * Checks whether the array is empty.
     *
     * @returns \c true if the array is empty (contains no elements); \c false otherwise.
     */
    bool empty() const noexcept;

    /**
     * Accesses an element with bounds checking.
     * @throws std::out_of_range if @p index is outside of `[0, size())`.
     * @param index The index of the array to access.
     * @returns a reference to the element at the requested @p index.
     */
    Variant& at(size_t index);

    //! @copydoc at()
    const Variant& at(size_t index) const;

    /**
     * Accesses an element without bounds checking.
     * @warning Providing an @p index value outside of `[0, size())` is undefined behavior.
     * @param index The index of the array to access.
     * @returns a reference to the element at the requested @p index.
     */
    Variant& operator[](size_t index) noexcept;

    //! @copydoc operator[]()
    const Variant& operator[](size_t index) const noexcept;

    /**
     * Accesses the element at the front of the array.
     * @warning Undefined behavior if \ref empty().
     * @returns a reference to the element at the front of the array.
     */
    Variant& front() noexcept;

    //! @copydoc front()
    const Variant& front() const noexcept;

    /**
     * Accesses the element at the back of the array.
     * @warning Undefined behavior if \ref empty().
     * @returns a reference to the element at the back of the array.
     */
    Variant& back() noexcept;

    //! @copydoc back()
    const Variant& back() const noexcept;

    /**
     * Provides iteration and ranged-for support; returns an iterator to the first element.
     *
     * @warning Iterators follow invalidation rules for `std::vector`.
     * @returns An iterator to the first element.
     */
    iterator begin() noexcept;

    /**
     * Provides iteration and ranged-for support; returns an iterator representing the iteration end.
     *
     * @warning Iterators follow invalidation rules for `std::vector`.
     * @returns An iterator representing the iteration end.
     */
    iterator end() noexcept;

    //! @copydoc begin()
    const_iterator begin() const noexcept;

    //! @copydoc end()
    const_iterator end() const noexcept;
};
//! Helper definition.
using VariantArrayPtr = ObjectPtr<VariantArray>;

struct KeyValuePair;

/**
 * An associative array (i.e. "map") of key/value Variant pairs that can itself be contained in a \ref Variant.
 *
 * Similar in many respects to `std::unordered_map`, but reference-counted and implemented within *carb.variant.plugin*.
 *
 * @note This is an *unordered* container, meaning that iterating over all values may not be in the same order as they
 * were inserted. This is a *unique* container, meaning that inserting a key that already exists in the container will
 * replace the previous key/value pair.
 *
 * Created via \ref IVariant::createMap().
 */
class VariantMap : public IObject
{
public:
    //! The key type
    using key_type = Variant;
    //! The mapped value type
    using mapped_type = Variant;
    //! The value type
    using value_type = KeyValuePair;
    //! Unsigned integer type
    using size_type = size_t;
    //! Signed integer type
    using difference_type = ptrdiff_t;
    //! Reference type
    using reference = value_type&;
    //! Const reference type
    using const_reference = const value_type&;
    //! Pointer type
    using pointer = value_type*;
    //! Const pointer type
    using const_pointer = const value_type*;

    // clang-format off
#ifndef DOXYGEN_SHOULD_SKIP_THIS
private:
    class iter_base
    {
    public:
        constexpr iter_base() noexcept = default;
        bool operator == (const iter_base& other) const noexcept { CARB_ASSERT(owner == other.owner); return where == other.where; }
        bool operator != (const iter_base& other) const noexcept { CARB_ASSERT(owner == other.owner); return where != other.where; }
    protected:
        constexpr iter_base(const VariantMap* owner_, pointer where_) noexcept : owner(owner_), where(where_) {}
        const VariantMap* owner{ nullptr };
        pointer where{ nullptr };
    };
public:
    class const_find_iterator : public iter_base
    {
        using Base = iter_base;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = VariantMap::value_type;
        using difference_type = VariantMap::difference_type;
        using pointer = VariantMap::const_pointer;
        using reference = VariantMap::const_reference;
        constexpr               const_find_iterator() noexcept = default;
        reference               operator *  () const noexcept   { CARB_ASSERT(this->where); return *this->where; }
        pointer                 operator -> () const noexcept   { CARB_ASSERT(this->where); return this->where; }
        const_find_iterator&    operator ++ () noexcept         { incr(); return *this; }
        const_find_iterator     operator ++ (int) noexcept      { const_find_iterator i{ *this }; incr(); return i; }
    protected:
        friend class VariantMap;
        constexpr const_find_iterator(const VariantMap* owner_, value_type* where_) noexcept : Base(owner_, where_) {}
        void incr() { CARB_ASSERT(this->owner && this->where); this->where = this->owner->findNext(this->where); }
    };
    class find_iterator : public const_find_iterator
    {
        using Base = const_find_iterator;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = VariantMap::value_type;
        using difference_type = VariantMap::difference_type;
        using pointer = VariantMap::pointer;
        using reference = VariantMap::reference;
        constexpr       find_iterator() noexcept = default;
        reference       operator *  () const noexcept   { CARB_ASSERT(this->where); return *this->where; }
        pointer         operator -> () const noexcept   { CARB_ASSERT(this->where); return this->where; }
        find_iterator&  operator ++ () noexcept         { this->incr(); return *this; }
        find_iterator   operator ++ (int) noexcept      { find_iterator i{ *this }; this->incr(); return i; }
    protected:
        friend class VariantMap;
        constexpr find_iterator(const VariantMap* owner_, value_type* where_) noexcept : Base(owner_, where_) {}
    };

    class const_iterator : public iter_base
    {
        using Base = iter_base;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = VariantMap::value_type;
        using difference_type = VariantMap::difference_type;
        using pointer = VariantMap::const_pointer;
        using reference = VariantMap::const_reference;
        constexpr       const_iterator() noexcept = default;
        reference       operator *  () const noexcept   { CARB_ASSERT(this->where); return *this->where; }
        pointer         operator -> () const noexcept   { CARB_ASSERT(this->where); return this->where; }
        const_iterator& operator ++ () noexcept         { incr(); return *this;}
        const_iterator  operator ++ (int) noexcept      { const_iterator i{ *this }; incr(); return i; }
    protected:
        friend class VariantMap;
        constexpr const_iterator(const VariantMap* owner_, value_type* where_) noexcept : Base{ owner_, where_ } {}
        void incr() { CARB_ASSERT(this->owner && this->where); this->where = this->owner->iterNext(this->where); }
    };
    class iterator : public const_iterator
    {
        using Base = const_iterator;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = VariantMap::value_type;
        using difference_type = VariantMap::difference_type;
        using pointer = VariantMap::pointer;
        using reference = VariantMap::reference;
        constexpr   iterator() noexcept = default;
        reference   operator *  () const noexcept   { CARB_ASSERT(this->where); return *this->where; }
        pointer     operator -> () const noexcept   { CARB_ASSERT(this->where); return this->where; }
        iterator&   operator ++ () noexcept         { this->incr(); return *this; }
        iterator    operator ++ (int) noexcept      { iterator i{ *this }; this->incr(); return i; }
    protected:
        friend class VariantMap;
        constexpr iterator(const VariantMap* owner_, value_type* where_) noexcept : Base(owner_, where_) {}
    };
#endif
    // clang-format on

    /**
     * Creates an iterator to the first element in the container.
     *
     * @returns a `const_iterator` to the first element in the container. If the container is \ref empty() the iterator
     * will be equal to \ref cend().
     */
    const_iterator cbegin() const noexcept;

    /**
     * Creates an iterator to the first element in the container.
     *
     * @returns a `const_iterator` to the first element in the container. If the container is \ref empty() the iterator
     * will be equal to \ref end().
     */
    const_iterator begin() const noexcept;

    /**
     * Creates an iterator to the first element in the container.
     *
     * @returns an `iterator` to the first element in the container. If the container is \ref empty() the iterator
     * will be equal to \ref end().
     */
    iterator begin() noexcept;

    /**
     * Creates an iterator to the past-the-end element in the container.
     *
     * @returns a `const_iterator` to the past-the-end element in the container. This iterator is a placeholder;
     * attempting to access it results in undefined behavior.
     */
    const_iterator cend() const noexcept;

    /**
     * Creates an iterator to the past-the-end element in the container.
     *
     * @returns a `const_iterator` to the past-the-end element in the container. This iterator is a placeholder;
     * attempting to access it results in undefined behavior.
     */
    const_iterator end() const noexcept;

    /**
     * Creates an iterator to the past-the-end element in the container.
     *
     * @returns an `iterator` to the past-the-end element in the container. This iterator is a placeholder;
     * attempting to access it results in undefined behavior.
     */
    iterator end() noexcept;

    /**
     * Checks if the container is empty.
     * @returns `true` if the container is empty; `false` otherwise.
     */
    bool empty() const noexcept;

    /**
     * Returns the number of keys contained.
     * @returns the number of keys contained.
     */
    virtual size_t size() const noexcept = 0;

    /**
     * Attempts to insert a new element into the container.
     *
     * If insertion is successful, all iterators, references and pointers are invalidated.
     *
     * \warning Variant comparison rules are taken in account. For instance, since Variant(bool) is considered equal
     * with Variant(int) for false/0 and true/1, these values would conflict.
     *
     * @param key The key to insert into the map.
     * @param value The value to associate with \p key. If the key already exists in the container, this value is not
     * used.
     * @returns A `pair` consisting of an `iterator` to the inserted element (or the existing element that prevented the
     * insertion) and a `bool` that will be `true` if insertion took place or `false` if insertion did *not* take place.
     */
    std::pair<iterator, bool> insert(const Variant& key, Variant value);

    /**
     * Erases a key from the map.
     * @param key The key value to erase.
     * @returns The number of entries removed from the map. This will be `0` if the key was not found or `1` if the key
     * was found and removed.
     */
    size_t erase(const Variant& key) noexcept;

    /**
     * Removes the given element.
     *
     * References, pointers and iterators to the erased element are invalidated. All other iterators, pointers and
     * references remain valid.
     *
     * @param pos The `const_iterator` to the element to remove. This iterator must be valid and dereferenceable.
     * @returns the iterator immediately following \p pos.
     */
    iterator erase(const_iterator pos) noexcept;

    /**
     * Removes the given element.
     *
     * References, pointers and iterators to the erased element are invalidated. All other iterators, pointers and
     * references remain valid.
     *
     * @param pos The `const_find_iterator` to the element to remove. This iterator must be valid and dereferenceable.
     * @returns the iterator immediately following \p pos.
     */
    find_iterator erase(const_find_iterator pos) noexcept;

    /**
     * Finds the first element with the specified key.
     *
     * \note `find_iterator` objects returned from this function will only iterate through elements with the same key;
     * they cannot be used to iterate through the entire container.
     *
     * @param key The key to search for.
     * @returns a `find_iterator` to the first element matching \p key, or \ref end() if no element was found matching
     * \p key.
     */
    find_iterator find(const Variant& key) noexcept;

    /**
     * Finds the first element with the specified key.
     *
     * \note `const_find_iterator` objects returned from this function will only iterate through elements with the same
     * key; they cannot be used to iterate through the entire container.
     *
     * @param key The key to search for.
     * @returns a `const_find_iterator` to the first element matching \p key, or \ref end() if no element was found
     * matching \p key.
     */
    const_find_iterator find(const Variant& key) const noexcept;

    /**
     * Checks whether the container has an element matching a given key.
     *
     * @param key The key of the element to search for.
     * @returns `true` if at least one element matching \p key exists in the container; \c false otherwise.
     */
    bool contains(const Variant& key) const noexcept;

    /**
     * Counts the number of elements matching a given key.
     *
     * \note as this is a unique container, this will always be either 0 or 1.
     * @param key The key to count.
     * @returns the number of elements matching \p key.
     */
    size_t count(const Variant& key) const noexcept;

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
    mapped_type& at(const Variant& key);

    //! @copydoc at()
    const mapped_type& at(const Variant& key) const;
#endif

    /**
     * Returns a reference to a value that is mapped to the given key, performing an insertion if such key does not
     * already exist.
     *
     * If \p key does not exist, the returned type will be a default-constructed \ref Variant.
     *
     * @param key the key of the element to find or insert
     * @returns a reference to the \ref mapped_type mapped to \p key.
     */
    mapped_type& operator[](const Variant& key);

    /**
     * Clears the contents. O(n) over \ref capacity().
     *
     * Erases all elements from the container. After this call \ref size() returns zero. Invalidates all iterators,
     * pointers and references to contained elements.
     *
     * @note This does not free the memory used by the container. To free the hash table memory, use `rehash(0)` after
     * this call.
     */
    virtual void clear() noexcept = 0;

    /**
     * Returns the number of elements that can be stored with the current memory usage.
     * @see reserve()
     * @returns the number of elements that can be stored with the current memory usage.
     */
    virtual size_t capacity() const noexcept = 0;

    /**
     * Reserves space for at least the specified number of elements and regenerates the hash table.
     *
     * Sets \ref capacity() of \c *this to a value greater-than-or-equal-to \p n. If \ref capacity() already exceeds
     * \p n, nothing happens.
     *
     * If a rehash occurs, all iterators, pointers and references to existing elements are invalidated.
     *
     * @param n The desired minimum capacity of `*this`.
     */
    virtual void reserve(size_t n) noexcept = 0;

    /**
     * Sets the capacity of the container to the lowest valid value greater-than-or-equal-to the given value, and
     * rehashes the container.
     *
     * If \p n is less-than \ref size(), \ref size() is used instead.
     *
     * If the container is empty and \p n is zero, the memory for the container is freed.
     *
     * After this function is called, all iterators, pointers and references to existing elements are invalidated.
     *
     * @param n The minimum capacity for the container. The actual size of the container may be larger than this.
     */
    virtual void rehash(size_t n) noexcept = 0;

private:
    virtual KeyValuePair* internalInsert(const Variant& key, bool& success) noexcept = 0;
    virtual void internalErase(const KeyValuePair*) noexcept = 0;
    virtual KeyValuePair* internalFind(const Variant& key) const noexcept = 0;
    virtual KeyValuePair* internalBegin() const noexcept = 0;
    virtual KeyValuePair* iterNext(KeyValuePair*) const noexcept = 0;
    virtual KeyValuePair* findNext(KeyValuePair*) const noexcept = 0;
};
//! Helper definition.
using VariantMapPtr = ObjectPtr<VariantMap>;

/**
 * Default implementation of a Translator type.
 *
 * Translator structs provide a \ref VTable and instruct the Variant system in how the \ref VariantData::data
 * member is to be interpreted for conversion to-and-from C++ types.
 *
 * All Translator specializations must provide three functions:
 *   - `RString type() const noexcept` - Retrieves the registered name of the type known to \ref IVariant via
 *     \ref IVariant::registerType(). The v-table will be looked up via \ref translate().
 *   - `void* data(T&&) const noexcept` - This function must convert the given value to a \c void* representation that
 *     is stored in the \ref VariantData struct. If this function allocates memory it should be from
 *     \ref carb::allocate() or originate within the plugin that contains the \ref VTable::Destructor() function
 *     that will be freeing the memory.
 *   - `T value(void*) const noexcept` - This function is the opposite of the \c data() function--it converts the
 *     \c void* value from \ref VariantData::data and converts it back to type \c T.
 *
 * Translator specializations are present for the following built-in types:
 *   - \c std::nullptr_t.
 *     * Does not convert to any other type.
 *     * Is only equal with other \c std::nullptr_t types.
 *   - \c bool
 *     * Can be convert to any integral type (will produce 0 or 1).
 *     * Will be equal with integer values of 0 or 1.
 *   - Integral types (8-, 16-, 32- and 64-bit; signed and unsigned).
 *     * Will convert to any other integral type as long as the value is representable in that type. For instance, a
 *       `Variant(-1)` would fail to convert to `unsigned`, and `Variant(999)` would fail to convert to `uint8_t`, but
 *       `Variant(uint64_t(-1))` would convert just fine to `int8_t`.
 *     * Equality checks follow the same rules as conversion.
 *     * Not convertible to floating point due to potential data loss.
 *     * Convertible to \c bool only if the value is 0 or 1.
 *   - \c float and \c double
 *     * Will convert to each other, but will not convert to integral types due to potential data loss.
 *     * Equality checks follows conversion rules, but will compare as the larger type.
 *   - \c omni::string
 *     * Convertible to `const char*`, but this value must only be used transiently--it is equivalent to `c_str()` and
 *       follows the same rules for lifetime of the pointer from `c_str()`.
 *     * Equality compares via `operator ==` for \c omni::string, and comparable with `[const] char*`.
 *   - `const char*`
 *     * Stores the pointer, so memory lifetime must be longer than the Variant.
 *     * Accepts a `char*` but stored as a `const char*`; any attempts to convert to `char*` will fail.
 *     * Attempts to copy a Variant containing a `const char*` just copy the same pointer, so the lifetime guarantee
 *       must include these copies as well.
 *     * Comparable with \c omni::string.
 *   - `dictionary::Item*`
 *     * Stores the pointer, so memory lifetime must be longer than the Variant.
 *     * Copying the variant will trivially copy the pointer.
 *     * Comparison will trivially compare the pointer.
 *   - `carb::Strong` (Carbonite strong types)
 *     * Auto-converts to and from the underlying numeric type (i.e. `int`, `size_t`, etc.), so it will lose the type
 *       safety of the strong type.
 *     * Comparable with similar numeric types.
 *   - \ref VariantArray* / \ref VariantArrayPtr
 *     * Comparable only with other VariantArray types, by pointer value.
 *     * Hashes based on the pointer value, not the contained values.
 *     * Variants containing this type always hold a reference.
 *   - \ref VariantMap* / \ref VariantMapPtr
 *     * Comparable only with other VariantMap types, by pointer value.
 *     * Hashes based on the pointer value, not the contained values.
 *     * Variants containing this type always hold a reference.
 *   - \ref RString / \ref RStringU / \ref RStringKey / \ref RStringUKey
 *     * Types are comparable with other instances of the same type.
 *     * Key types are only comparable with Key types; RString and RStringKey will compare with RStringU and RStringKeyU
 *       respectively, as uncased comparisons.
 *     * Hashing is as by the `getHash()` function for each of the RString types.
 *     * RString and RStringU can be converted to `const char*` or `omni::string` as if by `c_str()`.
 *     * RStringKey and RStringUKey can be converted to `omni::string` as if by `toString()`.
 *
 * @warning The default template does not provide the above functions which will allow compile to fail for unrecognized
 * types. Translations are available through specializations only.
 *
 * @tparam T The type handled by the specialization.
 * @tparam Enable A template parameter that can be used for SFINAE.
 */
template <class T, class Enable = void>
struct Translator;

} // namespace variant
} // namespace carb
