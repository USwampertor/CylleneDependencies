// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../BindingsPythonUtils.h"
#include "../Framework.h"
#include "../dictionary/DictionaryBindingsPython.h"
#include "../cpp17/Optional.h"
#include "EventsUtils.h"
#include "IEvents.h"

#include <memory>
#include <string>
#include <vector>

DISABLE_PYBIND11_DYNAMIC_CAST(carb::events::IEvents)
DISABLE_PYBIND11_DYNAMIC_CAST(carb::events::ISubscription)
DISABLE_PYBIND11_DYNAMIC_CAST(carb::events::IEvent)
DISABLE_PYBIND11_DYNAMIC_CAST(carb::events::IEventStream)

namespace carb
{
namespace events
{

struct IEventsHolder
{
    IEvents* events{ nullptr };

    IEventsHolder()
    {
        update();
    }

    ~IEventsHolder()
    {
        if (events)
        {
            g_carbFramework->removeReleaseHook(events, sReleaseHook, this);
        }
    }

    explicit operator bool() const noexcept
    {
        return events != nullptr;
    }

    IEvents* get(bool canUpdate = false)
    {
        if (canUpdate)
        {
            update();
        }
        return events;
    }

private:
    void update()
    {
        if (events)
            return;

        events = g_carbFramework->tryAcquireInterface<IEvents>();
        if (events)
        {
            g_carbFramework->addReleaseHook(events, sReleaseHook, this);
        }
    }

    CARB_PREVENT_COPY_AND_MOVE(IEventsHolder);
    static void sReleaseHook(void* iface, void* user)
    {
        static_cast<IEventsHolder*>(user)->events = nullptr;
        g_carbFramework->removeReleaseHook(iface, sReleaseHook, user);
    }
};

inline IEvents* getIEvents(bool update = false)
{
    static IEventsHolder holder;
    return holder.get(update);
}

// Wrap ObjectPtr because the underlying carb.events module could be unloaded before things destruct. In this case, we
// want to prevent destroying them to avoid crashing.
template <class T>
class ObjectPtrWrapper : public carb::ObjectPtr<T>
{
    using Base = ObjectPtr<T>;

public:
    ObjectPtrWrapper() = default;
    ObjectPtrWrapper(ObjectPtrWrapper&& other) : Base(std::move(other))
    {
    }
    explicit ObjectPtrWrapper(T* o) : Base(o)
    {
        getIEvents(); // Ensure that we've resolved IEvents
    }
    ~ObjectPtrWrapper()
    {
        // If IEvents no longer exists, detach the pointer without releasing it.
        if (!getIEvents())
            this->detach();
    }

    ObjectPtrWrapper& operator=(ObjectPtrWrapper&& other)
    {
        Base::operator=(std::move(other));
        return *this;
    }

    using Base::operator=;
};
} // namespace events
} // namespace carb
PYBIND11_DECLARE_HOLDER_TYPE(T, carb::events::ObjectPtrWrapper<T>, true);

namespace carb
{
namespace events
{

namespace
{

class PythonEventListener : public IEventListener
{
public:
    PythonEventListener(std::function<void(IEvent*)> fn) : m_fn(std::move(fn))
    {
    }

    void onEvent(IEvent* e) override
    {
        carb::callPythonCodeSafe(m_fn, e);
    }

private:
    std::function<void(IEvent*)> m_fn;

    CARB_IOBJECT_IMPL
};

// GIL must be held
std::string getPythonCaller() noexcept
{
    // Equivalent python:
    // def get_python_caller():
    //     try:
    //         from traceback import extract_stack
    //         tb = extract_stack(limit=1)
    //         l = tb.format()
    //         if len(l) >= 1:
    //             s = l[0]
    //             return s[0:s.find('\n')]
    //     except:
    //         pass
    //     return ""

    // Determine a name based on the caller
    try
    {
        // This cannot be static because the GIL won't be held for cleanup at shutdown
        auto extract_stack = py::module::import("traceback").attr("extract_stack");
        auto tb = extract_stack(py::arg("limit") = 1);
        py::list list = tb.attr("format")();
        if (list.size() >= 1)
        {
            std::string entry = py::str(list[0]);
            // sample entry:
            // ```File "C:\src\carbonite\source\tests\python\test_events.py", line 28, in main
            //     stream = events.create_event_stream()
            // ```
            // Return just the first line
            auto index = entry.find('\n');
            return entry.substr(0, index);
        }
    }
    catch (...)
    {
    }
    return {};
}

inline void definePythonModule(py::module& m)
{
    using namespace carb::events;

    m.def("acquire_events_interface", []() { return getIEvents(true); }, py::return_value_policy::reference,
          py::call_guard<py::gil_scoped_release>());

    m.def("type_from_string", [](const char* s) { return carb::events::typeFromString(s); },
          py::call_guard<py::gil_scoped_release>());

    py::class_<ISubscription, ObjectPtrWrapper<ISubscription>>(m, "ISubscription", R"(
        Subscription holder.
        )")
        .def("unsubscribe", &ISubscription::unsubscribe, R"(
            Unsubscribes this subscription.
        )",
             py::call_guard<py::gil_scoped_release>())
        .def_property_readonly("name", &ISubscription::getName, R"(
            Returns the name of this subscription.

            Returns:
                The name of this subscription.
        )",
                               py::call_guard<py::gil_scoped_release>());

    py::class_<IEvent, ObjectPtrWrapper<IEvent>>(m, "IEvent", R"(
        Event.

        Event has an Event type, a sender id and a payload. Payload is a dictionary like item with arbitrary data.
        )")
        .def_readonly("type", &IEvent::type)
        .def_readonly("sender", &IEvent::sender)
        .def_readonly("payload", &IEvent::payload)

        .def("consume", &IEvent::consume, "Consume event to stop it propagating to other listeners down the line.",
             py::call_guard<py::gil_scoped_release>());


    py::class_<IEventStream, ObjectPtrWrapper<IEventStream>>(m, "IEventStream")
        .def("create_subscription_to_pop",
             [](IEventStream* stream, std::function<void(IEvent*)> onEventFn, Order order, const char* name) {
                 std::string sname;
                 if (!name || name[0] == '\0')
                 {
                     sname = getPythonCaller();
                     name = sname.empty() ? nullptr : sname.c_str();
                 }
                 py::gil_scoped_release gil;
                 return stream->createSubscriptionToPop(
                     carb::stealObject(new PythonEventListener(std::move(onEventFn))).get(), order, name);
             },
             R"(
            Subscribes to event dispatching on the stream.

            See :class:`.Subscription` for more information on subscribing mechanism.

            Args:
                fn: The callback to be called on event dispatch.
                order: An integer order specifier. Lower numbers are called first. Negative numbers are allowed. Default is 0.
                name: The name of the subscription for profiling. If no name is provided one is generated from the traceback of the calling function.

            Returns:
                The subscription holder.)",
             py::arg("fn"), py::arg("order") = kDefaultOrder, py::arg("name") = nullptr)

        .def("create_subscription_to_pop_by_type",
             [](IEventStream* stream, EventType eventType, const std::function<void(IEvent*)>& onEventFn, Order order,
                const char* name) {
                 std::string sname;
                 if (!name || name[0] == '\0')
                 {
                     sname = getPythonCaller();
                     name = sname.empty() ? nullptr : sname.c_str();
                 }
                 py::gil_scoped_release gil;
                 return stream->createSubscriptionToPopByType(
                     eventType, carb::stealObject(new PythonEventListener(onEventFn)).get(), order, name);
             },
             R"(
            Subscribes to event dispatching on the stream.

            See :class:`.Subscription` for more information on subscribing mechanism.

            Args:
                event_type: Event type to listen to.
                fn: The callback to be called on event dispatch.
                order: An integer order specifier. Lower numbers are called first. Negative numbers are allowed. Default is 0.
                name: The name of the subscription for profiling. If no name is provided one is generated from the traceback of the calling function.

            Returns:
                The subscription holder.)",
             py::arg("event_type"), py::arg("fn"), py::arg("order") = kDefaultOrder, py::arg("name") = nullptr)

        .def("create_subscription_to_push",
             [](IEventStream* stream, const std::function<void(IEvent*)>& onEventFn, Order order, const char* name) {
                 std::string sname;
                 if (!name || name[0] == '\0')
                 {
                     sname = getPythonCaller();
                     name = sname.empty() ? nullptr : sname.c_str();
                 }
                 py::gil_scoped_release gil;
                 return stream->createSubscriptionToPush(
                     carb::stealObject(new PythonEventListener(onEventFn)).get(), order, name);
             },
             R"(
            Subscribes to pushing events into stream.

            See :class:`.Subscription` for more information on subscribing mechanism.

            Args:
                fn: The callback to be called on event push.
                order: An integer order specifier. Lower numbers are called first. Negative numbers are allowed. Default is 0.
                name: The name of the subscription for profiling. If no name is provided one is generated from the traceback of the calling function.

            Returns:
                The subscription holder.)",
             py::arg("fn"), py::arg("order") = kDefaultOrder, py::arg("name") = nullptr)
        .def("create_subscription_to_push_by_type",
             [](IEventStream* stream, EventType eventType, const std::function<void(IEvent*)>& onEventFn, Order order,
                const char* name) {
                 std::string sname;
                 if (!name || name[0] == '\0')
                 {
                     sname = getPythonCaller();
                     name = sname.empty() ? nullptr : sname.c_str();
                 }
                 py::gil_scoped_release gil;
                 return stream->createSubscriptionToPushByType(
                     eventType, carb::stealObject(new PythonEventListener(onEventFn)).get(), order, name);
             },
             R"(
            Subscribes to pushing events into stream.

            See :class:`.Subscription` for more information on subscribing mechanism.

            Args:
                event_type: Event type to listen to.
                fn: The callback to be called on event push.
                order: An integer order specifier. Lower numbers are called first. Negative numbers are allowed. Default is 0.
                name: The name of the subscription for profiling. If no name is provided one is generated from the traceback of the calling function.

            Returns:
                The subscription holder.)",
             py::arg("event_type"), py::arg("fn"), py::arg("order") = kDefaultOrder, py::arg("name") = nullptr)
        .def_property_readonly("event_count", &IEventStream::getCount, py::call_guard<py::gil_scoped_release>())
        .def("set_subscription_to_pop_order", &IEventStream::setSubscriptionToPopOrder,
             R"(
            Set subscription to pop order by name of subscription.
            )",
             py::arg("name"), py::arg("order"), py::call_guard<py::gil_scoped_release>())
        .def("set_subscription_to_push_order", &IEventStream::setSubscriptionToPushOrder,
             R"(
            Set subscription to push order by name of subscription.
            )",
             py::arg("name"), py::arg("order"), py::call_guard<py::gil_scoped_release>())
        .def("get_subscription_to_pop_order",
             [](IEventStream* self, const char* subscriptionName) -> py::object {
                 Order order;
                 bool b;
                 {
                     py::gil_scoped_release nogil;
                     b = self->getSubscriptionToPopOrder(subscriptionName, &order);
                 }
                 if (b)
                     return py::int_(order);
                 return py::none();
             },
             R"(
            Get subscription to pop order by name of subscription. Return None if subscription was not found.
            )",
             py::arg("name"))
        .def("get_subscription_to_push_order",
             [](IEventStream* self, const char* subscriptionName) -> py::object {
                 Order order;
                 bool b;
                 {
                     py::gil_scoped_release nogil;
                     b = self->getSubscriptionToPushOrder(subscriptionName, &order);
                 }
                 if (b)
                     return py::int_(order);
                 return py::none();
             },
             R"(
            Get subscription to push order by name of subscription. Return None if subscription was not found.
            )",
             py::arg("name"))
        .def("pop", &IEventStream::pop,
             R"(
            Pop event.

            This function blocks execution until there is an event to pop.

            Returns:
                (:class:`.Event`) object. You own this object, it can be stored.
            )",
             py::call_guard<py::gil_scoped_release>())
        .def("try_pop", &IEventStream::tryPop,
             R"(
            Try pop event.

            Returns:
                Pops (:class:`.Event`) if stream is not empty or return `None`.
            )",
             py::call_guard<py::gil_scoped_release>()

                 )
        .def("pump", &IEventStream::pump,
             R"(
            Pump event stream.

            Dispatches all events in a stream.
            )",
             py::call_guard<py::gil_scoped_release>()

                 )

        .def("push",
             [](IEventStream* self, EventType eventType, SenderId sender, py::dict dict) {
                 ObjectPtrWrapper<IEvent> e;
                 {
                     py::gil_scoped_release nogil;
                     e = self->createEvent(eventType, sender);
                 }
                 carb::dictionary::setPyObject(carb::dictionary::getDictionary(), e->payload, nullptr, dict);
                 {
                     py::gil_scoped_release nogil;
                     self->push(e.get());
                 }
             },
             R"(
            Push :class:`.Event` into stream.

            Args:
                event_type (int): :class:`.Event` type.
                sender (int): Sender id. Unique can be acquired using :func:`.acquire_unique_sender_id`.
                dict (typing.Dict): :class:`.Event` payload.
            )",
             py::arg("event_type") = 0, py::arg("sender") = 0, py::arg("payload") = py::dict())
        .def("dispatch",
             [](IEventStream* self, EventType eventType, SenderId sender, py::dict dict) {
                 ObjectPtrWrapper<IEvent> e;
                 {
                     py::gil_scoped_release nogil;
                     e = self->createEvent(eventType, sender);
                 }
                 carb::dictionary::setPyObject(carb::dictionary::getDictionary(), e->payload, nullptr, dict);
                 {
                     py::gil_scoped_release nogil;
                     self->dispatch(e.get());
                 }
             },
             R"(
            Dispatch :class:`.Event` immediately without putting it into stream.

            Args:
                event_type (int): :class:`.Event` type.
                sender (int): Sender id. Unique can be acquired using :func:`.acquire_unique_sender_id`.
                dict (typing.Dict): :class:`.Event` payload.
            )",
             py::arg("event_type") = 0, py::arg("sender") = 0, py::arg("payload") = py::dict())
        .def_property_readonly("name", &IEventStream::getName, R"(
                Gets the name of the :class:`.EventStream`.

                Returns:
                    The name of the event stream.
             )",
                               py::call_guard<py::gil_scoped_release>());

    CARB_IGNOREWARNING_MSC_WITH_PUSH(5205)
    py::class_<IEvents>(m, "IEvents")
        .def("create_event_stream",
             [](IEvents* events, const char* name) {
                 std::string sname;
                 if (!name)
                 {
                     sname = getPythonCaller();
                     name = sname.empty() ? nullptr : sname.c_str();
                 }
                 py::gil_scoped_release gil;
                 return events->createEventStream(name);
             },
             R"(
            Create new `.EventStream`.

            Args:
                name: The name of the .EventStream for profiling. If no name is provided one is generated from the traceback of the calling function.
            )",
             py::arg("name") = (const char*)nullptr)
        .def("acquire_unique_sender_id", &IEvents::acquireUniqueSenderId,
             R"(
            Acquire unique sender id.

            Call :func:`.release_unique_sender_id` when it is not needed anymore. It can be reused then.
            )",
             py::call_guard<py::gil_scoped_release>())
        .def("release_unique_sender_id", &IEvents::releaseUniqueSenderId, py::call_guard<py::gil_scoped_release>());
    CARB_IGNOREWARNING_MSC_POP
}
} // namespace

} // namespace events
} // namespace carb
