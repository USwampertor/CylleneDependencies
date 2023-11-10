// Copyright (c) 2018-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Interface.h"
#include "../Types.h"

namespace carb
{

namespace input
{
struct Keyboard;
struct Mouse;
struct Gamepad;
} // namespace input

namespace windowing
{

struct Window;
struct Cursor;
struct Monitor;

enum class MonitorChangeEvent : uint32_t
{
    eUnknown,
    eConnected,
    eDisconnected
};

typedef void (*OnWindowMoveFn)(Window* window, int x, int y, void* userData);
typedef void (*OnWindowResizeFn)(Window* window, int width, int height, void* userData);
typedef void (*OnWindowDropFn)(Window* window, const char** paths, int count, void* userData);
typedef void (*OnWindowCloseFn)(Window* window, void* userData);
typedef void (*OnWindowContentScaleFn)(Window* window, float scaleX, float scaleY, void* userData);
typedef void (*OnWindowFocusFn)(Window* window, bool isFocused, void* userData);
typedef void (*OnWindowMaximizeFn)(Window* window, bool isMaximized, void* userData);
typedef void (*OnWindowMinimizeFn)(Window* window, bool isMinimized, void* userData);
typedef void (*OnMonitorChangeFn)(const Monitor* monitor, MonitorChangeEvent evt);

typedef uint32_t WindowHints;
constexpr WindowHints kWindowHintNone = 0;
constexpr WindowHints kWindowHintNoResize = 1 << 0;
constexpr WindowHints kWindowHintNoDecoration = 1 << 1;
constexpr WindowHints kWindowHintNoAutoIconify = 1 << 2;
constexpr WindowHints kWindowHintNoFocusOnShow = 1 << 3;
constexpr WindowHints kWindowHintScaleToMonitor = 1 << 4;
constexpr WindowHints kWindowHintFloating = 1 << 5;
constexpr WindowHints kWindowHintMaximized = 1 << 6;

/**
 * Descriptor for how a window is to be created.
 */
struct WindowDesc
{
    int width; ///! The initial window width.
    int height; ///! The initial window height.
    const char* title; ///! The initial title of the window.
    bool fullscreen; ///! Should the window be initialized in fullscreen mode.
    WindowHints hints; ///! Initial window hints / attributes.
};

/**
 * Defines cursor standard shapes.
 */
enum class CursorStandardShape : uint32_t
{
    eArrow, ///! The regular arrow cursor shape.
    eIBeam, ///! The text input I-beam cursor shape.
    eCrosshair, ///! The crosshair shape.
    eHand, ///! The hand shape
    eHorizontalResize, ///! The horizontal resize arrow shape.
    eVerticalResize ///! The vertical resize arrow shape.
};

enum class CursorMode : uint32_t
{
    eNormal, ///! Cursor visible and behaving normally.
    eHidden, ///! Cursor invisible when over the content area of window but does not restrict the cursor from leaving.
    eDisabled, ///! Hides and grabs the cursor, providing virtual and unlimited cursor movement. This is useful
               /// for implementing for example 3D camera controls.
};

enum class InputMode : uint32_t
{
    eStickyKeys, ///! Config sticky key.
    eStickyMouseButtons, ///! Config sticky mouse button.
    eLockKeyMods, ///! Config lock key modifier bits.
    eRawMouseMotion ///! Config raw mouse motion.
};

struct VideoMode
{
    int width; ///! The width, in screen coordinates, of the video mode.
    int height; ///! The height, in screen coordinates, of the video mode.
    int redBits; ///! The bit depth of the red channel of the video mode.
    int greenBits; ///! The bit depth of the green channel of the video mode.
    int blueBits; ///! The bit depth of the blue channel of the video mode.
    int refreshRate; ///! The refresh rate, in Hz, of the video mode.
};

/**
 * This describes a single 2D image. See the documentation for each related function what the expected pixel format is.
 */
struct Image
{
    int32_t width; ///! The width, in pixels, of this image.
    int32_t height; ///! The height, in pixels, of this image.
    uint8_t* pixels; ///! The pixel data of this image, arranged left-to-right, top-to-bottom.
};


/**
 * Defines a windowing interface.
 */
struct IWindowing
{
    CARB_PLUGIN_INTERFACE("carb::windowing::IWindowing", 1, 4)

    /**
     * Creates a window.
     *
     * @param desc The descriptor for the window.
     * @return The window created.
     */
    Window*(CARB_ABI* createWindow)(const WindowDesc& desc);

    /**
     * Destroys a window.
     *
     * @param window The window to be destroyed.
     */
    void(CARB_ABI* destroyWindow)(Window* window);

    /**
     * Shows a window making it visible.
     *
     * @param window The window to use.
     */
    void(CARB_ABI* showWindow)(Window* window);

    /**
     * Hides a window making it hidden.
     *
     * @param window The window to use.
     */
    void(CARB_ABI* hideWindow)(Window* window);

    /**
     * Gets the current window width.
     *
     * @param window The window to use.
     * @return The current window width.
     */
    uint32_t(CARB_ABI* getWindowWidth)(Window* window);

    /**
     * Gets the current window height.
     *
     * @param window The window to use.
     * @return The current window height.
     */
    uint32_t(CARB_ABI* getWindowHeight)(Window* window);

    /**
     * Gets the current window position.
     *
     * @param window The window to use.
     * @return The current window position.
     */
    Int2(CARB_ABI* getWindowPosition)(Window* window);

    /**
     * Sets the current window position.
     *
     * @param window The window to use.
     * @param position The position to set the window to.
     */
    void(CARB_ABI* setWindowPosition)(Window* window, const Int2& position);

    /**
     * Sets the window title.
     *
     * @param window The window to use.
     * @param title The window title to be set (as a utf8 string)
     */
    void(CARB_ABI* setWindowTitle)(Window* window, const char* title);

    /**
     * Sets the window opacity.
     *
     * @param window The window to use.
     * @param opacity The window opacity. 1.0f is fully opaque. 0.0 is fully transparent.
     */
    void(CARB_ABI* setWindowOpacity)(Window* window, float opacity);

    /**
     * Gets the window opacity.
     *
     * @param window The window to use.
     * @return The window opacity. 1.0f is fully opaque. 0.0 is fully transparent.
     */
    float(CARB_ABI* getWindowOpacity)(Window* window);

    /**
     * Sets the window into fullscreen or windowed mode.
     *
     * @param window The window to use.
     * @param fullscreen true to be set to fullscreen, false to be set to windowed.
     */
    void(CARB_ABI* setWindowFullscreen)(Window* window, bool fullscreen);

    /**
     * Determines if the window is in fullscreen mode.
     *
     * @param window The window to use.
     * @return true if the window is in fullscreen mode, false if in windowed mode.
     */
    bool(CARB_ABI* isWindowFullscreen)(Window* window);

    /**
     * Sets the function for handling resize events.
     *
     * @param window The window to use.
     * @param onWindowResize The function callback to handle resize events on the window.
     */
    void(CARB_ABI* setWindowResizeFn)(Window* window, OnWindowResizeFn onWindowResize, void* userData);

    /**
     * Resizes the window.
     *
     * @param window The window to resize.
     * @param width The width to resize to.
     * @param height The height to resize to.
     */
    void(CARB_ABI* resizeWindow)(Window* window, int width, int height);

    /**
     * Set the window in focus.
     *
     * @param window The window to use.
     */
    void(CARB_ABI* focusWindow)(Window* window);

    /**
     * Sets the function for handling window focus events.
     *
     * @param window The window to use.
     * @param onWindowFocusFn The function callback to handle focus events on the window.
     */
    void(CARB_ABI* setWindowFocusFn)(Window* window, OnWindowFocusFn onWindowFocusFn, void* userData);

    /**
     * Determines if the window is in focus.
     *
     * @param window The window to use.
     * @return true if the window is in focus, false if it is not.
     */
    bool(CARB_ABI* isWindowFocused)(Window* window);

    /**
     * Sets the function for handling window minimize events.
     *
     * @param window The window to use.
     * @param onWindowMinimizeFn The function callback to handle minimize events on the window.
     */
    void(CARB_ABI* setWindowMinimizeFn)(Window* window, OnWindowMinimizeFn onWindowMinimizeFn, void* userData);

    /**
     * Determines if the window is minimized.
     *
     * @param window The window to use.
     * @return true if the window is minimized, false if it is not.
     */
    bool(CARB_ABI* isWindowMinimized)(Window* window);

    /**
     * Sets the function for handling drag-n-drop events.
     *
     * @param window The window to use.
     * @param onWindowDrop The function callback to handle drop events on the window.
     */
    void(CARB_ABI* setWindowDropFn)(Window* window, OnWindowDropFn onWindowDrop, void* userData);

    /**
     * Sets the function for handling window close events.
     *
     * @param window The window to use.
     * @param onWindowClose The function callback to handle window close events.
     */
    void(CARB_ABI* setWindowCloseFn)(Window* window, OnWindowCloseFn onWindowClose, void* userData);

    /**
     * Determines if the user has attempted to closer the window.
     *
     * @param window The window to use.
     * @return true if the user has attempted to closer the window, false if still open.
     */
    bool(CARB_ABI* shouldWindowClose)(Window* window);

    /**
     * Hints to the window that it should close.
     *
     * @param window The window to use.
     * @param value true to request the window to close, false to request it not to close.
     */
    void(CARB_ABI* setWindowShouldClose)(Window* window, bool value);

    /**
     * This function returns the current value of the user-defined pointer of the specified window.
     * The initial value is nullptr.
     *
     * @param window The window to use.
     * @return the current value of the user-defined pointer of the specified window.
     */
    void*(CARB_ABI* getWindowUserPointer)(Window* window);

    /**
     * This function sets the user-defined pointer of the specified window.
     * The current value is retained until the window is destroyed. The initial value is nullptr.
     *
     * @param window The window to use.
     * @param pointer The new pointer value.
     */
    void(CARB_ABI* setWindowUserPointer)(Window* window, void* pointer);

    /**
     * Sets the function for handling content scale events.
     *
     * @param window The window to use.
     * @param onWindowContentScale The function callback to handle content scale events on the window.
     */
    void(CARB_ABI* setWindowContentScaleFn)(Window* window, OnWindowContentScaleFn onWindowContentScale, void* userData);

    /**
     * Retrieves the content scale for the specified monitor.
     *
     * @param window The window to use.
     * @return The content scale of the window.
     */
    Float2(CARB_ABI* getWindowContentScale)(Window* window);

    /**
     * Gets the native display handle.
     *
     * windows = nullptr
     * linux = ::Display*
     *
     * @param window The window to use.
     * @return The native display handle.
     */
    void*(CARB_ABI* getNativeDisplay)(Window* window);

    /**
     * Gets the native window handle.
     *
     * windows = ::HWND
     * linux = ::Window
     *
     * @param window The window to use.
     * @return The native window handle.
     */
    void*(CARB_ABI* getNativeWindow)(Window* window);

    /**
     * Sets an input mode option for the specified window.
     *
     * @param window The window to set input mode.
     * @param mode The mode to set.
     * @param enabled The new value @ref mode should be changed to.
     */
    void(CARB_ABI* setInputMode)(Window* window, InputMode mode, bool enabled);

    /**
     * Gets the value of a input mode option for the specified window.
     *
     * @param window The window to get input mode value.
     * @param mode The input mode to get value from.
     * @return The input mode value associated with the window.
     */
    bool(CARB_ABI* getInputMode)(Window* window, InputMode mode);

    /**
     * Updates input device states.
     */
    void(CARB_ABI* updateInputDevices)();

    /**
     * Polls and processes only those events that have already been received and then returns immediately.
     */
    void(CARB_ABI* pollEvents)();

    /**
     * Puts the calling thread to sleep until at least one event has been received.
     */
    void(CARB_ABI* waitEvents)();

    /**
     * Gets the logical keyboard associated with the window.
     *
     * @param window The window to use.
     * @return The keyboard.
     */
    input::Keyboard*(CARB_ABI* getKeyboard)(Window* window);

    /**
     * Gets the logical mouse associated with the window.
     *
     * @param window The window to use.
     * @return The mouse.
     */
    input::Mouse*(CARB_ABI* getMouse)(Window* window);

    /**
     * Creates a cursor with a standard shape, that can be set for a window with @ref setCursor.
     *
     * Use @ref destroyCursor to destroy cursors.
     *
     * @param shape The standard shape of cursor to be created.
     * @return A new cursor ready to use or nullptr if an error occurred.
     */
    Cursor*(CARB_ABI* createCursorStandard)(CursorStandardShape shape);

    /**
     * Destroys a cursor previously created with @ref createCursorStandard.
     * If the specified cursor is current for any window, that window will be reverted to the default cursor.
     *
     * @param cursor the cursor object to destroy.
     */
    void(CARB_ABI* destroyCursor)(Cursor* cursor);

    /**
     * Sets the cursor image to be used when the cursor is over the content area of the specified window.
     *
     * @param window The window to set the cursor for.
     * @param cursor The cursor to set, or nullptr to switch back to the default arrow cursor.
     */
    void(CARB_ABI* setCursor)(Window* window, Cursor* cursor);

    /**
     * Sets cursor mode option for the specified window.
     *
     * @param window The window to set cursor mode.
     * @param mode The mouse mode to set to.
     */
    void(CARB_ABI* setCursorMode)(Window* window, CursorMode mode);

    /**
     * Gets cursor mode option for the specified window.
     *
     * @param window The window to get cursor mode.
     * @return The mouse mode associated with the window.
     */
    CursorMode(CARB_ABI* getCursorMode)(Window* window);

    /**
     * Sets cursor position relative to the window.
     *
     * @param window The window to set input mode.
     * @param position The x/y coordinates relative to the window.
     */
    void(CARB_ABI* setCursorPosition)(Window* window, const Int2& position);

    /**
     * Gets cursor position relative to the window.
     *
     * @param window The window to set input mode.
     * @return The x/y coordinates relative to the window.
     */
    Int2(CARB_ABI* getCursorPosition)(Window* window);

    /**
     * The set clipboard function, which expects a Window and text.
     *
     * @param window The window that contains a glfwWindow
     * @param text The text to set to the clipboard
     */
    void(CARB_ABI* setClipboard)(Window* window, const char* text);

    /**
     * Gets the clipboard text.
     *
     * @param window The window that contains a glfwWindow
     * @return The text from the clipboard
     */
    const char*(CARB_ABI* getClipboard)(Window* window);

    /**
     * Sets the monitors callback function for configuration changes
     *
     * The onMonitorChange function callback will occur when monitors are changed.
     * Current changes that can occur are connected/disconnected.
     *
     * @param onMonitorChange The callback function when monitors change.
     */
    void(CARB_ABI* setMonitorsChangeFn)(OnMonitorChangeFn onMonitorChange);

    /**
     * Gets the primary monitor.
     *
     * A Monitor object represents a currently connected monitor and is represented as a pointer
     * to the opaque native monitor. Monitor objects cannot be created or destroyed by the application
     * and retain their addresses until the monitors they represent are disconnected.
     *
     * @return The primary monitor.
     */
    const Monitor*(CARB_ABI* getMonitorPrimary)();

    /**
     * Gets the enumerated monitors.
     *
     * This represents a currently connected monitors and is represented as a pointer
     * to the opaque native monitor. Monitors cannot be created or destroyed
     * and retain their addresses until the monitors are disconnected.
     *
     * Use @ref setMonitorsChangeFn to know when a monitor is disconnected.
     *
     * @param monitorCount The returned number of monitors enumerated.
     * @return The enumerated monitors.
     */
    const Monitor**(CARB_ABI* getMonitors)(size_t* monitorCount);

    /**
     * Gets the human read-able monitor name.
     *
     * The name pointer returned is only valid for the life of the Monitor.
     * When the Monitor is disconnected, the name pointer becomes invalid.
     *
     * Use @ref setMonitorsChangeFn to know when a monitor is disconnected.
     *
     * @param monitor The monitor to use.
     * @return The human read-able monitor name. Pointer returned is owned by monitor.
     */
    const char*(CARB_ABI* getMonitorName)(const Monitor* monitor);

    /**
     * Gets a monitors physical size in millimeters.
     *
     * The size returned is only valid for the life of the Monitor.
     * When the Monitor is disconnected, the size becomes invalid.
     *
     * Use @ref setMonitorsChangeFn to know when a monitor is disconnected.
     *
     * @param monitor The monitor to use.
     * @param size The monitor physical size returned.
     */
    Int2(CARB_ABI* getMonitorPhysicalSize)(const Monitor* monitor);

    /**
     * Gets a monitors current video mode.
     *
     * The pointer returned is only valid for the life of the Monitor.
     * When the Monitor is disconnected, the pointer becomes invalid.
     *
     * Use @ref setMonitorsChangeFn to know when a monitor is disconnected.
     *
     * @param monitor The monitor to use.
     * @return The video mode.
     */
    const VideoMode*(CARB_ABI* getMonitorVideoMode)(const Monitor* monitor);

    /**
     * Gets a monitors virtual position.
     *
     * The position returned is only valid for the life of the Monitor.
     * When the Monitor is disconnected, the position becomes invalid.
     *
     * Use @ref setMonitorsChangeFn to know when a monitor is disconnected.
     *
     * @param monitor The monitor to use.
     * @param position The monitor virtual position returned.
     */
    Int2(CARB_ABI* getMonitorPosition)(const Monitor* monitor);

    /**
     * Gets a monitors content scale.
     *
     * The content scale is the ratio between the current DPI and the platform's default DPI.
     * This is especially important for text and any UI elements. If the pixel dimensions of
     * your UI scaled by this look appropriate on your machine then it should appear at a
     * reasonable size on other machines regardless of their DPI and scaling settings.
     * This relies on the system DPI and scaling settings being somewhat correct.
     *
     * The content scale returned is only valid for the life of the Monitor.
     * When the Monitor is disconnected, the content scale becomes invalid.
     *
     * Use @ref setMonitorsChangeFn to know when a monitor is disconnected.
     *
     * @param monitor The monitor to use.
     * @return The monitor content scale (dpi).
     */
    Float2(CARB_ABI* getMonitorContentScale)(const Monitor* monitor);

    /**
     * Gets a monitors work area.
     *
     * The area of a monitor not occupied by global task bars or
     * menu bars is the work area
     *
     * The work area returned is only valid for the life of the Monitor.
     * When the Monitor is disconnected, the work area becomes invalid.
     *
     * Use @ref setMonitorsChangeFn to know when a monitor is disconnected.
     *
     * @param monitor The monitor to use.
     * @param position The returned position.
     * @param size The returned size.
     */
    void(CARB_ABI* getMonitorWorkArea)(const Monitor* monitor, Int2* positionOut, Int2* sizeOut);

    /**
     * Sets the function for handling move events. Must be called on a main thread.
     *
     * @param window The window to use (shouldn't be nullptr).
     * @param onWindowMove The function callback to handle move events on the window (can be nullptr).
     * @param userData User-specified pointer to the data. Lifetime and value can be anything.
     */
    void(CARB_ABI* setWindowMoveFn)(Window* window, OnWindowMoveFn onWindowMove, void* userData);

    /**
     * Determines if the window is floating (or always-on-top).
     *
     * @param window The window to use.
     * @return true if the window is floating.
     */
    bool(CARB_ABI* isWindowFloating)(Window* window);

    /**
     * Sets the window into floating (always-on-top) or regular mode.
     *
     * @param window The window to use.
     * @param fullscreen true to be set to floating (always-on-top), false to be set to regular.
     */
    void(CARB_ABI* setWindowFloating)(Window* window, bool isFloating);

    /**
     * Creates a new custom cursor image that can be set for a window with @ref setCursor. The cursor can be destroyed
     * with @ref destroyCursor.
     *
     * The pixels are 32-bit, little-endian, non-premultiplied RGBA, i.e. eight bits per channel with the red channel
     * first. They are arranged canonically as packed sequential rows, starting from the top-left corner.
     *
     * The cursor hotspot is specified in pixels, relative to the upper-left corner of the cursor image. Like all other
     * coordinate systems in GLFW, the X-axis points to the right and the Y-axis points down.
     *
     * @param image	The desired cursor image.
     * @param xhot The desired x-coordinate, in pixels, of the cursor hotspot.
     * @param yhot The desired y-coordinate, in pixels, of the cursor hotspot.
     * @return created cursor, or nullptr if error occurred.
     */
    Cursor*(CARB_ABI* createCursor)(const Image& image, int32_t xhot, int32_t yhot);

    /**
     * Maximize the window.
     *
     * @param window The window to use.
     */
    void(CARB_ABI* maximizeWindow)(Window* window);

    /**
     * Minimize the window.
     *
     * @param window The window to use.
     */
    void(CARB_ABI* minimizeWindow)(Window* window);

    /**
     * Restore the window.
     *
     * @param window The window to use.
     */
    void(CARB_ABI* restoreWindow)(Window* window);

    /**
     * Sets the function for handling window maximize events.
     *
     * @param window The window to use.
     * @param onWindowMaximizeFn The function callback to handle maximize events on the window.
     */
    void(CARB_ABI* setWindowMaximizeFn)(Window* window, OnWindowMaximizeFn onWindowMaximizeFn, void* userData);

    /**
     * Determines if the window is maximized.
     *
     * @param window The window to use.
     * @return true if the window is maximized, false if it is not.
     */
    bool(CARB_ABI* isWindowMaximized)(Window* window);
};

} // namespace windowing
} // namespace carb
