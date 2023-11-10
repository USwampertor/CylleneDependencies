// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief An updated audio capture interface.
 */
#pragma once

#include <omni/core/BuiltIn.h>
#include <omni/core/IObject.h>
#include <omni/core/Api.h>
#include <omni/log/ILog.h>

#include <carb/audio/AudioTypes.h>


namespace omni
{

/** Omniverse audio project. */
namespace audio
{

/** The omni::audio namespace is part of an audio refresh that is still in the experimental stages.
 *  This currently only contains the IAudioCapture interface, which was created to address a number
 *  of defects in carb::audio::IAudioCapture.
 *  IAudioCapture is currently in beta stage; the interface is complete and unlikely to change
 *  substantially but it has not been tested heavily in a real-world use case yet.
 *  The ABI of IAudioCapture and ICaptureStream are stable and will not be broken without a
 *  deprecation warning given in advance.
 */
namespace experimental
{
/******************************** renamed carb::audio types **************************************/
/** @copydoc carb::audio::AudioResult */
using AudioResult = carb::audio::AudioResult;

/** @copydoc carb::audio::SpeakerMode */
using SpeakerMode = carb::audio::SpeakerMode;

/** @copydoc carb::audio::SampleFormat */
using SampleFormat = carb::audio::SampleFormat;

/** @copydoc carb::audio::SoundFormat */
using SoundFormat = carb::audio::SoundFormat;

/******************************** typedefs, enums, & macros **************************************/

/** The minimal format needed to interpret an PCM audio stream.
 *  Non-PCM (compressed) audio streams may need different format information or
 *  no format information to interpret.
 */
struct Format
{
    /** The rate at which this audio stream is intended to be played (number of
     *  audio frames per second).
     *  A frame is a group of @ref channels audio samples taken at a given time point;
     *  this is often referred to as the 'sample rate'.
     */
    size_t frameRate = 0;

    /** The number of channels in the audio stream. */
    size_t channels = 0;

    /** This specifies the intended usage of each channel in the audio stream.
     *  This will be a combination of one or more of the @ref carb::audio::Speaker names or
     *  a @ref carb::audio::SpeakerMode name.
     */
    SpeakerMode channelMask = carb::audio::kSpeakerModeDefault;

    /** The data type of each audio sample in this audio stream. */
    SampleFormat format = SampleFormat::eDefault;
};


/** Flags to indicate some additional behaviour of the device.
 *  No flags are currently defined.
 */
using CaptureDeviceFlags = uint32_t;

/** The parameters to use when opening a capture device. */
struct CaptureDeviceDesc
{
    /** Flags to indicate some additional behaviour of the device.
     *  No flags are currently defined.  This should be set to 0.
     */
    CaptureDeviceFlags flags = 0;

    /** The index of the device to be opened.
     *  Index 0 will always be the system's default capture device.
     *  These indices are the same as the ones used in @ref carb::audio::IAudioDevice,
     *  so that interface should be used to enumerate the system's audio devices.
     *
     *  This must be less than the return value of the most recent call to @ref
     *  carb::audio::IAudioDevice::getDeviceCount() for capture devices.
     *  Note that since the capture device list can change at any time
     *  asynchronously due to external user action, setting any particular
     *  value here is never guaranteed to be valid.
     *  There is always the possibility the user could remove the device after
     *  its information is collected but before it is opened.
     *
     *  Additionally, there is always a possibility that a device will fail to
     *  open or the system has no audio capture devices, so code must be able to
     *  handle that possibility.
     *  In particular, it is common to run into misconfigured devices under ALSA;
     *  some distributions automatically configure devices that will not open.
     */
    size_t index = 0;

    /** The format to use for the capture stream.
     *  Leaving this struct at its default values will cause the audio device's
     *  default format to be used
     *  (This can be queried later with @ref ICaptureStream_abi::getSoundFormat_abi()).
     *  The audio stream will accept any valid PCM format, even if the underlying
     *  device does not support that format.
     */
    Format format = {};

    /** The length of the ring buffer to capture data into, in frames.
     *  The buffer length in combination with @ref bufferFragments determines
     *  the minimum possible latency of the audio stream, which the time to
     *  record one fragment of audio.
     *  The buffer length will be automatically adjusted to ensure that its size
     *  is divisible by the fragment count.
     *
     *  This will not determine the buffer size of the underlying audio device,
     *  but if it is possible, the underlying audio device will be adjusted to
     *  best match this buffer length and fragment combination.
     *
     *  Setting this to 0 will choose the underlying device's buffer length or
     *  a default value if the underlying device has no buffer.
     */
    size_t bufferLength = 0;

    /** The number of fragments that the recording buffer is divided into.
     *  When using callback based recording, one fragment of audio will be sent
     *  to each callback.
     *  When using polling based recording, data will become available in one
     *  fragment increments.
     *  One fragment of audio becomes the minimum latency for the audio stream.
     *  Setting an excessive number of fragments will reduce the efficiency of
     *  the audio stream.
     *
     *  Setting this to 0 will choose the underlying device's fragment count or
     *  a default value if the underlying device doesn't use a ring buffer system.
     */
    size_t bufferFragments = 0;
};

class ICaptureStream;

/** A callback that's used to receive audio data from an audio capture stream.
 *  @param[inout] stream  The stream that this callback was fired from.
 *  @param[in]    buffer  The audio data that's been captured.
 *                        The data in the buffer will be in the format specified
 *                        by ICaptureStream::getSoundFormat().
 *                        If the stream was created with @ref fCaptureStreamFlagLowLatency,
 *                        this buffer will not be valid once the callback returns;
 *                        otherwise, the buffer will be invalidated once ICaptureStream::unlock()
 *                        is called for this portion of the buffer.
 *                        See the note on unlocking the buffer for more detail on this.
 *  @param[in]    frames  The length of audio in @p buffer, in frames.
 *                        If the stream was created with @ref fCaptureStreamFlagLowLatency,
 *                        the frame count passed to each callback may differ;
 *                        otherwise, the frame count will be exactly one fragment long.
 *                        If you need to convert to bytes, you can use
 *                        @ref carb::audio::convertUnits() or
 *                        @ref carb::audio::framesToBytes().
 *  @param[inout] context The user-specified callback data.
 *
 *  @note If the stream was not created with @ref fCaptureStreamFlagLowLatency,
 *        data returned from this function is considered *locked*.
 *        For data to become unlocked, the caller needs to call
 *        ICaptureStream::unlock() with the number of frames that have been
 *        locked by this call.
 *        If the data is not unlocked, that portion of the buffer cannot be overwritten.
 *        It is not required to release the buffer before the callback ends, but
 *        the data needs to be unlocked eventually otherwise you will stop receiving
 *        data and an overrun may occur.
 *
 *  @note This callback executes from its own thread, so thread safety will need
 *        to be considered.
 *        This is called on the same thread as CaptureInfoCallback, so these
 *        two calls can access the same data without concurrency issues.
 *
 *  @note This callback must return as fast as possible, since this is called
 *        directly from the audio capture thread (e.g. under 1ms).
 *        A callback that takes too long could cause capture overruns.
 *        Doing anything that could involve substantial waits (e.g. calling into python)
 *        should be avoided; in those cases you should copy the buffer somewhere else
 *        or use a omni::extras::DataStreamer for a series of asynchronous
 *        data packets.
 *
 *  @note It is safe to call any @ref ICaptureStream function from within this
 *        callback, but it is not safe to destroy the @ref ICaptureStream object
 *        from within it.
 */
using CaptureDataCallback = void (*)(ICaptureStream* stream, const void* buffer, size_t frames, void* context);

/** The reason that a CaptureInfoCallback was called. */
enum class CaptureInfoType
{
    /** An overrun has occurred.
     *  Overruns occur when data is not captured fast enough from the audio
     *  device, the device's buffer fills up and data is lost.
     *  An overrun can occur because a callback took too long, data was not
     *  unlocked fast enough or the underlying audio device encountered an
     *  overrun for other reasons (e.g. system latency).
     */
    eOverrun,

    /** The audio device has encountered an unrecoverable issue, such as the
     *  device being unplugged, so capture has stopped.
     */
    eDeviceLost,
};

/** Extra data for @ref CaptureInfoCallback.
 *  The union member used is based off of the `type` parameter.
 */
union CaptureInfoData
{
    /** Data for @ref CaptureInfoType::eOverrun.
     *  Overruns have no additional data.
     */
    struct
    {
    } overrun;

    /** Data for @ref CaptureInfoType::eDeviceLost.
     *  Device lost events have no additional data.
     */
    struct
    {
    } deviceLost;

    /** Size reservation to avoid ABI breaks. */
    struct
    {
        OMNI_ATTR("no_py") uint8_t x[256];
    } reserved;
};

/** A callback that's used to signal capture stream events.
 *  @param[inout] stream  The stream that this callback was fired from.
 *  @param[in]    type    What type of event occurred.
 *  @param[in]    data    Data related to this error.
 *  @param[inout] context The user-specified callback data.
 *
 *  @remarks This callback is used to signal various events that a capture stream
 *           can encounter.
 *           Overrun events indicate that data from the capture stream has been lost.
 *           The callee should do whatever is appropriate to handle this situation
 *           (e.g. toggle a warning indicator on the UI).
 *           Device lost events indicate that an unrecoverable issue has occurred
 *           with the audio device (such as it being unplugged) and capture cannot
 *           continue.
 *
 *  @note This callback executes from its own thread, so thread safety will need
 *        to be considered.
 *        This is called on the same thread as @ref CaptureDataCallback, so these
 *        two calls can access the same data without concurrency issues.
 *
 *  @note Similarly to @ref CaptureDataCallback, this callback should return as
 *        quickly as possible to avoid causing overruns.
 *        After an overrun has occurred, the device will be restarted immediately,
 *        so this callback taking too long could result in another overrun callback
 *        happening immediately after your callback returns.
 */
using CaptureInfoCallback = void (*)(ICaptureStream* stream,
                                     CaptureInfoType type,
                                     const CaptureInfoData* data,
                                     void* context);

/** Flags to indicate some additional behaviour of the stream. */
using CaptureStreamFlags = uint32_t;

/** Bypass the stream's capture buffer entirely to minimize audio capture
 *  latency as much as possible.
 *  When this flag is set, audio buffers received from the underlying device
 *  will be passed directly to the capture callback.
 *
 *  @note In this mode, you are at the mercy of the underlying audio system.
 *        The buffers sent to the callback may vary in size and timing.
 *        The bufferLength and bufferFragments parameters passed to the device
 *        are ignored when using this flag.
 *        Since the buffer size may vary, your callbacks must complete as fast
 *        as possible in case the device sends you very small buffers.
 *
 *  @note The audio buffer *must* be discarded before the callback returns.
 *        This will also cause @ref ICaptureStream_abi::unlock_abi() to be a noop.
 *
 *  @note If a format conversion is required, the data will be written to an
 *        intermediate buffer that will be sent to your callback.
 */
constexpr CaptureStreamFlags fCaptureStreamFlagLowLatency = 0x1;

/** The descriptor used to create a new @ref ICaptureStream. */
struct CaptureStreamDesc
{
    /** Flags to indicate some additional behaviour of the stream. */
    CaptureStreamFlags flags = 0;

    /** A callback that can be used to receive captured data.
     *  This can be nullptr if a polling style of capture is desired.
     *  This may not be nullptr if @ref fCaptureStreamFlagLowLatency is specified
     *  in @ref flags.
     */
    CaptureDataCallback dataCallback = nullptr;

    /** A callback that can be used to receive notifications of when an overrun occurs.
     *  This call be nullptr if these notifications are not needed.
     *  This callback can be used when either polling or callback style recording
     *  are being used.
     */
    CaptureInfoCallback errorCallback = nullptr;

    /** An opaque context value to be passed to the callback whenever it is performed.
     *  This is passed to @ref dataCallback and @ref errorCallback.
     */
    void* callbackContext = nullptr;

    /** A descriptor of the capture device to use for this stream. */
    CaptureDeviceDesc device;
};

/** Flags that alter the behavior of @ref ICaptureStream_abi::start_abi(). */
using CaptureStreamStartFlags = uint32_t;

/** This will cause recording to be stopped once the full buffer has been recorded.
 *  This may be useful if you want to capture a fixed length of data without having
 *  to time stopping the device.
 *  After capture has stopped, you can use @ref ICaptureStream_abi::lock_abi() to grab
 *  the whole buffer and just use it directly.
 */
constexpr CaptureStreamStartFlags fCaptureStreamStartFlagOneShot = 0x1;

/** Flags that alter the behavior of @ref ICaptureStream_abi::stop_abi(). */
using CaptureStreamStopFlags = uint32_t;

/** If this flag is passed, the call to @ref ICaptureStream_abi::stop_abi() will block
 *  until the capture thread has stopped.
 *  This guarantees that no more data will be captured and no more capture
 *  callbacks will be sent.
 */
constexpr CaptureStreamStopFlags fCaptureStreamStopFlagSync = 0x1;

/********************************** IAudioCapture Interface **************************************/

/** An individual audio capture stream. */
class ICaptureStream_abi : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.audio.ICaptureStream")>
{
protected:
    /** Starts capturing audio data from the stream.
     *
     *  @param[in] flags Flags to alter the recording behavior.
     *
     *  @returns @ref carb::audio::AudioResult::eOk if the capture is successfully started.
     *  @returns An @ref AudioResult error code if the stream could not be started.
     *
     *  @remarks Audio streams are in a stopped state when they're created.
     *           You must call start() to start recording audio.
     *
     *  @remarks Without @ref fCaptureStreamStartFlagOneShot, the stream will
     *           perform looping capture. This means that once the ring buffer
     *           has been filled, audio will start being recorded from the start
     *           of the buffer again.
     *
     *  @remarks Looping audio will not overwrite unread parts of the ring buffer.
     *           Only parts of the buffer that have been unlocked can be overwritten
     *           by the audio system.
     *           Data written into the ring buffer must be unlocked periodically
     *           when using looping capture or the ring buffer will fill up and
     *           the device will overrun.
     *
     *  @thread_safety Calls to this function cannot occur concurrently with
     *                 other calls to @ref ICaptureStream_abi::start_abi() or @ref ICaptureStream_abi::stop_abi().
     */
    virtual AudioResult start_abi(CaptureStreamStartFlags flags) noexcept = 0;

    /** Stop capturing audio data from the stream.
     *
     *  @param[in] flags Flags to alter the stopping behavior.
     *
     *  @returns @ref carb::audio::AudioResult::eOk if the capture was successfully stopped.
     *  @returns @ref carb::audio::AudioResult::eNotAllowed if the stream was already stopped.
     *  @returns An @ref AudioResult error code if something else went wrong.
     *           The stream is likely broken in this case.
     *
     *  @remarks This will stop capturing audio. Any audio data that would have
     *           been captured between this point and the next call to @ref ICaptureStream_abi::start_abi()
     *           will be lost.
     *
     *  @remarks If there is unread data in the buffer, that data can still be
     *           read with lock() after the stream is stopped.
     *
     *  @note If fCaptureStreamStopFlagSync was not specified, the stop call will not sync with the
     *        device so you could still receive callbacks after.
     *
     *  @note CC-1180: You cannot use fCaptureStreamStopFlagSync from a callback.
     *
     *  @thread_safety Calls to this function cannot occur concurrently with
     *                 other calls to @ref ICaptureStream_abi::start_abi() or @ref ICaptureStream_abi::stop_abi().
     */
    virtual AudioResult stop_abi(CaptureStreamStopFlags flags) noexcept = 0;

    /** Check if the stream is still capturing data.
     *  @returns `true` if the stream is still capturing.
     *  @returns `false` if the stream is stopped.
     *           Callbacks will no longer be sent if `false` was returned unless the strema was
     *           stoped with @ref ICaptureStream_abi::stop_abi() was called without fCaptureStreamStopFlagSync.
     */
    virtual bool isCapturing_abi() noexcept = 0;

    /** Get the available number of frames to be read.
     *
     *  @param[out] available The number of frames that can be read from the buffer.
     *
     *  @returns @ref carb::audio::AudioResult::eOk if the frame count was retrieved successfully.
     *  @returns @ref carb::audio::AudioResult::eOverrun if data has not been read fast enough and
     *           the buffer filled up.
     *  @returns @ref carb::audio::AudioResult::eNotAllowed if callback recording is being used.
     *  @returns An @ref AudioResult error code if the operation fails for any other reason.
     *
     *  @remarks This will check how much data is available to be read from the buffer.
     *           This call is only valid when polling style recording is in use.
     */
    virtual AudioResult getAvailableFrames_abi(OMNI_ATTR("out") size_t* available) noexcept = 0;

    /** Lock the next chunk of the buffer to be read.
     *
     *  @param[in]  request  The length of the buffer to lock, in frames.
     *                       This may be 0 to lock as much data as possible.
     *                       This does not need to be a multiple of the fragment
     *                       length.
     *                       If you need to convert bytes to frames, you can use
     *                       @ref carb::audio::convertUnits() or
     *                       @ref carb::audio::framesToBytes() (note that this is
     *                       slow due to requiring a division).
     *  @param[out] region   Receives the audio data.
     *                       This can be `nullptr` to query the available data
     *                       in the buffer, rather than locking it.
     *                       This buffer can be held until unlock() is called;
     *                       after unlock is called, the stream can start writing
     *                       into this buffer.
     *  @param[out] received The length of data available in @p buffer, in frames.
     *                       This will not exceed @p request.
     *                       Due to the fact that a ring buffer is used, you may
     *                       have more data in the buffer than is returned in one
     *                       call; a second call would be needed to read the full
     *                       buffer.
     *                       If you need to convert this to bytes, you can use
     *                       @ref carb::audio::convertUnits() or
     *                       @ref carb::audio::framesToBytes().
     *
     *  @returns @ref carb::audio::AudioResult::eOk if the requested region is successfully locked.
     *  @returns @ref carb::audio::AudioResult::eOutOfMemory if there is no audio data available
     *           in the buffer yet.
     *  @returns @ref carb::audio::AudioResult::eNotAllowed if a region is already locked
     *           and needs to be unlocked.
     *  @returns @ref carb::audio::AudioResult::eNotAllowed if the stream is using callback
     *           style recording.
     *  @returns @ref carb::audio::AudioResult::eOverrun if data has not been read fast
     *           enough and the underlying device has overrun.
     *           This will happen on some audio systems (e.g. ALSA) if the
     *           capture buffer fills up.
     *           This can also happen on some audio systems sporadically if the
     *           device's timing characteristics are too aggressive.
     *  @returns an carb::audio::AudioResult::e* error code if the region could not be locked.
     *
     *  @remarks This is used to get data from the capture stream when polling
     *           style recording is being used (ie: there is no data callback).
     *           When using this style of recording, portions of the buffer must
     *           be periodically locked and unlocked.
     *
     *  @remarks This retrieves the next portion of the buffer.
     *           This portion of the buffer is considered to be locked until
     *           @ref ICaptureStream_abi::unlock_abi() is called.
     *           Only one region of the buffer can be locked at a time.
     *           When using a looping capture, the caller should ensure that data
     *           is unlocked before the buffer fills up or overruns may occur.
     */
    virtual AudioResult lock_abi(size_t request,
                                 OMNI_ATTR("*in, out") const void** region,
                                 OMNI_ATTR("out") size_t* received) noexcept = 0;

    /** Unlocks a previously locked region of the buffer.
     *
     *  @param[in] consumed The number of frames in the previous buffer that were consumed.
     *                      Any frames that were not consumed will be returned in future
     *                      @ref ICaptureStream_abi::lock_abi() calls.
     *                      0 is valid here if you want to have the same locked region
     *                      returned on the next lock() call.
     *
     *  @returns @ref carb::audio::AudioResult::eOk if the region is successfully unlocked.
     *  @returns @ref carb::audio::AudioResult::eOutOfRange if @p consumed was larger than
     *           the locked region.
     *  @returns @ref carb::audio::AudioResult::eNotAllowed if no region is currently locked.
     *  @returns an carb::audio::AudioResult::e* error code if the region could not be unlocked.
     *
     *  @remarks This unlocks a region of the buffer that was locked with a
     *           previous call to ICaptureStream_abi::lock_abi().
     *           Now that this region is unlocked, the device can start writing to it,
     *           so the caller should no longer access that region.
     *           Once the buffer is unlocked, a new region may be locked with ICaptureStream_abi::lock_abi().
     *
     *  @note If the locked region is not fully unlocked, the part of the region that
     *        was not unlocked will be returned on the next call to ICaptureStream_abi::lock_abi().
     *        A second call to unlock cannot be made in this situation, it will fail.
     */
    virtual AudioResult unlock_abi(size_t consumed) noexcept = 0;

    /** Retrieve the size of the capture buffer.
     *
     *  @returns The size of the capture buffer, in frames.
     *           You can use @ref ICaptureStream_abi::getSoundFormat_abi() and @ref carb::audio::convertUnits()
     *           to convert to bytes or other units.
     *
     *  @remarks If your code is dependent on the buffer's actual size, it is
     *           better to retrieve it with this function since the buffer size
     *           used with device creation can be adjusted.
     */
    virtual size_t getBufferSize_abi() noexcept = 0;

    /** Retrieve the number of fragments used in this stream.
     *
     *  @returns The number of buffer fragments.
     *           This is the number of regions the recording buffer is divided
     *           into.
     */
    virtual size_t getFragmentCount_abi() noexcept = 0;

    /** Retrieve the format of the audio being captured.
     *
     *  @param[out] format The format of the audio being captured.
     *                     This may not be `nullptr`.
     */
    virtual void getSoundFormat_abi(OMNI_ATTR("out") SoundFormat* format) noexcept = 0;

    /** Clear any data that is currently in the recording buffer.
     *
     *  @returns @ref carb::audio::AudioResult::eOk if the buffer was successfully cleared.
     *  @returns an carb::audio::AudioResult::e* error code if the buffer could not be cleared.
     *
     *  @remarks This is a quick way to get rid of any data that is left in the
     *           buffer.
     *           This will also reset the write position on the buffer back to
     *           0, so the next lock call will return the start of the buffer
     *           (this can be useful if you want to do a one shot capture of the
     *           entire buffer).
     *
     *  @note If this is called while the capture stream is recording, the stream
     *        will be paused before it is reset.
     */
    virtual AudioResult reset_abi() noexcept = 0;
};

/** Low-Level Audio Capture Plugin Interface.
 *
 *  See these pages for more detail:
 *  @rst
    * :ref:`carbonite-audio-label`
    * :ref:`carbonite-audio-capture-label`
    @endrst
 */
class IAudioCapture
{
public:
    CARB_PLUGIN_INTERFACE("omni::audio::IAudioCapture", 0, 0)

    /** ABI version of createStream()
     *
     *  @param[in]  desc   A descriptor of the settings for the stream.
     *  @param[out] stream The raw stream object.
     *
     *  @returns @ref carb::audio::AudioResult::eOk on success. @see createStream() for failure codes.
     */
    AudioResult (*internalCreateStream)(const CaptureStreamDesc* desc, ICaptureStream** stream);

    /** Creates a new audio capture stream.
     *
     *  @param[in] desc A descriptor of the settings for the stream.
     *                  This may be nullptr to create a context that uses the
     *                  default capture device in its preferred format.
     *                  The device's format information may later be retrieved
     *                  with getSoundFormat().
     *  @param[out] stream The capture stream that is being created.
     *
     *  @returns @ref carb::audio::AudioResult::eOk if the stream was successfully created.
     *  @returns @ref carb::audio::AudioResult::eInvalidParameter if @p desc is `nullptr`.
     *  @returns @ref carb::audio::AudioResult::eOutOfMemory if the stream's allocation failed.
     *  @returns @ref carb::audio::AudioResult::eOutOfRange if the device's index does not
     *           correspond to a device that exists.
     *  @returns @ref carb::audio::AudioResult::eDeviceLost if the audio device selected fails
     *           to open.
     */
    inline AudioResult createStream(const CaptureStreamDesc* desc, omni::core::ObjectPtr<ICaptureStream>* stream)
    {
        if (stream == nullptr)
        {
            OMNI_LOG_ERROR("stream was null");
            return carb::audio::AudioResult::eInvalidParameter;
        }

        ICaptureStream* raw = nullptr;
        AudioResult res = internalCreateStream(desc, &raw);
        *stream = omni::core::steal(raw);
        return res;
    }
};

} // namespace experimental
} // namespace audio
} // namespace omni

#define OMNI_BIND_INCLUDE_INTERFACE_DECL
#include "IAudioCapture.gen.h"

/** @copydoc omni::audio::experimental::ICaptureStream_abi */
class omni::audio::experimental::ICaptureStream
    : public omni::core::Generated<omni::audio::experimental::ICaptureStream_abi>
{
};

#define OMNI_BIND_INCLUDE_INTERFACE_IMPL
#include "IAudioCapture.gen.h"
