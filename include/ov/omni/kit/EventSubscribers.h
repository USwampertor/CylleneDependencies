// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include <carb/Defines.h>

#include <vector>

namespace carb
{
template <class FuncT, typename SubIdT>
class EventSubscribers
{
public:
    SubIdT subscribe(FuncT fn, void* userData)
    {
        // Search for free slot
        size_t index;
        bool found = false;
        for (size_t i = 0; i < m_subscribers.size(); i++)
        {
            if (!m_subscribers[i].fn)
            {
                index = i;
                found = true;
                break;
            }
        }

        // Add new slot if haven't found a free one
        if (!found)
        {
            m_subscribers.push_back({});
            index = m_subscribers.size() - 1;
        }

        m_subscribers[index] = { fn, userData };
        return (SubIdT)index;
    }

    void unsubscribe(SubIdT id)
    {
        CARB_ASSERT(id < m_subscribers.size());
        m_subscribers[id] = {};
    }

    template <typename... Ts>
    void send(Ts... args)
    {
        // Iterate by index because subscribers can actually change during iteration (vector can grow)
        const size_t kCount = m_subscribers.size();
        for (size_t i = 0; i < kCount; i++)
        {
            auto& subsribers = m_subscribers[i];
            if (subsribers.fn)
                subsribers.fn(args..., subsribers.userData);
        }
    }

    template <typename... Ts>
    void sendForId(uint64_t id, Ts... args)
    {
        CARB_ASSERT(id < m_subscribers.size());
        if (m_subscribers[id].fn)
            m_subscribers[id].fn(args..., m_subscribers[id].userData);
    }

private:
    struct EventData
    {
        FuncT fn;
        void* userData;
    };
    std::vector<EventData> m_subscribers;
};
} // namespace carb
