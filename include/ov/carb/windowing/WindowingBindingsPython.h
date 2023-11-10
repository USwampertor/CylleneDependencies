// Copyright (c) 2018-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../BindingsPythonUtils.h"
#include "../BindingsPythonTypes.h"
#include "IGLContext.h"
#include "IWindowing.h"

#include <memory>
#include <string>
#include <vector>

namespace carb
{
namespace windowing
{
struct Cursor
{
};
struct GLContext
{
};

struct ImagePy
{
    int32_t width;
    int32_t height;
    py::bytes pixels;

    ImagePy(int32_t _width, int32_t _height, py::bytes& _pixels) : width(_width), height(_height), pixels(_pixels)
    {
    }
};

inline void definePythonModule(py::module& m)
{
    using namespace carb;
    using namespace carb::windowing;

    m.doc() = "pybind11 carb.windowing bindings";

    py::class_<Window>(m, "Window");
    py::class_<Cursor>(m, "Cursor");
    py::class_<GLContext>(m, "GLContext");

    py::class_<ImagePy>(m, "Image")
        .def(py::init<int32_t, int32_t, py::bytes&>(), py::arg("width"), py::arg("height"), py::arg("pixels"));

    m.attr("WINDOW_HINT_NONE") = py::int_(kWindowHintNone);
    m.attr("WINDOW_HINT_NO_RESIZE") = py::int_(kWindowHintNoResize);
    m.attr("WINDOW_HINT_NO_DECORATION") = py::int_(kWindowHintNoDecoration);
    m.attr("WINDOW_HINT_NO_AUTO_ICONIFY") = py::int_(kWindowHintNoAutoIconify);
    m.attr("WINDOW_HINT_NO_FOCUS_ON_SHOW") = py::int_(kWindowHintNoFocusOnShow);
    m.attr("WINDOW_HINT_SCALE_TO_MONITOR") = py::int_(kWindowHintScaleToMonitor);
    m.attr("WINDOW_HINT_FLOATING") = py::int_(kWindowHintFloating);
    m.attr("WINDOW_HINT_MAXIMIZED") = py::int_(kWindowHintMaximized);

    py::enum_<CursorStandardShape>(m, "CursorStandardShape")
        .value("ARROW", CursorStandardShape::eArrow)
        .value("IBEAM", CursorStandardShape::eIBeam)
        .value("CROSSHAIR", CursorStandardShape::eCrosshair)
        .value("HAND", CursorStandardShape::eHand)
        .value("HORIZONTAL_RESIZE", CursorStandardShape::eHorizontalResize)
        .value("VERTICAL_RESIZE", CursorStandardShape::eVerticalResize);

    py::enum_<CursorMode>(m, "CursorMode")
        .value("NORMAL", CursorMode::eNormal)
        .value("HIDDEN", CursorMode::eHidden)
        .value("DISABLED", CursorMode::eDisabled);

    py::enum_<InputMode>(m, "InputMode")
        .value("STICKY_KEYS", InputMode::eStickyKeys)
        .value("STICKY_MOUSE_BUTTONS", InputMode::eStickyMouseButtons)
        .value("LOCK_KEY_MODS", InputMode::eLockKeyMods)
        .value("RAW_MOUSE_MOTION", InputMode::eRawMouseMotion);

    defineInterfaceClass<IWindowing>(m, "IWindowing", "acquire_windowing_interface")
        .def("create_window",
             [](const IWindowing* iface, int width, int height, const char* title, bool fullscreen, int hints) {
                 WindowDesc desc = {};
                 desc.width = width;
                 desc.height = height;
                 desc.title = title;
                 desc.fullscreen = fullscreen;
                 desc.hints = hints;
                 return iface->createWindow(desc);
             },
             py::arg("width"), py::arg("height"), py::arg("title"), py::arg("fullscreen"),
             py::arg("hints") = kWindowHintNone, py::return_value_policy::reference)
        .def("destroy_window", wrapInterfaceFunction(&IWindowing::destroyWindow))
        .def("show_window", wrapInterfaceFunction(&IWindowing::showWindow))
        .def("hide_window", wrapInterfaceFunction(&IWindowing::hideWindow))
        .def("get_window_width", wrapInterfaceFunction(&IWindowing::getWindowWidth))
        .def("get_window_height", wrapInterfaceFunction(&IWindowing::getWindowHeight))
        .def("get_window_position", wrapInterfaceFunction(&IWindowing::getWindowPosition))
        .def("set_window_position", wrapInterfaceFunction(&IWindowing::setWindowPosition))
        .def("set_window_title", wrapInterfaceFunction(&IWindowing::setWindowTitle))
        .def("set_window_opacity", wrapInterfaceFunction(&IWindowing::setWindowOpacity))
        .def("get_window_opacity", wrapInterfaceFunction(&IWindowing::getWindowOpacity))
        .def("set_window_fullscreen", wrapInterfaceFunction(&IWindowing::setWindowFullscreen))
        .def("is_window_fullscreen", wrapInterfaceFunction(&IWindowing::isWindowFullscreen))
        .def("resize_window", wrapInterfaceFunction(&IWindowing::resizeWindow))
        .def("focus_window", wrapInterfaceFunction(&IWindowing::focusWindow))
        .def("is_window_focused", wrapInterfaceFunction(&IWindowing::isWindowFocused))
        .def("maximize_window", wrapInterfaceFunction(&IWindowing::maximizeWindow))
        .def("minimize_window", wrapInterfaceFunction(&IWindowing::minimizeWindow))
        .def("restore_window", wrapInterfaceFunction(&IWindowing::restoreWindow))
        .def("is_window_maximized", wrapInterfaceFunction(&IWindowing::isWindowMaximized))
        .def("is_window_minimized", wrapInterfaceFunction(&IWindowing::isWindowMinimized))
        .def("should_window_close", wrapInterfaceFunction(&IWindowing::shouldWindowClose))
        .def("set_window_should_close", wrapInterfaceFunction(&IWindowing::setWindowShouldClose))
        .def("get_window_user_pointer", wrapInterfaceFunction(&IWindowing::getWindowUserPointer))
        .def("set_window_user_pointer", wrapInterfaceFunction(&IWindowing::setWindowUserPointer))
        .def("set_window_content_scale", wrapInterfaceFunction(&IWindowing::getWindowContentScale))
        .def("get_native_display", wrapInterfaceFunction(&IWindowing::getNativeDisplay))
        .def("get_native_window", wrapInterfaceFunction(&IWindowing::getNativeWindow), py::return_value_policy::reference)
        .def("set_input_mode", wrapInterfaceFunction(&IWindowing::setInputMode))
        .def("get_input_mode", wrapInterfaceFunction(&IWindowing::getInputMode))
        .def("update_input_devices", wrapInterfaceFunction(&IWindowing::updateInputDevices))
        .def("poll_events", wrapInterfaceFunction(&IWindowing::pollEvents))
        .def("wait_events", wrapInterfaceFunction(&IWindowing::waitEvents))
        .def("get_keyboard", wrapInterfaceFunction(&IWindowing::getKeyboard), py::return_value_policy::reference)
        .def("get_mouse", wrapInterfaceFunction(&IWindowing::getMouse), py::return_value_policy::reference)
        .def("create_cursor_standard", wrapInterfaceFunction(&IWindowing::createCursorStandard),
             py::return_value_policy::reference)
        .def("create_cursor",
             [](IWindowing* windowing, ImagePy& imagePy, int32_t xhot, int32_t yhot) {
                 py::buffer_info info(py::buffer(imagePy.pixels).request());
                 uint8_t* data = reinterpret_cast<uint8_t*>(info.ptr);
                 Image image{ imagePy.width, imagePy.height, data };
                 return windowing->createCursor(image, xhot, yhot);
             },
             py::return_value_policy::reference)
        .def("destroy_cursor", wrapInterfaceFunction(&IWindowing::destroyCursor))
        .def("set_cursor", wrapInterfaceFunction(&IWindowing::setCursor))
        .def("set_cursor_mode", wrapInterfaceFunction(&IWindowing::setCursorMode))
        .def("get_cursor_mode", wrapInterfaceFunction(&IWindowing::getCursorMode))
        .def("set_cursor_position", wrapInterfaceFunction(&IWindowing::setCursorPosition))
        .def("get_cursor_position", wrapInterfaceFunction(&IWindowing::getCursorPosition))
        .def("set_clipboard", wrapInterfaceFunction(&IWindowing::setClipboard))
        .def("get_clipboard", wrapInterfaceFunction(&IWindowing::getClipboard));


    defineInterfaceClass<IGLContext>(m, "IGLContext", "acquire_gl_context_interface")
        .def("create_context_opengl",
             [](const IGLContext* iface, int width, int height) { return iface->createContextOpenGL(width, height); },
             py::arg("width"), py::arg("height"), py::return_value_policy::reference)
        .def("create_context_opengles",
             [](const IGLContext* iface, int width, int height) { return iface->createContextOpenGLES(width, height); },
             py::arg("width"), py::arg("height"), py::return_value_policy::reference)
        .def("destroy_context", wrapInterfaceFunction(&IGLContext::destroyContext))
        .def("make_context_current", wrapInterfaceFunction(&IGLContext::makeContextCurrent));
}
} // namespace windowing
} // namespace carb
