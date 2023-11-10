// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once
#include "../logging/Log.h"
#include "IInput.h"

#include <map>
#include <string>
#include <cstring>
#include <functional>


namespace carb
{
namespace input
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                  Name Mapping                                                      //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail
{

template <typename Key, typename LessFn, typename ExtractKeyFn, typename StaticMappingDesc, size_t count>
const StaticMappingDesc* getMappingByKey(Key key, const StaticMappingDesc (&items)[count])
{
    static std::map<Key, const StaticMappingDesc*, LessFn> s_mapping;
    static bool s_isInitialized = false;

    if (!s_isInitialized)
    {
        for (size_t i = 0; i < count; ++i)
        {
            s_mapping.insert(std::make_pair(ExtractKeyFn().operator()(items[i]), &items[i]));
        }

        s_isInitialized = true;
    }

    auto found = s_mapping.find(key);
    if (found != s_mapping.end())
    {
        return found->second;
    }

    return nullptr;
}

template <typename T>
struct Less
{
    bool operator()(const T& a, const T& b) const
    {
        return std::less<T>(a, b);
    }
};

template <>
struct Less<const char*>
{
    bool operator()(const char* a, const char* b) const
    {
        return std::strcmp(a, b) < 0;
    }
};

template <typename Struct, typename String>
struct ExtractName
{
    String operator()(const Struct& item) const
    {
        return item.name;
    }
};

template <typename Struct, typename Ident>
struct ExtractIdent
{
    Ident operator()(const Struct& item) const
    {
        return item.ident;
    }
};

template <typename Ident, typename String, typename StaticMappingDesc, size_t count>
Ident getIdentByName(String name, const StaticMappingDesc (&items)[count], Ident defaultIdent)
{
    using LessFn = Less<String>;
    using ExtractNameFn = ExtractName<StaticMappingDesc, String>;
    using ExtractIdentFn = ExtractIdent<StaticMappingDesc, Ident>;
    const auto* item = getMappingByKey<String, LessFn, ExtractNameFn, StaticMappingDesc, count>(name, items);
    return (item != nullptr) ? ExtractIdentFn().operator()(*item) : defaultIdent;
}

template <typename String, typename Ident, typename StaticMappingDesc, size_t count>
String getNameByIdent(Ident ident, const StaticMappingDesc (&items)[count], String defaultName)
{
    using LessFn = std::less<Ident>;
    using ExtractIdentFn = ExtractIdent<StaticMappingDesc, Ident>;
    using ExtractNameFn = ExtractName<StaticMappingDesc, String>;
    const auto* item = getMappingByKey<Ident, LessFn, ExtractIdentFn, StaticMappingDesc, count>(ident, items);
    return (item != nullptr) ? ExtractNameFn().operator()(*item) : defaultName;
}
} // namespace detail


static constexpr struct
{
    DeviceType ident;
    const char* name;
} g_deviceTypeToName[] = {
    // clang-format off
    { DeviceType::eKeyboard, "Keyboard" },
    { DeviceType::eMouse, "Mouse" },
    { DeviceType::eGamepad, "Gamepad" }
    // clang-format on
};

inline const char* getDeviceTypeString(DeviceType deviceType)
{
    using namespace detail;
    return getNameByIdent(deviceType, g_deviceTypeToName, "Unknown");
}

inline DeviceType getDeviceTypeFromString(const char* deviceTypeString)
{
    using namespace detail;
    return getIdentByName(deviceTypeString, g_deviceTypeToName, DeviceType::eUnknown);
}


static constexpr struct
{
    KeyboardInput ident;
    const char* name;
} kKeyboardInputCodeName[] = {
    // clang-format off
    { KeyboardInput::eUnknown, "Unknown" },
    { KeyboardInput::eSpace, "Space" },
    { KeyboardInput::eApostrophe, "'" },
    { KeyboardInput::eComma, "," },
    { KeyboardInput::eMinus, "-" },
    { KeyboardInput::ePeriod, "." },
    { KeyboardInput::eSlash, "/" },
    { KeyboardInput::eKey0, "0" },
    { KeyboardInput::eKey1, "1" },
    { KeyboardInput::eKey2, "2" },
    { KeyboardInput::eKey3, "3" },
    { KeyboardInput::eKey4, "4" },
    { KeyboardInput::eKey5, "5" },
    { KeyboardInput::eKey6, "6" },
    { KeyboardInput::eKey7, "7" },
    { KeyboardInput::eKey8, "8" },
    { KeyboardInput::eKey9, "9" },
    { KeyboardInput::eSemicolon, ";" },
    { KeyboardInput::eEqual, "=" },
    { KeyboardInput::eA, "A" },
    { KeyboardInput::eB, "B" },
    { KeyboardInput::eC, "C" },
    { KeyboardInput::eD, "D" },
    { KeyboardInput::eE, "E" },
    { KeyboardInput::eF, "F" },
    { KeyboardInput::eG, "G" },
    { KeyboardInput::eH, "H" },
    { KeyboardInput::eI, "I" },
    { KeyboardInput::eJ, "J" },
    { KeyboardInput::eK, "K" },
    { KeyboardInput::eL, "L" },
    { KeyboardInput::eM, "M" },
    { KeyboardInput::eN, "N" },
    { KeyboardInput::eO, "O" },
    { KeyboardInput::eP, "P" },
    { KeyboardInput::eQ, "Q" },
    { KeyboardInput::eR, "R" },
    { KeyboardInput::eS, "S" },
    { KeyboardInput::eT, "T" },
    { KeyboardInput::eU, "U" },
    { KeyboardInput::eV, "V" },
    { KeyboardInput::eW, "W" },
    { KeyboardInput::eX, "X" },
    { KeyboardInput::eY, "Y" },
    { KeyboardInput::eZ, "Z" },
    { KeyboardInput::eLeftBracket, "[" },
    { KeyboardInput::eBackslash, "\\" },
    { KeyboardInput::eRightBracket, "]" },
    { KeyboardInput::eGraveAccent, "`" },
    { KeyboardInput::eEscape, "Esc" },
    { KeyboardInput::eTab, "Tab" },
    { KeyboardInput::eEnter, "Enter" },
    { KeyboardInput::eBackspace, "Backspace" },
    { KeyboardInput::eInsert, "Insert" },
    { KeyboardInput::eDel, "Del" },
    { KeyboardInput::eRight, "Right" },
    { KeyboardInput::eLeft, "Left" },
    { KeyboardInput::eDown, "Down" },
    { KeyboardInput::eUp, "Up" },
    { KeyboardInput::ePageUp, "PageUp" },
    { KeyboardInput::ePageDown, "PageDown" },
    { KeyboardInput::eHome, "Home" },
    { KeyboardInput::eEnd, "End" },
    { KeyboardInput::eCapsLock, "CapsLock" },
    { KeyboardInput::eScrollLock, "ScrollLock" },
    { KeyboardInput::eNumLock, "NumLock" },
    { KeyboardInput::ePrintScreen, "PrintScreen" },
    { KeyboardInput::ePause, "Pause" },
    { KeyboardInput::eF1, "F1" },
    { KeyboardInput::eF2, "F2" },
    { KeyboardInput::eF3, "F3" },
    { KeyboardInput::eF4, "F4" },
    { KeyboardInput::eF5, "F5" },
    { KeyboardInput::eF6, "F6" },
    { KeyboardInput::eF7, "F7" },
    { KeyboardInput::eF8, "F8" },
    { KeyboardInput::eF9, "F9" },
    { KeyboardInput::eF10, "F10" },
    { KeyboardInput::eF11, "F11" },
    { KeyboardInput::eF12, "F12" },
    { KeyboardInput::eNumpad0, "Num0" },
    { KeyboardInput::eNumpad1, "Num1" },
    { KeyboardInput::eNumpad2, "Num2" },
    { KeyboardInput::eNumpad3, "Num3" },
    { KeyboardInput::eNumpad4, "Num4" },
    { KeyboardInput::eNumpad5, "Num5" },
    { KeyboardInput::eNumpad6, "Num6" },
    { KeyboardInput::eNumpad7, "Num7" },
    { KeyboardInput::eNumpad8, "Num8" },
    { KeyboardInput::eNumpad9, "Num9" },
    { KeyboardInput::eNumpadDel, "NumDel" },
    { KeyboardInput::eNumpadDivide, "NumDivide" },
    { KeyboardInput::eNumpadMultiply, "NumMultiply" },
    { KeyboardInput::eNumpadSubtract, "NumSubtract" },
    { KeyboardInput::eNumpadAdd, "NumAdd" },
    { KeyboardInput::eNumpadEnter, "NumEnter" },
    { KeyboardInput::eNumpadEqual, "NumEqual" },
    { KeyboardInput::eLeftShift, "LeftShift" },
    { KeyboardInput::eLeftControl, "LeftControl" },
    { KeyboardInput::eLeftAlt, "LeftAlt" },
    { KeyboardInput::eLeftSuper, "LeftSuper" },
    { KeyboardInput::eRightShift, "RightShift" },
    { KeyboardInput::eRightControl, "RightControl" },
    { KeyboardInput::eRightAlt, "RightAlt" },
    { KeyboardInput::eRightSuper, "RightSuper" },
    { KeyboardInput::eMenu, "Menu" }
    // clang-format on
};

inline const char* getKeyboardInputString(KeyboardInput key)
{
    using namespace detail;
    return getNameByIdent(key, kKeyboardInputCodeName, "");
}

inline KeyboardInput getKeyboardInputFromString(const char* inputString)
{
    using namespace detail;
    return getIdentByName(inputString, kKeyboardInputCodeName, KeyboardInput::eUnknown);
}


static constexpr struct
{
    KeyboardModifierFlags ident;
    const char* name;
} kModifierFlagName[] = {
    // clang-format off
    { kKeyboardModifierFlagShift, "Shift" },
    { kKeyboardModifierFlagControl, "Ctrl" },
    { kKeyboardModifierFlagAlt, "Alt" },
    { kKeyboardModifierFlagSuper, "Super" },
    { kKeyboardModifierFlagCapsLock, "CapsLock" },
    { kKeyboardModifierFlagNumLock, "NumLock" }
    // clang-format on
};

inline const char* getModifierFlagString(KeyboardModifierFlags flag)
{
    using namespace detail;
    return getNameByIdent(flag, kModifierFlagName, "");
}

inline KeyboardModifierFlags getModifierFlagFromString(const char* inputString)
{
    using namespace detail;
    return getIdentByName(inputString, kModifierFlagName, 0);
}


const char kDeviceNameSeparator[] = "::";
const char kModifierSeparator[] = " + ";

inline std::string getModifierFlagsString(KeyboardModifierFlags mod)
{
    std::string res = "";

    for (const auto& desc : kModifierFlagName)
    {
        const auto& flag = desc.ident;

        if ((mod & flag) != flag)
            continue;

        if (!res.empty())
            res += kModifierSeparator;

        res += desc.name;
    }

    return res;
}

inline KeyboardModifierFlags getModifierFlagsFromString(const char* modString)
{
    KeyboardModifierFlags res = KeyboardModifierFlags(0);

    const size_t kModifierSeparatorSize = strlen(kModifierSeparator);

    std::string modifierNameString;
    const char* modifierName = modString;
    while (true)
    {
        const char* modifierNameEnd = strstr(modifierName, kModifierSeparator);

        if (modifierNameEnd)
        {
            modifierNameString = std::string(modifierName, modifierNameEnd - modifierName);
        }
        else
        {
            modifierNameString = std::string(modifierName);
        }

        KeyboardModifierFlags mod = getModifierFlagFromString(modifierNameString.c_str());
        if (mod)
        {
            res = (KeyboardModifierFlags)((uint32_t)res | (uint32_t)mod);
        }
        else
        {
            CARB_LOG_VERBOSE("Unknown hotkey modifier encountered: %s in %s", modifierNameString.c_str(), modString);
        }

        if (!modifierNameEnd)
        {
            break;
        }
        modifierName = modifierNameEnd;
        modifierName += kModifierSeparatorSize;
    }

    return res;
}


static constexpr struct
{
    MouseInput ident;
    const char* name;
} kMouseInputCodeName[] = {
    // clang-format off
    { MouseInput::eLeftButton, "LeftButton" },
    { MouseInput::eRightButton, "RightButton" },
    { MouseInput::eMiddleButton, "MiddleButton" },
    { MouseInput::eForwardButton, "ForwardButton" },
    { MouseInput::eBackButton, "BackButton" },
    { MouseInput::eScrollRight, "ScrollRight" },
    { MouseInput::eScrollLeft, "ScrollLeft" },
    { MouseInput::eScrollUp, "ScrollUp" },
    { MouseInput::eScrollDown, "ScrollDown" },
    { MouseInput::eMoveRight, "MoveRight" },
    { MouseInput::eMoveLeft, "MoveLeft" },
    { MouseInput::eMoveUp, "MoveUp" },
    { MouseInput::eMoveDown, "MoveDown" }
    // clang-format on
};

inline const char* getMouseInputString(MouseInput key)
{
    using namespace detail;
    return getNameByIdent(key, kMouseInputCodeName, "");
}

inline MouseInput getMouseInputFromString(const char* inputString)
{
    using namespace detail;
    return getIdentByName(inputString, kMouseInputCodeName, MouseInput::eCount);
}


static constexpr struct
{
    GamepadInput ident;
    const char* name;
} kGamepadInputCodeName[] = {
    // clang-format off
    { GamepadInput::eLeftStickRight, "LeftStickRight" },
    { GamepadInput::eLeftStickLeft, "LeftStickLeft" },
    { GamepadInput::eLeftStickUp, "LeftStickUp" },
    { GamepadInput::eLeftStickDown, "LeftStickDown" },
    { GamepadInput::eRightStickRight, "RightStickRight" },
    { GamepadInput::eRightStickLeft, "RightStickLeft" },
    { GamepadInput::eRightStickUp, "RightStickUp" },
    { GamepadInput::eRightStickDown, "RightStickDown" },
    { GamepadInput::eLeftTrigger, "LeftTrigger" },
    { GamepadInput::eRightTrigger, "RightTrigger" },
    { GamepadInput::eA, "ButtonA" },
    { GamepadInput::eB, "ButtonB" },
    { GamepadInput::eX, "ButtonX" },
    { GamepadInput::eY, "ButtonY" },
    { GamepadInput::eLeftShoulder, "LeftShoulder" },
    { GamepadInput::eRightShoulder, "RightShoulder" },
    { GamepadInput::eMenu1, "Menu1" },
    { GamepadInput::eMenu2, "Menu2" },
    { GamepadInput::eLeftStick, "LeftStick" },
    { GamepadInput::eRightStick, "RightStick" },
    { GamepadInput::eDpadUp, "DpadUp" },
    { GamepadInput::eDpadRight, "DpadRight" },
    { GamepadInput::eDpadDown, "DpadDown" },
    { GamepadInput::eDpadLeft, "DpadLeft" }
    // clang-format on
};

inline const char* getGamepadInputString(GamepadInput key)
{
    using namespace detail;
    return getNameByIdent(key, kGamepadInputCodeName, "");
}

inline GamepadInput getGamepadInputFromString(const char* inputString)
{
    using namespace detail;
    return getIdentByName(inputString, kGamepadInputCodeName, GamepadInput::eCount);
}


enum class PreviousButtonState
{
    kUp,
    kDown
};

inline PreviousButtonState toPreviousButtonState(bool wasDown)
{
    return wasDown ? PreviousButtonState::kDown : PreviousButtonState::kUp;
}

enum class CurrentButtonState
{
    kUp,
    kDown
};

inline CurrentButtonState toCurrentButtonState(bool isDown)
{
    return isDown ? CurrentButtonState::kDown : CurrentButtonState::kUp;
}

inline ButtonFlags toButtonFlags(PreviousButtonState previousButtonState, CurrentButtonState currentButtonState)
{
    ButtonFlags flags = 0;
    if (currentButtonState == CurrentButtonState::kDown)
    {
        flags = kButtonFlagStateDown;
        if (previousButtonState == PreviousButtonState::kUp)
            flags |= kButtonFlagTransitionDown;
    }
    else
    {
        flags = kButtonFlagStateUp;
        if (previousButtonState == PreviousButtonState::kDown)
            flags |= kButtonFlagTransitionUp;
    }
    return flags;
}

inline std::string getDeviceNameString(DeviceType deviceType, const char* deviceId)
{
    if ((size_t)deviceType >= (size_t)DeviceType::eCount)
        return "";

    std::string result = getDeviceTypeString(deviceType);
    if (deviceId)
    {
        result.append("[");
        result.append(deviceId);
        result.append("]");
    }
    return result;
}

inline void parseDeviceNameString(const char* deviceName, DeviceType* deviceType, std::string* deviceId)
{
    if (!deviceName)
    {
        CARB_LOG_WARN("parseDeviceNameString: Empty device name");
        if (deviceType)
        {
            *deviceType = DeviceType::eCount;
        }
        return;
    }

    const char* deviceIdString = strstr(deviceName, "[");
    if (deviceType)
    {
        if (deviceIdString)
        {
            std::string deviceTypeString(deviceName, deviceIdString - deviceName);
            *deviceType = getDeviceTypeFromString(deviceTypeString.c_str());
        }
        else
        {
            *deviceType = getDeviceTypeFromString(deviceName);
        }
    }

    if (deviceId)
    {
        if (deviceIdString)
        {
            const char* deviceNameEnd = deviceIdString + strlen(deviceIdString);
            *deviceId = std::string(deviceIdString + 1, deviceNameEnd - deviceIdString - 2);
        }
        else
        {
            *deviceId = "";
        }
    }
}

inline bool getDeviceInputFromString(const char* deviceInputString,
                                     DeviceType* deviceTypeOut,
                                     KeyboardInput* keyboardInputOut,
                                     MouseInput* mouseInputOut,
                                     GamepadInput* gamepadInputOut,
                                     std::string* deviceIdOut = nullptr)
{
    if (!deviceTypeOut)
        return false;

    const char* deviceInputStringTrimmed = deviceInputString;

    // Skip initial spaces
    while (*deviceInputStringTrimmed == ' ')
        ++deviceInputStringTrimmed;

    // Skip device name
    const char* inputNameString = strstr(deviceInputStringTrimmed, kDeviceNameSeparator);

    std::string deviceName;

    // No device name specified - fall back
    if (!inputNameString)
        inputNameString = deviceInputStringTrimmed;
    else
    {
        deviceName = std::string(deviceInputStringTrimmed, inputNameString - deviceInputStringTrimmed);

        const size_t kDeviceNameSeparatorLen = strlen(kDeviceNameSeparator);
        inputNameString += kDeviceNameSeparatorLen;
    }

    parseDeviceNameString(deviceName.c_str(), deviceTypeOut, deviceIdOut);

    if ((*deviceTypeOut == DeviceType::eKeyboard) && keyboardInputOut)
    {
        KeyboardInput keyboardInput = getKeyboardInputFromString(inputNameString);
        *keyboardInputOut = keyboardInput;
        return (keyboardInput != KeyboardInput::eCount);
    }

    if ((*deviceTypeOut == DeviceType::eMouse) && mouseInputOut)
    {
        MouseInput mouseInput = getMouseInputFromString(inputNameString);
        *mouseInputOut = mouseInput;
        return (mouseInput != MouseInput::eCount);
    }

    if ((*deviceTypeOut == DeviceType::eGamepad) && gamepadInputOut)
    {
        GamepadInput gamepadInput = getGamepadInputFromString(inputNameString);
        *gamepadInputOut = gamepadInput;
        return (gamepadInput != GamepadInput::eCount);
    }

    return false;
}

inline ActionMappingDesc getActionMappingDescFromString(const char* hotkeyString, std::string* deviceId)
{
    const size_t kModifierSeparatorSize = strlen(kModifierSeparator);

    ActionMappingDesc actionMappingDesc;
    actionMappingDesc.keyboard = nullptr;
    actionMappingDesc.mouse = nullptr;
    actionMappingDesc.gamepad = nullptr;
    actionMappingDesc.modifiers = (KeyboardModifierFlags)0;

    std::string modifierNameString;
    const char* modifierName = hotkeyString;
    while (true)
    {
        const char* modifierNameEnd = strstr(modifierName, kModifierSeparator);

        if (modifierNameEnd)
        {
            modifierNameString = std::string(modifierName, modifierNameEnd - modifierName);
        }
        else
        {
            modifierNameString = std::string(modifierName);
        }

        KeyboardModifierFlags mod = getModifierFlagFromString(modifierNameString.c_str());
        if (mod)
        {
            actionMappingDesc.modifiers = (KeyboardModifierFlags)((uint32_t)actionMappingDesc.modifiers | (uint32_t)mod);
        }
        else
        {
            getDeviceInputFromString(modifierNameString.c_str(), &actionMappingDesc.deviceType,
                                     &actionMappingDesc.keyboardInput, &actionMappingDesc.mouseInput,
                                     &actionMappingDesc.gamepadInput, deviceId);
        }

        if (!modifierNameEnd)
        {
            break;
        }
        modifierName = modifierNameEnd;
        modifierName += kModifierSeparatorSize;
    }

    return actionMappingDesc;
}

inline std::string getStringFromActionMappingDesc(const ActionMappingDesc& actionMappingDesc,
                                                  const char* deviceName = nullptr)
{
    std::string result = getModifierFlagsString(actionMappingDesc.modifiers);
    if (!result.empty())
    {
        result.append(kModifierSeparator);
    }

    if (deviceName)
    {
        result.append(deviceName);
    }
    else
    {
        result.append(getDeviceTypeString(actionMappingDesc.deviceType));
    }
    result.append(kDeviceNameSeparator);

    switch (actionMappingDesc.deviceType)
    {
        case DeviceType::eKeyboard:
        {
            result.append(getKeyboardInputString(actionMappingDesc.keyboardInput));
            break;
        }
        case DeviceType::eMouse:
        {
            result.append(getMouseInputString(actionMappingDesc.mouseInput));
            break;
        }
        case DeviceType::eGamepad:
        {
            result.append(getGamepadInputString(actionMappingDesc.gamepadInput));
            break;
        }
        default:
        {
            break;
        }
    }
    return result;
}

inline bool setDefaultActionMapping(IInput* input,
                                    ActionMappingSet* actionMappingSet,
                                    const char* actionName,
                                    const ActionMappingDesc& desc)
{
    size_t actionMappingsCount = input->getActionMappingCount(actionMappingSet, actionName);
    if (actionMappingsCount > 0)
    {
        return false;
    }

    input->addActionMapping(actionMappingSet, actionName, desc);
    return true;
}

/**
 * Subscribes to the keyboard event stream for a specified keyboard.
 *
 * @param input A pointer to input interface.
 * @param keyboard A pointer to Logical keyboard, or nullptr if subscription to events from all keyboards is desired.
 * @param functor A universal reference to function-like callable object to be called on each keyboard event.
 * @return Subscription identifier.
 */
template <typename Functor>
inline SubscriptionId subscribeToKeyboardEvents(IInput* input, Keyboard* keyboard, Functor&& functor)
{
    return input->subscribeToKeyboardEvents(
        keyboard,
        [](const KeyboardEvent& evt, void* userData) -> bool { return (*static_cast<Functor*>(userData))(evt); },
        &functor);
}

/**
 * Subscribes to the mouse event stream for a specified mouse.
 *
 * @param input A pointer to input interface.
 * @param mouse A pointer to Logical mouse, or nullptr if subscription to events from all mice is desired.
 * @param functor A universal reference to function-like callable object to be called on each mouse event.
 * @return Subscription identifier.
 */
template <typename Functor>
inline SubscriptionId subscribeToMouseEvents(IInput* input, Mouse* mouse, Functor&& functor)
{
    return input->subscribeToMouseEvents(
        mouse, [](const MouseEvent& evt, void* userData) -> bool { return (*static_cast<Functor*>(userData))(evt); },
        &functor);
}

/**
 * Subscribes to the gamepad event stream for a specified gamepad.
 *
 * @param input A pointer to input interface.
 * @param gamepad A pointer to Logical gamepad, or nullptr if subscription to events from all gamepads is desired.
 * @param functor A universal reference to function-like callable object to be called on each gamepad event.
 * @return Subscription identifier.
 */
template <typename Functor>
inline SubscriptionId subscribeToGamepadEvents(IInput* input, Gamepad* gamepad, Functor&& functor)
{
    return input->subscribeToGamepadEvents(
        gamepad, [](const GamepadEvent& evt, void* userData) -> bool { return (*static_cast<Functor*>(userData))(evt); },
        &functor);
}

/**
 * Subscribes to the gamepad connection event stream.
 * Once subscribed callback is called for all previously created gamepads.
 *
 * @param input A pointer to input interface.
 * @param functor A universal reference to function-like callable object to be called on each gamepad connection event.
 * @return Subscription identifier.
 */
template <typename Functor>
inline SubscriptionId subscribeToGamepadConnectionEvents(IInput* input, Functor&& functor)
{
    return input->subscribeToGamepadConnectionEvents(
        [](const GamepadConnectionEvent& evt, void* userData) { (*static_cast<Functor*>(userData))(evt); }, &functor);
}

/**
 * Subscribes to the action event stream for a specified action.
 * Event is triggered on any action value change.
 *
 * @param input A pointer to input interface.
 * @param actionMappingSet A pointer to action mapping set
 * @param actionName A pointer to action string identifier.
 * @param functor A universal reference to function-like callable object to be called on the action event.
 * @return Subscription identifier.
 */
template <typename Functor>
inline SubscriptionId subscribeToActionEvents(IInput* input,
                                              ActionMappingSet* actionMappingSet,
                                              const char* actionName,
                                              Functor&& functor)
{
    return input->subscribeToActionEvents(
        actionMappingSet, actionName,
        [](const ActionEvent& evt, void* userData) -> bool { return (*static_cast<Functor*>(userData))(evt); }, &functor);
}

/**
 * Filter and modify a unified input events in the event buffer.
 *
 * @param input A pointer to input interface.
 * @param functor A universal reference to function-like callable object to be called on each input event.
 */
template <typename Callable>
inline void filterBufferedEvents(IInput* input, Callable&& callable)
{
    using Func = std::decay_t<Callable>;
    input->filterBufferedEvents(
        [](InputEvent& evt, void* userData) { return (*static_cast<Func*>(userData))(evt); }, &callable);
}


} // namespace input
} // namespace carb
