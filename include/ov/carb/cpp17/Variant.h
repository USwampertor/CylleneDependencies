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

#include "TypeTraits.h"
#include "Utility.h"

#include <cstdint>
#include <exception>
#include <new>
#include <tuple>
#include <utility>

// This class is a not quite standards conformant implementation of std::variant. Where it doesn't comply it is
// in the sense that it doesn't support everything. Such as all constexpr usages. Part of this is because it
// isn't possible on a C++14 compiler, the other part is full coverage of this class is difficult. Feel free
// to expand this class and make it more conforming to all use cases the standard version can given it will
// still compile on C++14. The long term intention is we will move to a C++17 compiler, and import the std
// version of this class, removing this code from our codebase. Therefore it is very important that this class
// doesn't do anything that the std can't, though the opposite is permissible.

namespace carb
{

namespace cpp17
{

static constexpr std::size_t variant_npos = (std::size_t)-1;

// Forward define.
template <typename... Types>
class variant;

template <std::size_t I, typename T>
struct variant_alternative;

#if CARB_EXCEPTIONS_ENABLED
class bad_variant_access final : public std::exception
{
public:
    bad_variant_access() noexcept = default;
    bad_variant_access(const bad_variant_access&) noexcept = default;
    bad_variant_access& operator=(const bad_variant_access&) noexcept = default;
    virtual const char* what() const noexcept override
    {
        return "bad variant access";
    }
};
#endif

namespace detail
{

// Common pathway for bad_variant_access.
[[noreturn]] inline void on_bad_variant_access()
{
#if CARB_EXCEPTIONS_ENABLED
    throw bad_variant_access();
#else
    std::terminate();
#endif
}

template <bool IsTriviallyDesctructable, typename... Types>
class VariantHold;

template <bool IsTriviallyDesctructable>
class VariantHold<IsTriviallyDesctructable>
{
    static constexpr size_t size = 0;
};

template <typename T, typename... Types>
class VariantHold<true, T, Types...>
{
public:
    static constexpr size_t size = 1 + sizeof...(Types);
    using Next = VariantHold<true, Types...>;

    union
    {
        CARB_VIZ std::remove_const_t<T> m_value;
        CARB_VIZ Next m_next;
    };
    constexpr VariantHold() noexcept
    {
    }

    template <class... Args>
    constexpr VariantHold(in_place_index_t<0>, Args&&... args) noexcept : m_value(std::forward<Args>(args)...)
    {
    }

    template <size_t I, class... Args>
    constexpr VariantHold(in_place_index_t<I>, Args&&... args) noexcept
        : m_next(in_place_index<I - 1>, std::forward<Args>(args)...)
    {
    }

    constexpr T& get() & noexcept
    {
        return m_value;
    }
    constexpr const T& get() const& noexcept
    {
        return m_value;
    }
    constexpr T&& get() && noexcept
    {
        return std::move(m_value);
    }
    constexpr const T&& get() const&& noexcept
    {
        return std::move(m_value);
    }
};


template <typename T, typename... Types>
class VariantHold<false, T, Types...>
{
public:
    static constexpr size_t size = 1 + sizeof...(Types);
    using Next = VariantHold<false, Types...>;

    union
    {
        CARB_VIZ std::remove_const_t<T> m_value;
        CARB_VIZ Next m_next;
    };

    constexpr VariantHold() noexcept
    {
    }
    template <class... Args>
    constexpr VariantHold(in_place_index_t<0>, Args&&... args) noexcept : m_value(std::forward<Args>(args)...)
    {
    }

    template <size_t I, class... Args>
    constexpr VariantHold(in_place_index_t<I>, Args&&... args) noexcept
        : m_next(in_place_index<I - 1>, std::forward<Args>(args)...)
    {
    }
    ~VariantHold()
    {
    }

    constexpr T& get() & noexcept
    {
        return m_value;
    }
    constexpr const T& get() const& noexcept
    {
        return m_value;
    }
    constexpr T&& get() && noexcept
    {
        return std::move(m_value);
    }
    constexpr const T&& get() const&& noexcept
    {
        return std::move(m_value);
    }
};

template <size_t I>
struct VariantGetFromHold
{
    template <class Hold>
    static decltype(auto) get(Hold&& hold)
    {
        return VariantGetFromHold<I - 1>::get(hold.m_next);
    }
};

template <>
struct VariantGetFromHold<0>
{
    template <class Hold>
    static decltype(auto) get(Hold&& hold)
    {
        return hold.get();
    }
};

template <size_t I, typename Hold>
constexpr decltype(auto) variant_get_from_hold(Hold&& hold)
{
    constexpr size_t size = std::remove_reference_t<Hold>::size;
    return VariantGetFromHold < I < size ? I : size - 1 > ::get(hold);
}

// Visitor with index feedback.
template <size_t I, typename Functor, typename Hold>
static decltype(auto) visitWithIndex(Functor&& functor, Hold&& hold)
{
    return std::forward<Functor>(functor)(I, VariantGetFromHold<I>::get(static_cast<Hold&&>(hold)));
}

template <int I>
struct VisitWithIndexHelper;

template <typename Functor, typename Hold, typename Ids = std::make_index_sequence<std::remove_reference<Hold>::type::size>>
struct VisitWithIndexTable;

template <typename Functor, typename Hold, size_t... Ids>
struct VisitWithIndexTable<Functor, Hold, std::index_sequence<Ids...>>
{
    using return_type = decltype(std::declval<Functor>()(VariantGetFromHold<0>::get(std::declval<Hold>())));
    using f_table_type = return_type (*)(Functor&&, Hold&&);
    static f_table_type& table(size_t id)
    {
        static f_table_type tbl[] = { &visitWithIndex<Ids, Functor, Hold>... };
        return tbl[id];
    }
};

template <>
struct VisitWithIndexHelper<-1>
{
    template <typename Functor, typename Hold>
    static constexpr decltype(auto) issue(Functor&& functor, size_t index, Hold&& hold)
    {
        auto& entry = VisitWithIndexTable<Functor, Hold>::table(index);
        return entry(std::forward<Functor>(functor), std::forward<Hold>(hold));
    }
};

#define VISIT_WITH_INDEX_1(n)                                                                                          \
    case (n):                                                                                                          \
        return std::forward<Functor>(functor)(                                                                         \
            n, VariantGetFromHold < n < size ? n : size - 1 > ::get(static_cast<Hold&&>(hold)));

#define VISIT_WITH_INDEX_2(n)                                                                                          \
    VISIT_WITH_INDEX_1(n)                                                                                              \
    VISIT_WITH_INDEX_1(n + 1)
#define VISIT_WITH_INDEX_4(n)                                                                                          \
    VISIT_WITH_INDEX_2(n)                                                                                              \
    VISIT_WITH_INDEX_2(n + 2)
#define VISIT_WITH_INDEX_8(n)                                                                                          \
    VISIT_WITH_INDEX_4(n)                                                                                              \
    VISIT_WITH_INDEX_4(n + 4)
#define VISIT_WITH_INDEX_16(n)                                                                                         \
    VISIT_WITH_INDEX_8(n)                                                                                              \
    VISIT_WITH_INDEX_8(n + 8)

template <>
struct VisitWithIndexHelper<0>
{
    template <typename Functor, typename Hold>
    static constexpr decltype(auto) issue(Functor&& functor, size_t index, Hold&& hold)
    {
        constexpr size_t size = std::remove_reference_t<Hold>::size;
        switch (index)
        {
            default:
                VISIT_WITH_INDEX_1(0);
        }
    }
};
template <>
struct VisitWithIndexHelper<1>
{
    template <typename Functor, typename Hold>
    static constexpr decltype(auto) issue(Functor&& functor, size_t index, Hold&& hold)
    {
        constexpr size_t size = std::remove_reference_t<Hold>::size;
        switch (index)
        {
            default:
                VISIT_WITH_INDEX_2(0);
        }
    }
};

template <>
struct VisitWithIndexHelper<2>
{
    template <typename Functor, typename Hold>
    static constexpr decltype(auto) issue(Functor&& functor, size_t index, Hold&& hold)
    {
        constexpr size_t size = std::remove_reference_t<Hold>::size;
        switch (index)
        {
            default:
                VISIT_WITH_INDEX_4(0);
        }
    }
};
template <>
struct VisitWithIndexHelper<3>
{
    template <typename Functor, typename Hold>
    static constexpr decltype(auto) issue(Functor&& functor, size_t index, Hold&& hold)
    {
        constexpr size_t size = std::remove_reference_t<Hold>::size;
        switch (index)
        {
            default:
                VISIT_WITH_INDEX_8(0);
        }
    }
};
template <>
struct VisitWithIndexHelper<4>
{
    template <typename Functor, typename Hold>
    static constexpr decltype(auto) issue(Functor&& functor, size_t index, Hold&& hold)
    {
        constexpr size_t size = std::remove_reference_t<Hold>::size;
        switch (index)
        {
            default:
                VISIT_WITH_INDEX_16(0);
        }
    }
};

#undef VISIT_WITH_INDEX_1
#undef VISIT_WITH_INDEX_2
#undef VISIT_WITH_INDEX_4
#undef VISIT_WITH_INDEX_8
#undef VISIT_WITH_INDEX_16

// Use this as the definition so that the template parameters can auto-deduce.
template <typename Functor, typename Hold>
decltype(auto) visitorWithIndex(Functor&& functor, size_t typeIndex, Hold&& hold)
{
    constexpr int size = std::remove_reference<Hold>::type::size;
    constexpr int version = size <= 1 ? 0 : size <= 2 ? 1 : size <= 4 ? 2 : size <= 8 ? 3 : size <= 16 ? 4 : -1;
    return VisitWithIndexHelper<version>::issue(std::forward<Functor>(functor), typeIndex, std::forward<Hold>(hold));
}

// Visitor without index feedback.

template <size_t I>
struct Dispatcher;

template <>
struct Dispatcher<1>
{
    template <typename Functor, typename Impl>
    using return_type = decltype(std::declval<Functor>()(VariantGetFromHold<0>::get(std::declval<Impl>())));

    template <size_t I, typename Functor, typename Impl>
    static decltype(auto) issue(Functor&& functor, Impl&& impl)
    {
        return std::forward<Functor>(functor)(variant_get_from_hold<I>(std::forward<Impl>(impl)));
    }
};

template <>
struct Dispatcher<2>
{
    template <typename Functor, typename Impl1, typename Impl2>
    using return_type = decltype(std::declval<Functor>()(
        VariantGetFromHold<0>::get(std::declval<Impl1>()), VariantGetFromHold<0>::get(std::declval<Impl2>())));

    template <size_t I, typename Functor, typename Impl1, typename Impl2>
    static decltype(auto) issue(Functor&& functor, Impl1&& impl1, Impl2&& impl2)
    {
        constexpr size_t size1 = std::remove_reference<Impl1>::type::size;
        constexpr size_t I1 = I % size1;
        constexpr size_t I2 = I / size1;
        return functor(variant_get_from_hold<I1>(std::forward<Impl1>(impl1)),
                       variant_get_from_hold<I2>(std::forward<Impl2>(impl2)));
    }
};

template <typename... Impl>
struct TotalStates;

template <typename Impl>
struct TotalStates<Impl>
{
    static constexpr size_t value = std::remove_reference<Impl>::type::size;
};

template <typename Impl, typename... Rem>
struct TotalStates<Impl, Rem...>
{
    static constexpr size_t value = std::remove_reference<Impl>::type::size * TotalStates<Rem...>::value;
};

template <typename... Ts>
struct Package
{
};

template <typename Functor, typename... Impl>
struct VisitHelperTable
{
    template <typename Ids>
    struct Instance;
};

template <typename Functor, typename... Impl>
template <size_t... Ids>
struct VisitHelperTable<Functor, Impl...>::Instance<std::index_sequence<Ids...>>
{
    using dispatcher = Dispatcher<sizeof...(Impl)>;
    using return_type = typename dispatcher::template return_type<Functor, Impl...>;
    using f_table_type = return_type (*)(Functor&&, Impl&&...);
    static f_table_type& table(size_t id)
    {
        static f_table_type tbl[] = { &Dispatcher<sizeof...(Impl)>::template issue<Ids, Functor>... };
        return tbl[id];
    }
};

inline size_t computeIndex()
{
    return 0;
}

template <class Impl, class... Impls>
constexpr size_t computeIndex(Impl&& impl, Impls&&... impls)
{
    if (impl.index() != variant_npos)
    {
        constexpr size_t size = std::remove_reference<Impl>::type::size;
        return impl.index() + size * computeIndex(impls...);
    }
    on_bad_variant_access();
}

template <int I>
struct VisitHelper;

template <>
struct VisitHelper<-1>
{
    template <typename Functor, typename... Impl>
    static constexpr decltype(auto) issue(Functor&& functor, Impl&&... impl)
    {
        constexpr size_t size = TotalStates<Impl...>::value;
        size_t index = computeIndex(impl...);
        auto& entry = VisitHelperTable<Functor, Impl...>::template Instance<std::make_index_sequence<size>>::table(index);
        return entry(std::forward<Functor>(functor), std::forward<Impl>(impl)...);
    }
};

#define VISIT_1(n)                                                                                                     \
    case (n):                                                                                                          \
        return dispatcher::template issue<n, Functor>(std::forward<Functor>(functor), std::forward<Impl>(impl)...);

#define VISIT_2(n)                                                                                                     \
    VISIT_1(n)                                                                                                         \
    VISIT_1(n + 1)
#define VISIT_4(n)                                                                                                     \
    VISIT_2(n)                                                                                                         \
    VISIT_2(n + 2)
#define VISIT_8(n)                                                                                                     \
    VISIT_4(n)                                                                                                         \
    VISIT_4(n + 4)
#define VISIT_16(n)                                                                                                    \
    VISIT_8(n)                                                                                                         \
    VISIT_8(n + 8)

template <>
struct VisitHelper<0>
{
    template <typename Functor, typename... Impl>
    static constexpr decltype(auto) issue(Functor&& functor, Impl&&... impl)
    {
        size_t index = computeIndex(impl...);
        using dispatcher = Dispatcher<sizeof...(Impl)>;
        switch (index)
        {
            default:
                VISIT_1(0);
        }
    }
};
template <>
struct VisitHelper<1>
{
    template <typename Functor, typename... Impl>
    static constexpr decltype(auto) issue(Functor&& functor, Impl&&... impl)
    {
        size_t index = computeIndex(impl...);
        using dispatcher = Dispatcher<sizeof...(Impl)>;
        switch (index)
        {
            default:
                VISIT_2(0);
        }
    }
};

template <>
struct VisitHelper<2>
{
    template <typename Functor, typename... Impl>
    static constexpr decltype(auto) issue(Functor&& functor, Impl&&... impl)
    {
        size_t index = computeIndex(impl...);
        using dispatcher = Dispatcher<sizeof...(Impl)>;
        switch (index)
        {
            default:
                VISIT_4(0);
        }
    }
};
template <>
struct VisitHelper<3>
{
    template <typename Functor, typename... Impl>
    static constexpr decltype(auto) issue(Functor&& functor, Impl&&... impl)
    {
        size_t index = computeIndex(impl...);
        using dispatcher = Dispatcher<sizeof...(Impl)>;
        switch (index)
        {
            default:
                VISIT_8(0);
        }
    }
};
template <>
struct VisitHelper<4>
{
    template <typename Functor, typename... Impl>
    static constexpr decltype(auto) issue(Functor&& functor, Impl&&... impl)
    {
        size_t index = computeIndex(impl...);
        using dispatcher = Dispatcher<sizeof...(Impl)>;
        switch (index)
        {
            default:
                VISIT_16(0);
        }
    }
};

#undef VISIT_1
#undef VISIT_2
#undef VISIT_4
#undef VISIT_8
#undef VISIT_16

// Use this as the definition so that the template parameters can auto-deduce.
template <typename Functor, typename... Impl>
decltype(auto) visitor(Functor&& functor, Impl&&... impl)
{
    constexpr size_t size = TotalStates<Impl...>::value;
    constexpr int version = size <= 1 ? 0 : size <= 2 ? 1 : size <= 4 ? 2 : size <= 8 ? 3 : size <= 16 ? 4 : -1;
    return VisitHelper<version>::issue(std::forward<Functor>(functor), std::forward<Impl>(impl)...);
}

// Internal helper that generates smaller code when scanning two variants.

template <size_t I, typename Functor, typename Hold1, typename Hold2>
decltype(auto) visitSameOnce(Functor&& functor, Hold1&& hold1, Hold2&& hold2)
{
    return std::forward<Functor>(functor)(
        variant_get_from_hold<I>(std::forward<Hold1>(hold1)), variant_get_from_hold<I>(std::forward<Hold2>(hold2)));
}

template <typename Functor,
          typename Hold1,
          typename Hold2,
          typename Ids = std::make_index_sequence<std::remove_reference<Hold1>::type::size>>
struct VisitSameHelperTable;

template <typename Functor, typename Hold1, typename Hold2, size_t... Ids>
struct VisitSameHelperTable<Functor, Hold1, Hold2, std::index_sequence<Ids...>>
{
    using return_type = decltype(std::declval<Functor>()(
        VariantGetFromHold<0>::get(std::declval<Hold1>()), VariantGetFromHold<0>::get(std::declval<Hold2>())));
    using f_table_type = return_type (*)(Functor&&, Hold1&&, Hold2&&);
    static f_table_type& table(size_t id)
    {
        static f_table_type tbl[] = { &visitSameOnce<Ids, Functor, Hold1, Hold2>... };
        return tbl[id];
    }
};

template <int I>
struct VisitSameHelper;

template <>
struct VisitSameHelper<-1>
{
    template <typename Functor, typename Hold1, typename Hold2>
    static constexpr decltype(auto) issue(Functor&& functor, size_t index, Hold1&& hold1, Hold2&& hold2)
    {
        constexpr auto& entry = VisitSameHelperTable<Functor, Hold1, Hold2>::table(index);
        return entry(std::forward<Functor>(functor), std::forward<Hold1>(hold1), std::forward<Hold2>(hold2));
    }
};

#define VISIT_SAME_1(n)                                                                                                \
    case (n):                                                                                                          \
        return functor(variant_get_from_hold<n>(std::forward<Hold1>(hold1)),                                           \
                       variant_get_from_hold<n>(std::forward<Hold2>(hold2)));
#define VISIT_SAME_2(n)                                                                                                \
    VISIT_SAME_1(n)                                                                                                    \
    VISIT_SAME_1(n + 1)
#define VISIT_SAME_4(n)                                                                                                \
    VISIT_SAME_2(n)                                                                                                    \
    VISIT_SAME_2(n + 2)
#define VISIT_SAME_8(n)                                                                                                \
    VISIT_SAME_4(n)                                                                                                    \
    VISIT_SAME_4(n + 4)
#define VISIT_SAME_16(n)                                                                                               \
    VISIT_SAME_8(n)                                                                                                    \
    VISIT_SAME_8(n + 8)

template <>
struct VisitSameHelper<0>
{
    template <typename Functor, typename Hold1, typename Hold2>
    static constexpr decltype(auto) issue(Functor&& functor, size_t index, Hold1&& hold1, Hold2&& hold2)
    {
        switch (index)
        {
            default:
                VISIT_SAME_1(0);
        }
    }
};
template <>
struct VisitSameHelper<1>
{
    template <typename Functor, typename Hold1, typename Hold2>
    static constexpr decltype(auto) issue(Functor&& functor, size_t index, Hold1&& hold1, Hold2&& hold2)
    {
        switch (index)
        {
            default:
                VISIT_SAME_2(0);
        }
    }
};

template <>
struct VisitSameHelper<2>
{
    template <typename Functor, typename Hold1, typename Hold2>
    static constexpr decltype(auto) issue(Functor&& functor, size_t index, Hold1&& hold1, Hold2&& hold2)
    {
        switch (index)
        {
            default:
                VISIT_SAME_4(0);
        }
    }
};
template <>
struct VisitSameHelper<3>
{
    template <typename Functor, typename Hold1, typename Hold2>
    static constexpr decltype(auto) issue(Functor&& functor, size_t index, Hold1&& hold1, Hold2&& hold2)
    {
        switch (index)
        {
            default:
                VISIT_SAME_8(0);
        }
    }
};
template <>
struct VisitSameHelper<4>
{
    template <typename Functor, typename Hold1, typename Hold2>
    static constexpr decltype(auto) issue(Functor&& functor, size_t index, Hold1&& hold1, Hold2&& hold2)
    {
        switch (index)
        {
            default:
                VISIT_SAME_16(0);
        }
    }
};

#undef VISIT_SAME_1
#undef VISIT_SAME_2
#undef VISIT_SAME_4
#undef VISIT_SAME_8
#undef VISIT_SAME_16

// Use this as the definition so that the template parameters can auto-deduce.
template <typename Functor, typename Hold1, typename Hold2>
decltype(auto) visitor_same(Functor&& functor, size_t typeIndex, Hold1&& hold1, Hold2&& hold2)
{
    constexpr int size = std::remove_reference<Hold1>::type::size;
    constexpr int version = size <= 1 ? 0 : size <= 2 ? 1 : size <= 4 ? 2 : size <= 8 ? 3 : size <= 16 ? 4 : -1;
    return VisitSameHelper<version>::issue(
        std::forward<Functor>(functor), typeIndex, std::forward<Hold1>(hold1), std::forward<Hold2>(hold2));
}

template <std::size_t I, typename T, typename... Candiates>
struct GetIndexOfHelper;

template <std::size_t I, typename T, typename Candiate>
struct GetIndexOfHelper<I, T, Candiate>
{
    static constexpr size_t value = I;
};

template <std::size_t I, typename T, typename FirstCandiate, typename... Candiates>
struct GetIndexOfHelper<I, T, FirstCandiate, Candiates...>
{
    static constexpr size_t value =
        std::is_same<T, FirstCandiate>::value ? I : GetIndexOfHelper<I + 1, T, Candiates...>::value;
};

template <typename T, typename... Candiates>
static constexpr size_t index_of_v = GetIndexOfHelper<0, T, Candiates...>::value;

template <typename T, typename... Candiates>
static constexpr size_t index_of_init_v =
    GetIndexOfHelper<0, std::remove_const_t<std::remove_reference_t<T>>, std::remove_const_t<Candiates>...>::value;

template <bool IsTriviallyDesctructable, typename... Types>
class VariantBaseImpl;

template <typename... Types>
class VariantBaseImpl<true, Types...> : public VariantHold<true, Types...>
{
public:
    using hold = VariantHold<true, Types...>;
    static constexpr size_t size = VariantHold<false, Types...>::size;
    CARB_VIZ size_t m_index;

    constexpr VariantBaseImpl() noexcept : hold{}, m_index(variant_npos)
    {
    }

    template <size_t I, class... Args>
    constexpr VariantBaseImpl(in_place_index_t<I>, Args&&... args) noexcept
        : hold{ in_place_index<I>, std::forward<Args>(args)... }, m_index(I)
    {
    }
    constexpr size_t index() const
    {
        return m_index;
    }

    void destroy()
    {
        m_index = variant_npos;
    }
};

template <typename... Types>
class VariantBaseImpl<false, Types...> : public VariantHold<false, Types...>
{
public:
    static constexpr size_t size = VariantHold<false, Types...>::size;
    using hold = VariantHold<false, Types...>;
    CARB_VIZ size_t m_index;

    constexpr VariantBaseImpl() noexcept : hold{}, m_index(variant_npos)
    {
    }

    template <size_t I, class... Args>
    constexpr VariantBaseImpl(in_place_index_t<I>, Args&&... args) noexcept
        : hold{ in_place_index<I>, std::forward<Args>(args)... }, m_index(I)
    {
    }

    constexpr size_t index() const
    {
        return m_index;
    }

    void destroy()
    {
        if (m_index != variant_npos)
        {
            detail::visitor(
                [](auto& obj) {
                    using type = typename std::remove_reference<decltype(obj)>::type;
                    obj.~type();
                },
                *this);
        }
        m_index = variant_npos;
    }

    ~VariantBaseImpl()
    {
        destroy();
    }
};

template <typename... Types>
using VariantBase = VariantBaseImpl<conjunction<std::is_trivially_destructible<Types>...>::value, Types...>;

} // namespace detail

struct monostate
{
    constexpr monostate()
    {
    }
};

constexpr bool operator==(monostate, monostate) noexcept
{
    return true;
}
constexpr bool operator!=(monostate, monostate) noexcept
{
    return false;
}
constexpr bool operator<(monostate, monostate) noexcept
{
    return false;
}
constexpr bool operator>(monostate, monostate) noexcept
{
    return false;
}
constexpr bool operator<=(monostate, monostate) noexcept
{
    return true;
}
constexpr bool operator>=(monostate, monostate) noexcept
{
    return true;
}

template <typename Type>
struct variant_alternative<0, variant<Type>>
{
    using type = Type;
};
template <typename Type, typename... OtherTypes>
struct variant_alternative<0, variant<Type, OtherTypes...>>
{
    using type = Type;
};
template <size_t I, typename Type, typename... OtherTypes>
struct variant_alternative<I, variant<Type, OtherTypes...>>
{
    using type = typename variant_alternative<I - 1, variant<OtherTypes...>>::type;
};

template <std::size_t I, typename T>
using variant_alternative_t = typename variant_alternative<I, T>::type;


template <typename... Types>
class CARB_VIZ variant : public detail::VariantBase<Types...>
{
private:
    using base = detail::VariantBase<Types...>;
    using hold = typename base::hold;
    using self_type = variant<Types...>;

public:
    constexpr variant() noexcept : base(in_place_index_t<0>())
    {
    }

    constexpr variant(const variant& other) noexcept
    {
        base::m_index = other.index();
        if (base::m_index != variant_npos)
        {
            detail::visitor_same(
                [](auto& dest, auto& src) {
                    using type = typename std::remove_reference<decltype(src)>::type;
                    new (&dest) type(src);
                },
                other.index(), *this, other);
            base::m_index = other.index();
        }
    }

    constexpr variant(variant&& other) noexcept
    {
        base::m_index = other.index();
        if (base::m_index != variant_npos)
        {
            CARB_ASSERT(base::m_index < sizeof...(Types));
            detail::visitor_same(
                [](auto& dest, auto&& src) {
                    using type = typename std::remove_reference<decltype(src)>::type;
                    new (&dest) type(std::move(src));
                },
                other.index(), *this, other);
            other.base::m_index = variant_npos;
        }
    }

    template <class T>
    constexpr variant(const T& value) noexcept : base{ in_place_index_t<detail::index_of_init_v<T, Types...>>(), value }
    {
    }
    template <class T,
              typename std::enable_if<!std::is_same<variant, std::remove_reference_t<std::remove_const_t<T>>>::value,
                                      bool>::type = true>
    constexpr variant(T&& value) noexcept
        : base{ in_place_index_t<detail::index_of_init_v<T, Types...>>(), std::move(value) }
    {
    }
    template <class T, class... Args>
    constexpr explicit variant(in_place_type_t<T>, Args&&... args)
        : base{ in_place_index_t<detail::index_of_init_v<T, Types...>>(), std::forward<Args>(args)... }
    {
    }
    template <std::size_t I, class... Args>
    constexpr explicit variant(in_place_index_t<I>, Args&&... args)
        : base{ in_place_index_t<I>(), std::forward<Args>(args)... }
    {
    }

    constexpr variant& operator=(const variant& rhs)
    {
        if (this != &rhs)
        {
            if (base::m_index != variant_npos)
            {
                base::destroy();
            }
            if (rhs.base::m_index != variant_npos)
            {
                detail::visitor_same(
                    [](auto& dest, auto& src) {
                        using type = typename std::remove_reference<decltype(src)>::type;
                        new (&dest) type(src);
                    },
                    rhs.index(), *this, rhs);
                base::m_index = rhs.base::m_index;
            }
        }
        return *this;
    }

    constexpr variant& operator=(variant&& rhs)
    {
        if (this == &rhs)
            return *this;

        if (base::m_index != variant_npos)
        {
            base::destroy();
        }
        base::m_index = rhs.base::m_index;
        if (base::m_index != variant_npos)
        {
            CARB_ASSERT(base::m_index < sizeof...(Types));
            detail::visitor_same(
                [](auto& dest, auto&& src) {
                    using type = typename std::remove_reference<decltype(src)>::type;
                    new (&dest) type(std::move(src));
                },
                rhs.index(), *this, rhs);
            rhs.base::m_index = variant_npos;
        }
        return *this;
    }

    template <class T,
              typename std::enable_if<!std::is_same<variant, std::remove_reference_t<std::remove_const_t<T>>>::value,
                                      bool>::type = true>
    variant& operator=(T&& t) noexcept
    {
        constexpr size_t I = detail::index_of_init_v<T, Types...>;
        using type = variant_alternative_t<I, self_type>;
        type& v = detail::variant_get_from_hold<I>(*static_cast<hold*>(this));
        if (index() == I)
        {
            v = std::move(t);
        }
        else
        {
            base::destroy();
            new (&v) type(std::move(t));
            base::m_index = I;
        }
        return *this;
    }

    constexpr bool valueless_by_exception() const noexcept
    {
        return base::m_index == variant_npos;
    }

    template <class T, class... Args>
    auto emplace(Args&&... args) -> std::remove_const_t<std::remove_reference_t<T>>&
    {
        base::destroy();
        using place_type = std::remove_const_t<std::remove_reference_t<T>>;
        base::m_index = detail::index_of_v<place_type, Types...>;
        return *(new (&detail::variant_get_from_hold<detail::index_of_v<T, Types...>>(static_cast<hold&>(*this)))
                     place_type(std::forward<Args>(args)...));
    }

    constexpr std::size_t index() const noexcept
    {
        return base::m_index;
    }
};

template <class T, class... Types>
constexpr bool holds_alternative(const variant<Types...>& v) noexcept
{
    return v.index() == detail::index_of_v<T, Types...>;
}

template <std::size_t I, class... Types>
constexpr std::add_pointer_t<variant_alternative_t<I, variant<Types...>>> get_if(variant<Types...>* pv) noexcept
{
    return (pv && I == pv->index()) ? &detail::variant_get_from_hold<I>(*pv) : nullptr;
}

template <std::size_t I, class... Types>
constexpr std::add_pointer_t<const variant_alternative_t<I, variant<Types...>>> get_if(const variant<Types...>* pv) noexcept
{
    return (pv && I == pv->index()) ? &detail::variant_get_from_hold<I>(*pv) : nullptr;
}

template <class T, class... Types>
constexpr std::add_pointer_t<T> get_if(variant<Types...>* pv) noexcept
{
    return get_if<detail::index_of_v<T, Types...>>(pv);
}

template <class T, class... Types>
constexpr std::add_pointer_t<const T> get_if(const variant<Types...>* pv) noexcept
{
    return get_if<detail::index_of_v<T, Types...>>(pv);
}

// Don't support moves yet...
template <std::size_t I, class... Types>
constexpr variant_alternative_t<I, variant<Types...>>& get(variant<Types...>& v)
{
    auto o = get_if<I>(&v);
    if (o)
    {
        return *o;
    }
    detail::on_bad_variant_access();
}

template <std::size_t I, class... Types>
constexpr const variant_alternative_t<I, variant<Types...>>& get(const variant<Types...>& v)
{
    auto o = get_if<I>(&v);
    if (o)
    {
        return *o;
    }
    detail::on_bad_variant_access();
}

template <class T, class... Types>
constexpr T& get(variant<Types...>& v)
{
    auto o = get_if<T>(&v);
    if (o)
    {
        return *o;
    }
    detail::on_bad_variant_access();
}

template <class T, class... Types>
constexpr const T& get(const variant<Types...>& v)
{
    auto o = get_if<T>(&v);
    if (o)
    {
        return *o;
    }
    detail::on_bad_variant_access();
}

// Comparison
template <class... Types>
constexpr bool operator==(const variant<Types...>& v, const variant<Types...>& w)
{
    return v.index() == w.index() &&
           detail::visitor_same([](const auto& a, const auto& b) { return a == b; }, v.index(), v, w);
}
template <class... Types>
constexpr bool operator!=(const variant<Types...>& v, const variant<Types...>& w)
{
    return v.index() != w.index() ||
           detail::visitor_same([](const auto& a, const auto& b) { return a != b; }, v.index(), v, w);
}
template <class... Types>
constexpr bool operator<(const variant<Types...>& v, const variant<Types...>& w)
{
    // If w.valueless_by_exception(), false; otherwise if v.valueless_by_exception(), true
    if (w.valueless_by_exception())
        return false;
    if (v.valueless_by_exception())
        return true;
    return v.index() < w.index() ||
           (v.index() == w.index() &&
            detail::visitor_same([](const auto& a, const auto& b) { return a < b; }, v.index(), v, w));
}
template <class... Types>
constexpr bool operator>(const variant<Types...>& v, const variant<Types...>& w)
{
    // If v.valueless_by_exception(), false; otherwise if w.valueless_by_exception(), true
    if (v.valueless_by_exception())
        return false;
    if (w.valueless_by_exception())
        return true;
    return v.index() > w.index() ||
           (v.index() == w.index() &&
            detail::visitor_same([](const auto& a, const auto& b) { return a > b; }, v.index(), v, w));
}
template <class... Types>
constexpr bool operator<=(const variant<Types...>& v, const variant<Types...>& w)
{
    // If v.valueless_by_exception(), true; otherwise if w.valueless_by_exception(), false
    if (v.valueless_by_exception())
        return true;
    if (w.valueless_by_exception())
        return false;
    return v.index() < w.index() ||
           (v.index() == w.index() &&
            detail::visitor_same([](const auto& a, const auto& b) { return a <= b; }, v.index(), v, w));
}
template <class... Types>
constexpr bool operator>=(const variant<Types...>& v, const variant<Types...>& w)
{
    // If w.valueless_by_exception(), true; otherwise if v.valueless_by_exception(), false
    if (w.valueless_by_exception())
        return true;
    if (v.valueless_by_exception())
        return false;
    return v.index() > w.index() ||
           (v.index() == w.index() &&
            detail::visitor_same([](const auto& a, const auto& b) { return a >= b; }, v.index(), v, w));
}


// Currently we only support one Variant and not a list.
template <class Visitor, class... Variants>
constexpr decltype(auto) visit(Visitor&& vis, Variants&&... variants)
{
    return detail::visitor(std::forward<Visitor>(vis), std::forward<Variants>(variants)...);
}

} // namespace cpp17
} // namespace carb

namespace std
{

template <>
struct hash<carb::cpp17::monostate>
{
    CARB_NODISCARD size_t operator()(const carb::cpp17::monostate&) const
    {
        // Just return something reasonable, there is not state with monostate.
        // This is just a random hex value.
        return 0x5f631327531c2962ull;
    }
};

template <typename... Types>
struct hash<carb::cpp17::variant<Types...>>
{
    CARB_NODISCARD size_t operator()(const carb::cpp17::variant<Types...>& vis) const
    {
        // Valueless
        if (vis.index() == carb::cpp17::variant_npos)
        {
            return 0;
        }
        return carb::hashCombine(std::hash<size_t>{}(vis.index()), carb::cpp17::visit(
                                                                       // Invoking dohash directly is a compile
                                                                       // error.
                                                                       [](const auto& s) { return dohash(s); }, vis));
    }

private:
    // This is a simple way to remove the C-Ref qualifiers.
    template <class T>
    static size_t dohash(const T& t)
    {
        return std::hash<T>{}(t);
    }
};

} // namespace std
