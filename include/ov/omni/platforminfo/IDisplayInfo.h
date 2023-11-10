// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Helper interface to retrieve display info.
 */
#pragma once

#include "../../carb/Types.h"
#include "../core/IObject.h"


namespace omni
{
/** Platform and operating system info namespace. */
namespace platforminfo
{

/** Forward declaration of the IDisplayInfo API object. */
class IDisplayInfo;

/** Base type for the display information flags.  These flags all start with @a fDisplayFlag*. */
using DisplayFlags OMNI_ATTR("flag, prefix=fDisplayFlag") = uint32_t;

/** Flag that indicates that the display is the primary one in the system.
 *
 *  This often means that new windows will open on this display by default or that the main
 *  system menu (ie: Windows' task bar, Linux's or Mac OS's menu bar, etc) will appear on.
 */
constexpr DisplayFlags fDisplayFlagPrimary = 0x01;

/** Base type for the display mode information flags.  These flags all start with @a fModeFlag*. */
using ModeFlags OMNI_ATTR("flag, prefix=fModeFlag") = uint32_t;

/** Flag to indicate that the screen mode is interlaced.
 *
 *  An interlaced display will render every other line of the image alternating between even
 *  and odd scanlines each frame.  This can result in less flicker at the same refresh rate
 *  as a non-interlaced display, or less data transmission required per frame at a lower
 *  refresh rate.
 */
constexpr ModeFlags fModeFlagInterlaced = 0x01;

/** Flag to indicate that this mode will be stretched.
 *
 *  When this flag is present, it indicates that the given display mode will be stretched to
 *  to fill the display if it is not natively supported by the hardware.  This may result in
 *  scaling artifacts depending on the amount of stretching that is done.
 */
constexpr ModeFlags fModeFlagStretched = 0x02;

/** Flag to indicate that this mode will be centered on the display.
 *
 *  When this flag is present, it indicates that the given display mode will be centered
 *  on the display if it is not natively supported by the hardware.  This will result in
 *  blank bars being used around the edges of the display to fill in unused space.
 */
constexpr ModeFlags fModeFlagCentered = 0x04;


/** Base type for a display mode index. */
using ModeIndex OMNI_ATTR("constant, prefix=kModeIndex") = size_t;

/** Special mode index value to get the information for a display's current mode.
 *
 *  This is accepted by IDisplayInfo::getModeInfo() in the @a modeIndex parameter.
 */
constexpr ModeIndex kModeIndexCurrent = (ModeIndex)~0ull;


/** Possible display orientation names.
 *
 *  These indicate how the screen is rotated from its native default orientation.  The rotation
 *  angle is considered in a clockwise direction.
 */
enum class OMNI_ATTR("prefix=e") Orientation
{
    eDefault, ///< The natural display orientation for the display.
    e90, ///< The image is rotated 90 degrees clockwise.
    e180, ///< The image is rotated 180 degrees clockwise.
    e270, ///< The image is rotated 270 degrees clockwise.
};

/** Contains information about a single display mode.  This includes the mode's size in pixels,
 *  bit depth, refresh rate, and orientation.
 */
struct ModeInfo
{
    carb::Int2 size = {}; ///< Horizontal (x) and vertical (y) size of the screen in pixels.
    uint32_t bitsPerPixel = 0; ///< Pixel bit depth.  Many modern systems will only report 32 bits.
    uint32_t refreshRate = 0; ///< The refresh rate of the display in Hertz or zero if not applicable.
    ModeFlags flags = 0; ///< Flags describing the state of the mode.
    Orientation orientation = Orientation::eDefault; ///< The orientation of the mode.
};

/** Contains information about a single display device.  This includes the name of the display,
 *  the adapter it is connected to, the virtual coordinates on the desktop where the display's
 *  image maps to, and the current display mode information.
 */
struct DisplayInfo
{
    /** The name of the display device.  This typically maps to a monitor, laptop screen, or other
     *  pixel display device.  This name should be suitable for display to a user.
     */
    char OMNI_ATTR("c_str") displayName[128] = {};

    /** The system specific identifier of the display device.  This is suitable for using with
     *  other platform specific APIs that accept a display device name.
     */
    char OMNI_ATTR("c_str") displayId[128] = {};

    /** The name of the graphics adapter the display is connected to.  Typically this is the
     *  name of the GPU or other graphics device that the display is connected to.  This name
     *  should be suitable for display to a user.
     */
    char OMNI_ATTR("c_str") adapterName[128] = {};

    /** The system specific identifier of the graphics adapter device.  This is suitable for using
     *  with other platform specific APIs that accept a graphics adapter name.
     */
    char OMNI_ATTR("c_str") adapterId[128] = {};

    /** The coordinates of the origin of this display device on the desktop's virtual screen.
     *  In situations where there is only a single display, this will always be (0, 0).  It will
     *  be in non-mirrored multi-display setups that this can be used to determine how each
     *  display's viewport is positioned relative to each other.
     */
    carb::Int2 origin = {};

    /** The current display mode in use on the display. */
    ModeInfo current = {};

    /** Flags to indicate additional information about this display. */
    DisplayFlags flags = 0;
};


/** Interface to collect and retrieve information about displays attached to the system.  Each
 *  display is a viewport onto the desktop's virtual screen space and has an origin and size.
 *  Most displays are capable of switching between several modes.  A mode is a combination of
 *  a viewport resolution (width, height, and color depth), and refresh rate.  Display info
 *  may be collected using this interface, but it does not handle making changes to the current
 *  mode for any given display.
 */
class IDisplayInfo_abi : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.platforminfo.IDisplayInfo")>
{
protected:
    /** Retrieves the total number of displays connected to the system.
     *
     *  @returns The total number of displays connected to the system.  This typically includes
     *           displays that are currently turned off.  Note that the return value here is
     *           volatile and may change at any point due to user action (either in the OS or
     *           by unplugging or connecting a display).  This value should not be cached for
     *           extended periods of time.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual size_t getDisplayCount_abi() noexcept = 0;

    /** Retrieves information about a single connected display.
     *
     *  @param[in] displayIndex The zero based index of the display to retrieve the information
     *                          for.  This call will fail if the index is out of the range of
     *                          the number of connected displays, thus it is not necessary to
     *                          IDisplayInfo::getDisplayCount() to enumerate display information
     *                          in a counted loop.
     *  @param[out] infoOut     Receives the information for the requested display.  This may
     *                          not be `nullptr`.  This returned information may change at any
     *                          time due to user action and should therefore not be cached.
     *  @returns `true` if the information for the requested display is successfully retrieved.
     *           Returns `false` if the @p displayIndex index was out of the range of connected
     *           display devices or the information could not be retrieved for any reason.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual bool getDisplayInfo_abi(size_t displayIndex, OMNI_ATTR("out, not_null") DisplayInfo* infoOut) noexcept = 0;

    /** Retrieves the total number of display modes for a given display.
     *
     *  @param[in] display  The display to retrieve the mode count for.  This may not be
     *                      `nullptr`.  This must have been retrieved from a recent call to
     *                      IDisplayInfo::getDisplayInfo().
     *  @returns The total number of display modes supported by the requested display.  Returns
     *           0 if the mode count information could not be retrieved.  A connected valid
     *           display will always support at least one mode.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual size_t getModeCount_abi(OMNI_ATTR("in, not_null") const DisplayInfo* display) noexcept = 0;

    /** Retrieves the information for a single display mode for a given display.
     *
     *  @param[in] display      The display to retrieve the mode count for.  This may not be
     *                          `nullptr`.  This must have been retrieved from a recent call to
     *                          IDisplayInfo::getDisplayInfo().
     *  @param[in] modeIndex    The zero based index of the mode to retrieve for the given
     *                          display.  This make also be @ref kModeIndexCurrent to retrieve the
     *                          information for the given display's current mode.  This call will
     *                          simply fail if this index is out of range of the number of modes
     *                          supported by the given display, thus it is not necessary to call
     *                          IDisplayInfo::getModeCount() to use in a counted loop.
     *  @param[out] infoOut     Receives the information for the requested mode of the given
     *                          display.  This may not be `nullptr`.
     *  @returns `true` if the information for the requested mode is successfully retrieved.
     *           Returns `false` if the given index was out of range of the number of modes
     *           supported by the given display or the mode's information could not be retrieved
     *           for any reason.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual bool getModeInfo_abi(OMNI_ATTR("in, not_null") const DisplayInfo* display,
                                 ModeIndex modeIndex,
                                 OMNI_ATTR("out, not_null") ModeInfo* infoOut) noexcept = 0;

    /** Retrieves the total virtual screen size that all connected displays cover.
     *
     *  @param[out] origin  Receives the coordinates of the origin of the rectangle that the
     *                      virtual screen covers.  This may be `nullptr` if the origin point
     *                      is not needed.
     *  @param[out] size    Receives the width and height of the rectangle that the virtual
     *                      screen covers.  This may be `nullptr` if the size is not needed.
     *  @returns `true` if either the origin, size, or both origin and size of the virtual
     *           screen are retrieved successfully.  Returns `false` if the size of the virtual
     *           screen could not be retrieved or both @p origin and @p size are `nullptr`.
     *
     *  @remarks This retrieves the total virtual screen size for the system.  This is the
     *           union of the rectangles that all connected displays cover.  Note that this
     *           will also include any empty space between or around displays that is not
     *           covered by another display.
     *
     *  @thread_safety This call is thread safe.
     */
    virtual bool getTotalDisplaySize_abi(OMNI_ATTR("out") carb::Int2* origin,
                                         OMNI_ATTR("out") carb::Int2* size) noexcept = 0;
};

} // namespace platforminfo
} // namespace omni

#define OMNI_BIND_INCLUDE_INTERFACE_DECL
#include "IDisplayInfo.gen.h"

/** @copydoc omni::platforminfo::IDisplayInfo_abi */
class omni::platforminfo::IDisplayInfo : public omni::core::Generated<omni::platforminfo::IDisplayInfo_abi>
{
};

#define OMNI_BIND_INCLUDE_INTERFACE_IMPL
#include "IDisplayInfo.gen.h"
