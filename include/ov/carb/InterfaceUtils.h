// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Utilities for Carbonite Interface management.
#pragma once

#include "Framework.h"
#include "cpp20/Atomic.h"

namespace carb
{

#ifndef DOXYGEN_BUILD
namespace detail
{
template <typename InterfaceT, const char* PluginName>
class CachedInterface
{
public:
    constexpr CachedInterface() = default;
    ~CachedInterface()
    {
        reset();
    }

    InterfaceT* get()
    {
        auto iface = m_cachedInterface.load(std::memory_order_relaxed);
        if (CARB_LIKELY(iface))
        {
            return iface;
        }
        return getInternal();
    }

    void reset()
    {
        ::carb::Framework* framework = ::carb::getFramework();
        if (!framework)
        {
            // Framework no longer valid or already unloaded.
            return;
        }

        auto iface = m_cachedInterface.exchange(nullptr, std::memory_order_relaxed);
        if (iface)
        {
            framework->removeReleaseHook(iface, sReleaseHook, this);
        }
        framework->removeReleaseHook(nullptr, sFrameworkReleased, this);
        m_reqState.store(NotRequested, std::memory_order_release);
        m_reqState.notify_all();
    }

private:
    enum RequestState
    {
        NotRequested,
        Requesting,
        Finished,
    };
    std::atomic<InterfaceT*> m_cachedInterface{ nullptr };
    carb::cpp20::atomic<RequestState> m_reqState{ NotRequested };

    static void sReleaseHook(void* iface, void* this_)
    {
        static_cast<CachedInterface*>(this_)->releaseHook(iface);
    }
    static void sFrameworkReleased(void*, void* this_)
    {
        // The Framework is fully released. Reset our request state.
        static_cast<CachedInterface*>(this_)->reset();
    }
    void releaseHook(void* iface)
    {
        // Clear the cached interface pointer, but don't fully reset. Further attempts to get() will proceed to
        // getInternal(), but will not attempt to acquire the interface again.
        CARB_ASSERT(iface == m_cachedInterface);
        CARB_UNUSED(iface);
        m_cachedInterface.store(nullptr, std::memory_order_relaxed);
    }

    CARB_NOINLINE InterfaceT* getInternal()
    {
        ::carb::Framework* framework = ::carb::getFramework();
        if (!framework)
        {
            return nullptr;
        }

        RequestState state = m_reqState.load(std::memory_order_acquire);
        while (state != Finished)
        {
            if (state == NotRequested && m_reqState.compare_exchange_weak(
                                             state, Requesting, std::memory_order_relaxed, std::memory_order_relaxed))
            {
                InterfaceT* iface = framework->tryAcquireInterface<InterfaceT>(PluginName);
                if (!iface)
                {
                    // Failed to acquire. Reset to initial state
                    m_reqState.store(NotRequested, std::memory_order_release);
                    m_reqState.notify_all();
                    return nullptr;
                }
                if (CARB_UNLIKELY(!framework->addReleaseHook(iface, sReleaseHook, this)))
                {
                    // This could only happen if something released the interface between us acquiring it and adding
                    // the release hook. Repeat the process again.
                    state = NotRequested;
                    m_reqState.store(state, std::memory_order_release);
                    m_reqState.notify_all();
                    continue;
                }
                bool b = framework->addReleaseHook(nullptr, sFrameworkReleased, this);
                CARB_UNUSED(b);
                CARB_ASSERT(b);
                m_cachedInterface.store(iface, std::memory_order_relaxed);
                m_reqState.store(Finished, std::memory_order_release);
                m_reqState.notify_all();
                return iface;
            }
            else if (state == Requesting)
            {
                m_reqState.wait(state, std::memory_order_relaxed);
                state = m_reqState.load(std::memory_order_acquire);
            }
        }
        return m_cachedInterface.load(std::memory_order_relaxed);
    }
};

template <class T, const char* PluginName>
CachedInterface<T, PluginName>& cachedInterface()
{
    static CachedInterface<T, PluginName> cached;
    return cached;
}
} // namespace detail
#endif

/**
 * Retrieves the specified interface as if from Framework::tryAcquireInterface() and caches it for fast retrieval.
 *
 * If the interface is released with Framework::releaseInterface(), the cached interface will be automatically
 * cleared. Calls to getCachedInterface() after this point will return `nullptr`. In order for getCachedInterface() to
 * call Framework::tryAcquireInterface() again, first call resetCachedInterface().
 *
 * @note Releasing the Carbonite Framework with carb::releaseFramework() automatically calls resetCachedInterface().
 *
 * @tparam InterfaceT The interface class to retrieve.
 * @tparam PluginName The name of a specific plugin to keep cached. Note: this must be a global char array or `nullptr`.
 * @returns The loaded and acquired interface class if successfully acquired through Framework::tryAcquireInterface(),
 * or a previously cached value. If the interface could not be found, or has been released with releaseFramework(),
 * `nullptr` is returned.
 */
template <typename InterfaceT, const char* PluginName = nullptr>
CARB_NODISCARD inline InterfaceT* getCachedInterface()
{
    return ::carb::detail::cachedInterface<InterfaceT, PluginName>().get();
}

/**
 * Resets any previously-cached interface of the given type and allows it to be acquired again.
 *
 * @note This does NOT *release* the interface as if Framework::releaseInterface() were called. It merely resets the
 * cached state so that getCachedInterface() will call Framework::tryAcquireInterface() again.
 *
 * @tparam InterfaceT The type of interface class to evict from cache.
 * @tparam PluginName The name of a specific plugin that is cached. Note: this must be a global char array or `nullptr`.
 */
template <typename InterfaceT, const char* PluginName = nullptr>
inline void resetCachedInterface()
{
    ::carb::detail::cachedInterface<InterfaceT, PluginName>().reset();
}

} // namespace carb
