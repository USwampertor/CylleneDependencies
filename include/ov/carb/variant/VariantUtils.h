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
//! @brief Utilities for *carb.variant.plugin*
#pragma once

#include "VariantTypes.h"

namespace carb
{
namespace variant
{

/**
 * A helper function to translate a typed value into a \ref VariantData via a \ref Translator. A compile error will
 * occur if no \ref Translator could be found for the decayed type.
 * @param type The value of type \c Type.
 * @returns A VariantData constructed from \p type. \ref traits::destruct() must be called on the VariantData when
 * finished with it.
 */
template <class Type>
VariantData translate(Type&& type) noexcept;

//! A structure containing functions for performing the prescribed actions on a \ref VariantData. The functions handle
//! the default behavior if the v-table or v-table function are \c nullptr.
struct traits
{
    /**
     * Swaps two \ref VariantData members.
     * @note \ref VariantData is treated as a trivial type and no v-table functions are required to perform this.
     * @param lhs The left-hand VariantData.
     * @param rhs The right-hand VariantData.
     */
    static void swap(VariantData& lhs, VariantData& rhs) noexcept;

    /**
     * Destructs a \ref VariantData.
     * @note The default behavior (if `!self.vtable->Destructor`) treats \p self as trivially destructible.
     * @param self The VariantData to destruct.
     */
    static void destruct(VariantData& self) noexcept;

    /**
     * Copies a \ref VariantData.
     * @note The default behavior (if `!self.vtable->Copy`) treats \p self as trivially copyable.
     * @param self The VariantData to copy.
     * @returns A VariantData that represents a copy of \p self. The v-table of the return value must be the same as
     * `self.vtable`. When finished with the return value, it must be destroyed via destruct().
     */
    static VariantData copy(const VariantData& self) noexcept;

    /**
     * Tests two \ref VariantData instances for equality.
     * @note The default behavior (if `!self.vtable->Equals`) treats \p self and \p other as trivially comparable (i.e.
     * bitwise compare via \c std::memcmp).
     * @param self The VariantData to compare. This parameter provides the v-table for the comparison.
     * @param other The other VariantData to compare.
     * @returns \c true if \p self and \p other are equal; \c false otherwise.
     */
    static bool equals(const VariantData& self, const VariantData& other) noexcept;

    /**
     * Renders a \ref VariantData as a string for debugging purposes.
     * @note The default behavior (if `!self.vtable->ToString`) produces `"<vtable>:<data>"`.
     * @param self The VariantData to render as a string.
     * @returns A string representing \p self for debugging purposes.
     */
    static omni::string toString(const VariantData& self) noexcept;

    /**
     * Attempts to convert a \ref VariantData to a different type. If \p newType is the same as `self.vtable`, then
     * \ref traits::copy() is invoked instead.
     * @note The default behavior (if `!self.vtable->ConvertTo`) merely returns \c false.
     * @param self The VariantData to convert. This parameter provides the v-table for the comparison.
     */
    static bool convertTo(const VariantData& self, const VTable* newType, VariantData& out) noexcept;

    /**
     * Computes a hash of a \ref VariantData.
     * @note The default behavior (if `!self.vtable->Hash`) produces `size_t(self.data)`.
     * @param self The VariantData to hash.
     * @returns A hash value representing \p self.
     */
    static size_t hash(const VariantData& self) noexcept;
};

//! A wrapper class for managing the lifetime of \ref VariantData and converting the contained value to C++ types.
class Variant final : protected VariantData
{
public:
    /**
     * Default constructor. Produces an empty Variant, that is, \ref hasValue() will return \c false. Any attempt to
     * \ref getValue() will fail and \ref convertTo() will produce an empty Variant. Empty Variants are only equal to
     * other empty Variants.
     */
    Variant() noexcept;

    /**
     * Construct based on given type.
     *
     * To allow copy/move constructors to work properly, this constructor participates in overload resolution only if
     * \c T is not \ref Variant.
     *
     * @warning This function will fail to compile if a \ref Translator cannot be found for \c T.
     * @param val The value to store in the variant.
     */
#ifndef DOXYGEN_BUILD
    template <class T, typename std::enable_if_t<!std::is_same<std::decay_t<T>, Variant>::value, bool> = true>
#else
    template <class T>
#endif
    explicit Variant(T&& val) noexcept
    {
        // This function cannot be externally inlined due to MSVC not understanding the enable_if in the inlined
        // function.
        data() = translate(std::forward<T>(val));
    }

    /**
     * Destructor.
     */
    ~Variant() noexcept;

    /**
     * Copy constructor.
     * @param other The Variant to copy.
     */
    Variant(const Variant& other) noexcept;

    /**
     * Copy-assign operator.
     * @param other The Variant to copy.
     * @returns \c *this
     */
    Variant& operator=(const Variant& other) noexcept;

    /**
     * Move constructor.
     * @param other The Variant to move from. \p other is left in an empty state.
     */
    Variant(Variant&& other) noexcept;

    /**
     * Move-assign operator.
     * @param other The Variant to move from.
     * @returns \c *this
     */
    Variant& operator=(Variant&& other) noexcept;

    /**
     * Tests for equality between two variants.
     * @param other The other Variant.
     * @returns \c true if the Variants are equal; \c false otherwise.
     */
    bool operator==(const Variant& other) const noexcept;

    /**
     * Tests for inequality between two variants.
     * @param other The other Variant.
     * @returns \c true if the Variants are not equal; \c false otherwise.
     */
    bool operator!=(const Variant& other) const noexcept;

    /**
     * Tests if a Variant is empty (i.e. contains no value).
     * @returns \c true if the Variant is empty; \c false otherwise.
     */
    bool hasValue() const noexcept;

    /**
     * Renders the Variant as a string for debugging purposes.
     * @returns The string value of the variant.
     */
    omni::string toString() const noexcept;

    /**
     * Obtains the hash value of the variant.
     * @returns The hash value of the variant.
     */
    size_t getHash() const noexcept;

    /**
     * Attempts to convert the Variant to the given type.
     * @returns A \c optional containing the requested value if conversion succeeds; an empty \c optional otherwise.
     */
    template <class T>
    cpp17::optional<T> getValue() const noexcept;

    /**
     * Attempts to convert the Variant to the given type with a fallback value if conversion fails.
     * @param fallback The default value to return if conversion fails.
     * @returns The contained value if conversion succeeds, or \p fallback if conversion fails.
     */
    template <class T>
    T getValueOr(T&& fallback) const noexcept;

    /**
     * Attempts to convert to a Variant of a different type.
     * @returns A Variant representing a different C++ type if conversion succeeds, otherwise returns an empty Variant.
     */
    template <class T>
    Variant convertTo() const noexcept;

    /**
     * Access the underlying \ref VariantData.
     * @returns The underlying \ref VariantData.
     */
    const VariantData& data() const noexcept
    {
        return *this;
    }

private:
    VariantData& data() noexcept
    {
        return *this;
    }
};
// The Variant class is intended only to add functions on top of VariantData. Therefore, the size must match.
static_assert(sizeof(Variant) == sizeof(VariantData), "");

// This is an ABI-stable representation of std::pair<Variant, Variant>, used only by Translator and internally by
// carb.variant.plugin.
//! @private
struct VariantPair
{
    Variant first;
    Variant second;
};
static_assert(std::is_standard_layout<VariantPair>::value, "");

/**
 * A representation of a key value pair, similar to `std::pair<const Variant, Variant>`.
 * ABI-stable representation to transact with *carb.variant.plugin*.
 */
struct KeyValuePair
{
    const Variant first; //!< The first item in the pair; the key.
    Variant second; //!< The second item in the pair; the value.
};
static_assert(std::is_standard_layout<KeyValuePair>::value, "");

//! Lifetime management wrapper for @ref IVariant::registerType().
class Registrar
{
public:
    /**
     * Default constructor. Constructs an empty registrar.
     */
    constexpr Registrar() noexcept;

    /**
     * Constructor. Registers the type.
     * @note If registration fails, isEmpty() will return `true`.
     * @see IVariant::registerType()
     * @param vtable The v-table pointer to register.
     */
    Registrar(const VTable* vtable) noexcept;

    /**
     * Destructor. Unregisters the type. No-op if isEmpty() returns `true`.
     */
    ~Registrar() noexcept;

    /**
     * Move-construct. Moves the registered type to \c *this and leaves \p other empty.
     * @param other The other Registrar to move from.
     */
    Registrar(Registrar&& other) noexcept;

    /**
     * Move-assign. Swaps state with \p other.
     * @param other The other Registrar to swap state with.
     * @returns \c *this
     */
    Registrar& operator=(Registrar&& other) noexcept;

    /**
     * Checks whether \c *this is empty.
     * @returns \c true if \c *this does not contain a valid type; \c false otherwise.
     */
    bool isEmpty() const noexcept;

    /**
     * Retrieves the registered type.
     * @returns The managed type, or an empty \c RString if \c *this is empty and no type is managed.
     */
    RString getType() const noexcept;

    /**
     * Resets \c *this to an empty state, unregistering any registered type.
     */
    void reset() noexcept;

private:
    RString m_type;
};

} // namespace variant

//! Namespace for *carb.variant* literal helpers.
namespace variant_literals
{

/** Literal cast operator for an unsigned long long variant value.
 *
 *  @param[in] val  The value to be contained in the variant object.
 *  @returns The variant object containing the requested value.
 */
CARB_NODISCARD inline variant::Variant operator"" _v(unsigned long long val) noexcept
{
    return variant::Variant{ val };
}

/** Literal cast operator for a long double variant value.
 *
 *  @param[in] val  The value to be contained in the variant object.
 *  @returns The variant object containing the requested value.
 */
CARB_NODISCARD inline variant::Variant operator"" _v(long double val) noexcept
{
    return variant::Variant{ (double)val };
}

/** Literal cast operator for a string variant value.
 *
 *  @param[in] str      The string to be contained in the variant object.
 *  @param[in] length   The length of the string to be contained in the variant object.
 *  @returns The variant object containing the requested value.
 */
CARB_NODISCARD inline variant::Variant operator"" _v(const char* str, size_t length) noexcept
{
    CARB_UNUSED(length);
    return variant::Variant{ str };
}

} // namespace variant_literals

} // namespace carb

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace std
{

template <>
struct hash<carb::variant::Variant>
{
    size_t operator()(const carb::variant::Variant& v) const noexcept
    {
        return v.getHash();
    }
};

inline std::string to_string(const carb::variant::Variant& v)
{
    auto str = v.toString();
    return std::string(str.begin(), str.end());
}

} // namespace std
#endif
