// Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
// --------- Warning: This is a build system generated file. ----------
//

//! @file
//!
//! @brief This file was generated by <i>omni.bind</i>.

#include <omni/core/OmniAttr.h>
#include <omni/core/Interface.h>
#include <omni/core/ResultError.h>

#include <functional>
#include <utility>
#include <type_traits>

#ifndef OMNI_BIND_INCLUDE_INTERFACE_IMPL


/** Interface to collect and retrieve information about displays attached to the system.  Each
 *  display is a viewport onto the desktop's virtual screen space and has an origin and size.
 *  Most displays are capable of switching between several modes.  A mode is a combination of
 *  a viewport resolution (width, height, and color depth), and refresh rate.  Display info
 *  may be collected using this interface, but it does not handle making changes to the current
 *  mode for any given display.
 */
template <>
class omni::core::Generated<omni::platforminfo::IDisplayInfo_abi> : public omni::platforminfo::IDisplayInfo_abi
{
public:
    OMNI_PLUGIN_INTERFACE("omni::platforminfo::IDisplayInfo")

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
    size_t getDisplayCount() noexcept;

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
    bool getDisplayInfo(size_t displayIndex, omni::platforminfo::DisplayInfo* infoOut) noexcept;

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
    size_t getModeCount(const omni::platforminfo::DisplayInfo* display) noexcept;

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
    bool getModeInfo(const omni::platforminfo::DisplayInfo* display,
                     omni::platforminfo::ModeIndex modeIndex,
                     omni::platforminfo::ModeInfo* infoOut) noexcept;

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
    bool getTotalDisplaySize(carb::Int2* origin, carb::Int2* size) noexcept;
};

#endif

#ifndef OMNI_BIND_INCLUDE_INTERFACE_DECL

inline size_t omni::core::Generated<omni::platforminfo::IDisplayInfo_abi>::getDisplayCount() noexcept
{
    return getDisplayCount_abi();
}

inline bool omni::core::Generated<omni::platforminfo::IDisplayInfo_abi>::getDisplayInfo(
    size_t displayIndex, omni::platforminfo::DisplayInfo* infoOut) noexcept
{
    return getDisplayInfo_abi(displayIndex, infoOut);
}

inline size_t omni::core::Generated<omni::platforminfo::IDisplayInfo_abi>::getModeCount(
    const omni::platforminfo::DisplayInfo* display) noexcept
{
    return getModeCount_abi(display);
}

inline bool omni::core::Generated<omni::platforminfo::IDisplayInfo_abi>::getModeInfo(
    const omni::platforminfo::DisplayInfo* display,
    omni::platforminfo::ModeIndex modeIndex,
    omni::platforminfo::ModeInfo* infoOut) noexcept
{
    return getModeInfo_abi(display, modeIndex, infoOut);
}

inline bool omni::core::Generated<omni::platforminfo::IDisplayInfo_abi>::getTotalDisplaySize(carb::Int2* origin,
                                                                                             carb::Int2* size) noexcept
{
    return getTotalDisplaySize_abi(origin, size);
}

#endif

#undef OMNI_BIND_INCLUDE_INTERFACE_DECL
#undef OMNI_BIND_INCLUDE_INTERFACE_IMPL
static_assert(std::is_standard_layout<omni::platforminfo::ModeInfo>::value,
              "omni::platforminfo::ModeInfo must be standard layout to be used in ONI ABI");
static_assert(std::is_standard_layout<omni::platforminfo::DisplayInfo>::value,
              "omni::platforminfo::DisplayInfo must be standard layout to be used in ONI ABI");
