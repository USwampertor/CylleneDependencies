// Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Type implementations for *carb.variant.plugin*
#pragma once

namespace carb
{
namespace variant
{

inline void VariantArray::clear() noexcept
{
    resize(0);
}

inline bool VariantArray::empty() const noexcept
{
    return size() == 0;
}

inline Variant& VariantArray::at(size_t index)
{
    if (index >= size())
    {
#if CARB_EXCEPTIONS_ENABLED
        throw std::out_of_range("out-of-range index specified");
#else
        CARB_FATAL_UNLESS(0, "out-of-range index specified");
#endif
    }
    return *(data() + index);
}

inline const Variant& VariantArray::at(size_t index) const
{
    if (index >= size())
    {
#if CARB_EXCEPTIONS_ENABLED
        throw std::out_of_range("out-of-range index specified");
#else
        CARB_FATAL_UNLESS(0, "out-of-range index specified");
#endif
    }
    return *(data() + index);
}

inline Variant& VariantArray::operator[](size_t index) noexcept
{
    return *(data() + index);
}
inline const Variant& VariantArray::operator[](size_t index) const noexcept
{
    return *(data() + index);
}

inline Variant& VariantArray::front() noexcept
{
    return *data();
}
inline const Variant& VariantArray::front() const noexcept
{
    return *data();
}

inline Variant& VariantArray::back() noexcept
{
    return *(data() + size() - 1);
}
inline const Variant& VariantArray::back() const noexcept
{
    return *(data() + size() - 1);
}

inline auto VariantArray::begin() noexcept -> iterator
{
    return iterator(data());
}

inline auto VariantArray::end() noexcept -> iterator
{
    return iterator(data() + size());
}

inline auto VariantArray::begin() const noexcept -> const_iterator
{
    return const_iterator(data());
}

inline auto VariantArray::end() const noexcept -> const_iterator
{
    return const_iterator(data() + size());
}

inline auto VariantMap::cbegin() const noexcept -> const_iterator
{
    return { this, internalBegin() };
}

inline auto VariantMap::begin() const noexcept -> const_iterator
{
    return cbegin();
}

inline auto VariantMap::begin() noexcept -> iterator
{
    return { this, internalBegin() };
}

inline auto VariantMap::cend() const noexcept -> const_iterator
{
    return { this, nullptr };
}

inline auto VariantMap::end() const noexcept -> const_iterator
{
    return cend();
}

inline auto VariantMap::end() noexcept -> iterator
{
    return { this, nullptr };
}

inline bool VariantMap::empty() const noexcept
{
    return size() == 0;
}

inline auto VariantMap::insert(const Variant& key, Variant value) -> std::pair<iterator, bool>
{
    std::pair<iterator, bool> result;
    result.first = iterator(this, internalInsert(key, result.second));
    if (result.second)
        result.first->second = std::move(value);
    return result;
}

inline size_t VariantMap::erase(const Variant& key) noexcept
{
    auto where = internalFind(key);
    return where ? (internalErase(where), 1) : 0;
}

inline auto VariantMap::erase(const_iterator pos) noexcept -> iterator
{
    CARB_ASSERT(pos.owner == this && pos.where);
    internalErase(pos.where);
    return iterator{ this, iterNext(pos.where) };
}

inline auto VariantMap::erase(const_find_iterator pos) noexcept -> find_iterator
{
    CARB_ASSERT(pos.owner == this && pos.where);
    auto next = findNext(pos.where);
    internalErase(pos.where);
    return find_iterator{ this, next };
}

inline auto VariantMap::find(const Variant& key) noexcept -> find_iterator
{
    return { this, internalFind(key) };
}

inline auto VariantMap::find(const Variant& key) const noexcept -> const_find_iterator
{
    return { this, internalFind(key) };
}

inline bool VariantMap::contains(const Variant& key) const noexcept
{
    return !!internalFind(key);
}

inline size_t VariantMap::count(const Variant& key) const noexcept
{
    return !internalFind(key) ? 0 : 1;
}

#if CARB_EXCEPTIONS_ENABLED
inline auto VariantMap::at(const Variant& key) -> mapped_type&
{
    auto vt = internalFind(key);
    if (!vt)
        throw std::out_of_range("key not found");
    return vt->second;
}

inline auto VariantMap::at(const Variant& key) const -> const mapped_type&
{
    auto vt = internalFind(key);
    if (!vt)
        throw std::out_of_range("key not found");
    return vt->second;
}
#endif

inline auto VariantMap::operator[](const Variant& key) -> mapped_type&
{
    bool success;
    auto vt = internalInsert(key, success);
    return vt->second;
}

} // namespace variant
} // namespace carb
