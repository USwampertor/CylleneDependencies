// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

#pragma once

#include "../Defines.h"

#include <algorithm>
#include <string>
#include <typeindex> // for std::hash
#include <utility>

// This class is a not quite standards conformant implementation of std::string_view. Where it doesn't comply it is
// in the sense that it doesn't support everything.

namespace carb
{

namespace cpp17
{

template <class CharT, class Traits = std::char_traits<CharT>>
class CARB_VIZ basic_string_view
{
public:
    using traits_type = Traits;
    using value_type = CharT;
    using pointer = CharT*;
    using const_pointer = const CharT*;
    using reference = CharT&;
    using const_reference = const CharT&;
    using const_iterator = const_pointer;
    using iterator = const_pointer;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator = const_reverse_iterator;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    static constexpr size_type npos = size_type(-1);

    constexpr basic_string_view() noexcept : m_data(nullptr), m_count(0)
    {
    }
    constexpr basic_string_view(const basic_string_view& other) noexcept = default;
    constexpr basic_string_view(const CharT* s, size_type count) : m_data(s), m_count(count)
    {
    }
    constexpr basic_string_view(const CharT* s) : m_data(s), m_count(traits_type::length(s))
    {
    }

    template <class It>
    constexpr basic_string_view(It first, It last) : m_data(std::addressof(*first)), m_count(std::distance(first, last))

    {
    }

    constexpr basic_string_view& operator=(const basic_string_view& view) noexcept = default;

    constexpr const_iterator begin() const noexcept
    {
        return m_data;
    }
    constexpr const_iterator cbegin() const noexcept
    {
        return m_data;
    }

    constexpr const_iterator end() const noexcept
    {
        return m_data + m_count;
    }
    constexpr const_iterator cend() const noexcept
    {
        return m_data + m_count;
    }

    constexpr const_reverse_iterator rbegin() const noexcept
    {
        return const_reverse_iterator(end());
    }
    constexpr const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }

    constexpr const_reverse_iterator rend() const noexcept
    {
        return const_reverse_iterator(begin());
    }
    constexpr const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(cbegin());
    }

    constexpr const_reference operator[](size_type pos) const
    {
        // Though the standard says no bounds checking we do assert
        CARB_ASSERT(pos < m_count);
        return m_data[pos];
    }

    constexpr const_reference at(size_type pos) const
    {
#if CARB_EXCEPTIONS_ENABLED
        if (pos >= m_count)
        {
            throw std::out_of_range("Invalid pos, past end of string");
        }
#else
        CARB_ASSERT(pos < m_count);
#endif
        return m_data[pos];
    }

    constexpr const_reference front() const
    {
        CARB_ASSERT(!empty());
        return *m_data;
    }

    constexpr const_reference back() const
    {
        CARB_ASSERT(!empty());
        return m_data[m_count - 1];
    }

    constexpr const_pointer data() const noexcept
    {
        return m_data;
    }

    constexpr size_type size() const noexcept
    {
        return m_count;
    }

    constexpr size_type length() const noexcept
    {
        return m_count;
    }

    constexpr size_type max_size() const noexcept
    {
        return size_type(-1);
    }

    constexpr bool empty() const noexcept
    {
        return m_count == 0;
    }

    constexpr void remove_prefix(const size_type n) noexcept
    {
        size_type toRemove = ::carb_min(n, m_count);
        m_data += toRemove;
        m_count -= toRemove;
    }

    constexpr void remove_suffix(const size_type n) noexcept
    {
        size_type count = ::carb_min(n, m_count);
        m_count = m_count - count;
    }

    constexpr void swap(basic_string_view& v) noexcept
    {
        std::swap(m_data, v.m_data);
        std::swap(m_count, v.m_count);
    }

    constexpr size_type copy(CharT* dest, size_type count, size_type pos = 0) const
    {
#if CARB_EXCEPTIONS_ENABLED
        if (pos > size())
            throw std::out_of_range("Invalid pos, past end of string");
#else
        CARB_ASSERT(pos <= size());
#endif
        size_type rcount = ::carb_min(count, size() - pos);
        Traits::copy(dest, m_data + pos, rcount);
        return rcount;
    }

    basic_string_view substr(size_t pos, size_t count = npos) const
    {
#if CARB_EXCEPTIONS_ENABLED
        if (pos > size())
            throw std::out_of_range("Invalid pos, past end of string");
#else
        CARB_ASSERT(pos <= size());
#endif
        size_t rcount = ::carb_min(count, size() - pos);
        return basic_string_view(m_data + pos, rcount);
    }

    constexpr int compare(basic_string_view v) const noexcept
    {
        int result = traits_type::compare(m_data, v.m_data, ::carb_min(m_count, v.m_count));

        if (result == 0 && m_count != v.m_count)
        {
            return (m_count < v.m_count) ? -1 : 1;
        }
        return result;
    }

    constexpr int compare(size_type pos1, size_type count1, basic_string_view v) const
    {
        return substr(pos1, count1).compare(v);
    }

    constexpr int compare(size_type pos1, size_type count1, basic_string_view v, size_type pos2, size_type count2) const
    {
        return substr(pos1, count1).compare(v.substr(pos2, count2));
    }
    constexpr int compare(const CharT* s) const
    {
        return compare(basic_string_view(s));
    }
    constexpr int compare(size_type pos1, size_type count1, const CharT* s) const
    {
        return substr(pos1, count1).compare(basic_string_view(s));
    }
    constexpr int compare(size_type pos1, size_type count1, const CharT* s, size_type count2) const
    {
        return substr(pos1, count1).compare(basic_string_view(s, count2));
    }

    constexpr bool starts_with(basic_string_view sv) const noexcept
    {
        if (sv.size() > size())
            return false;

        return traits_type::compare(m_data, sv.m_data, sv.m_count) == 0;
    }

    constexpr bool starts_with(CharT c) const noexcept
    {
        return !empty() && front() == c;
    }
    constexpr bool starts_with(const CharT* s) const
    {
        return starts_with(basic_string_view(s));
    }

    constexpr bool ends_with(basic_string_view sv) const noexcept
    {
        if (sv.size() > size())
            return false;

        return traits_type::compare(m_data + m_count - sv.m_count, sv.m_data, sv.m_count) == 0;
    }
    constexpr bool ends_with(CharT c) const noexcept
    {
        return !empty() && back() == c;
    }
    constexpr bool ends_with(const CharT* s) const
    {
        return ends_with(basic_string_view(s));
    }

    constexpr size_type find(basic_string_view str, size_type pos = 0) const noexcept
    {
        // [strings.view.find] in the Standard.
        size_type xpos = pos;

        while (xpos + str.size() <= size())
        {
            if (traits_type::compare(str.m_data, m_data + xpos, str.m_count) == 0)
            {
                return xpos;
            }
            xpos++;
        }
        return npos;
    }

    constexpr size_type find(CharT ch, size_type pos = 0) const noexcept
    {
        size_type xpos = pos;

        while (xpos + 1 <= size())
        {
            if (traits_type::eq(m_data[xpos], ch))
            {
                return xpos;
            }
            xpos++;
        }
        return npos;
    }

    constexpr size_type find(const CharT* s, size_type pos, size_type count) const
    {
        return find(basic_string_view(s, count), pos);
    }

    constexpr size_type find(const CharT* s, size_type pos = 0) const
    {
        return find(basic_string_view(s), pos);
    }

    constexpr size_type rfind(basic_string_view str, size_type pos = npos) const noexcept
    {
        if (str.m_count > m_count)
        {
            return npos;
        }

        // Clip the position to our string length.
        for (size_type xpos = ::carb_min(pos, m_count - str.m_count);; xpos--)
        {
            if (traits_type::compare(str.m_data, m_data + xpos, str.m_count) == 0)
            {
                return xpos;
            }
            if (xpos == 0)
            {
                break;
            }
        }
        return npos;
    }

    constexpr size_type rfind(CharT ch, size_type pos = npos) const noexcept
    {
        if (empty())
        {
            return npos;
        }

        // Clip the position to our string length.
        for (size_type xpos = ::carb_min(pos, m_count - 1);; xpos--)
        {
            if (traits_type::eq(ch, m_data[xpos]))
            {
                return xpos;
            }
            if (xpos == 0)
            {
                break;
            }
        }
        return npos;
    }

    constexpr size_type rfind(const CharT* s, size_type pos, size_type count) const
    {
        return rfind(basic_string_view(s, count), pos);
    }

    constexpr size_type rfind(const CharT* s, size_type pos = npos) const
    {
        return rfind(basic_string_view(s), pos);
    }

    constexpr size_type find_first_of(basic_string_view v, size_type pos = 0) const noexcept
    {
        if (v.empty())
        {
            return npos;
        }
        size_type xpos = pos;

        while (xpos < size())
        {
            if (v.find(m_data[xpos]) != npos)
            {
                return xpos;
            }
            xpos++;
        }
        return npos;
    }

    constexpr size_type find_first_of(CharT ch, size_type pos = 0) const noexcept
    {
        return find(ch, pos);
    }

    constexpr size_type find_first_of(const CharT* s, size_type pos, size_type count) const
    {
        return find_first_of(basic_string_view(s, count), pos);
    }

    constexpr size_type find_first_of(const CharT* s, size_type pos = 0) const
    {
        return find_first_of(basic_string_view(s), pos);
    }

    constexpr size_type find_last_of(basic_string_view v, size_type pos = npos) const noexcept
    {
        if (v.m_count == 0 || m_count == 0)
        {
            return npos;
        }

        // Clip the position to our string length.
        for (size_type xpos = ::carb_min(pos, m_count - 1);; xpos--)
        {
            if (v.find(m_data[xpos]) != npos)
            {
                return xpos;
            }
            if (xpos == 0)
            {
                break;
            }
        }
        return npos;
    }

    constexpr size_type find_last_of(CharT ch, size_type pos = npos) const noexcept
    {
        return rfind(ch, pos);
    }

    constexpr size_type find_last_of(const CharT* s, size_type pos, size_type count) const
    {
        return find_last_of(basic_string_view(s, count), pos);
    }

    constexpr size_type find_last_of(const CharT* s, size_type pos = npos) const
    {
        return find_last_of(basic_string_view(s), pos);
    }

    constexpr size_type find_first_not_of(basic_string_view v, size_type pos = 0) const noexcept
    {
        size_type xpos = pos;

        if (v.empty())
        {
            return xpos;
        }
        while (xpos < size())
        {
            if (v.find(m_data[xpos]) == npos)
            {
                return xpos;
            }
            xpos++;
        }
        return npos;
    }

    constexpr size_type find_first_not_of(CharT c, size_type pos = 0) const noexcept
    {
        size_type xpos = pos;

        while (xpos < size())
        {
            if (traits_type::eq(c, m_data[xpos]) == false)
            {
                return xpos;
            }
            xpos++;
        }
        return npos;
    }

    constexpr size_type find_first_not_of(const CharT* s, size_type pos, size_type count) const
    {
        return find_first_not_of(basic_string_view(s, count), pos);
    }

    constexpr size_type find_first_not_of(const CharT* s, size_type pos = 0) const
    {
        return find_first_not_of(basic_string_view(s), pos);
    }

    constexpr size_type find_last_not_of(basic_string_view v, size_type pos = 0) const noexcept
    {
        if (empty())
        {
            return pos; // Not of nothing is everything.
        }

        // Clip the position to our string length.
        for (size_type xpos = ::carb_min(pos, m_count - 1);; xpos--)
        {
            if (v.find(m_data[xpos]) == npos)
            {
                return xpos;
            }
            if (xpos == 0)
            {
                break;
            }
        }
        return npos;
    }

    constexpr size_type find_last_not_of(CharT c, size_type pos = 0) const noexcept
    {
        if (empty())
        {
            return pos; // Not of nothing is everything.
        }

        // Clip the position to our string length.
        for (size_type xpos = ::carb_min(pos, m_count - 1);; xpos--)
        {
            if (traits_type::eq(m_data[xpos], c) == false)
            {
                return xpos;
            }
            if (xpos == 0)
            {
                break;
            }
        }
        return npos;
    }

    constexpr size_type find_last_not_of(const CharT* s, size_type pos, size_type count) const
    {
        return find_last_not_of(basic_string_view(s, count), pos);
    }

    constexpr size_type find_last_not_of(const CharT* s, size_type pos = 0) const
    {
        return find_last_not_of(basic_string_view(s), pos);
    }

private:
    CARB_VIZ const_pointer m_data;
    CARB_VIZ size_type m_count;
};

template <class CharT, class Traits = std::char_traits<CharT>>
bool operator==(const basic_string_view<CharT, Traits>& a, const basic_string_view<CharT, Traits>& b)
{
    return a.compare(b) == 0;
}

template <class CharT, class Traits = std::char_traits<CharT>>
bool operator!=(const basic_string_view<CharT, Traits>& a, const basic_string_view<CharT, Traits>& b)
{
    return a.compare(b) != 0;
}

template <class CharT, class Traits = std::char_traits<CharT>>
bool operator<(const basic_string_view<CharT, Traits>& a, const basic_string_view<CharT, Traits>& b)
{
    return a.compare(b) < 0;
}

template <class CharT, class Traits = std::char_traits<CharT>>
bool operator<=(const basic_string_view<CharT, Traits>& a, const basic_string_view<CharT, Traits>& b)
{
    return a.compare(b) <= 0;
}

template <class CharT, class Traits = std::char_traits<CharT>>
bool operator>(const basic_string_view<CharT, Traits>& a, const basic_string_view<CharT, Traits>& b)
{
    return a.compare(b) > 0;
}

template <class CharT, class Traits = std::char_traits<CharT>>
bool operator>=(const basic_string_view<CharT, Traits>& a, const basic_string_view<CharT, Traits>& b)
{
    return a.compare(b) >= 0;
}

using string_view = basic_string_view<char>;
using wstring_view = basic_string_view<wchar_t>;
// C++ 20 and above have char8_t.
// using u8string_view = basic_string_view<char8_t> ;
using u16string_view = basic_string_view<char16_t>;
using u32string_view = basic_string_view<char32_t>;

// [string.view.comparison]
inline bool operator==(const char* t, string_view sv)
{
    return string_view(t) == sv;
}
inline bool operator==(string_view sv, const char* t)
{
    return sv == string_view(t);
}

inline bool operator!=(const char* t, string_view sv)
{
    return string_view(t) != sv;
}
inline bool operator!=(string_view sv, const char* t)
{
    return sv != string_view(t);
}

inline bool operator<(const char* t, string_view sv)
{
    return string_view(t) < sv;
}
inline bool operator<(string_view sv, const char* t)
{
    return sv < string_view(t);
}

inline bool operator<=(const char* t, string_view sv)
{
    return string_view(t) <= sv;
}
inline bool operator<=(string_view sv, const char* t)
{
    return sv <= string_view(t);
}


inline bool operator>(const char* t, string_view sv)
{
    return string_view(t) > sv;
}
inline bool operator>(string_view sv, const char* t)
{
    return sv > string_view(t);
}

inline bool operator>=(const char* t, string_view sv)
{
    return string_view(t) >= sv;
}
inline bool operator>=(string_view sv, const char* t)
{
    return sv >= string_view(t);
}

inline bool operator==(const wchar_t* t, wstring_view sv)
{
    return wstring_view(t) == sv;
}
inline bool operator==(wstring_view sv, const wchar_t* t)
{
    return sv == wstring_view(t);
}

inline bool operator!=(const wchar_t* t, wstring_view sv)
{
    return wstring_view(t) != sv;
}
inline bool operator!=(wstring_view sv, const wchar_t* t)
{
    return sv != wstring_view(t);
}

inline bool operator<(const wchar_t* t, wstring_view sv)
{
    return wstring_view(t) < sv;
}
inline bool operator<(wstring_view sv, const wchar_t* t)
{
    return sv < wstring_view(t);
}

inline bool operator<=(const wchar_t* t, wstring_view sv)
{
    return wstring_view(t) <= sv;
}
inline bool operator<=(wstring_view sv, const wchar_t* t)
{
    return sv <= wstring_view(t);
}


inline bool operator>(const wchar_t* t, wstring_view sv)
{
    return wstring_view(t) > sv;
}
inline bool operator>(wstring_view sv, const wchar_t* t)
{
    return sv > wstring_view(t);
}

inline bool operator>=(const wchar_t* t, wstring_view sv)
{
    return wstring_view(t) >= sv;
}
inline bool operator>=(wstring_view sv, const wchar_t* t)
{
    return sv >= wstring_view(t);
}

inline bool operator==(const char16_t* t, u16string_view sv)
{
    return u16string_view(t) == sv;
}
inline bool operator==(u16string_view sv, const char16_t* t)
{
    return sv == u16string_view(t);
}

inline bool operator!=(const char16_t* t, u16string_view sv)
{
    return u16string_view(t) != sv;
}
inline bool operator!=(u16string_view sv, const char16_t* t)
{
    return sv != u16string_view(t);
}

inline bool operator<(const char16_t* t, u16string_view sv)
{
    return u16string_view(t) < sv;
}
inline bool operator<(u16string_view sv, const char16_t* t)
{
    return sv < u16string_view(t);
}

inline bool operator<=(const char16_t* t, u16string_view sv)
{
    return u16string_view(t) <= sv;
}
inline bool operator<=(u16string_view sv, const char16_t* t)
{
    return sv <= u16string_view(t);
}

inline bool operator>(const char16_t* t, u16string_view sv)
{
    return u16string_view(t) > sv;
}
inline bool operator>(u16string_view sv, const char16_t* t)
{
    return sv > u16string_view(t);
}

inline bool operator>=(const char16_t* t, u16string_view sv)
{
    return u16string_view(t) >= sv;
}
inline bool operator>=(u16string_view sv, const char16_t* t)
{
    return sv >= u16string_view(t);
}

inline bool operator==(const char32_t* t, u32string_view sv)
{
    return u32string_view(t) == sv;
}
inline bool operator==(u32string_view sv, const char32_t* t)
{
    return sv == u32string_view(t);
}

inline bool operator!=(const char32_t* t, u32string_view sv)
{
    return u32string_view(t) != sv;
}
inline bool operator!=(u32string_view sv, const char32_t* t)
{
    return sv != u32string_view(t);
}

inline bool operator<(const char32_t* t, u32string_view sv)
{
    return u32string_view(t) < sv;
}
inline bool operator<(u32string_view sv, const char32_t* t)
{
    return sv < u32string_view(t);
}

inline bool operator<=(const char32_t* t, u32string_view sv)
{
    return u32string_view(t) <= sv;
}
inline bool operator<=(u32string_view sv, const char32_t* t)
{
    return sv <= u32string_view(t);
}

inline bool operator>(const char32_t* t, u32string_view sv)
{
    return u32string_view(t) > sv;
}
inline bool operator>(u32string_view sv, const char32_t* t)
{
    return sv > u32string_view(t);
}

inline bool operator>=(const char32_t* t, u32string_view sv)
{
    return u32string_view(t) >= sv;
}
inline bool operator>=(u32string_view sv, const char32_t* t)
{
    return sv >= u32string_view(t);
}

// Note that literal suffixes that don't start with _ are reserved, in addition we probably don't want to compete with
// the C++17 suffix either.
constexpr string_view operator""_sv(const char* str, std::size_t len) noexcept
{
    return string_view(str, len);
}

// C++ 20 and above have char8_t.
// constexpr u8string_view operator""_sv(const char8_t* str, std::size_t len) noexcept
constexpr u16string_view operator""_sv(const char16_t* str, std::size_t len) noexcept
{
    return u16string_view(str, len);
}
constexpr u32string_view operator""_sv(const char32_t* str, std::size_t len) noexcept
{
    return u32string_view(str, len);
}

constexpr wstring_view operator""_sv(const wchar_t* str, std::size_t len) noexcept
{
    return wstring_view(str, len);
}

template <class CharT, class Traits>
void swap(carb::cpp17::basic_string_view<CharT, Traits>& a, carb::cpp17::basic_string_view<CharT, Traits>& b)
{
    a.swap(b);
};

} // namespace cpp17
} // namespace carb

template <class CharT, class Traits>
struct std::hash<carb::cpp17::basic_string_view<CharT, Traits>>
{
    size_t operator()(const carb::cpp17::basic_string_view<CharT, Traits>& v) const
    {
        return carb::hashBuffer(v.data(), (uintptr_t)(v.data() + v.size()) - (uintptr_t)(v.data()));
    }
};
