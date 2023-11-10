// Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//! @brief Implementation of select components from C++20 `<span>` library
#pragma once

#include "../Defines.h"
#include "TypeTraits.h"
#include "ImplData.h"
#include "../cpp17/StdDef.h"
#include "../../omni/detail/PointerIterator.h"

#include <array>

namespace carb
{
namespace cpp20
{

//! A constant of type size_t that is used to differentiate carb::cpp20::span of static and dynamic extent
//! @see span
constexpr size_t dynamic_extent = size_t(-1);

//! \cond DEV
namespace detail
{

template <class T>
constexpr T* to_address(T* p) noexcept
{
    static_assert(!std::is_function<T>::value, "Ill-formed if T is function type");
    return p;
}

template <class Ptr, std::enable_if_t<std::is_pointer<decltype(std::declval<Ptr>().operator->())>::value, bool> = false>
constexpr auto to_address(const Ptr& p) noexcept
{
    return to_address(p.operator->());
}

template <size_t Extent, size_t Offset, size_t Count>
struct DetermineSubspanExtent
{
    constexpr static size_t value = Count;
};

template <size_t Extent, size_t Offset>
struct DetermineSubspanExtent<Extent, Offset, dynamic_extent>
{
    constexpr static size_t value = Extent - Offset;
};

template <size_t Offset>
struct DetermineSubspanExtent<dynamic_extent, Offset, dynamic_extent>
{
    constexpr static size_t value = dynamic_extent;
};

// GCC instantiates some functions always so they cannot use static_assert, but throwing an exception is allowed from a
// constexpr function (which will act as a compile failure if constexpr) so fall back to that.
#if CARB_EXCEPTIONS_ENABLED
#    define CARB_ALWAYS_FAIL(msg) throw std::out_of_range(msg)
#    define CARB_THROW_OR_CHECK(check, msg)                                                                            \
        if (!CARB_LIKELY(check))                                                                                       \
        throw std::out_of_range(msg)
#else
#    define CARB_THROW_OR_CHECK(check, msg) CARB_CHECK((check), msg)
#    if CARB_COMPILER_MSC
#        define CARB_ALWAYS_FAIL(msg) static_assert(false, msg)
#    else
#        define CARB_ALWAYS_FAIL(msg) CARB_FATAL_UNLESS(false, msg)
#    endif
#endif

} // namespace detail
//! \endcond

/**
 * An object that refers to a contiguous sequence of objects.
 *
 * The class template span describes an object that can refer to a contiguous sequence of objects with the first element
 * of the sequence at position zero. A span can either have a \a static extent, in which case the number of elements in
 * the sequence is known at compile-time and encoded in the type, or a \a dynamic extent.
 *
 * If a span has \a dynamic extent, this implementation holds two members: a pointer to T and a size. A span with
 * \a static extent has only one member: a pointer to T.
 *
 * Every specialization of \c span is a \a TriviallyCopyable type.
 *
 * This implementation of \c span is a C++14-compatible implementation of the
 * [C++20 std::span](https://en.cppreference.com/w/cpp/container/span), as such certain constructors that involve
 * `ranges` or `concepts` are either not implemented or are implemented using C++14 paradigms.
 *
 * This implementation of \c span is a guaranteed @rstref{ABI- and interop-safe <abi-compatibility>} type.
 *
 * @note For function definitions below the following definitions are used: An ill-formed program will generate a
 *   compiler error via `static_assert`. Undefined behavior is typically signaled by throwing a `std::out_of_range`
 *   exception as this is allowed for `constexpr` functions to cause a compiler error, however, if exceptions are
 *   disabled a \ref CARB_CHECK will occur (this also disables `constexpr` as \ref CARB_CHECK is not `constexpr`).
 * @tparam T Element type; must be a complete object type that is not an abstract class type
 * @tparam Extent The number of elements in the sequence, or \ref dynamic_extent if dynamic
 */
template <class T, size_t Extent = dynamic_extent>
class span
{
public:
    using element_type = T; //!< The element type `T`
    using value_type = std::remove_cv_t<T>; //!< The value type; const/volatile is removed from `T`
    using size_type = std::size_t; //!< The size type `size_t`
    using difference_type = std::ptrdiff_t; //!< The difference type `ptrdiff_t`
    using pointer = T*; //!< The pointer type `T*`
    using const_pointer = const T*; //!< The const pointer type `const T*`
    using reference = T&; //!< The reference type `T&`
    using const_reference = const T&; //!< The const reference type `const T&`

    //! The iterator type \ref omni::detail::PointerIterator
    using iterator = omni::detail::PointerIterator<pointer, span>;
    //! The reverse iterator type `std::reverse_iterator<iterator>`
    using reverse_iterator = std::reverse_iterator<iterator>;

    //! The number of elements in the sequence, or \ref dynamic_extent if dynamic
    constexpr static std::size_t extent = Extent;

#ifdef DOXYGEN_BUILD
    //! Default constructor
    //!
    //! Constructs an empty span whose \ref data() is `nullptr` and \ref size() is `0`. This constructor participates in
    //! overload resolution only if `extent == 0 || extent == dynamic_extent`.
    constexpr span() noexcept
    {
    }
#endif

    //! Constructs a \ref span that is a view over the range `[first, first + count)`.
    //!
    //! Behavior is undefined if `count != extent` (for static extent) or \p first does not model `contiguous_iterator`.
    //! However, since `contiguous_iterator` is not available until C++20, instead this function does not participate in
    //! overload resolution unless `std::iterator_traits<It>::iterator_category == std::random_access_iterator_tag`.
    //! @param first An iterator or pointer type that models C++20 `contiguous_iterator`
    //! @param count The number of elements from \p first to include in the span. If `extent != dynamic_extent` then
    //!   this must match \ref extent.
    template <class It,
              std::enable_if_t<std::is_same<typename std::iterator_traits<It>::iterator_category, std::random_access_iterator_tag>::value,
                               bool> = false>
    constexpr explicit span(It first, size_type count)
    {
        CARB_THROW_OR_CHECK(extent == count, "Undefined if count != extent");
        m_p = detail::to_address(first);
    }

    //! Constructs a \ref span that is a view over the range `[first, last)`.
    //!
    //! Behavior is undefined if `(last - first) != extent` (for static extent) or if `[first, last)` does not represent
    //! a contiguous range. This function differs significantly from the C++20 definition since the concepts of
    //! `contiguous_iterator` and `sized_sentinel_for` are not available. Since these concepts are not available until
    //! C++20, instead this function does not participate in overload resolution unless
    //! `std::iterator_traits<It>::iterator_category == std::random_access_iterator_tag`. Also \p first and \p last must
    //! be a matching iterator type.
    //! @param first An iterator or pointer type that represents the first element in the span.
    //! @param last An iterator or pointer type that represents the past-the-end element in the span.
    template <class It,
              std::enable_if_t<std::is_same<typename std::iterator_traits<It>::iterator_category, std::random_access_iterator_tag>::value,
                               bool> = false>
    constexpr explicit span(It first, It last)
    {
        CARB_THROW_OR_CHECK((last - first) == extent, "Undefined if (last - first) != extent");
        m_p = detail::to_address(first);
    }

    //! Constructs a span that is a view over an array.
    //!
    //! Behavior is undefined if `extent != dynamic_extent && N != extent`.
    //! @param arr The array to view
    template <std::size_t N>
    constexpr span(type_identity_t<element_type> (&arr)[N]) noexcept
    {
        static_assert(N == extent, "Undefined if N != extent");
        m_p = cpp20::data(arr);
    }

    //! Constructs a span that is a view over an array.
    //!
    //! Behavior is undefined if `extent != dynamic_extent && N != extent`.
    //! @param arr The array to view
    template <class U, std::size_t N, std::enable_if_t<std::is_convertible<U, element_type>::value, bool> = false>
    constexpr span(std::array<U, N>& arr) noexcept
    {
        static_assert(N == extent, "Undefined if N != extent");
        m_p = cpp20::data(arr);
    }

    //! Constructs a span that is a view over an array.
    //!
    //! Behavior is undefined if `extent != dynamic_extent && N != extent`.
    //! @param arr The array to view
    template <class U, std::size_t N, std::enable_if_t<std::is_convertible<U, element_type>::value, bool> = false>
    constexpr span(const std::array<U, N>& arr) noexcept
    {
        static_assert(N == extent, "Undefined if N != extent");
        m_p = cpp20::data(arr);
    }

    // template< class R >
    // explicit(extent != dynamic_extent)
    // constexpr span( R&& range );
    // (Constructor not available without ranges)

#ifndef DOXYGEN_BUILD
    template <class U,
              std::size_t N,
              std::enable_if_t<N == dynamic_extent && std::is_convertible<U, element_type>::value, bool> = false>
    constexpr explicit span(const span<U, N>& source) noexcept : m_p(source.data())
    {
        CARB_CHECK(source.size() == extent); // specified as `noexcept` so we cannot throw
    }

    template <class U,
              std::size_t N,
              std::enable_if_t<N != dynamic_extent && std::is_convertible<U, element_type>::value, bool> = false>
#else
    //! Converting constructor from another span.
    //!
    //! Behavior is undefined if `extent != dynamic_extent && source.size() != extent`.
    //! @tparam U `element_type` of \p source; must be convertible to `element_type`
    //! @tparam N `extent` of \p source
    //! @param source The span to convert from
    template <class U, std::size_t N>
#endif
    constexpr span(const span<U, N>& source) noexcept : m_p(source.data())
    {
        static_assert(N == extent, "Undefined if N != extent");
    }

    //! Copy constructor
    //! @param other A span to copy from
    constexpr span(const span& other) noexcept = default;

    //! Assignment operator
    //!
    //! @param other A span to copy from
    //! @returns *this
    constexpr span& operator=(const span& other) noexcept = default;

    //! Returns an iterator to the first element of the span.
    //!
    //! If the span is empty, the returned iterator will be equal to \ref end().
    //! @returns an \ref iterator to the first element of the span
    constexpr iterator begin() const noexcept
    {
        return iterator(m_p);
    }

    //! Returns an iterator to the element following the last element of the span.
    //!
    //! This element acts as a placeholder; attempting to access it results in undefined behavior.
    //! @returns an \ref iterator to the element following the last element of the span
    constexpr iterator end() const noexcept
    {
        return iterator(m_p + extent);
    }

    //! Returns a reverse iterator to the first element of the reversed span.
    //!
    //! It corresponds to the last element of the non-reversed span. If the span is empty, the returned iterator is
    //! equal to \ref rend().
    //! @returns a \ref reverse_iterator to the first element of the reversed span
    constexpr reverse_iterator rbegin() const noexcept
    {
        return reverse_iterator(end());
    }

    //! Reverse a reverse iterator to the element following the last element of the reversed span.
    //!
    //! It corresponds to the element preceding the first element of the non-reversed span. This element acts as a
    //! placeholder, attempting to access it results in undefined behavior.
    //! @returns a \ref reverse_iterator to the element following the last element of the reversed span
    constexpr reverse_iterator rend() const noexcept
    {
        return reverse_iterator(begin());
    }

    //! Returns a reference to the first element in the span.
    //!
    //! Calling this on an empty span results in undefined behavior.
    //! @returns a reference to the first element
    constexpr reference front() const
    {
        return *m_p;
    }

    //! Returns a reference to the last element in a span.
    //!
    //! Calling this on an empty span results in undefined behavior.
    //! @returns a reference to the last element
    constexpr reference back() const
    {
        return m_p[extent - 1];
    }

    //! Returns a reference to the element in the span at the given index.
    //!
    //! The behavior is undefined if \p index is out of range (i.e. if it is greater than or equal to \ref size() or
    //! the span is \ref empty()).
    //! @param index The index of the element to access
    //! @returns a reference to the element at position \p index
    constexpr reference operator[](size_type index) const
    {
        CARB_THROW_OR_CHECK(index < extent, "Behavior is undefined when index exceeds size()");
        return m_p[index];
    }

    //! Returns a pointer to the beginning of the sequence.
    //! @returns a pointer to the beginning of the sequence
    constexpr pointer data() const noexcept
    {
        return m_p;
    }

    //! Returns the number of elements in the span.
    //! @returns the number of elements in the span
    constexpr size_type size() const noexcept
    {
        return extent;
    }

    //! Returns the size of the sequence in bytes.
    //! @returns the size of the sequence in bytes
    constexpr size_type size_bytes() const noexcept
    {
        return sizeof(element_type) * extent;
    }

    //! Checks if the span is empty.
    //! @returns \c true if the span is empty (i.e. \ref size() == 0); \c false otherwise
    CARB_NODISCARD constexpr bool empty() const noexcept
    {
        return false;
    }

    //! Obtains a subspan consisting of the first N elements of the sequence.
    //!
    //! The program is ill-formed if `Count > extent`. The behavior is undefined if `Count > size()`.
    //! @tparam Count the number of elements of the subspan
    //! @returns a span that is a view over the first `Count` elements of `*this`
    template <std::size_t Count>
    constexpr span<element_type, Count> first() const
    {
        static_assert(Count <= extent, "Program ill-formed if Count > extent");
        return span<element_type, Count>{ m_p, Count };
    }

    //! Obtains a subspan consisting of the first N elements of the sequence.
    //!
    //! The program is ill-formed if `Count > extent`. The behavior is undefined if `Count > size()`.
    //! @param Count the number of elements of the subspan
    //! @returns a span of dynamic extent that is a view over the first `Count` elements of `*this`
    constexpr span<element_type, dynamic_extent> first(size_type Count) const
    {
        CARB_THROW_OR_CHECK(Count <= extent, "Program ill-formed if Count > extent");
        return span<element_type, dynamic_extent>{ m_p, Count };
    }

    //! Obtains a subspan consisting of the last N elements of the sequence.
    //!
    //! The program is ill-formed if `Count > Extent`. The behavior is undefined if `Count > size()`.
    //! @tparam Count the number of elements of the subspan
    //! @returns a span that is a view over the last `Count` elements of `*this`
    template <std::size_t Count>
    constexpr span<element_type, Count> last() const
    {
        static_assert(Count <= extent, "Program ill-formed if Count > extent");
        return span<element_type, Count>{ m_p + (extent - Count), Count };
    }

    //! Obtains a subspan consisting of the last N elements of the sequence.
    //!
    //! The program is ill-formed if `Count > extent`. The behavior is undefined if `Count > size()`.
    //! @param Count the number of elements of the subspan
    //! @returns a span of dynamic extent that is a view over the last `Count` elements of `*this`
    constexpr span<element_type, dynamic_extent> last(size_type Count) const
    {
        CARB_THROW_OR_CHECK(Count <= extent, "Program ill-formed if Count > extent");
        return span<element_type, dynamic_extent>{ m_p + (extent - Count), Count };
    }

    //! Obtains a subspan that is a view over Count elements of this span starting at Offset.
    //!
    //! If `Count` is \ref dynamic_extent, the number of elements in the subspan is \ref size() - `Offset` (i.e. it ends
    //! at the end of `*this`).
    //! Ill-formed if `Offset` is greater than \ref extent, or `Count` is not \ref dynamic_extent and `Count` is greater
    //! than \ref extent - `Offset`.
    //! Behavior is undefined if either `Offset` or `Count` is out of range. This happens if `Offset` is greater than
    //! \ref size(), or `Count` is not \ref dynamic_extent and `Count` is greater than \ref size() - `Offset`.
    //! The \ref extent of the returned span is determined as follows: if `Count` is not \ref dynamic_extent, `Count`;
    //! otherwise, if `Extent` is not \ref dynamic_extent, `Extent - Offset`; otherwise \ref dynamic_extent.
    //! @tparam Offset The offset within `*this` to start the subspan
    //! @tparam Count The length of the subspan, or \ref dynamic_extent to indicate the rest of the span.
    //! @returns A subspan of the given range
    template <std::size_t Offset, std::size_t Count = dynamic_extent>
    constexpr span<element_type, detail::DetermineSubspanExtent<extent, Offset, Count>::value> subspan() const
    {
        static_assert(Offset <= extent, "Ill-formed");
        static_assert(Count == dynamic_extent || (Offset + Count) <= extent, "Ill-formed");
        return span<element_type, detail::DetermineSubspanExtent<extent, Offset, Count>::value>{
            data() + Offset, carb_min(size() - Offset, Count)
        };
    }

    //! Obtains a subspan that is a view over Count elements of this span starting at Offset.
    //!
    //! If \p Count is \ref dynamic_extent, the number of elements in the subspan is \ref size() - `Offset` (i.e. it
    //! ends at the end of `*this`).
    //! Behavior is undefined if either \p Offset or \p Count is out of range. This happens if \p Offset is greater than
    //! \ref size(), or \p Count is not \ref dynamic_extent and \p Count is greater than \ref size() - \p Offset.
    //! The extent of the returned span is always \ref dynamic_extent.
    //! @param Offset The offset within `*this` to start the subspan
    //! @param Count The length of the subspan, or \ref dynamic_extent to indicate the rest of the span.
    //! @returns A subspan of the given range
    constexpr span<element_type, dynamic_extent> subspan(size_type Offset, size_type Count = dynamic_extent) const
    {
        CARB_THROW_OR_CHECK(Offset <= extent, "Program ill-formed if Offset > extent");
        CARB_THROW_OR_CHECK(Count == dynamic_extent || (Offset + Count) <= extent,
                            "Program ill-formed if Count is not dynamic_extent and is greater than Extent - Offset");
        return { data() + Offset, carb_min(size() - Offset, Count) };
    }

private:
    pointer m_p;
};

static_assert(std::is_standard_layout<span<int, 1>>::value, "Span must be standard layout");
static_assert(std::is_trivially_copyable<span<int, 1>>::value, "Span must be trivially copyable");

// Doxygen can ignore the specializations
#ifndef DOXYGEN_BUILD
// Specialization for dynamic_extent
template <class T>
class span<T, dynamic_extent>
{
public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = omni::detail::PointerIterator<pointer, span>;
    using reverse_iterator = std::reverse_iterator<iterator>;

    constexpr static std::size_t extent = dynamic_extent;

    constexpr span() noexcept : m_p(nullptr), m_size(0)
    {
    }

    template <class It,
              std::enable_if_t<std::is_same<typename std::iterator_traits<It>::iterator_category, std::random_access_iterator_tag>::value,
                               bool> = false>
    constexpr span(It first, size_type count)
    {
        m_p = detail::to_address(first);
        m_size = count;
    }

    template <class It,
              std::enable_if_t<std::is_same<typename std::iterator_traits<It>::iterator_category, std::random_access_iterator_tag>::value,
                               bool> = false>
    constexpr span(It first, It last)
    {
        m_p = detail::to_address(first);
        m_size = last - first;
    }

    template <std::size_t N>
    constexpr span(type_identity_t<element_type> (&arr)[N]) noexcept
    {
        m_p = cpp20::data(arr);
        m_size = N;
    }

    template <class U, std::size_t N, std::enable_if_t<std::is_convertible<U, element_type>::value, bool> = false>
    constexpr span(std::array<U, N>& arr) noexcept
    {
        m_p = cpp20::data(arr);
        m_size = N;
    }

    template <class U, std::size_t N, std::enable_if_t<std::is_convertible<U, element_type>::value, bool> = false>
    constexpr span(const std::array<U, N>& arr) noexcept
    {
        m_p = cpp20::data(arr);
        m_size = N;
    }

    // template< class R >
    // explicit(extent != dynamic_extent)
    // constexpr span( R&& range );
    // (Constructor not available without ranges)

    template <class U, std::size_t N, std::enable_if_t<std::is_convertible<U, element_type>::value, bool> = false>
    constexpr span(const span<U, N>& source) noexcept
    {
        m_p = source.data();
        m_size = source.size();
    }

    constexpr span(const span& other) noexcept = default;

    constexpr span& operator=(const span& other) noexcept = default;

    constexpr iterator begin() const noexcept
    {
        return iterator(m_p);
    }

    constexpr iterator end() const noexcept
    {
        return iterator(m_p + m_size);
    }

    constexpr reverse_iterator rbegin() const noexcept
    {
        return reverse_iterator(end());
    }

    constexpr reverse_iterator rend() const noexcept
    {
        return reverse_iterator(begin());
    }

    constexpr reference front() const
    {
        CARB_THROW_OR_CHECK(!empty(), "Behavior is undefined when calling front() on an empty span");
        return *m_p;
    }

    constexpr reference back() const
    {
        CARB_THROW_OR_CHECK(!empty(), "Behavior is undefined when calling back() on an empty span");
        return m_p[m_size - 1];
    }

    constexpr reference operator[](size_type index) const
    {
        CARB_THROW_OR_CHECK(index < m_size, "Behavior is undefined when index exceeds size()");
        return m_p[index];
    }

    constexpr pointer data() const noexcept
    {
        return m_p;
    }

    constexpr size_type size() const noexcept
    {
        return m_size;
    }

    constexpr size_type size_bytes() const noexcept
    {
        return sizeof(element_type) * m_size;
    }

    CARB_NODISCARD constexpr bool empty() const noexcept
    {
        return m_size == 0;
    }

    template <std::size_t Count>
    constexpr span<element_type, Count> first() const
    {
        CARB_THROW_OR_CHECK(Count <= m_size, "Behavior is undefined when Count > size()");
        return span<element_type, Count>{ m_p, Count };
    }

    constexpr span<element_type, dynamic_extent> first(size_type Count) const
    {
        CARB_THROW_OR_CHECK(Count <= m_size, "Behavior is undefined when Count > size()");
        return span<element_type, dynamic_extent>{ m_p, Count };
    }

    template <std::size_t Count>
    constexpr span<element_type, Count> last() const
    {
        CARB_THROW_OR_CHECK(Count <= m_size, "Behavior is undefined when Count > size()");
        return span<element_type, Count>{ m_p + (m_size - Count), Count };
    }

    constexpr span<element_type, dynamic_extent> last(size_type Count) const
    {
        CARB_THROW_OR_CHECK(Count <= m_size, "Behavior is undefined when Count > size()");
        return span<element_type, dynamic_extent>{ m_p + (m_size - Count), Count };
    }

    template <std::size_t Offset, std::size_t Count = dynamic_extent>
    constexpr span<element_type, detail::DetermineSubspanExtent<extent, Offset, Count>::value> subspan() const
    {
        CARB_THROW_OR_CHECK(Offset <= m_size, "Behavior is undefined when Offset > size()");
        CARB_THROW_OR_CHECK(Count == dynamic_extent || (Offset + Count) <= m_size,
                            "Behavior is undefined when Count != dynamic_extent and (Offset + Count) > size()");
        return span<element_type, detail::DetermineSubspanExtent<extent, Offset, Count>::value>{
            data() + Offset, carb_min(size() - Offset, Count)
        };
    }

    constexpr span<element_type, dynamic_extent> subspan(size_type Offset, size_type Count = dynamic_extent) const
    {
        CARB_THROW_OR_CHECK(Offset <= m_size, "Behavior is undefined when Offset > size()");
        CARB_THROW_OR_CHECK(Count == dynamic_extent || (Offset + Count) <= m_size,
                            "Behavior is undefined when Count != dynamic_extent and (Offset + Count) > size()");
        return { data() + Offset, carb_min(size() - Offset, Count) };
    }

private:
    pointer m_p;
    size_type m_size;
};

// Specialization for zero
template <class T>
class span<T, 0>
{
public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = omni::detail::PointerIterator<pointer, span>;
    using reverse_iterator = std::reverse_iterator<iterator>;

    constexpr static std::size_t extent = 0;

    constexpr span() noexcept = default;

    template <class It,
              std::enable_if_t<std::is_same<typename std::iterator_traits<It>::iterator_category, std::random_access_iterator_tag>::value,
                               bool> = false>
    constexpr explicit span(It first, size_type count)
    {
        CARB_UNUSED(first, count);
        CARB_THROW_OR_CHECK(count == 0, "Behavior is undefined if count != extent");
    }

    template <class It,
              std::enable_if_t<std::is_same<typename std::iterator_traits<It>::iterator_category, std::random_access_iterator_tag>::value,
                               bool> = false>
    constexpr explicit span(It first, It last)
    {
        CARB_UNUSED(first, last);
        CARB_THROW_OR_CHECK(first == last, "Behavior is undefined if (last - first) != extent");
    }

    template <std::size_t N>
    constexpr span(type_identity_t<element_type> (&arr)[N]) noexcept
    {
        CARB_UNUSED(arr);
        static_assert(N == extent, "Behavior is undefined if N != extent");
    }

    template <class U, std::size_t N, std::enable_if_t<std::is_convertible<U, element_type>::value, bool> = false>
    constexpr span(std::array<U, N>& arr) noexcept
    {
        CARB_UNUSED(arr);
        static_assert(N == extent, "Behavior is undefined if N != extent");
    }

    template <class U, std::size_t N, std::enable_if_t<std::is_convertible<U, element_type>::value, bool> = false>
    constexpr span(const std::array<U, N>& arr) noexcept
    {
        CARB_UNUSED(arr);
        static_assert(N == extent, "Behavior is undefined if N != extent");
    }

    // template< class R >
    // explicit(extent != dynamic_extent)
    // constexpr span( R&& range );
    // (Constructor not available without ranges)

    template <class U,
              std::size_t N,
              std::enable_if_t<N == dynamic_extent && std::is_convertible<U, element_type>::value, bool> = false>
    constexpr explicit span(const span<U, N>& source) noexcept
    {
        CARB_UNUSED(source);
        CARB_THROW_OR_CHECK(N == extent, "Behavior is undefined if N != extent");
    }

    template <class U,
              std::size_t N,
              std::enable_if_t<N != dynamic_extent && std::is_convertible<U, element_type>::value, bool> = false>
    constexpr explicit span(const span<U, N>& source) noexcept
    {
        CARB_UNUSED(source);
        CARB_THROW_OR_CHECK(N == extent, "Behavior is undefined if N != extent");
    }

    constexpr span(const span& other) noexcept = default;

    constexpr span& operator=(const span& other) noexcept = default;

    constexpr iterator begin() const noexcept
    {
        return end();
    }

    constexpr iterator end() const noexcept
    {
        return iterator(nullptr);
    }

    constexpr reverse_iterator rbegin() const noexcept
    {
        return rend();
    }

    constexpr reverse_iterator rend() const noexcept
    {
        return reverse_iterator(begin());
    }

    constexpr reference front() const
    {
        CARB_ALWAYS_FAIL("Behavior is undefined if front() is called on an empty span");
    }

    constexpr reference back() const
    {
        CARB_ALWAYS_FAIL("Behavior is undefined if back() is called on an empty span");
    }

    constexpr reference operator[](size_type index) const
    {
        CARB_UNUSED(index);
        CARB_ALWAYS_FAIL("Behavior is undefined if index >= size()");
    }

    constexpr pointer data() const noexcept
    {
        return nullptr;
    }

    constexpr size_type size() const noexcept
    {
        return 0;
    }

    constexpr size_type size_bytes() const noexcept
    {
        return 0;
    }

    CARB_NODISCARD constexpr bool empty() const noexcept
    {
        return true;
    }

    template <std::size_t Count>
    constexpr span<element_type, Count> first() const
    {
        static_assert(Count <= extent, "Program ill-formed if Count > extent");
        return span<element_type, Count>{};
    }

    constexpr span<element_type, dynamic_extent> first(size_type Count) const
    {
        CARB_UNUSED(Count);
        CARB_THROW_OR_CHECK(Count == 0, "Behavior is undefined if Count > extent");
        return span<element_type, dynamic_extent>{};
    }

    template <std::size_t Count>
    constexpr span<element_type, Count> last() const
    {
        CARB_THROW_OR_CHECK(Count == 0, "Behavior is undefined if Count > extent");
        return span<element_type, Count>{};
    }

    constexpr span<element_type, dynamic_extent> last(size_type Count) const
    {
        CARB_UNUSED(Count);
        CARB_THROW_OR_CHECK(Count == 0, "Behavior is undefined if Count > extent");
        return span<element_type, dynamic_extent>{};
    }

    template <std::size_t Offset, std::size_t Count = dynamic_extent>
    constexpr span<element_type, detail::DetermineSubspanExtent<extent, Offset, Count>::value> subspan() const
    {
        static_assert(Offset <= extent, "Ill-formed");
        static_assert(Count == dynamic_extent || (Offset + Count) <= extent, "Ill-formed");
        return {};
    }

    constexpr span<element_type, dynamic_extent> subspan(size_type Offset, size_type Count = dynamic_extent) const
    {
        CARB_UNUSED(Offset, Count);
        CARB_THROW_OR_CHECK(Offset <= extent, "Behavior is undefined if Offset > extent");
        CARB_THROW_OR_CHECK(Count == dynamic_extent || (Offset + Count) <= extent,
                            "Behavior is undefined if Count != dynamic_extent and (Offset + Count) > extent");
        return {};
    }
};

static_assert(std::is_standard_layout<span<int, 0>>::value, "Span must be standard layout");
static_assert(std::is_trivially_copyable<span<int, 0>>::value, "Span must be trivially copyable");
static_assert(std::is_standard_layout<span<int, dynamic_extent>>::value, "Span must be standard layout");
static_assert(std::is_trivially_copyable<span<int, dynamic_extent>>::value, "Span must be trivially copyable");
#endif

#ifndef DOXYGEN_BUILD
template <class T, std::size_t N, std::enable_if_t<N == dynamic_extent, bool> = false>
span<const cpp17::byte, dynamic_extent> as_bytes(span<T, N> s) noexcept
{
    return { reinterpret_cast<const cpp17::byte*>(s.data()), s.size_bytes() };
}

template <class T, std::size_t N, std::enable_if_t<N != dynamic_extent, bool> = false>
span<const cpp17::byte, sizeof(T) * N> as_bytes(span<T, N> s) noexcept
#else
//! Obtains a view ot the object representation of the elements of the given span
//! @tparam T the `element_type` of the span
//! @tparam N the `extent` of the span
//! @param s The span
//! @returns A \ref span of type `span<const cpp17::byte, E>` where `E` is \ref dynamic_extent if `N` is also
//!   \ref dynamic_extent, otherwise `E` is `sizeof(T) * N`.
template <class T, std::size_t N>
auto as_bytes(span<T, N> s) noexcept
#endif
{
    return span<const cpp17::byte, sizeof(T) * N>{ reinterpret_cast<const cpp17::byte*>(s.data()), s.size_bytes() };
}

#ifndef DOXYGEN_BUILD
template <class T, std::size_t N, std::enable_if_t<N == dynamic_extent, bool> = false>
span<cpp17::byte, dynamic_extent> as_writable_bytes(span<T, N> s) noexcept
{
    return { reinterpret_cast<cpp17::byte*>(s.data()), s.size_bytes() };
}

template <class T, std::size_t N, std::enable_if_t<N != dynamic_extent, bool> = false>
span<cpp17::byte, sizeof(T) * N> as_writable_bytes(span<T, N> s) noexcept
#else
//! Obtains a writable view to the object representation of the elements of the given span
//! @tparam T the `element_type` of the span
//! @tparam N the `extent` of the span
//! @param s The span
//! @returns A \ref span of type `span<cpp17::byte, E>` where `E` is \ref dynamic_extent if `N` is also
//!   \ref dynamic_extent, otherwise `E` is `sizeof(T) * N`.
template <class T, std::size_t N>
auto as_writable_bytes(span<T, N> s) noexcept
#endif
{
    return span<cpp17::byte, sizeof(T) * N>{ reinterpret_cast<cpp17::byte*>(s.data()), s.size_bytes() };
}

#undef CARB_ALWAYS_FAIL
#undef CARB_THROW_OR_CHECK

} // namespace cpp20
} // namespace carb
