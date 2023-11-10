// Copyright (c) 2018-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief carb.profiler helper utilities.
#pragma once

#include "IProfiler.h"
#include "../cpp20/Atomic.h"
#include "../settings/ISettings.h"
#include "../InterfaceUtils.h"

namespace carb
{
namespace profiler
{

/**
 * Profiler channel which can be configured via `ISettings`.
 *
 * @warning Do not use this class directly. Instead, use \ref CARB_PROFILE_DECLARE_CHANNEL().
 */
class Channel final
{
    uint64_t m_mask;
    bool m_enabled;
    const char* m_name;
    Channel* m_next;

    struct ModuleData
    {
        Channel* head{ nullptr };
        LoadHookHandle onSettingsLoadHandle{ kInvalidLoadHook };
        dictionary::SubscriptionId* changeSubscription{ nullptr };

#if CARB_ASSERT_ENABLED
        ~ModuleData()
        {
            // If these weren't unregistered we could crash later
            CARB_ASSERT(onSettingsLoadHandle == kInvalidLoadHook);
            CARB_ASSERT(changeSubscription == nullptr);
        }
#endif
    };

    static ModuleData& moduleData()
    {
        static ModuleData s_moduleData;
        return s_moduleData;
    }

    static void onSettingsLoad(const PluginDesc&, void*)
    {
        // DO NOT USE getCachedInterface here! This is called by a load hook, which can be triggered by
        // getCachedInterface in this module. This means if we were to recursively call getCachedInterface() here, we
        // could hang indefinitely as this thread is the thread responsible for loading the cached interface.
        if (loadSettings(getFramework()->tryAcquireInterface<settings::ISettings>(), true))
        {
            g_carbFramework->removeLoadHook(moduleData().onSettingsLoadHandle);
            moduleData().onSettingsLoadHandle = kInvalidLoadHook;
        }
    }

    static void onSettingsUnload(void*, void*)
    {
        // Settings was unloaded. Make sure we no longer have a subscription callback.
        moduleData().changeSubscription = nullptr;
    }

    static void onSettingsChange(const dictionary::Item*,
                                 const dictionary::Item* changedItem,
                                 dictionary::ChangeEventType eventType,
                                 void*)
    {
        if (eventType == dictionary::ChangeEventType::eDestroyed)
            return;

        auto dict = getCachedInterface<dictionary::IDictionary>();

        // Only care about elements that can change at runtime.
        const char* name = dict->getItemName(changedItem);
        if (strcmp(name, "enabled") != 0 && strcmp(name, "mask") != 0)
            return;

        loadSettings(
            getCachedInterface<settings::ISettings>(), false, dict->getItemName(dict->getItemParent(changedItem)));
    }

    static bool loadSettings(settings::ISettings* settings, bool initial, const char* channelName = nullptr)
    {
        // Only proceed if settings is already initialized
        if (!settings)
            return false;

        auto dict = carb::getCachedInterface<dictionary::IDictionary>();
        if (!dict)
            return false;

        auto root = settings->getSettingsDictionary("/profiler/channels");
        if (root)
        {
            for (Channel* c = moduleData().head; c; c = c->m_next)
            {
                if (channelName && strcmp(c->m_name, channelName) != 0)
                    continue;

                auto channelRoot = dict->getItem(root, c->m_name);
                if (!channelRoot)
                    continue;

                auto enabled = dict->getItem(channelRoot, "enabled");
                if (enabled)
                {
                    c->setEnabled(dict->getAsBool(enabled));
                }
                auto mask = dict->getItem(channelRoot, "mask");
                if (mask)
                {
                    c->setMask(uint64_t(dict->getAsInt64(mask)));
                }
            }
        }

        // Register a change subscription on initial setup if we have any channels.
        if (initial && !moduleData().changeSubscription && moduleData().head)
        {
            moduleData().changeSubscription =
                settings->subscribeToTreeChangeEvents("/profiler/channels", onSettingsChange, nullptr);

            ::g_carbFramework->addReleaseHook(settings, onSettingsUnload, nullptr);
        }
        return true;
    }

public:
    /**
     * Constructor
     *
     * @warning Do not call this directly. Instead use \ref CARB_PROFILE_DECLARE_CHANNEL().
     *
     * @warning Instances of this class must have static storage and module-lifetime, therefore they may only exist at
     * file-level scope, class-level (static) scope, or namespace-level scope only. Anything else is undefined behavior.
     *
     * @param mask The default profiler mask for this channel.
     * @param enabled Whether this channel is enabled by default.
     * @param name A literal string that is used to look up settings keys.
     */
    Channel(uint64_t mask, bool enabled, const char* name) : m_mask(mask), m_enabled(enabled), m_name(name)
    {
        // Add ourselves to the list of channels for this module
        auto& head = moduleData().head;
        m_next = head;
        head = this;
    }

    /**
     * Returns the name of this channel.
     * @returns the channel name.
     */
    const char* getName() const noexcept
    {
        return m_name;
    }

    /**
     * Returns the current mask for this channel.
     * @returns the current mask.
     */
    uint64_t getMask() const noexcept
    {
        return m_mask;
    }

    /**
     * Sets the mask value for *this.
     * @param mask The new profiler mask value.
     */
    void setMask(uint64_t mask) noexcept
    {
        cpp20::atomic_ref<uint64_t>(m_mask).store(mask, std::memory_order_release);
    }

    /**
     * Returns whether this channel is enabled.
     * @returns \c true if this channel is enabled; \c false otherwise.
     */
    bool isEnabled() const noexcept
    {
        return m_enabled;
    }

    /**
     * Sets *this to enabled or disabled.
     *
     * @param enabled Whether to enable (\c true) or disable (\c false) the channel.
     */
    void setEnabled(bool enabled) noexcept
    {
        cpp20::atomic_ref<bool>(m_enabled).store(enabled, std::memory_order_release);
    }

    /**
     * Called by profiler::registerProfilerForClient() to initialize all channels.
     *
     * If ISettings is available, it is queried for this module's channel's settings, and a subscription is installed to
     * be notified when settings change. If ISettings is not available, a load hook is installed with the framework in
     * order to be notified if and when ISettings becomes available.
     */
    static void onProfilerRegistered()
    {
        // example-begin acquire-without-init
        // Don't try to load settings, but if it's already available we will load settings from it.
        auto settings = g_carbFramework->tryAcquireExistingInterface<settings::ISettings>();
        // example-end acquire-without-init
        if (!loadSettings(settings, true))
        {
            // If settings isn't available, wait for it to load.
            moduleData().onSettingsLoadHandle =
                g_carbFramework->addLoadHook<settings::ISettings>(nullptr, onSettingsLoad, nullptr);
        }
    }

    /**
     * Called by profiler::deregisterProfilerForClient() to uninitialize all channels.
     *
     * Any load hooks and subscriptions installed with ISettings are removed.
     */
    static void onProfilerUnregistered()
    {
        if (moduleData().onSettingsLoadHandle != kInvalidLoadHook)
        {
            g_carbFramework->removeLoadHook(moduleData().onSettingsLoadHandle);
            moduleData().onSettingsLoadHandle = kInvalidLoadHook;
        }
        if (moduleData().changeSubscription)
        {
            // Don't re-initialize settings if it's already been unloaded (though in this case we should've gotten a
            // callback)
            auto settings = g_carbFramework->tryAcquireExistingInterface<settings::ISettings>();
            CARB_ASSERT(settings);
            if (settings)
            {
                settings->unsubscribeToChangeEvents(moduleData().changeSubscription);
                g_carbFramework->removeReleaseHook(settings, onSettingsUnload, nullptr);
            }
            moduleData().changeSubscription = nullptr;
        }
    }
};

/**
 * Helper class that allows to automatically stop profiling upon leaving block.
 * @note Typically this is not used by an application. It is generated automatically by the CARB_PROFILE_ZONE() macro.
 */
class ProfileZoneStatic final
{
    const uint64_t m_mask;
    ZoneId m_zoneId;

public:
    /**
     * Constructor.
     *
     * @param mask Profiling bitmask.
     * @param tup A `std::tuple` of registered static strings for `__func__`, `__FILE__` and event name.
     * @param line Line number in the file where the profile zone was started (usually `__LINE__`).
     */
    ProfileZoneStatic(const uint64_t mask, std::tuple<StaticStringType, StaticStringType, StaticStringType> tup, int line)
        : m_mask(mask)
    {
        if (g_carbProfiler && ((mask ? mask : kCaptureMaskDefault) & g_carbProfilerMask.load(std::memory_order_acquire)))
            m_zoneId = g_carbProfiler->beginStatic(m_mask, std::get<0>(tup), std::get<1>(tup), line, std::get<2>(tup));
        else
            m_zoneId = kNoZoneId;
    }

    /**
     * Constructor.
     *
     * @param channel A profiling channel.
     * @param tup A `std::tuple` of registered static strings for `__func__`, `__FILE__` and event name.
     * @param line Line number in the file where the profile zone was started (usually `__LINE__`).
     */
    ProfileZoneStatic(const Channel& channel, std::tuple<StaticStringType, StaticStringType, StaticStringType> tup, int line)
        : m_mask(channel.getMask())
    {
        if (g_carbProfiler && channel.isEnabled())
            m_zoneId = g_carbProfiler->beginStatic(m_mask, std::get<0>(tup), std::get<1>(tup), line, std::get<2>(tup));
        else
            m_zoneId = kNoZoneId;
    }

    /**
     * Destructor.
     */
    ~ProfileZoneStatic()
    {
        if (g_carbProfiler && m_zoneId != kNoZoneId)
            g_carbProfiler->endEx(m_mask, m_zoneId);
    }
};

//! @copydoc ProfileZoneStatic
class ProfileZoneDynamic final
{
    const uint64_t m_mask;
    ZoneId m_zoneId;

public:
    /**
     * Constructor.
     *
     * @param mask Profiling bitmask.
     * @param tup A `std::tuple` of registered static strings for `__func__` and `__FILE__`.
     * @param line Line number in the file where the profile zone was started (usually `__LINE__`).
     * @param nameFmt Profile zone name with printf-style formatting followed by arguments
     * @param args Printf-style arguments used with @p nameFmt.
     */
    template <typename... Args>
    ProfileZoneDynamic(const uint64_t mask,
                       std::tuple<StaticStringType, StaticStringType> tup,
                       int line,
                       const char* nameFmt,
                       Args&&... args)
        : m_mask(mask)
    {
        if (g_carbProfiler && ((mask ? mask : kCaptureMaskDefault) & g_carbProfilerMask.load(std::memory_order_acquire)))
            m_zoneId = g_carbProfiler->beginDynamic(
                m_mask, std::get<0>(tup), std::get<1>(tup), line, nameFmt, std::forward<Args>(args)...);
        else
            m_zoneId = kNoZoneId;
    }

    /**
     * Constructor.
     *
     * @param channel A profiling channel.
     * @param tup A `std::tuple` of registered static strings for `__func__` and `__FILE__`.
     * @param line Line number in the file where the profile zone was started (usually `__LINE__`).
     * @param nameFmt Profile zone name with printf-style formatting followed by arguments
     * @param args Printf-style arguments used with @p nameFmt.
     */
    template <typename... Args>
    ProfileZoneDynamic(const Channel& channel,
                       std::tuple<StaticStringType, StaticStringType> tup,
                       int line,
                       const char* nameFmt,
                       Args&&... args)
        : m_mask(channel.getMask())
    {
        if (g_carbProfiler && channel.isEnabled())
            m_zoneId = g_carbProfiler->beginDynamic(
                m_mask, std::get<0>(tup), std::get<1>(tup), line, nameFmt, std::forward<Args>(args)...);
        else
            m_zoneId = kNoZoneId;
    }

    /**
     * Destructor.
     */
    ~ProfileZoneDynamic()
    {
        if (g_carbProfiler && m_zoneId != kNoZoneId)
            g_carbProfiler->endEx(m_mask, m_zoneId);
    }
};

} // namespace profiler
} // namespace carb
