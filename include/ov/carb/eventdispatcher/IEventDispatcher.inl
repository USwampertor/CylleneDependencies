// Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

#include "../variant/IVariant.h"
#include "../variant/VariantUtils.h"

namespace carb
{
namespace eventdispatcher
{

namespace detail
{

template <class T>
NamedVariant translate(std::pair<RStringKey, T>&& p)
{
    return { p.first, variant::Variant{ p.second } };
}

struct NamedVariantLess
{
    bool operator()(const NamedVariant& lhs, const NamedVariant& rhs)
    {
        return lhs.name.owner_before(rhs.name);
    }
};

struct NamedVariantEqual
{
    bool operator()(const NamedVariant& lhs, const NamedVariant& rhs)
    {
        return lhs.name == rhs.name;
    }
};

} // namespace detail

inline constexpr ObserverGuard::ObserverGuard() noexcept : m_o(kInvalidObserver)
{
}

inline constexpr ObserverGuard::ObserverGuard(Observer o) noexcept : m_o(o)
{
}

inline ObserverGuard::ObserverGuard(ObserverGuard&& other) noexcept : m_o(std::exchange(other.m_o, kInvalidObserver))
{
}

inline ObserverGuard::~ObserverGuard() noexcept
{
    reset();
}

inline ObserverGuard& ObserverGuard::operator=(ObserverGuard&& other) noexcept
{
    swap(other);
    return *this;
}

inline Observer ObserverGuard::release() noexcept
{
    return std::exchange(m_o, kInvalidObserver);
}

inline void ObserverGuard::reset(Observer o) noexcept
{
    if (m_o != kInvalidObserver)
    {
        auto iface = carb::getCachedInterface<IEventDispatcher>();
        CARB_ASSERT(iface, "Failed to acquire interface IEventDispatcher");
        iface->stopObserving(m_o);
    }
    m_o = o;
}

inline void ObserverGuard::swap(ObserverGuard& o) noexcept
{
    std::swap(m_o, o.m_o);
}

constexpr inline Observer ObserverGuard::get() const noexcept
{
    return m_o;
}

inline ObserverGuard::operator bool() const noexcept
{
    return m_o != kInvalidObserver;
}

inline bool operator==(const ObserverGuard& lhs, const ObserverGuard& rhs)
{
    return lhs.get() == rhs.get();
}

inline bool operator!=(const ObserverGuard& lhs, const ObserverGuard& rhs)
{
    return lhs.get() != rhs.get();
}

inline bool operator<(const ObserverGuard& lhs, const ObserverGuard& rhs)
{
    return lhs.get() < rhs.get();
}

inline void swap(ObserverGuard& lhs, ObserverGuard& rhs) noexcept
{
    lhs.swap(rhs);
}

template <class Invokable, class... Args>
CARB_NODISCARD ObserverGuard
IEventDispatcher::observeEvent(int order, RString eventName, Invokable&& invokable, Args&&... filterArgs)
{
    using FunctionType = std::function<void(const Event&)>;
    auto pFunc = new FunctionType{ std::forward<Invokable>(invokable) };
    std::array<NamedVariant, sizeof...(filterArgs)> variants{ detail::translate(std::forward<Args>(filterArgs))... };
    std::sort(variants.begin(), variants.end(), detail::NamedVariantLess{});
    return ObserverGuard(
        internalObserveEvent(order, eventName, variants.size(), variants.data(),
                             [](const Event& event, void* ud) { (*static_cast<FunctionType*>(ud))(event); },
                             [](void* ud) { delete static_cast<FunctionType*>(ud); }, pFunc));
}

template <class Invokable, class InIter>
CARB_NODISCARD ObserverGuard
IEventDispatcher::observeEventIter(int order, RString eventName, Invokable&& invokable, InIter begin, InIter end)
{
    using FunctionType = std::function<void(const Event&)>;
    auto pFunc = new FunctionType{ std::forward<Invokable>(invokable) };
    std::vector<NamedVariant> variants{ begin, end };
    std::sort(variants.begin(), variants.end(), detail::NamedVariantLess{});
    return ObserverGuard(
        internalObserveEvent(order, eventName, variants.size(), variants.data(),
                             [](const Event& event, void* ud) { (*static_cast<FunctionType*>(ud))(event); },
                             [](void* ud) { delete static_cast<FunctionType*>(ud); }, pFunc));
}

template <class... Args>
bool IEventDispatcher::hasObservers(RString eventName, Args&&... filterArgs)
{
    std::array<NamedVariant, sizeof...(filterArgs)> variants{ detail::translate(std::forward<Args>(filterArgs))... };
    std::sort(variants.begin(), variants.end(), detail::NamedVariantLess{});
    CARB_ASSERT(std::adjacent_find(variants.begin(), variants.end(), detail::NamedVariantEqual{}) == variants.end(),
                "At least one non-unique key specified");
    return internalHasObservers(eventName, variants.size(), variants.data());
}

template <class InIter>
bool IEventDispatcher::hasObserversIter(RString eventName, InIter begin, InIter end)
{
    std::vector<NamedVariant> variants{ begin, end };
    std::sort(variants.begin(), variants.end(), detail::NamedVariantLess{});
    CARB_ASSERT(std::adjacent_find(variants.begin(), variants.end(), detail::NamedVariantEqual{}) == variants.end(),
                "At least one non-unique key specified");
    return internalHasObservers(eventName, variants.size(), variants.data());
}

template <class... Args>
size_t IEventDispatcher::dispatchEvent(RString eventName, Args&&... payload)
{
    std::array<NamedVariant, sizeof...(payload)> variants{ detail::translate(std::forward<Args>(payload))... };
    std::sort(variants.begin(), variants.end(), detail::NamedVariantLess{});
    CARB_ASSERT(std::adjacent_find(variants.begin(), variants.end(), detail::NamedVariantEqual{}) == variants.end(),
                "Event has duplicate keys");
    return internalDispatch({ eventName, variants.size(), variants.data() });
}

template <class InIter>
size_t IEventDispatcher::dispatchEventIter(RString eventName, InIter begin, InIter end)
{
    std::vector<NamedVariant> variants{ begin, end };
    std::sort(variants.begin(), variants.end(), detail::NamedVariantLess{});
    CARB_ASSERT(std::adjacent_find(variants.begin(), variants.end(), detail::NamedVariantEqual{}) == variants.end(),
                "Event has duplicate keys");
    return internalDispatch({ eventName, variants.size(), variants.data() });
}

inline const variant::Variant* Event::get(RStringKey key) const
{
    auto iter = std::lower_bound(variants, variants + numVariants, NamedVariant{ key, {} }, detail::NamedVariantLess{});
    if (iter != (variants + numVariants) && iter->name == key)
    {
        return &iter->value;
    }
    return nullptr;
}

inline bool Event::hasKey(RStringKey key) const
{
    return std::binary_search(variants, variants + numVariants, NamedVariant{ key, {} }, detail::NamedVariantLess{});
}

template <class T>
cpp17::optional<T> Event::getValue(RStringKey key) const
{
    if (auto variant = get(key))
        return variant->getValue<T>();
    return {};
}

template <class T>
T Event::getValueOr(RStringKey key, T&& defaultValue) const
{
    return getValue<T>(key).value_or(std::forward<T>(defaultValue));
}

} // namespace eventdispatcher
} // namespace carb

// Specialization for std::hash
template <>
struct std::hash<::carb::eventdispatcher::ObserverGuard>
{
    size_t operator()(const ::carb::eventdispatcher::ObserverGuard& g) const noexcept
    {
        return std::hash<carb::eventdispatcher::Observer>{}(g.get());
    }
};
