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
//! @brief ABI safe string implementation
#pragma once

#include "../carb/Defines.h"
#include "detail/PointerIterator.h"
#include "../carb/cpp17/StringView.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <istream>
#include <iterator>
#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>

#if CARB_HAS_CPP17
#    include <string_view>
#endif

CARB_IGNOREWARNING_MSC_WITH_PUSH(4201) // nonstandard extension used: nameless struct/union.

namespace omni
{

//! A flag type to select the omni::string constructor that allows \c printf style formatting.
struct formatted_t
{
};

//! A flag value to select the omni::string constructor that allows \c printf style formatting.
constexpr formatted_t formatted{};

//! A flag type to select the omni::string constructor that allows \c vprintf style formatting.
struct vformatted_t
{
};

//! A flag value to select the omni::string constructor that allows \c vprintf style formatting.
constexpr vformatted_t vformatted{};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail
{

/**
 * This struct provides implementations of a subset of the functions found in std::char_traits. It is used to provide
 * constexpr implementations of the functions because std::char_traits did not become constexpr until C++20. Currently
 * only the methods used by omni::string are provided.
 */
struct char_traits
{
    /**
     * Assigns \p c to \p dest.
     */
    static constexpr void assign(char& dest, const char& c) noexcept;

    /**
     * Assigns \p count copies of \p c to \p dest.
     *
     * @returns \p dest.
     */
    static constexpr char* assign(char* dest, std::size_t count, char c) noexcept;

    /**
     * Copies \p count characters from \p source to \p dest.
     *
     * This function performs correctly even if \p dest and \p source overlap.
     */
    static constexpr void move(char* dest, const char* source, std::size_t count) noexcept;

    /**
     * Copies \p count characters from \p source to \p dest.
     *
     * Behavior of this function is undefined if \p dest and \p source overlap.
     */
    static constexpr void copy(char* dest, const char* source, std::size_t count) noexcept;

    /**
     * Lexicographically compares the first \p count characters of \p s1 and \p s2.
     *
     * @return Negative value if \p s1 is less than \p s2.
     *         ​0​ if \p s1 is equal to \p s2.
     *         Positive value if \p s1 is greater than \p s2.
     */
    static constexpr int compare(const char* s1, const char* s2, std::size_t count) noexcept;

    /**
     * Computes the length of \p s.
     *
     * @return The length of \p s.
     */
    static constexpr std::size_t length(const char* s) noexcept;

    /**
     * Searches the first \p count characters of \p s for the character \p ch.
     *
     * @return A pointer to the first character equal to \p ch, or \c nullptr if no such character exists.
     */
    static constexpr const char* find(const char* s, std::size_t count, char ch) noexcept;
};

#    if CARB_HAS_CPP17
template <typename T, typename Res = void>
using is_sv_convertible =
    std::enable_if_t<std::conjunction<std::is_convertible<const T&, std::string_view>,
                                      std::negation<std::is_convertible<const T&, const char*>>>::value,
                     Res>;
#    endif
} // namespace detail
#endif

// Handle case where Windows.h may have defined 'max'
#pragma push_macro("max")
#undef max

/**
 * This class is an ABI safe string implementation. It is meant to be a drop-in replacement for std::string.
 *
 * This class is not templated for simplicity and @rstref{ABI safety <abi-compatibility>}.
 *
 * Note: If exceptions are not enabled, any function that would throw will terminate the program instead.
 *
 * Note: All functions provide a strong exception guarantee. If they throw an exception for any reason, the function
 *       has no effect.
 */
class CARB_VIZ string
{

public:
/** Char traits type alias. */
#if CARB_HAS_CPP20
    using traits_type = std::char_traits<char>;
#else
    using traits_type = detail::char_traits;
#endif
    /** Char type alias. */
    using value_type = char;

    /** Size type alias. */
    using size_type = std::size_t;
    /** Reference type alias. */
    using reference = value_type&;
    /** Const Reference type alias. */
    using const_reference = const value_type&;
    /** Pointer type alias. */
    using pointer = value_type*;
    /** Const Pointer type alias. */
    using const_pointer = const value_type*;
    /** Difference type alias. */
    using difference_type = std::pointer_traits<pointer>::difference_type;

    /** Iterator type alias. */
    using iterator = detail::PointerIterator<pointer, string>;
    /** Const Iterator type alias. */
    using const_iterator = detail::PointerIterator<const_pointer, string>;
    /** Const Reverse Iterator type alias. */
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    /** Reverse Iterator type alias. */
    using reverse_iterator = std::reverse_iterator<iterator>;

    /**
     * Special value normally used to indicate that an operation failed.
     */
    static constexpr size_type npos = std::numeric_limits<size_type>::max();

    /**
     * Default constructor. Constructs empty string.
     */
    string() noexcept;

    /**
     * Constructs the string with \p n copies of character \p c.
     * @param n Number of characters to initialize with.
     * @param c The character to initialize with.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     *
     */
    string(size_type n, value_type c);

    /**
     * Constructs the string with a substring `[pos, str.size())` of \p str.
     *
     * @param str Another string to use as source to initialize the string with.
     * @param pos Position of the first character to include.
     *
     * @throws std::out_of_range if \p pos is greater than `str.size()`.
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string(const string& str, size_type pos);

    /**
     * Constructs the string with a substring `[pos, pos + n)` of \p str. If `n == npos`, or if the
     * requested substring lasts past the end of the string, the resulting substring is `[pos, str.size())`.
     *
     * @param str Another string to use as source to initialize the string with.
     * @param pos Position of the first character to include.
     * @param n Number of characters to include.
     *
     * @throws std::out_of_range if \p pos is greater than `str.size()`.
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string(const string& str, size_type pos, size_type n);

    /**
     * Constructs the string with the first \p n characters of character string pointed to by \p s.  \p s can contain
     * null characters. The length of the string is \p n. The behavior is undefined if `[s, s + n)` is not
     * a valid range.
     *
     * @param s Pointer to an array of characters to use as source to initialize the string with.
     * @param n Number of characters to include.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string(const value_type* s, size_type n);

    /**
     * Constructs the string with the contents initialized with a copy of the null-terminated character string pointed
     * to by \p s . The length of the string is determined by the first null character. The behavior is undefined if
     * `[s, s + Traits::length(s))` is not a valid range (for example, if \p s is a null pointer).
     *
     * @param s Pointer to an array of characters to use as source to initialize the string with.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string(const value_type* s);

    /**
     * Constructs the string with the contents of the range `[first, last)`.
     *
     * @param begin Start of the range to copy characters from.
     * @param end End of the range to copy characters from.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <typename InputIterator>
    string(InputIterator begin, InputIterator end);

    /**
     * Copy constructor. Constructs the string with a copy of the contents of \p str.
     *
     * @param str Another string to use as source to initialize the string with.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string(const string& str);

    /**
     * Move constructor. Constructs the string with the contents of \p str using move semantics. \p str is left in
     * valid, but unspecified state.
     *
     * @param str Another string to use as source to initialize the string with.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string(string&& str) noexcept;

    /**
     * Constructs the string with the contents of the initializer list \p ilist.
     *
     * @param ilist initializer_list to initialize the string with.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string(std::initializer_list<value_type> ilist);

    /**
     * Constructs the string with the \c printf style format string and additional parameters.
     *
     * @param fmt A \c printf style format string.
     * @param args Additional arguments to the format string.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <class... Args>
    string(formatted_t, const char* fmt, Args&&... args);

    /**
     * Constructs the string with the \c vprintf style format string and additional parameters.
     *
     * @param fmt A \c printf style format string.
     * @param ap A \c va_list as initialized by \c va_start.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string(vformatted_t, const char* fmt, va_list ap);

    /**
     * Copy constructor. Constructs the string with a copy of the contents of \p str.
     *
     * @param str Another string to use as source to initialize the string with.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    explicit string(const std::string& str);

    /**
     * Constructs the string with a substring `[pos, pos + n)` of \p str. If `n == npos`, or if the
     * requested substring lasts past the end of the string, the resulting substring is `[pos, str.size())`.
     *
     * @param str Another string to use as source to initialize the string with.
     * @param pos Position of the first character to include.
     * @param n Number of characters to include.
     *
     * @throws std::out_of_range if \p pos is greater than `str.size()`.
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string(const std::string& str, size_type pos, size_type n);

    /**
     * Copy constructor. Constructs the string with a copy of the contents of \p sv.
     *
     * @param sv String view to use as source to initialize the string with.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    explicit string(const carb::cpp17::string_view& sv);

    /**
     * Constructs the string with a substring `[pos, pos + n)` of \p sv. If `n == npos`, or if the
     * requested substring lasts past the end of the string, the resulting substring is `[pos, sv.size())`.
     *
     * @param sv String view to use as source to initialize the string with.
     * @param pos Position of the first character to include.
     * @param n Number of characters to include.
     *
     * @throws std::out_of_range if \p pos is greater than `sv.size()`.
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string(const carb::cpp17::string_view& sv, size_type pos, size_type n);

#if CARB_HAS_CPP17
    /**
     * Implicitly converts @p t to type `std::string_view` and initializes this string with the contents of that
     * `std::string_view`. This overload participates in overload resolution only if `std::is_convertible_v<const T&,
     * std::string_view>` is true.
     *
     * @param t Object that can be converted to `std::string_view` to initialize with.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    explicit string(const T& t);

    /**
     * Implicitly converts @p t to type `std::string_view` and initializes this string with a substring `[pos, pos + n)`
     * of that `string_view`. If `n == npos`, or if the requested substring lasts past the end of the `string_view`,
     * the resulting substring is `[pos, sv.size())`. This overload participates in overload resolution only if
     * `std::is_convertible_v<const T&, std::string_view>` is true.
     *
     * @param t Object that can be converted to `std::string_view` to initialize with.
     * @param pos Position of the first character to include.
     * @param n Number of characters to include.
     *
     * @throws std::out_of_range if \p pos is greater than `sv.size()`.
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    string(const T& t, size_type pos, size_type n);
#endif

    /**
     * Cannot be constructed from `nullptr`.
     */
    string(std::nullptr_t) = delete;

    /**
     * Destroys the string, deallocating internal storage if used.
     */
    ~string() noexcept;

    /**
     * Replaces the contents with a copy of \p str. If \c *this and str are the same object, this function has no
     * effect.
     *
     * @param str String to be used as the source to initialize the string with.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& operator=(const string& str);

    /**
     * Replaces the contents with those of str using move semantics. str is in a valid but unspecified state afterwards.
     *
     * @param str String to be used as the source to initialize the string with.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& operator=(string&& str) noexcept;

    /**
     * Replaces the contents with those of null-terminated character string pointed to by \p s.
     *
     * @param s Pointer to a null-terminated character string to use as source to initialize the string with.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& operator=(const value_type* s);

    /**
     * Replaces the contents with character \p c.
     *
     * @param c Character to use as source to initialize the string with.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& operator=(value_type c);

    /**
     * Replaces the contents with those of the initializer list \p ilist.
     *
     * @param ilist initializer list to use as source to initialize the string with.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& operator=(std::initializer_list<value_type> ilist);

    /**
     * Replaces the contents with a copy of \p str.
     *
     * @param str String to be used as the source to initialize the string with.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& operator=(const std::string& str);

    /**
     * Replaces the contents with a copy of \p sv.
     *
     * @param sv String view to be used as the source to initialize the string with.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& operator=(const carb::cpp17::string_view& sv);

#if CARB_HAS_CPP17
    /**
     * Implicitly converts @p t to type `std::string_view` and replaces the contents of this string with the contents of
     * that `std::string_view`. This overload participates in overload resolution only if `std::is_convertible_v<const
     * T&, std::string_view>` is true.
     *
     * @param t Object that can be converted to `std::string_view` to initialize with.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    string& operator=(const T& t);
#endif

    /**
     * Cannot be assigned from `nullptr`.
     */
    string& operator=(std::nullptr_t) = delete;

    /**
     * Replaces the contents with \p n copies of character \p c.
     *
     * @param n Number of characters to initialize with.
     * @param c Character to use as source to initialize the string with.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& assign(size_type n, value_type c);

    /**
     * Replaces the contents with a copy of \p str.
     *
     * @param str String to be used as the source to initialize the string with.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& assign(const string& str);

    /**
     * Replaces the string with a substring `[pos, pos + n)` of \p str. If `n == npos`, or if the
     * requested substring lasts past the end of the string, the resulting substring is `[pos, str.size())`.
     *
     * @param str Another string to use as source to initialize the string with.
     * @param pos Position of the first character to include.
     * @param n Number of characters to include.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& assign(const string& str, size_type pos, size_type n = npos);

    /**
     * Replaces the contents with those of str using move semantics. \p str is in a valid but unspecified state
     * afterwards.
     *
     * @param str String to be used as the source to initialize the string with.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& assign(string&& str);

    /**
     * Replace the string with the first \p n characters of character string pointed to by \p s.  \p s can contain
     * null characters. The length of the string is \p n. The behavior is undefined if `[s, s + n)` is not
     * a valid range.
     *
     * @param s Pointer to an array of characters to use as source to initialize the string with.
     * @param n Number of characters to include.
     * @return \c *this.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& assign(const value_type* s, size_type n);

    /**
     * Replaces the contents with those of null-terminated character string pointed to by \p s.
     *
     * @param s Pointer to a null-terminated character string to use as source to initialize the string with.
     * @return \c *this.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& assign(const value_type* s);

    /**
     * Replace the string with the contents of the range `[first, last)`.
     *
     * @param first Start of the range to copy characters from.
     * @param last End of the range to copy characters from.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <class InputIterator>
    string& assign(InputIterator first, InputIterator last);

    /**
     * Replaces the contents with those of the initializer list \p ilist.
     *
     * @param ilist initializer list to use as source to initialize the string with.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& assign(std::initializer_list<value_type> ilist);


    /**
     * Replaces the contents with a copy of \p str.
     *
     * @param str String to be used as the source to initialize the string with.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& assign(const std::string& str);

    /**
     * Replaces the string with a substring `[pos, pos + n)` of \p str. If `n == npos`, or if the
     * requested substring lasts past the end of the string, the resulting substring is `[pos, str.size())`.
     *
     * @param str Another string to use as source to initialize the string with.
     * @param pos Position of the first character to include.
     * @param n Number of characters to include.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& assign(const std::string& str, size_type pos, size_type n = npos);

    /**
     * Replaces the contents with a copy of \p sv.
     *
     * @param sv String view to be used as the source to initialize the string with.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& assign(const carb::cpp17::string_view& sv);

    /**
     * Replaces the string with a substring `[pos, pos + n)` of \p sv. If `n == npos`, or if the
     * requested substring lasts past the end of the string, the resulting substring is `[pos, sv.size())`.
     *
     * @param sv String view to use as source to initialize the string with.
     * @param pos Position of the first character to include.
     * @param n Number of characters to include.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& assign(const carb::cpp17::string_view& sv, size_type pos, size_type n = npos);

#if CARB_HAS_CPP17
    /**
     * Implicitly converts @p t to type `std::string_view` and replaces the contents of this string with a substring
     * `[pos, pos + n)` of that `string_view`. This overload participates in overload resolution only if
     * `std::is_convertible_v<const T&, std::string_view>` is true.
     *
     * @param t Object that can be converted to `std::string_view` to initialize with.
     *
     * @throws std::out_of_range if \p pos is greater than `sv.size()`.
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    string& assign(const T& t);

    /**
     * Implicitly converts @p t to type `std::string_view` and replaces the contents of this string with a substring
     * `[pos, pos + n)` of that `string_view`. If `n == npos`, or if the requested substring lasts past the end of
     * the `string_view`, the resulting substring is `[pos, sv.size())`. This overload participates in overload
     * resolution only if `std::is_convertible_v<const T&, std::string_view>` is true.
     *
     * @param t Object that can be converted to `std::string_view` to initialize with.
     * @param pos Position of the first character to include.
     * @param n Number of characters to include.
     *
     * @throws std::out_of_range if \p pos is greater than `sv.size()`.
     * @throws std::length_error if the string would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    string& assign(const T& t, size_type pos, size_type n = npos);
#endif

    /**
     * Replaces the contents with those of the \c printf style format string and arguments.
     *
     * @param fmt \c printf style format string to initialize the string with. Must not overlap with \c *this.
     * @param ... additional arguments matching \p fmt. Arguments must not overlap with \c *this.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws std::runtime_error if an overlap is detected or \c vsnprintf reports error.
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& assign_printf(const char* fmt, ...) CARB_PRINTF_FUNCTION(2, 3);

    /**
     * Replaces the contents with those of the \c vprintf style format string and arguments.
     *
     * @param fmt \c vprintf style format string to initialize the string with. Must not overlap with \c *this.
     * @param ap \c va_list as initialized with \c va_start. Arguments must not overlap with \c *this.
     * @return \c *this.
     *
     * @throws std::length_error if the string would be larger than max_size().
     * @throws std::runtime_error if an overlap is detected or \c vsnprintf reports error.
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& assign_vprintf(const char* fmt, va_list ap);

    /**
     * Returns a reference to the character at specified location \p pos. Bounds checking is performed.
     *
     * @param pos Position of the character to return.
     * @return Reference to the character at pos.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     */
    constexpr reference at(size_type pos);

    /**
     * Returns a reference to the character at specified location \p pos. Bounds checking is performed.
     *
     * @param pos Position of the character to return.
     * @return Reference to the character at pos.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     */
    constexpr const_reference at(size_type pos) const;

    /**
     * Returns a reference to the character at specified location \p pos. No bounds checking is performed.
     *
     * @param pos Position of the character to return.
     * @return Reference to the character at pos.
     */
    constexpr reference operator[](size_type pos);

    /**
     * Returns a reference to the character at specified location \p pos. No bounds checking is performed.
     *
     * @param pos Position of the character to return.
     * @return Reference to the character at pos.
     */
    constexpr const_reference operator[](size_type pos) const;

    /**
     * Returns a reference to the first character. Behavior is undefined if this string is empty.
     *
     * @return Reference to the first character.
     */
    constexpr reference front();

    /**
     * Returns a reference to the first character. Behavior is undefined if this string is empty.
     *
     * @return Reference to the first character.
     */
    constexpr const_reference front() const;

    /**
     * Returns a reference to the last character. Behavior is undefined if this string is empty.
     *
     * @return Reference to the last character.
     */
    constexpr reference back();

    /**
     * Returns a reference to the last character. Behavior is undefined if this string is empty.
     *
     * @return Reference to the last character.
     */
    constexpr const_reference back() const;

    /**
     * Returns a pointer to the character array of the string. The returned array is null-terminated.
     *
     * @return Pointer to the character array of the string.
     */
    constexpr const value_type* data() const noexcept;

    /**
     * Returns a pointer to the character array of the string. The returned array is null-terminated.
     *
     * @return Pointer to the character array of the string.
     */
    constexpr value_type* data() noexcept;

    /**
     * Returns a pointer to the character array of the string. The returned array is null-terminated.
     *
     * @return Pointer to the character array of the string.
     */
    constexpr const value_type* c_str() const noexcept;

    /**
     * Returns a `carb::cpp17::string_view` constructed as if by `carb::cpp17::string_view(data(), size())`.
     *
     * @return A `carb::cpp17::string_view` representing the string.
     */
    constexpr operator carb::cpp17::string_view() const noexcept;

#if CARB_HAS_CPP17
    /**
     * Returns a `std::string_view` constructed as if by `std::string_view(data(), size())`.
     *
     * @return A `std::string_view` representing the string.
     */
    constexpr operator std::string_view() const noexcept;
#endif

    /**
     * Returns an iterator to the first character in the string.
     *
     * @return iterator to the first character in the string.
     */
    constexpr iterator begin() noexcept;

    /**
     * Returns a constant iterator to the first character in the string.
     *
     * @return iterator to the first character in the string.
     */
    constexpr const_iterator begin() const noexcept;

    /**
     * Returns a constant iterator to the first character in the string.
     *
     * @return iterator to the first character in the string.
     */
    constexpr const_iterator cbegin() const noexcept;

    /**
     * Returns an iterator to the character following the last character of the string.
     *
     * @return iterator to the character following the last character of the string.
     */
    constexpr iterator end() noexcept;

    /**
     * Returns a constant iterator to the character following the last character of the string.
     *
     * @return iterator to the character following the last character of the string.
     */
    constexpr const_iterator end() const noexcept;

    /**
     * Returns a constant iterator to the character following the last character of the string.
     *
     * @return iterator to the character following the last character of the string.
     */
    constexpr const_iterator cend() const noexcept;

    /**
     * Returns a reverse iterator to the first character in the reversed string. This character is the last character in
     * the non-reversed string.
     *
     * @return reverse iterator to the first character in the reversed string.
     */
    reverse_iterator rbegin() noexcept;

    /**
     * Returns a constant reverse iterator to the first character in the reversed string. This character is the last
     * character in the non-reversed string.
     *
     * @return reverse iterator to the first character in the reversed string.
     */
    const_reverse_iterator rbegin() const noexcept;

    /**
     * Returns a constant reverse iterator to the first character in the reversed string. This character is the last
     * character in the non-reversed string.
     *
     * @return reverse iterator to the first character in the reversed string.
     */
    const_reverse_iterator crbegin() const noexcept;

    /**
     * Returns a reverse iterator to the character following the last character in the reversed string. This character
     * corresponds to character before the first character in the non-reversed string.
     *
     * @return reverse iterator to the character following the last character in the reversed string.
     */
    reverse_iterator rend() noexcept;

    /**
     * Returns a constant reverse iterator to the character following the last character in the reversed string. This
     * character corresponds to character before the first character in the non-reversed string.
     *
     * @return reverse iterator to the character following the last character in the reversed string.
     */
    const_reverse_iterator rend() const noexcept;

    /**
     * Returns a constant reverse iterator to the character following the last character in the reversed string. This
     * character corresponds to character before the first character in the non-reversed string.
     *
     * @return reverse iterator to the character following the last character in the reversed string.
     */
    const_reverse_iterator crend() const noexcept;

    /**
     * Checks if the string is empty.
     *
     * @return true if the string is empty, false otherwise.
     */
    constexpr bool empty() const noexcept;

    /**
     * Returns the number of characters in the string.
     *
     * @return the number of characters in the string.
     */
    constexpr size_type size() const noexcept;

    /**
     * Returns the number of characters in the string.
     *
     * @return the number of characters in the string.
     */
    constexpr size_type length() const noexcept;

    /**
     * Returns the maximum number of characters that can be in the string.
     *
     * @return the maximum number of characters that can be in the string.
     */
    constexpr size_type max_size() const noexcept;

    /**
     * Attempt to change the capacity of the string.
     *
     * If \p new_cap is greater than the current capacity(), the string will allocate a new buffer equal to or
     * larger than \p new_cap.
     *
     * If \p new_cap is less than the current capacity(), the string may shrink the buffer.
     *
     * If \p new_cap is less that the current size(), the string will shrink the buffer to fit the current
     * size() as if by calling shrink_to_fit().
     *
     * If reallocation takes place, all pointers, references, and iterators are invalidated.
     *
     * @return none.
     *
     * @throws std::length_error if \p new_cap is larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    void reserve(size_type new_cap);

    /**
     * Reduce the capacity of the string as if by calling shrink_to_fit().
     *
     * If reallocation takes place, all pointers, references, and iterators are invalidated.
     *
     * @return none.
     *
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    void reserve(); // deprecated in C++20

    /**
     * Returns the number of characters that can fit in the current storage array.
     *
     * @return the number of characters that can fit in the current storage array.
     */
    constexpr size_type capacity() const noexcept;

    /**
     * Reduce capacity() to size().
     *
     * If reallocation takes place, all pointers, references, and iterators are invalidated.
     *
     * @return none.
     *
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    void shrink_to_fit();

    /**
     * Clears the contents of the string. capacity() is not changed by this function.
     *
     * All pointers, references, and iterators are invalidated.
     *
     * @return none.
     */
    constexpr void clear() noexcept;

    /**
     * Inserts \p n copies of character \p c at position \p pos.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param pos Position to insert characters.
     * @param n Number of characters to insert.
     * @param c Character to insert.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     * @throws std::length_error if `n + size()` would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& insert(size_type pos, size_type n, value_type c);

    /**
     * Inserts the string pointed to by \p s at position \p pos.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param pos Position to insert characters.
     * @param s String to insert.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::length_error if `Traits::length(s) + size()` would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& insert(size_type pos, const value_type* s);

    /**
     * Inserts the first \p n characters of the string pointed to by \p s at position \p pos. The range can contain null
     * characters.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param pos Position to insert characters.
     * @param s String to insert.
     * @param n Number of characters to insert.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::length_error if `n + size()` would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& insert(size_type pos, const value_type* s, size_type n);

    /**
     * Inserts the string \p str at position \p pos.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param pos Position to insert characters.
     * @param str String to insert.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     * @throws std::length_error if `str.size() + size()` would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& insert(size_type pos, const string& str);

    /**
     * Inserts the substring `str.substr(pos2, n)` at position \p pos1.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param pos1 Position to insert characters.
     * @param str String to insert.
     * @param pos2 Position in \p str to copy characters from.
     * @param n Number of characters to insert.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos1 is greater than size() or \p pos2 is greater than `str.size()`.
     * @throws std::length_error if `n + size()` would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& insert(size_type pos1, const string& str, size_type pos2, size_type n = npos);

    /**
     * Inserts the character \p c before the character pointed to by \p p.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param p Iterator to the position the character should be inserted before.
     * @param c Character to insert.
     *
     * @return Iterator to the inserted character, or \p p if no character was inserted.
     *
     * @throws std::out_of_range if \p p is not in the range [begin(), end()].
     * @throws std::length_error if `1 + size()` would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    iterator insert(const_iterator p, value_type c);

    /**
     * Inserts \p n copies of the character \p c before the character pointed to by \p p.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param p Iterator to the position the character should be inserted before.
     * @param n Number of characters to inserts.
     * @param c Character to insert.
     *
     * @return Iterator to the first inserted character, or \p p if no character was inserted.
     *
     * @throws std::out_of_range if \p p is not in the range [begin(), end()].
     * @throws std::length_error if `n + size()` would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    iterator insert(const_iterator p, size_type n, value_type c);

    /**
     * Inserts characters from the range `[first, last)` before the character pointed to by \p p.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param p Iterator to the position the character should be inserted before.
     * @param first Iterator to the first character to insert.
     * @param last Iterator to the first character not to be inserted.
     *
     * @return Iterator to the first inserted character, or \p p if no character was inserted.
     *
     * @throws std::out_of_range if \p p is not in the range [begin(), end()].
     * @throws std::length_error if `std::distance(first, last) + size()` would be larger than
     * max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <class InputIterator>
    iterator insert(const_iterator p, InputIterator first, InputIterator last);

    /**
     * Inserts the characters in \p ilist before the character pointed to by \p p.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param p Iterator to the position the character should be inserted before.
     * @param ilist Initializer list of characters to insert.
     *
     * @return Iterator to the first inserted character, or \p p if no character was inserted.
     *
     * @throws std::out_of_range if \p p is not in the range [begin(), end()].
     * @throws std::length_error if `ilist.size() + size()` would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    iterator insert(const_iterator p, std::initializer_list<value_type> ilist);

    /**
     * Inserts the string \p str at position \p pos.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param pos Position to insert characters.
     * @param str String to insert.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     * @throws std::length_error if `str.size() + size()` would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& insert(size_type pos, const std::string& str);

    /**
     * Inserts the substring `str.substr(pos2, n)` at position \p pos1.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param pos1 Position to insert characters.
     * @param str String to insert.
     * @param pos2 Position in \p str to copy characters from.
     * @param n Number of characters to insert.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos1 is greater than size() or \p pos2 is greater than `str.size()`.
     * @throws std::length_error if `n + size()` would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& insert(size_type pos1, const std::string& str, size_type pos2, size_type n = npos);

    /**
     * Inserts the string_view \p sv at position \p pos.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param pos Position to insert characters.
     * @param sv String view to insert.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     * @throws std::length_error if `str.size() + size()` would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& insert(size_type pos, const carb::cpp17::string_view& sv);

    /**
     * Inserts the substring `sv.substr(pos2, n)` at position \p pos1.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param pos1 Position to insert characters.
     * @param sv String view to insert.
     * @param pos2 Position in \p str to copy characters from.
     * @param n Number of characters to insert.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos1 is greater than size() or \p pos2 is greater than `sv.size()`.
     * @throws std::length_error if `n + size()` would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& insert(size_type pos1, const carb::cpp17::string_view& sv, size_type pos2, size_type n = npos);


#if CARB_HAS_CPP17
    /**
     * Implicitly converts @p t to type `std::string_view` and inserts that `string_view` at position \p pos. This
     * overload participates in overload resolution only if `std::is_convertible_v<const T&, std::string_view>` is true.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param pos Position to insert characters.
     * @param t Object that can be converted to `std::string_view` to initialize with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     * @throws std::length_error if `str.size() + size()` would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    string& insert(size_type pos, const T& t);

    /**
     * Implicitly converts @p t to type `std::string_view` and inserts inserts the substring `sv.substr(pos2, n)` at
     * position \p pos1. This overload participates in overload resolution only if `std::is_convertible_v<const T&,
     * std::string_view>` is true.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param pos1 Position to insert characters.
     * @param t Object that can be converted to `std::string_view` to initialize with.
     * @param pos2 Position in \p str to copy characters from.
     * @param n Number of characters to insert.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos1 is greater than size() or \p pos2 is greater than `sv.size()`.
     * @throws std::length_error if `n + size()` would be larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    string& insert(size_type pos1, const T& t, size_type pos2, size_type n = npos);
#endif

    /**
     * Inserts the \c printf style format string and arguments before the \p pos position.
     *
     * @param pos Position to insert characters.
     * @param fmt \c printf style format string to initialize the string with. Must not overlap with \c *this.
     * @param ... additional arguments matching \p fmt. Arguments must not overlap with \c *this.
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is not in the range [0, size()].
     * @throws std::length_error if the resulting string would be larger than max_size().
     * @throws std::runtime_error if an overlap is detected or \c vsnprintf reports error.
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& insert_printf(size_type pos, const char* fmt, ...) CARB_PRINTF_FUNCTION(3, 4);

    /**
     * Inserts the \c vprintf style format string and arguments before the \p pos position.
     *
     * @param pos Position to insert characters.
     * @param fmt \c vprintf style format string to initialize the string with. Must not overlap with \c *this.
     * @param ap \c va_list as initialized by \c va_start. Arguments must not overlap with \c *this.
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is not in the range [0, size()].
     * @throws std::length_error if the resulting string would be larger than max_size().
     * @throws std::runtime_error if an overlap is detected or \c vsnprintf reports error.
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& insert_vprintf(size_type pos, const char* fmt, va_list ap);

    /**
     * Inserts the \c printf style format string and arguments before the character pointed to by \p p.
     *
     * @param p Iterator to the position the string should be inserted before.
     * @param fmt \c printf style format string to initialize the string with. Must not overlap with \c *this.
     * @param ... additional arguments matching \p fmt. Arguments must not overlap with \c *this.
     * @return Iterator to the first inserted character, or \p p if nothing was inserted.
     *
     * @throws std::out_of_range if \p p is not in the range [begin(), end()].
     * @throws std::length_error if the resulting string would be larger than max_size().
     * @throws std::runtime_error if an overlap is detected or \c vsnprintf reports error.
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    iterator insert_printf(const_iterator p, const char* fmt, ...) CARB_PRINTF_FUNCTION(3, 4);

    /**
     * Inserts the \c vprintf style format string and arguments before the character pointed to by \p p.
     *
     * @param p Iterator to the position the string should be inserted before.
     * @param fmt \c vprintf style format string to initialize the string with. Must not overlap with \c *this.
     * @param ap \c va_list as initialized by \c va_start. Arguments must not overlap with \c *this.
     * @return Iterator to the first inserted character, or \p p if nothing was inserted.
     *
     * @throws std::out_of_range if \p p is not in the range [begin(), end()].
     * @throws std::length_error if the resulting string would be larger than max_size().
     * @throws std::runtime_error if an overlap is detected or \c vsnprintf reports error.
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    iterator insert_vprintf(const_iterator p, const char* fmt, va_list ap);

    /**
     * Erases \p n characters from the string starting at \p pos. If \p n is \p npos or `pos + n > size()`,
     * characters are erased to the end of the string.
     *
     * Pointers, references, and iterators may be invalidated.
     *
     * @param pos Position to begin erasing.
     * @param n Number of characters to erase.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     */
    constexpr string& erase(size_type pos = 0, size_type n = npos);

    /**
     * Erases the character pointed to by \p pos.
     *
     * Pointers, references, and iterators may be invalidated.
     *
     * @param pos Position to erase character at.
     *
     * @return iterator pointing to the character immediately following the character erased, or end() if no such
     * character exists.
     */
    constexpr iterator erase(const_iterator pos);

    /**
     * Erases characters in the range `[first, last)`.
     *
     * Pointers, references, and iterators may be invalidated.
     *
     * @param first Position to begin erasing at.
     * @param last Position to stop erasing at.
     *
     * @return iterator pointing to the character last pointed to before the erase, or end() if no such character
     * exists.
     *
     * @throws std::out_of_range if the range `[first, last)` is invalid (not in the range [begin(), end()],
     *         or `first > last`.
     */
    constexpr iterator erase(const_iterator first, const_iterator last);

    /**
     * Appends the character \p c to the string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @return none.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    void push_back(value_type c);

    /**
     * Removes the last character from the string.
     *
     * Pointers, references, and iterators may be invalidated.
     *
     * @return none.
     *
     * @throws std::runtime_error if the string is empty().
     */
    constexpr void pop_back();

    /**
     * Appends \p n copies of character \p c to the end of the string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param n Number of characters to append.
     * @param c Character to append.
     *
     * @return \c *this.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& append(size_type n, value_type c);

    /**
     * Appends \p str to the end of the string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param str The string to append.
     *
     * @return \c *this.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& append(const string& str);

    /**
     * Appends the substring `str.substr(pos2, n)` to the end of the string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param str The string to append.
     * @param pos Position of the first character to append.
     * @param n Number of characters to append.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than `str.size()`.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& append(const string& str, size_type pos, size_type n = npos);

    /**
     * Appends \p n character of the string \p s to the end of the string. The range can contain nulls.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param s String to append characters from.
     * @param n Number of characters to append.
     *
     * @return \c *this.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& append(const value_type* s, size_type n);

    /**
     * Appends the null-terminated string \p s to the end of the string. Behavior is undefined if \p s is not a valid
     * string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param s String to append characters from.
     *
     * @return \c *this.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& append(const value_type* s);

    /**
     * Appends characters in the range `[first, last)` to the string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param first First character in the range to append.
     * @param last End of the range to append.
     *
     * @return \c *this.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <class InputIterator>
    string& append(InputIterator first, InputIterator last);

    /**
     * Appends characters in \p ilist to the string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param ilist Initializer list of characters to append.
     *
     * @return \c *this.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& append(std::initializer_list<value_type> ilist);

    /**
     * Appends \p str to the end of the string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param str The string to append.
     *
     * @return \c *this.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& append(const std::string& str);

    /**
     * Appends the substring `str.substr(pos2, n)` to the end of the string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param str The string to append.
     * @param pos Position of the first character to append.
     * @param n Number of characters to append.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than `str.size()`.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& append(const std::string& str, size_type pos, size_type n = npos);

    /**
     * Appends \p sv to the end of the string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param sv The string view to append.
     *
     * @return \c *this.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& append(const carb::cpp17::string_view& sv);

    /**
     * Appends the substring `sv.substr(pos2, n)` to the end of the string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param sv The string view to append.
     * @param pos Position of the first character to append.
     * @param n Number of characters to append.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than `str.size()`.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& append(const carb::cpp17::string_view& sv, size_type pos, size_type n = npos);

#if CARB_HAS_CPP17
    /**
     * Implicitly converts @p t to type `std::string_view` and appends it to the end of the string. This overload
     * participates in overload resolution only if `std::is_convertible_v<const T&, std::string_view>` is true.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param t Object that can be converted to `std::string_view` to initialize with.
     *
     * @return \c *this.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    string& append(const T& t);

    /**
     * Implicitly converts @p t to type `std::string_view` and appends the substring `sv.substr(pos2, n)` to the end of
     * the string. This overload participates in overload resolution only if `std::is_convertible_v<const T&,
     * std::string_view>` is true.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param t Object that can be converted to `std::string_view` to initialize with.
     * @param pos Position of the first character to append.
     * @param n Number of characters to append.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than `str.size()`.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    string& append(const T& t, size_type pos, size_type n = npos);
#endif

    /**
     * Appends the \c printf style format string and arguments to the string.
     *
     * @param fmt \c printf style format string to initialize the string with. Must not overlap with \c *this.
     * @param ... additional arguments matching \p fmt. Arguments must not overlap with \c *this.
     * @return \c *this.
     *
     * @throws std::length_error if the resulting string would be larger than max_size().
     * @throws std::runtime_error if an overlap is detected or \c vsnprintf reports error.
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& append_printf(const char* fmt, ...) CARB_PRINTF_FUNCTION(2, 3);

    /**
     * Appends the \c printf style format string and arguments to the string.
     *
     * @param fmt \c printf style format string to initialize the string with. Must not overlap with \c *this.
     * @param ap \c va_list as initialized by \c va_start. Arguments must not overlap with \c *this.
     * @return \c *this.
     *
     * @throws std::length_error if the resulting string would be larger than max_size().
     * @throws std::runtime_error if an overlap is detected or \c vsnprintf reports error.
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& append_vprintf(const char* fmt, va_list ap);

    /**
     * Appends \p str to the end of the string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param str The string to append.
     *
     * @return \c *this.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& operator+=(const string& str);

    /**
     * Appends the character \p c to the end of the string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param c Character to append.
     *
     * @return \c *this.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& operator+=(value_type c);

    /**
     * Appends the null-terminated string \p s to the end of the string. Behavior is undefined if \p s is not a valid
     * string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param s String to append characters from.
     *
     * @return \c *this.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& operator+=(const value_type* s);

    /**
     * Appends characters in \p ilist to the string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param ilist Initializer list of characters to append.
     *
     * @return \c *this.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& operator+=(std::initializer_list<value_type> ilist);

    /**
     * Appends \p str to the end of the string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param str The string to append.
     *
     * @return \c *this.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& operator+=(const std::string& str);

    /**
     * Appends \p sv to the end of the string.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param sv The string view to append.
     *
     * @return \c *this.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& operator+=(const carb::cpp17::string_view& sv);

#if CARB_HAS_CPP17
    /**
     * Implicitly converts @c t to type `std::string_view` and appends it to the end of the string. This overload
     * participates in overload resolution only if `std::is_convertible_v<const T&, std::string_view>` is true.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param t Object that can be converted to `std::string_view` to append.
     *
     * @return \c *this.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    string& operator+=(const T& t);
#endif

    /**
     * Compares \p str to this string.
     *
     * \code
     * Comparison is performed as follows:
     * If Traits::compare(this, str, min(size(), str.size())) < 0, a negative value is returned
     * If Traits::compare(this, str, min(size(), str.size())) == 0, then:
     *     If size() < str.size(), a negative value is returned
     *     If size() = str.size(), zero is returned
     *     If size() > str.size(), a positive value is returned
     * If Traits::compare(this, str, min(size(), str.size())) > 0, a positive value is returned
     * \endcode
     *
     * @param str String to compare to.
     *
     * @return A negative value if \c *this appears before the other string in lexicographical order,
     *         zero if \c *this and the other string compare equivalent,
     *         or a positive value if \c *this appears after the other string in lexicographical order.
     */
    constexpr int compare(const string& str) const noexcept;

    /**
     * Compares \p str to the substring `substr(pos1, n1)`.
     *
     * @see compare() for details on how the comparison is performed.
     *
     * @param pos1 Position to start this substring.
     * @param n1 Number of characters in this substring.
     * @param str String to compare to.
     *
     * @return A negative value if \c *this appears before the other string in lexicographical order,
     *         zero if \c *this and the other string compare equivalent,
     *         or a positive value if \c *this appears after the other string in lexicographical order.
     *
     * @throws std::out_of_range if \p pos1 is greater than size().
     */
    constexpr int compare(size_type pos1, size_type n1, const string& str) const;

    /**
     * Compares `str.substr(pos2, n2)` to the substring `substr(pos1, n1)`.
     *
     * @see compare() for details on how the comparison is performed.
     *
     * @param pos1 Position to start this substring.
     * @param n1 Number of characters in this substring.
     * @param str String to compare to.
     * @param pos2 Position to start other substring.
     * @param n2 Number of characters in other substring.
     *
     * @return A negative value if \c *this appears before the other string in lexicographical order,
     *         zero if \c *this and the other string compare equivalent,
     *         or a positive value if \c *this appears after the other string in lexicographical order.
     *
     * @throws std::out_of_range if \p pos1 is greater than size() or \p pos2 is greater than `str.size()`.
     */
    constexpr int compare(size_type pos1, size_type n1, const string& str, size_type pos2, size_type n2 = npos) const;

    /**
     * Compares the null-terminated string \p s to this string.
     *
     * @see compare() for details on how the comparison is performed.
     *
     * @param s  String to compare to.
     *
     * @return A negative value if \c *this appears before the other string in lexicographical order,
     *         zero if \c *this and the other string compare equivalent,
     *         or a positive value if \c *this appears after the other string in lexicographical order.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr int compare(const value_type* s) const;

    /**
     * Compares the null-terminated string \p s to the substring `substr(pos1, n1)`.
     *
     * @see compare() for details on how the comparison is performed.
     *
     * @param pos1 Position to start this substring.
     * @param n1 Number of characters in this substring.
     * @param s  String to compare to.
     *
     * @return A negative value if \c *this appears before the other string in lexicographical order,
     *         zero if \c *this and the other string compare equivalent,
     *         or a positive value if \c *this appears after the other string in lexicographical order.
     *
     * @throws std::out_of_range if \p pos1 is greater than size().
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr int compare(size_type pos1, size_type n1, const value_type* s) const;

    /**
     * Compares the first \p n2 characters of string string \p s to the substring `substr(pos1, n1)`.
     *
     * @see compare() for details on how the comparison is performed.
     *
     * @param pos1 Position to start this substring.
     * @param n1 Number of characters in this substring.
     * @param s  String to compare to.
     * @param n2 Number of characters of \p s to compare.
     *
     * @return A negative value if \c *this appears before the other string in lexicographical order,
     *         zero if \c *this and the other string compare equivalent,
     *         or a positive value if \c *this appears after the other string in lexicographical order.
     *
     * @throws std::out_of_range if \p pos1 is greater than size().
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr int compare(size_type pos1, size_type n1, const value_type* s, size_type n2) const;

    /**
     * Compares \p str to this string.
     *
     * @see compare() for details on how the comparison is performed.
     *
     * @param str String to compare to.
     *
     * @return A negative value if \c *this appears before the other string in lexicographical order,
     *         zero if \c *this and the other string compare equivalent,
     *         or a positive value if \c *this appears after the other string in lexicographical order.
     */
    CARB_CPP20_CONSTEXPR int compare(const std::string& str) const noexcept;

    /**
     * Compares \p str to the substring `substr(pos1, n1)`.
     *
     * @see compare() for details on how the comparison is performed.
     *
     * @param pos1 Position to start this substring.
     * @param n1 Number of characters in this substring.
     * @param str String to compare to.
     *
     * @return A negative value if \c *this appears before the other string in lexicographical order,
     *         zero if \c *this and the other string compare equivalent,
     *         or a positive value if \c *this appears after the other string in lexicographical order.
     *
     * @throws std::out_of_range if \p pos1 is greater than size().
     */
    CARB_CPP20_CONSTEXPR int compare(size_type pos1, size_type n1, const std::string& str) const;

    /**
     * Compares `str.substr(pos2, n2)` to the substring `substr(pos1, n1)`.
     *
     * @see compare() for details on how the comparison is performed.
     *
     * @param pos1 Position to start this substring.
     * @param n1 Number of characters in this substring.
     * @param str String to compare to.
     * @param pos2 Position to start other substring.
     * @param n2 Number of characters in other substring.
     *
     * @return A negative value if \c *this appears before the other string in lexicographical order,
     *         zero if \c *this and the other string compare equivalent,
     *         or a positive value if \c *this appears after the other string in lexicographical order.
     *
     * @throws std::out_of_range if \p pos1 is greater than size() or \p pos2 is greater than `str.size()`.
     */
    CARB_CPP20_CONSTEXPR int compare(
        size_type pos1, size_type n1, const std::string& str, size_type pos2, size_type n2 = npos) const;

    /**
     * Compares \p sv to this string.
     *
     * @see compare() for details on how the comparison is performed.
     *
     * @param sv String view to compare to.
     *
     * @return A negative value if \c *this appears before the other string in lexicographical order,
     *         zero if \c *this and the other string compare equivalent,
     *         or a positive value if \c *this appears after the other string in lexicographical order.
     */
    constexpr int compare(const carb::cpp17::string_view& sv) const noexcept;

    /**
     * Compares \p sv to the substring `substr(pos1, n1)`.
     *
     * @see compare() for details on how the comparison is performed.
     *
     * @param pos1 Position to start this substring.
     * @param n1 Number of characters in this substring.
     * @param sv String view to compare to.
     *
     * @return A negative value if \c *this appears before the other string in lexicographical order,
     *         zero if \c *this and the other string compare equivalent,
     *         or a positive value if \c *this appears after the other string in lexicographical order.
     *
     * @throws std::out_of_range if \p pos1 is greater than size().
     */
    constexpr int compare(size_type pos1, size_type n1, const carb::cpp17::string_view& sv) const;

    /**
     * Compares `sv.substr(pos2, n2)` to the substring `substr(pos1, n1)`.
     *
     * @see compare() for details on how the comparison is performed.
     *
     * @param pos1 Position to start this substring.
     * @param n1 Number of characters in this substring.
     * @param sv String view to compare to.
     * @param pos2 Position to start other substring.
     * @param n2 Number of characters in other substring.
     *
     * @return A negative value if \c *this appears before the other string in lexicographical order,
     *         zero if \c *this and the other string compare equivalent,
     *         or a positive value if \c *this appears after the other string in lexicographical order.
     *
     * @throws std::out_of_range if \p pos1 is greater than size() or \p pos2 is greater than `sv.size()`.
     */
    constexpr int compare(
        size_type pos1, size_type n1, const carb::cpp17::string_view& sv, size_type pos2, size_type n2 = npos) const;

#if CARB_HAS_CPP17
    /**
     * Implicitly converts @p t to type `std::string_view` and compares it to the string. This overload
     * participates in overload resolution only if `std::is_convertible_v<const T&, std::string_view>` is true.
     *
     * @see compare() for details on how the comparison is performed.
     *
     * @param t Object that can be converted to `std::string_view` to compare to.
     *
     * @return A negative value if \c *this appears before the other string in lexicographical order,
     *         zero if \c *this and the other string compare equivalent,
     *         or a positive value if \c *this appears after the other string in lexicographical order.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    constexpr int compare(const T& t) const noexcept;

    /**
     * Implicitly converts @p t to type `std::string_view` and compares it to the substring `substr(pos1, n1)`. This
     * overload participates in overload resolution only if `std::is_convertible_v<const T&, std::string_view>` is true.
     *
     * @see compare() for details on how the comparison is performed.
     *
     * @param pos1 Position to start this substring.
     * @param n1 Number of characters in this substring.
     * @param t Object that can be converted to `std::string_view` to compare to.
     *
     * @return A negative value if \c *this appears before the other string in lexicographical order,
     *         zero if \c *this and the other string compare equivalent,
     *         or a positive value if \c *this appears after the other string in lexicographical order.
     *
     * @throws std::out_of_range if \p pos1 is greater than size().
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    constexpr int compare(size_type pos1, size_type n1, const T& t) const;

    /**
     * Implicitly converts @p t to type `std::string_view` and compares `sv.substr(pos2, n2)` to the substring
     * `substr(pos1, n1)`. This overload participates in overload resolution only if `std::is_convertible_v<const T&,
     * std::string_view>` is true.
     *
     * @see compare() for details on how the comparison is performed.
     *
     * @param pos1 Position to start this substring.
     * @param n1 Number of characters in this substring.
     * @param t Object that can be converted to `std::string_view` to compare to.
     * @param pos2 Position to start other substring.
     * @param n2 Number of characters in other substring.
     *
     * @return A negative value if \c *this appears before the other string in lexicographical order,
     *         zero if \c *this and the other string compare equivalent,
     *         or a positive value if \c *this appears after the other string in lexicographical order.
     *
     * @throws std::out_of_range if \p pos1 is greater than size() or \p pos2 is greater than `sv.size()`.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    constexpr int compare(size_type pos1, size_type n1, const T& t, size_type pos2, size_type n2 = npos) const;
#endif

    /**
     * Checks if the string begins with the character \p c.
     *
     * @param c Character to check.
     *
     * @return true if the string starts with \p c, false otherwise.
     */
    constexpr bool starts_with(value_type c) const noexcept;

    /**
     * Checks if the string begins with the string \p s.
     *
     * @param s String to check.
     *
     * @return true if the string starts with \p s, false otherwise.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr bool starts_with(const_pointer s) const;

    /**
     * Checks if the string begins with the string view \p sv.
     *
     * @param sv String view to check.
     *
     * @return true if the string starts with \p sv, false otherwise.
     */
    constexpr bool starts_with(carb::cpp17::string_view sv) const noexcept;

#if CARB_HAS_CPP17
    /**
     * Checks if the string begins with the string view \p sv.
     *
     * @param sv String view to check.
     *
     * @return true if the string starts with \p sv, false otherwise.
     */
    constexpr bool starts_with(std::string_view sv) const noexcept;
#endif

    /**
     * Checks if the string ends with the character \p c.
     *
     * @param c Character to check.
     *
     * @return true if the string ends with \p c, false otherwise.
     */
    constexpr bool ends_with(value_type c) const noexcept;

    /**
     * Checks if the string ends with the string \p s.
     *
     * @param s String to check.
     *
     * @return true if the string ends with \p s, false otherwise.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr bool ends_with(const_pointer s) const;

    /**
     * Checks if the string ends with the string view \p sv.
     *
     * @param sv String view to check.
     *
     * @return true if the string ends with \p sv, false otherwise.
     */
    constexpr bool ends_with(carb::cpp17::string_view sv) const noexcept;

#if CARB_HAS_CPP17
    /**
     * Checks if the string ends with the string view \p sv.
     *
     * @param sv String view to check.
     *
     * @return true if the string ends with \p sv, false otherwise.
     */
    constexpr bool ends_with(std::string_view sv) const noexcept;
#endif

    /**
     * Checks if the string contains the character \p c.
     *
     * @param c Character to check.
     *
     * @return true if the string contains \p c, false otherwise.
     */
    constexpr bool contains(value_type c) const noexcept;

    /**
     * Checks if the string contains the string \p s.
     *
     * @param s String to check.
     *
     * @return true if the string contains \p s, false otherwise.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr bool contains(const_pointer s) const;

    /**
     * Checks if the string contains the string view \p sv.
     *
     * @param sv String view to check.
     *
     * @return true if the string contains \p sv, false otherwise.
     */
    constexpr bool contains(carb::cpp17::string_view sv) const noexcept;

#if CARB_HAS_CPP17
    /**
     * Checks if the string contains the string view \p sv.
     *
     * @param sv String view to check.
     *
     * @return true if the string contains \p sv, false otherwise.
     */
    constexpr bool contains(std::string_view sv) const noexcept;
#endif

    /**
     * Replaces the portion of this string `[pos, pos + n1)` with \p str. If `n == npos`, or `pos + n` is greater than
     * size(), the substring to the end of the string is replaced.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param pos1 Position to start replacement.
     * @param n1 Number of characters to replace.
     * @param str String to replace characters with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos1 is greater than size().
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(size_type pos1, size_type n1, const string& str);

    /**
     * Replaces the portion of this string `[pos, pos + n1)` with the substring  `str.substr(pos2, n2)`. If
     * `n == npos`, or `pos + n` is greater than size(), the substring to the end of the string is replaced.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param pos1 Position to start replacement.
     * @param n1 Number of characters to replace.
     * @param str String to replace characters with.
     * @param pos2 Position of substring to replace characters with.
     * @param n2 Number of characters in the substring to replace with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos1 is greater than size() or \p pos2 is greater than `str.size()`.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(size_type pos1, size_type n1, const string& str, size_type pos2, size_type n2 = npos);

    /**
     * Replaces the portion of this string `[i1, i2)` with `[j1, j2)`.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param i1 Position to start replacement.
     * @param i2 Position to stop replacement.
     * @param j1 Start position of replacement characters.
     * @param j2 End position of replacement characters.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if the range `[i1,i2)` is invalid (not in the range [begin(), end()], or
     *         `i1 > i2`.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <class InputIterator>
    string& replace(const_iterator i1, const_iterator i2, InputIterator j1, InputIterator j2);

    /**
     * Replaces the portion of this string `[pos, pos + n1)` with \p n2 characters from string \p s. The character
     * sequence can contain null characters. If `n == npos`, or `pos + n` is greater than size(), the substring to
     * the end of the string is replaced.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param pos Position to start replacement.
     * @param n1 Number of characters to replace.
     * @param s String to replace characters with.
     * @param n2 The number of replacement characters.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(size_type pos, size_type n1, const value_type* s, size_type n2);

    /**
     * Replaces the portion of this string `[pos, pos + n1)` with the null-terminated string \p s. If `n == npos`, or
     * `pos + n` is greater than size(), the substring to the end of the string is replaced.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param pos Position to start replacement.
     * @param n1 Number of characters to replace.
     * @param s String to replace characters with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(size_type pos, size_type n1, const value_type* s);

    /**
     * Replaces the portion of this string `[pos, pos + n1)` with \p n2 copies of character \p c. If `n == npos`, or
     * `pos + n` is greater than size(), the substring to the end of the string is replaced.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param pos Position to start replacement.
     * @param n1 Number of characters to replace.
     * @param n2 Number of characters to replace with.
     * @param c Character to replace with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(size_type pos, size_type n1, size_type n2, value_type c);

    /**
     * Replaces the portion of this string `[pos, pos + n1)` with \p str. If `n == npos`, or `pos + n` is greater than
     * size(), the substring to the end of the string is replaced.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param pos1 Position to start replacement.
     * @param n1 Number of characters to replace.
     * @param str String to replace characters with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos1 is greater than size().
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(size_type pos1, size_type n1, const std::string& str);

    /**
     * Replaces the portion of this string `[pos, pos + n1)` with the substring  `str.substr(pos2, n2)`. If
     * `n == npos`, or `pos + n` is greater than size(), the substring to the end of the string is replaced.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param pos1 Position to start replacement.
     * @param n1 Number of characters to replace.
     * @param str String to replace characters with.
     * @param pos2 Position of substring to replace characters with.
     * @param n2 Number of characters in the substring to replace with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos1 is greater than size() or \p pos2 is greater than `str.size()`.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(size_type pos1, size_type n1, const std::string& str, size_type pos2, size_type n2 = npos);

    /**
     * Replaces the portion of this string `[pos, pos + n1)` with \c sv. If `n == npos`, or `pos + n` is greater than
     * size(), the substring to the end of the string is replaced.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param pos1 Position to start replacement.
     * @param n1 Number of characters to replace.
     * @param sv String view to replace characters with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos1 is greater than size().
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(size_type pos1, size_type n1, const carb::cpp17::string_view& sv);

    /**
     * Replaces the portion of this string `[pos, pos + n1)` with the substring  `sv.substr(pos2, n2)`. If
     * `n == npos`, or `pos + n` is greater than size(), the substring to the end of the string is replaced.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param pos1 Position to start replacement.
     * @param n1 Number of characters to replace.
     * @param sv String view to replace characters with.
     * @param pos2 Position of substring to replace characters with.
     * @param n2 Number of characters in the substring to replace with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos1 is greater than size() or \p pos2 is greater than `sv.size()`.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(size_type pos1, size_type n1, const carb::cpp17::string_view& sv, size_type pos2, size_type n2 = npos);

#if CARB_HAS_CPP17
    /**
     * Implicitly converts @p t to type `std::string_view` and replaces the portion of this string `[pos, pos + n1)`
     * with \p sv. If `n == npos`, or `pos + n` is greater than size(), the substring to the end of the string is
     * replaced. This overload participates in overload resolution only if `std::is_convertible_v<const T&,
     * std::string_view>` is true.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param pos1 Position to start replacement.
     * @param n1 Number of characters to replace.
     * @param t Object that can be converted to `std::string_view` to replace characters with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos1 is greater than size().
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    string& replace(size_type pos1, size_type n1, const T& t);

    /**
     * Implicitly converts @p t to type `std::string_view` and replaces the portion of this string `[pos, pos + n1)`
     * with the substring  `sv.substr(pos2, n2)`. If `n == npos`, or `pos + n is greater than size(), the substring to
     * the end of the string is replaced. This overload participates in overload resolution only if
     * `std::is_convertible_v<const T&, std::string_view>` is true.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param pos1 Position to start replacement.
     * @param n1 Number of characters to replace.
     * @param t Object that can be converted to `std::string_view` to replace characters with.
     * @param pos2 Position of substring to replace characters with.
     * @param n2 Number of characters in the substring to replace with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos1 is greater than size() or \p pos2 is greater than `sv.size()`.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    string& replace(size_type pos1, size_type n1, const T& t, size_type pos2, size_type n2);
#endif

    /**
     * Replaces the portion of this string `[i1, i2)` with \p str.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param i1 Position to start replacement.
     * @param i2 Position to stop replacement.
     * @param str String to replace characters with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if the range `[i1,i2)` is invalid (not in the range [begin(), end()], or
     *         `i1 > i2`.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(const_iterator i1, const_iterator i2, const string& str);

    /**
     * Replaces the portion of this string `[i1, i2)` with \p n characters from string \p s. The character
     * sequence can contain null characters.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param i1 Position to start replacement.
     * @param i2 Position to stop replacement.
     * @param s String to replace characters with.
     * @param n The number of replacement characters.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if the range `[i1,i2)` is invalid (not in the range [begin(), end()], or
     *         `i1 > i2`.
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(const_iterator i1, const_iterator i2, const value_type* s, size_type n);

    /**
     * Replaces the portion of this string `[i1, i2)` with the null-terminated string \p s.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param i1 Position to start replacement.
     * @param i2 Position to stop replacement.
     * @param s String to replace characters with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if the range `[i1,i2)` is invalid (not in the range [begin(), end()], or
     *         `i1 > i2`.
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(const_iterator i1, const_iterator i2, const value_type* s);

    /**
     * Replaces the portion of this string `[i1, i2)` with \p n copies of character \p c.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param i1 Position to start replacement.
     * @param i2 Position to stop replacement.
     * @param n Number of characters to replace with.
     * @param c Character to replace with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if the range `[i1,i2)` is invalid (not in the range [begin(), end()], or
     *         `i1 > i2`.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(const_iterator i1, const_iterator i2, size_type n, value_type c);

    /**
     * Replaces the portion of this string `[i1, i2)` with the characters in \p ilist.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param i1 Position to start replacement.
     * @param i2 Position to stop replacement.
     * @param ilist Initializer list of character to replace with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if the range `[i1,i2)` is invalid (not in the range [begin(), end()], or
     *         `i1 > i2`.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(const_iterator i1, const_iterator i2, std::initializer_list<value_type> ilist);

    /**
     * Replaces the portion of this string `[i1, i2)` with \p str.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param i1 Position to start replacement.
     * @param i2 Position to stop replacement.
     * @param str String to replace characters with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if the range `[i1,i2)` is invalid (not in the range [begin(), end()], or
     *         `i1 > i2`.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(const_iterator i1, const_iterator i2, const std::string& str);

    /**
     * Replaces the portion of this string `[i1, i2)` with \p sv.
     *
     * All pointers, references, and iterators may be invalidated.
     * @param sv String view to replace characters with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if the range `[i1,i2)` is invalid (not in the range [begin(), end()], or
     *         `i1 > i2`.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace(const_iterator i1, const_iterator i2, const carb::cpp17::string_view& sv);

#if CARB_HAS_CPP17
    /**
     * Implicitly converts @c t to type `std::string_view` and replaces the portion of this string `[i1, i2)` with
     * \p sv. This overload participates in overload resolution only if `std::is_convertible_v<const T&,
     * std::string_view>` is true.
     *
     * All pointers, references, and iterators may be invalidated.
     * @param t Object that can be converted to `std::string_view` to replace characters with.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if the range `[i1,i2)` is invalid (not in the range [begin(), end()], or
     *         `i1 > i2`.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    string& replace(const_iterator i1, const_iterator i2, const T& t);
#endif

    /**
     * Replaces the portion of this string `[pos, pos + n1)` with a \c printf style formatted string. If `n == npos`, or
     * `pos + n` is greater than size(), the substring to the end of the string is replaced.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param pos Position to start replacement.
     * @param n1 Number of characters to replace.
     * @param fmt \c printf style format string to replace characters with. Must not overlap with \c *this.
     * @param ... additional arguments matching \p fmt. Arguments must not overlap with \c *this.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws std::runtime_error if an overlap is detected or \c vsnprintf reports error.
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace_format(size_type pos, size_type n1, const value_type* fmt, ...) CARB_PRINTF_FUNCTION(4, 5);

    /**
     * Replaces the portion of this string `[pos, pos + n1)` with a \c vprintf style formatted string. If `n == npos`,
     * or `pos + n` is greater than size(), the substring to the end of the string is replaced.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param pos Position to start replacement.
     * @param n1 Number of characters to replace.
     * @param fmt \c printf style format string to replace characters with. Must not overlap with \c *this.
     * @param ap \c va_list as initialized with \c va_start. Arguments must not overlap with \c *this.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws std::runtime_error if an overlap is detected or \c vsnprintf reports error.
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace_vformat(size_type pos, size_type n1, const value_type* fmt, va_list ap);

    /**
     * Replaces the portion of this string `[i1, i2)` with a \c printf style formatted string.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param i1 Position to start replacement.
     * @param i2 Position to stop replacement.
     * @param fmt \c printf style format string to replace characters with. Must not overlap with \c *this.
     * @param ... additional arguments matching \p fmt. Arguments must not overlap with \c *this.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws std::runtime_error if an overlap is detected or \c vsnprintf reports error.
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace_format(const_iterator i1, const_iterator i2, const value_type* fmt, ...) CARB_PRINTF_FUNCTION(4, 5);

    /**
     * Replaces the portion of this string `[i1, i2)` with a \c printf style formatted string.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param i1 Position to start replacement.
     * @param i2 Position to stop replacement.
     * @param fmt \c printf style format string to replace characters with. Must not overlap with \c *this.
     * @param ap \c va_list as initialized with \c va_start. Arguments must not overlap with \c *this.
     *
     * @return \c *this.
     *
     * @throws std::out_of_range if \p pos is greater than \c size().
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws std::runtime_error if an overlap is detected or \c vsnprintf reports error.
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string& replace_vformat(const_iterator i1, const_iterator i2, const value_type* fmt, va_list ap);

    /**
     * Returns a substring from `[pos, pos + n)` of this string. If `n == npos`, or `pos + n` is greater than \ref
     * size(), the substring is to the end of the string.
     *
     * @param pos Position to start the substring.
     * @param n Number of characters to include in the substring.
     *
     * @return A new string containing the substring.
     *
     * @throws std::out_of_range if \p pos is greater than size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    string substr(size_type pos = 0, size_type n = npos) const;

    /**
     * Copies a substring from `[pos, pos + n)` to the provided destination \p s. If `n == npos`, or `pos + n` is
     * greater than size(), the substring is to the end of the string. The resulting character sequence is not null
     * terminated.
     *
     * @param s Destination to copy characters to.
     * @param n Number of characters to include in the substring.
     * @param pos Position to start the substring.
     *
     * @return number of characters copied.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     * @throws std::out_of_range if \p pos is greater than size().
     */
    constexpr size_type copy(value_type* s, size_type n, size_type pos = 0) const;

    /**
     * Resizes the string to contain \p n characters. If \p n is greater than size(), copies of the character \p c
     * are appended. If \p n is smaller than size(), the string is shrunk to size \p n.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param n New size of the string.
     * @param c Character to append when growing the string.
     *
     * @return none.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    void resize(size_type n, value_type c);

    /**
     * Resizes the string to contain \p n characters. If \p n is greater than size(), copies of \c NUL are
     * appended. If \p n is smaller than size(), the string is shrunk to size \p n.
     *
     * If reallocation occurs, all pointers, references, and iterators are invalidated.
     *
     * @param n New size of the string.
     *
     * @return none.
     *
     * @throws std::length_error if the function would result in size() being larger than max_size().
     * @throws Allocation This function may throw any exception thrown during allocation.
     */
    void resize(size_type n);

    /**
     * Swaps the contents of this string with \p str.
     *
     * All pointers, references, and iterators may be invalidated.
     *
     * @param str The string to swap with.
     *
     * @return none.
     */
    void swap(string& str) noexcept;

    /**
     * Finds the first substring of this string that matches \p str. The search begins at \p pos.
     *
     * @param str String to find.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     */
    constexpr size_type find(const string& str, size_type pos = 0) const noexcept;

    /**
     * Finds the first substring of this string that matches the first \p n characters of \p s. The string may contain
     * nulls.The search begins at \p pos.
     *
     * @param s String to find.
     * @param pos Position to begin the search.
     * @param n Number of characters to search for.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr size_type find(const value_type* s, size_type pos, size_type n) const;

    /**
     * Finds the first substring of this string that matches the null-terminated string \p s. The search begins at
     * \p pos.
     *
     * @param s String to find.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr size_type find(const value_type* s, size_type pos = 0) const;

    /**
     * Finds the first substring of this string that matches \p str. The search begins at \p pos.
     *
     * @param str String to find.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     */
    CARB_CPP20_CONSTEXPR size_type find(const std::string& str, size_type pos = 0) const noexcept;

    /**
     * Finds the first substring of this string that matches \p sv. The search begins at \p pos.
     *
     * @param sv String view to find.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     */
    constexpr size_type find(const carb::cpp17::string_view& sv, size_type pos = 0) const noexcept;

#if CARB_HAS_CPP17
    /**
     * Implicitly converts @c t to type `std::string_view` and finds the first substring of this string that matches it.
     * The search begins at \p pos. This overload participates in overload resolution only if
     * `std::is_convertible_v<const T&, std::string_view>` is true.
     *
     * @param t Object that can be converted to `std::string_view` to find.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    constexpr size_type find(const T& t, size_type pos = 0) const noexcept;
#endif

    /**
     * Finds the first substring of this string that matches \p c. The search begins at \p pos.
     *
     * @param c Character to find.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     */
    constexpr size_type find(value_type c, size_type pos = 0) const noexcept;

    /**
     * Finds the last substring of this string that matches \p str. The search begins at \p pos. If `pos == npos` or
     * `pos >= size()`, the whole string is searched.
     *
     * @param str String to find.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     */
    constexpr size_type rfind(const string& str, size_type pos = npos) const noexcept;

    /**
     * Finds the last substring of this string that matches the first \p n characters of \p s. The string may contain
     * nulls.The search begins at \p pos. If `pos == npos` or `pos >= size()`, the whole string is searched.
     *
     * @param s String to find.
     * @param pos Position to begin the search.
     * @param n Number of characters to search for.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr size_type rfind(const value_type* s, size_type pos, size_type n) const;

    /**
     * Finds the last substring of this string that matches the null-terminated string \p s. The search begins at
     * \p pos. If `pos == npos` or `pos >= size()`, the whole string is searched.
     *
     * @param s String to find.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr size_type rfind(const value_type* s, size_type pos = npos) const;

    /**
     * Finds the last substring of this string that matches \p c. The search begins at \p pos. If `pos == npos` or
     * `pos >= size()`, the whole string is searched.
     *
     * @param c Character to find.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     */
    constexpr size_type rfind(value_type c, size_type pos = npos) const noexcept;

    /**
     * Finds the last substring of this string that matches \p str. The search begins at \p pos. If `pos == npos` or
     * `pos >= size()`, the whole string is searched.
     *
     * @param str String to find.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     */
    CARB_CPP20_CONSTEXPR size_type rfind(const std::string& str, size_type pos = npos) const noexcept;

    /**
     * Finds the last substring of this string that matches \p sv. The search begins at \p pos. If `pos == npos` or
     * `pos >= size()`, the whole string is searched.
     *
     * @param sv String view to find.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     */
    constexpr size_type rfind(const carb::cpp17::string_view& sv, size_type pos = npos) const noexcept;

#if CARB_HAS_CPP17
    /**
     * Implicitly converts @c t to type `std::string_view` and finds the last substring of this string that matches it.
     * The search begins at \p pos. If `pos == npos` or `pos >= size()`, the whole string is searched. This
     * overload participates in overload resolution only if `std::is_convertible_v<const T&, std::string_view>` is true.
     *
     * @param t Object that can be converted to `std::string_view` to find.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    constexpr size_type rfind(const T& t, size_type pos = npos) const noexcept;
#endif

    /**
     * Finds the first character equal to one of the characters in string \p str. The search begins at \p pos.
     *
     * @param str String containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the first found character, or \c npos if no character is found.
     */
    constexpr size_type find_first_of(const string& str, size_type pos = 0) const noexcept;

    /**
     * Finds the first character equal to one of the characters in the first \p n characters of string \p s. The search
     * begins at \p pos.
     *
     * @param s String containing the characters to search for.
     * @param pos Position to begin the search.
     * @param n Number of characters in \p s to search for.
     *
     * @return Position of the first found character, or \c npos if no character is found.
     *
     * @throws std::invalid_argument if \p s is \c nullptr
     */
    constexpr size_type find_first_of(const value_type* s, size_type pos, size_type n) const;

    /**
     * Finds the first character equal to one of the characters in null-terminated string \p s. The search begins at
     * \p pos.
     *
     * @param s String containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the first found character, or \c npos if no character is found.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr size_type find_first_of(const value_type* s, size_type pos = 0) const;

    /**
     * Finds the first character equal to \p c. The search begins at \p pos.
     *
     * @param c Character to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the first found character, or \c npos if no character is found.
     */
    constexpr size_type find_first_of(value_type c, size_type pos = 0) const noexcept;

    /**
     * Finds the first character equal to one of the characters in string \p str. The search begins at \p pos.
     *
     * @param str String containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the first found character, or \c npos if no character is found.
     */
    CARB_CPP20_CONSTEXPR size_type find_first_of(const std::string& str, size_type pos = 0) const noexcept;

    /**
     * Finds the first character equal to one of the characters in string \p sv. The search begins at \p pos.
     *
     * @param sv String view containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the first found character, or \c npos if no character is found.
     */
    constexpr size_type find_first_of(const carb::cpp17::string_view& sv, size_type pos = 0) const noexcept;

#if CARB_HAS_CPP17
    /**
     * Implicitly converts @p t to type `std::string_view` and finds the first character equal to one of the characters
     * in that string view. The search begins at \p pos. This overload participates in overload resolution only if
     * `std::is_convertible_v<const T&, std::string_view>` is true.
     *
     * @param t Object that can be converted to `std::string_view` containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    constexpr size_type find_first_of(const T& t, size_type pos = 0) const noexcept;
#endif

    /**
     * Finds the last character equal to one of the characters in string \p str. The search begins at \p pos. If
     * `pos == npos` or `pos >= size()`, the whole string is searched.
     *
     * @param str String containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the last found character, or \c npos if no character is found.
     */
    constexpr size_type find_last_of(const string& str, size_type pos = npos) const noexcept;

    /**
     * Finds the last character equal to one of the characters in the first \p n characters of string \p s. The search
     * begins at \p pos. If `pos == npos` or `pos >= size()`, the whole string is searched.
     *
     * @param s String containing the characters to search for.
     * @param pos Position to begin the search.
     * @param n Number of characters in \p s to search for.
     *
     * @return Position of the last found character, or \c npos if no character is found.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr size_type find_last_of(const value_type* s, size_type pos, size_type n) const;

    /**
     * Finds the last character equal to one of the characters in the null-terminated string \p s. The search
     * begins at \p pos. If `pos == npos` or `pos >= size()`, the whole string is searched.
     *
     * @param s String containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the last found character, or \c npos if no character is found.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr size_type find_last_of(const value_type* s, size_type pos = npos) const;

    /**
     * Finds the last character equal to \p c. The search begins at \p pos. If `pos == npos` or `pos >= size()`,
     * the whole string is searched.
     *
     * @param c Character to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the last found character, or \c npos if no character is found.
     */
    constexpr size_type find_last_of(value_type c, size_type pos = npos) const noexcept;

    /**
     * Finds the last character equal to one of the characters in string \p str. The search begins at \p pos. If
     * `pos == npos` or `pos >= size()`, the whole string is searched.
     *
     * @param str String containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the last found character, or \c npos if no character is found.
     */
    CARB_CPP20_CONSTEXPR size_type find_last_of(const std::string& str, size_type pos = npos) const noexcept;

    /**
     * Finds the last character equal to one of the characters in string view \p sv. The search begins at \p pos. If
     * `pos == npos` or `pos >= size()`, the whole string is searched.
     *
     * @param sv String view containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the last found character, or \c npos if no character is found.
     */
    constexpr size_type find_last_of(const carb::cpp17::string_view& sv, size_type pos = npos) const noexcept;

#if CARB_HAS_CPP17
    /**
     * Implicitly converts @p t to type `std::string_view` and finds the last character equal to one of the characters
     * in that string view. The search begins at \p pos. If `pos == npos` or `pos >= size()`, the whole string is
     * searched. The search begins at \p pos. This overload participates in overload resolution only if
     * `std::is_convertible_v<const T&, std::string_view>` is true.
     *
     * @param t Object that can be converted to `std::string_view` containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    constexpr size_type find_last_of(const T& t, size_type pos = npos) const noexcept;
#endif

    /**
     * Finds the first character not equal to one of the characters in string \p str. The search begins at \p pos.
     *
     * @param str String containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the first found character, or \c npos if no character is found.
     */
    constexpr size_type find_first_not_of(const string& str, size_type pos = 0) const noexcept;

    /**
     * Finds the first character not equal to one of the characters in the first \p n characters of string \p s. The
     * search begins at \p pos.
     *
     * @param s String containing the characters to search for.
     * @param pos Position to begin the search.
     * @param n Number of characters in \p s to search for.
     *
     * @return Position of the first found character, or \c npos if no character is found.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr size_type find_first_not_of(const value_type* s, size_type pos, size_type n) const;

    /**
     * Finds the first character not equal to one of the characters in null-terminated string \p s. The search begins at
     * \p pos.
     *
     * @param s String containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the first found character, or \c npos if no character is found.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr size_type find_first_not_of(const value_type* s, size_type pos = 0) const;

    /**
     * Finds the first character equal to \p c. The search begins at \p pos.
     *
     * @param c Character to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the first found character, or \c npos if no character is found.
     */
    constexpr size_type find_first_not_of(value_type c, size_type pos = 0) const noexcept;

    /**
     * Finds the first character not equal to one of the characters in string \p str. The search begins at \p pos.
     *
     * @param str String containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the first found character, or \c npos if no character is found.
     */
    CARB_CPP20_CONSTEXPR size_type find_first_not_of(const std::string& str, size_type pos = 0) const noexcept;

    /**
     * Finds the first character not equal to one of the characters in string view \p sv. The search begins at \p pos.
     *
     * @param sv String view containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the first found character, or \c npos if no character is found.
     */
    constexpr size_type find_first_not_of(const carb::cpp17::string_view& sv, size_type pos = 0) const noexcept;

#if CARB_HAS_CPP17
    /**
     * Implicitly converts @p t to type `std::string_view` and finds the first character not equal to one of the
     * characters in that string view. The search begins at \p pos. This overload participates in overload resolution
     * only if `std::is_convertible_v<const T&, std::string_view>` is true.
     *
     * @param t Object that can be converted to `std::string_view` containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    constexpr size_type find_first_not_of(const T& t, size_type pos = 0) const noexcept;
#endif

    /**
     * Finds the last character not equal to one of the characters in string \p str. The search begins at \p pos. If
     * `pos == npos` or `pos >= size()`, the whole string is searched.
     *
     * @param str String containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the last found character, or \c npos if no character is found.
     */
    constexpr size_type find_last_not_of(const string& str, size_type pos = npos) const noexcept;

    /**
     * Finds the last character not equal to one of the characters in the first \p n characters of string \p s. The
     * search begins at \p pos. If `pos == npos` or `pos >= size()`, the whole string is searched.
     *
     * @param s String containing the characters to search for.
     * @param pos Position to begin the search.
     * @param n Number of characters in \p s to search for.
     *
     * @return Position of the last found character, or \c npos if no character is found.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr size_type find_last_not_of(const value_type* s, size_type pos, size_type n) const;

    /**
     * Finds the last character not equal to one of the characters in the null-terminated string \p s. The
     * search begins at \p pos. If `pos == npos` or `pos >= size()`, the whole string is searched.
     *
     * @param s String containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the last found character, or \c npos if no character is found.
     *
     * @throws std::invalid_argument if \p s is \c nullptr.
     */
    constexpr size_type find_last_not_of(const value_type* s, size_type pos = npos) const;

    /**
     * Finds the last character not equal to \p c. The search begins at \p pos. If `pos == npos` or
     * `pos >= size()`, the whole string is searched.
     *
     * @param c Character to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the last found character, or \c npos if no character is found.
     */
    constexpr size_type find_last_not_of(value_type c, size_type pos = npos) const noexcept;

    /**
     * Finds the last character not equal to one of the characters in string \p str. The search begins at \p pos. If
     * `pos == npos` or `pos >= size()`, the whole string is searched.
     *
     * @param str String containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the last found character, or \c npos if no character is found.
     */
    CARB_CPP20_CONSTEXPR size_type find_last_not_of(const std::string& str, size_type pos = npos) const noexcept;

    /**
     * Finds the last character not equal to one of the characters in string view \p sv. The search begins at \p pos. If
     * `pos == npos` or `pos >= size()`, the whole string is searched.
     *
     * @param sv String view containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the last found character, or \c npos if no character is found.
     */
    constexpr size_type find_last_not_of(const carb::cpp17::string_view& sv, size_type pos = npos) const noexcept;

#if CARB_HAS_CPP17
    /**
     * Implicitly converts @p t to type `std::string_view` and finds the last character not equal to one of the
     * characters in that string view. The search begins at \p pos. If `pos == npos` or `pos >= size()`, the
     * whole string is searched. This overload participates in overload resolution only if `std::is_convertible_v<const
     * T&, std::string_view>` is true.
     *
     * @param t Object that can be converted to `std::string_view` containing the characters to search for.
     * @param pos Position to begin the search.
     *
     * @return Position of the first character of the matching substring, or \c npos if no such substring exists.
     */
    template <typename T, typename = detail::is_sv_convertible<T>>
    constexpr size_type find_last_not_of(const T& t, size_type pos = npos) const noexcept;
#endif

private:
    // Size of the character buffer for small string optimization
    constexpr static size_type kSMALL_STRING_SIZE = 32;

    // The last byte of the SSO buffer contains the remaining size in the buffer
    constexpr static size_type kSMALL_SIZE_OFFSET = kSMALL_STRING_SIZE - 1;

    // Sentinel value indicating that the string is using heap allocated storage
    constexpr static value_type kSTRING_IS_ALLOCATED = std::numeric_limits<char>::max();

    // Struct that holds the data for an allocated string. This could be an anonymous struct inside the union,
    // however naming it outside the union allows for the static_asserts below for ABI safety.
    struct allocated_data
    {
        CARB_VIZ pointer m_ptr;
        CARB_VIZ size_type m_size;
        CARB_VIZ size_type m_capacity;
    };

    union
    {
        CARB_VIZ allocated_data m_allocated_data;

        /**
         * Local buffer for small string optimization. When the string is small enough to fit in the local buffer, the
         * last byte is the size remaining in the buffer. This has the advantage of when the local buffer is full, the
         * size remaining will be 0, which acts as the NUL terminator for the local string. When the string is larger
         * than the buffer, the last byte will be m_sentinel_value, indicated the string is allocated.
         */
        CARB_VIZ value_type m_local_data[kSMALL_STRING_SIZE];
    };

    static_assert(kSMALL_STRING_SIZE == 32, "ABI-safety: Cannot change the small string optimization size");
    static_assert(size_type(kSTRING_IS_ALLOCATED) >= kSMALL_STRING_SIZE,
                  "Invalid assumption: Sentinel Value must be greater than max small string size.");
    static_assert(sizeof(allocated_data) == 24, "ABI-safety: Cannot change allocated data size");
    static_assert(offsetof(allocated_data, m_ptr) == 0, "ABI-safety: Member offset cannot change");
    static_assert(offsetof(allocated_data, m_size) == 8, "ABI-safety: Member offset cannot change");
    static_assert(offsetof(allocated_data, m_capacity) == 16, "ABI-safety: Member offset cannot change");
    static_assert(sizeof(allocated_data) < kSMALL_STRING_SIZE,
                  "Invalid assumption: sizeof(allocated_data) must be less than the small string size.");

    // Helper functions
    constexpr bool is_local() const;

    constexpr void set_local(size_type new_size) noexcept;

    constexpr void set_allocated() noexcept;

    constexpr reference get_reference(size_type pos) noexcept;

    constexpr const_reference get_reference(size_type pos) const noexcept;

    constexpr pointer get_pointer(size_type pos) noexcept;

    constexpr const_pointer get_pointer(size_type pos) const noexcept;

    constexpr void set_empty() noexcept;

    constexpr void range_check(size_type pos, size_type size, const char* function) const;

    // Checks that pos is within the range [begin(), end()]
    constexpr void range_check(const_iterator pos, const char* function) const;

    // Checks that first is within the range [begin(), end()], that last is within the range [begin(), end()], and that
    // first <= last.
    constexpr void range_check(const_iterator first, const_iterator last, const char* function) const;

    // Checks if current+n > max_size(). If it is not, returns current+n.
    constexpr size_type length_check(size_type current, size_type n, const char* function) const;

    constexpr void set_size(size_type new_size) noexcept;

    constexpr bool should_allocate(size_type n) const noexcept;

    constexpr bool overlaps_this_string(const_pointer s) const noexcept;

    void overlap_check(const_pointer s) const;

    // Calls std::vsnprintf, but throws if it fails
    size_type vsnprintf_check(char* buffer, size_type buffer_size, const char* format, va_list args);
    template <class... Args>
    size_type snprintf_check(char* buffer, size_type buffer_size, const char* format, Args&&... args);

    void allocate_if_necessary(size_type size);

    void initialize(const_pointer src, size_type size);

    template <typename InputIterator>
    void initialize(InputIterator begin, InputIterator end, size_type size);

    void dispose();

    pointer allocate_buffer(size_type old_capacity, size_type& new_capacity);

    void grow_buffer_to(size_type new_capacity);

    // Grows a buffer to new_size, fills it with the data from the three provided pointers, and swaps the new buffer
    // for the old one. This is used in functions like insert and replace, which may need to fill the new buffer with
    // characters from multiple locations.
    void grow_buffer_and_fill(
        size_type new_size, const_pointer p1, size_type s1, const_pointer p2, size_type s2, const_pointer p3, size_type s3);

    template <class InputIterator>
    void grow_buffer_and_append(size_type new_size, InputIterator first, InputIterator last);

    // Internal implementations
    string& assign_internal(const_pointer src, size_type new_size);

    template <typename InputIterator>
    string& assign_internal(InputIterator begin, InputIterator end, size_type new_size);

    string& insert_internal(size_type pos, value_type c, size_type n);

    string& insert_internal(size_type pos, const_pointer src, size_type n);

    string& append_internal(const_pointer src, size_type n);

    constexpr int compare_internal(const_pointer this_str,
                                   const_pointer other_str,
                                   size_type this_size,
                                   size_type other_size) const noexcept;

    void replace_setup(size_type pos, size_type replaced_size, size_type replacement_size);
};

static_assert(std::is_standard_layout<string>::value, "string must be standard layout");
static_assert(sizeof(string) == 32, "ABI Safety: String must be 32 bytes");

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs String the comes first in the new string.
 * @param rhs String that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(const string& lhs, const string& rhs);

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs String the comes first in the new string.
 * @param rhs String that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(const string& lhs, const char* rhs);

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs String the comes first in the new string.
 * @param rhs String that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::invalid_argument if \p rhs is \c nullptr.
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(const string& lhs, char rhs);

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs String the comes first in the new string.
 * @param rhs String that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(const string& lhs, const std::string& rhs);

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs String the comes first in the new string.
 * @param rhs String that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::invalid_argument if \p lhs is \c nullptr.
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(const char* lhs, const string& rhs);

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs Character the comes first in the new string.
 * @param rhs String that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(char lhs, const string& rhs);

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs String the comes first in the new string.
 * @param rhs String that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(const std::string& lhs, const string& rhs);

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs String the comes first in the new string.
 * @param rhs String that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(string&& lhs, string&& rhs);

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs String the comes first in the new string.
 * @param rhs String that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(string&& lhs, const string& rhs);

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs String the comes first in the new string.
 * @param rhs String that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::invalid_argument if \p rhs is \c nullptr.
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(string&& lhs, const char* rhs);

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs String the comes first in the new string.
 * @param rhs Character that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(string&& lhs, char rhs);

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs String the comes first in the new string.
 * @param rhs String that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(string&& lhs, const string& rhs);

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs String the comes first in the new string.
 * @param rhs String that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(const string& lhs, string&& rhs);

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs String the comes first in the new string.
 * @param rhs String that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::invalid_argument if \p lhs is \c nullptr.
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(const char* lhs, string&& rhs);

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs Character the comes first in the new string.
 * @param rhs String that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(char lhs, string&& rhs);

/**
 * Creates a new string by concatenating \p lhs and \p rhs.
 *
 * @param lhs String the comes first in the new string.
 * @param rhs String that comes second in the new string.
 *
 * @return A new string containing the characters from \p lhs followed by the characters from \p rhs.
 *
 * @throws std::length_error if the resulting string would be being larger than max_size().
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string operator+(const std::string& lhs, string&& rhs);

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs and \p rhs are equal, false otherwise.
 */
constexpr bool operator==(const string& lhs, const string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs and \p rhs are equal, false otherwise.
 *
 * @throws std::invalid_argument if \p rhs is \c nullptr.
 */
constexpr bool operator==(const string& lhs, const char* rhs);

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs and \p rhs are equal, false otherwise.
 *
 * @throws std::invalid_argument if \p lhs is \c nullptr.
 */
constexpr bool operator==(const char* lhs, const string& rhs);

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs and \p rhs are equal, false otherwise.
 */
CARB_CPP20_CONSTEXPR bool operator==(const string& lhs, const std::string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs and \p rhs are equal, false otherwise.
 */
CARB_CPP20_CONSTEXPR bool operator==(const std::string& lhs, const string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs and \p rhs are not equal, false otherwise.
 */
constexpr bool operator!=(const string& lhs, const string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs and \p rhs are not equal, false otherwise.
 *
 * @throws std::invalid_argument if \p rhs is \c nullptr.
 */
constexpr bool operator!=(const string& lhs, const char* rhs);

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs and \p rhs are not equal, false otherwise.
 *
 * @throws std::invalid_argument if \p lhs is \c nullptr.
 */
constexpr bool operator!=(const char* lhs, const string& rhs);

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs and \p rhs are not equal, false otherwise.
 */
CARB_CPP20_CONSTEXPR bool operator!=(const string& lhs, const std::string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs and \p rhs are not equal, false otherwise.
 */
CARB_CPP20_CONSTEXPR bool operator!=(const std::string& lhs, const string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is less than \p rhs, false otherwise.
 */
constexpr bool operator<(const string& lhs, const string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is less than \p rhs, false otherwise.
 *
 * @throws std::invalid_argument if \p rhs is \c nullptr.
 */
constexpr bool operator<(const string& lhs, const char* rhs);

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is less than \p rhs, false otherwise.
 *
 * @throws std::invalid_argument if \p lhs is \c nullptr.
 */
constexpr bool operator<(const char* lhs, const string& rhs);

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is less than \p rhs, false otherwise.
 */
CARB_CPP20_CONSTEXPR bool operator<(const string& lhs, const std::string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is less than \p rhs, false otherwise.
 */
CARB_CPP20_CONSTEXPR bool operator<(const std::string& lhs, const string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is less than or equal to \p rhs, false otherwise.
 */
constexpr bool operator<=(const string& lhs, const string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is less than or equal to \p rhs, false otherwise.
 *
 * @throws std::invalid_argument if \p rhs is \c nullptr.
 */
constexpr bool operator<=(const string& lhs, const char* rhs);

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is less than or equal to \p rhs, false otherwise.
 *
 * @throws std::invalid_argument if \p lhs is \c nullptr.
 */
constexpr bool operator<=(const char* lhs, const string& rhs);

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is less than or equal to \p rhs, false otherwise.
 */
CARB_CPP20_CONSTEXPR bool operator<=(const string& lhs, const std::string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is less than or equal to \p rhs, false otherwise.
 */
CARB_CPP20_CONSTEXPR bool operator<=(const std::string& lhs, const string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is greater than \p rhs, false otherwise.
 */
constexpr bool operator>(const string& lhs, const string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is greater than \p rhs, false otherwise.
 *
 * @throws std::invalid_argument if \p rhs is \c nullptr.
 */
constexpr bool operator>(const string& lhs, const char* rhs);

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is greater than \p rhs, false otherwise.
 *
 * @throws std::invalid_argument if \p lhs is \c nullptr.
 */
constexpr bool operator>(const char* lhs, const string& rhs);

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is greater than \p rhs, false otherwise.
 */
CARB_CPP20_CONSTEXPR bool operator>(const string& lhs, const std::string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is greater than \p rhs, false otherwise.
 */
CARB_CPP20_CONSTEXPR bool operator>(const std::string& lhs, const string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is greater than or equal to \p rhs, false otherwise.
 */
constexpr bool operator>=(const string& lhs, const string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is greater than or equal to \p rhs, false otherwise.
 *
 * @throws std::invalid_argument if \p rhs is \c nullptr.
 */
constexpr bool operator>=(const string& lhs, const char* rhs);

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is greater than or equal to \p rhs, false otherwise.
 *
 * @throws std::invalid_argument if \p lhs is \c nullptr.
 */
constexpr bool operator>=(const char* lhs, const string& rhs);

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is greater than or equal to \p rhs, false otherwise.
 */
CARB_CPP20_CONSTEXPR bool operator>=(const string& lhs, const std::string& rhs) noexcept;

/**
 * Compares \p lhs and \p rhs. All comparisons are done lexicographically using omni::string::compare().
 *
 * @param lhs Left hand side of the comparison.
 * @param rhs Right hand side of the comparison.
 *
 * @return true if \p lhs is greater than or equal to \p rhs, false otherwise.
 */
CARB_CPP20_CONSTEXPR bool operator>=(const std::string& lhs, const string& rhs) noexcept;

/**
 * Swaps \p lhs and \p rhs via `lhs.swap(rhs)`.
 *
 * @param lhs String to swap.
 * @param rhs String to swap.
 *
 * @return none.
 */
void swap(string& lhs, string& rhs) noexcept;

/**
 * Erases all instances of \p val from \p str.
 *
 * @param str String to erase from.
 * @param val Character to erase.
 *
 * @return The number of characters erased.
 */
template <typename U>
CARB_CPP20_CONSTEXPR string::size_type erase(string& str, const U& val);

/**
 * Erases all elements of \p str the satisfy \p pred.
 *
 * @param str String to erase from.
 * @param pred Predicate to check characters with.
 *
 * @return The number of characters erased.
 */
template <typename Pred>
CARB_CPP20_CONSTEXPR string::size_type erase_if(string& str, Pred pred);

/**
 * Output stream operator overload. Outputs the contents of \p str to the stream \p os.
 *
 * @param os Stream to output the string to.
 * @param str The string to output.
 *
 * @return \p os.
 *
 * @throws std::ios_base::failure if an exception is thrown during output.
 */
std::basic_ostream<char, std::char_traits<char>>& operator<<(std::basic_ostream<char, std::char_traits<char>>& os,
                                                             const string& str);

/**
 * Input stream operator overload. Extracts a string from \p is into \p str.
 *
 * @param is Stream to get input from.
 * @param str The string to put input into.
 *
 * @return \p is.
 *
 * @throws std::ios_base::failure if no characters are extracted or an exception is thrown during input.
 */
std::basic_istream<char, std::char_traits<char>>& operator>>(std::basic_istream<char, std::char_traits<char>>& is,
                                                             string& str);

/**
 * Reads characters from the input stream \p input and places them in the string \p str. Characters are read until
 * end-of-file is reached on \p input, the next character in the input is \p delim, or max_size() characters have
 * been extracted.
 *
 * @param input Stream to get input from.
 * @param str The string to put input into.
 * @param delim Character that indicates that extraction should end.
 *
 * @return \p input.
 */
std::basic_istream<char, std::char_traits<char>>& getline(std::basic_istream<char, std::char_traits<char>>&& input,
                                                          string& str,
                                                          char delim);

/**
 * Reads characters from the input stream \p input and places them in the string \p str. Characters are read until
 * end-of-file is reached on \p input, the next character in the input it \c '\\n', or max_size() characters have
 * been extracted.
 *
 * @param input Stream to get input from.
 * @param str The string to put input into.
 *
 * @return \p input.
 */
std::basic_istream<char, std::char_traits<char>>& getline(std::basic_istream<char, std::char_traits<char>>&& input,
                                                          string& str);

/**
 * Interprets the string \p str as a signed integer value.
 *
 * @param str The string to convert.
 * @param pos Address of an integer to store the number of characters processed.
 * @param base The number base.
 *
 * @return Integer value corresponding to the contents of \p str.
 *
 * @throws std::invalid_argument If no conversion could be performed.
 * @throws std::out_of_range If the converted value would fall out of the range of the result type or if the
 *         underlying function (\c std::strtol) sets \c errno to \c ERANGE.
 */
int stoi(const string& str, std::size_t* pos = nullptr, int base = 10);

/**
 * Interprets the string \p str as a signed integer value.
 *
 * @param str The string to convert.
 * @param pos Address of an integer to store the number of characters processed.
 * @param base The number base.
 *
 * @return Integer value corresponding to the contents of \p str.
 *
 * @throws std::invalid_argument If no conversion could be performed.
 * @throws std::out_of_range If the converted value would fall out of the range of the result type or if the
 *         underlying function (\c std::strtol) sets \c errno to \c ERANGE.
 */
long stol(const string& str, std::size_t* pos = nullptr, int base = 10);

/**
 * Interprets the string \p str as a signed integer value.
 *
 * @param str The string to convert.
 * @param pos Address of an integer to store the number of characters processed.
 * @param base The number base.
 *
 * @return Integer value corresponding to the contents of \p str.
 *
 * @throws std::invalid_argument If no conversion could be performed.
 * @throws std::out_of_range If the converted value would fall out of the range of the result type or if the
 *         underlying function (\c std::strtoll) sets \c errno to \c ERANGE.
 */
long long stoll(const string& str, std::size_t* pos = nullptr, int base = 10);

/**
 * Interprets the string \p str as a unsigned integer value.
 *
 * @param str The string to convert.
 * @param pos Address of an integer to store the number of characters processed.
 * @param base The number base.
 *
 * @return Unsigned Integer value corresponding to the contents of \p str.
 *
 * @throws std::invalid_argument If no conversion could be performed.
 * @throws std::out_of_range If the converted value would fall out of the range of the result type or if the
 *         underlying function (\c std::strtoul) sets \c errno to \c ERANGE.
 */
unsigned long stoul(const string& str, std::size_t* pos = nullptr, int base = 10);

/**
 * Interprets the string \p str as a unsigned integer value.
 *
 * @param str The string to convert.
 * @param pos Address of an integer to store the number of characters processed.
 * @param base The number base.
 *
 * @return Unsigned Integer value corresponding to the contents of \p str.
 *
 * @throws std::invalid_argument If no conversion could be performed.
 * @throws std::out_of_range If the converted value would fall out of the range of the result type or if the
 *         underlying function (\c std::strtoull) sets \c errno to \c ERANGE.
 */
unsigned long long stoull(const string& str, std::size_t* pos = nullptr, int base = 10);

/**
 * Interprets the string \p str as a floating point value.
 *
 * @param str The string to convert.
 * @param pos Address of an integer to store the number of characters processed.
 *
 * @return Floating point value corresponding to the contents of \p str.
 *
 * @throws std::invalid_argument If no conversion could be performed.
 * @throws std::out_of_range If the converted value would fall out of the range of the result type or if the
 *         underlying function (\c std::strtof) sets \c errno to \c ERANGE.
 */
float stof(const string& str, std::size_t* pos = nullptr);

/**
 * Interprets the string \p str as a floating point value.
 *
 * @param str The string to convert.
 * @param pos Address of an integer to store the number of characters processed.
 *
 * @return Floating point value corresponding to the contents of \p str.
 *
 * @throws std::invalid_argument If no conversion could be performed.
 * @throws std::out_of_range If the converted value would fall out of the range of the result type or if the
 *         underlying function (\c std::strtod) sets \c errno to \c ERANGE.
 */
double stod(const string& str, std::size_t* pos = nullptr);

/**
 * Interprets the string \p str as a floating point value.
 *
 * @param str The string to convert.
 * @param pos Address of an integer to store the number of characters processed.
 *
 * @return Floating point value corresponding to the contents of \p str.
 *
 * @throws std::invalid_argument If no conversion could be performed.
 * @throws std::out_of_range If the converted value would fall out of the range of the result type or if the
 *         underlying function (\c std::strtold) sets \c errno to \c ERANGE.
 */
long double stold(const string& str, std::size_t* pos = nullptr);

/**
 * Converts the numerical value \p value to a string.
 *
 * @param value Value to convert.
 *
 * @return A string representing the value.
 *
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string to_string(int value);

/**
 * Converts the numerical value \p value to a string.
 *
 * @param value Value to convert.
 *
 * @return A string representing the value.
 *
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string to_string(long value);

/**
 * Converts the numerical value \p value to a string.
 *
 * @param value Value to convert.
 *
 * @return A string representing the value.
 *
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string to_string(long long value);

/**
 * Converts the numerical value \p value to a string.
 *
 * @param value Value to convert.
 *
 * @return A string representing the value.
 *
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string to_string(unsigned value);

/**
 * Converts the numerical value \p value to a string.
 *
 * @param value Value to convert.
 *
 * @return A string representing the value.
 *
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string to_string(unsigned long value);

/**
 * Converts the numerical value \p value to a string.
 *
 * @param value Value to convert.
 *
 * @return A string representing the value.
 *
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string to_string(unsigned long long value);

/**
 * Converts the numerical value \p value to a string.
 *
 * @param value Value to convert.
 *
 * @return A string representing the value.
 *
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string to_string(float value);

/**
 * Converts the numerical value \p value to a string.
 *
 * @param value Value to convert.
 *
 * @return A string representing the value.
 *
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string to_string(double value);

/**
 * Converts the numerical value \p value to a string.
 *
 * @param value Value to convert.
 *
 * @return A string representing the value.
 *
 * @throws Allocation This function may throw any exception thrown during allocation.
 */
string to_string(long double value);

} // namespace omni

namespace std
{

/**
 * Hash specialization for std::string
 */
template <>
struct hash<omni::string>
{
    /** Argument type alias. */
    using argument_type = omni::string;
    /** Result type alias. */
    using result_type = std::size_t;
    /** Hash operator */
    size_t operator()(const argument_type& x) const noexcept
    {
        return carb::hashBuffer(x.data(), x.size());
    }
};
} // namespace std

#include "String.inl"

#pragma pop_macro("max")

CARB_IGNOREWARNING_MSC_POP
