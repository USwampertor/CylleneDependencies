// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
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
#include "../Framework.h"
#include "IInput.h"
#include "InputProvider.h"
#include "InputUtils.h"

#include <memory>
#include <string>
#include <vector>

namespace carb
{
namespace input
{
struct InputDevice
{
};
struct Gamepad
{
};
class ActionMappingSet
{
};
} // namespace input


namespace input
{

namespace detail
{


inline ActionMappingDesc toMouseMapping(Mouse* mouse, MouseInput input, KeyboardModifierFlags modifiers)
{
    ActionMappingDesc mapping{};
    mapping.deviceType = DeviceType::eMouse;
    mapping.mouse = mouse;
    mapping.mouseInput = input;
    mapping.modifiers = modifiers;
    return mapping;
}

inline ActionMappingDesc toGamepadMapping(Gamepad* gamepad, GamepadInput input)
{
    ActionMappingDesc mapping{};
    mapping.deviceType = DeviceType::eGamepad;
    mapping.gamepad = gamepad;
    mapping.gamepadInput = input;
    return mapping;
}

} // namespace detail

inline void definePythonModule(py::module& m)
{
    m.doc() = "pybind11 carb.input bindings";

    py::class_<InputDevice> device(m, "InputDevice");
    py::class_<Keyboard>(m, "Keyboard", device);
    py::class_<Mouse>(m, "Mouse", device);
    py::class_<Gamepad>(m, "Gamepad", device);
    py::class_<ActionMappingSet>(m, "ActionMappingSet");

    py::enum_<EventType>(m, "EventType").value("UNKNOWN", EventType::eUnknown);
    m.attr("EVENT_TYPE_ALL") = py::int_(kEventTypeAll);
    m.attr("SUBSCRIPTION_ORDER_FIRST") = py::int_(kSubscriptionOrderFirst);
    m.attr("SUBSCRIPTION_ORDER_LAST") = py::int_(kSubscriptionOrderLast);
    m.attr("SUBSCRIPTION_ORDER_DEFAULT") = py::int_(kSubscriptionOrderDefault);

    py::enum_<DeviceType>(m, "DeviceType")
        .value("KEYBOARD", DeviceType::eKeyboard)
        .value("MOUSE", DeviceType::eMouse)
        .value("GAMEPAD", DeviceType::eGamepad);

    py::enum_<KeyboardEventType>(m, "KeyboardEventType")
        .value("KEY_PRESS", KeyboardEventType::eKeyPress)
        .value("KEY_REPEAT", KeyboardEventType::eKeyRepeat)
        .value("KEY_RELEASE", KeyboardEventType::eKeyRelease)
        .value("CHAR", KeyboardEventType::eChar);

    py::enum_<KeyboardInput>(m, "KeyboardInput")
        .value("UNKNOWN", KeyboardInput::eUnknown)
        .value("SPACE", KeyboardInput::eSpace)
        .value("APOSTROPHE", KeyboardInput::eApostrophe)
        .value("COMMA", KeyboardInput::eComma)
        .value("MINUS", KeyboardInput::eMinus)
        .value("PERIOD", KeyboardInput::ePeriod)
        .value("SLASH", KeyboardInput::eSlash)
        .value("KEY_0", KeyboardInput::eKey0)
        .value("KEY_1", KeyboardInput::eKey1)
        .value("KEY_2", KeyboardInput::eKey2)
        .value("KEY_3", KeyboardInput::eKey3)
        .value("KEY_4", KeyboardInput::eKey4)
        .value("KEY_5", KeyboardInput::eKey5)
        .value("KEY_6", KeyboardInput::eKey6)
        .value("KEY_7", KeyboardInput::eKey7)
        .value("KEY_8", KeyboardInput::eKey8)
        .value("KEY_9", KeyboardInput::eKey9)
        .value("SEMICOLON", KeyboardInput::eSemicolon)
        .value("EQUAL", KeyboardInput::eEqual)
        .value("A", KeyboardInput::eA)
        .value("B", KeyboardInput::eB)
        .value("C", KeyboardInput::eC)
        .value("D", KeyboardInput::eD)
        .value("E", KeyboardInput::eE)
        .value("F", KeyboardInput::eF)
        .value("G", KeyboardInput::eG)
        .value("H", KeyboardInput::eH)
        .value("I", KeyboardInput::eI)
        .value("J", KeyboardInput::eJ)
        .value("K", KeyboardInput::eK)
        .value("L", KeyboardInput::eL)
        .value("M", KeyboardInput::eM)
        .value("N", KeyboardInput::eN)
        .value("O", KeyboardInput::eO)
        .value("P", KeyboardInput::eP)
        .value("Q", KeyboardInput::eQ)
        .value("R", KeyboardInput::eR)
        .value("S", KeyboardInput::eS)
        .value("T", KeyboardInput::eT)
        .value("U", KeyboardInput::eU)
        .value("V", KeyboardInput::eV)
        .value("W", KeyboardInput::eW)
        .value("X", KeyboardInput::eX)
        .value("Y", KeyboardInput::eY)
        .value("Z", KeyboardInput::eZ)
        .value("LEFT_BRACKET", KeyboardInput::eLeftBracket)
        .value("BACKSLASH", KeyboardInput::eBackslash)
        .value("RIGHT_BRACKET", KeyboardInput::eRightBracket)
        .value("GRAVE_ACCENT", KeyboardInput::eGraveAccent)
        .value("ESCAPE", KeyboardInput::eEscape)
        .value("TAB", KeyboardInput::eTab)
        .value("ENTER", KeyboardInput::eEnter)
        .value("BACKSPACE", KeyboardInput::eBackspace)
        .value("INSERT", KeyboardInput::eInsert)
        .value("DEL", KeyboardInput::eDel)
        .value("RIGHT", KeyboardInput::eRight)
        .value("LEFT", KeyboardInput::eLeft)
        .value("DOWN", KeyboardInput::eDown)
        .value("UP", KeyboardInput::eUp)
        .value("PAGE_UP", KeyboardInput::ePageUp)
        .value("PAGE_DOWN", KeyboardInput::ePageDown)
        .value("HOME", KeyboardInput::eHome)
        .value("END", KeyboardInput::eEnd)
        .value("CAPS_LOCK", KeyboardInput::eCapsLock)
        .value("SCROLL_LOCK", KeyboardInput::eScrollLock)
        .value("NUM_LOCK", KeyboardInput::eNumLock)
        .value("PRINT_SCREEN", KeyboardInput::ePrintScreen)
        .value("PAUSE", KeyboardInput::ePause)
        .value("F1", KeyboardInput::eF1)
        .value("F2", KeyboardInput::eF2)
        .value("F3", KeyboardInput::eF3)
        .value("F4", KeyboardInput::eF4)
        .value("F5", KeyboardInput::eF5)
        .value("F6", KeyboardInput::eF6)
        .value("F7", KeyboardInput::eF7)
        .value("F8", KeyboardInput::eF8)
        .value("F9", KeyboardInput::eF9)
        .value("F10", KeyboardInput::eF10)
        .value("F11", KeyboardInput::eF11)
        .value("F12", KeyboardInput::eF12)
        .value("NUMPAD_0", KeyboardInput::eNumpad0)
        .value("NUMPAD_1", KeyboardInput::eNumpad1)
        .value("NUMPAD_2", KeyboardInput::eNumpad2)
        .value("NUMPAD_3", KeyboardInput::eNumpad3)
        .value("NUMPAD_4", KeyboardInput::eNumpad4)
        .value("NUMPAD_5", KeyboardInput::eNumpad5)
        .value("NUMPAD_6", KeyboardInput::eNumpad6)
        .value("NUMPAD_7", KeyboardInput::eNumpad7)
        .value("NUMPAD_8", KeyboardInput::eNumpad8)
        .value("NUMPAD_9", KeyboardInput::eNumpad9)
        .value("NUMPAD_DEL", KeyboardInput::eNumpadDel)
        .value("NUMPAD_DIVIDE", KeyboardInput::eNumpadDivide)
        .value("NUMPAD_MULTIPLY", KeyboardInput::eNumpadMultiply)
        .value("NUMPAD_SUBTRACT", KeyboardInput::eNumpadSubtract)
        .value("NUMPAD_ADD", KeyboardInput::eNumpadAdd)
        .value("NUMPAD_ENTER", KeyboardInput::eNumpadEnter)
        .value("NUMPAD_EQUAL", KeyboardInput::eNumpadEqual)
        .value("LEFT_SHIFT", KeyboardInput::eLeftShift)
        .value("LEFT_CONTROL", KeyboardInput::eLeftControl)
        .value("LEFT_ALT", KeyboardInput::eLeftAlt)
        .value("LEFT_SUPER", KeyboardInput::eLeftSuper)
        .value("RIGHT_SHIFT", KeyboardInput::eRightShift)
        .value("RIGHT_CONTROL", KeyboardInput::eRightControl)
        .value("RIGHT_ALT", KeyboardInput::eRightAlt)
        .value("RIGHT_SUPER", KeyboardInput::eRightSuper)
        .value("MENU", KeyboardInput::eMenu)
        .value("COUNT", KeyboardInput::eCount);

    py::enum_<MouseEventType>(m, "MouseEventType")
        .value("LEFT_BUTTON_DOWN", MouseEventType::eLeftButtonDown)
        .value("LEFT_BUTTON_UP", MouseEventType::eLeftButtonUp)
        .value("MIDDLE_BUTTON_DOWN", MouseEventType::eMiddleButtonDown)
        .value("MIDDLE_BUTTON_UP", MouseEventType::eMiddleButtonUp)
        .value("RIGHT_BUTTON_DOWN", MouseEventType::eRightButtonDown)
        .value("RIGHT_BUTTON_UP", MouseEventType::eRightButtonUp)
        .value("MOVE", MouseEventType::eMove)
        .value("SCROLL", MouseEventType::eScroll);

    py::enum_<MouseInput>(m, "MouseInput")
        .value("LEFT_BUTTON", MouseInput::eLeftButton)
        .value("RIGHT_BUTTON", MouseInput::eRightButton)
        .value("MIDDLE_BUTTON", MouseInput::eMiddleButton)
        .value("FORWARD_BUTTON", MouseInput::eForwardButton)
        .value("BACK_BUTTON", MouseInput::eBackButton)
        .value("SCROLL_RIGHT", MouseInput::eScrollRight)
        .value("SCROLL_LEFT", MouseInput::eScrollLeft)
        .value("SCROLL_UP", MouseInput::eScrollUp)
        .value("SCROLL_DOWN", MouseInput::eScrollDown)
        .value("MOVE_RIGHT", MouseInput::eMoveRight)
        .value("MOVE_LEFT", MouseInput::eMoveLeft)
        .value("MOVE_UP", MouseInput::eMoveUp)
        .value("MOVE_DOWN", MouseInput::eMoveDown)
        .value("COUNT", MouseInput::eCount);

    py::enum_<GamepadInput>(m, "GamepadInput")
        .value("LEFT_STICK_RIGHT", GamepadInput::eLeftStickRight)
        .value("LEFT_STICK_LEFT", GamepadInput::eLeftStickLeft)
        .value("LEFT_STICK_UP", GamepadInput::eLeftStickUp)
        .value("LEFT_STICK_DOWN", GamepadInput::eLeftStickDown)
        .value("RIGHT_STICK_RIGHT", GamepadInput::eRightStickRight)
        .value("RIGHT_STICK_LEFT", GamepadInput::eRightStickLeft)
        .value("RIGHT_STICK_UP", GamepadInput::eRightStickUp)
        .value("RIGHT_STICK_DOWN", GamepadInput::eRightStickDown)
        .value("LEFT_TRIGGER", GamepadInput::eLeftTrigger)
        .value("RIGHT_TRIGGER", GamepadInput::eRightTrigger)
        .value("A", GamepadInput::eA)
        .value("B", GamepadInput::eB)
        .value("X", GamepadInput::eX)
        .value("Y", GamepadInput::eY)
        .value("LEFT_SHOULDER", GamepadInput::eLeftShoulder)
        .value("RIGHT_SHOULDER", GamepadInput::eRightShoulder)
        .value("MENU1", GamepadInput::eMenu1)
        .value("MENU2", GamepadInput::eMenu2)
        .value("LEFT_STICK", GamepadInput::eLeftStick)
        .value("RIGHT_STICK", GamepadInput::eRightStick)
        .value("DPAD_UP", GamepadInput::eDpadUp)
        .value("DPAD_RIGHT", GamepadInput::eDpadRight)
        .value("DPAD_DOWN", GamepadInput::eDpadDown)
        .value("DPAD_LEFT", GamepadInput::eDpadLeft)
        .value("COUNT", GamepadInput::eCount);

    m.attr("BUTTON_FLAG_RELEASED") = py::int_(kButtonFlagTransitionUp);
    m.attr("BUTTON_FLAG_UP") = py::int_(kButtonFlagStateUp);
    m.attr("BUTTON_FLAG_PRESSED") = py::int_(kButtonFlagTransitionDown);
    m.attr("BUTTON_FLAG_DOWN") = py::int_(kButtonFlagStateDown);

    m.attr("KEYBOARD_MODIFIER_FLAG_SHIFT") = py::int_(kKeyboardModifierFlagShift);
    m.attr("KEYBOARD_MODIFIER_FLAG_CONTROL") = py::int_(kKeyboardModifierFlagControl);
    m.attr("KEYBOARD_MODIFIER_FLAG_ALT") = py::int_(kKeyboardModifierFlagAlt);
    m.attr("KEYBOARD_MODIFIER_FLAG_SUPER") = py::int_(kKeyboardModifierFlagSuper);
    m.attr("KEYBOARD_MODIFIER_FLAG_CAPS_LOCK") = py::int_(kKeyboardModifierFlagCapsLock);
    m.attr("KEYBOARD_MODIFIER_FLAG_NUM_LOCK") = py::int_(kKeyboardModifierFlagNumLock);

    py::class_<KeyboardEvent>(m, "KeyboardEvent")
        .def_readonly("device", &KeyboardEvent::device)
        .def_readonly("keyboard", &KeyboardEvent::keyboard)
        .def_readonly("type", &KeyboardEvent::type)
        .def_property_readonly("input",
                               [](const KeyboardEvent& desc) {
                                   switch (desc.type)
                                   {
                                       case KeyboardEventType::eChar:
                                           return pybind11::cast(std::string(
                                               desc.character, strnlen(desc.character, kCharacterMaxNumBytes)));
                                       default:
                                           return pybind11::cast(desc.key);
                                   }
                               })
        .def_readonly("modifiers", &KeyboardEvent::modifiers);

    py::class_<MouseEvent>(m, "MouseEvent")
        .def_readonly("device", &MouseEvent::device)
        .def_readonly("mouse", &MouseEvent::mouse)
        .def_readonly("type", &MouseEvent::type)
        .def_readonly("normalized_coords", &MouseEvent::normalizedCoords)
        .def_readonly("pixel_coords", &MouseEvent::pixelCoords)
        .def_readonly("scrollDelta", &MouseEvent::scrollDelta)
        .def_readonly("modifiers", &MouseEvent::modifiers);

    py::class_<GamepadEvent>(m, "GamepadEvent")
        .def_readonly("device", &GamepadEvent::device)
        .def_readonly("gamepad", &GamepadEvent::gamepad)
        .def_readonly("input", &GamepadEvent::input)
        .def_readonly("value", &GamepadEvent::value);

    py::enum_<GamepadConnectionEventType>(m, "GamepadConnectionEventType")
        .value("CREATED", GamepadConnectionEventType::eCreated)
        .value("CONNECTED", GamepadConnectionEventType::eConnected)
        .value("DISCONNECTED", GamepadConnectionEventType::eDisconnected)
        .value("DESTROYED", GamepadConnectionEventType::eDestroyed);

    py::class_<GamepadConnectionEvent>(m, "GamepadConnectionEvent")
        .def_readonly("type", &GamepadConnectionEvent::type)
        .def_readonly("gamepad", &GamepadConnectionEvent::gamepad)
        .def_readonly("device", &GamepadConnectionEvent::device);

    py::class_<InputEvent>(m, "InputEvent")
        .def_readonly("deviceType", &InputEvent::deviceType)
        .def_readonly("device", &InputEvent::device)
        .def_property_readonly("event", [](const InputEvent& desc) {
            switch (desc.deviceType)
            {
                case DeviceType::eKeyboard:
                    return pybind11::cast(desc.keyboardEvent);
                case DeviceType::eMouse:
                    return pybind11::cast(desc.mouseEvent);
                case DeviceType::eGamepad:
                    return pybind11::cast(desc.gamepadEvent);
                default:
                    return py::cast(nullptr);
            }
        });

    py::class_<ActionMappingDesc>(m, "ActionMappingDesc")
        .def_readonly("deviceType", &ActionMappingDesc::deviceType)
        .def_readonly("modifiers", &ActionMappingDesc::modifiers)
        .def_property_readonly("device",
                               [](const ActionMappingDesc& desc) {
                                   switch (desc.deviceType)
                                   {
                                       case DeviceType::eKeyboard:
                                           return pybind11::cast(desc.keyboard);
                                       case DeviceType::eMouse:
                                           return pybind11::cast(desc.mouse);
                                       case DeviceType::eGamepad:
                                           return pybind11::cast(desc.gamepad);
                                       default:
                                           return py::cast(nullptr);
                                   }
                               })
        .def_property_readonly("input", [](const ActionMappingDesc& desc) {
            switch (desc.deviceType)
            {
                case DeviceType::eKeyboard:
                    return pybind11::cast(desc.keyboardInput);
                case DeviceType::eMouse:
                    return pybind11::cast(desc.mouseInput);
                case DeviceType::eGamepad:
                    return pybind11::cast(desc.gamepadInput);
                default:
                    return py::cast(nullptr);
            }
        });


    py::class_<ActionEvent>(m, "ActionEvent")
        .def_readonly("action", &ActionEvent::action)
        .def_readonly("value", &ActionEvent::value)
        .def_readonly("flags", &ActionEvent::flags);

    m.def("get_action_mapping_desc_from_string", [](const std::string& str) {
        std::string deviceId;
        ActionMappingDesc actionMappingDesc;
        {
            py::gil_scoped_release nogil;
            actionMappingDesc = getActionMappingDescFromString(str.c_str(), &deviceId);
        }
        py::tuple t(4);
        t[0] = actionMappingDesc.deviceType;
        t[1] = actionMappingDesc.modifiers;
        switch (actionMappingDesc.deviceType)
        {
            case DeviceType::eKeyboard:
            {
                t[2] = actionMappingDesc.keyboardInput;
                break;
            }
            case DeviceType::eMouse:
            {
                t[2] = actionMappingDesc.mouseInput;
                break;
            }
            case DeviceType::eGamepad:
            {
                t[2] = actionMappingDesc.gamepadInput;
                break;
            }
            default:
            {
                t[2] = py::none();
                break;
            }
        }
        t[3] = deviceId;
        return t;
    });

    m.def("get_string_from_action_mapping_desc",
          [](KeyboardInput keyboardInput, KeyboardModifierFlags modifiers) {
              ActionMappingDesc actionMappingDesc = {};
              actionMappingDesc.deviceType = DeviceType::eKeyboard;
              actionMappingDesc.keyboardInput = keyboardInput;
              actionMappingDesc.modifiers = modifiers;
              return getStringFromActionMappingDesc(actionMappingDesc, nullptr);
          },
          py::call_guard<py::gil_scoped_release>())
        .def("get_string_from_action_mapping_desc",
             [](MouseInput mouseInput, KeyboardModifierFlags modifiers) {
                 ActionMappingDesc actionMappingDesc = {};
                 actionMappingDesc.deviceType = DeviceType::eMouse;
                 actionMappingDesc.mouseInput = mouseInput;
                 actionMappingDesc.modifiers = modifiers;
                 return getStringFromActionMappingDesc(actionMappingDesc, nullptr);
             },
             py::call_guard<py::gil_scoped_release>())
        .def("get_string_from_action_mapping_desc",
             [](GamepadInput gamepadInput) {
                 ActionMappingDesc actionMappingDesc = {};
                 actionMappingDesc.deviceType = DeviceType::eGamepad;
                 actionMappingDesc.gamepadInput = gamepadInput;
                 actionMappingDesc.modifiers = 0;
                 return getStringFromActionMappingDesc(actionMappingDesc, nullptr);
             },
             py::call_guard<py::gil_scoped_release>());


    static ScriptCallbackRegistryPython<size_t, bool, const InputEvent&> s_inputEventCBs;
    static ScriptCallbackRegistryPython<size_t, bool, const KeyboardEvent&> s_keyboardEventCBs;
    static ScriptCallbackRegistryPython<size_t, bool, const MouseEvent&> s_mouseEventCBs;
    static ScriptCallbackRegistryPython<size_t, bool, const GamepadEvent&> s_gamepadEventCBs;
    static ScriptCallbackRegistryPython<size_t, void, const GamepadConnectionEvent&> s_gamepadConnectionEventCBs;
    static ScriptCallbackRegistryPython<size_t, bool, const ActionEvent&> s_actionEventCBs;

    defineInterfaceClass<IInput>(m, "IInput", "acquire_input_interface")
        .def("get_device_name", wrapInterfaceFunction(&IInput::getDeviceName), py::call_guard<py::gil_scoped_release>())
        .def("get_device_type", wrapInterfaceFunction(&IInput::getDeviceType), py::call_guard<py::gil_scoped_release>())
        .def("subscribe_to_input_events",
             [](IInput* iface, const decltype(s_inputEventCBs)::FuncT& eventFn, EventTypeMask eventTypes,
                InputDevice* device, SubscriptionOrder order) {
                 auto eventFnCopy = s_inputEventCBs.create(eventFn);
                 SubscriptionId id =
                     iface->subscribeToInputEvents(device, eventTypes, s_inputEventCBs.call, eventFnCopy, order);
                 s_inputEventCBs.add(hashPair(0x3e1, id), eventFnCopy);
                 return id;
             },
             py::arg("eventFn"), py::arg("eventTypes") = kEventTypeAll, py::arg("device") = nullptr,
             py::arg("order") = kSubscriptionOrderDefault, py::call_guard<py::gil_scoped_release>())
        .def("unsubscribe_to_input_events",
             [](IInput* iface, SubscriptionId id) {
                 iface->unsubscribeToInputEvents(id);
                 s_inputEventCBs.removeAndDestroy(hashPair(0x3e1, id));
             },
             py::call_guard<py::gil_scoped_release>())
        .def("get_keyboard_name", wrapInterfaceFunction(&IInput::getKeyboardName),
             py::call_guard<py::gil_scoped_release>())
        .def("subscribe_to_keyboard_events",
             [](IInput* iface, Keyboard* keyboard, const decltype(s_keyboardEventCBs)::FuncT& eventFn) {
                 auto eventFnCopy = s_keyboardEventCBs.create(eventFn);
                 SubscriptionId id = iface->subscribeToKeyboardEvents(keyboard, s_keyboardEventCBs.call, eventFnCopy);
                 s_keyboardEventCBs.add(hashPair(keyboard, id), eventFnCopy);
                 return id;
             },
             py::call_guard<py::gil_scoped_release>())
        .def("unsubscribe_to_keyboard_events",
             [](IInput* iface, Keyboard* keyboard, SubscriptionId id) {
                 iface->unsubscribeToKeyboardEvents(keyboard, id);
                 s_keyboardEventCBs.removeAndDestroy(hashPair(keyboard, id));
             },
             py::call_guard<py::gil_scoped_release>())
        .def("get_keyboard_value", wrapInterfaceFunction(&IInput::getKeyboardValue),
             py::call_guard<py::gil_scoped_release>())
        .def("get_keyboard_button_flags", wrapInterfaceFunction(&IInput::getKeyboardButtonFlags),
             py::call_guard<py::gil_scoped_release>())
        .def("get_mouse_name", wrapInterfaceFunction(&IInput::getMouseName), py::call_guard<py::gil_scoped_release>())
        .def("get_mouse_value", wrapInterfaceFunction(&IInput::getMouseValue), py::call_guard<py::gil_scoped_release>())
        .def("get_mouse_button_flags", wrapInterfaceFunction(&IInput::getMouseButtonFlags),
             py::call_guard<py::gil_scoped_release>())
        .def("get_mouse_coords_normalized", wrapInterfaceFunction(&IInput::getMouseCoordsNormalized),
             py::call_guard<py::gil_scoped_release>())
        .def("get_mouse_coords_pixel", wrapInterfaceFunction(&IInput::getMouseCoordsPixel),
             py::call_guard<py::gil_scoped_release>())
        .def("subscribe_to_mouse_events",
             [](IInput* iface, Mouse* mouse, const decltype(s_mouseEventCBs)::FuncT& eventFn) {
                 auto eventFnCopy = s_mouseEventCBs.create(eventFn);
                 SubscriptionId id = iface->subscribeToMouseEvents(mouse, s_mouseEventCBs.call, eventFnCopy);
                 s_mouseEventCBs.add(hashPair(mouse, id), eventFnCopy);
                 return id;
             },
             py::call_guard<py::gil_scoped_release>())
        .def("unsubscribe_to_mouse_events",
             [](IInput* iface, Mouse* mouse, SubscriptionId id) {
                 iface->unsubscribeToMouseEvents(mouse, id);
                 s_mouseEventCBs.removeAndDestroy(hashPair(mouse, id));
             },
             py::call_guard<py::gil_scoped_release>())
        .def("get_gamepad_name", wrapInterfaceFunction(&IInput::getGamepadName), py::call_guard<py::gil_scoped_release>())
        .def("get_gamepad_guid", wrapInterfaceFunction(&IInput::getGamepadGuid), py::call_guard<py::gil_scoped_release>())
        .def("get_gamepad_value", wrapInterfaceFunction(&IInput::getGamepadValue),
             py::call_guard<py::gil_scoped_release>())
        .def("get_gamepad_button_flags", wrapInterfaceFunction(&IInput::getGamepadButtonFlags),
             py::call_guard<py::gil_scoped_release>())
        .def("subscribe_to_gamepad_events",
             [](IInput* iface, Gamepad* gamepad, const decltype(s_gamepadEventCBs)::FuncT& eventFn) {
                 auto eventFnCopy = s_gamepadEventCBs.create(eventFn);
                 SubscriptionId id = iface->subscribeToGamepadEvents(gamepad, s_gamepadEventCBs.call, eventFnCopy);
                 s_gamepadEventCBs.add(hashPair(gamepad, id), eventFnCopy);
                 return id;
             },
             py::call_guard<py::gil_scoped_release>())
        .def("unsubscribe_to_gamepad_events",
             [](IInput* iface, Gamepad* gamepad, SubscriptionId id) {
                 iface->unsubscribeToGamepadEvents(gamepad, id);
                 s_gamepadEventCBs.removeAndDestroy(hashPair(gamepad, id));
             },
             py::call_guard<py::gil_scoped_release>())
        .def("subscribe_to_gamepad_connection_events",
             [](IInput* iface, const decltype(s_gamepadConnectionEventCBs)::FuncT& eventFn) {
                 auto eventFnCopy = s_gamepadConnectionEventCBs.create(eventFn);
                 SubscriptionId id =
                     iface->subscribeToGamepadConnectionEvents(s_gamepadConnectionEventCBs.call, eventFnCopy);
                 s_gamepadConnectionEventCBs.add(id, eventFnCopy);
                 return id;
             },
             py::call_guard<py::gil_scoped_release>())
        .def("unsubscribe_to_gamepad_connection_events",
             [](IInput* iface, SubscriptionId id) {
                 iface->unsubscribeToGamepadConnectionEvents(id);
                 s_gamepadConnectionEventCBs.removeAndDestroy(id);
             },
             py::call_guard<py::gil_scoped_release>())
        .def("get_actions",
             [](const IInput* iface, ActionMappingSet* actionMappingSet) {
                 std::vector<std::string> res(iface->getActionCount(actionMappingSet));
                 auto actions = iface->getActions(actionMappingSet);
                 for (size_t i = 0; i < res.size(); i++)
                 {
                     res[i] = actions[i];
                 }
                 return res;
             },
             py::call_guard<py::gil_scoped_release>())
        .def("add_action_mapping",
             [](IInput* iface, ActionMappingSet* actionMappingSet, const char* action, Keyboard* keyboard,
                KeyboardInput keyboardInput, KeyboardModifierFlags modifiers) {
                 return iface->addActionMapping(
                     actionMappingSet, action,
                     ActionMappingDesc{ DeviceType::eKeyboard, { keyboard }, { keyboardInput }, modifiers });
             },
             py::call_guard<py::gil_scoped_release>())
        .def("add_action_mapping",
             [](IInput* iface, ActionMappingSet* actionMappingSet, const char* action, Gamepad* gamepad,
                GamepadInput gamepadInput) {
                 return iface->addActionMapping(
                     actionMappingSet, action, detail::toGamepadMapping(gamepad, gamepadInput));
             },
             py::call_guard<py::gil_scoped_release>())
        .def("add_action_mapping",
             [](IInput* iface, ActionMappingSet* actionMappingSet, const char* action, Mouse* mouse,
                MouseInput mouseInput, KeyboardModifierFlags modifiers) {
                 return iface->addActionMapping(
                     actionMappingSet, action, detail::toMouseMapping(mouse, mouseInput, modifiers));
             },
             py::call_guard<py::gil_scoped_release>())
        .def("set_action_mapping",
             [](IInput* iface, ActionMappingSet* actionMappingSet, const char* action, uint32_t index,
                Keyboard* keyboard, KeyboardInput keyboardInput, KeyboardModifierFlags modifiers) {
                 return iface->setActionMapping(
                     actionMappingSet, action, index,
                     ActionMappingDesc{ DeviceType::eKeyboard, { keyboard }, { keyboardInput }, modifiers });
             },
             py::call_guard<py::gil_scoped_release>())
        .def("set_action_mapping",
             [](IInput* iface, ActionMappingSet* actionMappingSet, const char* action, uint32_t index, Gamepad* gamepad,
                GamepadInput gamepadInput) {
                 return iface->setActionMapping(
                     actionMappingSet, action, index, detail::toGamepadMapping(gamepad, gamepadInput));
             },
             py::call_guard<py::gil_scoped_release>())
        .def("set_action_mapping",
             [](IInput* iface, ActionMappingSet* actionMappingSet, const char* action, uint32_t index, Mouse* mouse,
                MouseInput mouseInput, KeyboardModifierFlags modifiers) {
                 return iface->setActionMapping(
                     actionMappingSet, action, index, detail::toMouseMapping(mouse, mouseInput, modifiers));
             },
             py::call_guard<py::gil_scoped_release>())
        .def("remove_action_mapping", wrapInterfaceFunction(&IInput::removeActionMapping),
             py::call_guard<py::gil_scoped_release>())
        .def("clear_action_mappings", wrapInterfaceFunction(&IInput::clearActionMappings),
             py::call_guard<py::gil_scoped_release>())
        .def("get_action_mappings",
             [](const IInput* iface, ActionMappingSet* actionMappingSet, const char* action) {
                 auto size = iface->getActionMappingCount(actionMappingSet, action);
                 std::vector<ActionMappingDesc> res;
                 res.reserve(size);
                 auto mappings = iface->getActionMappings(actionMappingSet, action);
                 std::copy(mappings, mappings + size, std::back_inserter(res));
                 return res;
             },
             py::call_guard<py::gil_scoped_release>())
        .def("get_action_mapping_count", wrapInterfaceFunction(&IInput::getActionMappingCount),
             py::call_guard<py::gil_scoped_release>())

        .def("set_default_action_mapping",
             [](IInput* iface, ActionMappingSet* actionMappingSet, const char* action, Keyboard* keyboard,
                KeyboardInput keyboardInput, KeyboardModifierFlags modifiers) {
                 return setDefaultActionMapping(
                     iface, actionMappingSet, action,
                     ActionMappingDesc{ DeviceType::eKeyboard, { keyboard }, { keyboardInput }, modifiers });
             },
             py::call_guard<py::gil_scoped_release>())
        .def("set_default_action_mapping",
             [](IInput* iface, ActionMappingSet* actionMappingSet, const char* action, Gamepad* gamepad,
                GamepadInput gamepadInput) {
                 return setDefaultActionMapping(
                     iface, actionMappingSet, action, detail::toGamepadMapping(gamepad, gamepadInput));
             },
             py::call_guard<py::gil_scoped_release>())
        .def("set_default_action_mapping",
             [](IInput* iface, ActionMappingSet* actionMappingSet, const char* action, Mouse* mouse,
                MouseInput mouseInput, KeyboardModifierFlags modifiers) {
                 return setDefaultActionMapping(
                     iface, actionMappingSet, action, detail::toMouseMapping(mouse, mouseInput, modifiers));
             },
             py::call_guard<py::gil_scoped_release>())

        .def("get_action_value", wrapInterfaceFunction(&IInput::getActionValue), py::call_guard<py::gil_scoped_release>())
        .def("get_action_button_flags", wrapInterfaceFunction(&IInput::getActionButtonFlags),
             py::call_guard<py::gil_scoped_release>())
        .def("subscribe_to_action_events",
             [](IInput* iface, ActionMappingSet* actionMappingSet, const char* action,
                const decltype(s_actionEventCBs)::FuncT& eventFn) {
                 auto eventFnCopy = s_actionEventCBs.create(eventFn);
                 SubscriptionId id =
                     iface->subscribeToActionEvents(actionMappingSet, action, s_actionEventCBs.call, eventFnCopy);
                 s_actionEventCBs.add(id, eventFnCopy);
                 return id;
             },
             py::call_guard<py::gil_scoped_release>())
        .def("unsubscribe_to_action_events",
             [](IInput* iface, SubscriptionId id) {
                 iface->unsubscribeToActionEvents(id);
                 s_actionEventCBs.removeAndDestroy(id);
             },
             py::call_guard<py::gil_scoped_release>())
        .def("get_action_mapping_set_by_path", wrapInterfaceFunction(&IInput::getActionMappingSetByPath),
             py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>());

    m.def("acquire_input_provider",
          [](const char* pluginName, const char* libraryPath) {
              return libraryPath ? acquireInterfaceFromLibraryForBindings<IInput>(libraryPath)->getInputProvider() :
                                   acquireInterfaceForBindings<IInput>(pluginName)->getInputProvider();
          },
          py::arg("plugin_name") = nullptr, py::arg("library_path") = nullptr, py::return_value_policy::reference,
          py::call_guard<py::gil_scoped_release>());

    py::class_<InputProvider>(m, "InputProvider")
        .def("create_keyboard", wrapInterfaceFunction(&InputProvider::createKeyboard),
             py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>())
        .def("destroy_keyboard", wrapInterfaceFunction(&InputProvider::destroyKeyboard),
             py::call_guard<py::gil_scoped_release>())
        .def("update_keyboard", wrapInterfaceFunction(&InputProvider::updateKeyboard),
             py::call_guard<py::gil_scoped_release>())
        .def("buffer_keyboard_key_event",
             [](InputProvider* iface, Keyboard* keyboard, KeyboardEventType type, KeyboardInput key,
                KeyboardModifierFlags modifiers) {
                 KeyboardEvent event;
                 event.keyboard = keyboard;
                 event.type = type;
                 event.key = key;
                 event.modifiers = modifiers;
                 iface->bufferKeyboardEvent(event);
             },
             py::call_guard<py::gil_scoped_release>())
        .def("buffer_keyboard_char_event",
             [](InputProvider* iface, Keyboard* keyboard, py::str character, KeyboardModifierFlags modifiers) {
                 // Cast before releasing GIL
                 auto characterStr = character.cast<std::string>();

                 py::gil_scoped_release nogil;
                 KeyboardEvent event{};
                 event.keyboard = keyboard;
                 event.type = KeyboardEventType::eChar;
                 event.modifiers = modifiers;

                 size_t maxCopyBytes = ::carb_min(characterStr.length(), size_t(kCharacterMaxNumBytes));
                 memcpy((void*)event.character, characterStr.c_str(), maxCopyBytes);
                 iface->bufferKeyboardEvent(event);
             })
        .def("create_mouse", wrapInterfaceFunction(&InputProvider::createMouse), py::return_value_policy::reference,
             py::call_guard<py::gil_scoped_release>())
        .def("destroy_mouse", wrapInterfaceFunction(&InputProvider::destroyMouse),
             py::call_guard<py::gil_scoped_release>())
        .def("update_mouse", wrapInterfaceFunction(&InputProvider::updateMouse), py::call_guard<py::gil_scoped_release>())
        .def("buffer_mouse_event",
             [](InputProvider* iface, Mouse* mouse, MouseEventType type, Float2 value, KeyboardModifierFlags modifiers,
                Float2 pixelValue) {
                 MouseEvent event;
                 event.mouse = mouse;
                 event.type = type;
                 if (type == MouseEventType::eScroll)
                 {
                     event.scrollDelta = value;
                 }
                 else
                 {
                     event.normalizedCoords = value;
                 }
                 event.pixelCoords = pixelValue;
                 event.modifiers = modifiers;
                 iface->bufferMouseEvent(event);
             },
             py::call_guard<py::gil_scoped_release>())
        .def("create_gamepad", wrapInterfaceFunction(&InputProvider::createGamepad), py::return_value_policy::reference,
             py::call_guard<py::gil_scoped_release>())
        .def("set_gamepad_connected", wrapInterfaceFunction(&InputProvider::setGamepadConnected),
             py::call_guard<py::gil_scoped_release>())
        .def("destroy_gamepad", wrapInterfaceFunction(&InputProvider::destroyGamepad),
             py::call_guard<py::gil_scoped_release>())
        .def("update_gamepad", wrapInterfaceFunction(&InputProvider::updateGamepad),
             py::call_guard<py::gil_scoped_release>())
        .def("buffer_gamepad_event",
             [](InputProvider* iface, Gamepad* gamepad, GamepadInput input, float value) {
                 GamepadEvent event;
                 event.gamepad = gamepad;
                 event.input = input;
                 event.value = value;
                 iface->bufferGamepadEvent(event);
             },
             py::call_guard<py::gil_scoped_release>());
}
} // namespace input
} // namespace carb
