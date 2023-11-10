// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Interface.h"
#include "InputTypes.h"

namespace carb
{
namespace input
{

struct InputProvider;
class ActionMappingSet;

/**
 * Defines an input interface.
 *
 * Input plugin allows user to listen to the input devices, but it
 * is not intended to work with the input hardware. The input hardware processing
 * is delegated to the input providers, which should be implemented as a separate
 * plugins.
 * Input providers create logical input devices. For example, a window may have a keyboard and mouse associated
 * with it, i.e. a physical keyboard state may be different from a logical
 * keyboard associated with a window, due to some physical key state changes
 * being sent to a different window.
 *
 * Everything to be used by input providers is put into the InputProvider struct in the separate file.
 * All the functions from Input.h is meant to be used by input consumers (end user).
 *
 * User can subscribe to the device events, as well as device connection events,
 * and upon subscribing to device connection events, user immediately receives
 * "connect" notifications for all already present events of the kind. Similar is
 * true for unsubscribing - user will immediately get "disconnect" notifications
 * for all still present events.
 *
 * One notable feature of device handling is that there is no logical difference
 * between a button(key) and an axis: both can be either polled by value, producing
 * floating-point value, or by button flags, which allow to treat analog inputs
 * as buttons (one example is treat gamepad stick as discrete d-pad).
 *
 * The plugins also allows to map actions to device inputs, allowing to
 * set up multiple slots per action mapping. Those actions could be polled in
 * a similar manner (i.e. by value or as button flags).
 */
struct IInput
{
    CARB_PLUGIN_INTERFACE("carb::input::IInput", 1, 0)

    /**
     * Gets the input provider's part of the input interface.
     *
     * @return Input provider interface.
     */
    InputProvider*(CARB_ABI* getInputProvider)();

    /**
     * Start processing input.
     */
    void(CARB_ABI* startup)();

    /**
     * Shutdown and stop processing input.
     */
    void(CARB_ABI* shutdown)();

    /**
     * Get keyboard logical device name.
     *
     * @param keyboard Logical keyboard.
     * @return Specified keyboard logical device name string.
     *
     * @rst
       .. deprecated:: 100.1
            This method is deprecated and will be removed soon, please use getDeviceName instead.
       @endrst
     */
    const char*(CARB_ABI* getKeyboardName)(Keyboard* keyboard);

    /**
     * Subscribes plugin user to the keyboard event stream for a specified keyboard.
     *
     * @param keyboard Logical keyboard, or nullptr if subscription to events from all keyboards is desired.
     * @param fn Callback function to be called on received event.
     * @param userData Pointer to the user data to be passed into the callback.
     * @return Subscription identifier.
     *
     * @rst
       .. deprecated:: 100.1
            This method is deprecated and will be removed soon, please use subscribeToInputEvents instead.
       @endrst
     */
    SubscriptionId(CARB_ABI* subscribeToKeyboardEvents)(Keyboard* keyboard, OnKeyboardEventFn fn, void* userData);

    /**
     * Unsubscribes plugin user from the keyboard event stream for a specified keyboard.
     *
     * @param keyboard Logical keyboard.
     * @param subscriptionId Subscription identifier.
     *
     * @rst
       .. deprecated:: 100.1
            This method is deprecated and will be removed soon, please use unsubscribeToInputEvents instead.
       @endrst
     */
    void(CARB_ABI* unsubscribeToKeyboardEvents)(Keyboard* keyboard, SubscriptionId id);

    /**
     * Gets the value for the specified keyboard input kind.
     *
     * @param keyboard Logical keyboard.
     * @param input Keyboard input kind (key).
     * @return Specified keyboard input value.
     */
    float(CARB_ABI* getKeyboardValue)(Keyboard* keyboard, KeyboardInput input);

    /**
     * Gets the button flag for the specified keyboard input kind.
     * Each input is treated as button, based on the press threshold.
     *
     * @param keyboard Logical keyboard.
     * @param input Keyboard input kind (key).
     * @return Specified keyboard input as button flags.
     */
    ButtonFlags(CARB_ABI* getKeyboardButtonFlags)(Keyboard* keyboard, KeyboardInput input);

    /**
     * Get mouse logical device name.
     *
     * @param mouse Logical mouse.
     * @return Specified mouse logical device name string.
     *
     * @rst
       .. deprecated:: 100.1
            This method is deprecated and will be removed soon, please use getDeviceName instead.
       @endrst
     */
    const char*(CARB_ABI* getMouseName)(Mouse* mouse);

    /**
     * Gets the value for the specified mouse input kind.
     *
     * @param mouse Logical mouse.
     * @param input Mouse input kind (button/axis).
     * @return Specified mouse input value.
     */
    float(CARB_ABI* getMouseValue)(Mouse* mouse, MouseInput input);

    /**
     * Gets the button flag for the specified mouse input kind.
     * Each input is treated as button, based on the press threshold.
     *
     * @param mouse Logical mouse.
     * @param input Mouse input kind (button/axis).
     * @return Specified mouse input as button flags.
     */
    ButtonFlags(CARB_ABI* getMouseButtonFlags)(Mouse* mouse, MouseInput input);

    /**
     * Gets the mouse coordinates for the specified mouse, normalized by the associated window size.
     *
     * @param mouse Logical mouse.
     * @return Coordinates.
     */
    Float2(CARB_ABI* getMouseCoordsNormalized)(Mouse* mouse);

    /**
     * Gets the absolute mouse coordinates for the specified mouse.
     *
     * @param mouse Logical mouse.
     * @return Coordinates.
     */
    Float2(CARB_ABI* getMouseCoordsPixel)(Mouse* mouse);

    /**
     * Subscribes plugin user to the mouse event stream for a specified mouse.
     *
     * @param mouse Logical mouse, or nullptr if subscription to events from all mice is desired.
     * @param fn Callback function to be called on received event.
     * @param userData Pointer to the user data to be passed into the callback.
     * @return Subscription identifier.
     *
     * @rst
       .. deprecated:: 100.1
            This method is deprecated and will be removed soon, please use subscribeToInputEvents instead.
       @endrst
     */
    SubscriptionId(CARB_ABI* subscribeToMouseEvents)(Mouse* mouse, OnMouseEventFn fn, void* userData);

    /**
     * Unsubscribes plugin user from the mouse event stream for a specified mouse.
     *
     * @param mouse Logical mouse.
     * @param subscriptionId Subscription identifier.
     *
     * @rst
       .. deprecated:: 100.1
            This method is deprecated and will be removed soon, please use unsubscribeToInputEvents instead.
       @endrst
     */
    void(CARB_ABI* unsubscribeToMouseEvents)(Mouse* mouse, SubscriptionId id);

    /**
     * Get gamepad logical device name.
     *
     * @param gamepad Logical gamepad.
     * @return Specified gamepad logical device name string.
     *
     * @rst
       .. deprecated:: 100.1
            This method is deprecated and will be removed soon, please use getDeviceName instead.
       @endrst
     */
    const char*(CARB_ABI* getGamepadName)(Gamepad* gamepad);

    /**
     * Get gamepad GUID.
     *
     * @param gamepad Logical gamepad.
     * @return Specified gamepad logical device GUID.
     */
    const char*(CARB_ABI* getGamepadGuid)(Gamepad* gamepad);

    /**
     * Gets the value for the specified gamepad input kind.
     *
     * @param gamepad Logical gamepad.
     * @param input Gamepad input kind (button/axis).
     * @return Specified gamepad input value.
     */
    float(CARB_ABI* getGamepadValue)(Gamepad* gamepad, GamepadInput input);

    /**
     * Gets the button flag for the specified gamepad input kind.
     * Each input is treated as button, based on the press threshold.
     *
     * @param gamepad Logical gamepad.
     * @param input Gamepad input kind (button/axis).
     * @return Specified gamepad input as button flags.
     */
    ButtonFlags(CARB_ABI* getGamepadButtonFlags)(Gamepad* gamepad, GamepadInput input);

    /**
     * Subscribes plugin user to the gamepad event stream for a specified gamepad.
     *
     * @param gamepad Logical gamepad, or nullptr if subscription to events from all gamepads is desired.
     * @param fn Callback function to be called on received event.
     * @param userData Pointer to the user data to be passed into the callback.
     * @return Subscription identifier.
     *
     * @rst
       .. deprecated:: 100.1
            This method is deprecated and will be removed soon, please use subscribeToInputEvents instead.
       @endrst
     */
    SubscriptionId(CARB_ABI* subscribeToGamepadEvents)(Gamepad* gamepad, OnGamepadEventFn fn, void* userData);

    /**
     * Unsubscribes plugin user from the gamepad event stream for a specified gamepad.
     *
     * @param gamepad Logical gamepad.
     * @param subscriptionId Subscription identifier.
     *
     * @rst
       .. deprecated:: 100.1
            This method is deprecated and will be removed soon, please use unsubscribeToInputEvents instead.
       @endrst
     */
    void(CARB_ABI* unsubscribeToGamepadEvents)(Gamepad* gamepad, SubscriptionId id);

    /**
     * Subscribes plugin user to the gamepad connection event stream.
     * Once subscribed callback is called for all previously created gamepads.
     *
     * @param fn Callback function to be called on received event.
     * @param userData Pointer to the user data to be passed into the callback.
     * @return Subscription identifier.
     */
    SubscriptionId(CARB_ABI* subscribeToGamepadConnectionEvents)(OnGamepadConnectionEventFn fn, void* userData);

    /**
     * Unsubscribes plugin user from the gamepad connection event stream.
     * Unsubscription triggers callback to be called with all devices left as being destroyed.
     *
     * @param subscriptionId Subscription identifier.
     */
    void(CARB_ABI* unsubscribeToGamepadConnectionEvents)(SubscriptionId id);

    /**
     * Processes buffered events queue and sends unconsumed events as device events, action mapping events, and
     * updates device states. Clears buffered events queues.
     */
    void(CARB_ABI* distributeBufferedEvents)();

    /**
     * Create action mapping set - a place in settings where named action mappings are stored.
     *
     * @param settingsPath Path in settings where the set mappings are stored.
     * @return Opaque pointer to the action mapping set.
     */
    ActionMappingSet*(CARB_ABI* createActionMappingSet)(const char* settingsPath);
    /**
     * Get existing action mapping set from the settings path provided.
     *
     * @param settingsPath Path in settings where the set mappings are stored.
     * @return Opaque pointer to the action mapping set.
     */
    ActionMappingSet*(CARB_ABI* getActionMappingSetByPath)(const char* settingsPath);
    /**
     * Destroy action mapping set.
     *
     * @param actionMappingSet Opaque pointer to the action mapping set.
     */
    void(CARB_ABI* destroyActionMappingSet)(ActionMappingSet* actionMappingSet);

    /**
     * Get total action count registered in the plugin with 1 or more action mapping.
     *
     * @return The number of the actions.
     */
    size_t(CARB_ABI* getActionCount)(ActionMappingSet* actionMappingSet);

    /**
     * Get array of all actions.
     * The size of an array is equal to the Input::getActionCount().
     *
     * @return The array of actions.
     */
    const char* const*(CARB_ABI* getActions)(ActionMappingSet* actionMappingSet);

    /**
     * Adds action mapping to the specified action.
     * Each action keeps a list of mappings. This function push mapping to the end of the list.
     *
     * @param actionName Action string identifier.
     * @param desc Action mapping description.
     * @return The index of added mapping.
     */
    size_t(CARB_ABI* addActionMapping)(ActionMappingSet* actionMappingSet,
                                       const char* actionName,
                                       const ActionMappingDesc& desc);

    /**
     * Sets and overrides the indexed action mapping for the specified action.
     * Each action keeps a list of mappings. This function sets list item according by the index.
     *
     * @param actionName Action string identifier.
     * @param index The index of mapping to override. It should be in range [0, mapping count).
     * @param desc Action mapping description.
     */
    void(CARB_ABI* setActionMapping)(ActionMappingSet* actionMappingSet,
                                     const char* actionName,
                                     size_t index,
                                     const ActionMappingDesc& desc);

    /**
     * Remove indexed action mapping for the specified action.
     * Each action keeps a list of mappings. This function removes list item by the index.
     *
     * @param actionName Action string identifier.
     * @param index The index of mapping to remove. It should be in range [0, mapping count).
     */
    void(CARB_ABI* removeActionMapping)(ActionMappingSet* actionMappingSet, const char* actionName, size_t index);

    /**
     * Clears and removes all mappings associated with the action.
     *
     * @param actionName Action string identifier.
     */
    void(CARB_ABI* clearActionMappings)(ActionMappingSet* actionMappingSet, const char* actionName);

    /**
     * Get mappings count associated with the action.
     *
     * @param action Action string identifier.
     * @return The number of the mapping in the list for an action.
     */
    size_t(CARB_ABI* getActionMappingCount)(ActionMappingSet* actionMappingSet, const char* actionName);

    /**
     * Get array of mappings associated with the action.
     * The size of an array is equal to the Input::getMappingCount().
     *
     * @param actionName Action string identifier.
     * @return The array of mappings for an action.
     */
    const ActionMappingDesc*(CARB_ABI* getActionMappings)(ActionMappingSet* actionMappingSet, const char* actionName);

    /**
     * Gets the value for the specified action.
     * If multiple mapping are associated with the action the biggest value is returned.
     *
     * @param actionName Action string identifier.
     * @return Specified action value.
     */
    float(CARB_ABI* getActionValue)(ActionMappingSet* actionMappingSet, const char* actionName);

    /**
     * Gets the button flag for the specified action.
     * Each mapping is treated as button, based on the press threshold.
     *
     * @param actionName Action string identifier.
     * @return Specified action value as button flags.
     */
    ButtonFlags(CARB_ABI* getActionButtonFlags)(ActionMappingSet* actionMappingSet, const char* actionName);

    /**
     * Subscribes plugin user to the action event stream for a specified action.
     * Event is triggered on any action value change.
     *
     * @param action Action string identifier.
     * @param fn Callback function to be called on received event.
     * @param userData Pointer to the user data to be passed into the callback.
     * @return Subscription identifier.
     */
    SubscriptionId(CARB_ABI* subscribeToActionEvents)(ActionMappingSet* actionMappingSet,
                                                      const char* actionName,
                                                      OnActionEventFn fn,
                                                      void* userData);

    /**
     * Unsubscribes plugin user from the action event stream for a specified action.
     *
     * @param action Action string identifier.
     * @param subscriptionId Subscription identifier.
     */
    void(CARB_ABI* unsubscribeToActionEvents)(SubscriptionId id);

    /**
     * Filters all buffered events by calling the specified filter function on each event.
     *
     * The given @p fn may modify events in-place and/or may add additional events via the InputProvider obtained from
     * getInputProvider(). Any additional events that are added during a call to filterBufferedEvents() will not be
     * passed to @p fn during that call. However, future calls to filterBufferedEvents() will pass the events to @p fn.
     * Any new buffered events added by InputProvider during @p fn will be added to the end of the event list. Events
     * modified during @p fn remain in their relative position in the event list.
     *
     * The outcome of an event is based on what @p fn returns for that event. If FilterResult::eConsume is returned, the
     * event is considered processed and is removed from the list of buffered events. Future calls to
     * filterBufferedEvents() will not receive the event and it will not be sent when distributeBufferedEvents() is
     * called. If FilterResult::eRetain is returned, the (possibly modified) event remains in the list of buffered
     * events. Future calls to filterBufferedEvents() will receive the event and it will be sent when
     * distributeBufferedEvents() is called.
     *
     * This function may be called multiple times to re-filter events. For instance, the given @p fn may be interested
     * in only certain types of events.
     *
     * The remaining buffered events are sent when distributeBufferedEvents() is called, at which point the list of
     * buffered events is cleared.
     *
     * @warning Calling filterBufferedEvents() or distributeBufferedEvents() from @p fn is expressly disallowed.
     *
     * @thread_safety An internal lock is held while @p fn is called on all events, which synchronizes-with
     * distributeBufferedEvents() and the various InputProvider functions to buffer events. Although the lock provides
     * thread safety to synchronize these operations, if buffered events are added from other threads it is conceivable
     * that events could be added between filterBufferedEvents() and distributeBufferedEvents(), causing them to be sent
     * before being filtered. If this is a cause for concern, use of an external lock is recommended.
     *
     * @param fn A pointer to a callback function to be called on each input event.
     * @param userData A pointer to the user data to be passed into the callback.
     */
    void(CARB_ABI* filterBufferedEvents)(InputEventFilterFn fn, void* userData);

    /**
     * Get input device name.
     *
     * @param device Input device.
     * @return Specified input device name string.
     */
    const char*(CARB_ABI* getDeviceName)(InputDevice* device);

    /**
     * Get input device type.
     *
     * @param device Input device.
     * @return Specified input device type or DeviceType::eUnknown.
     */
    DeviceType(CARB_ABI* getDeviceType)(InputDevice* device);

    /**
     * Subscribes plugin user to the input event stream for a specified device.
     *
     * @param device Input device, or nullptr if subscription to events from all devices is desired.
     * @param events A bit mask to event types to subscribe to. Currently kEventTypeAll is only supported.
     * @param fn Callback function to be called on received event.
     * @param userData Pointer to the user data to be passed into the callback.
     * @param order Subscriber position hint [0..N-1] from the beginning, [-1, -N] from the end (-1 is default).
     * @return Subscription identifier.
     */
    SubscriptionId(CARB_ABI* subscribeToInputEvents)(
        InputDevice* device, EventTypeMask events, OnInputEventFn fn, void* userData, SubscriptionOrder order);

    /**
     * Unsubscribes plugin user from the input event stream for a specified device.
     *
     * @param subscriptionId Subscription identifier.
     */
    void(CARB_ABI* unsubscribeToInputEvents)(SubscriptionId id);
};

} // namespace input
} // namespace carb
