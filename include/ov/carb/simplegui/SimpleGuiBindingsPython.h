// Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
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
#include "ISimpleGui.h"

#include <algorithm>
#include <string>
#include <vector>

namespace carb
{
namespace simplegui
{

inline void definePythonModule(py::module& m)
{
    using namespace carb;
    using namespace carb::simplegui;

    m.doc() = "pybind11 carb.simplegui bindings";

    py::enum_<Condition>(m, "Condition")
        .value("ALWAYS", Condition::eAlways)
        .value("APPEARING", Condition::eAppearing)
        .value("FIRST_USE_EVER", Condition::eFirstUseEver)
        .value("ONCE", Condition::eOnce);

    defineInterfaceClass<ISimpleGui>(m, "ISimpleGui", "acquire_simplegui")
        //.def("create_context", wrapInterfaceFunction(&ISimpleGui::createContext))
        //.def("destroy_context", wrapInterfaceFunction(&ISimpleGui::destroyContext))
        //.def("set_current_context", wrapInterfaceFunction(&ISimpleGui::setCurrentContext))
        //.def("new_frame", wrapInterfaceFunction(&ISimpleGui::newFrame))
        //.def("render", wrapInterfaceFunction(&ISimpleGui::render))
        .def("set_display_size", wrapInterfaceFunction(&ISimpleGui::setDisplaySize))
        .def("get_display_size", wrapInterfaceFunction(&ISimpleGui::getDisplaySize))
        .def("show_demo_window", wrapInterfaceFunction(&ISimpleGui::showDemoWindow))
        .def("set_next_window_pos", wrapInterfaceFunction(&ISimpleGui::setNextWindowPos))
        .def("set_next_window_size", wrapInterfaceFunction(&ISimpleGui::setNextWindowSize))
        .def("begin",
             [](const ISimpleGui* iface, const char* label, bool opened, carb::simplegui::WindowFlags flags) {
                 bool visible = iface->begin(label, &opened, flags);
                 return py::make_tuple(visible, opened);
             })
        .def("end", wrapInterfaceFunction(&ISimpleGui::end))
        .def("collapsing_header", wrapInterfaceFunction(&ISimpleGui::collapsingHeader))
        .def("text", [](const ISimpleGui* iface, const char* text) { iface->text(text); })
        .def("text_unformatted", wrapInterfaceFunction(&ISimpleGui::textUnformatted))
        .def("text_wrapped", [](const ISimpleGui* iface, const char* text) { iface->textWrapped(text); })
        .def("button", &ISimpleGui::button)
        .def("small_button", wrapInterfaceFunction(&ISimpleGui::smallButton))
        .def("same_line", &ISimpleGui::sameLine)
        .def("same_line_ex", wrapInterfaceFunction(&ISimpleGui::sameLineEx), py::arg("pos_x") = 0.0f,
             py::arg("spacing_w") = -1.0f)
        .def("separator", wrapInterfaceFunction(&ISimpleGui::separator))
        .def("spacing", wrapInterfaceFunction(&ISimpleGui::spacing))
        .def("indent", wrapInterfaceFunction(&ISimpleGui::indent))
        .def("unindent", wrapInterfaceFunction(&ISimpleGui::unindent))
        .def("dummy", wrapInterfaceFunction(&ISimpleGui::dummy))
        .def("bullet", wrapInterfaceFunction(&ISimpleGui::bullet))
        .def("checkbox",
             [](const ISimpleGui* iface, const char* label, bool value) {
                 bool clicked = iface->checkbox(label, &value);
                 return py::make_tuple(clicked, value);
             })
        .def("input_float",
             [](const ISimpleGui* iface, const char* label, float value, float step) {
                 bool clicked = iface->inputFloat(label, &value, step, 0.0f, -1, 0);
                 return py::make_tuple(clicked, value);
             })
        .def("input_int",
             [](const ISimpleGui* iface, const char* label, int value, int step) {
                 bool clicked = iface->inputInt(label, &value, step, 0, 0);
                 return py::make_tuple(clicked, value);
             })
        .def("input_text",
             [](const ISimpleGui* iface, const char* label, const std::string& str, size_t size) {
                 std::vector<char> buf(str.begin(), str.end());
                 buf.resize(size);
                 bool clicked = iface->inputText(label, buf.data(), size, 0, nullptr, nullptr);
                 return py::make_tuple(clicked, buf.data());
             })
        .def("slider_float",
             [](const ISimpleGui* iface, const char* label, float value, float vMin, float vMax) {
                 bool clicked = iface->sliderFloat(label, &value, vMin, vMax, "%.3f", 1.0f);
                 return py::make_tuple(clicked, value);
             })
        .def("slider_int",
             [](const ISimpleGui* iface, const char* label, int value, int vMin, int vMax) {
                 bool clicked = iface->sliderInt(label, &value, vMin, vMax, "%.0f");
                 return py::make_tuple(clicked, value);
             })
        .def("combo",
             [](const ISimpleGui* iface, const char* label, int selectedItem, std::vector<std::string> items) {
                 std::vector<const char*> itemPtrs(items.size());
                 std::transform(
                     items.begin(), items.end(), itemPtrs.begin(), [](const std::string& s) { return s.c_str(); });
                 bool clicked = iface->combo(label, &selectedItem, itemPtrs.data(), (int)itemPtrs.size());
                 return py::make_tuple(clicked, selectedItem);
             })
        .def("progress_bar", wrapInterfaceFunction(&ISimpleGui::progressBar))
        .def("color_edit3",
             [](const ISimpleGui* iface, const char* label, Float3 color) {
                 bool clicked = iface->colorEdit3(label, &color.x, 0);
                 return py::make_tuple(clicked, color);
             })
        .def("color_edit4",
             [](const ISimpleGui* iface, const char* label, Float4 color) {
                 bool clicked = iface->colorEdit4(label, &color.x, 0);
                 return py::make_tuple(clicked, color);
             })
        .def("push_id_string", wrapInterfaceFunction(&ISimpleGui::pushIdString))
        .def("push_id_int", wrapInterfaceFunction(&ISimpleGui::pushIdInt))
        .def("pop_id", wrapInterfaceFunction(&ISimpleGui::popId))
        .def("push_item_width", wrapInterfaceFunction(&ISimpleGui::pushItemWidth))
        .def("pop_item_width", wrapInterfaceFunction(&ISimpleGui::popItemWidth))
        .def("tree_node_ptr",
             [](const ISimpleGui* iface, int64_t id, const char* text) { return iface->treeNodePtr((void*)id, text); })
        .def("tree_pop", wrapInterfaceFunction(&ISimpleGui::treePop))
        .def("begin_child", wrapInterfaceFunction(&ISimpleGui::beginChild))
        .def("end_child", wrapInterfaceFunction(&ISimpleGui::endChild))
        .def("set_scroll_here_y", wrapInterfaceFunction(&ISimpleGui::setScrollHereY))
        .def("open_popup", wrapInterfaceFunction(&ISimpleGui::openPopup))
        .def("begin_popup_modal", wrapInterfaceFunction(&ISimpleGui::beginPopupModal))
        .def("end_popup", wrapInterfaceFunction(&ISimpleGui::endPopup))
        .def("close_current_popup", wrapInterfaceFunction(&ISimpleGui::closeCurrentPopup))
        .def("push_style_color", wrapInterfaceFunction(&ISimpleGui::pushStyleColor))
        .def("pop_style_color", wrapInterfaceFunction(&ISimpleGui::popStyleColor))
        .def("push_style_var_float", wrapInterfaceFunction(&ISimpleGui::pushStyleVarFloat))
        .def("push_style_var_float2", wrapInterfaceFunction(&ISimpleGui::pushStyleVarFloat2))
        .def("pop_style_var", wrapInterfaceFunction(&ISimpleGui::popStyleVar))
        .def("menu_item_ex",
             [](const ISimpleGui* iface, const char* label, const char* shortcut, bool selected, bool enabled) {
                 bool activated = iface->menuItemEx(label, shortcut, &selected, enabled);
                 return py::make_tuple(activated, selected);
             })
        .def("dock_builder_dock_window", wrapInterfaceFunction(&ISimpleGui::dockBuilderDockWindow))
        .def("plot_lines", [](const ISimpleGui* self, const char* label, const std::vector<float>& values,
                              int valuesCount, int valuesOffset, const char* overlayText, float scaleMin,
                              float scaleMax, Float2 graphSize, int stride) {
            self->plotLines(
                label, values.data(), valuesCount, valuesOffset, overlayText, scaleMin, scaleMax, graphSize, stride);
        });

    m.attr("WINDOW_FLAG_NO_TITLE_BAR") = py::int_(kWindowFlagNoTitleBar);
    m.attr("WINDOW_FLAG_NO_RESIZE") = py::int_(kWindowFlagNoResize);
    m.attr("WINDOW_FLAG_NO_MOVE") = py::int_(kWindowFlagNoMove);
    m.attr("WINDOW_FLAG_NO_SCROLLBAR") = py::int_(kWindowFlagNoScrollbar);
    m.attr("WINDOW_FLAG_NO_SCROLL_WITH_MOUSE") = py::int_(kWindowFlagNoScrollWithMouse);
    m.attr("WINDOW_FLAG_NO_COLLAPSE") = py::int_(kWindowFlagNoCollapse);
    m.attr("WINDOW_FLAG_ALWAYS_AUTO_RESIZE") = py::int_(kWindowFlagAlwaysAutoResize);
    m.attr("WINDOW_FLAG_NO_BACKGROUND") = py::int_(kWindowFlagNoBackground);
    m.attr("WINDOW_FLAG_NO_SAVED_SETTINGS") = py::int_(kWindowFlagNoSavedSettings);
    m.attr("WINDOW_FLAG_NO_MOUSE_INPUTS") = py::int_(kWindowFlagNoMouseInputs);
    m.attr("WINDOW_FLAG_MENU_BAR") = py::int_(kWindowFlagMenuBar);
    m.attr("WINDOW_FLAG_HORIZONTAL_SCROLLBAR") = py::int_(kWindowFlagHorizontalScrollbar);
    m.attr("WINDOW_FLAG_NO_FOCUS_ON_APPEARING") = py::int_(kWindowFlagNoFocusOnAppearing);
    m.attr("WINDOW_FLAG_NO_BRING_TO_FRONT_ON_FOCUS") = py::int_(kWindowFlagNoBringToFrontOnFocus);
    m.attr("WINDOW_FLAG_ALWAYS_VERTICAL_SCROLLBAR") = py::int_(kWindowFlagAlwaysVerticalScrollbar);
    m.attr("WINDOW_FLAG_ALWAYS_HORIZONTAL_SCROLLBAR") = py::int_(kWindowFlagAlwaysHorizontalScrollbar);
    m.attr("WINDOW_FLAG_ALWAYS_USE_WINDOW_PADDING") = py::int_(kWindowFlagAlwaysUseWindowPadding);
    m.attr("WINDOW_FLAG_NO_NAV_INPUTS") = py::int_(kWindowFlagNoNavFocus);
    m.attr("WINDOW_FLAG_NO_NAV_FOCUS") = py::int_(kWindowFlagNoNavInputs);
    m.attr("WINDOW_FLAG_UNSAVED_DOCUMENT") = py::int_(kWindowFlagUnsavedDocument);
    m.attr("WINDOW_FLAG_NO_DOCKING") = py::int_(kWindowFlagNoDocking);
}

} // namespace simplegui
} // namespace carb
