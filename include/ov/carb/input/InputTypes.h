// Copyright (c) 2018-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once
#include "../Types.h"

namespace carb
{
namespace input
{

struct InputDevice;
struct Keyboard;
struct Mouse;
struct Gamepad;

/**
 * Type used as an identifier for all subscriptions.
 */
typedef uint32_t SubscriptionId;

/**
 * Subscription order.
 *
 * [0..N-1] requires to insert before the position from the beginning and shift tail on the right.
 * [-1..-N] requires to insert after the position relative from the end and shift head on the left.
 *
 * Please look at the examples below:
 *
 * Assume we initially have a queue of N subscribers  a b c .. y z:
 * +---+---+---+-- --+---+---+
 * | a | b | c |     | y | z | -----events--flow--->
 * +---+---+---+-- --+---+---+
 * | 0 | 1 | 2 |     |N-2|N-1| ---positive-order--->
 * +---+---+---+-- --+---+---+
 * | -N|   |   |     | -2| -1| <---negative-order---
 * +---+---+---+-- --+---+---+
 * first                  last
 *
 * After inserting subscriber e with the order 1:
 * +---+---+---+---+-- --+---+---+
 * | a | e | b | c |     | y | z |
 * +---+---+---+---+-- --+---+---+
 * | 0 | 1 | 2 | 3 |     |N-1| N |
 * +---+---+---+---+-- --+---+---+
 * first                      last
 *
 * After inserting subscriber f with the order -1:
 * +---+---+---+---+-- --+---+---+---+
 * | a | e | b | c |     | y | z | f |
 * +---+---+---+---+-- --+---+---+---+
 * | 0 | 1 | 2 | 3 |     |N-1| N |N+1|
 * +---+---+---+---+-- --+---+---+---+
 * | 0 | 1 | 2 | 3 |     |M-3|M-2|M-1|
 * +---+---+---+---+-- --+---+---+---+
 * first                          last
 *
 */
using SubscriptionOrder = int32_t;

/**
 * Default subscription order.
 */
static constexpr SubscriptionOrder kSubscriptionOrderFirst = 0;
static constexpr SubscriptionOrder kSubscriptionOrderLast = -1;
static constexpr SubscriptionOrder kSubscriptionOrderDefault = kSubscriptionOrderLast;

/**
 * Defines possible input event types.
 * TODO: This is not supported yet.
 */
enum class EventType : uint32_t
{
    eUnknown
};

/**
 * Defines event type mask.
 * TODO: Flags are not customized yet.
 */
typedef uint32_t EventTypeMask;
static constexpr EventTypeMask kEventTypeAll = EventTypeMask(-1);


/**
 * Defines possible press states.
 */
typedef uint32_t ButtonFlags;
const uint32_t kButtonFlagTransitionUp = 1;
const uint32_t kButtonFlagStateUp = (1 << 1);
const uint32_t kButtonFlagTransitionDown = (1 << 2);
const uint32_t kButtonFlagStateDown = (1 << 3);

/**
 * Defines possible device types.
 */
enum class DeviceType
{
    eKeyboard,
    eMouse,
    eGamepad,
    eCount,
    eUnknown = eCount
};

/**
 * Defines keyboard modifiers.
 */
typedef uint32_t KeyboardModifierFlags;
const uint32_t kKeyboardModifierFlagShift = 1 << 0;
const uint32_t kKeyboardModifierFlagControl = 1 << 1;
const uint32_t kKeyboardModifierFlagAlt = 1 << 2;
const uint32_t kKeyboardModifierFlagSuper = 1 << 3;
const uint32_t kKeyboardModifierFlagCapsLock = 1 << 4;
const uint32_t kKeyboardModifierFlagNumLock = 1 << 5;
/**
 * Defines totl number of keyboard modifiers.
 */
const uint32_t kKeyboardModifierFlagCount = 6;

/**
 * Defines keyboard event type.
 */
enum class KeyboardEventType
{
    eKeyPress, ///< Sent when key is pressed the first time.
    eKeyRepeat, ///< Sent after a platform-specific delay if key is held down.
    eKeyRelease, ///< Sent when the key is released.
    eChar, ///< Sent when a character is produced by the input actions, for example during key presses.

    // Must always be last
    eCount ///< The number of KeyboardEventType elements.
};


/**
 * Defines input code type.
 *
 */
typedef uint32_t InputType;

/**
 * Defines keyboard key codes
 *
 * The key code represents the physical key location in the standard US keyboard layout keyboard, if they exist
 * in the US keyboard.
 *
 * eUnknown is sent for key events that do not have a key code.
 */
enum class KeyboardInput : InputType
{
    eUnknown,
    eSpace,
    eApostrophe,
    eComma,
    eMinus,
    ePeriod,
    eSlash,
    eKey0,
    eKey1,
    eKey2,
    eKey3,
    eKey4,
    eKey5,
    eKey6,
    eKey7,
    eKey8,
    eKey9,
    eSemicolon,
    eEqual,
    eA,
    eB,
    eC,
    eD,
    eE,
    eF,
    eG,
    eH,
    eI,
    eJ,
    eK,
    eL,
    eM,
    eN,
    eO,
    eP,
    eQ,
    eR,
    eS,
    eT,
    eU,
    eV,
    eW,
    eX,
    eY,
    eZ,
    eLeftBracket,
    eBackslash,
    eRightBracket,
    eGraveAccent,
    eEscape,
    eTab,
    eEnter,
    eBackspace,
    eInsert,
    eDel,
    eRight,
    eLeft,
    eDown,
    eUp,
    ePageUp,
    ePageDown,
    eHome,
    eEnd,
    eCapsLock,
    eScrollLock,
    eNumLock,
    ePrintScreen,
    ePause,
    eF1,
    eF2,
    eF3,
    eF4,
    eF5,
    eF6,
    eF7,
    eF8,
    eF9,
    eF10,
    eF11,
    eF12,
    eNumpad0,
    eNumpad1,
    eNumpad2,
    eNumpad3,
    eNumpad4,
    eNumpad5,
    eNumpad6,
    eNumpad7,
    eNumpad8,
    eNumpad9,
    eNumpadDel,
    eNumpadDivide,
    eNumpadMultiply,
    eNumpadSubtract,
    eNumpadAdd,
    eNumpadEnter,
    eNumpadEqual,
    eLeftShift,
    eLeftControl,
    eLeftAlt,
    eLeftSuper,
    eRightShift,
    eRightControl,
    eRightAlt,
    eRightSuper,
    eMenu,

    eCount
};

/**
 * UTF8 RFC3629 - max 4 bytes per character
 */
const uint32_t kCharacterMaxNumBytes = 4;

/**
 * Defines a keyboard event.
 */
struct KeyboardEvent
{
    union
    {
        Keyboard* keyboard;
        InputDevice* device;
    };
    KeyboardEventType type;
    union
    {
        KeyboardInput key;
        InputType inputType;
        char character[kCharacterMaxNumBytes];
    };
    KeyboardModifierFlags modifiers;
};

/**
 * Defines the mouse event types.
 */
enum class MouseEventType
{
    eLeftButtonDown,
    eLeftButtonUp,
    eMiddleButtonDown,
    eMiddleButtonUp,
    eRightButtonDown,
    eRightButtonUp,
    eMove,
    eScroll,

    // Must always be last
    eCount ///< The number of MouseEventType elements.
};

/**
 * Defines the mouse event.
 *
 * normalizedCoords - mouse coordinates only active in move events, normalized to [0.0, 1.0] relative to the
 *   associated window size.
 * unscaledCoords - mouse coordinates only active in move events, not normalized.
 * scrollDelta - scroll delta, only active in scroll events.
 */
struct MouseEvent
{
    union
    {
        Mouse* mouse;
        InputDevice* device;
    };
    MouseEventType type;
    union
    {
        Float2 normalizedCoords;
        Float2 scrollDelta;
    };
    KeyboardModifierFlags modifiers;
    Float2 pixelCoords;
};

/**
 * Defines a mouse input.
 */
enum class MouseInput : InputType
{
    eLeftButton,
    eRightButton,
    eMiddleButton,
    eForwardButton,
    eBackButton,
    eScrollRight,
    eScrollLeft,
    eScrollUp,
    eScrollDown,
    eMoveRight,
    eMoveLeft,
    eMoveUp,
    eMoveDown,

    eCount
};

/**
 * Defines a gamepad input.
 *
 * Expected ABXY buttons layout:
 *   Y
 * X   B
 *   A
 * eMenu1 - maps to View (XBone) / Share (DS4)
 * eMenu2 - maps to Menu (XBone) / Options (DS4)
 */
enum class GamepadInput : InputType
{
    eLeftStickRight,
    eLeftStickLeft,
    eLeftStickUp,
    eLeftStickDown,
    eRightStickRight,
    eRightStickLeft,
    eRightStickUp,
    eRightStickDown,
    eLeftTrigger,
    eRightTrigger,
    eA,
    eB,
    eX,
    eY,
    eLeftShoulder,
    eRightShoulder,
    eMenu1,
    eMenu2,
    eLeftStick,
    eRightStick,
    eDpadUp,
    eDpadRight,
    eDpadDown,
    eDpadLeft,

    eCount
};

/**
 * Defines a gamepad event.
 */
struct GamepadEvent
{
    union
    {
        Gamepad* gamepad;
        InputDevice* device;
    };
    union
    {
        GamepadInput input;
        InputType inputType;
    };
    float value;
};

/**
 * Defines the gamepad connection event types.
 */
enum class GamepadConnectionEventType
{
    eCreated,
    eConnected,
    eDisconnected,
    eDestroyed
};

/**
 * Defines the gamepad connection event.
 */
struct GamepadConnectionEvent
{
    union
    {
        Gamepad* gamepad;
        InputDevice* device;
    };
    GamepadConnectionEventType type;
};

/**
 * Defines the unified input event.
 */
struct InputEvent
{
    DeviceType deviceType;
    union
    {
        KeyboardEvent keyboardEvent;
        MouseEvent mouseEvent;
        GamepadEvent gamepadEvent;
        InputDevice* device;
    };
};


/**
 * Defines action mapping description.
 */
struct ActionMappingDesc
{
    DeviceType deviceType;
    union
    {
        Keyboard* keyboard;
        Mouse* mouse;
        Gamepad* gamepad;
        InputDevice* device;
    };
    union
    {
        KeyboardInput keyboardInput;
        MouseInput mouseInput;
        GamepadInput gamepadInput;
        InputType inputType;
    };
    KeyboardModifierFlags modifiers;
};

/**
 * Defines an action event.
 */
struct ActionEvent
{
    const char* action;
    float value;
    ButtonFlags flags;
};

/**
 * Function type that describes keyboard event callback.
 *
 * @param evt The event description.
 * @param userData Pointer to the user data.
 * @return Whether event should be processed by subsequent event subscribers.
 */
typedef bool (*OnActionEventFn)(const ActionEvent& evt, void* userData);

/**
 * Function type that describes keyboard event callback.
 *
 * @param evt The event description.
 * @param userData Pointer to the user data.
 * @return Whether event should be processed by subsequent event subscribers.
 */
typedef bool (*OnKeyboardEventFn)(const KeyboardEvent& evt, void* userData);

/**
 * Function type that describes mouse event callback.
 *
 * @param evt The event description.
 * @param userData Pointer to the user data.
 * @return Whether event should be processed by subsequent event subscribers.
 */
typedef bool (*OnMouseEventFn)(const MouseEvent& evt, void* userData);

/**
 * Function type that describes gamepad event callback.
 *
 * @param evt The event description.
 * @param userData Pointer to the user data.
 * @return Whether event should be processed by subsequent event subscribers.
 */
typedef bool (*OnGamepadEventFn)(const GamepadEvent& evt, void* userData);

/**
 * Function type that describes gamepad connection event callback.
 *
 * @param evt The event description.
 * @param userData Pointer to the user data.
 * @return Whether event should not be processed anymore by subsequent event subscribers.
 */
typedef void (*OnGamepadConnectionEventFn)(const GamepadConnectionEvent& evt, void* userData);

/**
 * Function type that describes input event callback.
 *
 * @param evt The event description.
 * @param userData Pointer to the user data.
 * @return Whether event should be processed by subsequent event subscribers.
 */
typedef bool (*OnInputEventFn)(const InputEvent& evt, void* userData);

/**
 * The result returned by InputEventFilterFn.
 */
enum class FilterResult : uint8_t
{
    //! The event should be retained and sent later when IInput::distributeBufferedEvents() is called.
    eRetain = 0,

    //! The event has been fully processed by InputEventFilterFn and should NOT be sent later when
    //! IInput::distributeBufferedEvents() is called.
    eConsume = 1,
};

/**
 * Callback function type for filtering events.
 *
 * @see IInput::filterBufferedEvents() for more information.
 *
 * @param evt A reference to the unified event description. The event may be modified.
 * @param userData A pointer to the user data passed to IInput::filterBufferedEvents().
 * @return The FilterResult indicating what should happen with the event.
 */
typedef FilterResult (*InputEventFilterFn)(InputEvent& evt, void* userData);

static const char* const kAnyDevice = nullptr;
} // namespace input
} // namespace carb
