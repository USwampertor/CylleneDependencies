// Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Interface definition for *carb.eventdispatcher.plugin*
#pragma once

#include "../Interface.h"
#include "EventDispatcherTypes.h"

namespace carb
{

//! Namespace for *carb.eventdispatcher.plugin* and related utilities.
namespace eventdispatcher
{

class ObserverGuard;

//! Interface for *carb.eventdispatcher.plugin*.
struct IEventDispatcher
{
    CARB_PLUGIN_INTERFACE("carb::eventdispatcher::IEventDispatcher", 0, 1)

    //! @private (see observeEvent())
    Observer(CARB_ABI* internalObserveEvent)(int order,
                                             RString eventName,
                                             size_t numVariants,
                                             NamedVariant const* variants,
                                             ObserverFn fn,
                                             CleanupFn cleanup,
                                             void* ud);

    /**
     * Stops the given observer. Safe to perform while dispatching.
     *
     * Since observers can be in use by this thread or any thread, this function is carefully synchronized with all
     * other IEventDispatcher operations.
     *  - During stopObserving(), further calls to the observer are prevented, even if other threads are currently
     *    dispatching an event that would be observed by the observer in question.
     *  - If any other thread is currently calling the observer in question, stopObserving() will wait until all other
     *    threads have left the observer function.
     *  - If the observer function is \b not in the callstack of the current thread, the cleanup function provided to
     *    \c internalObserveEvent() is called and any \ref variant::Variant objects captured to filter events are
     *    destroyed.
     *  - If the observer function is in the callstack of the current thread, stopObserving() will return without
     *    waiting, calling the cleanup function or destroying \ref variant::Variant objects. Instead, this cleanup will
     *    be performed when the \ref dispatchEvent() call in the current thread finishes.
     *
     * When stopObserving() returns, it is guaranteed that the observer function will no longer be called and all calls
     * to it have completed (except if the calling thread is dispatching).
     *
     * @warning This function must be called exactly once per \ref Observer created by \ref observeEvent(). The
     * \ref ObserverGuard calls this function automatically.
     *
     * @thread_safety Safe to perform while any thread is performing any operation against IEventDispatcher.
     *
     * @param ob The \ref Observer to stop.
     * @returns \c true if the Observer was found and stopped; \c false otherwise.
     */
    bool(CARB_ABI* stopObserving)(Observer ob);

    //! @private (see hasObservers())
    bool(CARB_ABI* internalHasObservers)(RString eventName, size_t numVariants, NamedVariant const* variants);

    //! @private (see dispatchEvent())
    size_t(CARB_ABI* internalDispatch)(const EventData&);

    /**
     * Queries to see if the system is currently dispatching an event.
     *
     * @thread_safety Safe to perform while any thread is performing any operation against IEventDispatcher.
     *
     * @param currentThread If \c false, checks to see if any thread is currently dispatching. However, the return value
     * should be used for debugging purposes only as it is a transient value and could be stale by the time it is read
     * by the application. If \c true, checks to see if the current thread is dispatching (that is, the callstack
     * includes a call to \ref dispatchEvent()).
     * @returns \c true if any thread or the current thread is dispatching based on the value of \p currentThread;
     * \c false otherwise.
     */
    bool(CARB_ABI* isDispatching)(bool currentThread);

    /**
     * Registers an observer with the Event Dispatcher system.
     *
     * An observer is an invocable object (function, functor, lambda, etc.) that is called whenever \ref dispatchEvent()
     * is called. The observers are invoked in the thread that calls \ref dispatchEvent(), and multiple threads could be
     * calling \ref dispatchEvent() simultaneously, so observers must be thread-safe unless the application can ensure
     * synchronization around \ref dispatchEvent() calls.
     *
     * Observers can pass zero or any number of @p filterArgs parameters. These @p filterArgs cause an observer to only
     * be invoked for a \ref dispatchEvent() call that contains at least the same values. For instance, having a filter
     * pair for key "WindowID" with a specific value will only cause the observer to be called if \ref dispatchEvent()
     * is given the same value as a "WindowID" parameter.
     *
     * Observers can be added inside of an observer notification (i.e. during a call to \ref dispatchEvent()), however
     * these new observers will <b>not be called</b> for the currently dispatching event. A subsequent recursive call to
     * \ref dispatchEvent() <em>(on the current thread only)</em> will also call the new observer. The new observer will
     * be available to all other threads once the \ref dispatchEvent() call--in which it was added--returns.
     *
     * @thread_safety Safe to perform while any thread is performing any operation against IEventDispatcher.
     *
     * @param order A value determining call order. Observers with lower order values are called earlier. Observers with
     * the same order value and same filter argument values will be called in the order they are registered. Observers
     * with the same order value with \a different filter argument values are called in an indeterminate order.
     * @param eventName The name of the event to observe.
     * @param invocable An object that is invoked when an event matching the \p eventName and \p filterArgs is
     * dispatched. The object must be callable as `void(const Event&)`.
     * @param filterArgs Zero or more arguments that filter observer invocations. Each argument must be of type
     * `std::pair<RStringKey, T>` where the first parameter is the key and the second is the value. The value must be
     * of a type understood by a \ref variant::Translator specialization.
     * @returns An \ref ObserverGuard representing the lifetime of the observer. When the \ref ObserverGuard is reset or
     * destroyed, the observer is unregistered as with \ref stopObserving().
     */
    template <class Invocable, class... Args>
    CARB_NODISCARD ObserverGuard observeEvent(int order, RString eventName, Invocable&& invocable, Args&&... filterArgs);

    /**
     * Registers an observer with the Event Dispatcher system.
     *
     * An observer is an invocable object (function, functor, lambda, etc.) that is called whenever \ref dispatchEvent()
     * is called. The observers are invoked in the thread that calls \ref dispatchEvent(), and multiple threads could be
     * calling \ref dispatchEvent() simultaneously, so observers must be thread-safe unless the application can ensure
     * synchronization around \ref dispatchEvent() calls.
     *
     * Observers can pass zero or any number of @p filterArgs parameters. These @p filterArgs cause an observer to only
     * be invoked for a \ref dispatchEvent() call that contains at least the same values. For instance, having a filter
     * pair for key "WindowID" with a specific value will only cause the observer to be called if \ref dispatchEvent()
     * is given the same value as a "WindowID" parameter.
     *
     * Observers can be added inside of an observer notification (i.e. during a call to \ref dispatchEvent()), however
     * these new observers will <b>not be called</b> for the currently dispatching event. However, a recursive call to
     * \ref dispatchEvent() <em>(on the current thread only)</em> will also call the new observer. The new observer will
     * be available to all other threads once the \ref dispatchEvent() call--in which it was added--returns.
     *
     * @thread_safety Safe to perform while any thread is performing any operation against IEventDispatcher.
     *
     * @param order A value determining call order. Observers with lower order values are called earlier. Observers with
     * the same order value and same filter argument values will be called in the order they are registered. Observers
     * with the same order value with \a different filter argument values are called in an indeterminate order.
     * @param eventName The name of the event to observe.
     * @param invocable An object that is invoked when an event matching the \p eventName and \p filterArgs is
     * dispatched. The object must be callable as `void(const Event&)`.
     * @tparam InIter An InputIterator that is forward-iterable and resolves to a \ref NamedVariant when dereferenced.
     * @param begin An InputIterator representing the start of the filter parameters.
     * @param end A past-the-end InputIterator representing the end of the filter parameters.
     * @returns An \ref ObserverGuard representing the lifetime of the observer. When the \ref ObserverGuard is reset or
     * destroyed, the observer is unregistered as with \ref stopObserving().
     */
    template <class Invocable, class InIter>
    CARB_NODISCARD ObserverGuard
    observeEventIter(int order, RString eventName, Invocable&& invocable, InIter begin, InIter end);

    /**
     * Queries the Event Dispatcher whether any observers are listening to a specific event signature.
     *
     * Emulates a call to \ref dispatchEvent() (without actually calling any observers) and returns \c true if any
     * observers would be called.
     *
     * @thread_safety Safe to perform while any thread is performing any operation against IEventDispatcher.
     * @param eventName The name of the event to query.
     * @param filterArgs Zero or more key/value pairs that would be used for observer filtering as in a call to
     * \ref dispatchEvent(). Each argument must be of type `std::pair<RStringKey, T>` where the first parameter is the
     * key and the second is the value. The value must be of a type understood by a \ref variant::Translator
     * specialization.
     * @returns \c true if at least one observer would be called if the same arguments were passed to
     * \ref dispatchEvent(); \c false otherwise.
     */
    template <class... Args>
    bool hasObservers(RString eventName, Args&&... filterArgs);

    /**
     * Queries the Event Dispatcher whether any observers are listening to a specific event signature.
     *
     * Emulates a call to \ref dispatchEvent() (without actually calling any observers) and returns \c true if any
     * observers would be called.
     *
     * @thread_safety Safe to perform while any thread is performing any operation against IEventDispatcher.
     * @tparam InIter An InputIterator that is forward-iterable and resolves to a \ref NamedVariant when dereferenced.
     * The entries are used for observer filtering.
     * @param eventName The name of the event to query.
     * @param begin An InputIterator representing the start of the event key/value pairs.
     * @param end A past-the-end InputIterator representing the end of the event key/value pairs.
     * @returns \c true if at least one observer would be called if the same arguments were passed to
     * \ref dispatchEvent(); \c false otherwise.
     */
    template <class InIter>
    bool hasObserversIter(RString eventName, InIter begin, InIter end);

    /**
     * Dispatches an event and immediately calls all observers that would observe this particular event.
     *
     * Finds and calls all observers (in the current thread) that observe the given event signature.
     *
     * It is safe to recursively dispatch events (i.e. call dispatchEvent() from a called observer), but care must be
     * taken to avoid endless recursion. See the rules in observeEvent() for observers added during a dispatchEvent()
     * call.
     *
     * @thread_safety Safe to perform while any thread is performing any operation against IEventDispatcher.
     * @param eventName The name of the event to dispatch.
     * @param payload Zero or more key/value pairs that are used as the event payload and may be queried by observers or
     * used to filter observers. Each argument must be of type `std::pair<RStringKey, T>` where the first parameter is
     * the key and the second is the value. The value must be of a type understood by a \ref variant::Translator
     * specialization.
     * @returns The count of observers that were called. Recursive dispatch calls are not included.
     */
    template <class... Args>
    size_t dispatchEvent(RString eventName, Args&&... payload);

    /**
     * Dispatches an event and immediately calls all observers that would observe this particular event.
     *
     * Finds and calls all observers (in the current thread) that observe the given event signature.
     *
     * It is safe to recursively dispatch events (i.e. call dispatchEvent() from a called observer), but care must be
     * taken to avoid endless recursion. See the rules in observeEvent() for observers added during a dispatchEvent()
     * call.
     *
     * @thread_safety Safe to perform while any thread is performing any operation against IEventDispatcher.
     * @tparam InIter An InputIterator that is forward-iterable and resolves to a \ref NamedVariant when dereferenced.
     * The entries are used as the event payload and may be queried by observers or used to filter observers.
     * @param eventName The name of the event to dispatch.
     * @param begin An InputIterator representing the start of the event key/value pairs.
     * @param end A past-the-end InputIterator representing the end of the event key/value pairs.
     * @returns The count of observers that were called. Recursive dispatch calls are not included.
     */
    template <class InIter>
    size_t dispatchEventIter(RString eventName, InIter begin, InIter end);
};

} // namespace eventdispatcher
} // namespace carb

#include "IEventDispatcher.inl"
