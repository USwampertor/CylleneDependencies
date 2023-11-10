// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
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
#include "IProfileMonitor.h"

#include <memory>
#include <string>
#include <vector>

using namespace pybind11::literals;

namespace carb
{
namespace profiler
{

namespace
{

class ScopedProfileEvents
{
public:
    ScopedProfileEvents(const IProfileMonitor* profileMonitor) : m_profileMonitor(profileMonitor)
    {
        m_profileEvents = m_profileMonitor->getLastProfileEvents();
    }
    ScopedProfileEvents(ScopedProfileEvents&& rhs)
        : m_profileEvents(rhs.m_profileEvents), m_profileMonitor(rhs.m_profileMonitor)
    {
        rhs.m_profileEvents = nullptr;
    }
    ~ScopedProfileEvents()
    {
        if (m_profileEvents)
            m_profileMonitor->releaseLastProfileEvents(m_profileEvents);
    }

    ScopedProfileEvents& operator=(ScopedProfileEvents&& rhs)
    {
        std::swap(m_profileEvents, rhs.m_profileEvents);
        std::swap(m_profileMonitor, rhs.m_profileMonitor);
        return *this;
    }

    CARB_PREVENT_COPY(ScopedProfileEvents);

    const IProfileMonitor* mon() const
    {
        return m_profileMonitor;
    }

    operator ProfileEvents() const
    {
        return m_profileEvents;
    }

private:
    ProfileEvents m_profileEvents;
    const IProfileMonitor* m_profileMonitor;
};
inline void definePythonModule(py::module& m)
{
    using namespace carb::profiler;

    m.doc() = "pybind11 carb.profiler bindings";

    py::enum_<InstantType>(m, "InstantType").value("THREAD", InstantType::Thread).value("PROCESS", InstantType::Process)
        /**/;

    py::enum_<FlowType>(m, "FlowType").value("BEGIN", FlowType::Begin).value("END", FlowType::End)
        /**/;

    m.def("is_profiler_active", []() -> bool { return (g_carbProfiler != nullptr); },
          py::call_guard<py::gil_scoped_release>());
    m.def("supports_dynamic_source_locations",
          []() -> bool { return (g_carbProfiler && g_carbProfiler->supportsDynamicSourceLocations()); },
          py::call_guard<py::gil_scoped_release>());
    // This is an extended helper with location, the shorter `begin` method is defined in __init__.py
    m.def("begin_with_location",
          [](uint64_t mask, std::string name, std::string functionStr, std::string filepathStr, uint32_t line) {
              if (g_carbProfiler)
              {
                  static carb::profiler::StaticStringType sfunction{ g_carbProfiler->registerStaticString("Py::func") };
                  static carb::profiler::StaticStringType sfilepath{ g_carbProfiler->registerStaticString("Py::code") };

                  auto function = sfunction, filepath = sfilepath;
                  if (g_carbProfiler->supportsDynamicSourceLocations())
                  {
                      if (!functionStr.empty())
                      {
                          function = carb::profiler::StaticStringType(functionStr.c_str());
                      }
                      if (!filepathStr.empty())
                      {
                          filepath = carb::profiler::StaticStringType(filepathStr.c_str());
                      }
                  }
                  uint32_t linenumber = line;
                  g_carbProfiler->beginDynamic(mask, function, filepath, linenumber, "%s", name.c_str());
              }
          },
          py::arg("mask"), py::arg("name"), py::arg("function") = "", py::arg("filepath") = "", py::arg("lineno") = 0,
          py::call_guard<py::gil_scoped_release>());
    m.def("end",
          [](uint64_t mask) {
              if (g_carbProfiler)
              {
                  g_carbProfiler->end(mask);
              }
          },
          py::arg("mask"), py::call_guard<py::gil_scoped_release>());

    defineInterfaceClass<IProfiler>(m, "IProfiler", "acquire_profiler_interface")
        // .def("get_item", wrapInterfaceFunction(&IDictionary::getItem), py::arg("base_item"), py::arg("path") = "",
        //     py::return_value_policy::reference)
        .def("startup", [](IProfiler* self) { self->startup(); }, py::call_guard<py::gil_scoped_release>())
        .def("shutdown", [](IProfiler* self) { self->shutdown(); }, py::call_guard<py::gil_scoped_release>())
        .def("set_capture_mask", [](IProfiler* self, uint64_t mask) { return self->setCaptureMask(mask); },
             py::arg("mask"), py::call_guard<py::gil_scoped_release>())
        .def("get_capture_mask", [](IProfiler* self) { return self->getCaptureMask(); },
             py::call_guard<py::gil_scoped_release>())
        .def("begin",
             [](IProfiler* self, uint64_t mask, std::string name) {
                 static auto function = self->registerStaticString("pyfunc");
                 static auto file = self->registerStaticString("python");
                 self->beginDynamic(mask, function, file, 1, "%s", name.c_str());
             },
             py::arg("mask"), py::arg("name"), py::call_guard<py::gil_scoped_release>())
        .def("frame", [](IProfiler* self, uint64_t mask, const char* name) { self->frameDynamic(mask, "%s", name); },
             py::call_guard<py::gil_scoped_release>())
        .def("end", [](IProfiler* self, uint64_t mask) { self->end(mask); }, py::arg("mask"),
             py::call_guard<py::gil_scoped_release>())
        .def("value_float",
             [](IProfiler* self, uint64_t mask, float value, std::string name) {
                 self->valueFloatDynamic(mask, value, "%s", name.c_str());
             },
             py::arg("mask"), py::arg("value"), py::arg("name"), py::call_guard<py::gil_scoped_release>())
        .def("value_int",
             [](IProfiler* self, uint64_t mask, int32_t value, std::string name) {
                 self->valueIntDynamic(mask, value, "%s", name.c_str());
             },
             py::arg("mask"), py::arg("value"), py::arg("name"), py::call_guard<py::gil_scoped_release>())
        .def("value_uint",
             [](IProfiler* self, uint64_t mask, uint32_t value, std::string name) {
                 self->valueUIntDynamic(mask, value, "%s", name.c_str());
             },
             py::arg("mask"), py::arg("value"), py::arg("name"), py::call_guard<py::gil_scoped_release>())
        .def("instant",
             [](IProfiler* self, uint64_t mask, InstantType type, const char* name) {
                 static auto function = self->registerStaticString("pyfunc");
                 static auto file = self->registerStaticString("python");
                 self->emitInstantDynamic(mask, function, file, 1, type, "%s", name);
             },
             py::call_guard<py::gil_scoped_release>())
        .def("flow",
             [](IProfiler* self, uint64_t mask, FlowType type, uint64_t id, const char* name) {
                 static auto function = self->registerStaticString("pyfunc");
                 static auto file = self->registerStaticString("python");
                 self->emitFlowDynamic(mask, function, file, 1, type, id, "%s", name);
             },
             py::call_guard<py::gil_scoped_release>());
    /**/;

    py::class_<ScopedProfileEvents>(m, "ProfileEvents", R"(Profile Events holder)")
        .def("get_main_thread_id",
             [](const ScopedProfileEvents& events) { return events.mon()->getMainThreadId(events); },
             py::call_guard<py::gil_scoped_release>())
        .def("get_profile_thread_ids",
             [](const ScopedProfileEvents& profileEvents) {
                 size_t threadCount;
                 const uint64_t* ids;
                 {
                     py::gil_scoped_release nogil;
                     const IProfileMonitor* monitor = profileEvents.mon();
                     threadCount = monitor->getProfileThreadCount(profileEvents);
                     ids = monitor->getProfileThreadIds(profileEvents);
                 }
                 py::tuple threadIds(threadCount);
                 for (size_t i = 0; i < threadCount; i++)
                 {
                     threadIds[i] = ids[i];
                 }
                 return threadIds;
             })
        .def("get_profile_events",
             [](const ScopedProfileEvents& profileEvents, uint64_t threadId) {
                 size_t eventCount;
                 ProfileEvent* events;
                 uint32_t validEventCount{ 0 };
                 {
                     py::gil_scoped_release nogil;
                     const IProfileMonitor* monitor = profileEvents.mon();
                     eventCount = monitor->getLastProfileEventCount(profileEvents);
                     events = monitor->getLastProfileEventsData(profileEvents);

                     if (threadId == 0)
                     {
                         validEventCount = static_cast<uint32_t>(eventCount);
                     }
                     else
                     {
                         for (size_t i = 0; i < eventCount; i++)
                         {
                             if (events[i].threadId == threadId)
                                 validEventCount++;
                         }
                     }
                 }

                 py::tuple nodeList(validEventCount);
                 validEventCount = 0;
                 for (size_t i = 0; i < eventCount; i++)
                 {
                     if (threadId != 0 && events[i].threadId != threadId)
                         continue;
                     nodeList[validEventCount++] =
                         py::dict("name"_a = events[i].eventName, "thread_id"_a = events[i].threadId,
                                  "start_time"_a = events[i].startTime, "duration"_a = events[i].timeInMs,
                                  "indent"_a = events[i].level);
                 }
                 return nodeList;
             },
             py::arg("thread_id") = 0)
        /**/;

    defineInterfaceClass<IProfileMonitor>(m, "IProfileMonitor", "acquire_profile_monitor_interface")
        .def("get_last_profile_events", [](const IProfileMonitor* monitor) { return ScopedProfileEvents(monitor); },
             py::call_guard<py::gil_scoped_release>())
        .def("mark_frame_end", [](const IProfileMonitor* monitor) { return monitor->markFrameEnd(); },
             py::call_guard<py::gil_scoped_release>())
        /**/;
}

} // namespace
} // namespace profiler
} // namespace carb
