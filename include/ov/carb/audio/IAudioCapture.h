// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief The audio capture interface.
 */
#pragma once

#include "../Interface.h"
#include "../assets/IAssets.h"
#include "AudioTypes.h"


namespace carb
{
namespace audio
{

/******************************** typedefs, enums, & macros **************************************/

/** prototype for a device change callback function.
 *
 *  @param[in] context  the context object that triggered the callback.
 *  @param[in] userData the user data value that was registered with the callback when the
 *                      context object was created.
 *  @returns no return value.
 *
 *  @remarks This callback will be performed any time an audio capture device changes its
 *           connection state.  This includes when a device is connected to or disconnected
 *           from the system, or when the system's default capture device changes.
 *
 *  @remarks Only the fact that a change occurred will be implied with this callback.  It will
 *           not deliver specific information about the device or devices that changed (it could
 *           be many or just one).  It is the callback's responsibility to re-discover the
 *           device list information and decide what if anything should be done about the
 *           change.  The changed device is not reported due to the inherently asynchronous
 *           nature of device change notifications - once the callback decides to collect
 *           information about a device it may have already changed again.
 *
 *  @note The callback will always be performed in the context of the thread that calls into
 *        the update() function.  It is the callback's responsibility to ensure that any shared
 *        resources are accessed in a thread safe manner.
 */
typedef void(CARB_ABI* DeviceChangeCallback)(Context* context, void* userData);


/** flags to control the capture device selection.  No flags are currently defined. */
typedef uint32_t CaptureDeviceFlags;

/** Ignore overruns during capture.
 *  IAudioCapture::lock(), IAudioCapture::read(), IAudioCapture::lock(),
 *  IAudioCapture::unlock() and IAudioCapture::getAvailableFrames()
 *  will no longer return @ref AudioResult::eOverrun when the audio device overruns.
 *  This may be useful in some unexpected circumstance.
 */
constexpr CaptureDeviceFlags fCaptureDeviceFlagIgnoreOverruns = 0x1;

/** describes the parameters to use when selecting a capture device.  This includes the sound
 *  format, buffer length, and device index.  This is used when selecting an audio capture device
 *  with IAudioCapture::setSource().
 */
struct CaptureDeviceDesc
{
    /** flags to control the behavior of selecting a capture device.  No flags are currently
     *  defined.  These may be used to control how the values in @ref ext are interpreted.
     */
    CaptureDeviceFlags flags = 0;

    /** the index of the device to be opened.  This must be less than the return value of the most
     *  recent call to getDeviceCount().  Note that since the capture device list can change at
     *  any time asynchronously due to external user action, setting any particular value here
     *  is never guaranteed to be valid.  There is always the possibility the user could remove
     *  the device after its information is collected but before it is opened.  Using this
     *  index may either fail to open or open a different device if the system's set of connected
     *  capture devices changes.  The only value that guarantees a device will be opened (at
     *  least with default settings) is device 0 - the system's default capture device, as long
     *  as at least one is connected.
     */
    size_t deviceIndex = 0;

    /** the frame rate to capture audio at.  Note that if this is different from the device's
     *  preferred frame rate (retrieved from a call to getDeviceCaps()), a resampler will have
     *  to be added to convert the recorded sound to the requested frame rate.  This will incur
     *  an extra processing cost.  This may be 0 to use the device's preferred frame rate.  The
     *  actual frame rate may be retrieved with getSoundFormat().
     */
    size_t frameRate = 0;

    /** the number of channels to capture audio into.  This should match the device's preferred
     *  channel count (retrieved from a recent call to getDeviceCaps()).  If the channel count is
     *  different, the captured audio may not appear in the expected channels of the buffer.  For
     *  example, if a mono source is used, the captured audio may only be present in the front
     *  left channel of the buffer.  This may be 0 to use the device's preferred channel count.
     *  The actual channel count may be retrieved with getSoundFormat().
     */
    size_t channels = 0;

    /** the format of each audio sample that is captured.  If this does not match the device's
     *  preferred sample format, a conversion will be performed internally as needed.  This will
     *  incur an extra processing cost however.  This may be @ref SampleFormat::eDefault to
     *  indicate that the device's preferred format should be used instead.  In this case, the
     *  actual sample format may be retrieved with getSoundFormat().  Only PCM sample formats
     *  and @ref SampleFormat::eDefault are allowed.
     */
    SampleFormat sampleFormat = SampleFormat::eDefault;

    /** the requested length of the capture buffer.  The interpretation of this size value depends
     *  on the @a lengthType value.  This may be given as a byte count, a frame count, or a time
     *  in milliseconds or microseconds.  This may be set to 0 to allow a default buffer length
     *  to be chosen.  If a zero buffer size is used, the buffer's real size can be discovered
     *  later with a call to getBufferSize().  Note that if a buffer size is given in bytes, it
     *  may be adjusted slightly to ensure it is frame aligned.
     */
    size_t bufferLength = 0;

    /** describes how the buffer length value should be interpreted.  This value is ignored if
     *  @ref bufferLength is 0.  Note that the buffer size will always be rounded up to the next
     *  frame boundary even if the size is specified in bytes.
     */
    UnitType lengthType = UnitType::eFrames;

    /** The number of fragments that the recording buffer is divided into.
     *  The buffer is divided into this number of fragments; each fragment must
     *  be filled before the data will be returned by IAudioCapture::lock() or
     *  IAudioCapture::read().
     *  If all of the fragments are filled during a looping capture, then this
     *  is considered an overrun.
     *  This is important to configure for low latency capture; dividing the
     *  buffer into smaller fragments will reduce additional capture latency.
     *  The minimum possible capture latency is decided by the underlying audio
     *  device (typically 10-20ms).
     *  This will be clamped to the range [2, 64]
     */
    size_t bufferFragments = 8;

    /** extended information for this object.  This is reserved for future expansion and must be
     *  set to nullptr.  The values in @ref flags will affect how this is interpreted.
     */
    void* ext = nullptr;
};

/** flags to control the behavior of context creation.  No flags are currently defined. */
typedef uint32_t CaptureContextFlags;

/** descriptor used to indicate the options passed to the createContext() function.  This
 *  determines the how the context will behave and which capture device will be selected.
 */
struct CaptureContextDesc
{
    /** flags to indicate some additional behavior of the context.  No flags are currently
     *  defined.  This should be set to 0.  In future versions, these flags may be used to
     *  determine how the @ref ext member is interpreted.
     */
    CaptureContextFlags flags = 0;

    /** a callback function to be registered with the new context object.  This callback will be
     *  performed any time the capture device list changes.  This notification will only indicate
     *  that a change has occurred, not which specific change occurred.  It is the caller's
     *  responsibility to re-enumerate devices to determine if any further action is necessary
     *  for the updated device list.  This may be nullptr if no device change notifications are
     *  needed.
     */
    DeviceChangeCallback changeCallback = nullptr;

    /** an opaque context value to be passed to the callback whenever it is performed.  This value
     *  will never be accessed by the context object and will only be passed unmodified to the
     *  callback function.  This value is only used if a device change callback is provided.
     */
    void* callbackContext = nullptr;

    /** a descriptor of the capture device to initially use for this context.  If this device
     *  fails to be selected, the context will still be created and valid, but any attempt to
     *  capture audio on the context will fail until a source is successfully selected with
     *  IAudioCapture::setSource().  The source used on this context may be changed any time
     *  a capture is not in progress using IAudioCapture::setSource().
     */
    CaptureDeviceDesc device;

    /** extended information for this descriptor.  This is reserved for future expansion and
     *  must be set to nullptr.
     */
    void* ext = nullptr;
};

/** stores the buffer information for gaining access to a buffer of raw audio data.  A buffer of
 *  audio data is a linear buffer that may wrap around if it is set to loop.  If the requested
 *  lock region extends past the end of the buffer, it will wrap around to the beginning by
 *  returning a second pointer and length as well.  If the lock region does not wrap around,
 *  only the first pointer and length will be returned.  The second pointer will be nullptr and
 *  the second length will be 0.
 *
 *  The interpretation of this buffer is left up to the caller that locks it.  Since the lock
 *  regions point to the raw audio data, it is the caller's responsibility to know the data's
 *  format (ie: bits per sample) and channel count and interpret them properly.  The data format
 *  information can be collected with the IAudioCapture::getSoundFormat() call.
 *
 *  Note that these returned pointers are writable.  It is possible to write new data to the
 *  locked regions of the buffer (for example, clearing the buffer for security purposes after
 *  reading), but it is generally not advised or necessary.  If the capture buffer is looping,
 *  the contents will quickly be overwritten soon anyway.
 */
struct LockRegion
{
    /** pointer to the first chunk of locked audio data.  This will always be non-nullptr on a
     *  successful lock operation.  This will point to the first byte of the requested frame
     *  offset in the audio buffer.
     */
    void* ptr1;

    /** The length of ptr1 in frames. */
    size_t len1;

    /** pointer to the second chunk of locked audio data.  This will be nullptr on a successful
     *  lock operation if the requested lock region did not wrap around the end of the capture
     *  buffer.  This will always point to the first byte in the audio buffer if it is
     *  non-nullptr.
     */
    void* ptr2;

    /** The length of ptr2 in frames. */
    size_t len2;
};


/********************************** IAudioCapture Interface **************************************/
/** Low-Level Audio Capture Plugin Interface.
 *
 *  See these pages for more detail:
 *  @rst
    * :ref:`carbonite-audio-label`
    * :ref:`carbonite-audio-capture-label`
    @endrst
 */
struct IAudioCapture
{
    CARB_PLUGIN_INTERFACE("carb::audio::IAudioCapture", 1, 0)

    /************************ device and context management functions ****************************/
    /** retrieves the current audio capture device count for the system.
     *
     *  @returns    the number of audio capture devices that are currently connected to the system
     *              or known to the system.  The system's default or preferred device can be found
     *              by looking at the @a flags member of the info structure retrieved from
     *              getDeviceCaps().  The default capture device will always be device 0.
     *
     *  @note The device count is a potentially volatile value.  This can change at any time,
     *        without notice, due to user action.  For example, the user could remove an audio
     *        device from the system or add a new one at any time.  Thus it is a good idea to
     *        select the device with setSource() as quickly as possible after choosing the device
     *        index.  There is no guarantee that the device list will even remain stable during
     *        a single device enumeration loop.  The only device index that is guaranteed to be
     *        valid is the system default device index of 0 (as long as at least one capture
     *        device is connected).
     */
    size_t(CARB_ABI* getDeviceCount)();

    /** retrieves the capabilities and information about a single audio capture device.
     *
     *  @param[in] deviceIndex  the index of the device to retrieve info for.  This must be
     *                          between 0 and the most recent return value from getDeviceCount().
     *  @param[out] caps    receives the device information.  The @a thisSize value must be set
     *                      to sizeof(DeviceCaps) before calling.
     *  @retval AudioResult::eOk if the device info was successfully retrieved.
     *  @retval AudioResult::eOutOfRange if the requested device index was out of range
     *           of devices connected to the system.
     *  @retval AudioResult::eInvalidParameter if the @a thisSize value was not initialized
     *           in @p caps or @p caps was nullptr.
     *  @retval AudioResult::eNotAllowed if the device list could not be accessed.
     *  @returns an @ref AudioResult error code if the call fails for any other reason.
     *
     *  @remarks This retrieves information about a single audio capture device.  The
     *           information will be returned in the @p caps buffer.  This may fail if the
     *           device corresponding to the requested index has been removed from the
     *           system.
     */
    AudioResult(CARB_ABI* getDeviceCaps)(size_t deviceIndex, DeviceCaps* caps);


    /** creates a new audio capture context object.
     *
     *  @param[in] desc     a descriptor of the initial settings for the capture context.  This
     *                      may be nullptr to create a context that uses the default capture
     *                      device in its preferred format.  The device's format information
     *                      may later be retrieved with getSoundFormat().
     *  @returns the newly created audio capture context object if it was successfully created.
     *  @returns nullptr if a new context object could not be created.
     *
     *  @remarks This creates a new audio capture context object.  This object is responsible
     *           for managing all access to a single instance of the audio capture device.  Note
     *           that there will be a separate recording thread associated with each instance of
     *           the capture context.
     *
     *  @note If the requested device fails to be set during context creation, the returned
     *        context object will still be valid, but it will not be able to capture until a
     *        successful call to setSource() returns.  This case may be checked upon return
     *        using isSourceValid().
     */
    Context*(CARB_ABI* createContext)(const CaptureContextDesc* desc);

    /** destroys an audio capture context object.
     *
     *  @param[in] context  the context object to be destroyed.  Upon return, this object will no
     *                      longer be valid.
     *  @retval AudioResult::eOk if the object was successfully destroyed.
     *  @retval AudioResult::eInvalidParameter if nullptr is passed in.
     *
     *  @remarks This destroys an audio capture context object that was previously created
     *           with createContext().  If the context is still active and has a running capture
     *           thread, it will be stopped before the object is destroyed.  All resources
     *           associated with the context will be both invalidated and destroyed as well.
     */
    AudioResult(CARB_ABI* destroyContext)(Context* context);

    /** selects a capture device and prepares it to begin recording.
     *
     *  @param[in] context  the context object to set the capture source.  This context may not
     *                      be actively capturing data when calling this function.  A call to
     *                      captureStop() must be made first in this case.
     *  @param[in] desc     a descriptor of the device that should be opened and what format the
     *                      captured data should be in.  This may be nullptr to select the
     *                      system's default capture device in its preferred format and a default
     *                      capture buffer size.
     *  @retval AudioResult::eOk if the requested device was successfully opened.
     *  @retval AudioResult::eNotAllowed if a capture is currently running.
     *  @retval AudioResult::eOutOfRange if the requested device index is not valid.
     *  @retval AudioResult::eInvalidFormat if the requested frame rate or channel count
     *           is not allowed.
     *  @returns an @ref AudioResult error code if the device could not be opened.
     *
     *  @remarks This selects a capture device and sets up the recording buffer for it.  The audio
     *           will always be captured as uncompressed PCM data in the requested format.  The
     *           captured audio can be accessed using the lock() and unlock() functions, or with
     *           read().
     *
     *  @remarks The length of the buffer would depend on the needs of the caller.  For example,
     *           if a looping capture is used, the buffer should be long enough that the caller
     *           can keep pace with reading the data as it is generated, but not too long that
     *           it will introduce an unnecessary amount of latency between when the audio is
     *           captured and the caller does something with it.  In many situations, a 10-20
     *           millisecond buffer should suffice for streaming applications.  A delay greater
     *           than 100ms between when audio is produced and heard will be noticeable to the
     *           human ear.
     *
     *  @note If this fails, the state of the context may be reset.  This must succeed before
     *        any capture operation can be started with captureStart().  All efforts will be
     *        made to keep the previous source valid in as many failure cases as possible, but
     *        this cannot always be guaranteed.  If the call fails, the isSourceValid() call can
     *        be used to check whether a capture is still possible without having to call
     *        setSource() again.
     *
     *  @note If this succeeds (or fails in non-recoverable ways mentioned above), the context's
     *        state will have been reset.  This means that any previously captured data will be
     *        lost and that any previous data format information may be changed.  This includes
     *        the case of selecting the same device that was previously selected.
     */
    AudioResult(CARB_ABI* setSource)(Context* context, const CaptureDeviceDesc* desc);

    /** checks whether a valid capture source is currently selected.
     *
     *  @param[in] context  the context to check the capture source on.  This may not be nullptr.
     *  @returns true if the context's currently selected capture source is valid and can start
     *           a capture.
     *  @returns false if the context's source is not valid or could not be selected.
     *
     *  @remarks This checks whether a context's current source is valid for capture immediately.
     *           This can be used after capture creation to test whether the source was
     *           successfully selected instead of having to attempt to start a capture then
     *           clear the buffer.
     */
    bool(CARB_ABI* isSourceValid)(Context* context);


    /**************************** capture state management functions *****************************/
    /** starts capturing audio data from the currently opened device.
     *
     *  @param[in] context  the context object to start capturing audio from.  This context must
     *                      have successfully selected a capture device either on creation or by
     *                      using setSource().  This can be verified with isSourceValid().
     *  @param[in] looping  set to true if the audio data should loop over the buffer and
     *                      overwrite previous data once it reaches the end.  Set to false to
     *                      perform a one-shot capture into the buffer.  In this mode, the capture
     *                      will automatically stop once the buffer becomes full.
     *                      Note that the cursor position is not reset when capture is stopped,
     *                      so starting a non-looping capture session will result in the remainder
     *                      of the buffer being captured.
     *
     *  @retval AudioResult::eOk if the capture is successfully started.
     *  @retval AudioResult::eDeviceNotOpen if a device is not selected in the context.
     *  @returns an AudioResult::* error code if the capture could not be started for any other
     *           reason.
     *
     *  @remarks This starts an audio capture on the currently opened device for a context.  The
     *           audio data will be captured into an internal data buffer that was created with
     *           the information passed to the last call to setSource() or when the context was
     *           created.  The recorded audio data can be accessed by locking regions of the
     *           buffer and copying the data out, or by calling read() to retrieve the data as
     *           it is produced.
     *
     *  @remarks If the capture buffer is looping, old data will be overwritten after
     *           the buffer fills up.  It is the caller's responsibility in this case to
     *           periodically check the capture's position with getCaptureCuror(), and
     *           read the data out once enough has been captured.
     *
     *  @remarks If the capture is not looping, the buffer's data will remain intact even
     *           after the capture is complete or is stopped.  The caller can read the data
     *           back at any time.
     *
     *  @remarks When the capture is started, any previous contents of the buffer will remain
     *           and will be added to by the new captured data.  If the buffer should be
     *           cleared before continuing from a previous capture, the clear() function must
     *           be explicitly called first.  Each time a new capture device is selected with
     *           setSource(), the buffer will be cleared implicitly.  Each time the capture
     *           is stopped with captureStop() however, the buffer will not be cleared and can
     *           be added to by starting it again.
     */
    AudioResult(CARB_ABI* captureStart)(Context* context, bool looping);

    /** stops capturing audio data from the selected device.
     *
     *  @param[in] context  the context object to stop capturing audio on.  This context object
     *                      must have successfully opened a device either on creation or by using
     *                      setSource().
     *  @returns    @ref AudioResult::eOk if the capture is stopped.
     *  @returns    @ref AudioResult::eDeviceNotOpen if a device is not open.
     *  @returns    an AudioResult::* error code if the capture could not be stopped or was never
     *              started in the first place.
     *
     *  @remarks    This stops an active audio capture on the currently opened device for a
     *              context.  The contents of the capture buffer will be left unmodified and can
     *              still be accessed with lock() and unlock() or read().  If the capture is
     *              started again, it will be resumed from the same location in the buffer as
     *              when it was stopped unless clear() is called.
     *
     *  @note       If the capture is stopped somewhere in the middle of the buffer (whether
     *              looping or not), the contents of the remainder of the buffer will be
     *              undefined.  Calling getCaptureCursor() even after the capture is stopped
     *              will still return the correct position of the last valid frame of data.
     */
    AudioResult(CARB_ABI* captureStop)(Context* context);

    /** retrieves the current capture position in the device's buffer.
     *
     *  @param[in] context  the context object that the capture is occurring on.  This context
     *                      object must have successfully opened a device either on creation or
     *                      by using setSource().
     *  @param[in] type     the units to retrieve the current position in.  Note that this may
     *                      not return an accurate position if units in milliseconds or
     *                      microseconds are requested.  If a position in bytes is requested, the
     *                      returned value will always be aligned to a frame boundary.
     *  @param[out] position    receives the current position of the capture cursor in the units
     *                          specified by @p type.  All frames in the buffer up to this point
     *                          will contain valid audio data.
     *  @retval AudioResult::eOk if the current capture position is successfully retrieved.
     *  @retval AudioResult::eDeviceNotOpen if no device has been opened.
     *  @returns an AudioResult::* error code if the capture position could not be retrieved.
     *
     *  @remarks This retrieves the current capture position in the buffer.  This position will be
     *           valid even after the capture has been stopped with captureStop().  All data in
     *           the buffer up to the returned position will be valid.  If the buffer was looping,
     *           some of the data at the end of the buffer may be valid as well.
     */
    AudioResult(CARB_ABI* getCaptureCursor)(Context* context, UnitType type, size_t* position);

    /** checks whether the context is currently capturing audio data.
     *
     *  @param[in] context  the context object to check the recording state of.
     *  @returns true if the context is currently recording.
     *  @returns false if the context is not recording or no device has been opened.
     */
    bool(CARB_ABI* isCapturing)(Context* context);


    /********************************* data management functions *********************************/
    /** locks a portion of the buffer to be read.
     *
     *  @param[in] context  the context object to read data from.  This context object must have
     *                      successfully opened a device either on creation or by using
     *                      setSource().
     *  @param[in] length   The length of the buffer to lock, in frames.
     *                      This may be 0 to lock as much data as possible.
     *  @param[out] region  receives the lock region information if the lock operation is
     *                      successful.  This region may be split into two chunks if the region
     *                      wraps around the end of the buffer.  The values in this struct are
     *                      undefined if the function fails.
     *  @retval AudioResult::eOk if the requested region is successfully locked.
     *  @retval AudioResult::eDeviceNotOpen if no device is open.
     *  @retval AudioResult::eNotAllowed if a region is already locked and needs to be
     *           unlocked.
     *  @retval AudioResult::eOverrun if data has not been read fast
     *           enough and some captured data has overwritten unread data.
     *  @returns an @ref AudioResult error code if the region could not be locked.
     *
     *  @remarks This locks a portion of the buffer so that data can be read back.  The region may
     *           be split into two chunks if the region wraps around the end of the buffer.  A
     *           non-looping buffer will always be truncated to the end of the buffer and only one
     *           chunk will be returned.
     *
     *  @remarks Once the locked region is no longer needed, it must be unlocked with a call to
     *           unlock().  Only one region may be locked on the buffer at any given time.
     *           Attempting to call lock() twice in a row without unlocking first will result
     *           in the second call failing.
     */
    AudioResult(CARB_ABI* lock)(Context* context, size_t length, LockRegion* region);

    /** unlocks a previously locked region of the buffer.
     *
     *  @param[in] context  the context object to unlock the buffer on.  This context object must
     *                      have successfully opened a device either on creation or by using
     *                      setSource().
     *  @retval AudioResult::eOk if the region is successfully unlocked.
     *  @retval AudioResult::eDeviceNotOpen if no device has been opened.
     *  @retval AudioResult::eNotAllowed if no region is currently locked.
     *  @retval AudioResult::eOverrun if the audio device wrote to part
     *           of the locked buffer before unlock() was called.
     *  @returns an @ref AudioResult error code if the region could not be unlocked.
     *
     *  @remarks This unlocks a region of the buffer that was locked with a previous call to
     *           lock().  Once the buffer is unlocked, a new region may be locked with lock().
     *
     *  @note Once the buffer is unlocked, it is not guaranteed that the memory in the region
     *        will still be accessible.  The caller should never cache the locked region
     *        information between unlocks and future locks.  The next successful lock call
     *        may return a completely different region of memory even for the same offset
     *        in the buffer.
     */
    AudioResult(CARB_ABI* unlock)(Context* context);

    /** calculates the required buffer size to store the requested number of frames.
     *
     *  @param[in] context  the context object to calculate the buffer size for.  This context
     *                      object must have successfully opened a device either on creation
     *                      or by using setSource().
     *  @param[in] framesCount  the number of frames to calculate the storage space for.
     *  @returns the number of bytes required to store the requested frame count for this
     *           context's current data format.
     *  @returns 0 if no device has been selected.
     *
     *  @remarks This is a helper function to calculate the required size in bytes for a buffer
     *           to store a requested number of frames.  This can be used with read() to ensure
     *           a buffer is large enough to store the available number of frames of data.
     */
    size_t(CARB_ABI* calculateReadBufferSize)(Context* context, size_t frameCount);

    /** attempts to read captured data from the buffer.
     *
     *  @param[in] context  the context object to read data from.  This context must have
     *                      successfully opened a device either upon creation or by using
     *                      setSource().
     *  @param[out] buffer  receives the data that was read from the capture stream.  This may
     *                      be nullptr if only the available number of frames of data is required.
     *                      In this case, the available frame count will be returned in the
     *                      @p framesRead parameter. The contents of @p buffer are undefined if
     *                      this function fails.
     *  @param[in] lengthInFrames   the maximum number of frames of data that can fit in the
     *                              buffer @p buffer.  It is the caller's responsibility to
     *                              know what the device's channel count is and account for
     *                              that when allocating memory for the buffer.  The size of
     *                              the required buffer in bytes may be calculated with a call
     *                              to calculateReadBufferSize().  This will always account for
     *                              data format and channel count.  If @p buffer is nullptr, this
     *                              must be set to 0.
     *  @param[out] framesRead  receives the total number of frames of audio data that were read
     *                          into the buffer.  This may be 0 if no new data is available to be
     *                          read.  It is the caller's responsibility to check this value after
     *                          return to ensure garbage data is not being processed.  It cannot
     *                          be assumed that the buffer will always be completely filled.  The
     *                          calculateReadBufferSize() helper function can be used to calculate
     *                          the size of the read data in bytes from this value.  This value
     *                          will not exceed @p lengthInFrames if @p buffer is not nullptr.  If
     *                          the buffer is nullptr, this will receive the minimum number of
     *                          frames currently available to be read.
     *  @retval AudioResult::eOk if at least one frame of data is successfully read from the
     *           buffer.
     *  @retval AudioResult::eTryAgain if no data was available to read and @p buffer was
     *           not nullptr and no other errors occurred (the value in @p framesRead will be 0), or
     *           if @p buffer is `nullptr` and there is data to be read (the number of available
     *           frames will be stored in @p framesRead).
     *  @retval AudioResult::eDeviceNotOpen if no device has been opened.
     *  @retval AudioResult::eOutOfMemory if @p lengthInFrames is 0 and @p buffer is not
     *           nullptr.
     *  @retval AudioResult::eOverrun if data has not been read fast
     *           enough and some captured data has overwritten unread data.
     *  @returns an @ref AudioResult error code if the operation failed for some other reason.
     *
     *  @remarks This provides a means to read the captured audio data as a 'stream'.  This
     *           behaves similarly to the libc read() function - it will read data from the
     *           current cursor position up to either as much data will fit in the buffer or
     *           is currently available to immediately read.
     *
     *  @remarks This may be called with a nullptr buffer and 0 buffer length if only the number
     *           of available frames of data is needed.  This call method may be used to determine
     *           the size of the buffer to pass in or to ensure the buffer is large enough.  No
     *           actual data will be read and the next call with a non-nullptr buffer will be the
     *           one to consume the data.  Note that if audio data is still being captured, the
     *           number of available frames of data may increase between two consecutive calls.
     *           This is fine as only the number of frames that will fit in the output buffer
     *           will be consumed and any extra frames that were captured in the meantime can be
     *           consumed on the next call.  The calcReadBufferSize() function may be used to
     *           help calculate the required buffer size in bytes from the available frame count.
     *
     *  @note It is the caller's responsibility to call this frequently enough that the capture
     *        cursor on a looping buffer will not write over the data that has been read so far.
     *        If the capture cursor passes over the read cursor (ie: the last point that the data
     *        had been read up to), some corruption in the data may occur when it is finally read
     *        again.  Data should be read from the buffer with a period of at most half the length
     *        of time required to fill the capture buffer.
     *
     *  @note When this method of reading the captured data is used, it's not necessary to lock
     *        and unlock regions of the buffer.  While using this read method may be easier, it
     *        may end up being less efficient in certain applications because it may incur some
     *        extra processing overhead per call that can be avoided with the use of lock(),
     *        unlock(), and getCaptureCursor().  Also, using this method cannot guarantee that
     *        the data will be delivered in uniformly sized chunks.
     *
     *  @note The buffer length and read count must be specified in frames here, otherwise there
     *        is no accurate means of identifying how much data will fit in the buffer and how
     *        much data was actually read.
     */
    AudioResult(CARB_ABI* read)(Context* context, void* buffer, size_t lengthInFrames, size_t* framesRead);

    /** retrieves the size of the capture buffer in frames.
     *
     *  @param[in] context  the context object to retrieve the buffer size for.  This context
     *                      object must have successfully opened a device either upon creation or
     *                      by using setSource().
     *  @param[in] type     the units to retrieve the buffer size in.
     *  @returns the size of the capture buffer in the specified units.
     *  @returns 0 if a device hasn't been opened yet.
     *
     *  @remarks This retrieves the actual size of the capture buffer that was created when the
     *           context opened its device.  This is useful as a way to retrieve the buffer size
     *           in different units than it was created in (ie: create in frames but retrieved as
     *           time in milliseconds), or to retrieve the size of a buffer on a device that was
     *           opened with a zero buffer size.
     *
     *  @remarks If the buffer length is requested in milliseconds or microseconds, it may not be
     *           precise due to rounding issues.  The returned buffer size in this case will be
     *           the minimum time in milliseconds or microseconds that the buffer will cover.
     *
     *  @remarks If the buffer length is requested in bytes, the returned size will always be
     *           aligned to a frame boundary.
     */
    size_t(CARB_ABI* getBufferSize)(Context* context, UnitType type);

    /** retrieves the captured data's format information.
     *
     *  @param[in] context  the context object to retrieve the data format information for.  This
     *                      context object must have successfully opened a device either upon
     *                      creation or by using setSource().
     *  @param[out] format  receives the data format information.  This may not be nullptr.
     *  @retval AudioResult::eOk if the data format information is successfully returned.
     *  @retval AudioResult::eDeviceNotOpen if no device has been opened.
     *  @returns an @ref AudioResult error code if any other error occurs.
     *
     *  @remarks This retrieves the data format information for the internal capture buffer.  This
     *           will identify how the captured audio is intended to be processed.  This can be
     *           collected to identify the actual capture data format if the device is opened
     *           using its preferred channel count and frame rate.
     */
    AudioResult(CARB_ABI* getSoundFormat)(Context* context, SoundFormat* format);

    /** clears the capture buffer and resets it to the start.
     *
     *  @param[in] context  the context object to clear the buffer on.  This context object must
     *                      have successfully opened a device using setSource().
     *  @retval AudioResult::eOk if the capture buffer was successfully cleared.
     *  @retval AudioResult::eDeviceNotOpen if no device has been opened.
     *  @retval AudioResult::eNotAllowed if the buffer is currently locked or currently capturing.
     *  @returns an @ref AudioResult error code if the operation fails for any other reason.
     *
     *  @remarks This clears the contents of the capture buffer and resets its cursor back to the
     *           start of the buffer.  This should only be done when the device is not capturing
     *           data.  Attempting to clear the buffer while the capture is running will cause this
     *           call to fail.
     */
    AudioResult(CARB_ABI* clear)(Context* context);

    /** Get the available number of frames to be read.
     *  @param[in]  context   The context object to clear the buffer on.  This context object must
     *                        have successfully opened a device using setSource().
     *  @param[out] available The number of frames that can be read from the buffer.
     *  @retval AudioResult::eOk if the frame count was retrieved successfully.
     *  @retval AudioResult::eDeviceNotOpen if no device has been opened.
     *  @retval AudioResult::eOverrun if data has not been read fast enough and
     *           the buffer filled up.
     *
     */
    AudioResult(CARB_ABI* getAvailableFrames)(Context* context, size_t* available);
};
} // namespace audio
} // namespace carb
