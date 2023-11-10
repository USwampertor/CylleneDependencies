// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <locale>

#include "../carb/Memory.h"
#include "../carb/extras/ScopeExit.h"
#include "core/Assert.h"

// TODO(saxonp): Replace usage of std::memcpy with traits_type::copy and std::memmove with traits_type::move when
// making the functions using those functions constexpr. std::memcpy and std::memmove are currently used in
// non-constexpr functions for performance reasons because efficient constexpr copy functions are not available until
// C++20.

// `pyerrors.h` defines vsnprintf() to be _vsnprintf() on Windows, which is non-standard and
// breaks things. In the more modern C++ that we're using, std::vsnprintf() does what we want,
// so get rid of the baddness from `pyerrors.h` here.  As a service to others, we'll also undefine
// `snprintf` symbol that is also defined in `pyerrors.h`.
#if defined(Py_ERRORS_H) && CARB_PLATFORM_WINDOWS
#    undef vsnprintf
#    undef snprintf
#endif

namespace omni
{

namespace detail
{
constexpr void char_traits::assign(char& dest, const char& c) noexcept
{
    dest = c;
}

constexpr char* char_traits::assign(char* dest, std::size_t count, char c) noexcept
{
    for (std::size_t i = 0; i < count; ++i)
    {
        dest[i] = c;
    }
    return dest;
}

constexpr void char_traits::move(char* dest, const char* source, std::size_t count) noexcept
{
    // Fallback basic constexpr implemenation
    if (std::less<const char*>{}(source, dest))
    {
        // If source is less than dest, copy from the back to the front to avoid overwriting characters in source if
        // source and dest overlap.
        for (std::size_t i = count; i > 0; --i)
        {
            dest[i - 1] = source[i - 1];
        }
    }
    else if (std::greater<const char*>{}(source, dest))
    {
        // If source is greater than dest, copy from the the front to the back
        for (std::size_t i = 0; i < count; ++i)
        {
            dest[i] = source[i];
        }
    }
}

constexpr void char_traits::copy(char* dest, const char* source, std::size_t count) noexcept
{
    // Fallback basic constexpr implemenation
    for (std::size_t i = 0; i < count; ++i)
    {
        dest[i] = source[i];
    }
}

constexpr int char_traits::compare(const char* s1, const char* s2, std::size_t count) noexcept
{
#if CARB_HAS_CPP17
    return std::char_traits<char>::compare(s1, s2, count);
#else
    for (std::size_t i = 0; i < count; ++i)
    {
        if (s1[i] < s2[i])
        {
            return -1;
        }
        else if (s1[i] > s2[i])
        {
            return 1;
        }
    }

    return 0;
#endif
}

constexpr std::size_t char_traits::length(const char* s) noexcept
{
#if CARB_HAS_CPP17
    return std::char_traits<char>::length(s);
#else
    std::size_t result = 0;
    while (s[result] != char())
    {
        ++result;
    }

    return result;
#endif
}

constexpr const char* char_traits::find(const char* s, std::size_t count, char ch) noexcept
{
#if CARB_HAS_CPP17
    return std::char_traits<char>::find(s, count, ch);
#else
    for (std::size_t i = 0; i < count; ++i)
    {
        if (s[i] == ch)
        {
            return &s[i];
        }
    }

    return nullptr;
#endif
}

#pragma push_macro("min")
#undef min
#pragma push_macro("max")
#undef max

template <typename TRet, typename Ret = TRet, typename... Base>
Ret _sto_helper(TRet (*conversion_function)(const char*, char**, Base...),
                const char* name,
                const char* str,
                std::size_t* pos,
                Base... base)
{
    Ret result;
    char* endptr;

    // Save errno and restore it after this function completes
    // Ideally this could use CARB_SCOPE_EXIT, however, this causes an ICE in GCC7 in some cases.
    // See: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83204
    struct errno_saver
    {
        errno_saver() : m_saved(errno)
        {
            errno = 0;
        }
        ~errno_saver()
        {
            if (errno == 0)
            {
                errno = m_saved;
            }
        }
        const int m_saved;
    } saver{};

    struct range_check
    {
        static bool check(TRet, std::false_type)
        {
            return false;
        }
        static bool check(TRet val, std::true_type)
        {
            return val < TRet(std::numeric_limits<int>::min()) || val > TRet(std::numeric_limits<int>::max());
        }
    };

    const TRet conv_result = conversion_function(str, &endptr, base...);
#if CARB_EXCEPTIONS_ENABLED
    if (endptr == str)
    {
        throw std::invalid_argument(name);
    }
    else if (errno == ERANGE || range_check::check(conv_result, std::is_same<Ret, int>{}))
    {
        throw std::out_of_range(name);
    }
    else
    {
        result = conv_result;
    }
#else
    CARB_FATAL_UNLESS(endptr != str, "Unable to convert string: %s", name);
    CARB_FATAL_UNLESS((errno != ERANGE && !range_check::check(conv_result, std::is_same<Ret, int>{})),
                      "Converted value out of range: %s ", name);
    result = conv_result;
#endif

    if (pos)
    {
        *pos = endptr - str;
    }

    return result;
}

#pragma pop_macro("min")
#pragma pop_macro("max")

constexpr void null_check(const char* ptr, const char* function)
{
#if CARB_EXCEPTIONS_ENABLED
    if (ptr == nullptr)
    {
        throw std::invalid_argument(std::string(function) + " invalid nullptr argument");
    }
#else
    CARB_FATAL_UNLESS(ptr != nullptr, "%s invalid nullptr argument", function);
#endif
}
} // namespace detail

/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                    Constructors                                                    */
/* ------------------------------------------------------------------------------------------------------------------ */

inline string::string() noexcept
{
    set_empty();
}

inline string::string(size_type n, value_type c)
{
    size_type new_size = length_check(0, n, "string::string");
    set_empty();
    allocate_if_necessary(new_size);

    traits_type::assign(get_pointer(0), new_size, c);
    set_size(new_size);
}

inline string::string(const string& str, size_type pos)
{
    str.range_check(pos, str.size(), "string::string");
    set_empty();

    initialize(str.data() + pos, str.size() - pos);
}

inline string::string(const string& str, size_type pos, size_type n)
{
    str.range_check(pos, str.size(), "string::string");
    set_empty();

    size_type size = ::carb_min(n, str.size() - pos);

    initialize(str.data() + pos, size);
}

inline string::string(const value_type* s, size_type n)
{
    set_empty();

    ::omni::detail::null_check(s, "string::string");

    initialize(s, n);
}

inline string::string(const value_type* s)
{
    set_empty();

    ::omni::detail::null_check(s, "string::string");

    size_type new_size = length_check(0, traits_type::length(s), "string::string");

    initialize(s, new_size);
}

template <typename InputIterator>
string::string(InputIterator begin, InputIterator end)
{
    set_empty();
    initialize(begin, end, std::distance(begin, end));
}

inline string::string(const string& str)
{
    set_empty();
    initialize(str.data(), str.size());
}

inline string::string(string&& str) noexcept
{
    set_empty();
    if (str.is_local())
    {
        std::memcpy(get_pointer(0), str.data(), str.size());
        set_size(str.size());
        str.set_size(0);
    }
    else
    {
        m_allocated_data.m_ptr = str.m_allocated_data.m_ptr;
        m_allocated_data.m_capacity = str.m_allocated_data.m_capacity;
        m_allocated_data.m_size = str.size();
        set_allocated();
        str.set_empty();
    }
}

inline string::string(std::initializer_list<value_type> ilist)
{
    set_empty();
    initialize(ilist.begin(), ilist.end(), ilist.size());
}

inline string::string(const std::string& str)
{
    set_empty();
    initialize(str.data(), str.size());
}

inline string::string(const std::string& str, size_type pos, size_type n)
{
    range_check(pos, str.size(), "string::string");
    set_empty();

    size_type size = ::carb_min(n, str.size() - pos);

    initialize(str.data() + pos, size);
}

inline string::string(const carb::cpp17::string_view& sv)
{
    set_empty();
    initialize(sv.data(), sv.size());
}

inline string::string(const carb::cpp17::string_view& sv, size_type pos, size_type n)
{
    range_check(pos, sv.size(), "string::string");
    set_empty();

    size_type size = ::carb_min(n, sv.size() - pos);

    initialize(sv.data() + pos, size);
}

#if CARB_HAS_CPP17
template <typename T, typename>
string::string(const T& t)
{
    const std::string_view sv = t;
    set_empty();
    initialize(sv.data(), sv.size());
}

template <typename T, typename>
string::string(const T& t, size_type pos, size_type n)
{
    const std::string_view sv = t;
    range_check(pos, sv.size(), "string::string");
    set_empty();
    size_type size = ::carb_min(n, sv.size() - pos);
    initialize(sv.data() + pos, size);
}
#endif

// GCC 7 and/or GLIBC 2.27 seem to have a hard time with a string constructor that takes varargs. For instance, this
// test case works just fine on MSVC 19 but fails on GCC 7:
//
//   string s(formatted, "the %s brown fox jumped over %s %s %s%c %d", "quick", "the", "lazy", "dog", '.', 123456789);
// expected:
//   "the quick brown fox jumped over the lazy dog. 123456789" (55 chars)
// actual on GCC 7 / GLIBC 2.27:
//   "the quick brown fox jumped over the lazy dog" (48 chars)
//
// I suspect that this has something to do with the number of arguments (since some will be passed on the stack)
// combined with how the constructor is compiled since append_printf() et al do not appear to have this issue.
// Therefore, this function is implemented with variadic templates instead of varargs.
template <class... Args>
string::string(formatted_t, const char* fmt, Args&&... args)
{
    ::omni::detail::null_check(fmt, "string::string");

    set_empty();

    // Optimistically try to format as a small string. If the string would be >= kSMALL_STRING_SIZE, it
    // returns the number of charaters that would be written given unlimited buffer.
    size_type fmt_size = snprintf_check(m_local_data, kSMALL_STRING_SIZE, fmt, std::forward<Args>(args)...);

    length_check(0, fmt_size, "string::string");

    if (fmt_size >= kSMALL_STRING_SIZE)
    {
        allocate_if_necessary(fmt_size);
        // CARB_SCOPE_EXCEPT would be applicable here, but it is slow (much slower than a `try` block) and on older
        // versions of glibc (especially 2.17 which is what Centos7 uses) it can lock an internal mutex that is shared
        // with the loader, which can cause deadlocks.
        // If CARB_EXCEPTIONS_ENABLED is false, `snprintf_check` will terminate on failure.
#if CARB_EXCEPTIONS_ENABLED
        try
        {
#endif
            // If vsnprintf_check throws (it shouldn't), clean up.
            snprintf_check(get_pointer(0), fmt_size + 1, fmt, std::forward<Args>(args)...);
#if CARB_EXCEPTIONS_ENABLED
        }
        catch (...)
        {
            dispose();
            /*re-*/ throw;
        }
#endif
    }

    set_size(fmt_size);
}

inline string::string(vformatted_t, const char* fmt, va_list ap)
{
    ::omni::detail::null_check(fmt, "string::string");

    set_empty();

    va_list ap2;
    va_copy(ap2, ap);
    CARB_SCOPE_EXIT
    {
        va_end(ap2);
    };

    // Optimistically try to format as a small string. If the string would be bigger than kSMALL_STRING_SIZE - 1, it
    // returns the number of characters that would be written.
    size_type fmt_size = vsnprintf_check(m_local_data, kSMALL_STRING_SIZE, fmt, ap);

    length_check(0, fmt_size, "string::string");

    if (fmt_size >= kSMALL_STRING_SIZE)
    {
        allocate_if_necessary(fmt_size);
        CARB_SCOPE_EXCEPT
        {
            dispose();
        }; // If vsnprintf_check throws (it shouldn't), clean up.
        vsnprintf_check(get_pointer(0), fmt_size + 1, fmt, ap2);
    }

    set_size(fmt_size);
}

/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                     Destructor                                                     */
/* ------------------------------------------------------------------------------------------------------------------ */
inline string::~string() noexcept
{
    dispose();
}

/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                     Assignment                                                     */
/* ------------------------------------------------------------------------------------------------------------------ */
inline string& string::operator=(const string& str)
{
    // Because all omni::strings use the same allocator, we don't have to worry about allocator propogation or
    // incompatible allocators, we can simply copy the string.
    return this->assign(str);
}

inline string& string::operator=(string&& str) noexcept
{
    // Store our current pointer so we can give it to the other string.
    pointer cur_data = nullptr;
    size_type cur_capacity = 0;
    if (!is_local())
    {
        cur_data = m_allocated_data.m_ptr;
        cur_capacity = m_allocated_data.m_capacity;
    }

    // If other string is short string optimized, just copy
    if (str.is_local())
    {
        // This is now a local string
        set_local(0);

        // Copy the data
        if (!str.empty())
        {
            std::memcpy(get_pointer(0), str.data(), str.size());
        }
        set_size(str.size());
    }
    else
    {
        // This is now an allocated string
        set_allocated();

        // Take the other string's pointer
        m_allocated_data.m_ptr = str.m_allocated_data.m_ptr;
        m_allocated_data.m_capacity = str.m_allocated_data.m_capacity;
        m_allocated_data.m_size = str.m_allocated_data.m_size;

        str.set_empty();
    }

    if (cur_data != nullptr)
    {
        str.m_allocated_data.m_ptr = cur_data;
        str.m_allocated_data.m_capacity = cur_capacity;
        str.set_allocated();
    }

    str.clear();
    return *this;
}

inline string& string::operator=(const value_type* s)
{
    ::omni::detail::null_check(s, "string::operator=");
    return this->assign(s);
}

inline string& string::operator=(value_type c)
{
    return this->assign(1, c);
}

inline string& string::operator=(std::initializer_list<value_type> ilist)
{
    return this->assign(ilist);
}

inline string& string::operator=(const std::string& str)
{
    return this->assign(str);
}

inline string& string::operator=(const carb::cpp17::string_view& sv)
{
    return this->assign(sv);
}

#if CARB_HAS_CPP17
template <typename T, typename>
string& string::operator=(const T& t)
{
    return this->assign(t);
}
#endif

/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                       assign                                                       */
/* ------------------------------------------------------------------------------------------------------------------ */
inline string& string::assign(size_type n, value_type c)
{
    size_type new_size = length_check(0, n, "string::assign");

    grow_buffer_to(new_size);

    set_size(new_size);
    if (new_size)
    {
        traits_type::assign(get_pointer(0), new_size, c);
    }

    return *this;
}

inline string& string::assign(const string& str)
{
    // Assigning an object to itself has no effect
    if (this != &str)
    {
        return assign_internal(str.data(), str.size());
    }

    return *this;
}

inline string& string::assign(const string& str, size_type pos, size_type n)
{
    str.range_check(pos, str.size(), "string::assign");

    size_type new_size = ::carb_min(n, str.size() - pos);

    return assign_internal(str.data() + pos, new_size);
}

inline string& string::assign(string&& str)
{
    return *this = std::move(str);
}

inline string& string::assign(const value_type* s, size_type n)
{
    ::omni::detail::null_check(s, "string::assign");
    return assign_internal(s, n);
}

inline string& string::assign(const value_type* s)
{
    ::omni::detail::null_check(s, "string::assign");
    return assign_internal(s, traits_type::length(s));
}

template <class InputIterator>
string& string::assign(InputIterator first, InputIterator last)
{
    return assign_internal(first, last, std::distance(first, last));
}

inline string& string::assign(std::initializer_list<value_type> ilist)
{
    return assign_internal(ilist.begin(), ilist.end(), ilist.size());
}

inline string& string::assign(const std::string& str)
{
    return assign_internal(str.data(), str.size());
}

inline string& string::assign(const std::string& str, size_type pos, size_type n)
{
    range_check(pos, str.size(), "string::assign");

    size_type new_size = ::carb_min(n, str.size() - pos);

    return assign_internal(str.data() + pos, new_size);
}

inline string& string::assign(const carb::cpp17::string_view& sv)
{
    return assign_internal(sv.data(), sv.size());
}

inline string& string::assign(const carb::cpp17::string_view& sv, size_type pos, size_type n)
{
    range_check(pos, sv.size(), "string::assign");

    size_type new_size = ::carb_min(n, sv.size() - pos);

    return assign_internal(sv.data() + pos, new_size);
}

#if CARB_HAS_CPP17
template <typename T, typename>
string& string::assign(const T& t)
{
    const std::string_view sv = t;
    return assign_internal(sv.data(), sv.size());
}

template <typename T, typename>
string& string::assign(const T& t, size_type pos, size_type n)
{
    const std::string_view sv = t;
    range_check(pos, sv.size(), "string::assign");

    size_type new_size = ::carb_min(n, sv.size() - pos);

    return assign_internal(sv.data() + pos, new_size);
}
#endif

inline string& string::assign_printf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    CARB_SCOPE_EXIT
    {
        va_end(ap);
    };
    return assign_vprintf(fmt, ap);
}

inline string& string::assign_vprintf(const char* fmt, va_list ap)
{
    ::omni::detail::null_check(fmt, "string::assign_vprintf");
    overlap_check(fmt);

    va_list ap2;
    va_copy(ap2, ap);
    CARB_SCOPE_EXIT
    {
        va_end(ap2);
    };

    // Measure the string first
    size_type fmt_size = vsnprintf_check(nullptr, 0, fmt, ap);

    length_check(0, fmt_size, "assign_vprintf");

    allocate_if_necessary(fmt_size);
    int check = std::vsnprintf(get_pointer(0), fmt_size + 1, fmt, ap2);
    OMNI_FATAL_UNLESS(check >= 0, "Unrecoverable error: vsnprintf failed");

    set_size(fmt_size);

    return *this;
}

/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                   Element Access                                                   */
/* ------------------------------------------------------------------------------------------------------------------ */
constexpr string::reference string::at(size_type pos)
{
    range_check(pos, size(), "string::at");
    return get_reference(pos);
}

constexpr string::const_reference string::at(size_type pos) const
{
    range_check(pos, size(), "string::at");
    return get_reference(pos);
}

constexpr string::reference string::operator[](size_type pos)
{
    return get_reference(pos);
}

constexpr string::const_reference string::operator[](size_type pos) const
{
    return get_reference(pos);
}

constexpr string::reference string::front()
{
    return operator[](0);
}

constexpr string::const_reference string::front() const
{
    return operator[](0);
}

constexpr string::reference string::back()
{
    return operator[](size() - 1);
}

constexpr string::const_reference string::back() const
{
    return operator[](size() - 1);
}

constexpr const string::value_type* string::data() const noexcept
{
    return get_pointer(0);
}

constexpr string::value_type* string::data() noexcept
{
    return get_pointer(0);
}

constexpr const string::value_type* string::c_str() const noexcept
{
    return get_pointer(0);
}

constexpr string::operator carb::cpp17::string_view() const noexcept
{
    return carb::cpp17::string_view(data(), size());
}

#if CARB_HAS_CPP17
constexpr string::operator std::string_view() const noexcept
{
    return std::string_view(data(), size());
}
#endif

/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                      Iterators                                                     */
/* ------------------------------------------------------------------------------------------------------------------ */

constexpr string::iterator string::begin() noexcept
{
    return iterator(get_pointer(0));
}

constexpr string::const_iterator string::begin() const noexcept
{
    return const_iterator(get_pointer(0));
}

constexpr string::const_iterator string::cbegin() const noexcept
{
    return const_iterator(get_pointer(0));
}

constexpr string::iterator string::end() noexcept
{
    return iterator(get_pointer(0) + size());
}

constexpr string::const_iterator string::end() const noexcept
{
    return const_iterator(get_pointer(0) + size());
}

constexpr string::const_iterator string::cend() const noexcept
{
    return const_iterator(get_pointer(0) + size());
}

inline string::reverse_iterator string::rbegin() noexcept
{
    return reverse_iterator(end());
}

inline string::const_reverse_iterator string::rbegin() const noexcept
{
    return const_reverse_iterator(end());
}

inline string::const_reverse_iterator string::crbegin() const noexcept
{
    return const_reverse_iterator(end());
}

inline string::reverse_iterator string::rend() noexcept
{
    return reverse_iterator(begin());
}

inline string::const_reverse_iterator string::rend() const noexcept
{
    return const_reverse_iterator(begin());
}

inline string::const_reverse_iterator string::crend() const noexcept
{
    return const_reverse_iterator(begin());
}

/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                      Capacity                                                      */
/* ------------------------------------------------------------------------------------------------------------------ */

constexpr bool string::empty() const noexcept
{
    return size() == 0;
}

constexpr string::size_type string::size() const noexcept
{
    if (is_local())
    {
        return kSMALL_STRING_SIZE - m_local_data[kSMALL_SIZE_OFFSET] - 1;
    }
    else
    {
        return m_allocated_data.m_size;
    }
}

constexpr string::size_type string::length() const noexcept
{
    return size();
}

constexpr string::size_type string::max_size() const noexcept
{
    return std::numeric_limits<size_type>::max() - 1;
}

inline void string::reserve(size_type new_cap)
{
    // Don't shrink below size
    if (new_cap < size())
    {
        new_cap = size();
    }

    if (new_cap != capacity())
    {
        // If the new capacity is greater than the current capacity, or if it is smaller (so we're shrinking), but does
        // not fit in the local buffer, we need to allocate
        if (new_cap > capacity() || new_cap >= kSMALL_STRING_SIZE)
        {
            // Allocate new buffer first in case the allocation throws
            pointer tmp_ptr = allocate_buffer(capacity(), new_cap);
            size_type tmp_size = size();
            // Copy to new buffer, including null terminator
            std::memcpy(tmp_ptr, get_pointer(0), size() + 1);
            dispose();
            set_allocated();
            m_allocated_data.m_ptr = tmp_ptr;
            m_allocated_data.m_capacity = new_cap;
            m_allocated_data.m_size = tmp_size;
        }
        else if (!is_local())
        {
            // The case where we have allocated a buffer, but our size could now fit in the local buffer, and the new
            // capacity can fit in the local buffer.
            pointer tmp_ptr = m_allocated_data.m_ptr;
            size_type tmp_size = m_allocated_data.m_size;

            // Mark the string as local and copy the data to the local buffer
            set_local(tmp_size);
            std::memcpy(get_pointer(0), tmp_ptr, tmp_size);
            set_size(tmp_size);

            carb::deallocate(tmp_ptr);
        }
    }
}

inline void string::reserve()
{
    reserve(0);
}

constexpr string::size_type string::capacity() const noexcept
{
    if (is_local())
    {
        return kSMALL_STRING_SIZE - 1;
    }
    else
    {
        return m_allocated_data.m_capacity;
    }
}

inline void string::shrink_to_fit()
{
    if (capacity() > size())
    {
        reserve(0);
    }
}

/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                     Operations                                                     */
/* ------------------------------------------------------------------------------------------------------------------ */

constexpr void string::clear() noexcept
{
    set_size(0);
}

inline string& string::insert(size_type pos, size_type n, value_type c)
{
    return insert_internal(pos, c, n);
}

inline string& string::insert(size_type pos, const value_type* s)
{
    ::omni::detail::null_check(s, "string::insert");
    return insert_internal(pos, s, traits_type::length(s));
}

inline string& string::insert(size_type pos, const value_type* s, size_type n)
{
    ::omni::detail::null_check(s, "string::insert");
    return insert_internal(pos, s, n);
}

inline string& string::insert(size_type pos, const string& str)
{
    return insert_internal(pos, str.data(), str.size());
}

inline string& string::insert(size_type pos1, const string& str, size_type pos2, size_type n)
{
    str.range_check(pos2, str.size(), "string::insert");
    size_type substr_size = ::carb_min(n, (str.size() - pos2));
    return insert_internal(pos1, str.get_pointer(pos2), substr_size);
}

inline string::iterator string::insert(const_iterator p, value_type c)
{
    range_check(p, "string::insert");
    size_type pos = std::distance(cbegin(), p);

    insert_internal(pos, c, 1);
    return iterator(get_pointer(pos));
}

inline string::iterator string::insert(const_iterator p, size_type n, value_type c)
{
    range_check(p, "string::insert");
    size_type pos = std::distance(cbegin(), p);

    insert_internal(pos, c, n);
    return iterator(get_pointer(pos));
}

template <class InputIterator>
string::iterator string::insert(const_iterator p, InputIterator first, InputIterator last)
{
    range_check(p, "string::insert");
    size_type pos = std::distance(cbegin(), p);

    size_type n = std::distance(first, last);
    length_check(size(), n, "string::insert");

    // Reuse the logic in replace to handle potentially overlapping regions
    replace(p, p, first, last);

    return iterator(get_pointer(pos));
}

inline string::iterator string::insert(const_iterator p, std::initializer_list<value_type> ilist)
{
    return insert(p, ilist.begin(), ilist.end());
}

inline string& string::insert(size_type pos, const std::string& str)
{
    return insert_internal(pos, str.data(), str.size());
}

inline string& string::insert(size_type pos1, const std::string& str, size_type pos2, size_type n)
{
    range_check(pos2, str.size(), "string::insert");
    size_type substr_size = ::carb_min(n, (str.size() - pos2));
    return insert_internal(pos1, str.data() + pos2, substr_size);
}

inline string& string::insert(size_type pos, const carb::cpp17::string_view& sv)
{
    return insert_internal(pos, sv.data(), sv.size());
}

inline string& string::insert(size_type pos1, const carb::cpp17::string_view& sv, size_type pos2, size_type n)
{
    range_check(pos2, sv.size(), "string::insert");
    size_type substr_size = ::carb_min(n, (sv.size() - pos2));
    return insert_internal(pos1, sv.data() + pos2, substr_size);
}


#if CARB_HAS_CPP17
template <typename T, typename>
string& string::insert(size_type pos, const T& t)
{
    const std::string_view sv = t;
    return insert_internal(pos, sv.data(), sv.size());
}

template <typename T, typename>
string& string::insert(size_type pos1, const T& t, size_type pos2, size_type n)
{
    const std::string_view sv = t;
    range_check(pos2, sv.size(), "string::insert");
    size_type substr_size = ::carb_min(n, (sv.size() - pos2));
    return insert_internal(pos1, sv.data() + pos2, substr_size);
}
#endif

inline string& string::insert_printf(size_type pos, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    CARB_SCOPE_EXIT
    {
        va_end(ap);
    };
    return insert_vprintf(pos, fmt, ap);
}

inline string& string::insert_vprintf(size_type pos, const char* fmt, va_list ap)
{
    ::omni::detail::null_check(fmt, "string::insert_vprintf");
    overlap_check(fmt);
    range_check(pos, size(), "string::insert_vprintf");

    va_list ap2;
    va_copy(ap2, ap);
    CARB_SCOPE_EXIT
    {
        va_end(ap2);
    };

    // Measure first
    size_type fmt_size = vsnprintf_check(nullptr, 0, fmt, ap);
    size_type new_size = length_check(size(), fmt_size, "string::insert_vprintf");

    grow_buffer_to(new_size);

    size_type to_copy = size() - pos;
    pointer copy_start = get_pointer(pos);
    pointer copy_dest = copy_start + fmt_size;
    std::memmove(copy_dest, copy_start, to_copy);
    value_type c = *copy_dest; // vsnprintf will overwrite the first character copied with the NUL, so save it.
    int check = std::vsnprintf(copy_start, fmt_size + 1, fmt, ap2);
    OMNI_FATAL_UNLESS(check >= 0, "Unrecoverable error: vsnprintf failed");
    *copy_dest = c;

    set_size(new_size);

    return *this;
}

inline string::iterator string::insert_printf(const_iterator p, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    CARB_SCOPE_EXIT
    {
        va_end(ap);
    };
    return insert_vprintf(p, fmt, ap);
}

inline string::iterator string::insert_vprintf(const_iterator p, const char* fmt, va_list ap)
{
    ::omni::detail::null_check(fmt, "string::insert_vprintf");
    overlap_check(fmt);

    va_list ap2;
    va_copy(ap2, ap);
    CARB_SCOPE_EXIT
    {
        va_end(ap2);
    };

    // Measure first
    size_type fmt_size = vsnprintf_check(nullptr, 0, fmt, ap);

    size_type pos = std::distance(cbegin(), p);
    range_check(pos, size(), "string::insert_vprintf");

    size_type new_size = length_check(size(), fmt_size, "string::insert_vprintf");

    grow_buffer_to(new_size);

    size_type to_copy = size() - pos;
    pointer copy_start = get_pointer(pos);
    pointer copy_dest = copy_start + fmt_size;
    std::memmove(copy_dest, copy_start, to_copy);
    value_type c = *copy_dest; // vsnprintf will overwrite the first character copied with the NUL, so save it.
    int check = std::vsnprintf(copy_start, fmt_size + 1, fmt, ap2);
    OMNI_FATAL_UNLESS(check >= 0, "Unrecoverable error: vsnprintf failed");
    *copy_dest = c;

    set_size(new_size);

    return iterator(copy_start);
}

constexpr string& string::erase(size_type pos, size_type n)
{
    range_check(pos, size(), "string::erase");

    // Erase to end of the string
    if (n >= (size() - pos))
    {
        set_size(pos);
    }
    else if (n != 0)
    {
        size_type remainder = size() - pos - n;
        if (remainder)
        {
            traits_type::move(get_pointer(pos), get_pointer(pos + n), remainder);
        }
        set_size(size() - n);
    }

    return *this;
}

constexpr string::iterator string::erase(const_iterator pos)
{
    if (pos < cbegin())
    {
        return end();
    }
    size_type start = pos - cbegin();
    if (start >= size())
    {
        return end();
    }

    size_type remainder = size() - start - 1;
    if (remainder)
    {
        traits_type::move(get_pointer(start), get_pointer(start + 1), remainder);
    }
    set_size(size() - 1);
    return iterator(get_pointer(start));
}

constexpr string::iterator string::erase(const_iterator first, const_iterator last)
{
    range_check(first, last, "string::erase");
    size_type pos = first - cbegin();

    if (last == end())
    {
        set_size(pos);
    }
    else
    {
        erase(pos, (last - first));
    }

    return iterator(get_pointer(pos));
}

inline void string::push_back(value_type c)
{
    grow_buffer_to(size() + 1);
    traits_type::assign(get_reference(size()), c);
    set_size(size() + 1);
}

constexpr void string::pop_back()
{
#if CARB_EXCEPTIONS_ENABLED
    if (empty())
    {
        throw std::runtime_error("string::pop_back called on empty string");
    }
#else
    CARB_FATAL_UNLESS(!empty(), "string::pop_back called on empty string");
#endif
    erase(size() - 1, 1);
}

inline string& string::append(size_type n, value_type c)
{
    size_type new_size = length_check(size(), n, "string::append");

    grow_buffer_to(new_size);

    traits_type::assign(get_pointer(size()), n, c);
    set_size(new_size);

    return *this;
}

inline string& string::append(const string& str)
{
    return append_internal(str.data(), str.size());
}

inline string& string::append(const string& str, size_type pos, size_type n)
{
    str.range_check(pos, str.size(), "string::append");
    size_type substr_size = ::carb_min(n, (str.size() - pos));
    return append_internal(str.get_pointer(pos), substr_size);
}

inline string& string::append(const value_type* s, size_type n)
{
    ::omni::detail::null_check(s, "string::append");
    return append_internal(s, n);
}

inline string& string::append(const value_type* s)
{
    ::omni::detail::null_check(s, "string::append");
    return append_internal(s, traits_type::length(s));
}

template <class InputIterator>
string& string::append(InputIterator first, InputIterator last)
{
    size_type new_size = length_check(size(), std::distance(first, last), "string::append");

    if (new_size <= capacity())
    {
        for (auto ptr = get_pointer(size()); first != last; ++first, ++ptr)
        {
            traits_type::assign(*ptr, *first);
        }

        set_size(new_size);
    }
    else
    {
        grow_buffer_and_append(new_size, first, last);
    }

    return *this;
}

inline string& string::append(std::initializer_list<value_type> ilist)
{
    return append(ilist.begin(), ilist.end());
}

inline string& string::append(const std::string& str)
{
    return append_internal(str.data(), str.size());
}

inline string& string::append(const std::string& str, size_type pos, size_type n)
{
    range_check(pos, str.size(), "string::append");
    size_type substr_size = ::carb_min(n, (str.size() - pos));
    return append_internal(str.data() + pos, substr_size);
}

inline string& string::append(const carb::cpp17::string_view& sv)
{
    return append_internal(sv.data(), sv.size());
}

inline string& string::append(const carb::cpp17::string_view& sv, size_type pos, size_type n)
{
    range_check(pos, sv.size(), "string::append");
    size_type substr_size = ::carb_min(n, (sv.size() - pos));
    return append_internal(sv.data() + pos, substr_size);
}

#if CARB_HAS_CPP17
template <typename T, typename>
string& string::append(const T& t)
{
    std::string_view sv = t;
    return append_internal(sv.data(), sv.size());
}

template <typename T, typename>
string& string::append(const T& t, size_type pos, size_type n)
{
    std::string_view sv = t;
    range_check(pos, sv.size(), "string::append");
    size_type substr_size = ::carb_min(n, (sv.size() - pos));
    return append_internal(sv.data() + pos, substr_size);
}
#endif

inline string& string::append_printf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    CARB_SCOPE_EXIT
    {
        va_end(ap);
    };
    return append_vprintf(fmt, ap);
}

inline string& string::append_vprintf(const char* fmt, va_list ap)
{
    ::omni::detail::null_check(fmt, "string::append_vprintf");
    overlap_check(fmt);

    va_list ap2;
    va_copy(ap2, ap);
    CARB_SCOPE_EXIT
    {
        va_end(ap2);
    };

    // Measure first
    size_type fmt_size = vsnprintf_check(nullptr, 0, fmt, ap);

    size_type new_size = length_check(size(), fmt_size, "string::append_vprintf");

    grow_buffer_to(new_size);
    int check = std::vsnprintf(get_pointer(size()), fmt_size + 1, fmt, ap2);
    OMNI_FATAL_UNLESS(check >= 0, "Unrecoverable error: vsnprintf failed");

    set_size(new_size);

    return *this;
}

inline string& string::operator+=(const string& str)
{
    return append(str);
}

inline string& string::operator+=(value_type c)
{
    return append(1, c);
}

inline string& string::operator+=(const value_type* s)
{
    ::omni::detail::null_check(s, "string::operator+=");
    return append(s);
}

inline string& string::operator+=(std::initializer_list<value_type> ilist)
{
    return append(ilist);
}

inline string& string::operator+=(const std::string& str)
{
    return append(str);
}

inline string& string::operator+=(const carb::cpp17::string_view& sv)
{
    return append(sv);
}

#if CARB_HAS_CPP17
template <typename T, typename>
inline string& string::operator+=(const T& t)
{
    return append(t);
}
#endif

constexpr int string::compare(const string& str) const noexcept
{
    return compare_internal(get_pointer(0), str.data(), size(), str.size());
}

constexpr int string::compare(size_type pos1, size_type n1, const string& str) const
{
    range_check(pos1, size(), "string::compare");

    size_type this_size = ::carb_min(n1, size() - pos1);
    return compare_internal(get_pointer(pos1), str.data(), this_size, str.size());
}

constexpr int string::compare(size_type pos1, size_type n1, const string& str, size_type pos2, size_type n2) const
{
    range_check(pos1, size(), "string::compare");
    str.range_check(pos2, str.size(), "string::compare");

    size_type this_size = ::carb_min(n1, size() - pos1);
    size_type other_size = ::carb_min(n2, str.size() - pos2);
    return compare_internal(get_pointer(pos1), str.get_pointer(pos2), this_size, other_size);
}

constexpr int string::compare(const value_type* s) const
{
    ::omni::detail::null_check(s, "string::compare");
    return compare_internal(get_pointer(0), s, size(), traits_type::length(s));
}

constexpr int string::compare(size_type pos1, size_type n1, const value_type* s) const
{
    range_check(pos1, size(), "string::compare");
    ::omni::detail::null_check(s, "string::compare");

    size_type this_size = ::carb_min(n1, size() - pos1);
    return compare_internal(get_pointer(pos1), s, this_size, traits_type::length(s));
}

constexpr int string::compare(size_type pos1, size_type n1, const value_type* s, size_type n2) const
{
    range_check(pos1, size(), "string::compare");
    ::omni::detail::null_check(s, "string::compare");

    size_type this_size = ::carb_min(n1, size() - pos1);
    return compare_internal(get_pointer(pos1), s, this_size, n2);
}

CARB_CPP20_CONSTEXPR inline int string::compare(const std::string& str) const noexcept
{
    return compare_internal(get_pointer(0), str.data(), size(), str.size());
}

CARB_CPP20_CONSTEXPR inline int string::compare(size_type pos1, size_type n1, const std::string& str) const
{
    range_check(pos1, size(), "string::compare");

    size_type this_size = ::carb_min(n1, size() - pos1);
    return compare_internal(get_pointer(pos1), str.data(), this_size, str.size());
}

CARB_CPP20_CONSTEXPR inline int string::compare(
    size_type pos1, size_type n1, const std::string& str, size_type pos2, size_type n2) const
{
    range_check(pos1, size(), "string::compare");
    range_check(pos2, str.size(), "string::compare");

    size_type this_size = ::carb_min(n1, size() - pos1);
    size_type other_size = ::carb_min(n2, str.size() - pos2);
    return compare_internal(get_pointer(pos1), str.data() + pos2, this_size, other_size);
}

constexpr int string::compare(const carb::cpp17::string_view& sv) const noexcept
{
    return compare_internal(get_pointer(0), sv.data(), size(), sv.size());
}

constexpr int string::compare(size_type pos1, size_type n1, const carb::cpp17::string_view& sv) const
{
    range_check(pos1, size(), "string::compare");

    size_type this_size = ::carb_min(n1, size() - pos1);
    return compare_internal(get_pointer(pos1), sv.data(), this_size, sv.size());
}

constexpr int string::compare(
    size_type pos1, size_type n1, const carb::cpp17::string_view& sv, size_type pos2, size_type n2) const
{
    range_check(pos1, size(), "string::compare");
    range_check(pos2, sv.size(), "string::compare");

    size_type this_size = ::carb_min(n1, size() - pos1);
    size_type other_size = ::carb_min(n2, sv.size() - pos2);
    return compare_internal(get_pointer(pos1), sv.data() + pos2, this_size, other_size);
}

#if CARB_HAS_CPP17
template <typename T, typename>
constexpr int string::compare(const T& t) const noexcept
{
    std::string_view sv = t;
    return compare_internal(get_pointer(0), sv.data(), size(), sv.size());
}

template <typename T, typename>
constexpr int string::compare(size_type pos1, size_type n1, const T& t) const
{
    std::string_view sv = t;
    range_check(pos1, size(), "string::compare");

    size_type this_size = ::carb_min(n1, size() - pos1);
    return compare_internal(get_pointer(pos1), sv.data(), this_size, sv.size());
}

template <typename T, typename>
constexpr int string::compare(size_type pos1, size_type n1, const T& t, size_type pos2, size_type n2) const
{
    std::string_view sv = t;
    range_check(pos1, size(), "string::compare");
    range_check(pos2, sv.size(), "string::compare");

    size_type this_size = ::carb_min(n1, size() - pos1);
    size_type other_size = ::carb_min(n2, sv.size() - pos2);
    return compare_internal(get_pointer(pos1), sv.data() + pos2, this_size, other_size);
}
#endif

constexpr bool string::starts_with(value_type c) const noexcept
{
    if (!empty())
    {
        return get_reference(0) == c;
    }

    return false;
}

constexpr bool string::starts_with(const_pointer s) const
{
    ::omni::detail::null_check(s, "string::starts_with");
    size_type length = traits_type::length(s);

    if (size() >= length)
    {
        return traits_type::compare(get_pointer(0), s, length) == 0;
    }

    return false;
}

constexpr bool string::starts_with(carb::cpp17::string_view sv) const noexcept
{
    if (size() >= sv.size())
    {
        return traits_type::compare(get_pointer(0), sv.data(), sv.size()) == 0;
    }

    return false;
}

#if CARB_HAS_CPP17
constexpr bool string::starts_with(std::string_view sv) const noexcept
{
    if (size() >= sv.size())
    {
        return traits_type::compare(get_pointer(0), sv.data(), sv.size()) == 0;
    }

    return false;
}
#endif

constexpr bool string::ends_with(value_type c) const noexcept
{
    if (!empty())
    {
        return get_reference(size() - 1) == c;
    }

    return false;
}

constexpr bool string::ends_with(const_pointer s) const
{
    ::omni::detail::null_check(s, "string::ends_with");
    size_type length = traits_type::length(s);

    if (size() >= length)
    {
        return traits_type::compare(get_pointer(size() - length), s, length) == 0;
    }

    return false;
}

constexpr bool string::ends_with(carb::cpp17::string_view sv) const noexcept
{
    if (size() >= sv.size())
    {
        return traits_type::compare(get_pointer(size() - sv.size()), sv.data(), sv.size()) == 0;
    }

    return false;
}

#if CARB_HAS_CPP17
constexpr bool string::ends_with(std::string_view sv) const noexcept
{
    if (size() >= sv.size())
    {
        return traits_type::compare(get_pointer(size() - sv.size()), sv.data(), sv.size()) == 0;
    }

    return false;
}
#endif

constexpr bool string::contains(value_type c) const noexcept
{
    return find(c) != npos;
}

constexpr bool string::contains(const_pointer s) const
{
    ::omni::detail::null_check(s, "string::contains");
    return find(s) != npos;
}

constexpr bool string::contains(carb::cpp17::string_view sv) const noexcept
{
    return find(sv) != npos;
}

#if CARB_HAS_CPP17
template <typename T, typename>
constexpr string::size_type string::find(const T& t, size_type pos) const noexcept
{
    std::string_view sv = t;
    return find(sv.data(), pos, sv.size());
}

constexpr bool string::contains(std::string_view sv) const noexcept
{
    return find(sv) != npos;
}
#endif

inline string& string::replace(size_type pos1, size_type n1, const string& str)
{
    return replace(pos1, n1, str.data(), str.size());
}

inline string& string::replace(size_type pos1, size_type n1, const string& str, size_type pos2, size_type n2)
{
    str.range_check(pos2, str.size(), "string::replace");

    size_type replacement_size = ::carb_min(n2, (str.size() - pos2));
    return replace(pos1, n1, str.data() + pos2, replacement_size);
}

template <class InputIterator>
string& string::replace(const_iterator i1, const_iterator i2, InputIterator j1, InputIterator j2)
{
    range_check(i1, i2, "string::replace");
    size_type pos = std::distance(cbegin(), i1);
    size_type n1 = std::distance(i1, i2);

#if CARB_HAS_CPP17
    // If the InputIterator is a string::iterator, we can skip making the temporary string and convert the iterator to a
    // pointer and use that directly.
    if constexpr (std::is_same_v<InputIterator, iterator>)
    {
        return replace(pos, n1, j1.operator->(), std::distance(j1, j2));
    }
    else
    {
        // Make a temporary string and use that as the replacement string
        string tmp(j1, j2);
        return replace(pos, n1, tmp.data(), tmp.size());
    }
#else
    string tmp(j1, j2);
    return replace(pos, n1, tmp.data(), tmp.size());
#endif
}

inline string& string::replace(size_type pos, size_type n1, const value_type* s, size_type n2)
{
    ::omni::detail::null_check(s, "string::replace");

    size_type replaced_size = ::carb_min(n1, (size() - pos));
    size_type replacement_size = n2;
    size_type new_size = size() - replaced_size + replacement_size;
    size_type remaining_chars = size() - (pos + replaced_size);

    pointer replacement_start = get_pointer(pos);

    if (new_size <= capacity())
    {
        // If the replacement string is part of this string, and things fit into the current capacity, we do the
        // replacement in place
        if (overlaps_this_string(s))
        {
            if (replacement_size > 0 && replacement_size <= replaced_size)
            {
                std::memmove(replacement_start, s, replacement_size);
            }
            if (remaining_chars && replacement_size != replaced_size)
            {
                std::memmove(replacement_start + replacement_size, replacement_start + replaced_size, remaining_chars);
            }
            if (replacement_size > replaced_size)
            {
                // We may need to account for the replacement characters being shifted by the move above to shift the
                // remaining characters to the end to make room for the larger replacement string. There are three
                // possible scenarios for this:

                // Replacement characters not shifted at all
                if ((uintptr_t)(s + replacement_size) <= (uintptr_t)(replacement_start + replaced_size))
                {
                    std::memmove(replacement_start, s, replacement_size);
                }
                // Replacement characters entirely shifted
                else if (((uintptr_t)(s) >= (uintptr_t)(replacement_start + replaced_size)))
                {
                    std::memcpy(replacement_start, s + (replacement_size - replaced_size), replacement_size);
                }
                // Replacement characters split between shifted and non shifted
                else
                {
                    size_type non_shifted_amount = uintptr_t(replacement_start + replaced_size) - (uintptr_t)s;
                    std::memmove(replacement_start, s, non_shifted_amount);
                    std::memcpy(replacement_start + non_shifted_amount, replacement_start + replacement_size,
                                replacement_size - non_shifted_amount);
                }
            }
        }
        else
        {
            // If there's no overlap between the replacement characters and this string, we can simply make room for the
            // replacement characters and copy them in. Move remaining characters to their new location
            if (remaining_chars)
            {
                std::memmove(replacement_start + replacement_size, replacement_start + replaced_size, remaining_chars);
            }

            // Move in the replacement characters
            std::memcpy(replacement_start, s, replacement_size);
        }
    }
    else
    {
        // We need to grow the buffer. This function correctly copies the characters from the existing buffer into the
        // new buffer before disposing the existing buffer.
        grow_buffer_and_fill(new_size, get_pointer(0), pos, s, replacement_size,
                             remaining_chars ? get_pointer(pos + replaced_size) : nullptr, remaining_chars);
    }

    set_size(new_size);
    return *this;
}

inline string& string::replace(size_type pos, size_type n1, const value_type* s)
{
    ::omni::detail::null_check(s, "string::replace");
    size_type replacement_size = traits_type::length(s);
    return replace(pos, n1, s, replacement_size);
}

inline string& string::replace(size_type pos, size_type n1, size_type n2, value_type c)
{
    replace_setup(pos, n1, n2);
    traits_type::assign(get_pointer(pos), n2, c);
    return *this;
}

inline string& string::replace(size_type pos1, size_type n1, const std::string& str)
{
    return replace(pos1, n1, str.data(), str.size());
}

inline string& string::replace(size_type pos1, size_type n1, const std::string& str, size_type pos2, size_type n2)
{
    range_check(pos2, str.size(), "string::replace");

    size_type replacement_size = ::carb_min(n2, (str.size() - pos2));
    return replace(pos1, n1, str.data() + pos2, replacement_size);
}

inline string& string::replace(size_type pos1, size_type n1, const carb::cpp17::string_view& sv)
{
    return replace(pos1, n1, sv.data(), sv.size());
}

inline string& string::replace(size_type pos1, size_type n1, const carb::cpp17::string_view& sv, size_type pos2, size_type n2)
{
    range_check(pos2, sv.size(), "string::replace");

    size_type replacement_size = ::carb_min(n2, (sv.size() - pos2));
    return replace(pos1, n1, sv.data() + pos2, replacement_size);
}

#if CARB_HAS_CPP17
template <typename T, typename>
string& string::replace(size_type pos1, size_type n1, const T& t)
{
    std::string_view sv = t;
    return replace(pos1, n1, sv.data(), sv.size());
}

template <typename T, typename>
string& string::replace(size_type pos1, size_type n1, const T& t, size_type pos2, size_type n2)
{
    std::string_view sv = t;
    range_check(pos2, sv.size(), "string::replace");

    size_type replacement_size = ::carb_min(n2, (sv.size() - pos2));
    return replace(pos1, n1, sv.data() + pos2, replacement_size);
}
#endif

inline string& string::replace(const_iterator i1, const_iterator i2, const string& str)
{
    range_check(i1, i2, "string::replace");
    size_type pos = std::distance(cbegin(), i1);
    size_type n1 = std::distance(i1, i2);
    return replace(pos, n1, str.data(), str.size());
}

inline string& string::replace(const_iterator i1, const_iterator i2, const value_type* s, size_type n)
{
    range_check(i1, i2, "string::replace");
    ::omni::detail::null_check(s, "string::replace");
    size_type pos = std::distance(cbegin(), i1);
    size_type n1 = std::distance(i1, i2);
    return replace(pos, n1, s, n);
}

inline string& string::replace(const_iterator i1, const_iterator i2, const value_type* s)
{
    range_check(i1, i2, "string::replace");
    ::omni::detail::null_check(s, "string::replace");
    size_type replacement_size = traits_type::length(s);
    size_type pos = std::distance(cbegin(), i1);
    size_type n1 = std::distance(i1, i2);
    return replace(pos, n1, s, replacement_size);
}

inline string& string::replace(const_iterator i1, const_iterator i2, size_type n, value_type c)
{
    range_check(i1, i2, "string::replace");
    size_type pos = std::distance(cbegin(), i1);
    size_type n1 = std::distance(i1, i2);
    replace_setup(pos, n1, n);
    traits_type::assign(get_pointer(pos), n, c);
    return *this;
}

inline string& string::replace(const_iterator i1, const_iterator i2, std::initializer_list<value_type> ilist)
{
    return replace(i1, i2, ilist.begin(), ilist.end());
}

inline string& string::replace(const_iterator i1, const_iterator i2, const std::string& str)
{
    range_check(i1, i2, "string::replace");
    size_type pos = std::distance(cbegin(), i1);
    size_type n1 = std::distance(i1, i2);
    return replace(pos, n1, str.data(), str.size());
}

inline string& string::replace(const_iterator i1, const_iterator i2, const carb::cpp17::string_view& sv)
{
    range_check(i1, i2, "string::replace");
    size_type pos = std::distance(cbegin(), i1);
    size_type n1 = std::distance(i1, i2);
    return replace(pos, n1, sv.data(), sv.size());
}

#if CARB_HAS_CPP17
template <typename T, typename>
string& string::replace(const_iterator i1, const_iterator i2, const T& t)
{
    std::string_view sv = t;
    range_check(i1, i2, "string::replace");
    size_type pos = std::distance(cbegin(), i1);
    size_type n1 = std::distance(i1, i2);
    return replace(pos, n1, sv.data(), sv.size());
}
#endif

inline string& string::replace_format(size_type pos, size_type n1, const value_type* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    CARB_SCOPE_EXIT
    {
        va_end(ap);
    };
    return replace_vformat(pos, n1, fmt, ap);
}

inline string& string::replace_vformat(size_type pos, size_type n1, const value_type* fmt, va_list ap)
{
    ::omni::detail::null_check(fmt, "string::replace_vformat");
    overlap_check(fmt);

    va_list ap2;
    va_copy(ap2, ap);
    CARB_SCOPE_EXIT
    {
        va_end(ap2);
    };

    // Measure first
    size_type fmt_size = vsnprintf_check(nullptr, 0, fmt, ap);

    replace_setup(pos, n1, fmt_size);

    // vsnprintf will overwrite the last character with a NUL, so save it
    value_type c = *get_pointer(pos + fmt_size);
    int check = std::vsnprintf(get_pointer(pos), fmt_size + 1, fmt, ap2);
    OMNI_FATAL_UNLESS(check >= 0, "Unrecoverable error: vsnprintf failed");
    *get_pointer(pos + fmt_size) = c;

    return *this;
}

inline string& string::replace_format(const_iterator i1, const_iterator i2, const value_type* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    CARB_SCOPE_EXIT
    {
        va_end(ap);
    };
    return replace_vformat(i1, i2, fmt, ap);
}

inline string& string::replace_vformat(const_iterator i1, const_iterator i2, const value_type* fmt, va_list ap)
{
    ::omni::detail::null_check(fmt, "string::replace_vformat");
    range_check(i1, i2, "string::replace_format");
    size_type pos = std::distance(cbegin(), i1);
    size_type n1 = std::distance(i1, i2);
    return replace_vformat(pos, n1, fmt, ap);
}

inline string string::substr(size_type pos, size_type n) const
{
    range_check(pos, size(), "string::substr");

    size_type substr_size = ::carb_min(n, (size() - pos));
    return string(get_pointer(pos), substr_size);
}

constexpr string::size_type string::copy(value_type* s, size_type n, size_type pos) const
{
    ::omni::detail::null_check(s, "string::copy");
    range_check(pos, size(), "string::copy");

    size_type to_copy = ::carb_min(n, (size() - pos));
    traits_type::copy(s, get_pointer(pos), to_copy);
    return to_copy;
}

inline void string::resize(size_type n, value_type c)
{
    if (n < size())
    {
        set_size(n);
    }
    else if (n > size())
    {
        append((n - size()), c);
    }
}

inline void string::resize(size_type n)
{
    resize(n, value_type());
}

inline void string::swap(string& str) noexcept
{
    string tmp(std::move(*this));
    *this = std::move(str);
    str = std::move(tmp);
}

/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                       Search                                                       */
/* ------------------------------------------------------------------------------------------------------------------ */

constexpr string::size_type string::find(const string& str, size_type pos) const noexcept
{
    return find(str.data(), pos, str.size());
}

constexpr string::size_type string::find(const value_type* s, size_type pos, size_type n) const
{
    ::omni::detail::null_check(s, "string::find");
    if (n == 0)
    {
        return pos <= size() ? pos : npos;
    }
    if (pos >= size())
    {
        return npos;
    }

    const value_type first_element = s[0];
    const_pointer search_start = get_pointer(pos);
    const_pointer const last_char = get_pointer(size());
    size_type remaining_length = size() - pos;

    while (remaining_length >= n)
    {
        // Look for the first character
        search_start = traits_type::find(search_start, remaining_length - n + 1, first_element);

        // Not found
        if (search_start == nullptr)
        {
            return npos;
        }

        // Now compare the full string
        if (traits_type::compare(search_start, s, n) == 0)
        {
            // Return position of first character
            return search_start - get_pointer(0);
        }

        // Match was not found, update remaining length and try again
        ++search_start;
        remaining_length = last_char - search_start;
    }

    // Search string wasn't found
    return npos;
}

constexpr string::size_type string::find(const value_type* s, size_type pos) const
{
    ::omni::detail::null_check(s, "string::find");
    return find(s, pos, traits_type::length(s));
}

constexpr string::size_type string::find(value_type c, size_type pos) const noexcept
{
    if (pos >= size())
    {
        return npos;
    }

    const value_type* location = traits_type::find(get_pointer(pos), size() - pos, c);
    if (location != nullptr)
    {
        return location - get_pointer(0);
    }
    return npos;
}

CARB_CPP20_CONSTEXPR inline string::size_type string::find(const std::string& str, size_type pos) const noexcept
{
    return find(str.data(), pos, str.size());
}

constexpr string::size_type string::find(const carb::cpp17::string_view& sv, size_type pos) const noexcept
{
    return find(sv.data(), pos, sv.size());
}

constexpr string::size_type string::rfind(const string& str, size_type pos) const noexcept
{
    return rfind(str.data(), pos, str.size());
}

constexpr string::size_type string::rfind(const value_type* s, size_type pos, size_type n) const
{
    ::omni::detail::null_check(s, "string::rfind");
    if (n == 0)
    {
        if (pos > size())
        {
            return size();
        }
        else
        {
            return pos;
        }
    }

    if (n > size())
    {
        return npos;
    }

    size_type search_position = ::carb_min((size() - n), pos);
    do
    {
        if (traits_type::compare(get_pointer(search_position), s, n) == 0)
        {
            return search_position;
        }
    } while (search_position-- > 0);

    return npos;
}

constexpr string::size_type string::rfind(const value_type* s, size_type pos) const
{
    ::omni::detail::null_check(s, "string::rfind");
    return rfind(s, pos, traits_type::length(s));
}

constexpr string::size_type string::rfind(value_type c, size_type pos) const noexcept
{
    if (empty())
    {
        return npos;
    }

    size_type search_position = ::carb_min(pos, size() - 1);
    do
    {
        if (get_reference(search_position) == c)
        {
            return search_position;
        }
    } while (search_position-- > 0);

    return npos;
}

CARB_CPP20_CONSTEXPR inline string::size_type string::rfind(const std::string& str, size_type pos) const noexcept
{
    return rfind(str.data(), pos, str.size());
}

constexpr string::size_type string::rfind(const carb::cpp17::string_view& sv, size_type pos) const noexcept
{
    return rfind(sv.data(), pos, sv.size());
}

#if CARB_HAS_CPP17
template <typename T, typename>
constexpr string::size_type string::rfind(const T& t, size_type pos) const noexcept
{
    std::string_view sv = t;
    return rfind(sv.data(), pos, sv.size());
}
#endif

constexpr string::size_type string::find_first_of(const string& str, size_type pos) const noexcept
{
    return find_first_of(str.data(), pos, str.size());
}

constexpr string::size_type string::find_first_of(const value_type* s, size_type pos, size_type n) const
{
    ::omni::detail::null_check(s, "string::find_first_of");
    if (n == 0)
    {
        return npos;
    }

    for (; pos < size(); ++pos)
    {
        // Search the provided string for the character at pos
        if (traits_type::find(s, n, get_reference(pos)) != nullptr)
        {
            return pos;
        }
    }

    return npos;
}

constexpr string::size_type string::find_first_of(const value_type* s, size_type pos) const
{
    ::omni::detail::null_check(s, "string::find_first_of");
    return find_first_of(s, pos, traits_type::length(s));
}

constexpr string::size_type string::find_first_of(value_type c, size_type pos) const noexcept
{
    return find(c, pos);
}

CARB_CPP20_CONSTEXPR inline string::size_type string::find_first_of(const std::string& str, size_type pos) const noexcept
{
    return find_first_of(str.data(), pos, str.size());
}

constexpr string::size_type string::find_first_of(const carb::cpp17::string_view& sv, size_type pos) const noexcept
{
    return find_first_of(sv.data(), pos, sv.size());
}

#if CARB_HAS_CPP17
template <typename T, typename>
constexpr string::size_type string::find_first_of(const T& t, size_type pos) const noexcept
{
    std::string_view sv = t;
    return find_first_of(sv.data(), pos, sv.size());
}
#endif

constexpr string::size_type string::find_last_of(const string& str, size_type pos) const noexcept
{
    return find_last_of(str.data(), pos, str.size());
}

constexpr string::size_type string::find_last_of(const value_type* s, size_type pos, size_type n) const
{
    ::omni::detail::null_check(s, "string::find_last_of");
    if (empty() || n == 0)
    {
        return npos;
    }

    size_type search_position = ::carb_min(pos, size() - 1);

    do
    {
        // Search the provided string for the character at pos
        if (traits_type::find(s, n, get_reference(search_position)) != nullptr)
        {
            return search_position;
        }
    } while (search_position-- != 0);

    return npos;
}

constexpr string::size_type string::find_last_of(const value_type* s, size_type pos) const
{
    ::omni::detail::null_check(s, "string::find_last_of");
    return find_last_of(s, pos, traits_type::length(s));
}

constexpr string::size_type string::find_last_of(value_type c, size_type pos) const noexcept
{
    return rfind(c, pos);
}

CARB_CPP20_CONSTEXPR inline string::size_type string::find_last_of(const std::string& str, size_type pos) const noexcept
{
    return find_last_of(str.data(), pos, str.size());
}

constexpr string::size_type string::find_last_of(const carb::cpp17::string_view& sv, size_type pos) const noexcept
{
    return find_last_of(sv.data(), pos, sv.size());
}

#if CARB_HAS_CPP17
template <typename T, typename>
constexpr string::size_type string::find_last_of(const T& t, size_type pos) const noexcept
{
    std::string_view sv = t;
    return find_last_of(sv.data(), pos, sv.size());
}
#endif

constexpr string::size_type string::find_first_not_of(const string& str, size_type pos) const noexcept
{
    return find_first_not_of(str.data(), pos, str.size());
}

constexpr string::size_type string::find_first_not_of(const value_type* s, size_type pos, size_type n) const
{
    ::omni::detail::null_check(s, "string::find_first_not_of");
    for (; pos < size(); ++pos)
    {
        // If this char isn't in the search string, return
        if (traits_type::find(s, n, get_reference(pos)) == nullptr)
        {
            return pos;
        }
    }

    return npos;
}

constexpr string::size_type string::find_first_not_of(const value_type* s, size_type pos) const
{
    ::omni::detail::null_check(s, "string::find_first_not_of");
    return find_first_not_of(s, pos, traits_type::length(s));
}

constexpr string::size_type string::find_first_not_of(value_type c, size_type pos) const noexcept
{
    for (; pos < size(); ++pos)
    {
        if (c != get_reference(pos))
        {
            return pos;
        }
    }

    return npos;
}

CARB_CPP20_CONSTEXPR inline string::size_type string::find_first_not_of(const std::string& str, size_type pos) const noexcept
{
    return find_first_not_of(str.data(), pos, str.size());
}

constexpr string::size_type string::find_first_not_of(const carb::cpp17::string_view& sv, size_type pos) const noexcept
{
    return find_first_not_of(sv.data(), pos, sv.size());
}

#if CARB_HAS_CPP17
template <typename T, typename>
constexpr string::size_type string::find_first_not_of(const T& t, size_type pos) const noexcept
{
    std::string_view sv = t;
    return find_first_not_of(sv.data(), pos, sv.size());
}
#endif

constexpr string::size_type string::find_last_not_of(const string& str, size_type pos) const noexcept
{
    return find_last_not_of(str.data(), pos, str.size());
}

constexpr string::size_type string::find_last_not_of(const value_type* s, size_type pos, size_type n) const
{
    ::omni::detail::null_check(s, "string::find_last_not_of");
    if (empty())
    {
        return npos;
    }

    size_type search_position = ::carb_min(pos, size() - 1);

    do
    {
        // If the current character isn't in the search string, return
        if (traits_type::find(s, n, get_reference(search_position)) == nullptr)
        {
            return search_position;
        }
    } while (search_position-- > 0);

    return npos;
}

constexpr string::size_type string::find_last_not_of(const value_type* s, size_type pos) const
{
    ::omni::detail::null_check(s, "string::find_last_not_of");
    return find_last_not_of(s, pos, traits_type::length(s));
}

constexpr string::size_type string::find_last_not_of(value_type c, size_type pos) const noexcept
{
    if (empty())
    {
        return npos;
    }

    size_type search_position = ::carb_min(pos, size() - 1);

    do
    {
        if (c != get_reference(search_position))
        {
            return search_position;
        }
    } while (search_position-- > 0);

    return npos;
}

CARB_CPP20_CONSTEXPR inline string::size_type string::find_last_not_of(const std::string& str, size_type pos) const noexcept
{
    return find_last_not_of(str.data(), pos, str.size());
}

constexpr string::size_type string::find_last_not_of(const carb::cpp17::string_view& sv, size_type pos) const noexcept
{
    return find_last_not_of(sv.data(), pos, sv.size());
}

#if CARB_HAS_CPP17
template <typename T, typename>
constexpr string::size_type string::find_last_not_of(const T& t, size_type pos) const noexcept
{
    std::string_view sv = t;
    return find_last_not_of(sv.data(), pos, sv.size());
}
#endif


/* ------------------------------------------------------------------------------------------------------------------*/
/*                                                  Private Functions                                                */
/* ------------------------------------------------------------------------------------------------------------------*/

constexpr bool string::is_local() const
{
    return m_local_data[kSMALL_SIZE_OFFSET] != kSTRING_IS_ALLOCATED;
}

constexpr void string::set_local(size_type new_size) noexcept
{
    CARB_ASSERT(new_size < kSMALL_STRING_SIZE, "Local size must be less than kSMALL_STRING_SIZE");
    m_local_data[kSMALL_SIZE_OFFSET] = kSMALL_STRING_SIZE - static_cast<char>(new_size) - 1;
}

constexpr void string::set_allocated() noexcept
{
    m_local_data[kSMALL_SIZE_OFFSET] = kSTRING_IS_ALLOCATED;
}

constexpr string::reference string::get_reference(size_type pos) noexcept
{
    if (is_local())
    {
        return m_local_data[pos];
    }
    else
    {
        return m_allocated_data.m_ptr[pos];
    }
}

constexpr string::const_reference string::get_reference(size_type pos) const noexcept
{
    if (is_local())
    {
        return m_local_data[pos];
    }
    else
    {
        return m_allocated_data.m_ptr[pos];
    }
}

constexpr string::pointer string::get_pointer(size_type pos) noexcept
{
    if (is_local())
    {
        return m_local_data + pos;
    }
    else
    {
        return m_allocated_data.m_ptr + pos;
    }
}

constexpr string::const_pointer string::get_pointer(size_type pos) const noexcept
{
    if (is_local())
    {
        return m_local_data + pos;
    }
    else
    {
        return m_allocated_data.m_ptr + pos;
    }
}

constexpr void string::set_empty() noexcept
{
    m_local_data[kSMALL_SIZE_OFFSET] = kSMALL_STRING_SIZE - 1;
    m_local_data[0] = value_type();
}

constexpr void string::range_check(size_type pos, size_type size, const char* function) const
{
#if CARB_EXCEPTIONS_ENABLED
    if (CARB_UNLIKELY(pos > size))
    {
        throw std::out_of_range(std::string(function) + ": Provided pos " + std::to_string(pos) +
                                " greater than string size " + std::to_string(size));
    }
#else
    CARB_FATAL_UNLESS(pos <= size, "%s: Provided pos %zu greater than string size %zu", function, pos, size);
#endif
}

constexpr void string::range_check(const_iterator pos, const char* function) const
{
#if CARB_EXCEPTIONS_ENABLED
    if (CARB_UNLIKELY(pos < cbegin()))
    {
        throw std::out_of_range(std::string(function) + ": Provided iterator comes before the start of the string");
    }
    else if (CARB_UNLIKELY(pos > cend()))
    {
        throw std::out_of_range(std::string(function) + ": Provided iterator comes after the end of the string");
    }
#else
    CARB_FATAL_UNLESS(pos >= cbegin(), "%s: Provided iterator comes before the start of the string", function);
    CARB_FATAL_UNLESS(pos <= cend(), "%s: Provided iterator comes after the end of the string", function);
#endif
}


constexpr void string::range_check(const_iterator first, const_iterator last, const char* function) const
{
    range_check(first, function);
    range_check(last, function);
#if CARB_EXCEPTIONS_ENABLED
    if (CARB_UNLIKELY(first > last))
    {
        throw std::out_of_range(std::string(function) + ": Iterator range is not valid");
    }
#else
    CARB_FATAL_UNLESS(first <= last, "%s: Iterator range is not valid. first is after last", function);
#endif
}

constexpr string::size_type string::length_check(size_type current, size_type n, const char* function) const
{
#if CARB_EXCEPTIONS_ENABLED
    if (CARB_UNLIKELY(n > (max_size() - current)))
    {
        throw std::length_error(std::string(function) + ": Adding " + std::to_string(n) +
                                " additional bytes to current size of " + std::to_string(current) +
                                " would be greater than maximum size " + std::to_string(max_size()));
    }
#else
    CARB_FATAL_UNLESS(n <= (max_size() - current),
                      "%s: Adding %zu additional bytes to current size of %zu would be greater than maximum size %zu",
                      function, n, current, max_size());
#endif

    return current + n;
}

constexpr void string::set_size(size_type new_size) noexcept
{
    if (is_local())
    {
        CARB_IGNOREWARNING_GNUC_WITH_PUSH("-Warray-bounds") // error: array subscript is above array bounds
        CARB_ASSERT(new_size < kSMALL_STRING_SIZE, "Local size must be less than kSMALL_STRING_SIZE");
        m_local_data[new_size] = value_type();
        m_local_data[kSMALL_SIZE_OFFSET] = kSMALL_STRING_SIZE - static_cast<char>(new_size) - 1;
        CARB_IGNOREWARNING_GNUC_POP
    }
    else
    {
        m_allocated_data.m_ptr[new_size] = value_type();
        m_allocated_data.m_size = new_size;
    }
}

constexpr bool string::should_allocate(size_type n) const noexcept
{
    return n >= kSMALL_STRING_SIZE;
}

constexpr bool string::overlaps_this_string(const_pointer s) const noexcept
{
    const_pointer data = get_pointer(0);
    return std::greater_equal<const_pointer>{}(s, data) && std::less_equal<const_pointer>{}(s, data + size());
}

inline void string::overlap_check(const_pointer s) const
{
    if (CARB_LIKELY(!overlaps_this_string(s)))
        return;
#if CARB_EXCEPTIONS_ENABLED
    throw std::runtime_error("string may not overlap");
#else
    OMNI_FATAL_UNLESS(0, "string may not overlap");
#endif
}

inline string::size_type string::vsnprintf_check(char* buffer, size_type buffer_size, const char* format, va_list args)
{
    int fmt_size = std::vsnprintf(buffer, buffer_size, format, args);
    if (fmt_size < 0)
    {
#if CARB_EXCEPTIONS_ENABLED
        throw std::runtime_error("vsnprintf failed");
#else
        OMNI_FATAL_UNLESS(0, "vsnprintf failed");
#endif
    }
    return size_type(fmt_size);
}

template <class... Args>
string::size_type string::snprintf_check(char* buffer, size_type buffer_size, const char* format, Args&&... args)
{
    int fmt_size = std::snprintf(buffer, buffer_size, format, std::forward<Args>(args)...);
    if (fmt_size < 0)
    {
#if CARB_EXCEPTIONS_ENABLED
        throw std::runtime_error("snprintf failed");
#else
        OMNI_FATAL_UNLESS(0, "snprintf failed");
#endif
    }
    return size_type(fmt_size);
}

inline void string::allocate_if_necessary(size_type size)
{
    if (should_allocate(size))
    {
        m_allocated_data.m_ptr = static_cast<char*>(carb::allocate(size + 1));
        m_allocated_data.m_capacity = size;
        set_allocated();
    }
}

inline void string::initialize(const_pointer src, size_type size)
{
    allocate_if_necessary(size);
    std::memcpy(get_pointer(0), src, size);
    set_size(size);
}

template <typename InputIterator>
void string::initialize(InputIterator begin, InputIterator end, size_type size)
{
    allocate_if_necessary(size);
    for (auto ptr = get_pointer(0); begin != end; ++begin, ++ptr)
    {
        traits_type::assign(*ptr, *begin);
    }
    set_size(size);
}

inline void string::dispose()
{
    if (!is_local())
    {
        carb::deallocate(m_allocated_data.m_ptr);
        set_empty();
    }
}

inline string::pointer string::allocate_buffer(size_type old_capacity, size_type& new_capacity)
{
    length_check(0, new_capacity, "string::allocate_buffer");

    // Always grow buffer by at least 2x
    if ((new_capacity > old_capacity) && (new_capacity < old_capacity * 2))
    {
        new_capacity = old_capacity * 2;
        if (new_capacity > max_size())
        {
            new_capacity = max_size();
        }
    }

    return static_cast<pointer>(carb::allocate(new_capacity + 1));
}

inline void string::grow_buffer_to(size_type new_capacity)
{
    if (new_capacity > capacity())
    {
        // Allocate new buffer first. If this throws then *this is still valid
        pointer new_buffer = allocate_buffer(capacity(), new_capacity);
        size_type cur_size = size();

        if (cur_size > 0)
        {
            std::memcpy(new_buffer, get_pointer(0), cur_size);
        }

        // Destroy the old buffer
        dispose();
        set_allocated();
        m_allocated_data.m_ptr = new_buffer;
        m_allocated_data.m_capacity = new_capacity;
        m_allocated_data.m_size = cur_size;
    }
}

inline void string::grow_buffer_and_fill(
    size_type new_size, const_pointer p1, size_type s1, const_pointer p2, size_type s2, const_pointer p3, size_type s3)
{
    size_type new_capacity = new_size;
    pointer new_buffer = allocate_buffer(capacity(), new_capacity);
    size_type copy_dest = 0;
    if (p1)
    {
        std::memcpy(new_buffer, p1, s1);
        copy_dest += s1;
    }
    if (p2)
    {
        std::memcpy(new_buffer + copy_dest, p2, s2);
        copy_dest += s2;
    }
    if (p3)
    {
        std::memcpy(new_buffer + copy_dest, p3, s3);
    }

    // Destroy the old buffer
    dispose();
    set_allocated();
    m_allocated_data.m_ptr = new_buffer;
    m_allocated_data.m_capacity = new_capacity;
    m_allocated_data.m_size = new_size;
}

template <class InputIterator>
void string::grow_buffer_and_append(size_type new_size, InputIterator first, InputIterator last)
{
    size_type new_capacity = new_size;
    pointer new_buffer = allocate_buffer(capacity(), new_capacity);

    std::memcpy(new_buffer, get_pointer(0), size());
    for (auto ptr = new_buffer + size(); first != last; ++first, ++ptr)
    {
        traits_type::assign(*ptr, *first);
    }

    // Destroy the old buffer
    dispose();
    set_allocated();
    m_allocated_data.m_ptr = new_buffer;
    m_allocated_data.m_capacity = new_capacity;
    m_allocated_data.m_size = new_size;
}

inline string& string::assign_internal(const_pointer src, size_type new_size)
{
    new_size = length_check(0, new_size, "string::assign");

    // Attempt to allocate a new buffer first before modifying the string. If allocation throws, this string will be
    // unmodified.
    grow_buffer_to(new_size);

    if (new_size)
    {
        std::memmove(get_pointer(0), src, new_size);
    }
    set_size(new_size);

    return *this;
}

template <typename InputIterator>
string& string::assign_internal(InputIterator begin, InputIterator end, size_type new_size)
{
    new_size = length_check(0, new_size, "string::assign");

    // Attempt to allocate a new buffer first before modifying the string. If allocation throws, this string will be
    // unmodified.
    grow_buffer_to(new_size);

    if (new_size)
    {
        for (auto ptr = get_pointer(0); begin != end; ++begin, ++ptr)
        {
            traits_type::assign(*ptr, *begin);
        }
    }

    set_size(new_size);

    return *this;
}

inline string& string::insert_internal(size_type pos, value_type c, size_type n)
{
    range_check(pos, size(), "string::insert");

    size_type new_size = length_check(size(), n, "string::insert");

    grow_buffer_to(new_size);

    size_type to_copy = size() - pos;
    pointer copy_start = get_pointer(pos);
    pointer copy_dest = copy_start + n;
    std::memmove(copy_dest, copy_start, to_copy);
    traits_type::assign(copy_start, n, c);
    set_size(new_size);
    return *this;
}

inline string& string::insert_internal(size_type pos, const_pointer src, size_type n)
{
    range_check(pos, size(), "string::insert");
    length_check(size(), n, "string::insert");

    // Reuse the logic in replace to handle potentially overlapping regions
    return replace(pos, 0, src, n);
}

inline string& string::append_internal(const_pointer src, size_type n)
{
    size_type new_size = length_check(size(), n, "string::append");

    if (new_size <= capacity())
    {
        std::memcpy(get_pointer(size()), src, n);
        set_size(new_size);
    }
    else
    {
        grow_buffer_and_fill(new_size, get_pointer(0), size(), src, n, nullptr, 0);
    }

    return *this;
}

constexpr int string::compare_internal(const_pointer this_str,
                                       const_pointer other_str,
                                       size_type this_size,
                                       size_type other_size) const noexcept
{
    size_type compare_size = ::carb_min(this_size, other_size);
    int result = traits_type::compare(this_str, other_str, compare_size);

    // If the characters are equal, then compare the sizes
    if (result == 0)
    {
        if (this_size < other_size)
        {
            return -1;
        }
        else if (this_size > other_size)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    return result;
}

inline void string::replace_setup(size_type pos, size_type replaced_size, size_type replacement_size)
{
    range_check(pos, size(), "string::replace");

    replaced_size = ::carb_min(replaced_size, (size() - pos));
    size_type new_size = size() - replaced_size + replacement_size;

    grow_buffer_to(new_size);

    size_type remaining_chars = size() - (pos + replaced_size);
    if (remaining_chars)
    {
        std::memmove(get_pointer(pos + replacement_size), get_pointer(pos + replaced_size), remaining_chars);
    }

    set_size(new_size);
}

/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                Non-member functions                                                */
/* ------------------------------------------------------------------------------------------------------------------ */

/* ------------------------------------------------------------------------------------------------------------------ */
/*                                                      operator+                                                     */
/* ------------------------------------------------------------------------------------------------------------------ */

inline string operator+(const string& lhs, const string& rhs)
{
    string result;
    result.reserve(lhs.length() + rhs.length());
    result.assign(lhs);
    result.append(rhs);
    return result;
}

inline string operator+(const string& lhs, const char* rhs)
{
    ::omni::detail::null_check(rhs, "operator+");
    string::size_type rhs_len = string::traits_type::length(rhs);

    string result;
    result.reserve(lhs.length() + rhs_len);
    result.assign(lhs);
    result.append(rhs, rhs_len);
    return result;
}

inline string operator+(const string& lhs, char rhs)
{
    string result;
    result.reserve(lhs.length() + 1);
    result.assign(lhs);
    result.append(1, rhs);
    return result;
}

inline string operator+(const string& lhs, const std::string& rhs)
{
    string result;
    result.reserve(lhs.length() + rhs.length());
    result.assign(lhs);
    result.append(rhs);
    return result;
}

inline string operator+(const char* lhs, const string& rhs)
{
    ::omni::detail::null_check(lhs, "operator+");
    string::size_type lhs_len = string::traits_type::length(lhs);

    string result;
    result.reserve(lhs_len + rhs.size());
    result.assign(lhs, lhs_len);
    result.append(rhs);
    return result;
}

inline string operator+(char lhs, const string& rhs)
{
    string result;
    result.reserve(rhs.size() + 1);
    result.assign(1, lhs);
    result.append(rhs);
    return result;
}

inline string operator+(const std::string& lhs, const string& rhs)
{
    string result;
    result.reserve(lhs.length() + rhs.length());
    result.assign(lhs);
    result.append(rhs);
    return result;
}

inline string operator+(string&& lhs, string&& rhs)
{
    auto total_size = lhs.size() + rhs.size();
    if (total_size > lhs.capacity() && total_size <= rhs.capacity())
    {
        return std::move(rhs.insert(0, lhs));
    }
    else
    {
        return std::move(lhs.append(rhs));
    }
}

inline string operator+(string&& lhs, const string& rhs)
{
    return std::move(lhs.append(rhs));
}

inline string operator+(string&& lhs, const char* rhs)
{
    ::omni::detail::null_check(rhs, "operator+");
    return std::move(lhs.append(rhs));
}

inline string operator+(string&& lhs, char rhs)
{
    return std::move(lhs.append(1, rhs));
}

inline string operator+(string&& lhs, const std::string& rhs)
{
    return std::move(lhs.append(rhs));
}

inline string operator+(const string& lhs, string&& rhs)
{
    return std::move(rhs.insert(0, lhs));
}

inline string operator+(const char* lhs, string&& rhs)
{
    ::omni::detail::null_check(lhs, "operator+");
    return std::move(rhs.insert(0, lhs));
}

inline string operator+(char lhs, string&& rhs)
{
    return std::move(rhs.insert(static_cast<string::size_type>(0), static_cast<string::size_type>(1), lhs));
}

inline string operator+(const std::string& lhs, string&& rhs)
{
    return std::move(rhs.insert(0, lhs));
}

/* ------------------------------------------------------------------------------------------------------------------ */
/*                                               operator==,!=,<,<=,>,>=                                              */
/* ------------------------------------------------------------------------------------------------------------------ */

constexpr bool operator==(const string& lhs, const string& rhs) noexcept
{
    return lhs.compare(rhs) == 0;
}

constexpr bool operator!=(const string& lhs, const string& rhs) noexcept
{
    return !(lhs == rhs);
}

constexpr bool operator<(const string& lhs, const string& rhs) noexcept
{
    return lhs.compare(rhs) < 0;
}

constexpr bool operator<=(const string& lhs, const string& rhs) noexcept
{
    return lhs.compare(rhs) <= 0;
}

constexpr bool operator>(const string& lhs, const string& rhs) noexcept
{
    return lhs.compare(rhs) > 0;
}

constexpr bool operator>=(const string& lhs, const string& rhs) noexcept
{
    return lhs.compare(rhs) >= 0;
}

constexpr bool operator==(const string& lhs, const char* rhs)
{
    ::omni::detail::null_check(rhs, "operator==");
    return lhs.compare(rhs) == 0;
}

constexpr bool operator==(const char* lhs, const string& rhs)
{
    ::omni::detail::null_check(lhs, "operator==");
    return rhs.compare(lhs) == 0;
}

constexpr bool operator!=(const string& lhs, const char* rhs)
{
    ::omni::detail::null_check(rhs, "operator!=");
    return !(lhs == rhs);
}

constexpr bool operator!=(const char* lhs, const string& rhs)
{
    ::omni::detail::null_check(lhs, "operator!=");
    return !(lhs == rhs);
}

constexpr bool operator<(const string& lhs, const char* rhs)
{
    ::omni::detail::null_check(rhs, "operator<=");
    return lhs.compare(rhs) < 0;
}

constexpr bool operator<(const char* lhs, const string& rhs)
{
    ::omni::detail::null_check(lhs, "operator<=");
    return rhs.compare(lhs) > 0;
}

constexpr bool operator<=(const string& lhs, const char* rhs)
{
    ::omni::detail::null_check(rhs, "operator<=");
    return lhs.compare(rhs) <= 0;
}

constexpr bool operator<=(const char* lhs, const string& rhs)
{
    ::omni::detail::null_check(lhs, "operator<=");
    return rhs.compare(lhs) >= 0;
}

constexpr bool operator>(const string& lhs, const char* rhs)
{
    ::omni::detail::null_check(rhs, "operator>");
    return lhs.compare(rhs) > 0;
}

constexpr bool operator>(const char* lhs, const string& rhs)
{
    ::omni::detail::null_check(lhs, "operator>");
    return rhs.compare(lhs) < 0;
}

constexpr bool operator>=(const string& lhs, const char* rhs)
{
    ::omni::detail::null_check(rhs, "operator>=");
    return lhs.compare(rhs) >= 0;
}

constexpr bool operator>=(const char* lhs, const string& rhs)
{
    ::omni::detail::null_check(lhs, "operator>=");
    return rhs.compare(lhs) <= 0;
}

CARB_CPP20_CONSTEXPR inline bool operator==(const string& lhs, const std::string& rhs) noexcept
{
    return lhs.compare(rhs) == 0;
}

CARB_CPP20_CONSTEXPR inline bool operator==(const std::string& lhs, const string& rhs) noexcept
{
    return rhs.compare(lhs) == 0;
}

CARB_CPP20_CONSTEXPR inline bool operator!=(const string& lhs, const std::string& rhs) noexcept
{
    return !(lhs == rhs);
}

CARB_CPP20_CONSTEXPR inline bool operator!=(const std::string& lhs, const string& rhs) noexcept
{
    return !(lhs == rhs);
}

CARB_CPP20_CONSTEXPR inline bool operator<(const string& lhs, const std::string& rhs) noexcept
{
    return lhs.compare(rhs) < 0;
}

CARB_CPP20_CONSTEXPR inline bool operator<(const std::string& lhs, const string& rhs) noexcept
{
    return rhs.compare(lhs) > 0;
}

CARB_CPP20_CONSTEXPR inline bool operator<=(const string& lhs, const std::string& rhs) noexcept
{
    return lhs.compare(rhs) <= 0;
}

CARB_CPP20_CONSTEXPR inline bool operator<=(const std::string& lhs, const string& rhs) noexcept
{
    return rhs.compare(lhs) >= 0;
}

CARB_CPP20_CONSTEXPR inline bool operator>(const string& lhs, const std::string& rhs) noexcept
{
    return lhs.compare(rhs) > 0;
}

CARB_CPP20_CONSTEXPR inline bool operator>(const std::string& lhs, const string& rhs) noexcept
{
    return rhs.compare(lhs) < 0;
}

CARB_CPP20_CONSTEXPR inline bool operator>=(const string& lhs, const std::string& rhs) noexcept
{
    return lhs.compare(rhs) >= 0;
}

CARB_CPP20_CONSTEXPR inline bool operator>=(const std::string& lhs, const string& rhs) noexcept
{
    return rhs.compare(lhs) <= 0;
}

inline void swap(string& lhs, string& rhs) noexcept
{
    lhs.swap(rhs);
}

template <typename U>
CARB_CPP20_CONSTEXPR string::size_type erase(string& str, const U& val)
{
    auto it = std::remove(str.begin(), str.end(), val);
    auto removed = std::distance(it, str.end());
    str.erase(it, str.end());
    return removed;
}

template <class Pred>
CARB_CPP20_CONSTEXPR string::size_type erase_if(string& str, Pred pred)
{
    auto it = std::remove_if(str.begin(), str.end(), pred);
    auto removed = std::distance(it, str.end());
    str.erase(it, str.end());
    return removed;
}

inline std::basic_ostream<char, std::char_traits<char>>& operator<<(std::basic_ostream<char, std::char_traits<char>>& os,
                                                                    const string& str)
{
    using _ostream = std::basic_ostream<char, std::char_traits<char>>;
    using traits_type = std::char_traits<char>;
    using size_type = string::size_type;
    std::basic_ostream<char, std::char_traits<char>>::iostate err =
        std::basic_ostream<char, std::char_traits<char>>::goodbit;

    // Get and check the sentry first
    _ostream::sentry sentry(os);

    if (sentry)
    {
        size_type pad;
        if (os.width() <= 0 || static_cast<size_type>(os.width()) < str.size())
        {
            pad = 0;
        }
        else
        {
            pad = static_cast<size_type>(os.width()) - str.size();
        }

        using int_type = traits_type::int_type;
        int_type eof = traits_type::eof();

        // Pad on the left
        if ((os.flags() & _ostream::adjustfield) != _ostream::left)
        {
            for (; pad > 0; --pad)
            {
                int_type result = os.rdbuf()->sputc(os.fill());
                if (traits_type::eq_int_type(result, eof))
                {
                    err |= _ostream::badbit;
                    break;
                }
            }
        }

        if (err == _ostream::goodbit)
        {
            std::streamsize result = os.rdbuf()->sputn(str.data(), static_cast<std::streamsize>(str.size()));
            if (result != static_cast<std::streamsize>(str.size()))
            {
                err |= _ostream::badbit;
            }
            else
            {
                // Pad on the right;
                for (; pad > 0; --pad)
                {
                    int_type res = os.rdbuf()->sputc(os.fill());
                    if (traits_type::eq_int_type(res, eof))
                    {
                        err |= _ostream::badbit;
                        break;
                    }
                }
            }
        }
    }

    os.width(0);

    if (err)
    {
        os.setstate(err);
    }

    return os;
}

inline std::basic_istream<char, std::char_traits<char>>& operator>>(std::basic_istream<char, std::char_traits<char>>& is,
                                                                    string& str)
{
    string::size_type char_count = 0;

    using _istream = std::basic_istream<char, std::char_traits<char>>;
    using traits_type = std::char_traits<char>;
    _istream::iostate err = _istream::goodbit;

    // Get and check the sentry first
    _istream::sentry sentry(is, false);
    if (sentry)
    {
        str.erase();

        using size_type = string::size_type;
        size_type n = str.max_size();
        if (is.width() > 0)
        {
            n = static_cast<size_type>(is.width());
        }

        using int_type = traits_type::int_type;
        int_type eof = traits_type::eof();
        int_type c = is.rdbuf()->sgetc();

        while (!traits_type::eq_int_type(c, eof) && !std::isspace(traits_type::to_char_type(c), is.getloc()) &&
               char_count < n)
        {
            str.append(1UL, traits_type::to_char_type(c));
            ++char_count;
            c = is.rdbuf()->snextc();
        }

        if (traits_type::eq_int_type(c, eof))
        {
            err |= _istream::eofbit;
        }

        is.width(0);
    }

    // Extracting no characters is always a failure
    if (char_count == 0)
    {
        err |= _istream::failbit;
    }

    if (err)
    {
        is.setstate(err);
    }

    return is;
}

inline std::basic_istream<char, std::char_traits<char>>& getline(std::basic_istream<char, std::char_traits<char>>&& input,
                                                                 string& str,
                                                                 char delim)
{
    string::size_type char_count = 0;

    using _istream = std::basic_istream<char, std::char_traits<char>>;
    using traits_type = std::char_traits<char>;
    _istream::iostate err = _istream::goodbit;

    // Get and check the sentry first
    _istream::sentry sentry(input, true);
    if (sentry)
    {
        str.erase();

        using int_type = traits_type::int_type;
        int_type eof = traits_type::eof();
        int_type delim_int = traits_type::to_int_type(delim);
        int_type c = input.rdbuf()->sgetc();

        auto max_size = str.max_size();

        // Loop over the input stream until we get eof, the delimiter, or reach the max size of the string
        while (!traits_type::eq_int_type(c, eof) && !traits_type::eq_int_type(c, delim_int) && char_count < max_size)
        {
            str.append(1, traits_type::to_char_type(c));
            ++char_count;
            c = input.rdbuf()->snextc();
        }

        // Update any necessary failure bits
        if (traits_type::eq_int_type(c, eof))
        {
            err |= _istream::eofbit;
        }
        else if (traits_type::eq_int_type(c, delim_int))
        {
            ++char_count;
            // Pop off the delimiter
            input.rdbuf()->sbumpc();
        }
        else
        {
            err |= _istream::failbit;
        }
    }

    // Extracting no characters is always a failure
    if (char_count == 0)
    {
        err |= _istream::failbit;
    }

    if (err)
    {
        input.setstate(err);
    }

    return input;
}

inline std::basic_istream<char, std::char_traits<char>>& getline(std::basic_istream<char, std::char_traits<char>>&& input,
                                                                 string& str)
{
    return getline(std::move(input), str, input.widen('\n'));
}


inline int stoi(const string& str, std::size_t* pos, int base)
{
    return ::omni::detail::_sto_helper<long, int>(&std::strtol, "stoi", str.c_str(), pos, base);
}

inline long stol(const string& str, std::size_t* pos, int base)
{
    return ::omni::detail::_sto_helper(&std::strtol, "stol", str.c_str(), pos, base);
}

inline long long stoll(const string& str, std::size_t* pos, int base)
{
    return ::omni::detail::_sto_helper(&std::strtoll, "stoll", str.c_str(), pos, base);
}

inline unsigned long stoul(const string& str, std::size_t* pos, int base)
{
    return ::omni::detail::_sto_helper(&std::strtoul, "stoul", str.c_str(), pos, base);
}

inline unsigned long long stoull(const string& str, std::size_t* pos, int base)
{
    return ::omni::detail::_sto_helper(&std::strtoull, "stoull", str.c_str(), pos, base);
}

inline float stof(const string& str, std::size_t* pos)
{
    return ::omni::detail::_sto_helper(&std::strtof, "stof", str.c_str(), pos);
}

inline double stod(const string& str, std::size_t* pos)
{
    return ::omni::detail::_sto_helper(&std::strtod, "stod", str.c_str(), pos);
}

inline long double stold(const string& str, std::size_t* pos)
{
    return ::omni::detail::_sto_helper(&std::strtold, "stold", str.c_str(), pos);
}

inline string to_string(int value)
{
    return string(formatted, "%d", value);
}
inline string to_string(long value)
{
    return string(formatted, "%ld", value);
}
inline string to_string(long long value)
{
    return string(formatted, "%lld", value);
}
inline string to_string(unsigned value)
{
    return string(formatted, "%u", value);
}
inline string to_string(unsigned long value)
{
    return string(formatted, "%lu", value);
}
inline string to_string(unsigned long long value)
{
    return string(formatted, "%llu", value);
}
inline string to_string(float value)
{
    return string(formatted, "%f", value);
}
inline string to_string(double value)
{
    return string(formatted, "%f", value);
}
inline string to_string(long double value)
{
    return string(formatted, "%Lf", value);
}

} // namespace omni
