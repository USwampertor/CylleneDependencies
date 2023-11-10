// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Registered String utility. See carb::RString for more info.
#pragma once

#include "Defines.h"

#define RSTRINGENUM_FROM_RSTRING_H
#include "RStringEnum.inl"
#undef RSTRINGENUM_FROM_RSTRING_H

#include <memory> // for std::owner_before
#include <ostream> // for std::basic_ostream
#include <stdint.h>
#include <string>
#include <typeindex> // for std::hash

namespace carb
{

//! Operations for RString (and variant classes) constructor.
enum class RStringOp
{
    //! Attempt to find a matching registered string, or register a new string if not found.
    eRegister,

    //! Only attempt to find a matching registered string. If the string cannot be found, the RString will be empty and
    //! will return `true` to RString::isEmpty().
    eFindExisting,
};

//! Internal definition detail.
namespace detail
{
//! @private
struct RStringBase
{
    //! @private
    CARB_VIZ uint32_t m_stringId : 31;
    //! @private
    unsigned m_uncased : 1;
};

//! @private
// NOTE: In order to satisfy the StandardLayoutType named requirement (required for ABI safety), all non-static data
// members and bit-fields must be declared in the same class. As such, this class must match RStringBase, but cannot
// inherit from RStringBase.
struct RStringKeyBase
{
    //! @private
    CARB_VIZ uint32_t m_stringId : 31;
    //! @private
    unsigned m_uncased : 1;
    //! @private
    CARB_VIZ int32_t m_number;
};

// Validate assumptions
static_assert(std::is_standard_layout<RStringBase>::value, "Not standard layout");
static_assert(std::is_standard_layout<RStringKeyBase>::value, "Not standard layout");
static_assert(offsetof(RStringKeyBase, m_number) == sizeof(RStringBase), "Offset error");

/**
 * The base class for all registered string classes: RString, RStringU, RStringKey, and RStringUKey.
 *
 * @tparam Uncased `true` if representing an "un-cased" (i.e. case-insensitive) registered string; `false` otherwise.
 */
template <bool Uncased, class Base = RStringBase>
class RStringTraits : protected Base
{
public:
    /**
     * Constant that indicates whether this is "un-cased" (i.e. case-insensitive).
     */
    static constexpr bool IsUncased = Uncased;

    //! @private
    constexpr RStringTraits() noexcept;

    //! @private
    constexpr RStringTraits(eRString staticString) noexcept;

    //! @private
    RStringTraits(const char* str, RStringOp op);

    //! @private
    RStringTraits(const char* str, size_t len, RStringOp op);

    //! @private
    RStringTraits(const std::string& str, RStringOp op);

    //! @private
    RStringTraits(uint32_t stringId) noexcept;

    /**
     * Checks to see if this registered string has been corrupted.
     *
     * @note It is not possible for this registered string to become corrupted through normal use of the API. It could
     * be caused by bad casts or use-after-free.
     *
     * @returns `true` if `*this` represents a valid registered string; `false` if `*this` is corrupted.
     */
    bool isValid() const noexcept;

    /**
     * Checks to see if this registered string represents the "" (empty) value.
     *
     * @returns `true` if `*this` is default-initialized or initialized to eRString::Empty; `false` otherwise.
     */
    constexpr bool isEmpty() const noexcept;

    /**
     * Checks to see if this registered string represents an "un-cased" (i.e. case-insensitive) registered string.
     *
     * @returns `true` if `*this` is "un-cased" (i.e. case-insensitive); `false` if case-sensitive.
     */
    constexpr bool isUncased() const noexcept;

    /**
     * Returns the registered string ID. This ID is only useful for debugging purposes and should not be used for
     * comparisons.
     *
     * @returns The string ID for this registered string.
     */
    constexpr uint32_t getStringId() const noexcept;

    /**
     * Returns the hash value as by `carb::hashString(this->c_str())`.
     *
     * @note This value is computed once for a registered string and cached, so this operation is generally very fast.
     *
     * @returns The hash value as computed by `carb::hashString(this->c_str())`.
     */
    size_t getHash() const;

    /**
     * Returns the hash value as by `carb::hashLowercaseString(this->c_str())`.
     *
     * @note This value is pre-computed for registered strings and cached, so this operation is always O(1).
     *
     * @returns The hash value as computed by `carb::hashLowercaseString(this->c_str())`.
     */
    size_t getUncasedHash() const noexcept;

    /**
     * Resolves this registered string to a C-style NUL-terminated string.
     *
     * @note This operation is O(1).
     *
     * @returns The C-style string previously registered.
     */
    const char* c_str() const noexcept;

    /**
     * An alias for c_str(); resolves this registered string to a C-style NUL-terminated string.
     *
     * @note This operation is O(1).
     *
     * @returns The C-style string previously registered.
     */
    const char* data() const noexcept;

    /**
     * Returns the length of the registered string. If the string contains embedded NUL ('\0') characters this may
     * differ from `std::strlen(c_str())`.
     *
     * @note This operation is O(1).
     *
     * @returns The length of the registered string not including the NUL terminator.
     */
    size_t length() const noexcept;

#ifndef DOXYGEN_BUILD
    /**
     * Resolves this registered string to a `std::string`.
     *
     * @returns A `std::string` containing a copy of the previously registered string.
     */
    std::string toString() const;
#endif

    /**
     * Equality comparison between this registered string and another.
     *
     * @param other Another registered string.
     * @returns `true` if `*this` and `other` represent the same registered string; `false` otherwise.
     */
    bool operator==(const RStringTraits& other) const;

    /**
     * Inequality comparison between this registered string and another.
     *
     * @param other Another registered string.
     * @returns `false` if `*this` and `other` represent the same registered string; `true` otherwise.
     */
    bool operator!=(const RStringTraits& other) const;

    /**
     * Checks whether this registered string is stably (but not lexicographically) ordered before another registered
     * string.
     *
     * This ordering is to make registered strings usable as keys in ordered associative containers in O(1) time.
     *
     * @note This is NOT a lexicographical comparison; for that use one of the compare() functions. To reduce ambiguity
     * between a strict ordering and lexicographical comparison there is no `operator<` function for this string class.
     * While a lexicographical comparison would be O(n), this comparison is O(1).
     *
     * @param other Another registered string.
     * @returns `true` if `*this` should be ordered-before @p other; `false` otherwise.
     */
    bool owner_before(const RStringTraits& other) const;

    /**
     * Lexicographically compares this registered string with another.
     *
     * @note If either `*this` or @p other is "un-cased" (i.e. case-insensitive), a case-insensitive compare is
     * performed.
     *
     * @tparam OtherUncased `true` if @p other is "un-cased" (i.e. case-insensitive); `false` otherwise.
     * @param other Another registered string to compare against.
     * @returns `0` if the strings are equal, `>0` if @p other is lexicographically ordered before `*this`, or `<0` if
     * `*this` is lexicographically ordered before @p other. See note above regarding case-sensitivity.
     */
    template <bool OtherUncased, class OtherBase>
    int compare(const RStringTraits<OtherUncased, OtherBase>& other) const;

    /**
     * Lexicographically compares this registered string with a C-style string.
     *
     * @note If `*this` is "un-cased" (i.e. case-insensitive), a case-insensitive compare is performed.
     *
     * @param s A C-style string to compare against.
     * @returns `0` if the strings are equal, `>0` if @p s is lexicographically ordered before `*this`, or `<0` if
     * `*this` is lexicographically ordered before @p s. See note above regarding case-sensitivity.
     */
    int compare(const char* s) const;

    /**
     * Lexicographically compares a substring of this registered string with a C-style string.
     *
     * @note If `*this` is "un-cased" (i.e. case-insensitive), a case-insensitive compare is performed.
     *
     * @param pos The starting offset of the registered string represented by `*this`. Must less-than-or-equal-to the
     * length of the registered string.
     * @param count The length from @p pos to use in the comparison. This value is automatically clamped to the end of
     * the registered string.
     * @param s A C-style string to compare against.
     * @returns `0` if the strings are equal, `>0` if @p s is lexicographically ordered before the substring of `*this`,
     * or `<0` if the substring of `*this` is lexicographically ordered before @p s. See note above regarding
     * case-sensitivity.
     */
    int compare(size_t pos, size_t count, const char* s) const;

    /**
     * Lexicographically compares a substring of this registered string with a C-style string.
     *
     * @note If `*this` is "un-cased" (i.e. case-insensitive), a case-insensitive compare is performed.
     *
     * @param pos The starting offset of the registered string represented by `*this`. Must less-than-or-equal-to the
     * length of the registered string.
     * @param count The length from @p pos to use in the comparison. This value is automatically clamped to the end of
     * the registered string.
     * @param s A C-style string to compare against.
     * @param len The number of characters of @p s to compare against.
     * @returns `0` if the strings are equal, `>0` if @p s is lexicographically ordered before the substring of `*this`,
     * or `<0` if the substring of `*this` is lexicographically ordered before @p s. See note above regarding
     * case-sensitivity.
     */
    int compare(size_t pos, size_t count, const char* s, size_t len) const;

    /**
     * Lexicographically compares this registered string with a string.
     *
     * @note If `*this` is "un-cased" (i.e. case-insensitive), a case-insensitive compare is performed.
     *
     * @param s A string to compare against.
     * @returns `0` if the strings are equal, `>0` if @p s is lexicographically ordered before `*this`, or `<0` if
     * `*this` is lexicographically ordered before @p s. See note above regarding case-sensitivity.
     */
    int compare(const std::string& s) const;

    /**
     * Lexicographically compares a substring of this registered string with a string.
     *
     * @note If `*this` is "un-cased" (i.e. case-insensitive), a case-insensitive compare is performed.
     *
     * @param pos The starting offset of the registered string represented by `*this`. Must less-than-or-equal-to the
     * length of the registered string.
     * @param count The length from @p pos to use in the comparison. This value is automatically clamped to the end of
     * the registered string.
     * @param s A string to compare against.
     * @returns `0` if the strings are equal, `>0` if @p s is lexicographically ordered before the substring of `*this`,
     * or `<0` if the substring of `*this` is lexicographically ordered before @p s. See note above regarding
     * case-sensitivity.
     */
    int compare(size_t pos, size_t count, const std::string& s) const;
};
} // namespace detail

class RString;
class RStringU;
class RStringKey;
class RStringUKey;

/**
 * Carbonite registered strings.
 *
 * The Carbonite framework has a rich <a href="https://en.wikipedia.org/wiki/String_interning">string-interning</a>
 * interface that is very easily used through the RString (and other) classes. This implements a <a
 * href="https://en.wikipedia.org/wiki/Flyweight_pattern">Flyweight pattern</a> for strings. The registered string
 * interface is fully @rstref{ABI-safe <abi-compatibility>} due to versioning, and can even be used in an application
 * prior to the `main()`, `WinMain()` or `DllMain()` functions being called. Furthermore, the API is fully thread-safe.
 *
 * Registered strings have pre-computed hashes which make them ideal for identifiers and map keys, and string
 * (in-)equality checks are O(1) constant time. For ordered containers, registered strings have an `owner_before()`
 * function that can be used for stable (though not lexicographical) ordering. If lexicographical ordering is desired,
 * O(n) `compare()` functions are provided.
 *
 * Variations exist around case-sensitivity. The RStringU class (the U stands for "un-cased" which is used in this API
 * to denote case-insensitivity) is used to register a string that will compare in a case-insensitive manner. Although
 * RString and RStringU cannot be directly compared for equality, RString::toUncased() exists to explicitly create a
 * case-insensitive RStringU from an RString which can then be compared.
 *
 * Variations also exist around using registered strings as a key value. It can be useful to have an associated number
 * to denote multiple instances of a registered string: hence the RStringKey and RStringUKey classes.
 *
 * To register a string, pass a string to the RString constructor RAII-style. Strings that are registered stay as such
 * for the entire run of the application; strings are never unregistered. Registered strings are stored in a named
 * section of shared memory accessible by all modules loaded by an application. The memory for registered strings is
 * allocated directly from the operating system to avoid cross-DLL heap issues.
 *
 * @note Registered strings are a limited resource, but there exists slots for approximately two million strings.
 *
 * Variations:
 * * RStringU - an "un-cased" (i.e. case-insensitive) registered string
 * * RStringKey - Adds a numeric component to RString to create an identifier or key.
 * * RStringUKey - Adds a numeric component to RStringU to create an identifier or key that is case-insensitive.
 */
class CARB_VIZ RString final : public detail::RStringTraits<false>
{
    using Base = detail::RStringTraits<false>;

public:
    /**
     * Constant that indicates whether this is "un-cased" (i.e. case-insensitive) (will always be `false`).
     */
    using Base::IsUncased;

    /**
     * Default constructor. isEmpty() will report `true`.
     */
    constexpr RString() noexcept;

    /**
     * Initializes this registered string to one of the static pre-defined registered strings.
     * @param staticString The pre-defined registered string to use.
     */
    constexpr RString(eRString staticString) noexcept;

    /**
     * Finds or registers a new string.
     * @param str The string to find or register.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    explicit RString(const char* str, RStringOp op = RStringOp::eRegister);

    /**
     * Finds or registers a new counted string.
     * @note While generally not recommended, passing @p len allows the given string to contain embedded NUL ('\0')
     * characters.
     * @param str The string to find or register.
     * @param len The number of characters of @p str to include.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    explicit RString(const char* str, size_t len, RStringOp op = RStringOp::eRegister);

    /**
     * Finds or registers a new `std::string`.
     * @note If @p str contains embedded NUL ('\0') characters, the RString will contain the embedded NUL characters as
     * well.
     * @param str The `std::string` to find or register.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    explicit RString(const std::string& str, RStringOp op = RStringOp::eRegister);

    /**
     * Truncates RStringKey into only the registered string portion.
     * @param other The RStringKey to truncate.
     */
    explicit RString(const RStringKey& other) noexcept;

    /**
     * Converts this registered string into an "un-cased" (i.e. case-insensitive) registered string.
     *
     * @note The returned string may differ in case to `*this` when retrieved with c_str() or toString().
     *
     * @returns An "un-cased" (i.e. case-insensitive) string that matches `*this` when compared in a case-insensitive
     * manner.
     */
    RStringU toUncased() const noexcept;

    /**
     * Returns a copy of this registered string.
     * @note This function exists for compatibility with the RStringKey interface.
     * @returns `*this` since this string already has no number component.
     */
    RString truncate() const noexcept;

    /**
     * Appends a number to the registered string to form a RStringKey.
     *
     * @param number An optional number to append (default = `0`).
     * @returns An RStringKey based on `*this` and the provided number.
     */
    RStringKey toRStringKey(int32_t number = 0) const;

    /**
     * Equality comparison between this registered string and another.
     *
     * @param other Another registered string.
     * @returns `true` if `*this` and `other` represent the same registered string; `false` otherwise.
     */
    bool operator==(const RString& other) const noexcept;

    /**
     * Inequality comparison between this registered string and another.
     *
     * @param other Another registered string.
     * @returns `false` if `*this` and `other` represent the same registered string; `true` otherwise.
     */
    bool operator!=(const RString& other) const noexcept;

    /**
     * Checks whether this registered string is stably (but not lexicographically) ordered before another registered
     * string.
     *
     * This ordering is to make registered strings usable as keys in ordered associative containers in O(1) time.
     *
     * @note This is NOT a lexicographical comparison; for that use one of the compare() functions. To reduce ambiguity
     * between a strict ordering and lexicographical comparison there is no `operator<` function for this string class.
     * While a lexicographical comparison would be O(n), this comparison is O(1).
     *
     * @param other Another registered string.
     * @returns `true` if `*this` should be ordered-before @p other; `false` otherwise.
     */
    bool owner_before(const RString& other) const noexcept;
};

/**
 * Case-insensitive registered string.
 *
 * The "U" stands for "un-cased".
 *
 * See RString for system-level information. This class differs from RString in that it performs case-insensitive
 * operations.
 *
 * Since the desire is for equality comparisons to be speed-of-light (i.e. O(1) numeric comparisons), the first string
 * registered insensitive to casing is chosen as an "un-cased authority" and if any strings registered through RStringU
 * later match that string (in a case-insensitive manner), that authority string will be chosen instead. This also means
 * that when RStringU is used to register a string and then that string is retrieved with RStringU::c_str(), the casing
 * in the returned string might not match what was registered.
 */
class CARB_VIZ RStringU final : public detail::RStringTraits<true>
{
    using Base = detail::RStringTraits<true>;

public:
    /**
     * Constant that indicates whether this is "un-cased" (i.e. case-insensitive) (will always be `true`).
     */
    using Base::IsUncased;

    /**
     * Default constructor. isEmpty() will report `true`.
     */
    constexpr RStringU() noexcept;

    /**
     * Initializes this registered string to one of the static pre-defined registered strings.
     * @param staticString The pre-defined registered string to use.
     */
    constexpr RStringU(eRString staticString) noexcept;

    /**
     * Finds or registers a new case-insensitive string.
     *
     * @note The casing of the string actually used may be different than @p str when reported by c_str() or toString().
     *
     * @param str The string to find or register.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    explicit RStringU(const char* str, RStringOp op = RStringOp::eRegister);

    /**
     * Finds or registers a new counted case-insensitive string.
     * @note While generally not recommended, passing @p len allows the given string to contain embedded NUL ('\0')
     * characters.
     * @note The casing of the string actually used may be different than @p str when reported by c_str() or toString().
     * @param str The string to find or register.
     * @param len The number of characters of @p str to include.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    explicit RStringU(const char* str, size_t len, RStringOp op = RStringOp::eRegister);

    /**
     * Finds or registers a new case-insensitive `std::string`.
     * @note If @p str contains embedded NUL ('\0') characters, the RString will contain the embedded NUL characters as
     * well.
     * @note The casing of the string actually used may be different than @p str when reported by c_str() or toString().
     * @param str The `std::string` to find or register.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    explicit RStringU(const std::string& str, RStringOp op = RStringOp::eRegister);

    /**
     * Converts a registered string into an "un-cased" (i.e. case-insensitive) registered string.
     * @param other The RString to convert.
     */
    explicit RStringU(const RString& other);

    /**
     * Truncates RStringUKey into only the registered string portion.
     * @param other The RStringUKey to truncate.
     */
    explicit RStringU(const RStringUKey& other);

    /**
     * Returns a copy of this registered string.
     * @note This function exists for compatibility with the RString interface.
     * @returns `*this` since this string is already "un-cased" (i.e. case-insensitive).
     */
    RStringU toUncased() const noexcept;

    /**
     * Returns a copy of this registered string.
     * @note This function exists for compatibility with the RStringKey interface.
     * @returns `*this` since this string already has no number component.
     */
    RStringU truncate() const noexcept;

    /**
     * Appends a number to the registered string to form a RStringUKey.
     *
     * @param number An optional number to append (default = `0`).
     * @returns An RStringUKey based on `*this` and the provided number.
     */
    RStringUKey toRStringKey(int32_t number = 0) const;

    /**
     * Equality comparison between this registered string and another.
     *
     * @note A case-insensitive compare is performed.
     *
     * @param other Another registered string.
     * @returns `true` if `*this` and `other` represent the same registered string; `false` otherwise.
     */
    bool operator==(const RStringU& other) const noexcept;

    /**
     * Inequality comparison between this registered string and another.
     *
     * @note A case-insensitive compare is performed.
     *
     * @param other Another registered string.
     * @returns `false` if `*this` and `other` represent the same registered string; `true` otherwise.
     */
    bool operator!=(const RStringU& other) const noexcept;

    /**
     * Checks whether this registered string is stably (but not lexicographically) ordered before another registered
     * string.
     *
     * This ordering is to make registered strings usable as keys in ordered associative containers in O(1) time.
     *
     * @note This is NOT a lexicographical comparison; for that use one of the compare() functions. To reduce ambiguity
     * between a strict ordering and lexicographical comparison there is no `operator<` function for this string class.
     * While a lexicographical comparison would be O(n), this comparison is O(1).
     *
     * @param other Another registered string.
     * @returns `true` if `*this` should be ordered-before @p other; `false` otherwise.
     */
    bool owner_before(const RStringU& other) const noexcept;
};

/**
 * A registered string key.
 *
 * See \ref RString for high-level information about the registered string system.
 *
 * RStringKey is formed by appending a numeric component to a registered string. This numeric component can be used as a
 * unique instance identifier alongside the registered string. Additionally, the RStringKey::toString() function will
 * append a non-zero numeric component following an underscore.
 */
class CARB_VIZ RStringKey final : public detail::RStringTraits<false, detail::RStringKeyBase>
{
    using Base = detail::RStringTraits<false, detail::RStringKeyBase>;

public:
    /**
     * Constant that indicates whether this is "un-cased" (i.e. case-insensitive) (will always be `false`).
     */
    using Base::IsUncased;

    /**
     * Default constructor. isEmpty() will report `true` and getNumber() will return `0`.
     */
    constexpr RStringKey() noexcept;

    /**
     * Initializes this registered string to one of the static pre-defined registered strings.
     * @param staticString The pre-defined registered string to use.
     * @param number The number that will be returned by getNumber().
     */
    constexpr RStringKey(eRString staticString, int32_t number = 0) noexcept;

    /**
     * Finds or registers a new string.
     * @param str The string to find or register.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    explicit RStringKey(const char* str, RStringOp op = RStringOp::eRegister);

    /**
     * Finds or registers a new string with a given number component.
     * @param number The number that will be returned by getNumber().
     * @param str The string to find or register.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    RStringKey(int32_t number, const char* str, RStringOp op = RStringOp::eRegister);

    /**
     * Finds or registers a new counted string.
     * @note While generally not recommended, passing @p len allows the given string to contain embedded NUL ('\0')
     * characters.
     * @param str The string to find or register.
     * @param len The number of characters of @p str to include.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    explicit RStringKey(const char* str, size_t len, RStringOp op = RStringOp::eRegister);

    /**
     * Finds or registers a new counted string with a given number component.
     * @note While generally not recommended, passing @p len allows the given string to contain embedded NUL ('\0')
     * characters.
     * @param number The number that will be returned by getNumber().
     * @param str The string to find or register.
     * @param len The number of characters of @p str to include.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    explicit RStringKey(int32_t number, const char* str, size_t len, RStringOp op = RStringOp::eRegister);

    /**
     * Finds or registers a new `std::string`.
     * @note If @p str contains embedded NUL ('\0') characters, the RString will contain the embedded NUL characters as
     * well.
     * @param str The `std::string` to find or register.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    explicit RStringKey(const std::string& str, RStringOp op = RStringOp::eRegister);

    /**
     * Finds or registers a new `std::string` with a number component.
     * @note If @p str contains embedded NUL ('\0') characters, the RString will contain the embedded NUL characters as
     * well.
     * @param number The number that will be returned by getNumber().
     * @param str The `std::string` to find or register.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    explicit RStringKey(int32_t number, const std::string& str, RStringOp op = RStringOp::eRegister);

    /**
     * Appends a number component to a registered string to form a key.
     * @param str The registered string to decorate.
     * @param number The number that will be returned by getNumber().
     */
    RStringKey(const RString& str, int32_t number = 0);

    /**
     * Converts this registered string key into an "un-cased" (i.e. case-insensitive) registered string key.
     *
     * @note The returned string may differ in case to `*this` when retrieved with c_str() or toString().
     *
     * @returns An "un-cased" (i.e. case-insensitive) string that matches `*this` when compared in a case-insensitive
     * manner. The returned registered string key will have the same number component as `*this`.
     */
    RStringUKey toUncased() const noexcept;

    /**
     * Returns a registered string without the number component.
     * @returns A registered string that matches `*this` without a number component.
     */
    RString truncate() const noexcept;

    /**
     * Equality comparison between this registered string key and another.
     *
     * @param other Another registered string.
     * @returns `true` if `*this` and `other` represent the same registered string and have matching number components;
     * `false` otherwise.
     */
    bool operator==(const RStringKey& other) const noexcept;

    /**
     * Inequality comparison between this registered string key and another.
     *
     * @param other Another registered string.
     * @returns `false` if `*this` and `other` represent the same registered string and have matching number components;
     * `true` otherwise.
     */
    bool operator!=(const RStringKey& other) const noexcept;

    /**
     * Checks whether this registered string key is stably (but not lexicographically) ordered before another registered
     * string. The number component is also compared and keys with a lower number component will be ordered before.
     *
     * This ordering is to make registered strings usable as keys in ordered associative containers in O(1) time.
     *
     * @note This is NOT a lexicographical comparison; for that use one of the compare() functions. To reduce ambiguity
     * between a strict ordering and lexicographical comparison there is no `operator<` function for this string class.
     * While a lexicographical comparison would be O(n), this comparison is O(1).
     *
     * @param other Another registered string.
     * @returns `true` if `*this` should be ordered-before @p other; `false` otherwise.
     */
    bool owner_before(const RStringKey& other) const noexcept;

#ifndef DOXYGEN_BUILD // Sphinx warns about Duplicate C++ declaration
    /**
     * Returns the hash value as by `carb::hashString(this->truncate().c_str())` combined with the number component.
     *
     * @note This value is computed once for a registered string and cached, so this operation is generally very fast.
     *
     * @returns The hash value as computed by `carb::hashString(this->truncate().c_str())`.
     */
    size_t getHash() const;

    /**
     * Returns the hash value as by `carb::hashLowercaseString(this->truncate().c_str())` combined with the number
     * component.
     *
     * @note This value is pre-computed for registered strings and cached, so this operation is always O(1).
     *
     * @returns The hash value as computed by `carb::hashLowercaseString(this->truncate().c_str())`.
     */
    size_t getUncasedHash() const noexcept;
#endif

    /**
     * Returns a string containing the registered string, and if getNumber() is not zero, the number appended.
     *
     * Example: RStringKey(eRString::RS_carb, 1).toString() would produce "carb_1".
     * @returns A string containing the registered string. If getNumber() is non-zero, an underscore and the number are
     * appended.
     */
    std::string toString() const;

    /**
     * Returns the number component of this key.
     * @returns The number component previously specified in the constructor or with setNumber() or via number().
     */
    int32_t getNumber() const noexcept;

    /**
     * Sets the number component of this key.
     * @param num The new number component.
     */
    void setNumber(int32_t num) noexcept;

    /**
     * Direct access to the number component for manipulation or atomic operations via `atomic_ref`.
     * @returns A reference to the number component.
     */
    int32_t& number() noexcept;

private:
    // Hide these functions since they are incomplete
    using Base::c_str;
    using Base::data;
    using Base::length;
};

/**
 * A case-insensitive registered string key.
 *
 * See \ref RString for high-level information about the registered string system.
 *
 * RStringUKey is formed by appending a numeric component to an "un-cased" (i.e. case-insensitive) registered string.
 * This numeric component can be used as a unique instance identifier alongside the registered string. Additionally, the
 * RStringUKey::toString() function will append a non-zero numeric component following an underscore.
 */
class CARB_VIZ RStringUKey final : public detail::RStringTraits<true, detail::RStringKeyBase>
{
    using Base = detail::RStringTraits<true, detail::RStringKeyBase>;

public:
    /**
     * Constant that indicates whether this is "un-cased" (i.e. case-insensitive) (will always be `true`).
     */
    using Base::IsUncased;

    /**
     * Default constructor. isEmpty() will report `true` and getNumber() will return `0`.
     */
    constexpr RStringUKey() noexcept;

    /**
     * Initializes this registered string to one of the static pre-defined registered strings.
     * @param staticString The pre-defined registered string to use.
     * @param number The number that will be returned by getNumber().
     */
    constexpr RStringUKey(eRString staticString, int32_t number = 0) noexcept;

    /**
     * Finds or registers a new case-insensitive string.
     *
     * @note The casing of the string actually used may be different than @p str when reported by c_str() or toString().
     *
     * @param str The string to find or register.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    RStringUKey(const char* str, RStringOp op = RStringOp::eRegister);

    /**
     * Finds or registers a new string with a given number component.
     * @param number The number that will be returned by getNumber().
     * @param str The string to find or register.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    RStringUKey(int32_t number, const char* str, RStringOp op = RStringOp::eRegister);

    /**
     * Finds or registers a new counted case-insensitive string.
     * @note While generally not recommended, passing @p len allows the given string to contain embedded NUL ('\0')
     * characters.
     * @note The casing of the string actually used may be different than @p str when reported by c_str() or toString().
     * @param str The string to find or register.
     * @param len The number of characters of @p str to include.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    explicit RStringUKey(const char* str, size_t len, RStringOp op = RStringOp::eRegister);

    /**
     * Finds or registers a new counted case-insensitive string with a given number component.
     * @note While generally not recommended, passing @p len allows the given string to contain embedded NUL ('\0')
     * characters.
     * @note The casing of the string actually used may be different than @p str when reported by c_str() or toString().
     * @param number The number that will be returned by getNumber().
     * @param str The string to find or register.
     * @param len The number of characters of @p str to include.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    explicit RStringUKey(int32_t number, const char* str, size_t len, RStringOp op = RStringOp::eRegister);

    /**
     * Finds or registers a new case-insensitive `std::string`.
     * @note If @p str contains embedded NUL ('\0') characters, the RString will contain the embedded NUL characters as
     * well.
     * @note The casing of the string actually used may be different than @p str when reported by c_str() or toString().
     * @param str The `std::string` to find or register.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    explicit RStringUKey(const std::string& str, RStringOp op = RStringOp::eRegister);

    /**
     * Finds or registers a new case-insensitive `std::string` with a number component.
     * @note If @p str contains embedded NUL ('\0') characters, the RString will contain the embedded NUL characters as
     * well.
     * @note The casing of the string actually used may be different than @p str when reported by c_str() or toString().
     * @param number The number that will be returned by getNumber().
     * @param str The `std::string` to find or register.
     * @param op The operation to perform. If directed to RStringOp::eFindExisting and the string has not been
     * previously registered, `*this` is initialized as if with the default constructor.
     */
    explicit RStringUKey(int32_t number, const std::string& str, RStringOp op = RStringOp::eRegister);

    /**
     * Appends a number component to a registered string to form a key.
     * @param str The registered string to decorate.
     * @param number The number that will be returned by getNumber().
     */
    RStringUKey(const RStringU& str, int32_t number = 0);

    /**
     * Converts a registered string key into an "un-cased" (i.e. case-insensitive) registered string key.
     * @param other The RStringKey to convert. The number component is maintained.
     */
    explicit RStringUKey(const RStringKey& other);

    /**
     * Returns a copy of this registered string key.
     * @note This function exists for compatibility with the RStringKey interface.
     * @returns `*this` since this string is already "un-cased" (i.e. case-insensitive). The number component will be
     * the same as the number for `*this`.
     */
    RStringUKey toUncased() const noexcept;

    /**
     * Returns a registered string without the number component.
     * @returns A registered string that matches `*this` without a number component.
     */
    RStringU truncate() const noexcept;

    /**
     * Equality comparison between this registered string key and another.
     *
     * @note A case-insensitive compare is performed.
     *
     * @param other Another registered string.
     * @returns `true` if `*this` and `other` represent the same registered string and have matching number components;
     * `false` otherwise.
     */
    bool operator==(const RStringUKey& other) const noexcept;

    /**
     * Inequality comparison between this registered string key and another.
     *
     * @note A case-insensitive compare is performed.
     *
     * @param other Another registered string.
     * @returns `false` if `*this` and `other` represent the same registered string and have matching number components;
     * `true` otherwise.
     */
    bool operator!=(const RStringUKey& other) const noexcept;

    /**
     * Checks whether this registered string key is stably (but not lexicographically) ordered before another registered
     * string. The number component is also compared and keys with a lower number component will be ordered before.
     *
     * This ordering is to make registered strings usable as keys in ordered associative containers in O(1) time.
     *
     * @note This is NOT a lexicographical comparison; for that use one of the compare() functions. To reduce ambiguity
     * between a strict ordering and lexicographical comparison there is no `operator<` function for this string class.
     * While a lexicographical comparison would be O(n), this comparison is O(1).
     *
     * @param other Another registered string.
     * @returns `true` if `*this` should be ordered-before @p other; `false` otherwise.
     */
    bool owner_before(const RStringUKey& other) const noexcept;

#ifndef DOXYGEN_BUILD // Sphinx warns about Duplicate C++ declaration
    /**
     * Returns the hash value as by `carb::hashString(this->truncate().c_str())` combined with the number component.
     *
     * @note This value is computed once for a registered string and cached, so this operation is generally very fast.
     *
     * @returns The hash value as computed by `carb::hashString(this->truncate().c_str())`.
     */
    size_t getHash() const;

    /**
     * Returns the hash value as by `carb::hashLowercaseString(this->truncate().c_str())` combined with the number
     * component.
     *
     * @note This value is pre-computed for registered strings and cached, so this operation is always O(1).
     *
     * @returns The hash value as computed by `carb::hashLowercaseString(this->truncate().c_str())`.
     */
    size_t getUncasedHash() const noexcept;
#endif

    /**
     * Returns a string containing the registered string, and if getNumber() is not zero, the number appended.
     *
     * Example: RStringUKey(eRString::RS_carb, 1).toString() would produce "carb_1".
     * @returns A string containing the registered string. If getNumber() is non-zero, an underscore and the number are
     * appended.
     */
    std::string toString() const;

    /**
     * Returns the number component of this key.
     * @returns The number component previously specified in the constructor or with setNumber() or via number().
     */
    int32_t getNumber() const noexcept;

    /**
     * Sets the number component of this key.
     * @param num The new number component.
     */
    void setNumber(int32_t num) noexcept;

    /**
     * Direct access to the number component for manipulation or atomic operations via `atomic_ref`.
     * @returns A reference to the number component.
     */
    int32_t& number() noexcept;

private:
    // Hide these functions since they are incomplete
    using Base::c_str;
    using Base::data;
    using Base::length;
};

// Can use ADL specialization for global operator<< for stream output

/**
 * Global stream output operator for RString.
 * @param o The output stream to write to.
 * @param s The registered string to output.
 * @returns The output stream, @p o.
 */
template <class CharT, class Traits>
::std::basic_ostream<CharT, Traits>& operator<<(::std::basic_ostream<CharT, Traits>& o, const RString& s)
{
    o << s.c_str();
    return o;
}

/**
 * Global stream output operator for RStringU.
 * @param o The output stream to write to.
 * @param s The registered string to output.
 * @returns The output stream, @p o.
 */
template <class CharT, class Traits>
::std::basic_ostream<CharT, Traits>& operator<<(::std::basic_ostream<CharT, Traits>& o, const RStringU& s)
{
    o << s.c_str();
    return o;
}

/**
 * Global stream output operator for RStringKey.
 * @param o The output stream to write to.
 * @param s The registered string to output.
 * @returns The output stream, @p o.
 */
template <class CharT, class Traits>
::std::basic_ostream<CharT, Traits>& operator<<(::std::basic_ostream<CharT, Traits>& o, const RStringKey& s)
{
    o << s.toString();
    return o;
}

/**
 * Global stream output operator for RStringUKey.
 * @param o The output stream to write to.
 * @param s The registered string to output.
 * @returns The output stream, @p o.
 */
template <class CharT, class Traits>
::std::basic_ostream<CharT, Traits>& operator<<(::std::basic_ostream<CharT, Traits>& o, const RStringUKey& s)
{
    o << s.toString();
    return o;
}

} // namespace carb

// Specializations for std::hash and std::owner_less per type

/**
 * RString specialization for `std::hash`.
 */
template <>
struct std::hash<::carb::RString>
{
    /**
     * Returns the hash
     * @param v The registered string.
     * @returns The hash as via the getHash() function.
     */
    size_t operator()(const ::carb::RString& v) const
    {
        return v.getHash();
    }
};

/**
 * RString specialization for `std::owner_less`.
 */
template <>
struct std::owner_less<::carb::RString>
{
    /**
     * Returns true if @p lhs should be ordered-before @p rhs.
     * @param lhs A registered string.
     * @param rhs A registered string.
     * @returns `true` if @p lhs should be ordered-before @p rhs; `false` otherwise.
     */
    bool operator()(const ::carb::RString& lhs, const ::carb::RString& rhs) const
    {
        return lhs.owner_before(rhs);
    }
};

/**
 * RStringU specialization for `std::hash`.
 */
template <>
struct std::hash<::carb::RStringU>
{
    /**
     * Returns the hash
     * @param v The registered string.
     * @returns The hash as via the getHash() function.
     */
    size_t operator()(const ::carb::RStringU& v) const
    {
        return v.getHash();
    }
};

/**
 * RStringU specialization for `std::owner_less`.
 */
template <>
struct std::owner_less<::carb::RStringU>
{
    /**
     * Returns true if @p lhs should be ordered-before @p rhs.
     * @param lhs A registered string.
     * @param rhs A registered string.
     * @returns `true` if @p lhs should be ordered-before @p rhs; `false` otherwise.
     */
    bool operator()(const ::carb::RStringU& lhs, const ::carb::RStringU& rhs) const
    {
        return lhs.owner_before(rhs);
    }
};

/**
 * RStringKey specialization for `std::hash`.
 */
template <>
struct std::hash<::carb::RStringKey>
{
    /**
     * Returns the hash
     * @param v The registered string.
     * @returns The hash as via the getHash() function.
     */
    size_t operator()(const ::carb::RStringKey& v) const
    {
        return v.getHash();
    }
};

/**
 * RStringKey specialization for `std::owner_less`.
 */
template <>
struct std::owner_less<::carb::RStringKey>
{
    /**
     * Returns true if @p lhs should be ordered-before @p rhs.
     * @param lhs A registered string.
     * @param rhs A registered string.
     * @returns `true` if @p lhs should be ordered-before @p rhs; `false` otherwise.
     */
    bool operator()(const ::carb::RStringKey& lhs, const ::carb::RStringKey& rhs) const
    {
        return lhs.owner_before(rhs);
    }
};

/**
 * RStringUKey specialization for `std::hash`.
 */
template <>
struct std::hash<::carb::RStringUKey>
{
    /**
     * Returns the hash
     * @param v The registered string.
     * @returns The hash as via the getHash() function.
     */
    size_t operator()(const ::carb::RStringUKey& v) const
    {
        return v.getHash();
    }
};

/**
 * RStringUKey specialization for `std::owner_less`.
 */
template <>
struct std::owner_less<::carb::RStringUKey>
{
    /**
     * Returns true if @p lhs should be ordered-before @p rhs.
     * @param lhs A registered string.
     * @param rhs A registered string.
     * @returns `true` if @p lhs should be ordered-before @p rhs; `false` otherwise.
     */
    bool operator()(const ::carb::RStringUKey& lhs, const ::carb::RStringUKey& rhs) const
    {
        return lhs.owner_before(rhs);
    }
};

#include "RString.inl"
