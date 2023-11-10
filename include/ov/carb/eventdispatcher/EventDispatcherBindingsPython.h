// Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../BindingsPythonUtils.h"
#include "../dictionary/DictionaryBindingsPython.h"
#include "../variant/VariantBindingsPython.h"
#include "IEventDispatcher.h"

namespace carb
{
namespace eventdispatcher
{

inline variant::Variant objectToVariant(const py::handle& o)
{
    if (py::isinstance<py::bool_>(o))
    {
        return variant::Variant(o.cast<bool>());
    }
    else if (py::isinstance<py::int_>(o))
    {
        return variant::Variant(o.cast<int64_t>());
    }
    else if (py::isinstance<py::float_>(o))
    {
        return variant::Variant(o.cast<double>());
    }
    else if (py::isinstance<py::str>(o))
    {
        auto str = o.cast<std::string>();
        return variant::Variant(omni::string(str.begin(), str.end()));
    }
    else
    {
        return variant::Variant(o.cast<py::object>());
    }
}

// Requires the GIL
inline py::object variantToObject(const variant::Variant& v)
{
    if (v.data().vtable->typeName == variant::eBool)
    {
        return py::bool_(v.getValue<bool>().value());
    }
    else if (v.data().vtable->typeName == variant::eFloat || v.data().vtable->typeName == variant::eDouble)
    {
        return py::float_(v.getValue<double>().value());
    }
    else if (v.data().vtable->typeName == variant::eString)
    {
        omni::string str = v.getValue<omni::string>().value();
        return py::str(str.data(), str.length());
    }
    else if (v.data().vtable->typeName == variant::eCharPtr)
    {
        const char* p = v.getValue<const char*>().value();
        return py::str(p);
    }
    else if (v.data().vtable->typeName == variant::eDictionary)
    {
        auto item = v.getValue<const dictionary::Item*>().value();
        auto iface = getCachedInterface<dictionary::IDictionary>();
        CARB_ASSERT(iface, "Failed to acquire interface IDictionary");
        return dictionary::getPyObject(iface, item);
    }
    else if (v.data().vtable->typeName == variant::PyObjectVTable::get()->typeName)
    {
        return v.getValue<py::object>().value();
    }
    else
    {
        // Try numeric
        auto intval = v.getValue<int64_t>();
        if (intval)
            return py::int_(intval.value());

        CARB_LOG_WARN("Unknown type %s to convert to python object; using None", v.data().vtable->typeName.c_str());
        CARB_ASSERT(false, "Unknown type %s to convert to python", v.data().vtable->typeName.c_str());
        return py::none();
    }
}

class PyEvent : public std::enable_shared_from_this<PyEvent>
{
    const Event* p;
    std::vector<NamedVariant> variants;

    CARB_PREVENT_COPY_AND_MOVE(PyEvent);

public:
    RString eventName;

    PyEvent(const Event& e) : p(&e), eventName(p->eventName)
    {
    }

    void endRef()
    {
        if (p)
        {
            // Would use weak_from_this(), but need C++17; check against 2 since shared_from_this() will increase the
            // count by one.
            if (this->shared_from_this().use_count() > 2)
            {
                // Need a local copy
                variants = { p->variants, p->variants + p->numVariants };
            }
            p = nullptr;
        }
    }

    bool hasKey(const char* pkey) const
    {
        RStringKey key(pkey);
        if (p)
            return p->hasKey(key);

        return std::binary_search(variants.begin(), variants.end(), NamedVariant{ key, {} }, detail::NamedVariantLess{});
    }

    py::object get(const char* pkey) const
    {
        const variant::Variant* v = nullptr;
        RStringKey key(pkey);
        if (p)
            v = p->get(key);
        else
        {
            auto iter =
                std::lower_bound(variants.begin(), variants.end(), NamedVariant{ key, {} }, detail::NamedVariantLess{});
            if (iter != variants.end() && iter->name == key)
                v = &iter->value;
        }

        return v ? variantToObject(*v) : py::none();
    }
};
using PyEventPtr = std::shared_ptr<PyEvent>;

inline void definePythonModule(py::module& m)
{
    m.def("acquire_eventdispatcher_interface", []() { return getCachedInterface<IEventDispatcher>(); },
          py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>(),
          R"(Acquires the Event Dispatcher interface.)");

    py::class_<ObserverGuard, std::shared_ptr<ObserverGuard>>(m, "ObserverGuard", R"(ObserverGuard.

Lifetime control for a registered observer. Unregister the observer by calling the reset() function or allowing the
object to be collected.)")
        .def("reset", [](ObserverGuard& self) { self.reset(); }, py::call_guard<py::gil_scoped_release>(),
             R"(Explicitly stops an observer.

Having this object collected has the same effect, implicitly.

This is safe to perform while dispatching.

Since observers can be in use by this thread or any thread, this function is carefully synchronized with all other
Event Dispatcher operations.
 - During `reset()`, further calls to the observer are prevented, even if other threads are currently dispatching an
   event that would be observed by the observer in question.
 - If any other thread is currently calling the observer in question, `reset()` will wait until all other threads have
   left the observer callback function.
 - If the observer function is *not* in the backtrace of the current thread, the observer function is immediately
   released.
 - If the observer function is in the backtrace of the current thread, `reset()` will return without waiting and without
   releasing the observer callback. Instead, releasing the function will be performed when the `dispatch_event()` call
   in the current thread finishes.

When `reset()` returns, it is guaranteed that the observer callback function will no longer be called and all calls to
it have completed (except if the calling thread is dispatching).)");

    py::class_<PyEvent, PyEventPtr>(m, "Event", R"(Event.

        Contains the event_name and payload for a dispatched event.
    )")
        .def_property_readonly("event_name", [](const PyEventPtr& self) { return self->eventName.c_str(); },
                               py::call_guard<py::gil_scoped_release>(), R"(The name of the event)")
        .def("has_key", &PyEvent::hasKey, py::arg("key_name"), py::call_guard<py::gil_scoped_release>(),
             R"(Returns True if a given key name is present in the payload.

Args:
    key_name: The name of a key to check against the payload.

Returns:
    `True` if the key is present in the payload; `False` otherwise.)")
        .def("get", &PyEvent::get, py::arg("key_name") /* GIL managed internally */, R"(Accesses a payload item by key name.

Args:
    key_name: The name of a key to find in the payload.

Returns:
    None if the key is not present, otherwise returns an object representative of the type in the payload.)")
        .def("__getitem__", &PyEvent::get, py::arg("key_name") /* GIL managed internally */,
             R"(Accesses a payload item by key name.

Args:
    key_name: The name of a key to find in the payload.

Returns:
    None if the key is not present, otherwise returns an object representative of the type in the payload.)");

    py::class_<IEventDispatcher>(m, "IEventDispatcher", R"()")
        .def("observe_event",
             [](IEventDispatcher* ed, int order, const char* eventName, std::function<void(PyEventPtr)> onEventFn,
                py::handle filterDict) {
                 std::vector<NamedVariant> vec;
                 if (!filterDict.is_none())
                 {
                     auto dict = filterDict.cast<py::dict>();
                     vec.reserve(dict.size());
                     for (auto& entry : dict)
                     {
                         vec.push_back(
                             { RStringKey(entry.first.cast<std::string>().c_str()), objectToVariant(entry.second) });
                     }
                     std::sort(vec.begin(), vec.end(), detail::NamedVariantLess{});
                 }
                 auto p = new decltype(onEventFn)(std::move(onEventFn));
                 auto func = [](const Event& e, void* ud) {
                     // Convert the event
                     auto event = std::make_shared<PyEvent>(e);
                     callPythonCodeSafe(*static_cast<decltype(onEventFn)*>(ud), event);
                     event->endRef();
                 };
                 auto cleanup = [](void* ud) { delete static_cast<decltype(onEventFn)*>(ud); };
                 py::gil_scoped_release nogil;
                 return ObserverGuard(
                     ed->internalObserveEvent(order, RString(eventName), vec.size(), vec.data(), func, cleanup, p));
             },
             py::arg("order") = 0, py::arg("event_name"), py::arg("on_event"), py::arg("filter") = py::none(),
             py::return_value_policy::move,
             R"(Registers an observer with the Event Dispatcher system.

An observer is a callback that is called whenever :func:`.dispatch_event` is called. The observers are invoked in the
thread that calls `dispatch_event()`, and multiple threads may be calling `dispatch_event()` simultaneously, so
observers must be thread-safe unless the application can ensure synchronization around `dispatch_event()` calls.

Observers can pass an optional dictionary of `filter` arguments. The key/value pairs of `filter` arguments cause an
observer to only be invoked for a `dispatch_event()` call that contains at least the same values. For instance, having a
filter dictionary of `{"WindowID": 1234}` will only cause the observer to be called if `dispatch_event()` is given the
same value as a `"WindowID"` parameter.

Observers can be added inside of an observer notification (i.e. during a call to `dispatch_event()`), however these new
observers will not be called for currently the dispatching event. A subsequent recursive call to `dispatch_event()` (on
the current thread only) will also call the new observer. The new observer will be available to all other threads once
the `dispatch_event()` call (in which it was added) completes.

Args:
    order: (int) A value determining call order. Observers with lower order values are called earlier. Observers with
        the same order value and same filter argument values will be called in the order they are registered. Observers
        with the same order value with different filter arguments are called in an indeterminate order.
    event_name: (str) The event name to observe
    on_event: (function) A function that is invoked when an event matching `event_name` and any `filter` arguments is
        dispatched.
    filter: [optional] (dict) If present, must be a dict of key(str)/value(any) pairs.

Returns:
    An ObserverGuard object that, when collected, removes the observer from the Event Dispatcher system.
             )")
        .def("has_observers",
             [](IEventDispatcher* ed, const char* eventName, py::handle filterDict) {
                 std::vector<NamedVariant> vec;
                 if (!filterDict.is_none())
                 {
                     auto dict = filterDict.cast<py::dict>();
                     vec.reserve(dict.size());
                     for (auto& entry : dict)
                     {
                         vec.push_back(
                             { RStringKey(entry.first.cast<std::string>().c_str()), objectToVariant(entry.second) });
                     }
                     std::sort(vec.begin(), vec.end(), detail::NamedVariantLess{});
                 }
                 py::gil_scoped_release nogil;
                 return ed->internalHasObservers(RString(eventName), vec.size(), vec.data());
             },
             py::arg("event_name"), py::arg("filter") = py::none(),
             R"(Queries the Event Dispatcher whether any observers are listening to a specific event signature.

Emulates a call to :func:`.dispatch_event()` (without actually calling any observers) and returns `True` if any
observers would be called.

Args:
    event_name: (str) The event name to query
    filter: [optional] (dict) If present, must be a dict of key(str)/value(any) pairs.

Returns:
    `True` if at least one observer would be called with the given `filter` arguments; `False` otherwise.)")
        .def("dispatch_event",
             [](IEventDispatcher* ed, const char* eventName, py::handle payload) {
                 std::vector<NamedVariant> vec;
                 if (!payload.is_none())
                 {
                     auto dict = payload.cast<py::dict>();
                     vec.reserve(dict.size());
                     for (auto& entry : dict)
                     {
                         vec.push_back({ RStringKey(entry.first.cast<std::string>().c_str()),
                                         variant::Variant(entry.second.cast<py::object>()) });
                     }
                     std::sort(vec.begin(), vec.end(), detail::NamedVariantLess{});
                 }
                 py::gil_scoped_release nogil;
                 return ed->internalDispatch({ RString(eventName), vec.size(), vec.data() });
             },
             py::arg("event_name"), py::arg("payload") = py::none(),
             R"(Dispatches an event and immediately calls all observers that would observe this particular event.

Finds and calls all observers (in the current thread) that observe the given event signature.

It is safe to recursively dispatch events (i.e. call `dispatch_event()` from within a called observer), but care must be
taken to avoid endless recursion. See the rules in :func:`.observe_event()` for observers added during a
`dispatch_event()` call.

Args:
    event_name: (str) The name of the event to dispatch
    payload: (dict) If present, must be a dict of key(str)/value(any) pairs.

Returns:
    The number of observers that were called, excluding those from recursive calls.)");
}

} // namespace eventdispatcher
} // namespace carb
