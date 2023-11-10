// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief The audio device enumeration interface.
 */
#pragma once

#include "../Interface.h"
#include "AudioTypes.h"


namespace carb
{
namespace audio
{

/** the direction to collect device information for. */
enum class DeviceType
{
    ePlayback, /**< Audio playback devices (e.g. headphones) */
    eCapture, /**< Audio capture devices (e.g. microphone) */
};

/** Which device backend is being used for audio.
 *
 *  @note @ref IAudioCapture will always use DirectSound as a backend on Windows.
 *       This behavior will be changed eventually so that @ref IAudioCapture uses
 *       the same device backend as other systems.
 */
enum class DeviceBackend
{
    /** The null audio device backend was selected.
     *  Audio playback  and capture will still function as expected, but the
     *  output audio will be dropped and input audio will be silence.
     *  This will only be used if manually selected via the `audio/deviceBackend`
     *  settings key or in the case where a system is missing its core audio libraries.
     */
    eNull,

    /** Windows Audio Services device API (aka WASAPI).
     *  This is the only device backend on Windows.
     *  This is fairly user-friendly and should not require any special handling.
     */
    eWindowsAudioServices,

    /** Pulse Audio sound server for Linux.
     *  This is the standard sound server on Linux for consumer audio.
     *  This API is fairly user-friendly and should not require any special handling.
     *  Each of the audio streams through Pulse Audio will be visible through
     *  programs such as `pavucontrol` (volume control program).
     *  The name of these streams can be set for @ref IAudioPlayback with
     *  @ref PlaybackContextDesc::outputDisplayName; if that was not set, a generic
     *  name will be used.
     */
    ePulseAudio,

    /** Advance Linux Audio System (ALSA).
     *  This is the underlying kernel sound system as well as an array of plugins.
     *  Some users may use ALSA so they can use the JACK plugin for professional
     *  audio applications.
     *  Some users also prefer to use the `dmix` and `dsnoop` sound servers
     *  instead of Pulse Audio.
     *  ALSA is not user-friendly, so the following issues may appear:
     *   - ALSA devices are sensitive to latency because, for the most part,
     *     they use a fixed-size ring buffer, so it is possible to get audio
     *     underruns or overruns on a heavily loaded system or a device
     *     configured with an extremely small buffer.
     *   - Some ALSA devices are exclusive access, so there is no guaranteed that
     *     they will open properly.
     *   - Multiple configurations of each physical device show up as a separate
     *     audio device, so a system with two audio devices will have ~40 ALSA
     *     devices.
     *   - Opening an ALSA device can take hundreds of milliseconds.
     *     Combined with the huge device count, this can mean that manually
     *     enumerating all devices on the system can take several seconds.
     *   - Some versions of libasound will automatically create devices with
     *     invalid configurations, such as `dmix` devices that are flagged as
     *     supporting playback and capture but will fail to open for capture.
     *   - ALSA devices can be configured with some formats that carb.audio
     *     does not support, such as big endian formats, ULAW or 64 bit float.
     *     Users should use a `plug` (format conversion) plugin for ALSA if they
     *     need to use a device that requires a format such as this.
     */
    eAlsa,

    /** The Mac OS CoreAudio system.
     *  This is the standard sound system used on Mac OS.
     *  This is fairly user-friendly and should not require any special handling.
     */
    eCoreAudio,
};

/** A callback that is performed when a device notification occurs.
 *  @param[in] ctx The context value this notification was registered with.
 *
 *  @remarks This notification will occur on every device change that @p ctx
 *           registered to. No information about what changed is provided.
 */
typedef void (*DeviceNotifyCallback)(void* ctx);

/** A device change notification context.
 *  This instance exists to track the lifetime of a device change notification
 *  subscription.
 */
class DeviceChangeNotifier;

/** An interface to provide simple audio device enumeration functionality, as
 *  well as device change notifications.  This is able to enumerate all audio
 *  devices attached to the system at any given point and collect the
 *  information for each device.  This is able to collect information and
 *  provide notifications for both playback and capture devices.
 */
struct IAudioDevice
{
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    CARB_PLUGIN_INTERFACE("carb::audio::IAudioDevice", 1, 1);
#endif

    /** retrieves the total number of devices attached to the system of a requested type.
     *
     *  @param[in] dir  the audio direction to get the device count for.
     *  @returns the total number of connected audio devices of the requested type.
     *  @returns 0 if no audio devices are connected to the system.
     */
    size_t(CARB_ABI* getDeviceCount)(DeviceType dir);

    /** Retrieve the capabilities of a device.
     *  @param[in]    dir   The audio direction of the device.
     *  @param[in]    index The index of the device to retrieve the description for.  This
     *                      should be between 0 and one less than the most recent return
     *                      value of getDeviceCount().
     *  @param[inout] caps  The capabilities of this device.
     *                      `caps->thisSize` must be set to `sizeof(*caps)`
     *                      before passing it.
     *  @returns  @ref AudioResult::eOk if the device info was successfully retrieved.
     *  @returns  @ref AudioResult::eInvalidParameter if the @a thisSize value is not properly
     *            initialized in @p caps or @p caps is nullptr.
     *  @returns  @ref AudioResult::eOutOfRange if the requested device index is out of range of
     *            the system's current device count.
     *  @returns  @ref AudioResult::eNotSupported if a device is found but it requires an
     *            unsupported sample format.
     *  @returns  an AudioResult::* error code if another failure occurred.
     */
    AudioResult(CARB_ABI* getDeviceCaps)(DeviceType dir, size_t index, carb::audio::DeviceCaps* caps);

    /** Create a device notification object.
     *  @param[in] type     The device type to fire the callback for.
     *  @param[in] callback The callback that will be fired when a device change occurs.
     *                      This must not be nullptr.
     *  @param[in] context  The object passed to the parameter of @p callback.
     *
     *  @returns A valid device notifier object if successful.
     *           This must be destroyed with destroyNotifier() when device
     *           notifications are no longer needed.
     *  @returns nullptr if an error occurred.
     */
    DeviceChangeNotifier*(CARB_ABI* createNotifier)(DeviceType type, DeviceNotifyCallback callback, void* context);

    /** Destroy a device notification object.
     *  @param[in] notifier The notification object to free.
     *                      Device notification callbacks for this object will
     *                      no longer occur.
     */
    void(CARB_ABI* destroyNotifier)(DeviceChangeNotifier* notifier);

    /** Query the device backend that's currently in use.
     *  @returns The device backend in use.
     *  @note This returned value is cached internally, so these calls are inexpensive.
     *  @note The value this returns will not change until carb.audio reloads.
     */
    DeviceBackend(CARB_ABI* getBackend)();

    /** Retrieve a minimal set of device properties.
     *  @param[in]    dir   The audio direction of the device.
     *  @param[in]    index The index of the device to retrieve the description for.  This
     *                      should be between 0 and one less than the most recent return
     *                      value of getDeviceCount().
     *  @param[inout] caps  The basic properties of this device.
     *                      @ref DeviceCaps::name and @ref DeviceCaps::guid will
     *                      be written to this.
     *                      @ref DeviceCaps::flags will have @ref fDeviceFlagDefault
     *                      set if this is the default device, but no other flags
     *                      will be set.
     *                      All other members of this struct will be set to default
     *                      values.
     *                      `caps->thisSize` must be set to `sizeof(*caps)`
     *                      before passing it.
     *
     *  @retval AudioResult::eOk on success.
     *  @retval AudioResult::eInvalidParameter if @p caps had an invalid `thisSize` member or was `nullptr`.
     *  @retval AudioResult::eOutOfRange if @p index was past the end of the device list.
     */
    AudioResult(CARB_ABI* getDeviceName)(DeviceType type, size_t index, DeviceCaps* caps);

    /** Retrieve the capabilities of a device.
     *  @param[in]    dir   The audio direction of the device.
     *  @param[in]    guid  The guid of the device to retrieve the description for.
     *  @param[inout] caps  The capabilities of this device.
     *                      `caps->thisSize` must be set to `sizeof(*caps)`
     *                      before passing it.
     *  @returns  @ref AudioResult::eOk if the device info was successfully retrieved.
     *  @returns  @ref AudioResult::eInvalidParameter if the @a thisSize value is not properly
     *            initialized in @p caps, @p caps is `nullptr` or @p guid is `nullptr`.
     *  @returns  @ref AudioResult::eOutOfRange if @p guid did not correspond to a device.
     *  @returns  @ref AudioResult::eNotSupported if a device is found but it requires an
     *            unsupported sample format.
     *  @returns  an AudioResult::* error code if another failure occurred.
     */
    AudioResult(CARB_ABI* getDeviceCapsByGuid)(DeviceType dir,
                                               const carb::extras::Guid* guid,
                                               carb::audio::DeviceCaps* caps);

    /** Retrieve a minimal set of device properties.
     *  @param[in]    dir   The audio direction of the device.
     *  @param[in]    guid  The guid of the device to retrieve the description for.
     *  @param[inout] caps  The basic properties of this device.
     *                      @ref DeviceCaps::name and @ref DeviceCaps::guid will
     *                      be written to this.
     *                      @ref DeviceCaps::flags will have @ref fDeviceFlagDefault
     *                      set if this is the default device, but no other flags
     *                      will be set.
     *                      All other members of this struct will be set to default
     *                      values.
     *                      `caps->thisSize` must be set to `sizeof(*caps)`
     *                      before passing it.
     *
     *  @retval AudioResult::eOk on success.
     *  @retval AudioResult::eInvalidParameter if the @a thisSize value is not properly
     *           initialized in @p caps, @p caps is `nullptr` or @p guid is `nullptr`.
     *  @retval AudioResult::eOutOfRange if @p guid did not correspond to a device.
     */
    AudioResult(CARB_ABI* getDeviceNameByGuid)(DeviceType dir,
                                               const carb::extras::Guid* guid,
                                               carb::audio::DeviceCaps* caps);
};

} // namespace audio
} // namespace carb
