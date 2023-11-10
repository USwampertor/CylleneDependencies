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
//! @brief carb.simplegui type definitions.
#pragma once

#include "../Types.h"

namespace carb
{

//! Namespace for Carbonite Input system.
namespace input
{
struct Keyboard;
struct Mouse;
struct Gamepad;
struct KeyboardEvent;
struct MouseEvent;
} // namespace input

//! Namespace for Carbonite Windowing system.
namespace windowing
{
struct Window;
}

namespace simplegui
{

//! An opaque type representing a SimpleGui "context," or instance of a GUI.
struct Context;

//! An opaque type representing a SimpleGui font.
struct Font;

//! An opaque type returned by ISimpleGui::dockBuilderGetNode().
struct DockNode;

/**
 * Defines a descriptor for simplegui context.
 */
struct ContextDesc
{
    Float2 displaySize; //!< The display size.
    windowing::Window* window; //!< The Window to use.
    input::Keyboard* keyboard; //!< The Keyboard to listen for events. Can be nullptr.
    input::Mouse* mouse; //!< The Mouse to listen for events. Can be nullptr.
    input::Gamepad* gamepad; //!< The Gamepad to listen for events. Can be nullptr.
};

/**
 * Key modifiers returned by ISimpleGui::getKeyModifiers().
 */
typedef uint32_t KeyModifiers;

const KeyModifiers kKeyModifierNone = 0; //!< Indicates no key modifiers.
const KeyModifiers kKeyModifierCtrl = 1 << 0; //!< Indicates CTRL is held.
const KeyModifiers kKeyModifierShift = 1 << 1; //!< Indicates SHIFT is held.
const KeyModifiers kKeyModifierAlt = 1 << 2; //!< Indicates ALT is held.
const KeyModifiers kKeyModifierSuper = 1 << 3; //!< Indicates a "super key" is held (Cmd/Windows/etc.).

/**
 * Defines window flags for simplegui::begin()
 */
typedef uint32_t WindowFlags;

const WindowFlags kWindowFlagNone = 0; //!< Indicates the absence of all other window flags.
const WindowFlags kWindowFlagNoTitleBar = 1 << 0; //!< Window Flag to disable the title bar.
const WindowFlags kWindowFlagNoResize = 1 << 1; //!< Window Flag to disable user resizing with the lower-right grip.
const WindowFlags kWindowFlagNoMove = 1 << 2; //!< Window Flag to disable user moving the window.
const WindowFlags kWindowFlagNoScrollbar = 1 << 3; //!< Window Flag to disable user moving the window.
const WindowFlags kWindowFlagNoScrollWithMouse = 1 << 4; //!< Window Flag to disable user vertically scrolling with
                                                         //!< mouse wheel. On child window, mouse wheel will be
                                                         //!< forwarded to the parent unless NoScrollbar is also set..
const WindowFlags kWindowFlagNoCollapse = 1 << 5; //!< Window Flag to disable user collapsing window by double-clicking
                                                  //!< on it.
const WindowFlags kWindowFlagAlwaysAutoResize = 1 << 6; //!< Window Flag to resize every window to its content every
                                                        //!< frame.
const WindowFlags kWindowFlagNoBackground = 1 << 7; //!< Window Flag to disable drawing background color (WindowBg,
                                                    //!< etc.) and outside border. Similar as using
                                                    //!< SetNextWindowBgAlpha(0.0f).
const WindowFlags kWindowFlagNoSavedSettings = 1 << 8; //!< Window Flag to never load/save settings in .ini file
const WindowFlags kWindowFlagNoMouseInputs = 1 << 9; //!< Window Flag to disable catching mouse, hovering test with pass
                                                     //!< through.
const WindowFlags kWindowFlagMenuBar = 1 << 10; //!< Window Flag to state that this has a menu-bar.
const WindowFlags kWindowFlagHorizontalScrollbar = 1 << 11; //!< Window Flag to allow horizontal scrollbar to appear
                                                            //!< (off by default). You may use
                                                            //!< `SetNextWindowContentSize(Float2(width,0.0f))`, prior
                                                            //!< to calling Begin() to specify width.
const WindowFlags kWindowFlagNoFocusOnAppearing = 1 << 12; //!< Window Flag to disable taking focus when transitioning
                                                           //!< from hidden to visible state.
const WindowFlags kWindowFlagNoBringToFrontOnFocus = 1 << 13; //!< Window Flag to disable bringing window to front when
                                                              //!< taking focus. (Ex. clicking on it or programmatically
                                                              //!< giving it focus).
const WindowFlags kWindowFlagAlwaysVerticalScrollbar = 1 << 14; //!< Window Flag to always show vertical scrollbar (even
                                                                //!< if content Size.y < Size.y).
const WindowFlags kWindowFlagAlwaysHorizontalScrollbar = 1 << 15; //!< Window Flag to always show horizontal scrollbar
                                                                  //!< (even if content Size.x < Size.x).
const WindowFlags kWindowFlagAlwaysUseWindowPadding = 1 << 16; //!< Window Flag to ensure child windows without border
                                                               //!< uses style.WindowPadding. Ignored by default for
                                                               //!< non-bordered child windows, because more convenient.
const WindowFlags kWindowFlagNoNavInputs = 1 << 18; //!< No gamepad/keyboard navigation within the window
const WindowFlags kWindowFlagNoNavFocus = 1 << 19; //!< No focusing toward this window with gamepad/keyboard navigation
                                                   //!< (e.g. skipped by CTRL+TAB)
const WindowFlags kWindowFlagUnsavedDocument = 1 << 20; //!< Append '*' to title without affecting the ID, as a
                                                        //!< convenience to avoid using the ### operator. When used in a
                                                        //!< tab/docking context, tab is selected on closure and closure
                                                        //!< is deferred by one frame to allow code to cancel the
                                                        //!< closure (with a confirmation popup, etc.) without flicker.
const WindowFlags kWindowFlagNoDocking = 1 << 21; //!< Disable docking of this window

//! Special composed Window Flag to disable navigation.
const WindowFlags kWindowFlagNoNav = kWindowFlagNoNavInputs | kWindowFlagNoNavFocus;

//! Special composed Window Flag to disable all decorative elements.
const WindowFlags kWindowFlagNoDecoration =
    kWindowFlagNoTitleBar | kWindowFlagNoResize | kWindowFlagNoScrollbar | kWindowFlagNoCollapse;

//! Special composed Window Flag to disable input.
const WindowFlags kWindowFlagNoInput = kWindowFlagNoMouseInputs | kWindowFlagNoNavInputs | kWindowFlagNoNavFocus;

/**
 * Defines item flags for simplegui::pushItemFlags().
 *
 * Transient per-window flags, reset at the beginning of the frame. For child window, inherited from parent on first
 * Begin().
 */
typedef uint32_t ItemFlags;

const ItemFlags kItemFlagDefault = 0; //!< Absence of other item flags.
const ItemFlags kItemFlagsNoTabStop = 1 << 0; //!< No tab stop.
const ItemFlags kItemFlagButtonRepeat = 1 << 1; //!< Button repeat.
const ItemFlags kItemFlagDisabled = 1 << 2; //!< Disable interactions.
const ItemFlags kItemFlagNoNav = 1 << 3; //!< No Navigation
const ItemFlags kItemFlagNoNavDefaultFocus = 1 << 4; //!< No Navigation Default Focus.
const ItemFlags kItemFlagSelectableDontClosePopup = 1 << 5; //!< MenuItem/Selectable() automatically closes current
                                                            //!< Popup window.

/**
 * Defines input text flags for simplegui::inputText()
 */
typedef uint32_t InputTextFlags;

const InputTextFlags kInputTextFlagNone = 0; //!< Absence of other input text flags.
const InputTextFlags kInputTextFlagCharsDecimal = 1 << 0; //!< Allow `0123456789.+-*/`
const InputTextFlags kInputTextFlagCharsHexadecimal = 1 << 1; //!< Allow `0123456789ABCDEFabcdef`
const InputTextFlags kInputTextFlagCharsUppercase = 1 << 2; //!< Turn `a..z` into `A..Z`
const InputTextFlags kInputTextFlagCharsNoBlank = 1 << 3; //!< Filter out spaces, tabs
const InputTextFlags kInputTextFlagAutoSelectAll = 1 << 4; //!< Select entire text when first taking mouse focus
const InputTextFlags kInputTextFlagEnterReturnsTrue = 1 << 5; //!< Return 'true' when Enter is pressed (as opposed to
                                                              //!< when the value was modified)
const InputTextFlags kInputTextFlagCallbackCompletion = 1 << 6; //!< Call user function on pressing TAB (for completion
                                                                //!< handling)
const InputTextFlags kInputTextFlagCallbackHistory = 1 << 7; //!< Call user function on pressing Up/Down arrows (for
                                                             //!< history handling)
const InputTextFlags kInputTextFlagCallbackAlways = 1 << 8; //!< Call user function every time. User code may query
                                                            //!< cursor position, modify text buffer.
const InputTextFlags kInputTextFlagCallbackCharFilter = 1 << 9; //!< Call user function to filter character. Modify
                                                                //!< data->EventChar to replace/filter input, or return
                                                                //!< 1 to discard character.
const InputTextFlags kInputTextFlagAllowTabInput = 1 << 10; //!< Pressing TAB input a `\t` character into the text field
const InputTextFlags kInputTextFlagCtrlEnterForNewLine = 1 << 11; //!< In multi-line mode, unfocus with Enter, add new
                                                                  //!< line with Ctrl+Enter (default is opposite:
                                                                  //!< unfocus with Ctrl+Enter, add line with Enter).
const InputTextFlags kInputTextFlagNoHorizontalScroll = 1 << 12; //!< Disable following the cursor horizontally
const InputTextFlags kInputTextFlagAlwaysInsertMode = 1 << 13; //!< Insert mode
const InputTextFlags kInputTextFlagReadOnly = 1 << 14; //!< Read-only mode
const InputTextFlags kInputTextFlagPassword = 1 << 15; //!< Password mode, display all characters as '*'
const InputTextFlags kInputTextFlagNoUndoRedo = 1 << 16; //!< Disable undo/redo. Note that input text owns the text data
                                                         //!< while active, if you want to provide your own undo/redo
                                                         //!< stack you need e.g. to call ClearActiveID().
const InputTextFlags kInputTextFlagCharsScientific = 1 << 17; //!< Allow `0123456789.+-*/eE` (Scientific notation input)
const InputTextFlags kInputTextFlagCallbackResize = 1 << 18; //!< Callback on buffer capacity changes request (beyond
                                                             //!< `buf_size` parameter value)


/**
 * Defines tree node flags to be used in simplegui::collapsingHeader(), simplegui::treeNodeEx()
 */
typedef uint32_t TreeNodeFlags;

const TreeNodeFlags kTreeNodeFlagNone = 0; //!< Absence of other tree node flags.
const TreeNodeFlags kTreeNodeFlagSelected = 1 << 0; //!< Draw as selected
const TreeNodeFlags kTreeNodeFlagFramed = 1 << 1; //!< Full colored frame (e.g. for CollapsingHeader)
const TreeNodeFlags kTreeNodeFlagAllowItemOverlap = 1 << 2; //!< Hit testing to allow subsequent widgets to overlap this
                                                            //!< one
const TreeNodeFlags kTreeNodeFlagNoTreePushOnOpen = 1 << 3; //!< Don't do a TreePush() when open (e.g. for
                                                            //!< CollapsingHeader) = no extra indent nor pushing on ID
                                                            //!< stack
const TreeNodeFlags kTreeNodeFlagNoAutoOpenOnLog = 1 << 4; //!< Don't automatically and temporarily open node when
                                                           //!< Logging is active (by default logging will automatically
                                                           //!< open tree nodes)
const TreeNodeFlags kTreeNodeFlagDefaultOpen = 1 << 5; //!< Default node to be open
const TreeNodeFlags kTreeNodeFlagOpenOnDoubleClick = 1 << 6; //!< Need double-click to open node
const TreeNodeFlags kTreeNodeFlagOpenOnArrow = 1 << 7; //!< Only open when clicking on the arrow part. If
                                                       //!< kTreeNodeFlagOpenOnDoubleClick is also set, single-click
                                                       //!< arrow or double-click all box to open.
const TreeNodeFlags kTreeNodeFlagLeaf = 1 << 8; //!< No collapsing, no arrow (use as a convenience for leaf nodes).
const TreeNodeFlags kTreeNodeFlagBullet = 1 << 9; //!< Display a bullet instead of arrow
const TreeNodeFlags kTreeNodeFlagFramePadding = 1 << 10; //!< Use FramePadding (even for an unframed text node) to
                                                         //!< vertically align text baseline to regular widget
const TreeNodeFlags kTreeNodeFlagNavLeftJumpsBackHere = 1 << 13; //!< (WIP) Nav: left direction may move to this
                                                                 //!< TreeNode() from any of its child (items submitted
                                                                 //!< between TreeNode and TreePop)

//! Composed flag indicating collapsing header.
const TreeNodeFlags kTreeNodeFlagCollapsingHeader =
    kTreeNodeFlagFramed | kTreeNodeFlagNoTreePushOnOpen | kTreeNodeFlagNoAutoOpenOnLog;


/**
 * Defines flags to be used in simplegui::selectable()
 */
typedef uint32_t SelectableFlags;

const SelectableFlags kSelectableFlagNone = 0; //!< Absence of other selectable flags.
const SelectableFlags kSelectableFlagDontClosePopups = 1 << 0; //!<  Clicking this don't close parent popup window
const SelectableFlags kSelectableFlagSpanAllColumns = 1 << 1; //!<  Selectable frame can span all columns (text will
                                                              //!< still fit in current column)
const SelectableFlags kSelectableFlagAllowDoubleClick = 1 << 2; //!<  Generate press events on double clicks too
const SelectableFlags kSelectableFlagDisabled = 1 << 3; //!<  Cannot be selected, display grayed out text

/**
 * Defines flags to be used in simplegui::beginCombo()
 */
typedef uint32_t ComboFlags;

const ComboFlags kComboFlagNone = 0; //!< Absence of other combo flags.
const ComboFlags kComboFlagPopupAlignLeft = 1 << 0; //!<  Align the popup toward the left by default
const ComboFlags kComboFlagHeightSmall = 1 << 1; //!<  Max ~4 items visible. Tip: If you want your combo popup to be a
                                                 //!< specific size you can use SetNextWindowSizeConstraints() prior to
                                                 //!< calling BeginCombo()
const ComboFlags kComboFlagHeightRegular = 1 << 2; //!<  Max ~8 items visible (default)
const ComboFlags kComboFlagHeightLarge = 1 << 3; //!<  Max ~20 items visible
const ComboFlags kComboFlagHeightLargest = 1 << 4; //!<  As many fitting items as possible
const ComboFlags kComboFlagNoArrowButton = 1 << 5; //!<  Display on the preview box without the square arrow button
const ComboFlags kComboFlagNoPreview = 1 << 6; //!<  Display only a square arrow button
const ComboFlags kComboFlagHeightMask =
    kComboFlagHeightSmall | kComboFlagHeightRegular | kComboFlagHeightLarge | kComboFlagHeightLargest; //!< Composed
                                                                                                       //!< flag.


/**
 * Defines flags to be used in simplegui::beginTabBar()
 */
typedef uint32_t TabBarFlags;

const TabBarFlags kTabBarFlagNone = 0; //!< Absence of other tab bar flags.
const TabBarFlags kTabBarFlagReorderable = 1 << 0; //!< Allow manually dragging tabs to re-order them + New tabs are
                                                   //!< appended at the end of list
const TabBarFlags kTabBarFlagAutoSelectNewTabs = 1 << 1; //!< Automatically select new tabs when they appear
const TabBarFlags kTabBarFlagTabListPopupButton = 1 << 2; //!< Tab list popup button.
const TabBarFlags kTabBarFlagNoCloseWithMiddleMouseButton = 1 << 3; //!< Disable behavior of closing tabs (that are
                                                                    //!< submitted with `p_open != NULL`) with middle
                                                                    //!< mouse button. You can still repro this behavior
                                                                    //!< on user's side with if (IsItemHovered() &&
                                                                    //!< IsMouseClicked(2)) `*p_open = false`.
const TabBarFlags kTabBarFlagNoTabListScrollingButtons = 1 << 4; //!< No scrolling buttons.
const TabBarFlags kTabBarFlagNoTooltip = 1 << 5; //!<  Disable tooltips when hovering a tab
const TabBarFlags kTabBarFlagFittingPolicyResizeDown = 1 << 6; //!<  Resize tabs when they don't fit
const TabBarFlags kTabBarFlagFittingPolicyScroll = 1 << 7; //!<  Add scroll buttons when tabs don't fit
const TabBarFlags kTabBarFlagFittingPolicyMask =
    kTabBarFlagFittingPolicyResizeDown | kTabBarFlagFittingPolicyScroll; //!< Composed flag.
const TabBarFlags kTabBarFlagFittingPolicyDefault = kTabBarFlagFittingPolicyResizeDown; //!< Composed flag.


/**
 * Defines flags to be used in simplegui::beginTabItem()
 */
typedef uint32_t TabItemFlags;

const TabItemFlags kTabItemFlagNone = 0; //!< Absence of other tab item flags.
const TabItemFlags kTabItemFlagUnsavedDocument = 1 << 0; //!< Append '*' to title without affecting the ID; as a
                                                         //!< convenience to avoid using the ### operator. Also: tab is
                                                         //!< selected on closure and closure is deferred by one frame
                                                         //!< to allow code to undo it without flicker.
const TabItemFlags kTabItemFlagSetSelected = 1 << 1; //!< Trigger flag to programmatically make the tab selected when
                                                     //!< calling BeginTabItem()
const TabItemFlags kTabItemFlagNoCloseWithMiddleMouseButton = 1 << 2; //!< Disable behavior of closing tabs (that are
                                                                      //!< submitted with `p_open != NULL`) with middle
                                                                      //!< mouse button. You can still repro this
                                                                      //!< behavior on user's side with if
                                                                      //!< (IsItemHovered() && IsMouseClicked(2))
                                                                      //!< `*p_open = false`.
const TabItemFlags kTabItemFlagNoPushId = 1 << 3; //!< Don't call PushID(tab->ID)/PopID() on BeginTabItem()/EndTabItem()


/**
 * Defines flags to be used in simplegui::dockSpace()
 */
typedef uint32_t DockNodeFlags;

const DockNodeFlags kDockNodeFlagNone = 0; //!< Absence of other dock node flags.
const DockNodeFlags kDockNodeFlagKeepAliveOnly = 1 << 0; //!< Don't display the dockspace node but keep it alive.
                                                         //!< Windows docked into this dockspace node won't be undocked.
// const DockNodeFlags kDockNodeFlagNoCentralNode = 1 << 1;   //!<  Disable Central Node (the node which can stay empty)
const DockNodeFlags kDockNodeFlagNoDockingInCentralNode = 1 << 2; //!<  Disable docking inside the Central Node, which
                                                                  //!< will be always kept empty.
const DockNodeFlags kDockNodeFlagPassthruCentralNode = 1 << 3; //!< Enable passthru dockspace: 1) DockSpace() will
                                                               //!< render a ImGuiCol_WindowBg background covering
                                                               //!< everything excepted the Central Node when empty.
                                                               //!< Meaning the host window should probably use
                                                               //!< SetNextWindowBgAlpha(0.0f) prior to Begin() when
                                                               //!< using this. 2) When Central Node is empty: let
                                                               //!< inputs pass-through + won't display a DockingEmptyBg
                                                               //!< background. See demo for details.
const DockNodeFlags kDockNodeFlagNoSplit = 1 << 4; //!< Disable splitting the node into smaller nodes. Useful e.g. when
                                                   //!< embedding dockspaces into a main root one (the root one may have
                                                   //!< splitting disabled to reduce confusion)
const DockNodeFlags kDockNodeFlagNoResize = 1 << 5; //!< Disable resizing child nodes using the splitter/separators.
                                                    //!< Useful with programmatically setup dockspaces.
const DockNodeFlags kDockNodeFlagAutoHideTabBar = 1 << 6; //!< Tab bar will automatically hide when there is a single
                                                          //!< window in the dock node.


/**
 * Defines flags to be used in simplegui::isWindowFocused()
 */
typedef uint32_t FocusedFlags;

const FocusedFlags kFocusedFlagNone = 0; //!< Absence of other focused flags.
const FocusedFlags kFocusedFlagChildWindows = 1 << 0; //!< IsWindowFocused(): Return true if any children of the window
                                                      //!< is focused
const FocusedFlags kFocusedFlagRootWindow = 1 << 1; //!< IsWindowFocused(): Test from root window (top most parent of
                                                    //!< the current hierarchy)
const FocusedFlags kFocusedFlagAnyWindow = 1 << 2; //!< IsWindowFocused(): Return true if any window is focused
const FocusedFlags kFocusedFlagRootAndChildWindows = kFocusedFlagRootWindow | kFocusedFlagChildWindows; //!< Composed
                                                                                                        //!< flag.


/**
 * Defines flags to be used in simplegui::isItemHovered(), simplegui::isWindowHovered()
 */
typedef uint32_t HoveredFlags;

const HoveredFlags kHoveredFlagNone = 0; //!< Return true if directly over the item/window, not obstructed by another
                                         //!< window, not obstructed by an active popup or modal blocking inputs under
                                         //!< them.
const HoveredFlags kHoveredFlagChildWindows = 1 << 0; //!< IsWindowHovered() only: Return true if any children of the
                                                      //!< window is hovered
const HoveredFlags kHoveredFlagRootWindow = 1 << 1; //!< IsWindowHovered() only: Test from root window (top most parent
                                                    //!< of the current hierarchy)
const HoveredFlags kHoveredFlagAnyWindow = 1 << 2; //!< IsWindowHovered() only: Return true if any window is hovered
const HoveredFlags kHoveredFlagAllowWhenBlockedByPopup = 1 << 3; //!< Return true even if a popup window is normally
                                                                 //!< blocking access to this item/window
const HoveredFlags kHoveredFlagAllowWhenBlockedByActiveItem = 1 << 5; //!< Return true even if an active item is
                                                                      //!< blocking access to this item/window. Useful
                                                                      //!< for Drag and Drop patterns.
const HoveredFlags kHoveredFlagAllowWhenOverlapped = 1 << 6; //!< Return true even if the position is overlapped by
                                                             //!< another window
const HoveredFlags kHoveredFlagAllowWhenDisabled = 1 << 7; //!< Return true even if the item is disabled
const HoveredFlags kHoveredFlagRectOnly = kHoveredFlagAllowWhenBlockedByPopup | kHoveredFlagAllowWhenBlockedByActiveItem |
                                          kHoveredFlagAllowWhenOverlapped; //!< Composed flag.
const HoveredFlags kHoveredFlagRootAndChildWindows = kHoveredFlagRootWindow | kHoveredFlagChildWindows; //!< Composed
                                                                                                        //!< flag.


/**
 * Defines flags to be used in simplegui::beginDragDropSource(), simplegui::acceptDragDropPayload()
 */
typedef uint32_t DragDropFlags;

const DragDropFlags kDragDropFlagNone = 0; //!< Absence of other drag/drop flags.
// BeginDragDropSource() flags
const DragDropFlags kDragDropFlagSourceNoPreviewTooltip = 1 << 0; //!< By default, a successful call to
                                                                  //!< BeginDragDropSource opens a tooltip so you can
                                                                  //!< display a preview or description of the source
                                                                  //!< contents. This flag disable this behavior.
const DragDropFlags kDragDropFlagSourceNoDisableHover = 1 << 1; //!< By default, when dragging we clear data so that
                                                                //!< IsItemHovered() will return true, to avoid
                                                                //!< subsequent user code submitting tooltips. This flag
                                                                //!< disable this behavior so you can still call
                                                                //!< IsItemHovered() on the source item.
const DragDropFlags kDragDropFlagSourceNoHoldToOpenOthers = 1 << 2; //!< Disable the behavior that allows to open tree
                                                                    //!< nodes and collapsing header by holding over
                                                                    //!< them while dragging a source item.
const DragDropFlags kDragDropFlagSourceAllowNullID = 1 << 3; //!< Allow items such as Text(), Image() that have no
                                                             //!< unique identifier to be used as drag source, by
                                                             //!< manufacturing a temporary identifier based on their
                                                             //!< window-relative position. This is extremely unusual
                                                             //!< within the simplegui ecosystem and so we made it
                                                             //!< explicit.
const DragDropFlags kDragDropFlagSourceExtern = 1 << 4; //!< External source (from outside of simplegui), won't attempt
                                                        //!< to read current item/window info. Will always return true.
                                                        //!< Only one Extern source can be active simultaneously.
const DragDropFlags kDragDropFlagSourceAutoExpirePayload = 1 << 5; //!< Automatically expire the payload if the source
                                                                   //!< cease to be submitted (otherwise payloads are
                                                                   //!< persisting while being dragged)
// AcceptDragDropPayload() flags
const DragDropFlags kDragDropFlagAcceptBeforeDelivery = 1 << 10; //!< AcceptDragDropPayload() will returns true even
                                                                 //!< before the mouse button is released. You can then
                                                                 //!< call IsDelivery() to test if the payload needs to
                                                                 //!< be delivered.
const DragDropFlags kDragDropFlagAcceptNoDrawDefaultRect = 1 << 11; //!< Do not draw the default highlight rectangle
                                                                    //!< when hovering over target.
const DragDropFlags kDragDropFlagAcceptNoPreviewTooltip = 1 << 12; //!< Request hiding the BeginDragDropSource tooltip
                                                                   //!< from the BeginDragDropTarget site.
const DragDropFlags kDragDropFlagAcceptPeekOnly =
    kDragDropFlagAcceptBeforeDelivery | kDragDropFlagAcceptNoDrawDefaultRect; //!< For peeking ahead and inspecting the
                                                                              //!< payload before delivery.


/**
 * A primary data type
 */
enum class DataType
{
    eS8, //!< char
    eU8, //!< unsigned char
    eS16, //!< short
    eU16, //!< unsigned short
    eS32, //!< int
    eU32, //!< unsigned int
    eS64, //!< long long, __int64
    eU64, //!< unsigned long long, unsigned __int64
    eFloat, //!< float
    eDouble, //!< double
    eCount //!< Number of items.
};

/**
 * A cardinal direction
 */
enum class Direction
{
    eNone = -1, //!< None
    eLeft = 0, //!< Left
    eRight = 1, //!< Right
    eUp = 2, //<! Up
    eDown = 3, //!< Down
    eCount //!< Number of items.
};

/**
 * Enumeration for pushStyleColor() / popStyleColor()
 */
enum class StyleColor
{
    eText, //!< Text
    eTextDisabled, //!< Disabled text
    eWindowBg, //!< Background of normal windows
    eChildBg, //!< Background of child windows
    ePopupBg, //!< Background of popups, menus, tooltips windows
    eBorder, //!< Border
    eBorderShadow, //!< Border Shadow
    eFrameBg, //!< Background of checkbox, radio button, plot, slider, text input
    eFrameBgHovered, //!< Hovered background
    eFrameBgActive, //!< Active background
    eTitleBg, //!< Title background
    eTitleBgActive, //!<  Active title background
    eTitleBgCollapsed, //!< Collapsed title background
    eMenuBarBg, //!< Menu bar background
    eScrollbarBg, //!< Scroll bar background
    eScrollbarGrab, //!< Grabbed scroll bar
    eScrollbarGrabHovered, //!< Hovered grabbed scroll bar
    eScrollbarGrabActive, //!< Active grabbed scroll bar
    eCheckMark, //!< Check box
    eSliderGrab, //!< Grabbed slider
    eSliderGrabActive, //!< Active grabbed slider
    eButton, //!< Button
    eButtonHovered, //!< Hovered button
    eButtonActive, //!< Active button
    eHeader, //!< Header* colors are used for CollapsingHeader, TreeNode, Selectable, MenuItem
    eHeaderHovered, //!< Hovered header
    eHeaderActive, //!< Active header
    eSeparator, //!< Separator
    eSeparatorHovered, //!< Hovered separator
    eSeparatorActive, //!< Active separator
    eResizeGrip, //!< Resize grip
    eResizeGripHovered, //!< Hovered resize grip
    eResizeGripActive, //!< Active resize grip
    eTab, //!< Tab
    eTabHovered, //!< Hovered tab
    eTabActive, //!< Active tab
    eTabUnfocused, //!< Unfocused tab
    eTabUnfocusedActive, //!< Active unfocused tab
    eDockingPreview, //!< Preview overlay color when about to docking something
    eDockingEmptyBg, //!< Background color for empty node (e.g. CentralNode with no window docked into it)
    ePlotLines, //!< Plot lines
    ePlotLinesHovered, //!< Hovered plot lines
    ePlotHistogram, //!< Histogram
    ePlotHistogramHovered, //!< Hovered histogram
    eTableHeaderBg, //!< Table header background
    eTableBorderStrong, //!< Table outer and header borders (prefer using Alpha=1.0 here)
    eTableBorderLight, //!< Table inner borders (prefer using Alpha=1.0 here)
    eTableRowBg, //!< Table row background (even rows)
    eTableRowBgAlt, //!< Table row background (odd rows)
    eTextSelectedBg, //!< Selected text background
    eDragDropTarget, //!< Drag/drop target
    eNavHighlight, //!< Gamepad/keyboard: current highlighted item
    eNavWindowingHighlight, //!< Highlight window when using CTRL+TAB
    eNavWindowingDimBg, //!< Darken/colorize entire screen behind the CTRL+TAB window list, when active
    eModalWindowDimBg, //!< Darken/colorize entire screen behind a modal window, when one is active
    eWindowShadow, //!< Window shadows
#ifdef IMGUI_NVIDIA
    eCustomChar, //!< Color to render custom char
#endif
    eCount //!< Number of items
};

/**
 * Defines style variable (properties) that can be used
 * to temporarily modify UI styles.
 *
 * The enum only refers to fields of ImGuiStyle which makes sense to be pushed/popped inside UI code. During
 * initialization, feel free to just poke into ImGuiStyle directly.
 *
 * @see  pushStyleVarFloat
 * @see  pushStyleVarFloat2
 * @see  popStyleVar
 */
enum class StyleVar
{
    eAlpha, //!<  (float, Style::alpha)
    eWindowPadding, //!<  (Float2, Style::windowPadding)
    eWindowRounding, //!<  (float, Style::windowRounding)
    eWindowBorderSize, //!<  (float, Style::windowBorderSize)
    eWindowMinSize, //!<  (Float2, Style::windowMinSize)
    eWindowTitleAlign, //!<  (Float2, Style::windowTitleAlign)
    eChildRounding, //!<  (float, Style::childRounding)
    eChildBorderSize, //!<  (float, Style::childBorderSize)
    ePopupRounding, //!<  (float, Style::popupRounding)
    ePopupBorderSize, //!<  (float, Style::popupBorderSize)
    eFramePadding, //!<  (Float2, Style::framePadding)
    eFrameRounding, //!<  (float, Style::frameRounding)
    eFrameBorderSize, //!<  (float, Style::frameBorderSize)
    eItemSpacing, //!<  (Float2, Style::itemSpacing)
    eItemInnerSpacing, //!<  (Float2, Style::itemInnerSpacing)
    eIndentSpacing, //!<  (float, Style::indentSpacing)
    eCellPadding, //!<  (Float2, Style::cellPadding)
    eScrollbarSize, //!<  (float, Style::scrollbarSize)
    eScrollbarRounding, //!<  (float, Style::scrollbarRounding)
    eGrabMinSize, //!<  (float, Style::grabMinSize)
    eGrabRounding, //!<  (float, Style::grabRounding)
    eTabRounding, //!<   (float, Style::tabRounding)
    eButtonTextAlign, //!<  (Float2, Style::buttonTextAlign)
    eSelectableTextAlign, //!<  (Float2, Style::selectableTextAlign)
#ifdef IMGUI_NVIDIA
    eDockSplitterSize, //!<  (float, Style::dockSplitterSize)
#endif
    eCount //!<  Number of items
};


/**
 * Defines flags to be used in colorEdit3() / colorEdit4() / colorPicker3() / colorPicker4() / colorButton()
 */
typedef uint32_t ColorEditFlags;

const ColorEditFlags kColorEditFlagNone = 0; //!< Absence of other color edit flags.
const ColorEditFlags kColorEditFlagNoAlpha = 1 << 1; //!< ColorEdit, ColorPicker, ColorButton: ignore Alpha component
                                                     //!< (read 3 components from the input pointer).
const ColorEditFlags kColorEditFlagNoPicker = 1 << 2; //!< ColorEdit: disable picker when clicking on colored square.
const ColorEditFlags kColorEditFlagNoOptions = 1 << 3; //!< ColorEdit: disable toggling options menu when right-clicking
                                                       //!< on inputs/small preview.
const ColorEditFlags kColorEditFlagNoSmallPreview = 1 << 4; //!< ColorEdit, ColorPicker: disable colored square preview
                                                            //!< next to the inputs. (e.g. to show only the inputs)
const ColorEditFlags kColorEditFlagNoInputs = 1 << 5; //!< ColorEdit, ColorPicker: disable inputs sliders/text widgets
                                                      //!< (e.g. to show only the small preview colored square).
const ColorEditFlags kColorEditFlagNoTooltip = 1 << 6; //!< ColorEdit, ColorPicker, ColorButton: disable tooltip when
                                                       //!< hovering the preview.
const ColorEditFlags kColorEditFlagNoLabel = 1 << 7; //!< ColorEdit, ColorPicker: disable display of inline text label
                                                     //!< (the label is still forwarded to the tooltip and picker).
const ColorEditFlags kColorEditFlagNoSidePreview = 1 << 8; //!< ColorPicker: disable bigger color preview on right side
                                                           //!< of the picker, use small colored square preview instead.
// User Options (right-click on widget to change some of them). You can set application defaults using
// SetColorEditOptions(). The idea is that you probably don't want to override them in most of your calls, let the user
// choose and/or call SetColorEditOptions() during startup.
const ColorEditFlags kColorEditFlagAlphaBar = 1 << 9; //!< ColorEdit, ColorPicker: show vertical alpha bar/gradient in
                                                      //!< picker.
const ColorEditFlags kColorEditFlagAlphaPreview = 1 << 10; //!< ColorEdit, ColorPicker, ColorButton: display preview as
                                                           //!< a transparent color over a checkerboard, instead of
                                                           //!< opaque.
const ColorEditFlags kColorEditFlagAlphaPreviewHalf = 1 << 11; //!< ColorEdit, ColorPicker, ColorButton: display half
                                                               //!< opaque / half checkerboard, instead of opaque.
const ColorEditFlags kColorEditFlagHDR = 1 << 12; //!< (WIP) ColorEdit: Currently only disable 0.0f..1.0f limits in RGBA
                                                  //!< edition (note: you probably want to use ImGuiColorEditFlags_Float
                                                  //!< flag as well).
const ColorEditFlags kColorEditFlagRGB = 1 << 13; //!< [Inputs] ColorEdit: choose one among RGB/HSV/HEX. ColorPicker:
                                                  //!< choose any combination using RGB/HSV/HEX.
const ColorEditFlags kColorEditFlagHSV = 1 << 14; //!< [Inputs]
const ColorEditFlags kColorEditFlagHEX = 1 << 15; //!< [Inputs]
const ColorEditFlags kColorEditFlagUint8 = 1 << 16; //!< [DataType] ColorEdit, ColorPicker, ColorButton: _display_
                                                    //!< values formatted as 0..255.
const ColorEditFlags kColorEditFlagFloat = 1 << 17; //!< [DataType] ColorEdit, ColorPicker, ColorButton: _display_
                                                    //!< values formatted as 0.0f..1.0f floats instead of 0..255
                                                    //!< integers. No round-trip of value via integers.
const ColorEditFlags kColorEditFlagPickerHueBar = 1 << 18; //!< [PickerMode] // ColorPicker: bar for Hue, rectangle for
                                                           //!< Sat/Value.
const ColorEditFlags kColorEditFlagPickerHueWheel = 1 << 19; //!< [PickerMode] // ColorPicker: wheel for Hue, triangle
                                                             //!< for Sat/Value.

/**
 * Defines DrawCornerFlags.
 */
typedef uint32_t DrawCornerFlags;

const DrawCornerFlags kDrawCornerFlagTopLeft = 1 << 0; //!< Top left
const DrawCornerFlags kDrawCornerFlagTopRight = 1 << 1; //!< Top right
const DrawCornerFlags kDrawCornerFlagBotLeft = 1 << 2; //!< Bottom left
const DrawCornerFlags kDrawCornerFlagBotRight = 1 << 3; //!< Bottom right
const DrawCornerFlags kDrawCornerFlagTop = kDrawCornerFlagTopLeft | kDrawCornerFlagTopRight; //!< Top
const DrawCornerFlags kDrawCornerFlagBot = kDrawCornerFlagBotLeft | kDrawCornerFlagBotRight; //!< Bottom
const DrawCornerFlags kDrawCornerFlagLeft = kDrawCornerFlagTopLeft | kDrawCornerFlagBotLeft; //!< Left
const DrawCornerFlags kDrawCornerFlagRight = kDrawCornerFlagTopRight | kDrawCornerFlagBotRight; //!< Right
const DrawCornerFlags kDrawCornerFlagAll = 0xF; //!< All corners

/**
 * Enumeration for GetMouseCursor()
 * User code may request binding to display given cursor by calling SetMouseCursor(), which is why we have some cursors
 * that are marked unused here
 */
enum class MouseCursor
{
    eNone = -1, //!< No mouse cursor.
    eArrow = 0, //!< Arrow
    eTextInput, //!< When hovering over InputText, etc.
    eResizeAll, //!< Unused by simplegui functions
    eResizeNS, //!< When hovering over an horizontal border
    eResizeEW, //!< When hovering over a vertical border or a column
    eResizeNESW, //!< When hovering over the bottom-left corner of a window
    eResizeNWSE, //!< When hovering over the bottom-right corner of a window
    eHand, //!< Unused by simplegui functions. Use for e.g. hyperlinks
    eNotAllowed, //!< When hovering something with disallowed interaction. Usually a crossed circle.
    eCount //!< Number of items
};


/**
 * Condition for simplegui::setWindow***(), setNextWindow***(), setNextTreeNode***() functions
 */
enum class Condition
{
    eAlways = 1 << 0, //!< Set the variable
    eOnce = 1 << 1, //!< Set the variable once per runtime session (only the first call with succeed)
    eFirstUseEver = 1 << 2, //!< Set the variable if the object/window has no persistently saved data (no entry in .ini
                            //!< file)
    eAppearing = 1 << 3, //!< Set the variable if the object/window is appearing after being hidden/inactive (or the
                         //!< first time)
};


/**
 * Struct with all style variables
 *
 *  You may modify the simplegui::getStyle() main instance during initialization and before newFrame().
 * During the frame, use simplegui::pushStyleVar()/popStyleVar() to alter the main style values, and
 * simplegui::pushStyleColor()/popStyleColor() for colors.
 */
struct Style
{
    float alpha; //!< Global alpha applies to everything in simplegui.
    Float2 windowPadding; //!< Padding within a window.
    float windowRounding; //!< Radius of window corners rounding. Set to 0.0f to have rectangular windows.
    float windowBorderSize; //!< Thickness of border around windows. Generally set to 0.0f or 1.0f. (Other values are
                            //!< not well tested and more CPU/GPU costly).
    Float2 windowMinSize; //!< Minimum window size. This is a global setting. If you want to constraint individual
                          //!< windows, use SetNextWindowSizeConstraints().
    Float2 windowTitleAlign; //!< Alignment for title bar text. Defaults to (0.0f,0.5f) for left-aligned,vertically
                             //!< centered.
    Direction windowMenuButtonPosition; //!< Side of the collapsing/docking button in the title bar (None/Left/Right).
                                        //!< Defaults to ImGuiDir_Left.
    float childRounding; //!< Radius of child window corners rounding. Set to 0.0f to have rectangular windows.
    float childBorderSize; //!< Thickness of border around child windows. Generally set to 0.0f or 1.0f. (Other values
                           //!< are not well tested and more CPU/GPU costly).
    float popupRounding; //!< Radius of popup window corners rounding. (Note that tooltip windows use WindowRounding)
    float popupBorderSize; //!< Thickness of border around popup/tooltip windows. Generally set to 0.0f or 1.0f. (Other
                           //!< values are not well tested and more CPU/GPU costly).
    Float2 framePadding; //!< Padding within a framed rectangle (used by most widgets).
    float frameRounding; //!< Radius of frame corners rounding. Set to 0.0f to have rectangular frame (used by most
                         //!< widgets).
    float frameBorderSize; //!< Thickness of border around frames. Generally set to 0.0f or 1.0f. (Other values are not
                           //!< well tested and more CPU/GPU costly).
    Float2 itemSpacing; //!< Horizontal and vertical spacing between widgets/lines.
    Float2 itemInnerSpacing; //!< Horizontal and vertical spacing between within elements of a composed widget (e.g. a
                             //!< slider and its label).
    Float2 cellPadding; //!< Padding within a table cell
    Float2 touchExtraPadding; //!< Expand reactive bounding box for touch-based system where touch position is not
                              //!< accurate enough. Unfortunately we don't sort widgets so priority on overlap will
                              //!< always be given to the first widget. So don't grow this too much!
    float indentSpacing; //!< Horizontal indentation when e.g. entering a tree node. Generally == (FontSize +
                         //!< FramePadding.x*2).
    float columnsMinSpacing; //!< Minimum horizontal spacing between two columns.
    float scrollbarSize; //!< Width of the vertical scrollbar, Height of the horizontal scrollbar.
    float scrollbarRounding; //!< Radius of grab corners for scrollbar.
    float grabMinSize; //!< Minimum width/height of a grab box for slider/scrollbar.
    float grabRounding; //!< Radius of grabs corners rounding. Set to 0.0f to have rectangular slider grabs.
    float tabRounding; //!< Radius of upper corners of a tab. Set to 0.0f to have rectangular tabs.
    float tabBorderSize; //!< Thickness of border around tabs.
    float tabMinWidthForUnselectedCloseButton; //!< Minimum width for close button to appears on an unselected tab when
                                               //!< hovered. Set to 0.0f to always show when hovering, set to FLT_MAX to
                                               //!< never show close button unless selected.
    Direction colorButtonPosition; //!< Side of the color button in the ColorEdit4 widget (left/right). Defaults to
                                   //!< ImGuiDir_Right.
    Float2 buttonTextAlign; //!< Alignment of button text when button is larger than text. Defaults to (0.5f,0.5f) for
                            //!< horizontally+vertically centered.
    Float2 selectableTextAlign; //!< Alignment of selectable text when selectable is larger than text. Defaults to
                                //!< (0.0f, 0.0f) (top-left aligned).
    Float2 displayWindowPadding; //!< Window positions are clamped to be visible within the display area by at least
                                 //!< this amount. Only covers regular windows.
    Float2 displaySafeAreaPadding; //!< If you cannot see the edge of your screen (e.g. on a TV) increase the safe area
                                   //!< padding. Covers popups/tooltips as well regular windows.
    float mouseCursorScale; //!< Scale software rendered mouse cursor (when io.MouseDrawCursor is enabled). May be
                            //!< removed later.
    bool antiAliasedLines; //!< Enable anti-aliasing on lines/borders. Disable if you are really tight on CPU/GPU.
    bool antiAliasedFill; //!< Enable anti-aliasing on filled shapes (rounded rectangles, circles, etc.)
    float curveTessellationTol; //!< Tessellation tolerance when using PathBezierCurveTo() without a specific number of
                                //!< segments. Decrease for highly tessellated curves (higher quality, more polygons),
                                //!< increase to reduce quality.
    float circleSegmentMaxError; //!< Maximum error (in pixels) allowed when using AddCircle()/AddCircleFilled() or
                                 //!< drawing rounded corner rectangles with no explicit segment count specified.
                                 //!< Decrease for higher quality but more geometry.
    float windowShadowSize; //!< Size (in pixels) of window shadows. Set this to zero to disable shadows.
    float windowShadowOffsetDist; //!< Offset distance (in pixels) of window shadows from casting window.
    float windowShadowOffsetAngle; //!< Offset angle of window shadows from casting window (0.0f = left, 0.5f*PI =
                                   //!< bottom, 1.0f*PI = right, 1.5f*PI = top).
    Float4 colors[(size_t)StyleColor::eCount]; //!< Color by style.

#ifdef IMGUI_NVIDIA
    float dockSplitterSize; //!< Thickness of border around docking window. Set to 0.0f to no splitter.
    uint16_t customCharBegin; //!< Code of first custom char. Custom char will be rendered with ImGuiCol_CustomChar.
                              //!< 0xFFFF means no custom char.
#endif

    //! Constructor.
    Style()
    {
    }
};

/**
 * Predefined Style Colors presets.
 */
enum class StyleColorsPreset
{
    eNvidiaDark, //!< NVIDIA Dark colors.
    eNvidiaLight, //!< NVIDIA Light colors.
    eDark, //!< New Dear ImGui style
    eLight, //!< Best used with borders and a custom, thicker font
    eClassic, //!< Classic Dear ImGui style
    eCount //!< Number of items.
};

struct DrawCommand;
struct DrawData;

/**
 * User data to identify a texture.
 */
typedef void* TextureId;

/**
 * Draw callbacks for advanced uses.
 */
typedef void (*DrawCallback)(const DrawData* drawData, const DrawCommand* cmd);

/**
 * Defines a drawing command.
 */
struct DrawCommand
{
    /**
     * The number of indices (multiple of 3) to be rendered as triangles.
     * The vertices are stored in the callee DrawList::vertexBuffer array,
     * indices in IdxBuffer.
     */
    uint32_t elementCount;
    /**
     * The clipping rectangle (x1, y1, x2, y2).
     */
    carb::Float4 clipRect;
    /**
     * User provided texture ID.
     */
    TextureId textureId;
    /**
     * If != NULL, call the function instead of rendering the vertices.
     */
    DrawCallback userCallback;
    /**
     * The draw callback code can access this.
     */
    void* userCallbackData;
};

/**
 * Defines a vertex used for drawing lists.
 */
struct DrawVertex
{
    Float2 position; //!< Position
    Float2 texCoord; //!< Texture Coordinate
    uint32_t color; //!< Color
};

/**
 * Defines a list of draw commands.
 */
struct DrawList
{
    uint32_t commandBufferCount; //!< The number of command in the command buffers.
    DrawCommand* commandBuffers; //!< Draw commands. (Typically 1 command = 1 GPU draw call)
    uint32_t indexBufferSize; //!< The number of index buffers.
    uint32_t* indexBuffer; //!< The index buffers. (Each command consumes command)
    uint32_t vertexBufferSize; //!< The number of vertex buffers.
    DrawVertex* vertexBuffer; //!< The vertex buffers.
};

/**
 * Defines the data used for drawing back-ends
 */
struct DrawData
{
    uint32_t commandListCount; //!< Number of command lists
    DrawList* commandLists; //!< Command lists
    uint32_t vertexCount; //!< Count of vertexes.
    uint32_t indexCount; //!< Count of indexes.
};

//! SimpleGui-specific definition of a wide character.
typedef unsigned short Wchar;

/**
 * Structure defining the configuration for a font.
 */
struct FontConfig
{
    void* fontData; //!< TTF/OTF data
    int fontDataSize; //!< TTF/OTF data size
    bool fontDataOwnedByAtlas; //!< `true` - TTF/OTF data ownership taken by the container ImFontAtlas (will delete
                               //!< memory itself).
    int fontNo; //!< 0 - Index of font within TTF/OTF file
    float sizePixels; //!< Size in pixels for rasterizer (more or less maps to the resulting font height).
    int oversampleH; //!< 3 - Rasterize at higher quality for sub-pixel positioning. We don't use sub-pixel positions on
                     //!< the Y axis.
    int oversampleV; //!< 1 - Rasterize at higher quality for sub-pixel positioning. We don't use sub-pixel positions on
                     //!< the Y axis.
    bool pixelSnapH; //!< false - Align every glyph to pixel boundary. Useful e.g. if you are merging a non-pixel
                     //!< aligned font with the default font. If enabled, you can set OversampleH/V to 1.
    Float2 glyphExtraSpacing; //!< 0, 0 - Extra spacing (in pixels) between glyphs. Only X axis is supported for now.
    Float2 glyphOffset; //!< 0, 0 - Offset all glyphs from this font input.
    const Wchar* glyphRanges; //!< NULL - Pointer to a user-provided list of Unicode range (2 value per range, values
                              //!< are inclusive, zero-terminated list). THE ARRAY DATA NEEDS TO PERSIST AS LONG AS THE
                              //!< FONT IS ALIVE.
    float glyphMinAdvanceX; //!< 0 - Minimum AdvanceX for glyphs, set Min to align font icons, set both Min/Max to
                            //!< enforce mono-space font
    float glyphMaxAdvanceX; //!< FLT_MAX - Maximum AdvanceX for glyphs
    bool mergeMode; //!< false - Merge into previous ImFont, so you can combine multiple inputs font into one ImFont
                    //!< (e.g. ASCII font + icons + Japanese glyphs). You may want to use GlyphOffset.y when merge font
                    //!< of different heights.
    uint32_t rasterizerFlags; //!< 0x00 - Settings for custom font rasterizer (e.g. ImGuiFreeType). Leave as zero if you
                              //!< aren't using one.
    float rasterizerMultiply; //!< 1.0f - Brighten (>1.0f) or darken (<1.0f) font output. Brightening small fonts may be
                              //!< a good workaround to make them more readable.
    char name[40]; //!< (internal) Name (strictly to ease debugging)
    Font* dstFont; //!< (internal)

    //! Constructor.
    FontConfig()
    {
        fontData = nullptr;
        fontDataSize = 0;
        fontDataOwnedByAtlas = true;
        fontNo = 0;
        sizePixels = 0.0f;
        oversampleH = 3;
        oversampleV = 1;
        pixelSnapH = false;
        glyphExtraSpacing = Float2{ 0.0f, 0.0f };
        glyphOffset = Float2{ 0.0f, 0.0f };
        glyphRanges = nullptr;
        glyphMinAdvanceX = 0.0f;
        glyphMaxAdvanceX = CARB_FLOAT_MAX;
        mergeMode = false;
        rasterizerFlags = 0x00;
        rasterizerMultiply = 1.0f;
        memset(name, 0, sizeof(name));
        dstFont = nullptr;
    }
};

/**
 * Structure of a custom rectangle for a font definition.
 */
struct FontCustomRect
{
    uint32_t id; //!< [input] User ID. Use <0x10000 to map into a font glyph, >=0x10000 for other/internal/custom
                 //!< texture data.
    uint16_t width; //!< [input] Desired rectangle dimension
    uint16_t height; //!< [input] Desired rectangle dimension
    uint16_t x; //!< [output] Packed position in Atlas
    uint16_t y; //!< [output] Packed position in Atlas
    float glyphAdvanceX; //!< [input] For custom font glyphs only (ID<0x10000): glyph xadvance
    carb::Float2 glyphOffset; //!< [input] For custom font glyphs only (ID<0x10000): glyph display offset
    Font* font; //!< [input] For custom font glyphs only (ID<0x10000): target font
    //! Constructor.
    FontCustomRect()
    {
        id = 0xFFFFFFFF;
        width = height = 0;
        x = y = 0xFFFF;
        glyphAdvanceX = 0.0f;
        glyphOffset = { 0.0f, 0.0f };
        font = nullptr;
    }
    //! Checks if the font custom rect is packed or not.
    //! @returns `true` if the font is packed; `false` otherwise.
    bool isPacked() const
    {
        return x != 0xFFFF;
    }
};

/**
 * Shared state of InputText(), passed to callback when a ImGuiInputTextFlags_Callback* flag is used and the
 * corresponding callback is triggered.
 */
struct TextEditCallbackData
{
    InputTextFlags eventFlag; //!< One of ImGuiInputTextFlags_Callback* - Read-only
    InputTextFlags flags; //!< What user passed to InputText() - Read-only
    void* userData; //!< What user passed to InputText() - Read-only
    uint16_t eventChar; //!< Character input - Read-write (replace character or set to zero)
    int eventKey; //!< Key pressed (Up/Down/TAB) - Read-only
    char* buf; //!< Current text buffer - Read-write (pointed data only, can't replace the actual pointer)
    int bufTextLen; //!< Current text length in bytes - Read-write
    int bufSize; //!< Maximum text length in bytes - Read-only
    bool bufDirty; //!< Set if you modify Buf/BufTextLen - Write
    int cursorPos; //!< Read-write
    int selectionStart; //!< Read-write (== to SelectionEnd when no selection)
    int selectionEnd; //!< Read-write
};

//! Definition of callback from InputText().
typedef int (*TextEditCallback)(TextEditCallbackData* data);

/**
 * Data payload for Drag and Drop operations: acceptDragDropPayload(), getDragDropPayload()
 */
struct Payload
{
    // Members
    void* data; //!< Data (copied and owned by simplegui)
    int dataSize; //!< Data size

    // [Internal]
    uint32_t sourceId; //!< Source item id
    uint32_t sourceParentId; //!< Source parent id (if available)
    int dataFrameCount; //!< Data timestamp
    char dataType[32 + 1]; //!< Data type tag (short user-supplied string, 32 characters max)
    bool preview; //!< Set when AcceptDragDropPayload() was called and mouse has been hovering the target item (nb:
                  //!< handle overlapping drag targets)
    bool delivery; //!< Set when AcceptDragDropPayload() was called and mouse button is released over the target item.

    //! Constructor.
    Payload()
    {
        clear();
    }
    //! Resets the state to cleared.
    void clear()
    {
        sourceId = sourceParentId = 0;
        data = nullptr;
        dataSize = 0;
        memset(dataType, 0, sizeof(dataType));
        dataFrameCount = -1;
        preview = delivery = false;
    }
    //! Checks if the Payload matches the given type.
    //! @param type The type that should be checked against the `dataType` member.
    //! @returns `true` if the type matches `dataType`; `false` otherwise.
    bool isDataType(const char* type) const
    {
        return dataFrameCount != -1 && strcmp(type, dataType) == 0;
    }
    //! Returns the state of the `preview` member.
    //! @returns The state of the `preview` member.
    bool isPreview() const
    {
        return preview;
    }
    //! Returns the state of the `delivery` member.
    //! @returns The state of the `delivery` member.
    bool isDelivery() const
    {
        return delivery;
    }
};

/**
 * Flags stored in ImGuiViewport::Flags, giving indications to the platform back-ends
 */
typedef uint32_t ViewportFlags;

const ViewportFlags kViewportFlagNone = 0; //!< Absence of other viewport flags.
const ViewportFlags kViewportFlagNoDecoration = 1 << 0; //!< Platform Window: Disable platform decorations: title bar;
                                                        //!< borders; etc.
const ViewportFlags kViewportFlagNoTaskBarIcon = 1 << 1; //!< Platform Window: Disable platform task bar icon (for
                                                         //!< popups; menus; or all windows if
                                                         //!< ImGuiConfigFlags_ViewportsNoTaskBarIcons if set)
const ViewportFlags kViewportFlagNoFocusOnAppearing = 1 << 2; //!< Platform Window: Don't take focus when created.
const ViewportFlags kViewportFlagNoFocusOnClick = 1 << 3; //!< Platform Window: Don't take focus when clicked on.
const ViewportFlags kViewportFlagNoInputs = 1 << 4; //!< Platform Window: Make mouse pass through so we can drag this
                                                    //!< window while peaking behind it.
const ViewportFlags kViewportFlagNoRendererClear = 1 << 5; //!< Platform Window: Renderer doesn't need to clear the
                                                           //!< framebuffer ahead.
const ViewportFlags kViewportFlagTopMost = 1 << 6; //!< Platform Window: Display on top (for tooltips only)

/**
 * The viewports created and managed by simplegui. The role of the platform back-end is to create the platform/OS
 * windows corresponding to each viewport.
 */
struct Viewport
{
    uint32_t id; //!< Unique identifier.
    ViewportFlags flags; //!< Flags describing this viewport.
    Float2 pos; //!< Position of viewport both in simplegui space and in OS desktop/native space
    Float2 size; //!< Size of viewport in pixel
    Float2 workOffsetMin; //!< Work Area: Offset from Pos to top-left corner of Work Area. Generally (0,0) or
                          //!< (0,+main_menu_bar_height). Work Area is Full Area but without menu-bars/status-bars (so
                          //!< WorkArea always fit inside Pos/Size!)
    Float2 workOffsetMax; //!< Work Area: Offset from Pos+Size to bottom-right corner of Work Area. Generally (0,0) or
                          //!< (0,-status_bar_height).
    float dpiScale; //!< 1.0f = 96 DPI = No extra scale
    DrawData* drawData; //!< The ImDrawData corresponding to this viewport. Valid after Render() and until the next call
                        //!< to NewFrame().
    uint32_t parentViewportId; //!< (Advanced) 0: no parent. Instruct the platform back-end to setup a parent/child
                               //!< relationship between platform windows.

    void* rendererUserData; //!< void* to hold custom data structure for the renderer (e.g. swap chain, frame-buffers
                            //!< etc.)
    void* platformUserData; //!< void* to hold custom data structure for the platform (e.g. windowing info, render
                            //!< context)
    void* platformHandle; //!< void* for FindViewportByPlatformHandle(). (e.g. suggested to use natural platform handle
                          //!< such as HWND, GlfwWindow*, SDL_Window*)
    void* platformHandleRaw; //!< void* to hold lower-level, platform-native window handle (e.g. the HWND) when using an
                             //!< abstraction layer like GLFW or SDL (where PlatformHandle would be a SDL_Window*)
    bool platformRequestMove; //!< Platform window requested move (e.g. window was moved by the OS / host window
                              //!< manager, authoritative position will be OS window position)
    bool platformRequestResize; //!< Platform window requested resize (e.g. window was resized by the OS / host window
                                //!< manager, authoritative size will be OS window size)
    bool platformRequestClose; //!< Platform windows requested closure (e.g. window was moved by the OS / host window
                               //!< manager, e.g. pressing ALT-F4)

    //! Constructor.
    Viewport()
    {
        id = 0;
        flags = 0;
        dpiScale = 0.0f;
        drawData = NULL;
        parentViewportId = 0;
        rendererUserData = platformUserData = platformHandle = platformHandleRaw = nullptr;
        platformRequestMove = platformRequestResize = platformRequestClose = false;
    }
    //! Destructor.
    ~Viewport()
    {
        CARB_ASSERT(platformUserData == nullptr && rendererUserData == nullptr);
    }
};


//! [BETA] Rarely used / very advanced uses only. Use with SetNextWindowClass() and DockSpace() functions.
//! Provide hints to the platform back-end via altered viewport flags (enable/disable OS decoration, OS task bar icons,
//! etc.) and OS level parent/child relationships.
struct WindowClass
{
    uint32_t classId; //!< User data. 0 = Default class (unclassed)
    uint32_t parentViewportId; //!< Hint for the platform back-end. If non-zero, the platform back-end can create a
                               //!< parent<>child relationship between the platform windows. Not conforming back-ends
                               //!< are free to e.g. parent every viewport to the main viewport or not.
    ViewportFlags viewportFlagsOverrideSet; //!< Viewport flags to set when a window of this class owns a viewport. This
                                            //!< allows you to enforce OS decoration or task bar icon, override the
                                            //!< defaults on a per-window basis.
    ViewportFlags viewportFlagsOverrideClear; //!< Viewport flags to clear when a window of this class owns a viewport.
                                              //!< This allows you to enforce OS decoration or task bar icon, override
                                              //!< the defaults on a per-window basis.
    DockNodeFlags dockNodeFlagsOverrideSet; //!< [EXPERIMENTAL] Dock node flags to set when a window of this class is
                                            //!< hosted by a dock node (it doesn't have to be selected!)
    DockNodeFlags dockNodeFlagsOverrideClear; //!< [EXPERIMENTAL]
    bool dockingAlwaysTabBar; //!< Set to true to enforce single floating windows of this class always having their own
                              //!< docking node (equivalent of setting the global io.ConfigDockingAlwaysTabBar)
    bool dockingAllowUnclassed; //!< Set to true to allow windows of this class to be docked/merged with an unclassed
                                //!< window. // FIXME-DOCK: Move to DockNodeFlags override?

    //! Constructor.
    WindowClass()
    {
        classId = 0;
        parentViewportId = 0;
        viewportFlagsOverrideSet = viewportFlagsOverrideClear = 0x00;
        dockNodeFlagsOverrideSet = dockNodeFlagsOverrideClear = 0x00;
        dockingAlwaysTabBar = false;
        dockingAllowUnclassed = true;
    }
};

/**
 * Helper: Manually clip large list of items.
 * If you are submitting lots of evenly spaced items and you have a random access to the list, you can perform coarse
 * clipping based on visibility to save yourself from processing those items at all. The clipper calculates the range of
 * visible items and advance the cursor to compensate for the non-visible items we have skipped.
 */
struct ListClipper
{
    float startPosY; //!< Start Y coordinate position;
    float itemsHeight; //!< Height of items.
    int32_t itemsCount; //!< Number of items.
    int32_t stepNo; //!< Stepping
    int32_t displayStart; //!< Display start index.
    int32_t displayEnd; //!< Display end index.
};
} // namespace simplegui
} // namespace carb
