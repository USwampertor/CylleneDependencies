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
//! @brief Provides @c omni::detail::PointerIterator class for constructing pointer-based iterators.
#pragma once

#include "../../carb/Defines.h"

#include <iterator>
#include <type_traits>

#if CARB_HAS_CPP20
#    include <concepts>
#endif

namespace omni
{
//! Internal implementation details namespace.
namespace detail
{

//! @class PointerIterator
//!
//! This iterator adapter wraps a pointer type into a class. It does not change the semantics of any operations from the
//! fundamental logic of pointers. There are no bounds checks, there is no additional safety.
//!
//! @tparam TPointer   The pointer to base this wrapper on. The underlying value type of the pointer can be
//!                    const-qualified.
//! @tparam TContainer The container type this iterator iterates over. This is useful to make types distinct, even in
//!                    cases where @c TPointer would be identical; for example, a @c string vs a @c vector<char>. This
//!                    is allowed to be @c void for cases where there is no underlying container.
//!
//! @code
//! // This is meant to be used on contiguous containers where returning a pointer from @c begin and @c end would be
//! // inappropriate.
//! template <typename T>
//! class MyContainer
//! {
//! public:
//!     using iterator       = detail::PointerIterator<T*,       MyContainer>;
//!     using const_iterator = detail::PointerIterator<T const*, MyContainer>;
//!
//!     // ...
//!
//!     iterator begin() { return iterator{data()}; }
//! };
//! @endcode
template <typename TPointer, typename TContainer>
class PointerIterator
{
    static_assert(std::is_pointer<TPointer>::value, "TPointer must be a pointer type");

private:
    using underlying_traits_type = std::iterator_traits<TPointer>;

public:
    //! The underlying value that dereferencing this iterator returns. Note that this is not const-qualified; the
    //! @c const_iterator for a container will still return @c T.
    using value_type = typename underlying_traits_type::value_type;

    //! The reference type dereferencing this iterator returns. This is const-qualified, so @c iterator for a container
    //! will return @c T& and @c const_iterator will return `const T&`.
    using reference = typename underlying_traits_type::reference;

    //! The underlying pointer type @c operator->() returns. This is const-qualified, so @c iterator for a container
    //! will return @c T* and @c const_iterator will return `const T*`.
    using pointer = typename underlying_traits_type::pointer;

    //! The type used to represent the distance between two iterators. This will always be @c std::ptrdiff_t.
    using difference_type = typename underlying_traits_type::difference_type;

    //! The category of this iterator. This will always be @c std::random_access_iterator_tag.
    using iterator_category = typename underlying_traits_type::iterator_category;

#if CARB_HAS_CPP20
    //! The concept this iterator models. This will always be @c std::contiguous_iterator_tag.
    using iterator_concept = typename underlying_traits_type::iterator_concept;
#endif

public:
    //! Default construction of a pointer-iterator results in an iterator pointing to @c nullptr.
    constexpr PointerIterator() noexcept : m_ptr{ nullptr }
    {
    }

    //! Create an iterator from @a src pointer.
    explicit constexpr PointerIterator(pointer src) noexcept : m_ptr{ src }
    {
    }

    //! Instances are trivially copy-constructible.
    PointerIterator(const PointerIterator&) = default;

    //! Instances are trivially move-constructible.
    PointerIterator(PointerIterator&&) = default;

    //! Instances are trivially copyable.
    PointerIterator& operator=(const PointerIterator&) = default;

    //! Instances are trivially movable.
    PointerIterator& operator=(PointerIterator&&) = default;

    //! Instances are trivially destructible.
    ~PointerIterator() = default;

    // clang-format off
    //! Converting constructor to allow conversion from a non-const iterator type to a const iterator type. Generally,
    //! this allows a @c TContainer::iterator to become a @c TContainer::const_iterator.
    //!
    //! This constructor is only enabled if:
    //!
    //! 1. The @c TContainer for the two types is the same.
    //! 2. The @c TPointer is different from @c UPointer (there is a conversion which needs to occur).
    //! 3. A pointer to an array of the @a src const-qualified @c value_type (aka `remove_reference_t<reference>`) is
    //!    convertible to an array of this const-qualified @c value_type. This restricts conversion of iterators from a
    //!    derived to a base class pointer type.
    template <typename UPointer,
              typename = std::enable_if_t
                         <  !std::is_same<TPointer, UPointer>::value
                         && std::is_convertible
                            <
                                std::remove_reference_t<typename PointerIterator<UPointer, TContainer>::reference>(*)[],
                                std::remove_reference_t<reference>(*)[]
                            >::value
                         >
             >
    constexpr PointerIterator(const PointerIterator<UPointer, TContainer>& src) noexcept
        : m_ptr{src.operator->()}
    {
    }
    // clang-format on

    //! Dereference this iterator to get its value.
    constexpr reference operator*() const noexcept
    {
        return *m_ptr;
    }

    //! Get the value at offset @a idx from this iterator. Negative values are supported to reference behind this
    //! instance.
    constexpr reference operator[](difference_type idx) const noexcept
    {
        return m_ptr[idx];
    }

    //! Get a pointer to the value.
    constexpr pointer operator->() const noexcept
    {
        return m_ptr;
    }

    //! Move the iterator forward by one.
    constexpr PointerIterator& operator++() noexcept
    {
        ++m_ptr;
        return *this;
    }

    //! Move the iterator forward by one, but return the previous position.
    constexpr PointerIterator operator++(int) noexcept
    {
        PointerIterator save{ *this };
        ++m_ptr;
        return save;
    }

    //! Move the iterator backwards by one.
    constexpr PointerIterator& operator--() noexcept
    {
        --m_ptr;
        return *this;
    }

    //! Move the iterator backwards by one, but return the previous position.
    constexpr PointerIterator operator--(int) noexcept
    {
        PointerIterator save{ *this };
        --m_ptr;
        return save;
    }

    //! Move the iterator forward by @a dist.
    constexpr PointerIterator& operator+=(difference_type dist) noexcept
    {
        m_ptr += dist;
        return *this;
    }

    //! Get a new iterator pointing @a dist elements forward from this one.
    constexpr PointerIterator operator+(difference_type dist) const noexcept
    {
        return PointerIterator{ m_ptr + dist };
    }

    //! Move the iterator backwards by @a dist.
    constexpr PointerIterator& operator-=(difference_type dist) noexcept
    {
        m_ptr -= dist;
        return *this;
    }

    //! Get a new iterator pointing @a dist elements backwards from this one.
    constexpr PointerIterator operator-(difference_type dist) const noexcept
    {
        return PointerIterator{ m_ptr - dist };
    }

private:
    pointer m_ptr;
};

//! Get an iterator @a dist elements forward from @a iter.
template <typename TPointer, typename TContainer>
inline constexpr PointerIterator<TPointer, TContainer> operator+(
    typename PointerIterator<TPointer, TContainer>::difference_type dist,
    const PointerIterator<TPointer, TContainer>& iter) noexcept
{
    return iter + dist;
}

//! Get the distance between iterators @a lhs and @a rhs. If `lhs < rhs`, this value will be negative.
template <typename TPointer, typename UPointer, typename TContainer>
inline constexpr auto operator-(const PointerIterator<TPointer, TContainer>& lhs,
                                const PointerIterator<UPointer, TContainer>& rhs) noexcept
    -> decltype(lhs.operator->() - rhs.operator->())
{
    return lhs.operator->() - rhs.operator->();
}

//! Test for equality between @a lhs and @a rhs.
template <typename TPointer, typename UPointer, typename TContainer>
inline constexpr bool operator==(const PointerIterator<TPointer, TContainer>& lhs,
                                 const PointerIterator<UPointer, TContainer>& rhs) noexcept
{
    return lhs.operator->() == rhs.operator->();
}

//! Test for inequality between @a lhs and @a rhs.
template <typename TPointer, typename UPointer, typename TContainer>
inline constexpr bool operator!=(const PointerIterator<TPointer, TContainer>& lhs,
                                 const PointerIterator<UPointer, TContainer>& rhs) noexcept
{
    return lhs.operator->() != rhs.operator->();
}

//! Test that @a lhs points to something less than @a rhs.
template <typename TPointer, typename UPointer, typename TContainer>
inline constexpr bool operator<(const PointerIterator<TPointer, TContainer>& lhs,
                                const PointerIterator<UPointer, TContainer>& rhs) noexcept
{
    return lhs.operator->() < rhs.operator->();
}

//! Test that @a lhs points to something less than or equal to @a rhs.
template <typename TPointer, typename UPointer, typename TContainer>
inline constexpr bool operator<=(const PointerIterator<TPointer, TContainer>& lhs,
                                 const PointerIterator<UPointer, TContainer>& rhs) noexcept
{
    return lhs.operator->() <= rhs.operator->();
}

//! Test that @a lhs points to something greater than @a rhs.
template <typename TPointer, typename UPointer, typename TContainer>
inline constexpr bool operator>(const PointerIterator<TPointer, TContainer>& lhs,
                                const PointerIterator<UPointer, TContainer>& rhs) noexcept
{
    return lhs.operator->() > rhs.operator->();
}

//! Test that @a lhs points to something greater than or equal to @a rhs.
template <typename TPointer, typename UPointer, typename TContainer>
inline constexpr bool operator>=(const PointerIterator<TPointer, TContainer>& lhs,
                                 const PointerIterator<UPointer, TContainer>& rhs) noexcept
{
    return lhs.operator->() >= rhs.operator->();
}

} // namespace detail
} // namespace omni
