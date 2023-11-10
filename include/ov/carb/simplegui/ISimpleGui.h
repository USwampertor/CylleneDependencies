// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief *carb.simplegui* interface definition file.
#pragma once

#include "SimpleGuiTypes.h"

#include "../Interface.h"

namespace carb
{
//! Namespace for *carb.simplegui* plugin.
namespace simplegui
{

/**
 * Defines the simplegui interface.
 *
 * Based on: ImGui 1.70
 */
struct ISimpleGui
{
    CARB_PLUGIN_INTERFACE("carb::simplegui::ISimpleGui", 1, 1)

    /**
     * Creates a new immediate mode GUI context.
     *
     * @param desc Context descriptor.
     * @return The context used for the current state of the GUI.
     */
    Context*(CARB_ABI* createContext)(const ContextDesc& desc);

    /**
     * Destroys an immediate mode GUI context.
     *
     * @param ctx The context to be destroyed.
     */
    void(CARB_ABI* destroyContext)(Context* ctx);

    /**
     * Sets the current immediate mode GUI context to be used.
     */
    void(CARB_ABI* setCurrentContext)(Context* ctx);

    /**
     * Gets the current immediate mode GUI context that is used.
     * @return returns current immediate GUI mode context.
     */
    Context*(CARB_ABI* getCurrentContext)();

    /**
     * Start rendering a new immediate mode GUI frame.
     */
    void(CARB_ABI* newFrame)();

    /**
     * Render the immediate mode GUI frame.
     *
     * @param elapsedTime The amount of elapsed time since the last render() call.
     */
    void(CARB_ABI* render)(float elapsedTime);

    /**
     * Sets the display size.
     *
     * @param size The display size to be set.
     */
    void(CARB_ABI* setDisplaySize)(Float2 size);

    /**
     * Gets the display size.
     *
     * @return The display size.
     */
    Float2(CARB_ABI* getDisplaySize)();

    /**
     * Gets the style struct.
     *
     * @return The style struct.
     */
    Style*(CARB_ABI* getStyle)();

    /**
     * Shows a demo window of all features supported.
     *
     * @param open Is window opened, can be changed after the call.
     */
    void(CARB_ABI* showDemoWindow)(bool* open);

    /**
     * Create metrics window.
     *
     * Display simplegui internals: draw commands (with individual draw calls and vertices), window list, basic internal
     * state, etc.
     *
     * @param open Is window opened, can be changed after the call.
     */
    void(CARB_ABI* showMetricsWindow)(bool* open);

    /**
     * Add style editor block (not a window).
     *
     * You can pass in a reference ImGuiStyle structure to compare to, revert to and save to (else it uses the default
     * style)
     *
     * @param style Style to edit. Pass nullptr to edit the default one.
     */
    void(CARB_ABI* showStyleEditor)(Style* style);

    /**
     * Add style selector block (not a window).
     *
     * Essentially a combo listing the default styles.
     *
     * @param label Label.
     */
    bool(CARB_ABI* showStyleSelector)(const char* label);

    /**
     * Add font selector block (not a window).
     *
     * Essentially a combo listing the loaded fonts.
     *
     * @param label Label.
     */
    void(CARB_ABI* showFontSelector)(const char* label);

    /**
     * Add basic help/info block (not a window): how to manipulate simplegui as a end-user (mouse/keyboard controls).
     */
    void(CARB_ABI* showUserGuide)();

    /**
     * Get underlying ImGui library version string.
     *
     * @return The version string e.g. "1.70".
     */
    const char*(CARB_ABI* getImGuiVersion)();

    /**
     * Set style colors from one of the predefined presets:
     *
     * @param style Style to change. Pass nullptr to change the default one.
     * @param preset Colors preset.
     */
    void(CARB_ABI* setStyleColors)(Style* style, StyleColorsPreset preset);

    /**
     * Begins defining a new immediate mode GUI (window).
     *
     * @param label The window label.
     * @param open Returns if the window was active.
     * @param windowFlags The window flags to be used.
     * @return return false to indicate the window is collapsed or fully clipped,
     *  so you may early out and omit submitting anything to the window.
     */
    bool(CARB_ABI* begin)(const char* label, bool* open, WindowFlags flags);

    /**
     * Ends defining a immediate mode.
     */
    void(CARB_ABI* end)();

    /**
     * Begins a scrolling child region.
     *
     * @param strId The string identifier for the child region.
     * @param size The size of the region.
     *  size==0.0f: use remaining window size, size<0.0f: use remaining window.
     * @param border Draw border.
     * @param flags Sets any WindowFlags for the dialog.
     * @return true if the region change, false if not.
     */
    bool(CARB_ABI* beginChild)(const char* strId, Float2 size, bool border, WindowFlags flags);

    /**
     * Begins a scrolling child region.
     *
     * @param id The identifier for the child region.
     * @param size The size of the region.
     *  size==0.0f: use remaining window size, size<0.0f: use remaining window.
     * @param border Draw border.
     * @param flags Sets any WindowFlags for the dialog.
     * @return true if the region change, false if not.
     */
    bool(CARB_ABI* beginChildId)(uint32_t id, Float2 size, bool border, WindowFlags flags);

    /**
     * Ends a child region.
     */
    void(CARB_ABI* endChild)();

    /**
     * Is window appearing?
     *
     * @return true if window is appearing, false if not.
     */
    bool(CARB_ABI* isWindowAppearing)();

    /**
     * Is window collapsed?
     *
     * @return true if window is collapsed, false is not.
     */
    bool(CARB_ABI* isWindowCollapsed)();

    /**
     * Is current window focused? or its root/child, depending on flags. see flags for options.
     *
     * @param flags The focused flags to use.
     * @return true if window is focused, false if not.
     */
    bool(CARB_ABI* isWindowFocused)(FocusedFlags flags);

    /**
     * Is current window hovered (and typically: not blocked by a popup/modal)? see flags for options.
     *
     * If you are trying to check whether your mouse should be dispatched to simplegui or to your app, you should use
     * the 'io.WantCaptureMouse' boolean for that! Please read the FAQ!
     *
     * @param flags The hovered flags to use.
     * @return true if window is currently hovered over, false if not.
     */
    bool(CARB_ABI* isWindowHovered)(HoveredFlags flags);

    /**
     * Get the draw list associated to the window, to append your own drawing primitives.
     *
     * @return The draw list associated to the window, to append your own drawing primitives.
     */
    DrawList*(CARB_ABI* getWindowDrawList)();

    /**
     * Gets the DPI scale currently associated to the current window's viewport.
     *
     * @return The DPI scale currently associated to the current window's viewport.
     */
    float(CARB_ABI* getWindowDpiScale)();

    /**
     * Get current window position in screen space
     *
     * This is useful if you want to do your own drawing via the DrawList API.
     *
     * @return The current window position in screen space.
     */
    Float2(CARB_ABI* getWindowPos)();

    /**
     * Gets the current window size.
     *
     * @return The current window size
     */
    Float2(CARB_ABI* getWindowSize)();

    /**
     * Gets the current window width.
     *
     * @return The current window width.
     */
    float(CARB_ABI* getWindowWidth)();

    /**
     * Gets the current window height.
     *
     * @return The current window height.
     */
    float(CARB_ABI* getWindowHeight)();

    /**
     * Gets the current content boundaries.
     *
     * This is typically window boundaries including scrolling,
     * or current column boundaries, in windows coordinates.
     *
     * @return The current content boundaries.
     */
    Float2(CARB_ABI* getContentRegionMax)();

    /**
     * Gets the current content region available.
     *
     * This is: getContentRegionMax() - getCursorPos()
     *
     * @return The current content region available.
     */
    Float2(CARB_ABI* getContentRegionAvail)();

    /**
     * Gets the width of the current content region available.
     *
     * @return The width of the current content region available.
     */
    float(CARB_ABI* ContentRegionAvailWidth)();

    /**
     * Content boundaries min (roughly (0,0)-Scroll), in window coordinates
     */
    Float2(CARB_ABI* getWindowContentRegionMin)();

    /**
     * Gets the maximum content boundaries.
     *
     * This is roughly (0,0)+Size-Scroll) where Size can be override with SetNextWindowContentSize(),
     * in window coordinates.
     *
     * @return The maximum content boundaries.
     */
    Float2(CARB_ABI* getWindowContentRegionMax)();

    /**
     * Content region width.
     */
    float(CARB_ABI* getWindowContentRegionWidth)();

    /**
     * Sets the next window position.
     *
     * Call before begin(). use pivot=(0.5f,0.5f) to center on given point, etc.
     *
     * @param position The position to set.
     * @param pivot The offset pivot.
     */
    void(CARB_ABI* setNextWindowPos)(Float2 position, Condition cond, Float2 pivot);

    /**
     * Set next window size.
     *
     * Set axis to 0.0f to force an auto-fit on this axis. call before begin()
     *
     * @param size The next window size.
     */
    void(CARB_ABI* setNextWindowSize)(Float2 size, Condition cond);

    /**
     * Set next window size limits. use -1,-1 on either X/Y axis to preserve the current size. Use callback to apply
     * non-trivial programmatic constraints.
     */
    void(CARB_ABI* setNextWindowSizeConstraints)(const Float2& sizeMin, const Float2& sizeMax);

    /**
     * Set next window content size (~ enforce the range of scrollbars). not including window decorations (title bar,
     * menu bar, etc.). set an axis to 0.0f to leave it automatic. call before Begin()
     */
    void(CARB_ABI* setNextWindowContentSize)(const Float2& size);

    /**
     * Set next window collapsed state. call before Begin()
     */
    void(CARB_ABI* setNextWindowCollapsed)(bool collapsed, Condition cond);

    /**
     * Set next window to be focused / front-most. call before Begin()
     */
    void(CARB_ABI* setNextWindowFocus)();

    /**
     * Set next window background color alpha. helper to easily modify ImGuiCol_WindowBg/ChildBg/PopupBg.
     */
    void(CARB_ABI* setNextWindowBgAlpha)(float alpha);

    /**
     * Set font scale. Adjust IO.FontGlobalScale if you want to scale all windows
     */
    void(CARB_ABI* setWindowFontScale)(float scale);

    /**
     * Set named window position.
     */
    void(CARB_ABI* setWindowPos)(const char* name, const Float2& pos, Condition cond);

    /**
     * Set named window size. set axis to 0.0f to force an auto-fit on this axis.
     */
    void(CARB_ABI* setWindowSize)(const char* name, const Float2& size, Condition cond);

    /**
     * Set named window collapsed state
     */
    void(CARB_ABI* setWindowCollapsed)(const char* name, bool collapsed, Condition cond);

    /**
     * Set named window to be focused / front-most. use NULL to remove focus.
     */
    void(CARB_ABI* setWindowFocus)(const char* name);

    /**
     * Get scrolling amount [0..GetScrollMaxX()]
     */
    float(CARB_ABI* getScrollX)();

    /**
     * Get scrolling amount [0..GetScrollMaxY()]
     */
    float(CARB_ABI* getScrollY)();

    /**
     * Get maximum scrolling amount ~~ ContentSize.X - WindowSize.X
     */
    float(CARB_ABI* getScrollMaxX)();

    /**
     * Get maximum scrolling amount ~~ ContentSize.Y - WindowSize.Y
     */
    float(CARB_ABI* getScrollMaxY)();

    /**
     * Set scrolling amount [0..GetScrollMaxX()]
     */
    void(CARB_ABI* setScrollX)(float scrollX);

    /**
     * Set scrolling amount [0..GetScrollMaxY()]
     */
    void(CARB_ABI* setScrollY)(float scrollY);

    /**
     * Adjust scrolling amount to make current cursor position visible.
     *
     * @param centerYRatio Center y ratio: 0.0: top, 0.5: center, 1.0: bottom.
     */
    void(CARB_ABI* setScrollHereY)(float centerYRatio);

    /**
     * Adjust scrolling amount to make given position valid. use getCursorPos() or getCursorStartPos()+offset to get
     * valid positions. default: centerYRatio = 0.5f
     */
    void(CARB_ABI* setScrollFromPosY)(float posY, float centerYRatio);

    /**
     * Use NULL as a shortcut to push default font
     */
    void(CARB_ABI* pushFont)(Font* font);

    /**
     * Pop font from the stack
     */
    void(CARB_ABI* popFont)();

    /**
     * Pushes and applies a style color for the current widget.
     *
     * @param styleColorIndex The style color index.
     * @param color The color to be applied for the style color being pushed.
     */
    void(CARB_ABI* pushStyleColor)(StyleColor styleColorIndex, Float4 color);

    /**
     * Pops off and stops applying the style color for the current widget.
     */
    void(CARB_ABI* popStyleColor)();

    /**
     * Pushes a style variable(property) with a float value.
     *
     * @param styleVarIndex The style variable(property) index.
     * @param value The value to be applied.
     */
    void(CARB_ABI* pushStyleVarFloat)(StyleVar styleVarIndex, float value);

    /**
     * Pushes a style variable(property) with a Float2 value.
     *
     * @param styleVarIndex The style variable(property) index.
     * @param value The value to be applied.
     */
    void(CARB_ABI* pushStyleVarFloat2)(StyleVar styleVarIndex, Float2 value);

    /**
     * Pops off and stops applying the style variable(property) for the current widget.
     */
    void(CARB_ABI* popStyleVar)();

    /**
     * Retrieve style color as stored in ImGuiStyle structure. use to feed back into PushStyleColor(), otherwise use
     * GetColorU32() to get style color with style alpha baked in.
     */
    const Float4&(CARB_ABI* getStyleColorVec4)(StyleColor colorIndex);

    /**
     * Get current font
     */
    Font*(CARB_ABI* getFont)();

    /**
     * Get current font size (= height in pixels) of current font with current scale applied
     */
    float(CARB_ABI* getFontSize)();

    /**
     * Get UV coordinate for a while pixel, useful to draw custom shapes via the ImDrawList API
     */
    Float2(CARB_ABI* getFontTexUvWhitePixel)();

    /**
     * Retrieve given style color with style alpha applied and optional extra alpha multiplier
     */
    uint32_t(CARB_ABI* getColorU32StyleColor)(StyleColor colorIndex, float alphaMul);

    /**
     * Retrieve given color with style alpha applied
     */
    uint32_t(CARB_ABI* getColorU32Vec4)(Float4 color);

    /**
     * Retrieve given color with style alpha applied
     */
    uint32_t(CARB_ABI* getColorU32)(uint32_t color);

    /**
     * Push an item width for next widgets.
     * In pixels. 0.0f = default to ~2/3 of windows width, >0.0f: width in pixels, <0.0f align xx pixels to the right of
     * window (so -1.0f always align width to the right side).
     *
     * @param width The width.
     */
    void(CARB_ABI* pushItemWidth)(float width);

    /**
     * Pops an item width.
     */
    void(CARB_ABI* popItemWidth)();

    /**
     * Size of item given pushed settings and current cursor position
     * NOTE: This is not the same as calcItemWidth
     */
    carb::Float2(CARB_ABI* calcItemSize)(carb::Float2 size, float defaultX, float defaultY);

    /**
     * Width of item given pushed settings and current cursor position
     */
    float(CARB_ABI* calcItemWidth)();

    //! @private Unknown/Undocumented
    void(CARB_ABI* pushItemFlag)(ItemFlags option, bool enabled);

    //! @private Unknown/Undocumented
    void(CARB_ABI* popItemFlag)();

    /**
     * Word-wrapping for Text*() commands. < 0.0f: no wrapping; 0.0f: wrap to end of window (or column); > 0.0f: wrap at
     * 'wrapPosX' position in window local space
     */
    void(CARB_ABI* pushTextWrapPos)(float wrapPosX);

    /**
     * Pop text wrap pos form the stack
     */
    void(CARB_ABI* popTextWrapPos)();

    /**
     * Allow focusing using TAB/Shift-TAB, enabled by default but you can disable it for certain widgets
     */
    void(CARB_ABI* pushAllowKeyboardFocus)(bool allow);

    /**
     * Pop allow keyboard focus
     */
    void(CARB_ABI* popAllowKeyboardFocus)();

    /**
     * In 'repeat' mode, Button*() functions return repeated true in a typematic manner (using
     * io.KeyRepeatDelay/io.KeyRepeatRate setting). Note that you can call IsItemActive() after any Button() to tell if
     * the button is held in the current frame.
     */
    void(CARB_ABI* pushButtonRepeat)(bool repeat);

    /**
     * Pop button repeat
     */
    void(CARB_ABI* popButtonRepeat)();

    /**
     * Adds a widget separator.
     */
    void(CARB_ABI* separator)();

    /**
     * Tell the widget to stay on the same line.
     */
    void sameLine();

    /**
     * Tell the widget to stay on the same line with parameters.
     */
    void(CARB_ABI* sameLineEx)(float posX, float spacingW);

    /**
     * Undo sameLine()
     */
    void(CARB_ABI* newLine)();

    /**
     * Adds widget spacing.
     */
    void(CARB_ABI* spacing)();

    /**
     * Adds a dummy element of a given size
     *
     * @param size The size of a dummy element.
     */
    void(CARB_ABI* dummy)(Float2 size);

    /**
     * Indents.
     */
    void(CARB_ABI* indent)();

    /**
     * Indents with width indent spacing.
     */
    void(CARB_ABI* indentEx)(float indentWidth);

    /**
     * Undo indent.
     */
    void(CARB_ABI* unindent)();

    /**
     * Lock horizontal starting position + capture group bounding box into one "item" (so you can use IsItemHovered() or
     * layout primitives such as () on whole group, etc.)
     */
    void(CARB_ABI* beginGroup)();

    /**
     * End group
     */
    void(CARB_ABI* endGroup)();

    /**
     * Cursor position is relative to window position
     */
    Float2(CARB_ABI* getCursorPos)();

    //! @private Unknown/Undocumented
    float(CARB_ABI* getCursorPosX)();

    //! @private Unknown/Undocumented
    float(CARB_ABI* getCursorPosY)();

    //! @private Unknown/Undocumented
    void(CARB_ABI* setCursorPos)(const Float2& localPos);

    //! @private Unknown/Undocumented
    void(CARB_ABI* setCursorPosX)(float x);

    //! @private Unknown/Undocumented
    void(CARB_ABI* setCursorPosY)(float y);

    /**
     * Initial cursor position
     */
    Float2(CARB_ABI* getCursorStartPos)();

    /**
     * Cursor position in absolute screen coordinates [0..io.DisplaySize] (useful to work with ImDrawList API)
     */
    Float2(CARB_ABI* getCursorScreenPos)();

    /**
     * Cursor position in absolute screen coordinates [0..io.DisplaySize]
     */
    void(CARB_ABI* setCursorScreenPos)(const Float2& pos);

    /**
     * Vertically align upcoming text baseline to FramePadding.y so that it will align properly to regularly framed
     * items (call if you have text on a line before a framed item)
     */
    void(CARB_ABI* alignTextToFramePadding)();

    /**
     * ~ FontSize
     */
    float(CARB_ABI* getTextLineHeight)();

    /**
     * ~ FontSize + style.ItemSpacing.y (distance in pixels between 2 consecutive lines of text)
     */
    float(CARB_ABI* getTextLineHeightWithSpacing)();

    /**
     * ~ FontSize + style.FramePadding.y * 2
     */
    float(CARB_ABI* getFrameHeight)();

    /**
     * ~ FontSize + style.FramePadding.y * 2 + style.ItemSpacing.y (distance in pixels between 2 consecutive lines of
     * framed widgets)
     */
    float(CARB_ABI* getFrameHeightWithSpacing)();

    /**
     * Push a string id for next widgets.
     *
     * If you are creating widgets in a loop you most likely want to push a unique identifier (e.g. object pointer, loop
     * index) so simplegui can differentiate them. popId() must be called later.
     * @param id The string id.
     */
    void(CARB_ABI* pushIdString)(const char* id);

    /**
     * Push a string id for next widgets.
     *
     * If you are creating widgets in a loop you most likely want to push a unique identifier (e.g. object pointer, loop
     * index) so simplegui can differentiate them. popId() must be called later.
     * @param id The string id.
     */
    void(CARB_ABI* pushIdStringBeginEnd)(const char* idBegin, const char* idEnd);

    /**
     * Push an integer id for next widgets.
     *
     * If you are creating widgets in a loop you most likely want to push a unique identifier (e.g. object pointer, loop
     * index) so simplegui can differentiate them. popId() must be called later.
     * @param id The integer id.
     */
    void(CARB_ABI* pushIdInt)(int id);

    /**
     * Push pointer id for next widgets.
     */
    void(CARB_ABI* pushIdPtr)(const void* id);

    /**
     * Pops an id.
     */
    void(CARB_ABI* popId)();

    /**
     * Calculate unique ID (hash of whole ID stack + given parameter). e.g. if you want to query into ImGuiStorage
     * yourself.
     */
    uint32_t(CARB_ABI* getIdString)(const char* id);

    /**
     * Calculate unique ID (hash of whole ID stack + given parameter). e.g. if you want to query into ImGuiStorage
     * yourself.
     */
    uint32_t(CARB_ABI* getIdStringBeginEnd)(const char* idBegin, const char* idEnd);

    //! @private Unknown/Undocumented
    uint32_t(CARB_ABI* getIdPtr)(const void* id);

    /**
     * Shows a text widget, without text formatting. Faster version, use for big texts.
     *
     * @param text The null terminated text string.
     */
    void(CARB_ABI* textUnformatted)(const char* text);


    /**
     * Shows a text widget.
     *
     * @param fmt The formatted label for the text.
     * @param ... The variable arguments for the label.
     */
    void(CARB_ABI* text)(const char* fmt, ...);

    /**
     * Shows a colored text widget.
     *
     * @param fmt The formatted label for the text.
     * @param ... The variable arguments for the label.
     */
    void(CARB_ABI* textColored)(const Float4& color, const char* fmt, ...);

    /**
     * Shows a disabled text widget.
     *
     * @param fmt The formatted label for the text.
     * @param ... The variable arguments for the label.
     */
    void(CARB_ABI* textDisabled)(const char* fmt, ...);

    /**
     * Shows a wrapped text widget.
     *
     * @param fmt The formatted label for the text.
     * @param ... The variable arguments for the label.
     */
    void(CARB_ABI* textWrapped)(const char* fmt, ...);

    /**
     * Display text+label aligned the same way as value+label widgets.
     */
    void(CARB_ABI* labelText)(const char* label, const char* fmt, ...);

    /**
     * Shortcut for Bullet()+Text()
     */
    void(CARB_ABI* bulletText)(const char* fmt, ...);

    /**
     * Shows a button widget.
     *
     * @param label The label for the button.
     * @return true if the button was pressed, false if not.
     */
    bool(CARB_ABI* buttonEx)(const char* label, const Float2& size);

    //! @private Unknown/Undocumented
    bool button(const char* label);

    /**
     * Shows a smallButton widget.
     *
     * @param label The label for the button.
     * @return true if the button was pressed, false if not.
     */
    bool(CARB_ABI* smallButton)(const char* label);

    /**
     * Button behavior without the visuals.
     *
     * Useful to build custom behaviors using the public api (along with isItemActive, isItemHovered, etc.)
     */
    bool(CARB_ABI* invisibleButton)(const char* id, const Float2& size);

    /**
     * Arrow-like button with specified direction.
     */
    bool(CARB_ABI* arrowButton)(const char* id, Direction dir);

    /**
     * Image with user texture id
     * defaults:
     *      uv0 = Float2(0,0)
     *      uv1 = Float2(1,1)
     *      tintColor = Float4(1,1,1,1)
     *      borderColor = Float4(0,0,0,0)
     */
    void(CARB_ABI* image)(TextureId userTextureId,
                          const Float2& size,
                          const Float2& uv0,
                          const Float2& uv1,
                          const Float4& tintColor,
                          const Float4& borderColor);

    /**
     * Image as a button. <0 framePadding uses default frame padding settings. 0 for no padding
     * defaults:
     *     uv0 = Float2(0,0)
     *     uv1 = Float2(1,1)
     *     framePadding = -1
     *     bgColor = Float4(0,0,0,0)
     *     tintColor = Float4(1,1,1,1)
     */
    bool(CARB_ABI* imageButton)(TextureId userTextureId,
                                const Float2& size,
                                const Float2& uv0,
                                const Float2& uv1,
                                int framePadding,
                                const Float4& bgColor,
                                const Float4& tintColor);

    /**
     * Adds a checkbox widget.
     *
     * @param label The checkbox label.
     * @param value The current value of the checkbox
     *
     * @return true if the checkbox was pressed, false if not.
     */
    bool(CARB_ABI* checkbox)(const char* label, bool* value);

    /**
     * Flags checkbox
     */
    bool(CARB_ABI* checkboxFlags)(const char* label, uint32_t* flags, uint32_t flagsValue);

    /**
     * Radio button
     */
    bool(CARB_ABI* radioButton)(const char* label, bool active);

    /**
     * Radio button
     */
    bool(CARB_ABI* radioButtonEx)(const char* label, int* v, int vButton);

    /**
     * Adds a progress bar widget.
     *
     * @param fraction The progress value (0-1).
     * @param size The widget size.
     * @param overlay The text overlay, if nullptr the default with percents is displayed.
     */
    void(CARB_ABI* progressBar)(float fraction, Float2 size, const char* overlay);

    /**
     * Draws a small circle.
     */
    void(CARB_ABI* bullet)();

    /**
     * The new beginCombo()/endCombo() api allows you to manage your contents and selection state however you want it.
     * The old Combo() api are helpers over beginCombo()/endCombo() which are kept available for convenience purpose.
     */
    bool(CARB_ABI* beginCombo)(const char* label, const char* previewValue, ComboFlags flags);

    /**
     * only call endCombo() if beginCombo() returns true!
     */
    void(CARB_ABI* endCombo)();

    /**
     * Adds a combo box widget.
     *
     * @param label The label for the combo box.
     * @param currentItem The current (selected) element index.
     * @param items The array of items.
     * @param itemCount The number of items.
     *
     * @return true if the selected item value has changed, false if not.
     */
    bool(CARB_ABI* combo)(const char* label, int* currentItem, const char* const* items, int itemCount);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). If vMin >= vMax we have no bound. For all the Float2/Float3/Float4/Int2/Int3/Int4 versions of
     * every functions, note that a 'float v[X]' function argument is the same as 'float* v', the array syntax is just a
     * way to document the number of elements that are expected to be accessible. You can pass address of your first
     * element out of a contiguous set, e.g. &myvector.x. Speed are per-pixel of mouse movement (vSpeed=0.2f: mouse
     * needs to move by 5 pixels to increase value by 1). For gamepad/keyboard navigation, minimum speed is Max(vSpeed,
     * minimum_step_at_given_precision). Defaults: float vSpeed = 1.0f, float vMin = 0.0f, float vMax = 0.0f, const
     * char* displayFormat = "%.3f", float power = 1.0f)
     */
    bool(CARB_ABI* dragFloat)(
        const char* label, float* v, float vSpeed, float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds) Defaults: float vSpeed = 1.0f, float vMin = 0.0f, float vMax = 0.0f, const char* displayFormat =
     * "%.3f", float power = 1.0f)
     */
    bool(CARB_ABI* dragFloat2)(
        const char* label, float v[2], float vSpeed, float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds) Defaults: float vSpeed = 1.0f, float vMin = 0.0f, float vMax = 0.0f, const char* displayFormat =
     * "%.3f", float power = 1.0f)
     */
    bool(CARB_ABI* dragFloat3)(
        const char* label, float v[3], float vSpeed, float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds) Defaults: float vSpeed = 1.0f, float vMin = 0.0f, float vMax = 0.0f, const char* displayFormat =
     * "%.3f", float power = 1.0f)
     */
    bool(CARB_ABI* dragFloat4)(
        const char* label, float v[4], float vSpeed, float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: float vSpeed = 1.0f, float vMin = 0.0f, float vMax = 0.0f, const char* displayFormat =
     * "%.3f", const char* displayFormatMax = NULL, float power = 1.0f
     */
    bool(CARB_ABI* dragFloatRange2)(const char* label,
                                    float* vCurrentMin,
                                    float* vCurrentMax,
                                    float vSpeed,
                                    float vMin,
                                    float vMax,
                                    const char* displayFormat,
                                    const char* displayFormatMax,
                                    float power);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). If vMin >= vMax we have no bound.. Defaults: float vSpeed = 1.0f, int vMin = 0, int vMax = 0,
     * const char* displayFormat = "%.0f"
     */
    bool(CARB_ABI* dragInt)(const char* label, int* v, float vSpeed, int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: float vSpeed = 1.0f, int vMin = 0, int vMax = 0, const char* displayFormat = "%.0f"
     */
    bool(CARB_ABI* dragInt2)(const char* label, int v[2], float vSpeed, int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: float vSpeed = 1.0f, int vMin = 0, int vMax = 0, const char* displayFormat = "%.0f"
     */
    bool(CARB_ABI* dragInt3)(const char* label, int v[3], float vSpeed, int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: float vSpeed = 1.0f, int vMin = 0, int vMax = 0, const char* displayFormat = "%.0f"
     */
    bool(CARB_ABI* dragInt4)(const char* label, int v[4], float vSpeed, int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: float vSpeed = 1.0f, int vMin = 0, int vMax = 0, const char* displayFormat = "%.0f",
     * const char* displayFormatMax = NULL
     */
    bool(CARB_ABI* dragIntRange2)(const char* label,
                                  int* vCurrentMin,
                                  int* vCurrentMax,
                                  float vSpeed,
                                  int vMin,
                                  int vMax,
                                  const char* displayFormat,
                                  const char* displayFormatMax);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). If vMin >= vMax we have no bound.. Defaults: float vSpeed = 1.0f, int vMin = 0, int vMax = 0,
     * const char* displayFormat = "%.0f", power = 1.0f
     */
    bool(CARB_ABI* dragScalar)(const char* label,
                               DataType dataType,
                               void* v,
                               float vSpeed,
                               const void* vMin,
                               const void* vMax,
                               const char* displayFormat,
                               float power);

    /**
     * Widgets: Drags (tip: ctrl+click on a drag box to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). If vMin >= vMax we have no bound.. Defaults: float vSpeed = 1.0f, int vMin = 0, int vMax = 0,
     * const char* displayFormat = "%.0f", power = 1.0f
     */
    bool(CARB_ABI* dragScalarN)(const char* label,
                                DataType dataType,
                                void* v,
                                int components,
                                float vSpeed,
                                const void* vMin,
                                const void* vMax,
                                const char* displayFormat,
                                float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Adjust displayFormat to decorate the value with a prefix or a suffix for in-slider labels or unit
     * display. Use power!=1.0 for logarithmic sliders . Defaults: const char* displayFormat = "%.3f", float power
     * = 1.0f
     */
    bool(CARB_ABI* sliderFloat)(const char* label, float* v, float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.3f", float power = 1.0f
     */
    bool(CARB_ABI* sliderFloat2)(
        const char* label, float v[2], float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.3f", float power = 1.0f
     */
    bool(CARB_ABI* sliderFloat3)(
        const char* label, float v[3], float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.3f", float power = 1.0f
     */
    bool(CARB_ABI* sliderFloat4)(
        const char* label, float v[4], float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: float vDegreesMin = -360.0f, float vDegreesMax = +360.0f
     */
    bool(CARB_ABI* sliderAngle)(const char* label, float* vRad, float vDegreesMin, float vDegreesMax);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f"
     */
    bool(CARB_ABI* sliderInt)(const char* label, int* v, int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f"
     */
    bool(CARB_ABI* sliderInt2)(const char* label, int v[2], int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f"
     */
    bool(CARB_ABI* sliderInt3)(const char* label, int v[3], int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f"
     */
    bool(CARB_ABI* sliderInt4)(const char* label, int v[4], int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f", power = 1.0f
     */
    bool(CARB_ABI* sliderScalar)(const char* label,
                                 DataType dataType,
                                 void* v,
                                 const void* vMin,
                                 const void* vMax,
                                 const char* displayFormat,
                                 float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f", power = 1.0f
     */
    bool(CARB_ABI* sliderScalarN)(const char* label,
                                  DataType dataType,
                                  void* v,
                                  int components,
                                  const void* vMin,
                                  const void* vMax,
                                  const char* displayFormat,
                                  float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.3f", float power = 1.0f
     */
    bool(CARB_ABI* vSliderFloat)(
        const char* label, const Float2& size, float* v, float vMin, float vMax, const char* displayFormat, float power);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f"
     */
    bool(CARB_ABI* vSliderInt)(const char* label, const Float2& size, int* v, int vMin, int vMax, const char* displayFormat);

    /**
     * Widgets: Sliders (tip: ctrl+click on a slider to input with keyboard. manually input values aren't clamped, can
     * go off-bounds). Defaults: const char* displayFormat = "%.0f", power = 1.0f
     */
    bool(CARB_ABI* vSliderScalar)(const char* label,
                                  const Float2& size,
                                  DataType dataType,
                                  void* v,
                                  const void* vMin,
                                  const void* vMax,
                                  const char* displayFormat,
                                  float power);

    /**
     * Widgets: Input with Keyboard
     */
    bool(CARB_ABI* inputText)(
        const char* label, char* buf, size_t bufSize, InputTextFlags flags, TextEditCallback callback, void* userData);

    /**
     * Widgets: Input with Keyboard
     */
    bool(CARB_ABI* inputTextMultiline)(const char* label,
                                       char* buf,
                                       size_t bufSize,
                                       const Float2& size,
                                       InputTextFlags flags,
                                       TextEditCallback callback,
                                       void* userData);

    /**
     * Widgets: Input with Keyboard. Defaults: float step = 0.0f, float stepFast = 0.0f, int decimalPrecision = -1,
     * InputTextFlags extraFlags = 0
     */
    bool(CARB_ABI* inputFloat)(
        const char* label, float* v, float step, float stepFast, int decimalPrecision, InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard. Defaults: int decimalPrecision = -1, InputTextFlags extraFlags = 0
     */
    bool(CARB_ABI* inputFloat2)(const char* label, float v[2], int decimalPrecision, InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard. Defaults: int decimalPrecision = -1, InputTextFlags extraFlags = 0
     */
    bool(CARB_ABI* inputFloat3)(const char* label, float v[3], int decimalPrecision, InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard. Defaults: int decimalPrecision = -1, InputTextFlags extraFlags = 0
     */
    bool(CARB_ABI* inputFloat4)(const char* label, float v[4], int decimalPrecision, InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard. Defaults: int step = 1, int stepFast = 100, InputTextFlags extraFlags = 0
     */
    bool(CARB_ABI* inputInt)(const char* label, int* v, int step, int stepFast, InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard
     */
    bool(CARB_ABI* inputInt2)(const char* label, int v[2], InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard
     */
    bool(CARB_ABI* inputInt3)(const char* label, int v[3], InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard
     */
    bool(CARB_ABI* inputInt4)(const char* label, int v[4], InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard. Defaults: double step = 0.0f, double stepFast = 0.0f, const char* displayFormat =
     * "%.6f", InputTextFlags extraFlags = 0
     */
    bool(CARB_ABI* inputDouble)(
        const char* label, double* v, double step, double stepFast, const char* displayFormat, InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard. Defaults: double step = 0.0f, double stepFast = 0.0f, const char* displayFormat =
     * "%.6f", InputTextFlags extraFlags = 0
     */
    bool(CARB_ABI* inputScalar)(const char* label,
                                DataType dataType,
                                void* v,
                                const void* step,
                                const void* stepFast,
                                const char* displayFormat,
                                InputTextFlags extraFlags);

    /**
     * Widgets: Input with Keyboard. Defaults: double step = 0.0f, double stepFast = 0.0f, const char* displayFormat =
     * "%.6f", InputTextFlags extraFlags = 0
     */
    bool(CARB_ABI* inputScalarN)(const char* label,
                                 DataType dataType,
                                 void* v,
                                 int components,
                                 const void* step,
                                 const void* stepFast,
                                 const char* displayFormat,
                                 InputTextFlags extraFlags);

    /**
     * Widgets: Color Editor/Picker (tip: the ColorEdit* functions have a little colored preview square that can be
     * left-clicked to open a picker, and right-clicked to open an option menu.)
     */
    bool(CARB_ABI* colorEdit3)(const char* label, float col[3], ColorEditFlags flags);

    /**
     * Widgets: Color Editor/Picker (tip: the ColorEdit* functions have a little colored preview square that can be
     * left-clicked to open a picker, and right-clicked to open an option menu.)
     */
    bool(CARB_ABI* colorEdit4)(const char* label, float col[4], ColorEditFlags flags);

    /**
     * Widgets: Color Editor/Picker (tip: the ColorEdit* functions have a little colored preview square that can be
     * left-clicked to open a picker, and right-clicked to open an option menu.)
     */
    bool(CARB_ABI* colorPicker3)(const char* label, float col[3], ColorEditFlags flags);

    /**
     * Widgets: Color Editor/Picker (tip: the ColorEdit* functions have a little colored preview square that can be
     * left-clicked to open a picker, and right-clicked to open an option menu.)
     */
    bool(CARB_ABI* colorPicker4)(const char* label, float col[4], ColorEditFlags flags, const float* refCol);

    /**
     * display a colored square/button, hover for details, return true when pressed.
     */
    bool(CARB_ABI* colorButton)(const char* descId, const Float4& col, ColorEditFlags flags, Float2 size);

    /**
     * initialize current options (generally on application startup) if you want to select a default format, picker
     * type, etc. User will be able to change many settings, unless you pass the _NoOptions flag to your calls.
     */
    void(CARB_ABI* setColorEditOptions)(ColorEditFlags flags);

    /**
     * Tree node. if returning 'true' the node is open and the tree id is pushed into the id stack. user is responsible
     * for calling TreePop().
     */
    bool(CARB_ABI* treeNode)(const char* label);

    /**
     * Tree node with string id. read the FAQ about why and how to use ID. to align arbitrary text at the same level as
     * a TreeNode() you can use Bullet().
     */
    bool(CARB_ABI* treeNodeString)(const char* strId, const char* fmt, ...);

    /**
     * Tree node with ptr id.
     */
    bool(CARB_ABI* treeNodePtr)(const void* ptrId, const char* fmt, ...);

    /**
     * Tree node with flags.
     */
    bool(CARB_ABI* treeNodeEx)(const char* label, TreeNodeFlags flags);

    /**
     * Tree node with flags and string id.
     */
    bool(CARB_ABI* treeNodeStringEx)(const char* strId, TreeNodeFlags flags, const char* fmt, ...);

    /**
     * Tree node with flags and ptr id.
     */
    bool(CARB_ABI* treeNodePtrEx)(const void* ptrId, TreeNodeFlags flags, const char* fmt, ...);

    /**
     * ~ Indent()+PushId(). Already called by TreeNode() when returning true, but you can call Push/Pop yourself for
     * layout purpose
     */
    void(CARB_ABI* treePushString)(const char* strId);

    //! @private Unknown/Undocumented
    void(CARB_ABI* treePushPtr)(const void* ptrId);

    /**
     * ~ Unindent()+PopId()
     */
    void(CARB_ABI* treePop)();

    /**
     * Advance cursor x position by GetTreeNodeToLabelSpacing()
     */
    void(CARB_ABI* treeAdvanceToLabelPos)();

    /**
     * Horizontal distance preceding label when using TreeNode*() or Bullet() == (g.FontSize + style.FramePadding.x*2)
     * for a regular unframed TreeNode
     */
    float(CARB_ABI* getTreeNodeToLabelSpacing)();

    /**
     * Set next TreeNode/CollapsingHeader open state.
     */
    void(CARB_ABI* setNextTreeNodeOpen)(bool isOpen, Condition cond);

    /**
     * If returning 'true' the header is open. doesn't indent nor push on ID stack. user doesn't have to call TreePop().
     */
    bool(CARB_ABI* collapsingHeader)(const char* label, TreeNodeFlags flags);

    /**
     * When 'open' isn't NULL, display an additional small close button on upper right of the header
     */
    bool(CARB_ABI* collapsingHeaderEx)(const char* label, bool* open, TreeNodeFlags flags);

    /**
     * Selectable. "bool selected" carry the selection state (read-only). Selectable() is clicked is returns true so you
     * can modify your selection state. size.x==0.0: use remaining width, size.x>0.0: specify width. size.y==0.0: use
     * label height, size.y>0.0: specify height.
     */
    bool(CARB_ABI* selectable)(const char* label,
                               bool selected /* = false*/,
                               SelectableFlags flags /* = 0*/,
                               const Float2& size /* = Float2(0,0)*/);

    /**
     * Selectable. "bool* selected" point to the selection state (read-write), as a convenient helper.
     */
    bool(CARB_ABI* selectableEx)(const char* label,
                                 bool* selected,
                                 SelectableFlags flags /* = 0*/,
                                 const Float2& size /* = Float2(0,0)*/);

    /**
     * ListBox.
     */
    bool(CARB_ABI* listBox)(
        const char* label, int* currentItem, const char* const items[], int itemCount, int heightInItems /* = -1*/);

    /**
     * ListBox.
     */
    bool(CARB_ABI* listBoxEx)(const char* label,
                              int* currentItem,
                              bool (*itemsGetterFn)(void* data, int idx, const char** out_text),
                              void* data,
                              int itemCount,
                              int heightInItems /* = -1*/);

    /**
     * ListBox Header. use if you want to reimplement ListBox() will custom data or interactions. make sure to call
     * ListBoxFooter() afterwards.
     */
    bool(CARB_ABI* listBoxHeader)(const char* label, const Float2& size /* = Float2(0,0)*/);

    /**
     * ListBox Header.
     */
    bool(CARB_ABI* listBoxHeaderEx)(const char* label, int itemCount, int heightInItems /* = -1*/);

    /**
     * Terminate the scrolling region
     */
    void(CARB_ABI* listBoxFooter)();

    /**
     * Plot
     * defaults:
     *      valuesOffset = 0
     *      overlayText = nullptr
     *      scaleMin = FLT_MAX
     *      scaleMax = FLT_MAX
     *      graphSize = Float2(0,0)
     *      stride = sizeof(float)
     */
    void(CARB_ABI* plotLines)(const char* label,
                              const float* values,
                              int valuesCount,
                              int valuesOffset,
                              const char* overlayText,
                              float scaleMin,
                              float scaleMax,
                              Float2 graphSize,
                              int stride);

    /**
     * Plot
     * defaults:
     *      valuesOffset = 0
     *      overlayText = nullptr
     *      scaleMin = FLT_MAX
     *      scaleMax = FLT_MAX
     *      graphSize = Float2(0,0)
     */
    void(CARB_ABI* plotLinesEx)(const char* label,
                                float (*valuesGetterFn)(void* data, int idx),
                                void* data,
                                int valuesCount,
                                int valuesOffset,
                                const char* overlayText,
                                float scaleMin,
                                float scaleMax,
                                Float2 graphSize);

    /**
     * Histogram
     * defaults:
     *      valuesOffset = 0
     *      overlayText = nullptr
     *      scaleMin = FLT_MAX
     *      scaleMax = FLT_MAX
     *      graphSize = Float2(0,0)
     *      stride = sizeof(float)
     */
    void(CARB_ABI* plotHistogram)(const char* label,
                                  const float* values,
                                  int valuesCount,
                                  int valuesOffset,
                                  const char* overlayText,
                                  float scaleMin,
                                  float scaleMax,
                                  Float2 graphSize,
                                  int stride);

    /**
     * Histogram
     * defaults:
     *      valuesOffset = 0
     *      overlayText = nullptr
     *      scaleMin = FLT_MAX
     *      scaleMax = FLT_MAX
     *      graphSize = Float2(0,0)
     */
    void(CARB_ABI* plotHistogramEx)(const char* label,
                                    float (*valuesGetterFn)(void* data, int idx),
                                    void* data,
                                    int valuesCount,
                                    int valuesOffset,
                                    const char* overlayText,
                                    float scaleMin,
                                    float scaleMax,
                                    Float2 graphSize);

    /**
     * Widgets: Value() Helpers. Output single value in "name: value" format.
     */
    void(CARB_ABI* valueBool)(const char* prefix, bool b);

    /**
     * Widgets: Value() Helpers. Output single value in "name: value" format.
     */
    void(CARB_ABI* valueInt)(const char* prefix, int v);

    /**
     * Widgets: Value() Helpers. Output single value in "name: value" format.
     */
    void(CARB_ABI* valueUInt32)(const char* prefix, uint32_t v);

    /**
     * Widgets: Value() Helpers. Output single value in "name: value" format.
     */
    void(CARB_ABI* valueFloat)(const char* prefix, float v, const char* floatFormat /* = nullptr*/);

    /**
     * Create and append to a full screen menu-bar.
     */
    bool(CARB_ABI* beginMainMenuBar)();

    /**
     * Only call EndMainMenuBar() if BeginMainMenuBar() returns true!
     */
    void(CARB_ABI* endMainMenuBar)();

    /**
     * Append to menu-bar of current window (requires WindowFlags_MenuBar flag set on parent window).
     */
    bool(CARB_ABI* beginMenuBar)();

    /**
     * Only call EndMenuBar() if BeginMenuBar() returns true!
     */
    void(CARB_ABI* endMenuBar)();

    /**
     * Create a sub-menu entry. only call EndMenu() if this returns true!
     */
    bool(CARB_ABI* beginMenu)(const char* label, bool enabled /* = true*/);

    /**
     * Only call EndMenu() if BeginMenu() returns true!
     */
    void(CARB_ABI* endMenu)();

    /**
     * Return true when activated. shortcuts are displayed for convenience but not processed by simplegui at the moment
     */
    bool(CARB_ABI* menuItem)(const char* label,
                             const char* shortcut /* = NULL*/,
                             bool selected /* = false*/,
                             bool enabled /* = true*/);

    /**
     * Return true when activated + toggle (*pSelected) if pSelected != NULL
     */
    bool(CARB_ABI* menuItemEx)(const char* label, const char* shortcut, bool* pSelected, bool enabled /* = true*/);

    /**
     * Set text tooltip under mouse-cursor, typically use with ISimpleGui::IsItemHovered(). Overrides any previous call
     * to SetTooltip().
     */
    void(CARB_ABI* setTooltip)(const char* fmt, ...);

    /**
     * Begin/append a tooltip window. to create full-featured tooltip (with any kind of contents).
     */
    void(CARB_ABI* beginTooltip)();

    /**
     * End tooltip
     */
    void(CARB_ABI* endTooltip)();

    /**
     * Call to mark popup as open (don't call every frame!). popups are closed when user click outside, or if
     * CloseCurrentPopup() is called within a BeginPopup()/EndPopup() block. By default, Selectable()/MenuItem() are
     * calling CloseCurrentPopup(). Popup identifiers are relative to the current ID-stack (so OpenPopup and BeginPopup
     * needs to be at the same level).
     */
    void(CARB_ABI* openPopup)(const char* strId);

    /**
     * Return true if the popup is open, and you can start outputting to it. only call EndPopup() if BeginPopup()
     * returns true!
     */
    bool(CARB_ABI* beginPopup)(const char* strId, WindowFlags flags /* = 0*/);

    /**
     * Helper to open and begin popup when clicked on last item. if you can pass a NULL strId only if the previous item
     * had an id. If you want to use that on a non-interactive item such as Text() you need to pass in an explicit ID
     * here. read comments in .cpp!
     */
    bool(CARB_ABI* beginPopupContextItem)(const char* strId /* = NULL*/, int mouseButton /* = 1*/);

    /**
     * Helper to open and begin popup when clicked on current window.
     */
    bool(CARB_ABI* beginPopupContextWindow)(const char* strId /* = NULL*/,
                                            int mouseButton /* = 1*/,
                                            bool alsoOverItems /* = true*/);

    /**
     * Helper to open and begin popup when clicked in void (where there are no simplegui windows).
     */
    bool(CARB_ABI* beginPopupContextVoid)(const char* strId /* = NULL*/, int mouseButton /* = 1*/);

    /**
     * Modal dialog (regular window with title bar, block interactions behind the modal window, can't close the modal
     * window by clicking outside)
     */
    bool(CARB_ABI* beginPopupModal)(const char* name, bool* open /* = NULL*/, WindowFlags flags /* = 0*/);

    /**
     * Only call EndPopup() if BeginPopupXXX() returns true!
     */
    void(CARB_ABI* endPopup)();

    /**
     * Helper to open popup when clicked on last item. return true when just opened.
     */
    bool(CARB_ABI* openPopupOnItemClick)(const char* strId /* = NULL*/, int mouseButton /* = 1*/);

    /**
     * Return true if the popup is open
     */
    bool(CARB_ABI* isPopupOpen)(const char* strId);

    /**
     * Close the popup we have begin-ed into. clicking on a MenuItem or Selectable automatically close the current
     * popup.
     */
    void(CARB_ABI* closeCurrentPopup)();

    /**
     * Columns. You can also use SameLine(pos_x) for simplified columns. The columns API is still work-in-progress and
     * rather lacking.
     */
    void(CARB_ABI* columns)(int count /* = 1*/, const char* id /* = NULL*/, bool border /* = true*/);

    /**
     * Next column, defaults to current row or next row if the current row is finished
     */
    void(CARB_ABI* nextColumn)();

    /**
     * Get current column index
     */
    int(CARB_ABI* getColumnIndex)();

    /**
     * Get column width (in pixels). pass -1 to use current column
     */
    float(CARB_ABI* getColumnWidth)(int columnIndex /* = -1*/);

    /**
     * Set column width (in pixels). pass -1 to use current column
     */
    void(CARB_ABI* setColumnWidth)(int columnIndex, float width);

    /**
     * Get position of column line (in pixels, from the left side of the contents region). pass -1 to use current
     * column, otherwise 0..GetColumnsCount() inclusive. column 0 is typically 0.0f
     */
    float(CARB_ABI* getColumnOffset)(int columnIndex /* = -1*/);

    /**
     * Set position of column line (in pixels, from the left side of the contents region). pass -1 to use current column
     */
    void(CARB_ABI* setColumnOffset)(int columnIndex, float offsetX);

    /**
     * Columns count.
     */
    int(CARB_ABI* getColumnsCount)();

    /**
     * Create and append into a TabBar.
     * defaults:
     *  flags = 0
     */
    bool(CARB_ABI* beginTabBar)(const char* strId, TabBarFlags flags);

    /**
     * End TabBar.
     */
    void(CARB_ABI* endTabBar)();

    /**
     * Create a Tab. Returns true if the Tab is selected.
     * defaults:
     *  open = nullptr
     *  flags = 0
     */
    bool(CARB_ABI* beginTabItem)(const char* label, bool* open, TabItemFlags flags);

    /**
     * Only call endTabItem() if beginTabItem() returns true!
     */
    void(CARB_ABI* endTabItem)();

    /**
     * Notify TabBar or Docking system of a closed tab/window ahead (useful to reduce visual flicker on reorderable
     * tab bars). For tab-bar: call after beginTabBar() and before Tab submissions. Otherwise call with a window name.
     */
    void(CARB_ABI* setTabItemClosed)(const char* tabOrDockedWindowLabel);

    /**
     * defaults:
     *  size = Float2(0, 0),
     *  flags = 0,
     *  windowClass = nullptr
     */
    void(CARB_ABI* dockSpace)(uint32_t id, const Float2& size, DockNodeFlags flags, const WindowClass* windowClass);

    /**
     * defaults:
     *  viewport = nullptr,
     *  dockspaceFlags = 0,
     *  windowClass = nullptr
     */
    uint32_t(CARB_ABI* dockSpaceOverViewport)(Viewport* viewport,
                                              DockNodeFlags dockspaceFlags,
                                              const WindowClass* windowClass);

    /**
     * Set next window dock id (FIXME-DOCK).
     */
    void(CARB_ABI* setNextWindowDockId)(uint32_t dockId, Condition cond);

    /**
     * Set next window user type (docking filters by same user_type).
     */
    void(CARB_ABI* setNextWindowClass)(const WindowClass* windowClass);

    /**
     * Get window dock Id.
     */
    uint32_t(CARB_ABI* getWindowDockId)();

    /**
     * Return is window Docked.
     */
    bool(CARB_ABI* isWindowDocked)();

    /**
     * Call when the current item is active. If this return true, you can call setDragDropPayload() +
     * endDragDropSource()
     */
    bool(CARB_ABI* beginDragDropSource)(DragDropFlags flags);

    /**
     * Type is a user defined string of maximum 32 characters. Strings starting with '_' are reserved for simplegui
     * internal types. Data is copied and held by simplegui. Defaults: cond = 0
     */
    bool(CARB_ABI* setDragDropPayload)(const char* type, const void* data, size_t size, Condition cond);

    /**
     * Only call endDragDropSource() if beginDragDropSource() returns true!
     */
    void(CARB_ABI* endDragDropSource)();

    /**
     * Call after submitting an item that may receive a payload. If this returns true, you can call
     * acceptDragDropPayload() + endDragDropTarget()
     */
    bool(CARB_ABI* beginDragDropTarget)();

    /**
     * Accept contents of a given type. If ImGuiDragDropFlags_AcceptBeforeDelivery is set you can peek into the payload
     * before the mouse button is released.
     */
    const Payload*(CARB_ABI* acceptDragDropPayload)(const char* type, DragDropFlags flags);

    /**
     * Only call endDragDropTarget() if beginDragDropTarget() returns true!
     */
    void(CARB_ABI* endDragDropTarget)();

    /**
     * Peek directly into the current payload from anywhere. may return NULL. use ImGuiPayload::IsDataType() to test for
     * the payload type.
     */
    const Payload*(CARB_ABI* getDragDropPayload)();

    /**
     * Clipping.
     */
    void(CARB_ABI* pushClipRect)(const Float2& clipRectMin, const Float2& clipRectMax, bool intersectWithCurrentClipRect);

    /**
     * Clipping.
     */
    void(CARB_ABI* popClipRect)();

    /**
     * Make last item the default focused item of a window. Please use instead of "if (IsWindowAppearing())
     * SetScrollHere()" to signify "default item".
     */
    void(CARB_ABI* setItemDefaultFocus)();

    /**
     * Focus keyboard on the next widget. Use positive 'offset' to access sub components of a multiple component widget.
     * Use -1 to access previous widget.
     */
    void(CARB_ABI* setKeyboardFocusHere)(int offset /* = 0*/);

    /**
     * Clears the active element id in the internal state.
     */
    void(CARB_ABI* clearActiveId)();

    /**
     * Is the last item hovered? (and usable, aka not blocked by a popup, etc.). See HoveredFlags for more options.
     */
    bool(CARB_ABI* isItemHovered)(HoveredFlags flags /* = 0*/);

    /**
     * Is the last item active? (e.g. button being held, text field being edited- items that don't interact will always
     * return false)
     */
    bool(CARB_ABI* isItemActive)();

    /**
     * Is the last item focused for keyboard/gamepad navigation?
     */
    bool(CARB_ABI* isItemFocused)();

    /**
     * Is the last item clicked? (e.g. button/node just clicked on)
     */
    bool(CARB_ABI* isItemClicked)(int mouseButton /* = 0*/);

    /**
     * Is the last item visible? (aka not out of sight due to clipping/scrolling.)
     */
    bool(CARB_ABI* isItemVisible)();

    /**
     * Is the last item visible? (items may be out of sight because of clipping/scrolling)
     */
    bool(CARB_ABI* isItemEdited)();

    /**
     * Was the last item just made inactive (item was previously active).
     *
     * Useful for Undo/Redo patterns with widgets that requires continuous editing.
     */
    bool(CARB_ABI* isItemDeactivated)();

    /**
     * Was the last item just made inactive and made a value change when it was active? (e.g. Slider/Drag moved).
     *
     * Useful for Undo/Redo patterns with widgets that requires continuous editing.
     * Note that you may get false positives (some widgets such as Combo()/ListBox()/Selectable()
     will return true even when clicking an already selected item).
     */
    bool(CARB_ABI* isItemDeactivatedAfterEdit)();

    //! @private Unknown/Undocumented
    bool(CARB_ABI* isAnyItemHovered)();

    //! @private Unknown/Undocumented
    bool(CARB_ABI* isAnyItemActive)();

    //! @private Unknown/Undocumented
    bool(CARB_ABI* isAnyItemFocused)();

    /**
     * Is specific item active?
     */
    bool(CARB_ABI* isItemIdActive)(uint32_t id);

    /**
     * Get bounding rectangle of last item, in screen space
     */
    Float2(CARB_ABI* getItemRectMin)();

    //! @private Unknown/Undocumented
    Float2(CARB_ABI* getItemRectMax)();

    /**
     * Get size of last item, in screen space
     */
    Float2(CARB_ABI* getItemRectSize)();

    /**
     * Allow last item to be overlapped by a subsequent item. sometimes useful with invisible buttons, selectables, etc.
     * to catch unused area.
     */
    void(CARB_ABI* setItemAllowOverlap)();

    /**
     * Test if rectangle (of given size, starting from cursor position) is visible / not clipped.
     */
    bool(CARB_ABI* isRectVisible)(const Float2& size);

    /**
     * Test if rectangle (in screen space) is visible / not clipped. to perform coarse clipping on user's side.
     */
    bool(CARB_ABI* isRectVisibleEx)(const Float2& rectMin, const Float2& rectMax);

    /**
     * Time.
     */
    float(CARB_ABI* getTime)();

    /**
     * Frame Count.
     */
    int(CARB_ABI* getFrameCount)();

    /**
     * This draw list will be the last rendered one, useful to quickly draw overlays shapes/text
     */
    DrawList*(CARB_ABI* getOverlayDrawList)();

    //! @private Unknown/Undocumented
    const char*(CARB_ABI* getStyleColorName)(StyleColor color);

    //! @private Unknown/Undocumented
    Float2(CARB_ABI* calcTextSize)(const char* text,
                                   const char* textEnd /* = nullptr*/,
                                   bool hideTextAfterDoubleHash /* = false*/,
                                   float wrap_width /* = -1.0f*/);

    /**
     * Calculate coarse clipping for large list of evenly sized items. Prefer using the ImGuiListClipper higher-level
     * helper if you can.
     */
    void(CARB_ABI* calcListClipping)(int itemCount, float itemsHeight, int* outItemsDisplayStart, int* outItemsDisplayEnd);

    /**
     * Helper to create a child window / scrolling region that looks like a normal widget frame
     */
    bool(CARB_ABI* beginChildFrame)(uint32_t id, const Float2& size, WindowFlags flags /* = 0*/);

    /**
     * Always call EndChildFrame() regardless of BeginChildFrame() return values (which indicates a collapsed/clipped
     * window)
     */
    void(CARB_ABI* endChildFrame)();

    //! @private Unknown/Undocumented
    Float4(CARB_ABI* colorConvertU32ToFloat4)(uint32_t in);

    //! @private Unknown/Undocumented
    uint32_t(CARB_ABI* colorConvertFloat4ToU32)(const Float4& in);

    //! @private Unknown/Undocumented
    void(CARB_ABI* colorConvertRGBtoHSV)(float r, float g, float b, float& outH, float& outS, float& outV);

    //! @private Unknown/Undocumented
    void(CARB_ABI* colorConvertHSVtoRGB)(float h, float s, float v, float& outR, float& outG, float& outB);

    /**
     * Map ImGuiKey_* values into user's key index. == io.KeyMap[key]
     */
    int(CARB_ABI* getKeyIndex)(int imguiKey);

    /**
     * Is key being held. == io.KeysDown[userKeyIndex]. note that simplegui doesn't know the semantic of each entry of
     * io.KeyDown[]. Use your own indices/enums according to how your backend/engine stored them into KeyDown[]!
     */
    bool(CARB_ABI* isKeyDown)(int userKeyIndex);

    /**
     * Was key pressed (went from !Down to Down). if repeat=true, uses io.KeyRepeatDelay / KeyRepeatRate
     */
    bool(CARB_ABI* isKeyPressed)(int userKeyIndex, bool repeat /* = true*/);

    /**
     * Was key released (went from Down to !Down).
     */
    bool(CARB_ABI* isKeyReleased)(int userKeyIndex);

    /**
     * Uses provided repeat rate/delay. return a count, most often 0 or 1 but might be >1 if RepeatRate is small enough
     * that DeltaTime > RepeatRate
     */
    int(CARB_ABI* getKeyPressedAmount)(int keyIndex, float repeatDelay, float rate);

    /**
     * Gets the key modifiers for each frame.
     *
     * Shortcut to bitwise modifier from ImGui::GetIO().KeyCtrl + .KeyShift + .KeyAlt + .KeySuper
     *
     * @return The key modifiers for each frame.
     */
    KeyModifiers(CARB_ABI* getKeyModifiers)();

    /**
     * Is mouse button held
     */
    bool(CARB_ABI* isMouseDown)(int button);

    /**
     * Is any mouse button held
     */
    bool(CARB_ABI* isAnyMouseDown)();

    /**
     * Did mouse button clicked (went from !Down to Down)
     */
    bool(CARB_ABI* isMouseClicked)(int button, bool repeat /* = false*/);

    /**
     * Did mouse button double-clicked. a double-click returns false in IsMouseClicked(). uses io.MouseDoubleClickTime.
     */
    bool(CARB_ABI* isMouseDoubleClicked)(int button);

    /**
     * Did mouse button released (went from Down to !Down)
     */
    bool(CARB_ABI* isMouseReleased)(int button);

    /**
     * Is mouse dragging. if lockThreshold < -1.0f uses io.MouseDraggingThreshold
     */
    bool(CARB_ABI* isMouseDragging)(int button /* = 0*/, float lockThreshold /* = -1.0f*/);

    /**
     * Is mouse hovering given bounding rect (in screen space). clipped by current clipping settings. disregarding of
     * consideration of focus/window ordering/blocked by a popup.
     */
    bool(CARB_ABI* isMouseHoveringRect)(const Float2& rMin, const Float2& rMax, bool clip /* = true*/);

    //! @private Unknown/Undocumented
    bool(CARB_ABI* isMousePosValid)(const Float2* mousePos /* = nullptr*/);

    /**
     * Shortcut to ImGui::GetIO().MousePos provided by user, to be consistent with other calls
     */
    Float2(CARB_ABI* getMousePos)();

    /**
     * Retrieve backup of mouse position at the time of opening popup we have BeginPopup() into
     */
    Float2(CARB_ABI* getMousePosOnOpeningCurrentPopup)();

    /**
     * Dragging amount since clicking. if lockThreshold < -1.0f uses io.MouseDraggingThreshold
     */
    Float2(CARB_ABI* getMouseDragDelta)(int button /* = 0*/, float lockThreshold /* = -1.0f*/);

    //! @private Unknown/Undocumented
    void(CARB_ABI* resetMouseDragDelta)(int button /* = 0*/);

    /**
     * Gets the mouse wheel delta for each frame.
     *
     * Shortcut to ImGui::GetIO().MouseWheel + .MouseWheelH.
     *
     * @return The mouse wheel delta for each frame.
     */
    carb::Float2(CARB_ABI* getMouseWheel)();

    /**
     * Get desired cursor type, reset in ISimpleGui::newFrame(), this is updated during the frame. valid before
     * Render(). If you use software rendering by setting io.MouseDrawCursor simplegui will render those for you
     */
    MouseCursor(CARB_ABI* getMouseCursor)();

    /**
     * Set desired cursor type
     */
    void(CARB_ABI* setMouseCursor)(MouseCursor type);

    /**
     * Manually override io.WantCaptureKeyboard flag next frame (said flag is entirely left for your application to
     * handle). e.g. force capture keyboard when your widget is being hovered.
     */
    void(CARB_ABI* captureKeyboardFromApp)(bool capture /* = true*/);

    /**
     * Manually override io.WantCaptureMouse flag next frame (said flag is entirely left for your application to
     * handle).
     */
    void(CARB_ABI* captureMouseFromApp)(bool capture /* = true*/);

    /**
     * Used to capture text data to the clipboard.
     *
     * @return The text captures from the clipboard
     */
    const char*(CARB_ABI* getClipboardText)();

    /**
     * Used to apply text into the clipboard.
     *
     * @param text The text to be set into the clipboard.
     */
    void(CARB_ABI* setClipboardText)(const char* text);

    /**
     * Shortcut to ImGui::GetIO().WantSaveIniSettings provided by user, to be consistent with other calls
     */
    bool(CARB_ABI* getWantSaveIniSettings)();

    /**
     * Shortcut to ImGui::GetIO().WantSaveIniSettings provided by user, to be consistent with other calls
     */
    void(CARB_ABI* setWantSaveIniSettings)(bool wantSaveIniSettings);

    /**
     * Manually load the previously saved setting from memory loaded from an .ini settings file.
     *
     * @param iniData The init data to be loaded.
     * @param initSize The size of the ini data to be loaded.
     */
    void(CARB_ABI* loadIniSettingsFromMemory)(const char* iniData, size_t iniSize);

    /**
     * Manually save settings to a ini memory as a string.
     *
     * @param iniSize[out] The ini size of memory to be saved.
     * @return The memory
     */
    const char*(CARB_ABI* saveIniSettingsToMemory)(size_t* iniSize);

    /**
     * Main viewport. Same as GetPlatformIO().MainViewport == GetPlatformIO().Viewports[0]
     */
    Viewport*(CARB_ABI* getMainViewport)();

    /**
     * Associates a windowName to a dock node id.
     *
     * @param windowName The name of the window.
     * @param nodeId The dock node id.
     */
    void(CARB_ABI* dockBuilderDockWindow)(const char* windowName, uint32_t nodeId);

    /**
     * DO NOT HOLD ON ImGuiDockNode* pointer, will be invalided by any split/merge/remove operation.
     */
    DockNode*(CARB_ABI* dockBuilderGetNode)(uint32_t nodeId);

    /**
     * Defaults:
     *  flags = 0
     */
    void(CARB_ABI* dockBuilderAddNode)(uint32_t nodeId, DockNodeFlags flags);

    /**
     * Remove node and all its child, undock all windows
     */
    void(CARB_ABI* dockBuilderRemoveNode)(uint32_t nodeId);

    /**
     * Defaults:
     *  clearPersistentDockingReferences = true
     */
    void(CARB_ABI* dockBuilderRemoveNodeDockedWindows)(uint32_t nodeId, bool clearPersistentDockingReferences);

    /**
     * Remove all split/hierarchy. All remaining docked windows will be re-docked to the root.
     */
    void(CARB_ABI* dockBuilderRemoveNodeChildNodes)(uint32_t nodeId);

    /**
     * Dock building split node.
     */
    uint32_t(CARB_ABI* dockBuilderSplitNode)(
        uint32_t nodeId, Direction splitDir, float sizeRatioForNodeAtDir, uint32_t* outIdDir, uint32_t* outIdOther);

    /**
     * Dock building finished.
     */
    void(CARB_ABI* dockBuilderFinish)(uint32_t nodeId);

    /**
     * Adds a font from a given font config.
     * @param fontConfig The font config struct.
     * @returns A valid font, or `nullptr` if an error occurred.
     */
    Font*(CARB_ABI* addFont)(const FontConfig* fontConfig);

    /**
     * Adds a default font from a given font config.
     * @param fontConfig The font config struct.
     * @returns A valid font, or `nullptr` if an error occurred.
     */
    Font*(CARB_ABI* addFontDefault)(const FontConfig* fontConfig /* = NULL */);

    /**
     * Adds a TTF font from a file.
     * @param filename The path to the file.
     * @param sizePixels The size of the font in pixels.
     * @param fontCfg (Optional) The font config struct.
     * @param glyphRanges (Optional) The range of glyphs.
     * @returns A valid font, or `nullptr` if an error occurred.
     */
    Font*(CARB_ABI* addFontFromFileTTF)(const char* filename,
                                        float sizePixels,
                                        const FontConfig* fontCfg /*= NULL */,
                                        const Wchar* glyphRanges /*= NULL*/);

    /**
     * Adds a TTF font from a memory region.
     * @param fontData The font data in memory.
     * @param fontSize The number of bytes of the font data in memory.
     * @param sizePixels The size of the font in pixels.
     * @param fontCfg (Optional) The font config struct.
     * @param glyphRanges (Optional) The range of glyphs.
     * @returns A valid font, or `nullptr` if an error occurred.
     */
    Font*(CARB_ABI* addFontFromMemoryTTF)(void* fontData,
                                          int fontSize,
                                          float sizePixels,
                                          const FontConfig* fontCfg /* = NULL */,
                                          const Wchar* glyphRanges /* = NULL */);

    /**
     * Adds a compressed TTF font from a memory region.
     * @param compressedFontData The font data in memory.
     * @param compressedFontSize The number of bytes of the font data in memory.
     * @param sizePixels The size of the font in pixels.
     * @param fontCfg (Optional) The font config struct.
     * @param glyphRanges (Optional) The range of glyphs.
     * @returns A valid font, or `nullptr` if an error occurred.
     */
    Font*(CARB_ABI* addFontFromMemoryCompressedTTF)(const void* compressedFontData,
                                                    int compressedFontSize,
                                                    float sizePixels,
                                                    const FontConfig* fontCfg /* = NULL */,
                                                    const Wchar* glyphRanges /*= NULL */);

    /**
     * Adds a compressed base-85 TTF font from a memory region.
     * @param compressedFontDataBase85 The font data in memory.
     * @param sizePixels The size of the font in pixels.
     * @param fontCfg (Optional) The font config struct.
     * @param glyphRanges (Optional) The range of glyphs.
     * @returns A valid font, or `nullptr` if an error occurred.
     */
    Font*(CARB_ABI* addFontFromMemoryCompressedBase85TTF)(const char* compressedFontDataBase85,
                                                          float sizePixels,
                                                          const FontConfig* fontCfg /* = NULL */,
                                                          const Wchar* glyphRanges /* = NULL */);

    /**
     * Add a custom rect glyph that can be built into the font atlas. Call buildFont after.
     *
     * @param font The font to add to.
     * @param id The Unicode point to add for.
     * @param width The width of the glyph.
     * @param height The height of the glyph
     * @param advanceX The advance x for the glyph.
     * @param offset The glyph offset.
     * @return The glyph index.
     */
    int(CARB_ABI* addFontCustomRectGlyph)(
        Font* font, Wchar id, int width, int height, float advanceX, const carb::Float2& offset /* (0, 0) */);

    /**
     * Gets the font custom rect by glyph index.
     *
     * @param index The glyph index to get the custom rect information for.
     * @return The font glyph custom rect information.
     */
    const FontCustomRect*(CARB_ABI* getFontCustomRectByIndex)(int index);

    /**
     * Builds the font atlas.
     *
     * @return true if the font atlas was built successfully.
     */
    bool(CARB_ABI* buildFont)();

    /**
     * Determines if changes have been made to font atlas
     *
     * @return true if the font atlas is built.
     */
    bool(CARB_ABI* isFontBuilt)();

    /**
     * Gets the font texture data.
     *
     * @param outPixel The pixel texture data in A8 format.
     * @param outWidth The texture width.
     * @param outPixel The texture height.
     */
    void(CARB_ABI* getFontTexDataAsAlpha8)(unsigned char** outPixels, int* outWidth, int* outHeight);

    /**
     * Clear input data (all ImFontConfig structures including sizes, TTF data, glyph ranges, etc.) = all the data used
     * to build the texture and fonts.
     */
    void(CARB_ABI* clearFontInputData)();

    /**
     * Clear output texture data (CPU side). Saves RAM once the texture has been copied to graphics memory.
     */
    void(CARB_ABI* clearFontTexData)();

    /**
     * Clear output font data (glyphs storage, UV coordinates).
     */
    void(CARB_ABI* clearFonts)();

    /**
     * Clear all input and output.
     */
    void(CARB_ABI* clearFontInputOutput)();

    /**
     * Basic Latin, Extended Latin
     */
    const Wchar*(CARB_ABI* getFontGlyphRangesDefault)();

    /**
     * Default + Korean characters
     */
    const Wchar*(CARB_ABI* getFontGlyphRangesKorean)();

    /**
     * Default + Hiragana, Katakana, Half-Width, Selection of 1946 Ideographs
     */
    const Wchar*(CARB_ABI* getFontGlyphRangesJapanese)();

    /**
     * Default + Half-Width + Japanese Hiragana/Katakana + full set of about 21000 CJK Unified Ideographs
     */
    const Wchar*(CARB_ABI* getFontGlyphRangesChineseFull)();

    /**
     * Default + Half-Width + Japanese Hiragana/Katakana + set
     * of 2500 CJK Unified Ideographs for common simplified Chinese
     */
    const Wchar*(CARB_ABI* getGlyphRangesChineseSimplifiedCommon)();

    /**
     * Default + about 400 Cyrillic characters
     */
    const Wchar*(CARB_ABI* getFontGlyphRangesCyrillic)();

    /**
     * Default + Thai characters
     */
    const Wchar*(CARB_ABI* getFontGlyphRangesThai)();

    /**
     * set Global Font Scale
     */
    void(CARB_ABI* setFontGlobalScale)(float scale);

    /**
     * Shortcut for getWindowDrawList() + DrawList::AddCallback()
     */
    void(CARB_ABI* addWindowDrawCallback)(DrawCallback callback, void* userData);

    /**
     * Adds a line to the draw list.
     */
    void(CARB_ABI* addLine)(DrawList* drawList, const carb::Float2& a, const carb::Float2& b, uint32_t col, float thickness);

    /**
     * Adds a rect to the draw list.
     *
     * @param a Upper-left.
     * @param b Lower-right.
     * @param col color.
     * @param rounding Default = 0.f;
     * @param roundingCornersFlags 4-bits corresponding to which corner to round. Default = kDrawCornerFlagAll
     * @param thickness Default = 1.0f
     */
    void(CARB_ABI* addRect)(DrawList* drawList,
                            const carb::Float2& a,
                            const carb::Float2& b,
                            uint32_t col,
                            float rounding,
                            DrawCornerFlags roundingCornersFlags,
                            float thickness);

    /**
     * Adds a filled rect to the draw list.
     *
     * @param a Upper-left.
     * @param b Lower-right.
     * @param col color.
     * @param rounding Default = 0.f;
     * @param roundingCornersFlags 4-bits corresponding to which corner to round. Default = kDrawCornerFlagAll
     */
    void(CARB_ABI* addRectFilled)(DrawList* drawList,
                                  const carb::Float2& a,
                                  const carb::Float2& b,
                                  uint32_t col,
                                  float rounding,
                                  DrawCornerFlags roundingCornersFlags);

    /**
     * Adds a filled multi-color rect to the draw list.
     */
    void(CARB_ABI* addRectFilledMultiColor)(DrawList* drawList,
                                            const carb::Float2& a,
                                            const carb::Float2& b,
                                            uint32_t colUprLeft,
                                            uint32_t colUprRight,
                                            uint32_t colBotRight,
                                            uint32_t colBotLeft);
    /**
     * Adds a quad to the draw list.
     * Default: thickness = 1.0f.
     */
    void(CARB_ABI* addQuad)(DrawList* drawList,
                            const carb::Float2& a,
                            const carb::Float2& b,
                            const carb::Float2& c,
                            const carb::Float2& d,
                            uint32_t col,
                            float thickness);

    /**
     * Adds a filled quad to the draw list.
     */
    void(CARB_ABI* addQuadFilled)(DrawList* drawList,
                                  const carb::Float2& a,
                                  const carb::Float2& b,
                                  const carb::Float2& c,
                                  const carb::Float2& d,
                                  uint32_t col);

    /**
     * Adds a triangle to the draw list.
     * Defaults: thickness = 1.0f.
     */
    void(CARB_ABI* addTriangle)(DrawList* drawList,
                                const carb::Float2& a,
                                const carb::Float2& b,
                                const carb::Float2& c,
                                uint32_t col,
                                float thickness);

    /**
     * Adds a filled triangle to the draw list.
     */
    void(CARB_ABI* addTriangleFilled)(
        DrawList* drawList, const carb::Float2& a, const carb::Float2& b, const carb::Float2& c, uint32_t col);

    /**
     * Adds a circle to the draw list.
     * Defaults: numSegments = 12, thickness = 1.0f.
     */
    void(CARB_ABI* addCircle)(
        DrawList* drawList, const carb::Float2& centre, float radius, uint32_t col, int32_t numSegments, float thickness);

    /**
     * Adds a filled circle to the draw list.
     * Defaults: numSegments = 12, thickness = 1.0f.
     */
    void(CARB_ABI* addCircleFilled)(
        DrawList* drawList, const carb::Float2& centre, float radius, uint32_t col, int32_t numSegments);

    /**
     * Adds text to the draw list.
     */
    void(CARB_ABI* addText)(
        DrawList* drawList, const carb::Float2& pos, uint32_t col, const char* textBegin, const char* textEnd);

    /**
     * Adds text to the draw list.
     * Defaults: textEnd = nullptr, wrapWidth = 0.f, cpuFineClipRect = nullptr.
     */
    void(CARB_ABI* addTextEx)(DrawList* drawList,
                              const Font* font,
                              float fontSize,
                              const carb::Float2& pos,
                              uint32_t col,
                              const char* textBegin,
                              const char* textEnd,
                              float wrapWidth,
                              const carb::Float4* cpuFineClipRect);

    /**
     * Adds an image to the draw list.
     */
    void(CARB_ABI* addImage)(DrawList* drawList,
                             TextureId textureId,
                             const carb::Float2& a,
                             const carb::Float2& b,
                             const carb::Float2& uvA,
                             const carb::Float2& uvB,
                             uint32_t col);

    /**
     * Adds an image quad to the draw list.
     * defaults: uvA = (0, 0), uvB = (1, 0), uvC = (1, 1), uvD = (0, 1), col = 0xFFFFFFFF.
     */
    void(CARB_ABI* addImageQuad)(DrawList* drawList,
                                 TextureId textureId,
                                 const carb::Float2& a,
                                 const carb::Float2& b,
                                 const carb::Float2& c,
                                 const carb::Float2& d,
                                 const carb::Float2& uvA,
                                 const carb::Float2& uvB,
                                 const carb::Float2& uvC,
                                 const carb::Float2& uvD,
                                 uint32_t col);

    /**
     * Adds an rounded image to the draw list.
     * defaults: roundingCorners = kDrawCornerFlagAll
     */
    void(CARB_ABI* addImageRounded)(DrawList* drawList,
                                    TextureId textureId,
                                    const carb::Float2& a,
                                    const carb::Float2& b,
                                    const carb::Float2& uvA,
                                    const carb::Float2& uvB,
                                    uint32_t col,
                                    float rounding,
                                    DrawCornerFlags roundingCorners);

    /**
     * Adds a polygon line to the draw list.
     */
    void(CARB_ABI* addPolyline)(DrawList* drawList,
                                const carb::Float2* points,
                                const int32_t numPoints,
                                uint32_t col,
                                bool closed,
                                float thickness);

    /**
     * Adds a filled convex polygon to draw list.
     * Note: Anti-aliased filling requires points to be in clockwise order.
     */
    void(CARB_ABI* addConvexPolyFilled)(DrawList* drawList,
                                        const carb::Float2* points,
                                        const int32_t numPoints,
                                        uint32_t col);

    /**
     * Adds a Bezier curve to draw list.
     * defaults: numSegments = 0.
     */
    void(CARB_ABI* addBezierCurve)(DrawList* drawList,
                                   const carb::Float2& pos0,
                                   const carb::Float2& cp0,
                                   const carb::Float2& cp1,
                                   const carb::Float2& pos1,
                                   uint32_t col,
                                   float thickness,
                                   int32_t numSegments);

    /**
     * Creates a ListClipper to clip large list of items.
     *
     * @param itemsCount Number of items to clip. Use INT_MAX if you don't know how many items you have (in which case
     * the cursor won't be advanced in the final step)
     * @param float itemsHeight Use -1.0f to be calculated automatically on first step. Otherwise pass in the distance
     * between your items, typically getTextLineHeightWithSpacing() or getFrameHeightWithSpacing().
     *
     * @return returns the created ListClipper instance.
     */
    ListClipper*(CARB_ABI* createListClipper)(int32_t itemsCount, float itemsHeight);

    /**
     * Call until it returns false. The displayStart/displayEnd fields will be set and you can process/draw those items.
     *
     * @param listClipper The listClipper instance to advance.
     */
    bool(CARB_ABI* stepListClipper)(ListClipper* listClipper);

    /**
     * Destroys a listClipper instance.
     *
     * @param listClipper The listClipper instance to destroy.
     */
    void(CARB_ABI* destroyListClipper)(ListClipper* listClipper);

    /**
     * Feed the keyboard event into the simplegui.
     *
     * @param ctx The context to be fed input event to.
     * @param event Keyboard event description.
     */
    bool(CARB_ABI* feedKeyboardEvent)(Context* ctx, const input::KeyboardEvent& event);

    /**
     * Feed the mouse event into the simplegui.
     *
     * @param ctx The context to be fed input event to.
     * @param event Mouse event description.
     */
    bool(CARB_ABI* feedMouseEvent)(Context* ctx, const input::MouseEvent& event);
};

inline void ISimpleGui::sameLine()
{
    sameLineEx(0, -1.0f);
}

inline bool ISimpleGui::button(const char* label)
{
    return buttonEx(label, { 0, 0 });
}

} // namespace simplegui
} // namespace carb
