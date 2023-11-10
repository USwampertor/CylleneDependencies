// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

CARB_IGNOREWARNING_CLANG_WITH_PUSH("-Wunused-local-typedef") // unused type alias 'Tuple'

namespace carb
{

namespace delegate
{

#ifndef DOXYGEN_BUILD
namespace detail
{
template <class F, class Tuple, size_t... I, class... Args>
constexpr decltype(auto) applyExtraImpl(F&& f, Tuple&& t, std::index_sequence<I...>, Args&&... args)
{
    return carb::cpp17::invoke(std::forward<F>(f), std::get<I>(std::forward<Tuple>(t))..., std::forward<Args>(args)...);
}

// like std::apply() but allows additional args
template <class F, class Tuple, class... Args>
constexpr decltype(auto) applyExtra(F&& f, Tuple&& t, Args&&... args)
{
    return applyExtraImpl(std::forward<F>(f), std::forward<Tuple>(t),
                          std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>{},
                          std::forward<Args>(args)...);
}

// Returns a unique identifier for a type that doesn't require RTTI like type_info
template <class T>
size_t typeId()
{
    static const bool addr{ false };
    return size_t(std::addressof(addr));
}
} // namespace detail
#endif

template <class... Args>
Delegate<void(Args...)>::Impl::~Impl()
{
    // Cannot be destroyed while in an active call!
    CARB_CHECK(m_activeCalls.empty());
}

template <class... Args>
Delegate<void(Args...)>::Delegate(Delegate&& other) : m_impl(std::exchange(other.m_impl, std::make_shared<Impl>()))
{
}

template <class... Args>
constexpr Delegate<void(Args...)>::Delegate(std::nullptr_t) : m_impl{}
{
}

template <class... Args>
Delegate<void(Args...)>::Delegate(std::shared_ptr<Impl> pImpl) : m_impl(std::move(pImpl))
{
}

template <class... Args>
auto Delegate<void(Args...)>::operator=(Delegate&& other) -> Delegate&
{
    swap(other);
    return *this;
}

template <class... Args>
void Delegate<void(Args...)>::swap(Delegate& other)
{
    std::swap(m_impl, other.m_impl);
}

template <class... Args>
Delegate<void(Args...)>::~Delegate()
{
    // The only time that m_impl is allowed to be `nullptr` is an empty DelegateRef.
    if (m_impl)
    {
        // UnbindAll() will wait for calls in progress by unbound callbacks to complete before returning, but will allow
        // calls from the current thread to remain.
        UnbindAll();
    }
}

template <class... Args>
template <class Callable, class... BindArgs>
auto Delegate<void(Args...)>::Bind(Handle* hOut, Callable&& func, BindArgs&&... args) -> Handle
{
    using Base = KeyedBinding<Handle>;
    struct Binding : Base
    {
        using Tuple = std::tuple<std::decay_t<BindArgs>...>;
        std::decay_t<Callable> func;
        Tuple bindArgs;
        Binding(Handle h, Callable&& func_, BindArgs&&... args)
            : Base(h), func(std::forward<Callable>(func_)), bindArgs(std::forward<BindArgs>(args)...)
        {
        }

        void Call(Args... args) override
        {
            detail::applyExtra(func, bindArgs, args...);
        }
    };
    Handle h(nextHandle());
    if (hOut)
    {
        *hOut = h;
    }
    Binding* b = new Binding(h, std::forward<Callable>(func), std::forward<BindArgs>(args)...);

    std::lock_guard<carb::thread::mutex> g(m_impl->m_mutex);
    ActiveCall* call = lastCurrentThreadCall();
    if (!call)
    {
        m_impl->m_entries.push_back(*b);
    }
    else
    {
        call->newEntries.push_back(*b);
    }

    return h;
}

template <class... Args>
template <class KeyType, class Callable, class... BindArgs>
void Delegate<void(Args...)>::BindWithKey(KeyType&& key, Callable&& func, BindArgs&&... args)
{
    static_assert(!std::is_same<Handle, std::decay_t<KeyType>>::value, "Handle not allowed as a key type (use Bind())");
    static_assert(
        !std::is_null_pointer<std::decay_t<KeyType>>::value, "nullptr_t not allowed as a key type (use Bind())");
    using Base = KeyedBinding<std::decay_t<KeyType>>;
    struct Binding : Base
    {
        using Tuple = std::tuple<std::decay_t<BindArgs>...>;
        std::decay_t<Callable> func;
        Tuple bindArgs;
        Binding(KeyType&& key, Callable&& func_, BindArgs&&... args)
            : Base(std::forward<KeyType>(key)),
              func(std::forward<Callable>(func_)),
              bindArgs(std::forward<BindArgs>(args)...)
        {
        }

        void Call(Args... args) override
        {
            detail::applyExtra(func, bindArgs, args...);
        }
    };
    Binding* b = new Binding(std::forward<KeyType>(key), std::forward<Callable>(func), std::forward<BindArgs>(args)...);

    std::lock_guard<carb::thread::mutex> g(m_impl->m_mutex);
    ActiveCall* call = lastCurrentThreadCall();
    if (!call)
    {
        m_impl->m_entries.push_back(*b);
    }
    else
    {
        call->newEntries.push_back(*b);
    }
}

template <class... Args>
template <class KeyType>
bool Delegate<void(Args...)>::Unbind(KeyType&& key)
{
    using BindingType = KeyedBinding<std::decay_t<KeyType>>;
    std::unique_lock<carb::thread::mutex> g(m_impl->m_mutex);
    for (auto iter = m_impl->m_entries.begin(); iter != m_impl->m_entries.end(); ++iter)
    {
        if (iter->getKeyType() == BindingType::keyType())
        {
            BindingType& binding = static_cast<BindingType&>(*iter);
            if (binding.key == key)
            {
                UnbindInternal(g, iter);
                return true;
            }
        }
    }

    // Must belong to a specific list. Find it and remove it.
    for (auto& active : m_impl->m_activeCalls)
    {
        for (auto& entry : active.newEntries)
        {
            if (entry.getKeyType() == BindingType::keyType())
            {
                BindingType& binding = static_cast<BindingType&>(entry);
                if (binding.key == key)
                {
                    active.newEntries.remove(entry);
                    entry.release();
                    return true;
                }
            }
        }
    }

    return false;
}

template <class... Args>
template <class KeyType>
bool Delegate<void(Args...)>::HasKey(KeyType&& key) const noexcept
{
    using BindingType = KeyedBinding<std::decay_t<KeyType>>;
    std::unique_lock<carb::thread::mutex> g(m_impl->m_mutex);
    for (auto iter = m_impl->m_entries.begin(); iter != m_impl->m_entries.end(); ++iter)
    {
        if (iter->getKeyType() == BindingType::keyType())
        {
            BindingType& binding = static_cast<BindingType&>(*iter);
            if (binding.key == key)
            {
                return true;
            }
        }
    }

    // May belong to a specific list
    for (auto& active : m_impl->m_activeCalls)
    {
        for (auto& entry : active.newEntries)
        {
            if (entry.getKeyType() == BindingType::keyType())
            {
                BindingType& binding = static_cast<BindingType&>(entry);
                if (binding.key == key)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

template <class... Args>
bool Delegate<void(Args...)>::UnbindCurrent()
{
    std::unique_lock<carb::thread::mutex> g(m_impl->m_mutex);

    ActiveCall* ac = lastCurrentThreadCall();
    if (ac && ac->cur != m_impl->m_entries.end())
    {
        UnbindInternal(g, ac->cur);
        return true;
    }
    return false;
}

template <class... Args>
void Delegate<void(Args...)>::UnbindAll()
{
    std::unique_lock<carb::thread::mutex> g(m_impl->m_mutex);

    while (m_impl->m_entries.rbegin() != m_impl->m_entries.rend())
    {
        UnbindInternal(g, --m_impl->m_entries.rbegin().base());
        // UnbindInternal() unlocks, so re-lock here
        g.lock();
    }

    for (auto& active : m_impl->m_activeCalls)
    {
        while (!active.newEntries.empty())
        {
            active.newEntries.remove(active.newEntries.front()).release();
        }
    }
}

template <class... Args>
size_t Delegate<void(Args...)>::Count() const noexcept
{
    return m_impl->m_entries.size();
}

template <class... Args>
bool Delegate<void(Args...)>::HasPending() const noexcept
{
    std::unique_lock<carb::thread::mutex> g(m_impl->m_mutex);
    for (auto& active : m_impl->m_activeCalls)
    {
        if (!active.newEntries.empty())
            return true;
    }
    return false;
}

template <class... Args>
bool Delegate<void(Args...)>::IsEmpty() const noexcept
{
    std::unique_lock<carb::thread::mutex> g(m_impl->m_mutex);
    if (!m_impl->m_entries.empty())
        return false;

    for (auto& active : m_impl->m_activeCalls)
    {
        if (!active.newEntries.empty())
            return false;
    }

    return true;
}

template <class... Args>
template <class KeyType>
std::vector<std::decay_t<KeyType>> Delegate<void(Args...)>::GetKeysByType() const
{
    std::vector<std::decay_t<KeyType>> vec;

    using BindingType = KeyedBinding<std::decay_t<KeyType>>;
    std::unique_lock<carb::thread::mutex> g(m_impl->m_mutex);

    for (auto& entry : m_impl->m_entries)
    {
        if (entry.getKeyType() == BindingType::keyType())
        {
            const BindingType& binding = static_cast<const BindingType&>(entry);
            vec.emplace_back(binding.key);
        }
    }

    for (auto& active : m_impl->m_activeCalls)
    {
        for (auto& entry : active.newEntries)
        {
            if (entry.getKeyType() == BindingType::keyType())
            {
                const BindingType& binding = static_cast<const BindingType&>(entry);
                vec.emplace_back(binding.key);
            }
        }
    }

    return vec;
}

template <class... Args>
void Delegate<void(Args...)>::Call(Args... args)
{
    std::shared_ptr<Impl> impl;
    std::unique_lock<carb::thread::mutex> g(m_impl->m_mutex);

    // Early out if there is nothing to do
    if (m_impl->m_entries.empty())
    {
        return;
    }

    impl = m_impl; // Hold a reference while calling

    ActiveCall activeCall;
    impl->m_activeCalls.push_back(activeCall);

    activeCall.next = activeCall.cur = impl->m_entries.begin();
    ++activeCall.next;

    for (;;)
    {
        BaseBinding& cc = *activeCall.cur;
        cc.addRef();
        g.unlock();

        cc.Call(args...);

        g.lock();
        cc.release(activeCall.cur == impl->m_entries.end());

        activeCall.cur = activeCall.next;
        if (activeCall.cur == impl->m_entries.end())
        {
            break;
        }

        ++activeCall.next;
    }

    impl->m_activeCalls.remove(activeCall);

    if (!activeCall.newEntries.empty())
    {
        // Add the new entries to the end of the list
        BaseBinding& cc = activeCall.newEntries.front();
        impl->m_entries.splice(impl->m_entries.end(), activeCall.newEntries);
        auto newiter = impl->m_entries.iter_from_value(cc);

        // Add the new entries to any active calls in progress
        for (auto& call : impl->m_activeCalls)
        {
            if (call.next == impl->m_entries.end())
            {
                call.next = newiter;
            }
        }
    }
}

template <class... Args>
void Delegate<void(Args...)>::operator()(Args... args)
{
    Call(std::forward<Args>(args)...);
}

template <class... Args>
struct Delegate<void(Args...)>::BaseBinding
{
    constexpr BaseBinding() = default;
    virtual ~BaseBinding() = default;

    virtual size_t getKeyType() const = 0;

    virtual void Call(Args...) = 0;

    void addRef() noexcept
    {
        auto old = refCount.fetch_add(1, std::memory_order_relaxed);
        CARB_UNUSED(old);
        CARB_ASSERT(old != 0); // no resurrection
        CARB_ASSERT((old + 1) != 0); // no rollover
    }
    void addRefs(size_t count) noexcept
    {
        auto old = refCount.fetch_add(count, std::memory_order_relaxed);
        CARB_UNUSED(old);
        CARB_ASSERT(old != 0); // no resurrection
        CARB_ASSERT((old + count) >= old); // no rollover or negative
    }
    void release(bool notifyLastRef = false)
    {
        auto const current = (refCount.fetch_sub(1, std::memory_order_release) - 1);
        CARB_ASSERT((current + 1) != 0); // no rollover/double-release
        if (current == 1 && notifyLastRef)
        {
            refCount.notify_one();
        }
        if (current == 0)
        {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete this;
        }
    }
    void waitForLastRef()
    {
        auto ref = refCount.load(std::memory_order_relaxed);
        while (ref != 1)
        {
            refCount.wait(ref, std::memory_order_relaxed);
            ref = refCount.load(std::memory_order_relaxed);
        }
    }

    carb::cpp20::atomic_size_t refCount{ 1 };
    carb::container::IntrusiveListLink<BaseBinding> link;
};

template <class... Args>
template <class KeyType>
struct Delegate<void(Args...)>::KeyedBinding : BaseBinding
{
    // Ensure that KeyType is a decayed type
    static_assert(std::is_same<std::decay_t<KeyType>, KeyType>::value, "Must be decayed!");

    static size_t keyType()
    {
        return detail::typeId<KeyType>();
    }

    virtual size_t getKeyType() const override
    {
        return keyType();
    }

    KeyType const key;

    template <class Key>
    constexpr KeyedBinding(Key&& key_) : BaseBinding(), key(std::forward<Key>(key_))
    {
    }
};

template <class... Args>
struct Delegate<void(Args...)>::ActiveCall
{
    typename Container::iterator cur;
    typename Container::iterator next;
    uint32_t threadId{ carb::this_thread::getId() };
    Container newEntries;

    carb::container::IntrusiveListLink<ActiveCall> link;
};

template <class... Args>
auto Delegate<void(Args...)>::lastCurrentThreadCall() -> ActiveCall*
{
    if (!m_impl->m_activeCalls.empty())
    {
        const uint32_t threadId = this_thread::getId();
        for (auto iter = m_impl->m_activeCalls.rbegin(); iter != m_impl->m_activeCalls.rend(); ++iter)
        {
            if (iter->threadId == threadId)
            {
                return std::addressof(*iter);
            }
        }
    }
    return nullptr;
}

template <class... Args>
auto Delegate<void(Args...)>::lastCurrentThreadCall() const -> const ActiveCall*
{
    if (!m_impl->m_activeCalls.empty())
    {
        const uint32_t threadId = this_thread::getId();
        for (auto iter = m_impl->m_activeCalls.rbegin(); iter != m_impl->m_activeCalls.rend(); ++iter)
        {
            if (iter->threadId == threadId)
            {
                return std::addressof(*iter);
            }
        }
    }
    return nullptr;
}

template <class... Args>
void Delegate<void(Args...)>::UnbindInternal(std::unique_lock<carb::thread::mutex>& g, typename Container::iterator iter)
{
    // Found it. See if it's involved in any active calls and wait.
    bool hasActiveInOtherThreads = false;
    size_t thisThreadActiveCount = 0;
    if (!m_impl->m_activeCalls.empty())
    {
        const uint32_t thisThread = this_thread::getId();
        for (auto& active : m_impl->m_activeCalls)
        {
            // Skip if next
            if (active.next == iter)
            {
                ++active.next;
            }
            // Handle if current
            if (active.cur == iter)
            {
                // Clear the current entry. This signals to the ActiveCall that we're waiting on it.
                active.cur = m_impl->m_entries.end();
                if (active.threadId != thisThread)
                {
                    hasActiveInOtherThreads = true;
                }
                else
                {
                    // Since we want to wait for the last ref, release any references that the current thread has on it.
                    ++thisThreadActiveCount;
                    iter->release();
                }
            }
        }
    }

    BaseBinding& cc = *iter;
    m_impl->m_entries.remove(iter);
    g.unlock();

    if (hasActiveInOtherThreads)
    {
        cc.waitForLastRef();
    }
    if (thisThreadActiveCount)
    {
        // Add back any references that the current thread was holding
        cc.addRefs(thisThreadActiveCount);
    }
    // Release the implicit ref. If the current thread was holding any references, the earliest Call() will be the one
    // to actually release it.
    cc.release();
}

template <class... Args>
size_t Delegate<void(Args...)>::nextHandle()
{
    static std::atomic_size_t handle{ 1 };
    size_t val;
    do
    {
        val = handle.fetch_add(1, std::memory_order_relaxed);
    } while (CARB_UNLIKELY(val == 0));
    return val;
}

template <class... Args>
constexpr DelegateRef<void(Args...)>::DelegateRef() noexcept : m_delegate{ nullptr }
{
}

template <class... Args>
DelegateRef<void(Args...)>::DelegateRef(DelegateType& delegate) : m_delegate{ delegate.m_impl }
{
}

template <class... Args>
DelegateRef<void(Args...)>::DelegateRef(const DelegateRef& other) : m_delegate{ other.m_delegate.m_impl }
{
}

template <class... Args>
DelegateRef<void(Args...)>::~DelegateRef()
{
    // The Delegate destructor calls UnbindAll(), which we definitely don't want. So just reset our reference.
    m_delegate.m_impl.reset();
}

template <class... Args>
auto DelegateRef<void(Args...)>::operator=(const DelegateRef& other) -> DelegateRef&
{
    m_delegate.m_impl = other.m_delegate.m_impl;
    return *this;
}

template <class... Args>
DelegateRef<void(Args...)>::operator bool() const noexcept
{
    return m_delegate.m_impl.operator bool();
}

template <class... Args>
void DelegateRef<void(Args...)>::reset()
{
    m_delegate.m_impl.reset();
}

template <class... Args>
void DelegateRef<void(Args...)>::reset(DelegateType& delegate)
{
    m_delegate.m_impl = delegate.m_impl;
}

template <class... Args>
void DelegateRef<void(Args...)>::swap(DelegateRef& other)
{
    m_delegate.swap(other.m_delegate);
}

template <class... Args>
auto DelegateRef<void(Args...)>::get() const noexcept -> DelegateType*
{
    return m_delegate.m_impl ? const_cast<DelegateType*>(&m_delegate) : nullptr;
}

template <class... Args>
auto DelegateRef<void(Args...)>::operator*() const noexcept -> DelegateType&
{
    CARB_ASSERT(*this);
    return const_cast<DelegateType&>(m_delegate);
}

template <class... Args>
auto DelegateRef<void(Args...)>::operator->() const noexcept -> DelegateType*
{
    CARB_ASSERT(*this);
    return const_cast<DelegateType*>(&m_delegate);
}


} // namespace delegate

} // namespace carb

CARB_IGNOREWARNING_CLANG_POP
