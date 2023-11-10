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
//! @brief carb.events interface definition file.
#pragma once

#include "../IObject.h"
#include "../Interface.h"
#include "../InterfaceUtils.h"
#include "../dictionary/IDictionary.h"

#include <utility>

namespace carb
{

/**
 * Namespace for the *carb.events* plugin.
 */
namespace events
{

/**
 * Event type is 64-bit number. To use strings as event types hash function can be used. @see CARB_EVENTS_TYPE_FROM_STR
 */
using EventType = uint64_t;

/**
 * Event sender identifier. It is useful in some cases to differentiate who send an event. E.g. to know that is was you.
 */
using SenderId = uint32_t;

/**
 * Default sender id to use if you don't want it to be unique
 */
static constexpr SenderId kGlobalSenderId = 0;


/**
 * Event notification order.
 */
using Order = int32_t;

/**
 * Default order. If 2 subscriptions are done with the same Order value, their callback order is undefined.
 */
static constexpr Order kDefaultOrder = 0;


/**
 * Event object which is sent and received. Lifetime is managed by carb::IObject refcounting.
 */
class IEvent : public IObject
{
public:
    EventType type; //!< Event type.
    SenderId sender; //!< The sender of the event, or carb::events::kGlobalSenderId.
    dictionary::Item* payload; //!< Event payload is dictionary Item. Any data can be put into.

    /**
     * Helper function to build a *carb.dictionary* of values.
     * @param param The key/value pair to include in the *carb.dictionary*.
     */
    template <typename ValueT>
    void setValues(const std::pair<const char*, ValueT>& param);

    /**
     * Helper function to build a *carb.dictionary* of values.
     * @param param The key/value pair to include in the *carb.dictionary*.
     * @param params Additional key/value pairs.
     */
    template <typename ValueT, typename... ValuesT>
    void setValues(const std::pair<const char*, ValueT>& param, ValuesT&&... params);

    /**
     * Stops propagation of an event during dispatch.
     */
    virtual void consume() = 0;
};

/// Helper definition for IEvent smart pointer.
using IEventPtr = ObjectPtr<IEvent>;


/**
 * Interface to implement for event listener.
 */
class IEventListener : public IObject
{
public:
    /**
     * Function to be implemented that will handle an event.
     * @param e The event to process. To stop further propagation of the event, call IEvent::consume().
     */
    virtual void onEvent(IEvent* e) = 0;
};

/// Helper definition for IEventListener smart pointer.
using IEventListenerPtr = ObjectPtr<IEventListener>;


/**
 * Subscription holder is created by all event listening subscription functions. Subscription is valid while the holder
 * is alive.
 */
class ISubscription : public IObject
{
public:
    /**
     * Unsubscribes the associated IEventListener.
     */
    virtual void unsubscribe() = 0;

    /**
     * Returns the name of the subscription.
     *
     * @note If a name was not given when the @ref IEventStream subscription function was called, a name is generated
     * based on the address of the @ref IEventListener::onEvent() function.
     *
     * @returns The name of the subscription.
     */
    virtual const char* getName() const noexcept = 0;
};

/// Helper definition for ISubscription smart pointer.
using ISubscriptionPtr = ObjectPtr<ISubscription>;


/**
 * Compile-time conversion of string to carb::events::EventType.
 * @param STR The string to convert.
 * @returns The EventType corresponding to the given string.
 */
#define CARB_EVENTS_TYPE_FROM_STR(STR) CARB_HASH_STRING(STR)

/**
 * Run-time conversion of string to carb::events::EventType.
 * @param str The string to convert.
 * @returns The EventType corresponding to the given string.
 */
inline EventType typeFromString(const char* str)
{
    return carb::hashString(str);
}

/**
 * Event stream is fundamental primitive used to send, receive and listen for events. Different event system models can
 * be designed using it. It is thread-safe and can also be used as a sync primitive. Similar to Go Language Channels.
 *
 * Basically stream is just a queue with listeners:
 *
 * @code{.txt}
 *                                 +------------------+
 *       push() / pushBlocked()    |                  |   tryPop() / pop()
 *    +--------------------------->+   IEventStream   +------------------------>
 *                            ^    |                  |    ^
 *                            |    +------------------+    |
 *          subscribeToPush() |                            |  subscribeToPop()
 *                            |                            |
 *                            |                            |
 *                  +---------+--------+          +--------+---------+
 *                  |  IEventListener  |          |  IEventListener  |
 *                  +------------------+          +------------------+
 * @endcode
 *
 * 1. You can push to stream and pop events (acts like a queue).
 * 2. Given the stream you can listen for pushed and/or popped events. That is basically immediate callbacks vs deferred
 *      callbacks.
 * 3. Blocking methods (pushBlocked() and pop()) will block the thread until event is popped (for push method) or any
 *      event is pushed (for pop method). That can be used as thread sync.
 * 4. Pumping of stream is just popping all events until it is empty.
 * 5. EventType allows you to filter which events to listen for. IEventStream may contain only one type, conceptually
 *      more like channels, or a lot of events - like event bus.
 * 6. Listeners are subscribed specifying the order in which they want to receive events. When processing, they may
 *      consume an event which stops other listeners from receiving it (only when being dispatched).
 */
class IEventStream : public IObject
{
public:
    /**
     * Create new event of certain type.
     * @param eventType The type of event to create.
     * @param sender The sender of the event, or carb::events::kGlobalSenderId.
     * @returns A pointer to a newly created event.
     */
    ObjectPtr<IEvent> createEvent(EventType eventType, SenderId sender);

    /// @copydoc createEvent()
    virtual IEvent* createEventPtr(EventType eventType, SenderId sender) = 0;

    /**
     * Dispatch event immediately without putting it into stream.
     * @param e The event to dispatch.
     */
    virtual void dispatch(IEvent* e) = 0;

    /**
     * Push event into the stream.
     * @param e The event to push.
     */
    virtual void push(IEvent* e) = 0;

    /**
     * Push event into the stream and block until it is dispatched by some other thread.
     * @param e The event to push.
     */
    virtual void pushBlocked(IEvent* e) = 0;

    /**
     * Get approximate number of events waiting for dispatch in the stream.
     * @thread_safety While safe to call from multiple threads, since there is no lock involved this value should be
     *      considered approximate as another thread could modify the number before the return value is read. This call
     *      is strongly ordered with respect to other calls.
     * @returns The approximate number of events waiting for dispatch in the stream.
     */
    virtual size_t getCount() = 0;

    /**
     * Pop and dispatches a single event from the stream, blocking until one becomes available.
     * @warning This function blocks until an event is available.
     * @returns The event that was popped and dispatched.
     */
    ObjectPtr<IEvent> pop();

    /// @copydoc pop()
    virtual IEvent* popPtr() = 0;

    /**
     * Pops and dispatches a single event from the stream, if available.
     * @note Unlike pop(), this function does not wait for an event to become available.
     * @returns The event that was popped and dispatched, or `nullptr` if no event was available.
     */
    ObjectPtr<IEvent> tryPop();

    /// @copydoc tryPop()
    virtual IEvent* tryPopPtr() = 0;

    /**
     * Dispatches and pops all available events from the stream.
     */
    void pump();

    /**
     * Subscribe to receive notification when event stream is popped.
     * @sa pump() pop() tryPop()
     * @note Adding or removing a subscription from \ref IEventListener::onEvent() is perfectly valid. The newly added
     *   subscription will not be called until the next event.
     * @warning Recursively pushing and/or popping events from within \ref IEventListener::onEvent() is not recommended
     *   and can lead to undefined behavior.
     * @param listener The listener to subscribe. IEventListener::onEvent() will be called for each popped event. If an
     *   event listener calls IEvent::consume() on the given event, propagation of the event will stop and no more event
     *   listeners will receive the event.
     * @param order An optional value used to specify the order tier. Lower order tiers will be notified first.
     *   Multiple IEventListener objects at the same order tier are notified in an undefined order.
     * @param subscriptionName An optional name for the subscription. Names do not need to be unique.
     * @returns A ISubscription pointer. The subscription is valid as long as the ISubscription pointer is referenced.
     *   Alternately, the IEventListener can be unsubscribed by calling ISubscription::unsubscribe().
     */
    ObjectPtr<ISubscription> createSubscriptionToPop(IEventListener* listener,
                                                     Order order = kDefaultOrder,
                                                     const char* subscriptionName = nullptr);
    /// @copydoc createSubscriptionToPop()
    /// @param eventType A specific event to listen for.
    ObjectPtr<ISubscription> createSubscriptionToPopByType(EventType eventType,
                                                           IEventListener* listener,
                                                           Order order = kDefaultOrder,
                                                           const char* subscriptionName = nullptr);
    /// @copydoc createSubscriptionToPop()
    virtual ISubscription* createSubscriptionToPopPtr(IEventListener* listener,
                                                      Order order = kDefaultOrder,
                                                      const char* subscriptionName = nullptr) = 0;
    /// @copydoc createSubscriptionToPop()
    /// @param eventType A specific event to listen for.
    virtual ISubscription* createSubscriptionToPopByTypePtr(EventType eventType,
                                                            IEventListener* listener,
                                                            Order order = kDefaultOrder,
                                                            const char* subscriptionName = nullptr) = 0;

    /**
     * Subscribe to receive notification when an event is pushed into the event stream.
     * @sa push() pushBlocked()
     * @note Adding or removing a subscription from \ref IEventListener::onEvent() is perfectly valid. The newly added
     *   subscription will not be called until the next event.
     * @warning Recursively pushing and/or popping events from within \ref IEventListener::onEvent() is not recommended
     *   and can lead to undefined behavior.
     * @param listener The listener to subscribe. IEventListener::onEvent() will be called for each pushed event. The
     *   IEvent::consume() function has no effect for notifications of push events.
     * @param order An optional value used to specify the order tier. Lower order tiers will be notified first.
     *   Multiple IEventListener objects at the same order tier are notified in an undefined order.
     * @param subscriptionName An optional name for the subscription. Names do not need to be unique.
     * @returns A ISubscription pointer. The subscription is valid as long as the ISubscription pointer is referenced.
     *   Alternately, the IEventListener can be unsubscribed by calling ISubscription::unsubscribe().
     */
    ObjectPtr<ISubscription> createSubscriptionToPush(IEventListener* listener,
                                                      Order order = kDefaultOrder,
                                                      const char* subscriptionName = nullptr);
    /// @copydoc createSubscriptionToPush()
    /// @param eventType A specific event to listen for.
    ObjectPtr<ISubscription> createSubscriptionToPushByType(EventType eventType,
                                                            IEventListener* listener,
                                                            Order order = kDefaultOrder,
                                                            const char* subscriptionName = nullptr);
    /// @copydoc createSubscriptionToPush()
    virtual ISubscription* createSubscriptionToPushPtr(IEventListener* listener,
                                                       Order order = kDefaultOrder,
                                                       const char* subscriptionName = nullptr) = 0;
    /// @copydoc createSubscriptionToPush()
    /// @param eventType A specific event to listen for.
    virtual ISubscription* createSubscriptionToPushByTypePtr(EventType eventType,
                                                             IEventListener* listener,
                                                             Order order = kDefaultOrder,
                                                             const char* subscriptionName = nullptr) = 0;

    /**
     * Sets the notification order for named subscriptions.
     * @note If multiple subscriptions exist with the same name, all are updated.
     * @param subscriptionName the name previously assigned when the subscription was created.
     * @param order The new order tier. Lower order tiers will be notified first. Multiple IEventListener objects at the
     *   same order tier are notified in an undefined order.
     * @returns `true` if the subscription was found and updated; `false` otherwise.
     */
    virtual bool setSubscriptionToPopOrder(const char* subscriptionName, Order order) = 0;
    /// @copydoc setSubscriptionToPopOrder
    virtual bool setSubscriptionToPushOrder(const char* subscriptionName, Order order) = 0;

    /**
     * Retrieves the notification order for a named subscription.
     * @param subscriptionName the name previously assigned when the subscription was created.
     * @param order Must be a valid pointer that will receive the current order tier.
     * @returns `true` if the subscription was found; `false` if the subscription was not found or @p order is
     *   `nullptr`.
     */
    virtual bool getSubscriptionToPopOrder(const char* subscriptionName, Order* order) = 0;
    /// @copydoc setSubscriptionToPopOrder
    virtual bool getSubscriptionToPushOrder(const char* subscriptionName, Order* order) = 0;

    /**
     * Returns the name of the IEventStream.
     *
     * @note If a name was not given when @ref IEvents::createEventStream() was called, a name is generated based on the
     * function that called @ref IEvents::createEventStream().
     *
     * @returns The name of the IEventStream.
     */
    virtual const char* getName() const noexcept = 0;

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                                    Helpers                                                     //
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * @defgroup helpers Helper functions
     * @sa createEvent() push() pushBlocked() dispatch()
     * @{
     */

    /**
     * @copydoc createEvent()
     * @param values Key/value pairs as passed to IEvent::setValues().
     */
    template <typename... ValuesT>
    IEvent* createEventPtr(EventType eventType, SenderId sender, ValuesT&&... values);

    /**
     * Helper function that combines createEvent() with push().
     * @param eventType The type of event to create.
     * @param sender The sender of the event, or carb::events::kGlobalSenderId.
     */
    void pushWithSender(EventType eventType, SenderId sender);

    /// @copydoc pushWithSender
    /// @param values Key/value pairs as passed to IEvent::setValues().
    template <typename... ValuesT>
    void pushWithSender(EventType eventType, SenderId sender, ValuesT&&... values);

    /**
     * Helper function that combines createEvent() with push().
     * @param eventType The type of event to create.
     * @param values Key/value pairs as passed to IEvent::setValues().
     */
    template <typename... ValuesT>
    void push(EventType eventType, ValuesT&&... values);

    /**
     * Helper function that combines createEvent() with pushBlocked().
     * @param eventType The type of event to create.
     * @param sender The sender of the event, or carb::events::kGlobalSenderId.
     */
    void pushBlockedWithSender(EventType eventType, SenderId sender);

    /// @copydoc pushBlockedWithSender
    /// @param values Key/value pairs as passed to IEvent::setValues().
    template <typename... ValuesT>
    void pushBlockedWithSender(EventType eventType, SenderId sender, ValuesT&&... values);

    /**
     * Helper function that combines createEvent() with pushBlocked().
     * @param eventType The type of event to create.
     * @param values Key/value pairs as passed to IEvent::setValues().
     */
    template <typename... ValuesT>
    void pushBlocked(EventType eventType, ValuesT&&... values);

    /**
     * Helper function that combines createEvent() with dispatch().
     * @param eventType The type of event to create.
     * @param sender The sender of the event, or carb::events::kGlobalSenderId.
     * @param values Key/value pairs as passed to IEvent::setValues().
     */
    template <typename... ValuesT>
    void dispatch(EventType eventType, SenderId sender, ValuesT&&... values);

    /**
     * Helper function that combines createEvent() with dispatch().
     * @param eventType The type of event to create.
     * @param sender The sender of the event, or carb::events::kGlobalSenderId.
     */
    void dispatch(EventType eventType, SenderId sender);
    ///@}
};

/// Helper definition for IEventStream smart pointer.
using IEventStreamPtr = ObjectPtr<IEventStream>;

/**
 * Interface definition for *carb.events*.
 */
struct IEvents
{
    // 1.0 - Initial release
    // 1.1 - added internalCreateEventStream
    CARB_PLUGIN_INTERFACE("carb::events::IEvents", 1, 1)

    /**
     * Create new event stream.
     * @param name An optional name to give the event stream for logging and profiling.
     * @returns A pointer to the new event stream.
     */
    IEventStreamPtr createEventStream(const char* name = nullptr) noexcept;

    /**
     * Create new event stream.
     * @rst
     * .. deprecated:: ``carb::events::IEvents::internalCreateEventStream`` instead.
     * @endrst
     * @returns A pointer to the new event stream.
     */
    CARB_DEPRECATED("Use internalCreateEventStream instead") virtual IEventStream* createEventStreamPtr() = 0;

    /**
     * Get a unique sender identifier.
     * @note The sender identifier may be a previously released sender identifier.
     * @returns A unique sender identifier. When finished with the sender identifier, it should be returned with
     *   releaseUniqueSenderId().
     */
    virtual SenderId acquireUniqueSenderId() = 0;

    /**
     * Releases a unique sender identifier previously acquired with acquireUniqueSenderId().
     * @param senderId The previously acquired senderId.
     */
    virtual void releaseUniqueSenderId(SenderId senderId) = 0;

    //! @private
    virtual IEventStream* internalCreateEventStream(const char* name) noexcept = 0;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                Inline Functions                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


////////////// IEvent //////////////

template <typename ValueT>
inline void IEvent::setValues(const std::pair<const char*, ValueT>& param)
{
    carb::getCachedInterface<dictionary::IDictionary>()->makeAtPath<ValueT>(this->payload, param.first, param.second);
}

template <typename ValueT, typename... ValuesT>
inline void IEvent::setValues(const std::pair<const char*, ValueT>& param, ValuesT&&... params)
{
    this->setValues<ValueT>(param);
    this->setValues(std::forward<ValuesT>(params)...);
}


////////////// IEventStream //////////////

template <typename... ValuesT>
inline IEvent* IEventStream::createEventPtr(EventType type, SenderId sender, ValuesT&&... values)
{
    IEvent* e = createEventPtr(type, sender);
    e->setValues(std::forward<ValuesT>(values)...);
    return e;
}

inline ObjectPtr<ISubscription> IEventStream::createSubscriptionToPop(IEventListener* listener,
                                                                      Order order,
                                                                      const char* name)
{
    return stealObject(this->createSubscriptionToPopPtr(listener, order, name));
}

inline ObjectPtr<ISubscription> IEventStream::createSubscriptionToPopByType(EventType eventType,
                                                                            IEventListener* listener,
                                                                            Order order,
                                                                            const char* name)
{
    return stealObject(this->createSubscriptionToPopByTypePtr(eventType, listener, order, name));
}

inline ObjectPtr<ISubscription> IEventStream::createSubscriptionToPush(IEventListener* listener,
                                                                       Order order,
                                                                       const char* name)
{
    return stealObject(this->createSubscriptionToPushPtr(listener, order, name));
}

inline ObjectPtr<ISubscription> IEventStream::createSubscriptionToPushByType(EventType eventType,
                                                                             IEventListener* listener,
                                                                             Order order,
                                                                             const char* name)
{
    return stealObject(this->createSubscriptionToPushByTypePtr(eventType, listener, order, name));
}

template <typename... ValuesT>
inline void IEventStream::pushWithSender(EventType type, SenderId sender, ValuesT&&... values)
{
    IEvent* e = createEventPtr(type, sender, std::forward<ValuesT>(values)...);
    push(e);
    e->release();
}

inline void IEventStream::pushWithSender(EventType type, SenderId sender)
{
    IEvent* e = createEventPtr(type, sender);
    push(e);
    e->release();
}

inline ObjectPtr<IEvent> IEventStream::createEvent(EventType eventType, SenderId sender)
{
    return carb::stealObject(this->createEventPtr(eventType, sender));
}

template <typename... ValuesT>
inline void IEventStream::push(EventType type, ValuesT&&... values)
{
    return pushWithSender(type, kGlobalSenderId, std::forward<ValuesT>(values)...);
}

template <typename... ValuesT>
inline void IEventStream::pushBlockedWithSender(EventType type, SenderId sender, ValuesT&&... values)
{
    IEvent* e = createEventPtr(type, sender, std::forward<ValuesT>(values)...);
    pushBlocked(e);
    e->release();
}

inline void IEventStream::pushBlockedWithSender(EventType type, SenderId sender)
{
    IEvent* e = createEventPtr(type, sender);
    pushBlocked(e);
    e->release();
}

template <typename... ValuesT>
inline void IEventStream::pushBlocked(EventType type, ValuesT&&... values)
{
    return pushBlockedWithSender(type, kGlobalSenderId, std::forward<ValuesT>(values)...);
}

template <typename... ValuesT>
inline void IEventStream::dispatch(EventType type, SenderId sender, ValuesT&&... values)
{
    IEvent* e = createEventPtr(type, sender, std::forward<ValuesT>(values)...);
    dispatch(e);
    e->release();
}

inline void IEventStream::dispatch(EventType type, SenderId sender)
{
    IEvent* e = createEventPtr(type, sender);
    dispatch(e);
    e->release();
}

inline void IEventStream::pump()
{
    const size_t eventCount = this->getCount();
    for (size_t i = 0; i < eventCount; i++)
    {
        IEvent* e = this->tryPopPtr();
        if (!e)
            break;
        e->release();
    }
}

inline ObjectPtr<IEvent> IEventStream::pop()
{
    return carb::stealObject(this->popPtr());
}

inline ObjectPtr<IEvent> IEventStream::tryPop()
{
    return carb::stealObject(this->tryPopPtr());
}

inline ObjectPtr<IEventStream> IEvents::createEventStream(const char* name) noexcept
{
    return carb::stealObject(this->internalCreateEventStream(name));
}

} // namespace events
} // namespace carb
