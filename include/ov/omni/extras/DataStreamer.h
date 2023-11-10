// Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../log/ILog.h"
#include "../../carb/events/IEvents.h"
#include "../../carb/events/EventsUtils.h"
#include "../../carb/tasking/ITasking.h"
#include "../../carb/tasking/TaskingUtils.h"
#include "../../carb/RString.h"

namespace omni
{
namespace extras
{

/** An ID that identifies a data type. */
using DataStreamType = int64_t;

/** The event type when an event contains data.
 *  If you need to send other events through the stream, you can use another ID.
 */
constexpr carb::events::EventType kEventTypeData = 0;

/** Generate a unique ID for a specific data type.
 *  @tparam T The type to generate an ID for.
 *  @returns A unique ID for T.
 */
template <typename T>
DataStreamType getDataStreamType() noexcept
{
#if CARB_COMPILER_MSC
    return carb::fnv1aHash(__FUNCSIG__);
#elif CARB_COMPILER_GNUC
    return carb::fnv1aHash(__PRETTY_FUNCTION__);
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

/** This allows a series of ordered packets of raw binary data to be sent
 *  through @ref carb::events::IEvents.
 *
 *  To use this class, the data producer will call pushData() whenever it has
 *  a packet of data, then it will either call pump() or pumpAsync() to send
 *  the data to the listeners.
 *
 *  It's also possible to grab the underlying event stream and send other events
 *  through for the listener to consume.
 *
 *  Each listener should call getEventStream() and use it to construct a
 *  @ref DataListener implementation.
 */
class DataStreamer
{
public:
    DataStreamer() noexcept
    {
        m_events = carb::getFramework()->tryAcquireInterface<carb::events::IEvents>();
        if (m_events == nullptr)
        {
            OMNI_LOG_ERROR("unable to acquire IEvents");
            return;
        }

        m_tasking = carb::getFramework()->tryAcquireInterface<carb::tasking::ITasking>();
        if (m_tasking == nullptr)
        {
            OMNI_LOG_ERROR("unable to acquire ITasking");
            return;
        }

        m_eventStream = m_events->createEventStream();
        if (!m_eventStream)
        {
            OMNI_LOG_ERROR("unable to create an event stream");
            return;
        }

        m_initialized = true;
    }

    ~DataStreamer() noexcept
    {
        flush();
    }

    /** Synchronously submit a packet of data to all listeners.
     *
     *  @remarks This will call the onDataReceived() function for all listeners
     *           on the current thread.
     *           The onDataReceived() calls are serialized, so this can be
     *           called concurrently with pumpAsync() or other calls to pump().
     */
    void pump() noexcept
    {
        if (!m_initialized)
        {
            return;
        }

        m_throttler.acquire();
        m_eventStream->pump();
        m_throttler.release();
    }

    /** Asynchronously submit a packet of data to all listeners.
     *
     *  @remarks This will spawn a task which calls the onDataReceived()
     *           function for all listeners.
     *
     *  @note To verify that all tasks have finished, you must call flush().
     *        This is automatically done when deleting the class instance.
     */
    void pumpAsync(carb::tasking::Priority priority = carb::tasking::Priority::eLow) noexcept
    {
        if (!m_initialized)
        {
            return;
        }

        m_tasking->addThrottledTask(m_throttler, priority, m_tasks, [this] { m_eventStream->pump(); });
    }

    /** Push a new packet of data into the stream.
     *
     *  @tparam T The type pointed to by @p data.
     *            This type will be copied around, so it must be trivially copyable.
     *  @param[in] data The data packet to submit to the stream.
     *                  The data in this pointer is copied, so the pointer is
     *                  is safe to be invalidated after this call returns.
     *  @param[in] size The length of @p data in elements.
     *
     *  @remarks After a packet of data is placed in the stream, it will just be
     *           queued up.
     *           For each call to pushData(), there should be a corresponding
     *           call to pump() or pumpAsync() to dequeue that packet and send
     *           it to the listeners.
     */
    template <typename T>
    void pushData(const T* data, size_t size) noexcept
    {
        static_assert(std::is_trivially_copyable<T>::value, "non-trivially-copyable types will not work here");
        if (!m_initialized)
        {
            return;
        }

        m_eventStream->push(
            kEventTypeData,
            std::make_pair("data", carb::cpp17::string_view(reinterpret_cast<const char*>(data), size * sizeof(T))),
            std::make_pair("type", getDataStreamType<T>()));
    }

    /** Retrieve the event stream used by the data streamer.
     *  @returns The event stream used by the data streamer.
     *
     *  @remarks This event stream can be subscribed to, but you can also send
     *           further events on this stream as long as their type is not
     *           @ref kEventTypeData.
     */
    carb::events::IEventStreamPtr getEventStream() noexcept
    {
        return m_eventStream;
    }

    /** Check if the class actually initialized successfully.
     *  @returns whether the class actually initialized successfully.
     */
    bool isWorking() noexcept
    {
        return m_initialized;
    }

    /** Wait for all asynchronous tasks created by pumpAsync() to finish. */
    void flush() noexcept
    {
        CARB_LOG_INFO("waiting for all tasks to finish");
        m_tasks.wait();
        CARB_LOG_INFO("all tasks have finished");
    }

private:
    /** The events interface. */
    carb::events::IEvents* m_events;

    /** The tasking interface, which we need for pumpAsync(). */
    carb::tasking::ITasking* m_tasking;

    /** The event stream we're using to queue and send data. */
    carb::events::IEventStreamPtr m_eventStream;

    /** A group to hold all of our tasks so we can shut down safely. */
    carb::tasking::TaskGroup m_tasks;

    /** This is used to serialize calls to @ref m_eventStream->pump(). */
    carb::tasking::SemaphoreWrapper m_throttler{ 1 };

    /** Flag to specify whether the class initialized properly. */
    bool m_initialized = false;
};

/** This is an abstract class that allows data from a @ref DataStreamer to be
 *  received in an easy way.
 *
 *  The listener implementation just needs to add an onDataReceived() call to
 *  receive the raw binary data and an onEventReceived() call to handle any
 *  other events that may be sent on the stream.
 */
class DataListener
{
public:
    /** Constructor.
     *  @param[in] stream The event stream from a @ref DataStreamer to listen to.
     */
    DataListener(carb::events::IEventStreamPtr stream) noexcept
    {
        m_dict = carb::getFramework()->tryAcquireInterface<carb::dictionary::IDictionary>();
        if (m_dict == nullptr)
        {
            OMNI_LOG_ERROR("failed to acquire IDictionary");
            return;
        }

        m_sub = carb::events::createSubscriptionToPop(stream.get(),
                                                      [this](const carb::events::IEvent* e) {
                                                          if (e->type != kEventTypeData)
                                                          {
                                                              onEventReceived(e);
                                                              return;
                                                          }
                                                          const carb::dictionary::Item* child;
                                                          child = m_dict->getItem(e->payload, "type");
                                                          if (child == nullptr)
                                                          {
                                                              OMNI_LOG_ERROR("the event had no /type field?");
                                                              return;
                                                          }
                                                          DataStreamType type = m_dict->getAsInt64(child);
                                                          child = m_dict->getItem(e->payload, "data");
                                                          if (child == nullptr)
                                                          {
                                                              OMNI_LOG_ERROR("the event had no /data field?");
                                                              return;
                                                          }
                                                          size_t len;
                                                          const char* bytes = m_dict->getStringBuffer(child, &len);
                                                          onDataReceived(bytes, len, type);
                                                      },
                                                      0, "DataListener");
    }

    virtual ~DataListener() noexcept
    {
        // in case disconnect() has additional functionality in the future
        disconnect();
    }

protected:
    /** The function that will receive data packets.
     *  @param[in] payload The packet of data that was received.
     *                     This pointer will be invalid after this call returns,
     *                     so it must not be held.
     *                     Due to the nature of @ref carb::events::IEvents, there
     *                     is no guarantee that the alignment of this data will
     *                     be correct, so you should memcpy it into a separate
     *                     buffer first to be safe.
     *  @param[in] bytes   The length of @p payload in bytes.
     *  @param[in] type    The data type ID of the data contained in @p payload.
     */
    virtual void onDataReceived(const void* payload, size_t bytes, DataStreamType type) noexcept = 0;


    /** The function that will receive non-data events from the stream.
     *  @param[in] e The event that was received.
     */
    virtual void onEventReceived(const carb::events::IEvent* e) noexcept = 0;

    /** Disconnect this listener from the event stream.
     *  @remarks This might be useful if your information wants to do something
     *           during shutdown that would crash if an event was received
     *           concurrently.
     */
    void disconnect() noexcept
    {
        m_sub = nullptr;
    }

    /** The dictionary interface used to read the event payloads. */
    carb::dictionary::IDictionary* m_dict = nullptr;

private:
    /** The event subscription used for this stream. */
    carb::events::ISubscriptionPtr m_sub;
};

} // namespace extras
} // namespace omni
