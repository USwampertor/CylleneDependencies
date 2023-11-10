// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief The audio playback interface.
 */
#pragma once

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#    define _USE_MATH_DEFINES
#endif

#include "../Interface.h"
#include "AudioTypes.h"
#include "IAudioData.h"

#include <math.h>


namespace carb
{

/** Audio playback and capture. */
namespace audio
{

/******************************** typedefs, enums, & macros **************************************/
/** specifies the relative direction to a single speaker.  This is effectively a 3-space vector,
 *  but is explicitly described here to avoid any confusion between various common coordinate
 *  systems (ie: graphics systems take Z as pointing in the direction of the camera whereas
 *  audio systems take Z as pointing up).
 *
 *  This direction vector is always taken to be relative to the [real biological] user's expected
 *  position within the [physical] speaker setup where the origin is the user's position and the
 *  coordinates in each direction range from -1.0 to 1.0.  The provided vector does not need to
 *  be normalized.
 */
struct SpeakerDirection
{
    /** the left-to-right coordinate.  A value of 0.0 is in line with the user's gaze.  A value
     *  of -1.0 puts the speaker fully on the user's left side.  A value of 1.0 puts the speaker
     *  fully on the user's right side.
     */
    float leftRight = 0.0f;

    /** the front-to-back coordinate.  A value of 0.0 is in line with the user's ears.  A value
     *  of -1.0 puts the speaker fully behind the user.  A value of 1.0 puts the speaker fully
     *  in front of the user.
     */
    float frontBack = 0.0f;

    /** the top-to-bottom coordinate.  A value of 0.0 is on the plane formed from the user's ears
     *  and gaze.  A value of -1.0 puts the speaker fully below the user.  A value of 1.0 puts the
     *  speaker fully above the user.
     */
    float topBottom = 0.0f;
};

/** flags to affect the behavior of a speaker direction descriptor when it is passed to
 *  setSpeakerDirections().
 */
typedef uint32_t SpeakerDirectionFlags;

/** a descriptor of the direction from the [real biological] user to all speakers in the user's
 *  sound setup.  This is specified by the direction vector from the user's position (assumed
 *  to be at the origin) to each speaker.  This speaker direction set given here must always
 *  match the speaker mode for the current device and the user's speaker layout, otherwise the
 *  output results will be undefined.  These speaker directions are used to calculate all
 *  spatial sounds for a context so that they map as accurately as possible to the user's
 *  specific speaker setup.
 */
struct SpeakerDirectionDesc
{
    /** flags to control the behavior of the speaker positioning operation.  No flags are
     *  currently defined.  This must be set to 0.
     */
    SpeakerDirectionFlags flags = 0;

    /** the audio engine's output speaker mode that was selected at either context creation time
     *  or device opening time.  If the speaker mode was set to @ref kSpeakerModeDefault at
     *  context creation time, this will be set to the channel mask of the device that was
     *  selected.  Each set bit in this mask must have a corresponding vector set in the
     *  @ref directions table.  All other vectors will be ignored.  This channel mask can be
     *  retrieved from getContextCaps() for the currently selected device.  Note that selecting
     *  a new device with setOutput() will always reset the channel mask back to the defaults
     *  for the new device.
     */
    SpeakerMode speakerMode;

    /** the direction vectors from the user to each speaker.  This does not need to be normalized.
     *  Note that when this information is retrieved with getContextCaps(), the speaker direction
     *  vectors may not match the original values that were set since they may have already been
     *  normalized internally.  Each entry in this table is indexed by a @ref Speaker name.  For
     *  speaker modes that have more than @ref Speaker::eCount speakers, the specific mapping and
     *  positioning for each speaker is left entirely up to the caller (ie: a set of custom
     *  speaker directions must be specified for each channel for all contexts).  Each valid
     *  vector in this set, except for @ref Speaker::eLowFrequencyEffect, should have a non-zero
     *  direction.  The LFE speaker is always assumed to be located at the origin.
     */
    SpeakerDirection directions[kMaxChannels];

    /** provides an extended information block.  This is reserved for future expansion and should
     *  be set to nullptr.
     */
    void* ext = nullptr;
};

/** names for the state of a stream used in the Streamer object.  These are returned from the
 *  writeData() function to indicate to the audio processing engine whether the processing rate
 *  needs to temporarily speed up or slow down (or remain the same).  For non-real-time contexts,
 *  this would effectively always be requesting more data after each call.  For real-time
 *  contexts however, if the streamer is writing the audio data to a custom device, these state
 *  values are used to prevent the device from being starved or overrun.
 */
enum class StreamState
{
    /** indicates that the streamer is content with the current data production rate and that
     *  this rate should be maintained.
     */
    eNormal,

    /** indicates that the streamer is dangerously close to starving for data.  The audio
     *  processing engine should immediately run another cycle and produce more data.
     */
    eCritical,

    /** indicates that the streamer is getting close to starving for data and the audio processing
     *  engine should temporarily increase its data production rate.
     */
    eMore,

    /** indicates that the streamer is getting close to overrunning and the audio processing
     *  engine should temporarily decrease its data production rate.
     */
    eLess,

    /** indicates that the streamer is dangerously close to overrunning its buffer or device.
     *  The audio processing engine should decrease its data production rate.
     */
    eMuchLess,
};

/** interface for a streamer object.  This gives the audio context a way to write audio data
 *  to a generic interface instead of writing the data to an actual audio device.  This
 *  interface is used by specifying a streamer object in the @ref OutputDesc struct when creating
 *  the context or when calling setOutput().  Note that this interface is intended to implement
 *  a stateful object with each instance.  Two different instances of this interface should
 *  be able to operate independently from each other.
 *
 *  There is no requirement for how the incoming audio data is processed.  The streamer
 *  implementation may do whatever it sees fit with the incoming audio data.  Between the
 *  any two paired openStream() and closeStream() calls, each writeStreamData() call will
 *  be delivering sequential buffers in a single stream of audio data.  It is up to the
 *  streamer implementation to interpret that audio data and do something useful with it.
 *
 *  Since the internal usage of this streamer object is inherently asynchronous, all streamer
 *  objects must be atomically reference counted.  When a streamer is set on an audio context,
 *  a reference will be taken on it.  This reference will be released when the streamer is
 *  either removed (ie: a new output target is set for the context), or the context is destroyed.
 *
 *  @note This interface must be implemented by the host application.  No default implementation
 *        exists.  Some example streamer objects can be found in 'carb/audio/Streamers.h'.
 */
struct Streamer
{
    /** acquires a single reference to a Streamer object.
     *
     *  @param[in] self     the object to take the reference of.
     *  @returns no return value.
     *
     *  @remarks This acquires a new reference to a Streamer object.  The reference must be
     *           released later with releaseReference() when it is no longer needed.  Each
     *           call to acquireReference() on an object must be balanced with a call to
     *           releaseReference().  When a new streamer object is created, it should be
     *           given a reference count of 1.
     */
    void(CARB_ABI* acquireReference)(Streamer* self);

    /** releases a single reference to a Streamer object.
     *
     *  @param[in] self     the object to release a reference to.  The object may be destroyed
     *                      if it was the last reference that was released.  The caller should
     *                      consider the object invalid upon return unless it is known that
     *                      additional local references still exist on it.
     *  @returns no return value.
     *
     *  @remarks This releases a single reference to a Streamer object.  If the reference count
     *           reaches zero, the object will be destroyed.
     */
    void(CARB_ABI* releaseReference)(Streamer* self);

    /** sets the suggested format for the stream output.
     *
     *  @param[in] self         the streamer object to open the stream for.  This will not be
     *                          nullptr.
     *  @param[inout] format    on input, this contains the suggested data format for the stream.
     *                          On output, this contains the accepted data format.  The streamer
     *                          may make some changes to the data format including the data type,
     *                          sample rate, and channel count.  It is strongly suggested that the
     *                          input format be accepted since that will result in the least
     *                          amount of processing overhead.  The @a format, @a channels,
     *                          @a frameRate, and @a bitsPerSample members must be valid upon
     *                          return.  If the streamer changes the data format, only PCM data
     *                          formats are acceptable.
     *  @returns true if the data format is accepted by the streamer.
     *  @returns false if the streamer can neither handle the requested format nor
     *           can it change the requested format to something it likes.
     *
     *  @remarks This sets the data format that the streamer will receive its data in.  The
     *           streamer may change the data format to another valid PCM data format if needed.
     *           Note that if the streamer returns a data format that cannot be converted to by
     *           the processing engine, the initialization of the output will fail.  Also note
     *           that if the streamer changes the data format, this will incur a small performance
     *           penalty to convert the data to the new format.
     *
     *  @remarks This will be called when the audio context is first created.  Once the format
     *           is accepted by both the audio context and the streamer, it will remain constant
     *           as long as the processing engine is still running on that context.  When the
     *           engine is stopped (or the context is destroyed), a Streamer::close() call will
     *           be performed signaling the end of the stream.  If the engine is restarted again,
     *           another open() call will be performed to signal the start of a new stream.
     */
    bool(CARB_ABI* openStream)(Streamer* self, SoundFormat* format);

    /** writes a buffer of data to the stream.
     *
     *  @param[in] self     the streamer object to write a buffer of data into.  This will not
     *                      be nullptr.
     *  @param[in] data     the audio data being written to the streamer.  This data will be in
     *                      the format that was decided on in the call to open() during the
     *                      context creation or the last call to setOutput().  This buffer will
     *                      not persist upon return.  The implementation must copy the contents
     *                      of the buffer if it still needs to access the data later.
     *  @param[in] bytes    the number of bytes of valid data in the buffer @p data.
     *  @retval StreamState::eNormal if the data was written successfully to the streamer
     *           and the data production rate should continue at the current rate.
     *  @retval StreamState::eMore if the data was written successfully to the streamer and
     *           the data production rate should be temporarily increased.
     *  @retval StreamState::eLess if the data was written successfully to the streamer and
     *           the data production rate should be temporarily reduced.
     *
     *  @remarks This writes a buffer of data to the streamer.  The streamer is responsible for
     *           doing something useful with the audio data (ie: write it to a file, write it to
     *           a memory buffer, stream it to another voice, etc).  The caller of this function
     *           is not interested in whether the streamer successfully does something with the
     *           data - it is always assumed that the operation is successful.
     *
     *  @note This must execute as quickly as possible.  If this call takes too long to return
     *        and the output is going to a real audio device (through the streamer or some other
     *        means), an audible audio dropout could occur.  If the audio context is executing
     *        in non-realtime mode (ie: baking audio data), this may take as long as it needs
     *        only at the expense of making the overall baking process take longer.
     */
    StreamState(CARB_ABI* writeStreamData)(Streamer* self, const void* data, size_t bytes);

    /** closes the stream.
     *
     *  @param[in] self     the streamer object to close the stream for.  This will not be
     *                      nullptr.
     *  @returns no return value.
     *
     *  @remarks This signals that a stream has been finished.  This occurs when the engine is
     *           stopped or the audio context is destroyed.  No more calls to writeData() should
     *           be expected until the streamer is opened again.
     */
    void(CARB_ABI* closeStream)(Streamer* self);
};

/** base type for the flags that control the output of an audio context. */
typedef uint32_t OutputFlags;

/** flag to indicate that the output should target a real audio device.  When set, this indicates
 *  that the @ref OutputDesc::deviceIndex value will be valid and that it identifies the index of
 *  the audio device that should be opened.
 */
constexpr OutputFlags fOutputFlagDevice = 0x00000001;

/** flag to indicate that the output should target one or more generic streamer objects.  These
 *  objects are provided by the host app in the @ref OutputDesc::streamer value.  The stream will
 *  be opened when the audio processing engine is started and closed when the engine is stopped
 *  or the context is destroyed.
 */
constexpr OutputFlags fOutputFlagStreamer = 0x00000002;

/** flag to indicate that an empty streamer table is allowed.  This can be used
 *  to provide a way to set a null output in cases where audio output is not
 *  wanted, but audio engine behavior, such as voice callbacks, are still desired.
 *  This is also useful for testing, since you won't be dependent on the test
 *  systems having a physical audio device.
 */
constexpr OutputFlags fOutputFlagAllowNoStreamers = 0x00000004;

/** mask of output flag bits that are available for public use.  Clear bits in this mask are
 *  used internally and should not be referenced in other flags here.  This is not valid as a
 *  output flag or flag set.
 */
constexpr OutputFlags fOutputFlagAvailableMask = 0xffffffff;

/** descriptor of the audio output target to use for an audio context.  This may be specified
 *  both when creating an audio context and when calling setOutput().  An output may consist
 *  of a real audio device or one or more streamers.
 */
struct OutputDesc
{
    /** flags to indicate which output target is to be used.  Currently only a single output
     *  target may be specified.  This must be one of the fOutputFlag* flags.  Future versions
     *  may allow for multiple simultaneous outputs.  If this is 0, the default output device
     *  will be selected.  If more than one flag is specified, the audio device index will
     *  specify which device to use, and the streamer table will specify the set of streamers
     *  to attach to the output.
     */
    OutputFlags flags = 0;

    /** the index of the device to open.  This must be greater than or equal to 0 and less than
     *  the most recent return value of getDeviceCount().  Set this to 0 to choose the system's
     *  default playback device.  This defaults to 0.  This value is always ignored if the
     *  @ref fOutputFlagDevice flag is not used.
     */
    size_t deviceIndex = 0;

    /** the output speaker mode to set.  This is one of the SpeakerMode names or a combination of
     *  the fSpeakerFlag* speaker flags.  This will determine the channel layout for the final
     *  output of the audio engine.  This channel layout will be mapped to the selected device's
     *  speaker mode before sending the final output to the device.  This may be set to
     *  @ref kSpeakerModeDefault to cause the engine's output to match the output of the device
     *  that is eventually opened.  This defaults to @ref kSpeakerModeDefault.
     */
    SpeakerMode speakerMode = kSpeakerModeDefault;

    /** the frame rate in Hertz to run the processing engine at.  This should be 0 for any output
     *  that targets a real hardware device (ie: the @ref fOutputFlagDevice is used in @ref flags
     *  or @ref flags is 0),  It is possible to specify a non-zero value here when a hardware
     *  device is targeted, but that may lead to unexpected results depending on the value given.
     *  For example, this could be set to 1000Hz, but the device could run at 48000Hz.  This
     *  would process the audio data properly, but it would sound awful since the processing
     *  data rate is so low.  Conversely, if a frame rate of 200000Hz is specified but the device
     *  is running at 48000Hz, a lot of CPU processing time will be wasted.
     *
     *  When an output is targeting only a streamer table, this should be set to the frame rate
     *  to process audio at.  If this is 0, a frame rate of 48000Hz will be used.  This value
     *  should generally be a common audio processing rate such as 44100Hz, 48000Hz, etc.  Other
     *  frame rates can be used, but the results may be unexpected if a frame cannot be perfectly
     *  aligned to the cycle period.
     */
    size_t frameRate = 0;

    /** the number of streams objects in the @ref streamer table.  This value is ignored if the
     *  @ref fOutputFlagStreamer flag is not used.
     */
    size_t streamerCount = 0;

    /** a table of one or more streamer objects to send the final output to.  The number of
     *  streamers in the table is specified in @ref streamerCount.  These streamer objects must
     *  be both implemented and instantiated by the caller.  The streamers will be opened when
     *  the new output target is first setup.  All streamers in the table will receive the same
     *  input data and format settings when opened.  This value is ignored if the
     *  @ref fOutputFlagStreamer flag is not used.  Each object in this table will be opened
     *  when the audio processing engine is started on the context.  Each streamer will receive
     *  the same data in the same format.  Each streamer will be closed when the audio processing
     *  engine is stopped or the context is destroyed.
     *
     *  A reference to each streamer will be acquired when they are assigned as an output.  These
     *  references will all be released when they are no longer active as outputs on the context
     *  (ie: replaced as outputs or the context is destroyed).  No entries in this table may be
     *  nullptr.
     *
     *  @note If more than one streamer is specified here, It is the caller's responsibility to
     *        ensure all of them will behave well together.  This means that none of them should
     *        take a substantial amount of time to execute and that they should all return as
     *        consistent a stream state as possible from writeStreamData().  Furthermore, if
     *        multiple streamers are targeting real audio devices, their implementations should
     *        be able to potentially be able to handle large amounts of latency (ie: up to 100ms)
     *        since their individual stream state returns may conflict with each other and affect
     *        each other's timings.  Ideally, no more than one streamer should target a real
     *        audio device to avoid this stream state build-up behavior.
     */
    Streamer** streamer = nullptr;

    /** extended information for this descriptor.  This is reserved for future expansion and
     *  should be set to nullptr.
     */
    void* ext = nullptr;
};


/** names for events that get passed to a context callback function. */
enum class ContextCallbackEvent
{
    /** an audio device in the system has changed its state.  This could mean that one or more
     *  devices were either added to or removed from the system.  Since device change events
     *  are inherently asynchronous and multiple events can occur simultaneously, this callback
     *  does not provide any additional information except that the event itself occurred.  It
     *  is the host app's responsibility to enumerate devices again and decide what to do with
     *  the event.  This event will only occur for output audio devices in the system.  With
     *  this event, the @a data parameter to the callback will be nullptr.  This callback will
     *  always be performed during the context's update() call.
     */
    eDeviceChange,

    /** the audio engine is about to start a processing cycle.  This will occur in the audio
     *  engine thread.  This is given as an opportunity to update any voice parameters in a
     *  manner that can always be completed immediately and will be synchronized with other
     *  voice parameter changes made at the same time.  Since this occurs in the engine thread,
     *  it is the caller's responsibility to ensure this callback returns as quickly as possible
     *  so that it does not interrupt the audio processing timing.  If this callback takes too
     *  long to execute, it will cause audio drop-outs.  The callback's @a data parameter will
     *  be set to nullptr for this event.
     */
    eCycleStart,

    /** the audio engine has just finished a processing cycle.  This will occur in the audio
     *  engine thread.  Voice operations performed in here will take effect immediately.  Since
     *  this occurs in the engine thread, it is the caller's responsibility to ensure this
     *  callback returns as quickly as possible so that it does not interrupt the audio processing
     *  timing.  If this callback takes too long to execute, it will cause audio drop-outs.  The
     *  callback's @a data parameter will be set to nullptr for this event.
     */
    eCycleEnd,
};


/** prototype for a context callback event function.
 *
 *  @param[in] context  the context object that triggered the callback.
 *  @param[in] event    the event that occurred on the context.
 *  @param[in] data     provides some additional information for the event that occurred.  The
 *                      value and content of this object depends on the event that occurred.
 *  @param[in] userData the user data value that was registered with the callback when the
 *                      context object was created.
 *  @returns no return value.
 */
typedef void(CARB_ABI* ContextCallback)(Context* context, ContextCallbackEvent event, void* data, void* userData);


/** flags to control the behavior of context creation.  No flags are currently defined. */
typedef uint32_t PlaybackContextFlags;

/** flag to indicate that the audio processing cycle start and end callback events should be
 *  performed.  By default, these callbacks are not enabled unless a baking context is created.
 *  This flag will be ignored if no callback is provided for the context.
 */
constexpr PlaybackContextFlags fContextFlagCycleCallbacks = 0x00000001;

/** flag to indicate that the audio processing engine should be run in 'baking' mode.  This will
 *  allow it to process audio data as fast as the system permits instead of waiting for a real
 *  audio device to catch up.  When this flag is used, it is expected that the context will also
 *  use a streamer for its output.  If a device index is selected as an output, context creation
 *  and setting a new output will still succeed, but the results will not sound correct due to
 *  the audio processing likely going much faster than the device can consume the data.  Note
 *  that using this flag will also imply the use of @ref fContextFlagCycleCallbacks if a
 *  callback function is provided for the context.
 *
 *  When this flag is used, the new context will be created with the engine in a stopped state.
 *  The engine must be explicitly started with startProcessing() once the audio graph has been
 *  setup and all initial voices created.  This can be combined with @ref fContextFlagManualStop
 *  to indicate that the engine will be explicitly stopped instead of stopping automatically once
 *  the engine goes idle (ie: no more voices are active).
 */
constexpr PlaybackContextFlags fContextFlagBaking = 0x00000002;

/** flag to indicate that the audio processing engine will be manually stopped when a baking
 *  task is complete instead of stopping it when all of the input voices run out of data to
 *  process.  This flag is ignored if the @ref fContextFlagBaking flag is not used.  If this
 *  flag is not used, the intention for the context is to queue an initial set of sounds to
 *  play on voices before starting the processing engine with startProcessing().  Once all
 *  triggered voices are complete, the engine will stop automatically.  When this flag is
 *  used, the intention is that the processing engine will stay running, producing silence,
 *  even if no voices are currently playing.  The caller will be expected to make a call to
 *  stopProcessing() at some point when the engine no longer needs to be running.
 */
constexpr PlaybackContextFlags fContextFlagManualStop = 0x00000004;

/** flag to enable callbacks of type ContextCallbackEvent::eDeviceChange.
 *  If this flag is not passed, no callbacks of this type will be performed for
 *  this context.
 */
constexpr PlaybackContextFlags fContextFlagDeviceChangeCallbacks = 0x00000008;

/** descriptor used to indicate the options passed to the createContext() function.  This
 *  determines the how the context will behave.
 */
struct PlaybackContextDesc
{
    /** flags to indicate some additional behavior of the context.  No flags are currently
     *  defined.  This should be set to 0.  In future versions, these flags may be used to
     *  determine how the @ref ext member is interpreted.
     */
    PlaybackContextFlags flags = 0;

    /** a callback function to be registered with the new context object.  This callback will be
     *  performed any time the output device list changes.  This notification will only indicate
     *  that a change has occurred, not which specific change occurred.  It is the caller's
     *  responsibility to re-enumerate devices to determine if any further action is necessary
     *  for the updated device list.  This may be nullptr if no device change notifications are
     *  needed.
     */
    ContextCallback callback = nullptr;

    /** an opaque context value to be passed to the callback whenever it is performed.  This value
     *  will never be accessed by the context object and will only be passed unmodified to the
     *  callback function.  This value is only used if a device change callback is provided.
     */
    void* callbackContext = nullptr;

    /** the maximum number of data buses to process simultaneously.  This is equivalent to the
     *  maximum number of potentially audible sounds that could possibly affect the output on the
     *  speakers.  If more voices than this are active, the quietest or furthest voice will be
     *  deactivated.  The default value for this is 64.  Set this to 0 to use the default for
     *  the platform.
     *
     *  For a baking context, this should be set to a value that is large enough to guarantee
     *  that no voices will become virtualized.  If a voice becomes virtualized in a baking
     *  context and it is set to simulate that voice's current position, the resulting position
     *  when it becomes a real voice again is unlikely to match the expected position.  This is
     *  because there is no fixed time base to simulate the voice on.  The voice will be simulated
     *  as if it were playing in real time.
     */
    size_t maxBuses = 0;

    /** descriptor for the output to choose for the new audio context.  This must specify at
     *  least one output target.
     */
    OutputDesc output = {};

    /** A name to use for any audio device connection that requires one.
     *  This can be set to nullptr to use a generic default name, if necessary.
     *  This is useful in situations where a platform, such as Pulse Audio,
     *  will display a name to the user for each individual audio connection.
     *  This string is copied internally, so the memory can be discarded after
     *  calling createContext().
     */
    const char* outputDisplayName = nullptr;

    /** extended information for this descriptor.  This is reserved for future expansion and
     *  should be set to nullptr.
     */
    void* ext = nullptr;
};

/** the capabilities of the context object.  Some of these values are set at the creation time
 *  of the context object.  Others are updated when speaker positions are set or an output
 *  device is opened.  This is only used to retrieve the capabilities of the context.
 */
struct ContextCaps
{
    /** the maximum number of data buses to process simultaneously.  This is equivalent to the
     *  maximum number of potentially audible sounds that could possibly affect the output on the
     *  speakers.  If more voices than this are active, the quietest or furthest voice will be
     *  deactivated.  This value is set at creation time of the context object.  This can be
     *  changed after creation with setBusCount().
     */
    size_t maxBuses;

    /** the directions of each speaker.  Only the speaker directions for the current speaker
     *  mode will be valid.  All other speaker directions will be set to (0, 0, 0).  This table
     *  can be indexed using the Speaker::* names.  When a new context is first created or when
     *  a new device is selected with setOutput(), the speaker layout will always be set to
     *  the default for that device.  The host app must set a new speaker direction layout
     *  if needed after selecting a new device.
     */
    SpeakerDirectionDesc speakerDirections;

    /** the info for the connection to the currently selected device.
     *  If no device is selected, the @a flags member will be set to @ref
     *  fDeviceFlagNotOpen.  If a device is selected, its information will be
     *  set in here, including its preferred output format.
     *  Note that the format information may differ from the information
     *  returned by getDeviceDetails() as this is the format that the audio is
     *  being processed at before being sent to the device.
     */
    DeviceCaps selectedDevice;

    /** the output target that is currently in use for the context.  This will provide access
     *  to any streamer objects in use or the index of the currently active output device.  The
     *  device index stored in here will match the one in @ref selectedDevice if a real audio
     *  device is in use.
     */
    OutputDesc output;

    /** reserved for future expansion.  This must be nullptr. */
    void* ext;
};


/** flags that indicate how listener and voice entity objects should be treated and which
 *  values in the @ref EntityAttributes object are valid.  This may be 0 or any combination
 *  of the fEntityFlag* flags.
 *  @{
 */
typedef uint32_t EntityAttributeFlags;

/** flags for listener and voice entity attributes.  These indicate which entity attributes
 *  are valid or will be updated, and indicate what processing still needs to be done on them.
 */
/** the @ref EntityAttributes::position vector is valid. */
constexpr EntityAttributeFlags fEntityFlagPosition = 0x00000001;

/** the @ref EntityAttributes::velocity vector is valid. */
constexpr EntityAttributeFlags fEntityFlagVelocity = 0x00000002;

/** the @ref EntityAttributes::forward vector is valid. */
constexpr EntityAttributeFlags fEntityFlagForward = 0x00000004;

/** the @ref EntityAttributes::up vector is valid. */
constexpr EntityAttributeFlags fEntityFlagUp = 0x00000008;

/** the @ref EntityAttributes::cone values are valid. */
constexpr EntityAttributeFlags fEntityFlagCone = 0x00000010;

/** the @ref EmitterAttributes::rolloff values are valid. */
constexpr EntityAttributeFlags fEntityFlagRolloff = 0x00000020;

/** all vectors in the entity information block are valid. */
constexpr EntityAttributeFlags fEntityFlagAll = fEntityFlagPosition | fEntityFlagVelocity | fEntityFlagForward |
                                                fEntityFlagUp | fEntityFlagCone | fEntityFlagRolloff;

/** when set, this flag indicates that setListenerAttributes() or setEmitterAttributes()
 *  should make the @a forward and @a up vectors perpendicular and normalized before setting
 *  them as active.  This flag should only be used when it is known that the vectors are not
 *  already perpendicular.
 */
constexpr EntityAttributeFlags fEntityFlagMakePerp = 0x80000000;

/** when set, this flag indicates that setListenerAttributes() or setEmitterAttributes()
 *  should normalize the @a forward and @a up vectors before setting them as active.  This
 *  flag should only be used when it is known that the vectors are not already normalized.
 */
constexpr EntityAttributeFlags fEntityFlagNormalize = 0x40000000;
/** @} */


/** distance based DSP value rolloff curve types.  These represent the formula that will be
 *  used to calculate the spatial DSP values given the distance between an emitter and the
 *  listener.  This represents the default attenuation calculation that will be performed
 *  for all curves that are not overridden with custom curves.  The default rolloff model
 *  will follow the inverse square law (ie: @ref RolloffType::eInverse).
 */
enum class RolloffType
{
    /** defines an inverse attenuation curve where a distance at the 'near' distance is full
     *  volume and at the 'far' distance gives silence.  This is calculated with a formula
     *  proportional to (1 / distance).
     */
    eInverse,

    /** defines a linear attenuation curve where a distance at the 'near' distance is full
     *  volume and at the 'far' distance gives silence.  This is calculated with
     *  (max - distance) / (max - min).
     */
    eLinear,

    /** defines a linear square attenuation curve where a distance at the 'near' distance is
     *  full volume and at the 'far' distance gives silence.  This is calculated with
     *  ((max - distance) / (max - min)) ^ 2.
     */
    eLinearSquare,
};

/** defines the point-wise curve that is used for specifying custom rolloff curves.
 *  This curve is defined as a set of points that are treated as a piece-wise curve where each
 *  point's X value represents the listener's normalized distance from the emitter, and the Y
 *  value represents the DSP value at that distance.  A linear interpolation is done between
 *  points on the curve.  Any distances beyond the X value on the last point on the curve
 *  simply use the DSP value for the last point.  Similarly, and distances closer than the
 *  first point simply use the first point's DSP value.  This curve should always be normalized
 *  such that the first point is at a distance of 0.0 (maps to the emitter's 'near' distance)
 *  and the last point is at a distance of 1.0 (maps to the emitter's 'far' distance).  The
 *  interpretation of the specific DSP values at each point depends on the particular usage
 *  of the curve.  If used, this must contain at least one point.
 */
struct RolloffCurve
{
    /** the total number of points on the curve.  This is the total number of entries in the
     *  @ref points table.  This must be greater than zero.
     */
    size_t pointCount;

    /** the table of data points in the curve.  Each point's X value represents a normalized
     *  distance from the emitter to the listener.  Each point's Y value is the corresponding
     *  DSP value to be interpolated at that distance.  The points in this table must be sorted
     *  in increasing order of distance.  No two points on the curve may have the same distance
     *  value.  Failure to meet these requirements will result in undefined behavior.
     */
    Float2* points;
};

/** descriptor of the rolloff mode, range, and curves to use for an emitter. */
struct RolloffDesc
{
    /** the default type of rolloff calculation to use for all DSP values that are not overridden
     *  by a custom curve.  This defaults to @ref RolloffType::eLinear.
     */
    RolloffType type;

    /** the near distance range for the sound.  This is specified in arbitrary world units.
     *  When a custom curve is used, this near distance will map to a distance of 0.0 on the
     *  curve.  This must be less than the @ref farDistance distance.  The near distance is the
     *  closest distance that the emitter's attributes start to rolloff at.  At distances
     *  closer than this value, the calculated DSP values will always be the same as if they
     *  were at the near distance.  This defaults to 0.0.
     */
    float nearDistance;

    /** the far distance range for the sound.  This is specified in arbitrary world units.  When
     *  a custom curve is used, this far distance will map to a distance of 1.0 on the curve.
     *  This must be greater than the @ref nearDistance distance.  The far distance is the
     *  furthest distance that the emitters attributes will rolloff at.  At distances further
     *  than this value, the calculated DSP values will always be the same as if they were at
     *  the far distance (usually silence).  Emitters further than this distance will often
     *  become inactive in the scene since they cannot be heard any more.  This defaults to
     *  10000.0.
     */
    float farDistance;

    /** the custom curve used to calculate volume attenuation over distance.  This must be a
     *  normalized curve such that a distance of 0.0 maps to the @ref nearDistance distance
     *  and a distance of 1.0 maps to the @ref farDistance distance.  When specified, this
     *  overrides the rolloff calculation specified by @ref type when calculating volume
     *  attenuation.  This defaults to nullptr.
     */
    RolloffCurve* volume;

    /** the custom curve used to calculate low frequency effect volume over distance.  This
     *  must be a normalized curve such that a distance of 0.0 maps to the @ref nearDistance
     *  distance and a distance of 1.0 maps to the @ref farDistance distance.  When specified,
     *  this overrides the rolloff calculation specified by @ref type when calculating the low
     *  frequency effect volume.  This defaults to nullptr.
     */
    RolloffCurve* lowFrequency;

    /** the custom curve used to calculate low pass filter parameter on the direct path over
     *  distance.  This must be a normalized curve such that a distance of 0.0 maps to the
     *  @ref nearDistance distance and a distance of 1.0 maps to the @ref farDistance distance.
     *  When specified, this overrides the rolloff calculation specified by @ref type when
     *  calculating the low pass filter parameter.  This defaults to nullptr.
     */
    RolloffCurve* lowPassDirect;

    /** the custom curve used to calculate low pass filter parameter on the reverb path over
     *  distance.  This must be a normalized curve such that a distance of 0.0 maps to the
     *  @ref nearDistance distance and a distance of 1.0 maps to the @ref farDistance distance.
     *  When specified, this overrides the rolloff calculation specified by @ref type when
     *  calculating the low pass filter parameter.  This defaults to nullptr.
     */
    RolloffCurve* lowPassReverb;

    /** the custom curve used to calculate reverb mix level over distance.  This must be a
     *  normalized curve such that a distance of 0.0 maps to the @ref nearDistance distance and
     *  a distance of 1.0 maps to the @ref farDistance distance.  When specified, this overrides
     *  the rolloff calculation specified by @ref type when calculating the low pass filter
     *  parameter.  This defaults to nullptr.
     */
    RolloffCurve* reverb;

    /** reserved for future expansion.  This must be set to nullptr. */
    void* ext = nullptr;
};

/** specifies a pair of values that define a DSP value range to be interpolated between based
 *  on an emitter-listener angle that is between a cone's inner and outer angles.  For angles
 *  that are smaller than the cone's inner angle, the 'inner' DSP value will always be used.
 *  Similarly, for angles that are larger than the cone's outer angle, the 'outer' DSP value
 *  will always be used.  Interpolation will only occur when the emitter-listener angle is
 *  between the cone's inner and outer angles.  No specific meaning or value range is attached
 *  to the 'inner' and 'outer' DSP values.  These come from the specific purpose that this
 *  object is used for.
 */
struct DspValuePair
{
    /** the DSP value to be used for angles less than the cone's inner angle and as one of
     *  the interpolation endpoints for angles between the cone's inner and outer angles.
     */
    float inner;

    /** the DSP value to be used for angles greater than the cone's outer angle and as one of
     *  the interpolation endpoints for angles between the cone's inner and outer angles.
     */
    float outer;
};

/** the angle to specify for @ref EntityCone::insideAngle and @ref EntityCone::outsideAngle
 *  in order to mark the cone as disabled.  This will treat the emitter or listener as
 *  omni-directional.
 */
constexpr float kConeAngleOmnidirectional = (float)(2.0 * M_PI);

/** defines a sound cone relative to an entity's front vector.  It is defined by two angles -
 *  the inner and outer angles.  When the angle between an emitter and the listener (relative
 *  to the entity's front vector) is smaller than the inner angle, the resulting DSP value
 *  will be the 'inner' value.  When the emitter-listener angle is larger than the outer
 *  angle, the resulting DSP value will be the 'outer' value.  For emitter-listener angles
 *  that are between the inner and outer angles, the DSP value will be interpolated between
 *  the inner and outer angles.  If a cone is valid for an entity, the @ref fEntityFlagCone
 *  flag should be set in @ref EntityAttributes::flags.
 *
 *  Note that a cone's effect on the spatial volume of a sound is purely related to the angle
 *  between the emitter and listener.  Any distance attenuation is handled separately.
 */
struct EntityCone
{
    /** the inside angle of the entity's sound cone in radians.  This describes the angle around
     *  the entity's forward vector within which the entity's DSP values will always use their
     *  'inner' values.  This angle will extend half way in all directions around the forward
     *  vector.  For example, a 30 degree (as converted to radians to store here) inside angle
     *  will extend 15 degrees in all directions around the forward vector.  Set this to
     *  @ref kConeAngleOmnidirectional to define an omni-directional entity.  This must be
     *  greater than or equal to 0 and less than or equal to @ref outsideAngle.
     */
    float insideAngle;

    /** the outside angle of the entity's sound cone in radians.  This describes the angle
     *  around the entity's forward vector up to which the volume will be interpolated.  When
     *  the emitter-listener angle is larger than this angle, the 'outer' DSP values will always
     *  be used.  This angle will extend half way in all directions around the forward vector.
     *  For example, a 30 degree (as converted to radians to store here) inside angle will extend
     *  15 degrees in all directions around the forward vector.  Set this to
     *  @ref kConeAngleOmnidirectional to define an omni-directional entity.  This must be greater
     *  than or equal to @ref insideAngle.
     */
    float outsideAngle;

    /** the volumes to use for emitter-listener lines that are inside and outside the entity's
     *  cone angles.  These will be used as the endpoint values to interpolate to for angles
     *  between the inner and outer angles, and for the values for all angles outside the cone's
     *  inner and outer angles.  These should be in the range 0.0 (silence) to 1.0 (full volume).
     */
    DspValuePair volume;

    /** the low pass filter parameter values to use for emitter-listener lines that are inside
     *  and outside the entity's cone angles.  These will be used as the endpoint values to
     *  interpolate to for angles between the inner and outer angles, and for the values for all
     *  angles outside the cone's inner and outer angles.  There is no specific range for these
     *  values other than what is commonly accepted for low pass filter parameters.
     *  This multiplies by member `direct` of @ref VoiceParams::occlusion, if
     *  that is set to anything other than 1.0.
     *  Setting this to a value outside of [0.0, 1.0] will result in an undefined low pass
     *  filter value being used.
     */
    DspValuePair lowPassFilter;

    /** the reverb mix level values to use for emitter-listener lines that are inside and outside
     *  the entity's cone angles.  These will be used as the endpoint values to interpolate to for
     *  angles between the inner and outer angles, and for the values for all angles outside the
     *  cone's inner and outer angles.  This should be in the range 0.0 (no reverb) to 1.0 (full
     *  reverb).
     */
    DspValuePair reverb;

    /** reserved for future expansion.  This must be nullptr. */
    void* ext = nullptr;
};

/** base spatial attributes of the entity.  This includes its position, orientation, and velocity
 *  and an optional cone.
 */
struct EntityAttributes
{
    /** a set of flags that indicate which of members of this struct are valid.  This may be
     *  fEntityFlagAll to indicate that all members contain valid data.
     */
    EntityAttributeFlags flags;

    /** the current position of the listener in world units.  This should only be expressed in
     *  meters if the world units scale is set to 1.0 for this context.  This value is ignored
     *  if the fEntityFlagPosition flag is not set in @ref flags.
     */
    Float3 position;

    /** the current velocity of the listener in world units per second.  This should only be
     *  expressed in meters per second if the world units scale is set to 1.0 with
     *  for the context.  The magnitude of this vector will be taken as the listener's
     *  current speed and the vector's direction will indicate the listener's current direction.
     *  This vector should not be normalized unless the listener's speed is actually 1.0 units
     *  per second.  This may be a zero vector if the listener is not moving.  This value is
     *  ignored if the fEntityFlagVelocity flag is not set in @ref flags.  This vector will
     *  not be modified in any way before use.
     */
    Float3 velocity;

    /** a vector indicating the direction the listener is currently facing.  This does not need
     *  to be normalized, but should be for simplicity.  If the fEntityFlagNormalize flag is
     *  set in @ref flags, this vector will be normalized internally before being used.  This
     *  vector should be perpendicular to the @ref up vector unless the fEntityFlagMakePerp
     *  flag is set in @ref flags.  This must not be a zero vector unless the fEntityFlagForward
     *  flag is not set in @ref flags.
     */
    Float3 forward;

    /** a vector indicating the upward direction for the listener.  This does not need to be
     *  normalized, but should be for simplicity.  If the fEntityFlagNormalize flag is set in
     *  @ref flags, this vector will be normalized internally before being used.  This vector
     *  should be perpendicular to the @ref forward vector unless the fEntityFlagMakePerp flag
     *  is set in @ref flags.  This must not be a zero vector unless the fEntityFlagUp flag is
     *  not set in @ref flags.
     */
    Float3 up;

    /** defines an optional sound cone for an entity.  The cone is a segment of a sphere around
     *  the entity's position opening up toward its front vector.  The cone is defined by an
     *  inner and outer angle, and several DSP values to be interpolated between for those two
     *  endpoint angles.  This cone is valid if the @ref fEntityFlagCone flag is set in
     *  @ref flags.
     */
    EntityCone cone;
};

/** spatial attributes for a listener entity. */
struct ListenerAttributes : EntityAttributes
{
    /** provides an extended information block.  This is reserved for future expansion and should
     *  be set to nullptr.  The values in @ref flags may be used to indicate how this value should
     *  be interpreted.
     */
    void* ext = nullptr;
};

/** spatial attributes of an emitter entity. */
struct EmitterAttributes : EntityAttributes
{
    /** a descriptor of the rolloff parameters for this emitter.  This is valid and accessed only
     *  if the @ref fEntityFlagRolloff flag is set in @ref EntityAttributes::flags.  The curves
     *  (if any) in this descriptor must remain valid for the entire time a voice is playing the
     *  sound represented by this emitter.
     */
    RolloffDesc rolloff;

    /** provides an extended information block.  This is reserved for future expansion and must
     *  be set to nullptr.  The values in @ref flags may be used to indicate how this value should
     *  be interpreted.
     */
    void* ext = nullptr;
};


/** voice callback reason names.  These identify what kind of callback is being performed.
 *  This can either be an event point or the end of the sound.
 */
enum class VoiceCallbackType
{
    /** the sound reached its end and has implicitly stopped.  This will not be hit for looping
     *  sounds until the last loop iteration completes and the sound stops playing.  In the
     *  voice callback for this type, the @a data parameter will be set to nullptr.
     */
    eSoundEnded,

    /** the engine has reached one of the playing sound's event points.  These are arbitrary
     *  points within the sound that need notification so that other objects in the simulation
     *  can react to the sound.  These event points are often chosen by the sound designers.
     *  In the voice callback for this type, the @a data parameter will be set to the event
     *  point object that was triggered.
     */
    eEventPoint,

    /** the engine has reached a loop point.  This callback occurs when the new loop iteration
     *  starts.
     *  This will only be triggered on voices that have the @ref fPlayFlagRealtimeCallbacks flag
     *  set.
     *  On streaming sounds, these callbacks are triggered when the buffer containing a loop end
     *  has finished playing, rather than when the loop end is decoded. This is not guaranteed to
     *  be called exactly on the ending frame of the loop.
     *  If an excessively short loop (e.g. 2ms) is put on a streaming sound,
     *  some loop callbacks may be skipped.
     */
    eLoopPoint,
};

/** prototype for a voice event callback function.
 *
 *  @param[in] voice    the voice object that produced the event callback.
 *  @param[in] callbackType the type of callback being performed.  This can either be that the
 *                          sound ended or that one of its event points was hit.
 *  @param[in] sound    the sound data object that the voice is playing or just finished playing.
 *  @param[in] data     some extra data specific to the callback type that occurred.  This will
 *                      be nullptr for @ref VoiceCallbackType::eSoundEnded callbacks.  This will
 *                      be the EventPoint information for the event point that was hit for
 *                      @ref VoiceCallbackType::eEventPoint callbacks.
 *  @param[in] context  the context value that was specified at the time the callback function was
 *                      registered with the voice.
 *  @return @ref AudioResult::eOk.  Returning any other value may result in the mixer being halted
 *           or the playback of the current sound stopping.
 *
 *  @remarks This callback allows a system to receive notifications of events occurring on a sound
 *           object that is playing on a voice.  This callback is performed when either the
 *           sound finishes playing or when one of its event points is hit.  Since this callback
 *           may be performed in the context of the mixer thread, it should take as little time as
 *           possible to return.  Instead of performing heavy calculations, it should simply set a
 *           flag that those calculations need to be handled on another worker thread at a later
 *           time.
 */
typedef AudioResult(CARB_ABI* VoiceCallback)(
    Voice* voice, VoiceCallbackType callbackType, SoundData* sound, void* data, void* context);


/** base type for the various playback mode flags.  The default state for all these flags
 *  is false (ie: all flags cleared).  These flags are set in @ref VoiceParams::playbackMode
 *  and are only valid when @ref fVoiceParamPlaybackMode, @ref fVoiceParamPause, or
 *  @ref fVoiceParamMute are used for the parameter block flags.
 *
 *  @{
 */
typedef uint32_t PlaybackModeFlags;

/** flag to indicate whether a sound should be played back as a spatial or non-spatial
 *  sound.  When false, the sound is played in non-spatial mode.  This means that only
 *  the current explicit volume level and pan values will affect how the sound is mapped
 *  to the output channels.  If true, all spatial positioning calculations will be
 *  performed for the sound and its current emitter attributes (ie: position, orientation,
 *  velocity) will affect how it is mapped to the output channels.
 */
constexpr PlaybackModeFlags fPlaybackModeSpatial = 0x00000001;

/** flag to indicate how the spatial attributes of an emitter are to be interpreted.
 *  When true, the emitter's current position, orientation, and velocity will be added
 *  to those of the listener object to get the world coordinates for each value.  The
 *  spatial calculations will be performed using these calculated world values instead.
 *  This is useful for orienting and positioning sounds that are 'attached' to the
 *  listener's world representation (ie: camera, player character, weapon, etc), but
 *  still move somewhat relative to the listener.  When not set, the emitter's spatial
 *  attributes will be assumed to be in world coordinates already and just used directly.
 */
constexpr PlaybackModeFlags fPlaybackModeListenerRelative = 0x00000002;

/** flag to indicate whether triggering this sound should be delayed to simulate its
 *  travel time to reach the listener.  The sound's trigger time is considered the time
 *  of the event that produces the sound at the emitter's current location.  For distances
 *  to the listener that are less than an imperceptible threshold (around 20ms difference
 *  between the travel times of light and sound), this value will be ignored and the sound
 *  will just be played immediately.  The actual distance for this threshold depends on
 *  the current speed of sound versus the speed of light.  For all simulation purposes, the
 *  speed of light is considered to be infinite (at ~299700km/s in air, there is no
 *  terrestrial environment that could contain a larger space than light can travel (or
 *  that scene be rendered) in even a fraction of a second).
 *
 *  Note that if the distance delay is being used to play a sound on a voice, that voice's
 *  current frequency ratio will not affect how long it takes for the delay period to expire.
 *  If the frequency ratio is being used to provide a time dilation effect on the sound rather
 *  than a pitch change or playback speed change, the initial distance delay period will seem
 *  to be different than expected because of this.  If a time dilation effect is needed, that
 *  should be done by changing the context's spatial sound frequency ratio instead.
 */
constexpr PlaybackModeFlags fPlaybackModeDistanceDelay = 0x00000004;

/** flag to indicate whether interaural time delay calculations should occur on this
 *  voice.  This will cause the left or right channel(s) of the voice to be delayed
 *  by a few frames to simulate the difference in time that it takes a sound to reach
 *  one ear versus the other.  These calculations will be performed using the current
 *  speed of sound for the context and a generalized acoustic model of the human head.
 *  This will be ignored for non-spatial sounds and sounds with fewer than two channels.
 *  This needs to be set when the voice is created for it to take effect.
 *  The performance cost of this effect is minimal when the interaural time
 *  delay is set to 0.
 *  For now, interaural time delay will only take effect when the @ref SoundData
 *  being played is 1 channel and the output device was opened with 2 channels.
 *  This effect adds a small level of distortion when an emitter's angle is changing
 *  relative to the listener; this is imperceptible in most audio, but it can be
 *  audible with pure sine waves.
 */
constexpr PlaybackModeFlags fPlaybackModeInterauralDelay = 0x00000008;

/** flag to indicate whether Doppler calculations should be performed on this sound.
 *  This is ignored for non-spatial sounds.  When true, the Doppler calculations will
 *  be automatically performed for this voice if it is moving relative to the listener.
 *  If neither the emitter nor the listener are moving, the Doppler shift will not
 *  have any affect on the sound.  When false, the Doppler calculations will be skipped
 *  regardless of the relative velocities of this emitter and the listener.
 */
constexpr PlaybackModeFlags fPlaybackModeUseDoppler = 0x00000010;

/** flag to indicate whether this sound is eligible to be sent to a reverb effect.  This
 *  is ignored for non-spatial sounds.  When true, the sound playing on this voice will
 *  be sent to a reverb effect if one is present in the current sound environment.  When
 *  false, the sound playing on this voice will bypass any reverb effects in the current
 *  sound environment.
 */
constexpr PlaybackModeFlags fPlaybackModeUseReverb = 0x00000020;

/** flag to indicate whether filter parameters should be automatically calculated and
 *  applied for the sound playing on this voice.  The filter parameters can be changed
 *  by the spatial occlusion factor and interaural time delay calculations.
 */
constexpr PlaybackModeFlags fPlaybackModeUseFilters = 0x00000040;

/** flag to indicate the current mute state for a voice.  This is valid only when the
 *  @ref fVoiceParamMute flag is used.  When this flag is set, the voice's output will be
 *  muted.  When this flag is not set, the voice's volume will be restored to its previous
 *  level.  This is useful for temporarily silencing a voice without having to clobber its
 *  current volume level or affect its emitter attributes.
 */
constexpr PlaybackModeFlags fPlaybackModeMuted = 0x00000080;

/** flag to indicate the current pause state of a sound.  This is valid only when the
 *  @ref fVoiceParamPause flag is used.  When this flag is set, the voice's playback is
 *  paused.  When this flag is not set, the voice's playback will resume.  Note that if all
 *  buses are occupied, pausing a voice may allow another voice to steal its bus.  When a
 *  voice is resumed, it will continue playback from the same location it was paused at.
 *
 *  When pausing or unpausing a sound, a small volume ramp will be used internally to avoid
 *  a popping artifact in the stream.  This will not affect the voice's current volume level.
 *
 *  Note that if the voice is on the simulation path and it is paused and unpaused rapidly,
 *  the simulated position may not be updated properly unless the context's update() function
 *  is also called at least at the same rate that the voice is paused and unpaused at.  This
 *  can lead to a voice's simulated position not being accurately tracked if care is not also
 *  taken with the frequency of update() calls.
 */
constexpr PlaybackModeFlags fPlaybackModePaused = 0x00000100;

/** Flag to indicate that the sound should fade in when being initially played.
 *  This should be used when it's not certain that the sound is starting on a
 *  zero-crossing, so playing it without a fade-in will cause a pop.
 *  The fade-in takes 10-20ms, so this isn't suitable for situations where a
 *  gradual fade-in is desired; that should be done manually using callbacks.
 *  This flag has no effect after the sound has already started playing;
 *  actions like unpausing and unmuting will always fade in to avoid a pop.
 */
constexpr PlaybackModeFlags fPlaybackModeFadeIn = 0x00000200;

/** Flags to indicate the behavior that is used when a simulated voice gets assigned
 *  to a bus.
 *
 *  @ref fPlaybackModeSimulatePosition will cause the voice's playback position to
 *  be simulated during update() calls while the voice is not assigned to a bus.
 *  When the voice is assigned to a bus, it will begin at that simulated
 *  position. For example if a sound had its bus stolen then was assigned to a
 *  bus 2 seconds later, the sound would begin playback at the position 2
 *  seconds after the position it was when the bus was stolen.
 *  Voices are faded in from silence when the beginning playback part way
 *  through to avoid discontinuities.
 *  Additionally, voices that have finished playback while in the simulated state
 *  will be cleaned up automatically.
 *
 *  @ref fPlaybackModeNoPositionSimulation will cause a simulated voice to begin
 *  playing from the start of its sound when it is assigned to a bus.
 *  Any voice with this setting will not finish when being simulated unless stopVoice()
 *  is called.
 *  This is intended for uses cases such as infinitely looping ambient noise
 *  (e.g. a burning torch); cases where a sound will not play infinitely may
 *  sound strange when this option is used.
 *  This option reduces the calculation overhead of update().
 *  This behavior will be used for sounds that are infinitely looping if neither
 *  of @ref fPlaybackModeSimulatePosition or @ref fPlaybackModeNoPositionSimulation
 *  are specified.
 *
 *  If neither of @ref fPlaybackModeSimulatePosition nor
 *  @ref fPlaybackModeNoPositionSimulation are specified, the default behavior
 *  will be used. Infinitely looping sounds have the default behavior of
 *  @ref fPlaybackModeNoPositionSimulation and sounds with no loop or a finite
 *  loop count will use @ref fPlaybackModeSimulatePosition.
 *
 *  When retrieving the playback mode from a playing voice, exactly one of
 *  these bits will always be set. Trying to update the playback mode to have
 *  both of these bits clear or both of these bits set will result in the
 *  previous value of these two bits being preserved in the playback mode.
 */
constexpr PlaybackModeFlags fPlaybackModeSimulatePosition = 0x00000400;

/** @copydoc fPlaybackModeSimulatePosition */
constexpr PlaybackModeFlags fPlaybackModeNoPositionSimulation = 0x00000800;

/** flag to indicate that a multi-channel spatial voice should treat a specific matrix as its
 *  non-spatial output, when using the 'spatial mix level' parameter, rather than blending all
 *  channels evenly into the output. The matrix used will be the one specified by the voice
 *  parameters. If no matrix was specified in the voice parameters, the default matrix for
 *  that channel combination will be used. If there is no default matrix for that channel
 *  combination, the default behavior of blending all channels evenly into the output will be used.
 *  This flag is ignored on non-spatial voices, since they cannot use the 'spatial mix level'
 *  parameter.
 *  This flag is also ignored on mono voices.
 *  This should be used with caution as the non-spatial mixing matrix may add a direction to each
 *  channel (the default ones do this), which could make the spatial audio sound very strange.
 *  Additionally, the default mixing matrices will often be quieter than the fully spatial sound
 *  so this may sound unexpected
 *  (this is because the default matrices ensure no destination channel has more than 1.0 volume,
 *  while the spatial calculations are treated as multiple emitters at the same point in
 *  space).
 *  The default behavior of mixing all channels evenly is intended to make the sound become
 *  omni-present which is the intended use case for this feature.
 */
constexpr PlaybackModeFlags fPlaybackModeSpatialMixLevelMatrix = 0x00001000;

/** flag to disable the low frequency effect channel (@ref Speaker::eLowFrequencyEffect)
 *  for a spatial voice. By default, some component of the 3D audio will be mixed into
 *  the low frequency effect channel; setting this flag will result in none of the audio
 *  being mixed into this channel.
 *  This flag may be desirable in cases where spatial sounds have a specially mastered
 *  low frequency effect channel that will be played separately.
 *  This has no effect on a non-spatial voice; disabling the low frequency
 *  effect channel on a non-spatial voice can be done through by setting its
 *  mixing matrix.
 */
constexpr PlaybackModeFlags fPlaybackModeNoSpatialLowFrequencyEffect = 0x00002000;

/** flag to indicate that a voice should be immediately stopped if it ever gets unassigned from
 *  its bus and put on the simulation path.  This will also fail calls to playSound() if the
 *  voice is immediately put on the simulation path.  The default behavior is that voices will
 *  be able to go into simulation mode.
 */
constexpr PlaybackModeFlags fPlaybackModeStopOnSimulation = 0x00004000;

/** mask of playback mode bits that are available for public use.  Clear bits in this mask are
 *  used internally and should not be referenced in other flags here.  This is not valid as a
 *  playback mode flag or flag set.
 */
constexpr PlaybackModeFlags fPlaybackModeAvailableMask = 0x7fffffff;

/** default playback mode flag states.  These flags allow the context's default playback mode
 *  for selected playback modes to be used instead of always having to explicitly specify various
 *  playback modes for each new play task.  Note that these flags only take effect when a sound
 *  is first played on a voice, or when update() is called after changing a voice's default
 *  playback mode state.  Changing the context's default playback mode settings will not take
 *  effect on voices that are already playing.
 *
 *  When these flags are used in the VoiceParams::playbackMode flag set in a PlaySoundDesc
 *  descriptor or a setVoiceParameters() call, these will take the state for their corresponding
 *  non-default flags from the context's default playback mode state value and ignore the flags
 *  specified for the voice's parameter itself.
 *
 *  When these flags are used in the ContextParams::playbackMode flag set, they can be used
 *  to indicate a 'force off', 'force on', or absolute state for the value.  When used, the
 *  'force' states will cause the feature to be  temporarily disabled or enabled on all playing
 *  buses on the next update cycle.  Using these forced states will not change the playback
 *  mode settings of each individual playing voice, but simply override them.  When the force
 *  flag is removed from the context, each voice's previous behavior will resume (on the next
 *  update cycle).  When these are used as an absolute state, they determine what the context's
 *  default playback mode for each flag will be.
 *
 *  Note that for efficiency's sake, if both 'force on' and 'force off' flags are specified
 *  for any given state, the 'force off' flag will always take precedence.  It is up to the
 *  host app to ensure that conflicting flags are not specified simultaneously.
 *
 *  @{
 */
/** when used in a voice parameters playback mode flag set, this indicates that new voices will
 *  always use the context's current default Doppler playback mode flag and ignore any specific
 *  flag set on the voice parameters.  When used on the context's default playback mode flag set,
 *  this indicates that the context's default Doppler mode is enabled.
 */
constexpr PlaybackModeFlags fPlaybackModeDefaultUseDoppler = 0x40000000;

/** when used in a voice parameters playback mode flag set, this indicates that new voices will
 *  always use the context's current default distance delay playback mode flag and ignore any
 *  specific flag set on the voice parameters.  When used on the context's default playback mode
 *  flag set, this indicates that the context's default distance delay mode is enabled.
 */
constexpr PlaybackModeFlags fPlaybackModeDefaultDistanceDelay = 0x20000000;

/** when used in a voice parameters playback mode flag set, this indicates that new voices will
 *  always use the context's current default interaural time delay playback mode flag and ignore
 *  any specific flag set on the voice parameters.  When used on the context's default playback
 *  mode flag set, this indicates that the context's default interaural time delay mode is
 *  enabled.
 */
constexpr PlaybackModeFlags fPlaybackModeDefaultInterauralDelay = 0x10000000;

/** when used in a voice parameters playback mode flag set, this indicates that new voices will
 *  always use the context's current default reverb playback mode flag and ignore any specific
 *  flag set on the voice parameters.  When used on the context's default playback mode flag set,
 *  this indicates that the context's default reverb mode is enabled.
 */
constexpr PlaybackModeFlags fPlaybackModeDefaultUseReverb = 0x08000000;

/** when used in a voice parameters playback mode flag set, this indicates that new voices will
 *  always use the context's current default filters playback mode flag and ignore any specific
 *  flag set on the voice parameters.  When used on the context's default playback mode flag set,
 *  this indicates that the context's default filters mode is enabled.
 */
constexpr PlaybackModeFlags fPlaybackModeDefaultUseFilters = 0x04000000;

/** the mask of all 'default' state playback mode flags.  If new flags are added above,
 *  they must be added here as well.
 */
constexpr PlaybackModeFlags fPlaybackModeDefaultMask =
    fPlaybackModeDefaultUseDoppler | fPlaybackModeDefaultDistanceDelay | fPlaybackModeDefaultInterauralDelay |
    fPlaybackModeDefaultUseReverb | fPlaybackModeDefaultUseFilters;

/** the maximum number of 'default' state playback mode flags.  If new flags are added above,
 *  their count may not exceed this value otherwise the playback mode flags will run out of
 *  bits.
 */
constexpr size_t kPlaybackModeDefaultFlagCount = 10;

// FIXME: exhale can't handle this
#ifndef DOXYGEN_SHOULD_SKIP_THIS
/** retrieves a set of default playback mode flags that will behave as 'force off' behavior.
 *
 *  @param[in] flags    the set of fPlaybackModeDefault* flags to use as 'force off' flags.
 *  @returns the corresponding 'force off' flags for the input flags.  These are suitable
 *           for setting in the context parameters' default playback mode value.  These
 *           should not be used in the voice parameter block's playback mode value - this
 *           will lead to unexpected behavior.
 */
constexpr PlaybackModeFlags getForceOffPlaybackModeFlags(PlaybackModeFlags flags)
{
    return (flags & fPlaybackModeDefaultMask) >> kPlaybackModeDefaultFlagCount;
}

/** retrieves a set of default playback mode flags that will behave as 'force on' behavior.
 *
 *  @param[in] flags    the set of fPlaybackModeDefault* flags to use as 'force on' flags.
 *  @returns the corresponding 'force on' flags for the input flags.  These are suitable
 *           for setting in the context parameters' default playback mode value.  These
 *           should not be used in the voice parameter block's playback mode value - this
 *           will lead to unexpected behavior.
 */
constexpr PlaybackModeFlags getForceOnPlaybackModeFlags(PlaybackModeFlags flags)
{
    return (flags & fPlaybackModeDefaultMask) >> (kPlaybackModeDefaultFlagCount * 2);
}
#endif
/** @} */
/** @} */

/** base type for the voice parameter flags.  These flags describe the set of parameters in the
 *  @ref VoiceParams structure that are valid for an operation.  For setting operations, the
 *  caller is responsible for ensuring that all flagged values are valid in the @ref VoiceParams
 *  structure before calling.  For getting operations, the flagged values will be valid upon
 *  return.
 *  @{
 */
typedef uint32_t VoiceParamFlags;

/** all parameters are valid. */
constexpr VoiceParamFlags fVoiceParamAll = static_cast<VoiceParamFlags>(~0);

/** when set, this flag indicates that the @ref VoiceParams::playbackMode value is valid.  This
 *  does not however include the @ref fPlaybackModeMuted and @ref fPlaybackModePaused states in
 *  @ref VoiceParams::playbackMode unless the @ref fVoiceParamMute or @ref fVoiceParamPause
 *  flags are also set.
 */
constexpr VoiceParamFlags fVoiceParamPlaybackMode = 0x00000001;

/** when set, this flag indicates that the @ref VoiceParams::volume value is valid. */
constexpr VoiceParamFlags fVoiceParamVolume = 0x00000002;

/** when set, this flag indicates that the state of the @ref fPlaybackModeMuted flag is valid
 *  in the @ref VoiceParams::playbackMode flag set.
 */
constexpr VoiceParamFlags fVoiceParamMute = 0x00000004;

/** when set, this flag indicates that the @ref VoiceParams::balance values are valid. */
constexpr VoiceParamFlags fVoiceParamBalance = 0x00000008;

/** when set, this flag indicates that the @ref VoiceParams::frequencyRatio value is valid. */
constexpr VoiceParamFlags fVoiceParamFrequencyRatio = 0x00000010;

/** when set, this flag indicates that the @ref VoiceParams::priority value is valid. */
constexpr VoiceParamFlags fVoiceParamPriority = 0x00000020;

/** when set, this flag indicates that the state of the @ref fPlaybackModePaused flag is valid
 *  in the @ref VoiceParams::playbackMode flag set.
 */
constexpr VoiceParamFlags fVoiceParamPause = 0x00000040;

/** when set, this flag indicates that the @ref VoiceParams::spatialMixLevel value is valid. */
constexpr VoiceParamFlags fVoiceParamSpatialMixLevel = 0x00000080;

/** when set, this flag indicates that the @ref VoiceParams::dopplerScale value is valid. */
constexpr VoiceParamFlags fVoiceParamDopplerScale = 0x00000100;

/** when set, this flag indicates that the @ref VoiceParams::occlusion values are valid. */
constexpr VoiceParamFlags fVoiceParamOcclusionFactor = 0x00000200;

/** when set, this flag indicates that the @ref VoiceParams::emitter values are valid. */
constexpr VoiceParamFlags fVoiceParamEmitter = 0x00000400;

/** when set, this flag indicates that the @ref VoiceParams::matrix values are valid. */
constexpr VoiceParamFlags fVoiceParamMatrix = 0x00000800;
/** @} */

/** voice parameters block.  This can potentially contain all of a voice's parameters and their
 *  current values.  This is used to both set and retrieve one or more of a voice's parameters
 *  in a single call.  The fVoiceParam* flags that are passed to setVoiceParameters() or
 *  getVoiceParameters() determine which values in this block are guaranteed to be valid.
 */
struct VoiceParams
{
    /** flags to indicate how a sound is to be played back.  These values is valid only when the
     *  @ref fVoiceParamPlaybackMode, @ref fVoiceParamMute, or @ref fVoiceParamPause flags are
     *  used.  This controls whether the sound is played as a spatial or non-spatial sound and
     *  how the emitter's attributes will be interpreted (ie: either world coordinates or
     *  listener relative).
     */
    PlaybackModeFlags playbackMode;

    /** the volume level for the voice.  This is valid when the @ref fVoiceParamVolume flag is
     *  used.  This should be 0.0 for silence or 1.0 for normal volume.  A negative value may be
     *  used to invert the signal.  A value greater than 1.0 will amplify the signal.  The volume
     *  level can be interpreted as a linear scale where a value of 0.5 is half volume and 2.0 is
     *  double volume.  Any volume values in decibels must first be converted to a linear volume
     *  scale before setting this value.  The default value is 1.0.
     */
    float volume;

    /** non-spatial sound positioning parameters.  These provide pan and fade values for the
     *  voice to give the impression that the sound is located closer to one of the quadrants
     *  of the acoustic space versus the others.  These values are ignored for spatial sounds.
     */
    struct VoiceParamBalance
    {
        /** sets the non-spatial panning value for a voice.  This value is valid when the
         *  @ref fVoiceParamBalance flag is used.  This is 0.0 to have the sound "centered" in all
         *  speakers.  This is -1.0 to have the sound balanced to the left side.  This is 1.0 to
         *  have the sound balanced to the right side.  The way the sound is balanced depends on
         *  the number of channels.  For example, a mono sound will be balanced between the left
         *  and right sides according to the panning value, but a stereo sound will just have the
         *  left or right channels' volumes turned down according to the panning value.  This
         *  value is ignored for spatial sounds.  The default value is 0.0.
         *
         *  Note that panning on non-spatial sounds should only be used for mono or stereo sounds.
         *  When it is applied to sounds with more channels, the results are often undefined or
         *  may sound odd.
         */
        float pan;

        /** sets the non-spatial fade value for a voice.  This value is valid when the
         *  @ref fVoiceParamBalance flag is used.  This is 0.0 to have the sound "centered" in all
         *  speakers.  This is -1.0 to have the sound balanced to the back side.  This is 1.0 to
         *  have the sound balanced to the front side.  The way the sound is balanced depends on
         *  the number of channels.  For example, a mono sound will be balanced between the front
         *  and back speakers according to the fade value, but a 5.1 sound will just have the
         *  front or back channels' volumes turned down according to the fade value.  This value
         *  is ignored for spatial sounds.  The default value is 0.0.
         *
         *  Note that using fade on non-spatial sounds should only be used for mono or stereo
         *  sounds.  When it is applied to sounds with more channels, the results are often
         *  undefined or may sound odd.
         */
        float fade;
    } balance; //!< Non-spatial sound positioning parameters.

    /** the frequency ratio for a voice.  This is valid when the @ref fVoiceParamFrequencyRatio
     *  flag is used.  This will be 1.0 to play back a sound at its normal rate, a value less than
     *  1.0 to lower the pitch and play it back more slowly, and a value higher than 1.0 to
     *  increase the pitch and play it back faster.  For example, a pitch scale of 0.5 will play
     *  back at half the pitch (ie: lower frequency, takes twice the time to play versus normal),
     *  and a pitch scale of 2.0 will play back at double the pitch (ie: higher frequency, takes
     *  half the time to play versus normal).  The default value is 1.0.
     *
     *  On some platforms, the frequency ratio may be silently clamped to an acceptable range
     *  internally.  For example, a value of 0.0 is not allowed.  This will be clamped to the
     *  minimum supported value instead.
     *
     *  Note that the even though the frequency ratio *can* be set to any value in the range from
     *  1/1024 to 1024, this very large range should only be used in cases where it is well known
     *  that the particular sound being operated on will still sound valid after the change.  In
     *  the real world, some of these extreme frequency ratios may make sense, but in the digital
     *  world, extreme frequency ratios can result in audio corruption or even silence.  This
     *  happens because the new frequency falls outside of the range that is faithfully
     *  representable by either the audio device or sound data itself.  For example, a 4KHz tone
     *  being played at a frequency ratio larger than 6.0 will be above the maximum representable
     *  frequency for a 48KHz device or sound file.  This case will result in a form of corruption
     *  known as aliasing, where the frequency components above the maximum representable
     *  frequency will become audio artifacts.  Similarly, an 800Hz tone being played at a
     *  frequency ratio smaller than 1/40 will be inaudible because it falls below the frequency
     *  range of the human ear.
     *
     *  In general, most use cases will find that the frequency ratio range of [0.1, 10] is more
     *  than sufficient for their needs.  Further, for many cases, the range from [0.2, 4] would
     *  suffice.  Care should be taken to appropriately cap the used range for this value.
     */
    float frequencyRatio;

    /** the playback priority of this voice.  This is valid when the @ref fVoiceParamPriority
     *  flag is used.  This is an arbitrary value whose scale is defined by the host app.  A
     *  value of 0 is the default priority.  Negative values indicate lower priorities and
     *  positive values indicate higher priorities.  This priority value helps to determine
     *  which voices are the most important to be audible at any given time.  When all buses
     *  are busy, this value will be used to compare against other playing voices to see if
     *  it should steal a bus from another lower priority sound or if it can wait until another
     *  bus finishes first.  Higher priority sounds will be ensured a bus to play on over lower
     *  priority sounds.  If multiple sounds have the same priority levels, the louder sound(s)
     *  will take priority.  When a higher priority sound is queued, it will try to steal a bus
     *  from the quietest sound with lower or equal priority.
     */
    int32_t priority;

    /** the spatial mix level.  This is valid when @ref fVoiceParamSpatialMixLevel flag is used.
     *  This controls the mix between the results of a voice's spatial sound calculations and its
     *  non-spatial calculations.  When this is set to 1.0, only the spatial sound calculations
     *  will affect the voice's playback.  This is the default when state.  When set to 0.0, only
     *  the non-spatial sound calculations will affect the voice's playback.  When set to a value
     *  between 0.0 and 1.0, the results of the spatial and non-spatial sound calculations will
     *  be mixed with the weighting according to this value.  This value will be ignored if
     *  @ref fPlaybackModeSpatial is not set.  The default value is 1.0.
     *  Values above 1.0 will be treated as 1.0. Values below 0.0 will be treated as 0.0.
     *
     *  @ref fPlaybackModeSpatialMixLevelMatrix affects the non-spatial mixing behavior of this
     *  parameter for multi-channel voices. By default, a multi-channel spatial voice's non-spatial
     *  component will treat each channel as a separate mono voice. With the
     *  @ref fPlaybackModeSpatialMixLevelMatrix flag set, the non-spatial component will be set
     *  with the specified output matrix or the default output matrix.
     */
    float spatialMixLevel;

    /** the Doppler scale value.  This is valid when the @ref fVoiceParamDopplerScale flag is
     *  used.  This allows the result of internal Doppler calculations to be scaled to emulate
     *  a time warping effect.  This should be near 0.0 to greatly reduce the effect of the
     *  Doppler calculations, and up to 5.0 to exaggerate the Doppler effect.  A value of 1.0
     *  will leave the calculated Doppler factors unmodified.  The default value is 1.0.
     */
    float dopplerScale;

    /** the occlusion factors for a voice.  This is valid when the @ref fVoiceParamOcclusionFactor
     *  flag is used.  These values control automatic low pass filters that get applied to the
     *  spatial sounds to simulate object occlusion between the emitter and listener positions.
     */
    struct VoiceParamOcclusion
    {
        /** the occlusion factor for the direct path of the sound.  This is the path directly from
         *  the emitter to the listener.  This factor describes how occluded the sound's path
         *  actually is.  A value of 1.0 means that the sound is fully occluded by an object
         *  between the voice and the listener.  A value of 0.0 means that the sound is not
         *  occluded by any object at all.  This defaults to 0.0.
         *  This factor multiplies by @ref EntityCone::lowPassFilter, if a cone with a non 1.0
         *  lowPassFilter value is specified.
         *  Setting this to a value outside of [0.0, 1.0] will result in an undefined low pass
         *  filter value being used.
         */
        float direct;

        /** the occlusion factor for the reverb path of the sound.  This is the path taken for
         *  sounds reflecting back to the listener after hitting a wall or other object.  A value
         *  of 1.0 means that the sound is fully occluded by an object between the listener and
         *  the object that the sound reflected off of.  A value of 0.0 means that the sound is
         *  not occluded by any object at all.  This defaults to 1.0.
         */
        float reverb;
    } occlusion; //!< Occlusion factors for a voice.

    /** the attributes of the emitter related for this voice.  This is only valid when the
     *  @ref fVoiceParamEmitter flag is used.  This includes the emitter's position, orientation,
     *  velocity, cone, and rolloff curves.  The default values for these attributes are noted
     *  in the @ref EmitterAttributes object.  This will be ignored for non-spatial sounds.
     */
    EmitterAttributes emitter;

    /** the channel mixing matrix to use for this @ref Voice. The rows of this matrix represent
     *  each output channel from this @ref Voice and the columns of this matrix represent the
     *  input channels of this @ref Voice (e.g. this is a inputChannels x outputChannels matrix).
     *  The output channel count will always be the number of audio channels set on the
     *  @ref Context.
     *  Each cell in the matrix should be a value from 0.0-1.0 to specify the volume that
     *  this input channel should be mixed into the output channel. Setting negative values
     *  will invert the signal. Setting values above 1.0 will amplify the signal past unity
     *  gain when being mixed.
     *
     *  This setting is mutually exclusive with @ref balance; setting one will disable the
     *  other.
     *  This setting is only available for spatial sounds if @ref fPlaybackModeSpatialMixLevelMatrix
     *  if set in the playback mode parameter. Multi-channel spatial audio is interpreted as
     *  multiple emitters existing at the same point in space, so a purely spatial voice cannot
     *  have an output matrix specified.
     *
     *  Setting this to nullptr will reset the matrix to the default for the given channel
     *  count.
     *  The following table shows the speaker modes that are used for the default output
     *  matrices. Voices with a speaker mode that is not in the following table will
     *  use the default output matrix for the speaker mode in the following table that
     *  has the same number of channels.
     *  If there is no default matrix for the channel count of the @ref Voice, the output
     *  matrix will have 1.0 in the any cell (i, j) where i == j and 0.0 in all other cells.
     *
     *  | Channels | Speaker Mode                       |
     *  | -------- | ---------------------------------- |
     *  |        1 | kSpeakerModeMono                   |
     *  |        2 | kSpeakerModeStereo                 |
     *  |        3 | kSpeakerModeTwoPointOne            |
     *  |        4 | kSpeakerModeQuad                   |
     *  |        5 | kSpeakerModeFourPointOne           |
     *  |        6 | kSpeakerModeFivePointOne           |
     *  |        7 | kSpeakerModeSixPointOne            |
     *  |        8 | kSpeakerModeSevenPointOne          |
     *  |       10 | kSpeakerModeNinePointOne           |
     *  |       12 | kSpeakerModeSevenPointOnePointFour |
     *  |       14 | kSpeakerModeNinePointOnePointFour  |
     *  |       16 | kSpeakerModeNinePointOnePointSix   |
     *
     *  It is recommended to explicitly set an output matrix on a non-spatial @ref Voice
     *  if the @ref Voice or the @ref Context have a speaker layout that is not found in
     *  the above table.
     */
    const float* matrix;

    /** reserved for future expansion.  This must be set to nullptr. */
    void* ext = nullptr;
};


/** base type for the context parameter flags.  These flags describe the set of parameters in the
 *  @ref ContextParams structure that are valid for an operation.  For setting operations, the
 *  caller is responsible for ensuring that all flagged values are valid in the @ref ContextParams
 *  structure before calling.  For getting operations, the flagged values will be valid upon
 *  return.
 *  @{
 */
using ContextParamFlags = uint32_t;

/** all parameters are valid. */
constexpr ContextParamFlags fContextParamAll = ~0u;

/** when set, this flag indicates that the @ref ContextParams::speedOfSound value is valid. */
constexpr ContextParamFlags fContextParamSpeedOfSound = 0x00000001;

/** when set, this flag indicates that the @ref ContextParams::unitsPerMeter value is valid. */
constexpr ContextParamFlags fContextParamWorldUnitScale = 0x00000002;

/** when set, this flag indicates that the @ref ContextParams::listener values are valid. */
constexpr ContextParamFlags fContextParamListener = 0x00000004;

/** when set, this flag indicates that the @ref ContextParams::dopplerScale value is valid. */
constexpr ContextParamFlags fContextParamDopplerScale = 0x00000008;

/** when set, this flag indicates that the @ref ContextParams::virtualizationThreshold value
 *  is valid.
 */
constexpr ContextParamFlags fContextParamVirtualizationThreshold = 0x00000010;

/** when set, this flag indicates that the @ref ContextParams::spatialFrequencyRatio value
 *  is valid.
 */
constexpr ContextParamFlags fContextParamSpatialFrequencyRatio = 0x00000020;

/** when set, this flag indicates that the @ref ContextParams::nonSpatialFrequencyRatio value
 *  is valid.
 */
constexpr ContextParamFlags fContextParamNonSpatialFrequencyRatio = 0x00000040;

/** when set, this flag indicates that the @ref ContextParams::masterVolume value is valid. */
constexpr ContextParamFlags fContextParamMasterVolume = 0x00000080;

/** when set, this flag indicates that the @ref ContextParams::spatialVolume value is valid. */
constexpr ContextParamFlags fContextParamSpatialVolume = 0x00000100;

/** when set, this flag indicates that the @ref ContextParams::nonSpatialVolume value is valid. */
constexpr ContextParamFlags fContextParamNonSpatialVolume = 0x00000200;

/** when set, this flag indicates that the @ref ContextParams::dopplerLimit value is valid. */
constexpr ContextParamFlags fContextParamDopplerLimit = 0x00000400;

/** when set, this flag indicates that the @ref ContextParams::defaultPlaybackMode value is valid. */
constexpr ContextParamFlags fContextParamDefaultPlaybackMode = 0x00000800;

/** when set, this flag indicates that the @ref ContextParams::flags value is valid. */
constexpr ContextParamFlags fContextParamFlags = 0x00001000;

/** When set, this flag indicates that the @ref ContextParams2::videoLatency and
 *  @ref ContextParams2::videoLatencyTrim values are valid.  In this case, the
 *  @ref ContextParams::ext value must be set to point to a @ref ContextParams2
 *  object.
 */
constexpr ContextParamFlags fContextParamVideoLatency = 0x00002000;

/** flag to indicate that voice table validation should be performed any time a voice is allocated
 *  or recycled.  There is a performance cost to enabling this.  Any operation involving a new
 *  voice or recycling a voice will become O(n^2).  This flag is ignored in release builds.
 *  When this flag is not used, no voice validation will be performed.
 */
constexpr ContextParamFlags fContextParamFlagValidateVoiceTable = 0x10000000;
/** @} */


/** The default speed of sound parameter for a Context.
 *  This is specified in meters per second.
 *  This is the approximate speed of sound in air at sea level at 15 degrees Celsius.
 */
constexpr float kDefaultSpeedOfSound = 340.f;

/** context parameters block.  This can potentially contain all of a context's parameters and
 *  their current values.  This is used to both set and retrieve one or more of a context's
 *  parameters in a single call.  The set of fContextParam* flags that are passed to
 *  getContextParameter() or setContextParameter() indicates which values in the block are
 *  guaranteed to be valid.
 */
struct ContextParams
{
    /** the speed of sound for the context.  This is valid when the @ref fContextParamSpeedOfSound
     *  flag is used.  This is measured in meters per second.  This will affect the calculation of
     *  Doppler shift factors and interaural time difference for spatial voices.  The default
     *  value is @ref kDefaultSpeedOfSound m/s.
     *  This value can be changed at any time to affect Doppler calculations globally.  This should
     *  only change when the sound environment itself changes (ie: move from air into water).
     */
    float speedOfSound;

    /** the number of arbitrary world units per meter.  This is valid when the
     *  @ref fContextParamWorldUnitScale flag is used.  World units are arbitrary and often
     *  determined by the level/world designers.  For example, if the world units are in feet,
     *  this value would need to be set to 3.28.  All spatial audio calculations are performed in
     *  SI units (ie: meters for distance, seconds for time).  This provides a conversion factor
     *  from arbitrary world distance units to SI units.  This conversion factor will affect all
     *  audio distance and velocity calculations.  This defaults to 1.0, indicating that the world
     *  units are assumed to be measured in meters.  This must not be 0.0 or negative.
     */
    float unitsPerMeter;

    /** the global Doppler scale value for this context.  This is applied to all calculated
     *  Doppler factors to enhance or diminish the effect of the Doppler factor.  A value of
     *  1.0 will leave the calculated Doppler levels unmodified.  A value lower than 1.0 will
     *  have the result of diminishing the Doppler effect.  A value higher than 1.0 will have
     *  the effect of enhancing the Doppler effect.  This is useful for handling global time
     *  warping effects.  This parameter is a unit less scaling factor.  This defaults to 1.0.
     */
    float dopplerScale;

    /** the global Doppler limit value for this context.  This will cap the calculated Doppler
     *  factor at the high end.  This is to avoid excessive aliasing effects for very fast moving
     *  emitters or listener.  When the relative velocity between the listener and an emitter is
     *  nearing the speed of sound, the calculated Doppler factor can approach ~11 million.
     *  Accurately simulating this would require a very large scale anti-aliasing or filtering pass
     *  on all resampling operations on the emitter.  That is unfortunately not possible in
     *  general.  Clamping the Doppler factor to something relatively small like 16 will still
     *  result in the effect being heard but will reduce the resampling artifacts related to high
     *  frequency ratios.  A Doppler factor of 16 represents a relative velocity of ~94% the speed
     *  of sound so there shouldn't be too much of a loss in the frequency change simulation.
     *  This defaults to 16.  This should not be negative or zero.
     */
    float dopplerLimit;

    /** the effective volume level at which a voice will become virtual.  A virtual voice will
     *  not decode any data and will perform only minimal update tasks when needed.  This
     *  is expressed as a linear volume value from 0.0 (silence) to 1.0 (full volume).  The
     *  default volume is 0.0.
     */
    float virtualizationThreshold;

    /** the global frequency ratio to apply to all active spatial voices.  This should only be
     *  used to handle time dilation effects on the voices, not to deal with pitch changes (ie:
     *  record a sound at a high frequency to save on storage space and load time, but then always
     *  play it back at a reduced pitch).  If this is used for pitch changes, it will interfere
     *  with distance delay calculations and possibly lead to other undesirable behavior.
     *  This will not affect any non-spatial voices.  This defaults to 1.0.
     */
    float spatialFrequencyRatio;

    /** the global frequency ratio to apply to all active non-spatial voices.  This can be used
     *  to perform a pitch change (and as a result play time change) on all non-spatial voices,
     *  or it can be used to simulate a time dilation effect on all non-spatial voices.  This
     *  will not affect any spatial voices.  This defaults to 1.0.
     */
    float nonSpatialFrequencyRatio;

    /** the global master volume for this context.  This will only affect the volume level of
     *  the final mixed output signal.  It will not directly affect the relative volumes of
     *  each voice being played.  No individual voice will be able to produce a signal louder
     *  than this volume level.  This is set to 0.0 to silence all output.  This defaults to
     *  1.0 (ie: full volume).  This should not be set beyond 1.0.
     */
    float masterVolume;

    /** the relative volume of all spatial voices.  This will be the initial volume level of
     *  any new spatial voice.  The volume specified in that voice's parameters will be multiplied
     *  by this volume.  This does not affect the volume level of non-spatial voices.  This is
     *  set to 0.0 to silence all spatial voices.  This defaults to 1.0 (ie: full volume).  This
     *  should not be set beyond 1.0.  This volume is independent of the @ref masterVolume value
     *  but will effectively be multiplied by it for the final output.
     */
    float spatialVolume;

    /** the relative volume of all non-spatial voices.  This will be the initial volume level of
     *  any new non-spatial voice.  The volume specified in that voice's parameters will be
     *  multiplied by this volume.  This does not affect the volume level of spatial voices.  This
     *  is set to 0.0 to silence all non-spatial voices.  This defaults to 1.0 (ie: full volume).
     *  This should not be set beyond 1.0.  This volume is independent of the @ref masterVolume
     *  value but will effectively be multiplied by it for the final output.
     */
    float nonSpatialVolume;

    /** attributes of the listener.  This is valid when the @ref fContextParamListener flag is
     *  used.  Only the values that have their corresponding fEntityFlag* flags set will be
     *  updated on a set operation.  Any values that do not have their corresponding flag set
     *  will just be ignored and the previous value will remain current.  This can be used to (for
     *  example) only update the position for the listener but leave its orientation and velocity
     *  unchanged.  The flags would also indicate whether the orientation vectors need to be
     *  normalized or made perpendicular before continuing.  Fixing up these orientation vectors
     *  should only be left up to this call if it is well known that the vectors are not already
     *  correct.
     *
     *  It is the caller's responsibility to decide on the listener's appropriate position and
     *  orientation for any given situation.  For example, if the listener is represented by a
     *  third person character, it would make the most sense to set the position to the
     *  character's head position, but to keep the orientation relative to the camera's forward
     *  and up vectors (possibly have the camera's forward vector projected onto the character's
     *  forward vector).  If the character's orientation were used instead, the audio may jump
     *  from one speaker to another as the character rotates relative to the camera.
     */
    ListenerAttributes listener;

    /** defines the default playback mode flags for the context.  These will provide 'default'
     *  behavior for new voices.  Any new voice that uses the fPlaybackModeDefault* flags in
     *  its voice parameters block will inherit the default playback mode behavior from this
     *  value.  Note that only specific playback modes are supported in these defaults. This
     *  also provides a means to either 'force on' or 'force off' certain features for each
     *  voice.  This value is a collection of zero or more of the fPlaybackModeDefault* flags,
     *  and flags that have been returned from either getForceOffPlaybackModeFlags() or
     *  getForceOnPlaybackModeFlags().  This may not include any of the other fPlaybackMode*
     *  flags that are not part of the fPlaybackModeDefault* set.  If the other playback mode
     *  flags are used, the results will be undefined.  This defaults to 0.
     */
    PlaybackModeFlags defaultPlaybackMode;

    /** flags to control the behavior of the context.  This is zero or more of the
     *  @ref ContextParamFlags flags.  This defaults to 0.
     */
    ContextParamFlags flags;

    /** Reserved for future expansion.  This must be set to nullptr unless a @ref ContextParams2
     *  object is also being passed in.
     */
    void* ext = nullptr;
};

/** Extended context parameters descriptor object.  This provides additional context parameters
 *  that were not present in the original version of @ref ContextParams.  This structure also
 *  has room for future expansion.
 */
struct ContextParams2
{
    /** An estimate of the current level of latency between when a video frame is produced
     *  and when it will be displayed to the screen.  In cases where a related renderer is
     *  buffering frames (ie: double or triple buffering frames in flight), there can be
     *  a latency build-up between when a video frame is displayed on screen and when sounds
     *  related to that particular image should start playing.  This latency is related to
     *  the frame buffering count from the renderer and the current video frame rate.  Since
     *  the video framerate can vary at runtime, this value could be updated frequently with
     *  new estimates based on the current video frame rate.
     *
     *  This value is measured in microseconds and is intended to be an estimate of the current
     *  latency level between video frame production and display.  This will be used by the
     *  playback context to delay the start of new voices by approximately this amount of time.
     *  This should be used to allow the voices to be delayed until a time where it is more
     *  likely that a related video frame is visible on screen.  If this is set to 0 (the
     *  default), all newly queued voices will be scheduled to start playing as soon as possible.
     *
     *  The carb::audio::estimateVideoLatency() helper function can be used to help calculate
     *  this latency estimate.  Negative values are not allowed.
     */
    int64_t videoLatency = 0;

    /** An additional user specified video latency value that can be used to adjust the
     *  value specified in @ref videoLatency by a constant amount.  This is expressed in
     *  microseconds.  This could for example be exposed to user settings to allow user-level
     *  adjustment of audio/video sync or it could be a per-application tuned value to tweak
     *  audio/video sync.  This defaults to 0 microseconds.  Negative values are allowed if
     *  needed as long as @ref videoLatency plus this value is still positive.  The total
     *  latency will always be silently clamped to 0 as needed.
     */
    int64_t videoLatencyTrim = 0;

    /** Extra padding space reserved for future expansion.  Do not use this value directly.
     *  In future versions, new context parameters will be borrowed from this buffer and given
     *  proper names and types as needed.  When this padding buffer is exhausted, a new
     *  @a ContextParams3 object will be added that can be chained to this one.
     */
    void* padding[32] = {};

    /** Reserved for future expansion.  This must be set to `nullptr`. */
    void* ext = nullptr;
};

/** special value for @ref LoopPointDesc::loopPointIndex that indicates no loop point will be
 *  used.  This will be overridden if the @ref LoopPointDesc::loopPoint value is non-nullptr.
 */
constexpr size_t kLoopDescNoLoop = ~0ull;

/** descriptor of a loop point to set on a voice.  This may be specified both when a sound is
 *  first assigned to a voice in playSound() and to change the current loop point on the voice
 *  at a later time with setLoopPoint().
 */
struct LoopPointDesc
{
    /** the event point to loop at.  This may either be an explicitly constructed event point
     *  information block, or be one of the sound's event points, particularly one that is flagged
     *  as a loop point.  This will define a location to loop back to once the end of the initial
     *  play region is reached.  This event point must be within the range of the sound's data
     *  buffer.  If the event point contains a zero length, the loop region will be taken to be
     *  the remainder of the sound starting from the loop point.  This loop point will also
     *  contain a loop count.  This loop count can be an explicit count or indicate that it
     *  should loop infinitely.  Only a single loop point may be set for any given playing
     *  instance of a sound.  Once the sound starts playing on a voice, the loop point and loop
     *  count may not be changed.  A loop may be broken early by passing nullptr or an empty
     *  loop point descriptor to setLoopPoint() if needed.  This may be nullptr to use one of
     *  the loop points from the sound itself by its index by specifying it in
     *  @ref loopPointIndex.
     *
     *  When a loop point is specified explicitly in this manner, it will only be shallow
     *  copied into the voice object.  If its string data is to be maintained to be passed
     *  through to a callback, it is the caller's responsibility to ensure the original loop
     *  point object remains valid for the entire period the voice is playing.  Otherwise, the
     *  string members of the loop point should be set to nullptr.
     *
     *  For performance reasons, it is important to ensure that streaming
     *  sounds do not have short loops (under 1 second is considered very
     *  short), since streaming loops will usually require seeking and some
     *  formats will need to partially decode blocks, which can require
     *  decoding thousands of frames in some formats.
     */
    const EventPoint* loopPoint = nullptr;

    /** the index of the loop point to use from the sound data object itself.  This may be
     *  @ref kLoopDescNoLoop to indicate that no loop point is necessary.  This value is only
     *  used if @ref loopPoint is nullptr.  If the sound data object does not have a valid
     *  loop point at the specified index, the sound will still play but it will not loop.
     *  The loop point identified by this index must remain valid on the sound data object
     *  for the entire period that the voice is playing.
     */
    size_t loopPointIndex = kLoopDescNoLoop;

    /** reserved for future expansion.  This must be set to nullptr. */
    void* ext = nullptr;
};

/** base type for the play descriptor flags.  Zero or more of these may be specified in
 *  @ref PlaySoundDesc::flags.
 *  @{
 */
typedef uint32_t PlayFlags;

/** when set, this indicates that the event points from the sound data object
 *  @ref PlaySoundDesc::sound should be used to trigger callbacks.  If this flag is not set,
 *  no event point callbacks will be triggered for the playing sound.
 */
constexpr PlayFlags fPlayFlagUseEventPoints = 0x00000001;

/** when set, this indicates that voice event callbacks will be performed on the engine thread
 *  immediately when the event occurs instead of queuing it to be performed in a later call to
 *  update() on the host app thread.  This flag is useful for streaming sounds or sounds that
 *  have data dynamically written to them.  This allows for a lower latency callback system.
 *  If the callback is performed on the engine thread, it must complete as quickly as possible
 *  and not perform any operations that may block (ie: read from a file, wait on a signal, use
 *  a high contention lock, etc).  If the callback takes too long, it will cause the engine cycle
 *  to go over time and introduce an artifact or audio dropout into the stream.  In general, if
 *  a real-time callback takes more than ~1ms to execute, there is a very high possibility that
 *  it could cause an audio dropout.  If this flag is not set, all callback events will be queued
 *  when they occur and will not be called until the next call to update() on the context object.
 */
constexpr PlayFlags fPlayFlagRealtimeCallbacks = 0x00000002;

/** when set, this indicates that if a voice is started with a sound that is already over its
 *  max play instances count, it should still be started but immediately put into simulation
 *  mode instead of taking a bus.  When not set, the call to playSound() will fail if the sound's
 *  max instance count is above the max instances count.  Note that if the new voice is put into
 *  simulation mode, it will still count as a playing instance of the sound which will cause its
 *  instance count to go above the maximum.  This flag should be used sparingly at best.
 *
 *  Note that when this flag is used, a valid voice will be created for the sound even though
 *  it is at or above its current instance limit.  Despite it being started in simulation mode,
 *  this will still consume an extra instance count for the sound.  The new voice will only be
 *  allowed to be devirtualized once its sound's instance count has dropped below its limit
 *  again.  However, if the voice has a high priority level and the instance count is still
 *  above the limit, this will prevent other voices from being devirtualized too.  This flag
 *  is best used on low priority, short, fast repeating sounds.
 */
constexpr PlayFlags fPlayFlagMaxInstancesSimulate = 0x00000004;

/** mask of playback flag bits that are available for public use.  Clear bits in this mask are
 *  used internally and should not be referenced in other flags here.  This is not valid as a
 *  playback flag or flag set.
 */
constexpr PlayFlags fPlayFlagAvailableMask = 0x01ffffff;
/** @} */


/** descriptor of how to play a single sound.  At the very least, the @ref sound value must be
 *  specified.
 */
struct PlaySoundDesc
{
    /** flags to describe the way to allocate the voice and play the sound.  This may be zero
     *  or more of the fPlayFlag* flags.
     */
    PlayFlags flags = 0;

    /** the sound data object to be played.  This may not be nullptr.  The sound's data buffer
     *  will provide the data for the voice.  By default, the whole sound will be played.  If
     *  a non-zero value for @ref playStart and @ref playLength are also given, only a portion
     *  of the sound's data will be played.
     */
    SoundData* sound;

    /** the set of voice parameters that are valid.  This can be @ref fVoiceParamAll if all of
     *  the parameters in the block are valid.  It may be a combination of other fVoiceParam*
     *  flags if only certain parameters are valid.  If set to 0, the parameters block in
     *  @ref params will be ignored and the default parameters used instead.
     */
    VoiceParamFlags validParams = 0;

    /** the initial parameters to set on the voice.  This may be nullptr to use the default
     *  parameters for the voice.  Note that the default parameters for spatial sounds will
     *  not necessarily be positioned appropriately in the world.  If needed, only a portion
     *  of the parameter block may be specified by listing only the flags for the valid parameters
     *  in @ref validParams.
     */
    VoiceParams* params = nullptr;

    /** descriptor of an optional loop point to set for the voice.  This may be changed at a
     *  later point on the voice with setLoopPoint() as well.  This will identify the region
     *  to be played after the initial play region from @ref playStart through @ref playLength
     *  completes, or after the previous loop completes.
     */
    LoopPointDesc loopPoint = {};

    /** the callback function to bind to the voice.  This provides notifications of the sound
     *  ending, loops starting on the sound, and event points getting hit in the sound.  This
     *  callback will occur during an update() call if the @ref fPlayFlagRealtimeCallbacks flag
     *  is not used in @ref flags.  This will occur on the engine thread if the flag is set.
     *  Note that if the callback is executing on the engine thread, it must complete its task
     *  as quickly as possible so that it doesn't stall the engine's processing operations.
     *  In most cases, it should just flag that the event occurred and return.  The flagged
     *  event should then be handled on another thread.  Blocking the engine thread can result
     *  in audio dropouts or popping artifacts in the stream.  In general, if a real-time
     *  callback takes more than ~1ms to execute, there is a very high possibility that it could
     *  cause an audio dropout.  This may be nullptr if no callbacks are needed.
     */
    VoiceCallback callback = nullptr;

    /** the opaque user context value that will be passed to the callback function any time
     *  it is called.  This value is ignored if @ref callback is nullptr.
     */
    void* callbackContext = nullptr;

    /** the offset in the sound to start the initial playback at.  The units of this value are
     *  specified in @ref playUnits.  If this is set to 0, playback will start from the beginning
     *  of the sound data object.  It is the caller's responsibility to ensure that this starting
     *  point occurs at a zero crossing in the data (otherwise a popping artifact will occur).
     *  This starting offset must be within the range of the sound data object's buffer.  This
     *  may be outside of any chosen loop region.  This region of the sound will be played before
     *  the loop region plays.  This initial play region (starting at this offset and extending
     *  through @ref playLength) does not count as one of the loop iterations.
     */
    size_t playStart = 0;

    /** the length of the initial play region.  The units of this value are specified in
     *  @ref playUnits.  This may be set to 0 to indicate that the remainder of the sound data
     *  object's buffer should be played starting from @ref playStart.  This length plus the
     *  starting offset must be within the range of the sound data object's buffer.
     */
    size_t playLength = 0;

    /** the units that the @ref playStart and @ref playLength values are given in.  This may be
     *  any unit, but the use of time units could result in undesirable artifacts since they are
     *  not accurate units.
     */
    UnitType playUnits = UnitType::eFrames;

    /** reserved for future expansion.  This must be set to `nullptr` unless it is pointing
     *  to a @ref PlaySoundDesc2 object.
     */
    void* ext = nullptr;
};

/** Extended descriptor to allow for further control over how a new voice plays its sound.  This
 *  must be set in @ref PlaySoundDesc::ext if it is to be included in the original play descriptor
 *  for the voice.
 */
struct PlaySoundDesc2
{
    /** The time in microseconds to delay the start of this voice's sound.  This will delay the
     *  start of the sound by an explicit amount requested by the caller.  This specified delay
     *  time will be in addition to the context's delay time (@ref ContextParams2::videoLatency)
     *  and the calculated distance delay.  This value may be negative to offset the context's
     *  delay time.  However, once all delay times have been combined (context, per-voice,
     *  distance delay), the total delay will be clamped to 0.  This defaults to 0us.
     */
    int64_t delayTime = 0;

    /** Extra padding space reserved for future expansion.  Do not use this value directly.
     *  In future versions, new context parameters will be borrowed from this buffer and given
     *  proper names and types as needed.  When this padding buffer is exhausted, a new
     *  @a PlaySoundDesc3 object will be added that can be chained to this one.
     */
    void* padding[32] = {};

    /** Reserved for future expansion.  This must be set to `nullptr`. */
    void* ext = nullptr;
};


/********************************** IAudioPlayback Interface **********************************/
/** Low-Level Audio Playback Plugin Interface.
 *
 *  See these pages for more detail:
 *  @rst
    * :ref:`carbonite-audio-label`
    * :ref:`carbonite-audio-playback-label`
    @endrst
 */
struct IAudioPlayback
{
    CARB_PLUGIN_INTERFACE("carb::audio::IAudioPlayback", 1, 1)

    /************************ context and device management functions ****************************/
    /** retrieves the current audio output device count for the system.
     *
     *  @return the number of audio output devices that are currently connected to the system.
     *          Device 0 in the list will be the system's default or preferred device.
     *
     *  @note The device count is a potentially volatile value.  This can change at any time
     *        without notice due to user action.  For example, the user could remove an audio
     *        device from the system or add a new one at any time.  Thus it is a good idea to
     *        open the device as quickly as possible after choosing the device index.  There
     *        is no guarantee that the device list will even remain stable during a single
     *        device enumeration loop.  The only device index that is guaranteed to be valid
     *        is the system default device index of 0.
     */
    size_t(CARB_ABI* getDeviceCount)();

    /** retrieves the capabilities and information about a single audio output device.
     *
     *  @param[in] deviceIndex  the index of the device to retrieve info for.  This must be
     *                          between 0 and the most recent return value from getDeviceCount().
     *  @param[out] caps    receives the device information.  The @a thisSize value must be set
     *                      to sizeof(DeviceCaps) before calling.
     *  @returns    @ref AudioResult::eOk if the device info was successfully retrieved.
     *  @returns    @ref AudioResult::eInvalidParameter if the @a thisSize value is not properly
     *              initialized in @p caps or @p caps is nullptr.
     *  @returns    @ref AudioResult::eOutOfRange if the requested device index is out of range of
     *              the system's current device count.
     *  @returns    @ref AudioResult::eNotSupported if a device is found but it requires an
     *              unsupported sample format.
     *  @returns    an AudioResult::* error code if the requested device index was out of range
     *              or the info buffer was invalid.
     *
     *  @remarks    This retrieves information about a single audio output device.  The
     *              information will be returned in the @p info buffer.  This may fail if the
     *              device corresponding to the requested index has been removed from the
     *              system.
     */
    AudioResult(CARB_ABI* getDeviceCaps)(size_t deviceIndex, DeviceCaps* caps);

    /** creates a new audio output context object.
     *
     *  @param[in] desc a description of the initial settings and capabilities to use for the
     *                  new context object.  Set this to nullptr to use all default context
     *                  settings.
     *  @returns the newly created audio output context object.
     *
     *  @remarks This creates a new audio output context object.  This object is responsible for
     *           managing all access to a single instance of the audio context.  Note that there
     *           will be a separate audio engine thread associated with each instance of this
     *           context object.
     *
     *  @remarks Upon creation, this object will be in a default state.  This means that the
     *           selected device will be opened and its processing engine created.  It will
     *           output at the chosen device's default frame rate and channel count.  If needed,
     *           a new device may be selected with setOutput().  If a custom speaker mask
     *           (ie: not one of the standard preset kSpeakerMode* modes) was specified when
     *           creating the context or a device or speaker mask with more than
     *           @ref Speaker::eCount channels was used, the caller must make a call to
     *           setSpeakerDirections() before any spatial sounds can be played.  Failing to do
     *           so will result in undefined behavior in the final audio output.
     *
     *  @note If selecting a device fails during context creation, the context will still be
     *        created successfully and be valid for future operations.  The caller will have
     *        to select another valid device at a later point before any audio will be output
     *        however.  A caller can check if a device has been opened successfully by calling
     *        getContextCaps() and checking the @a selectedDevice.flags member to see if it has
     *        been set to something other than @ref fDeviceFlagNotOpen.
     */
    Context*(CARB_ABI* createContext)(const PlaybackContextDesc* desc);

    /** destroys an audio output context object.
     *
     *  @param[in] context  the context object to be destroyed.  Upon return, this object will no
     *                      longer be valid.
     *  @retval AudioResult::eOk.
     *
     *  @remarks This destroys an audio output context object that was previously created
     *           with createContext().  If the context is still active and has a running mixer, it
     *           will be stopped before the object is destroyed.  All resources associated with
     *           the device will be both invalidated and destroyed as well.  Only audio data
     *           buffers that were queued on voices will be left (it is the caller's
     *           responsibility to destroy those).
     */
    AudioResult(CARB_ABI* destroyContext)(Context* context);

    /** retrieves the current capabilities and settings for a context object.
     *
     *  @param[in] context  the context object to retrieve the capabilities for.
     *  @returns the context's current capabilities and settings.  This includes the speaker mode,
     *           speaker positions, maximum bus count, and information about the output device
     *           that is opened (if any).
     *  @returns nullptr if @p context is nullptr.
     *
     *  @remarks This retrieves the current capabilities and settings for a context object.
     *           Some of these settings may change depending on whether the context has opened
     *           an output device or not.
     */
    const ContextCaps*(CARB_ABI* getContextCaps)(Context* context);

    /** sets one or more parameters on a context.
     *
     *  @param[in] context      the context to set the parameter(s) on.  This may not be nullptr.
     *  @param[in] paramsToSet  the set of flags to indicate which parameters in the parameter
     *                          block @p params are valid and should be set on the context.  This
     *                          may be zero or more of the fContextParam* flags.  If this is 0,
     *                          the call will be treated as a no-op.
     *  @param[in] params       the parameter(s) to be set on the context.  The flags indicating
     *                          which parameters need to be set are given in @p paramsToSet.
     *                          Undefined behavior may occur if a flag is set but its
     *                          corresponding value(s) have not been properly initialized.  This
     *                          may not be nullptr.
     *  @returns no return value.
     *
     *  @remarks This sets one or more context parameters in a single call.  Only parameters that
     *           have their corresponding flag set in @p paramsToSet will be modified.  If a
     *           change is to be relative to the context's current parameter value, the current
     *           value should be retrieved first, modified, then set.
     */
    void(CARB_ABI* setContextParameters)(Context* context, ContextParamFlags paramsToSet, const ContextParams* params);

    /** retrieves one or more parameters for a context.
     *
     *  @param[in] context      the context to retrieve parameters for.  This may not be nullptr.
     *  @param[in] paramsToGet  flags indicating which parameter values need to be retrieved.
     *  @param[out] params      receives the requested parameter values.  This may not be nullptr.
     *  @returns no return value.
     *
     *  @remarks This retrieves the current values of one or more of a context's parameters.  Only
     *           the parameter values listed in @p paramsToGet flags will be guaranteed to be
     *           valid upon return.
     */
    void(CARB_ABI* getContextParameters)(Context* context, ContextParamFlags paramsToGet, ContextParams* params);

    /** Change the maximum number of voices that have their output mixed to the audio device.
     *  @param[in] ctx     The context to modify.
     *  @param[in] count   The new number of buses to select.
     *                     This will be clamped to @ref PlaybackContextDesc::maxBuses,
     *                     if @p count exceeds it.
     *                     This cannot be set to 0.
     *
     *  @returns true if the max bus count was successfully changed.
     *  @returns false if any voices are still playing on buses.
     *  @returns false if the operation failed for any other reason (e.g. out of memory).
     *
     *  @note This call needs to stop the audio engine, so this will block for 10-20ms
     *        if any voices are still playing actively.
     *
     *  @note This should be called infrequently and only in cases where destroying
     *        the context is not an option.
     *
     *  @note To use this function without introducing audio artifacts, this
     *        function can only be called after all voices playing on buses
     *        have become silent or have stopped. The easiest way to ensure this
     *        is to set the master volume on the context to 0.0.
     *        At least 10ms must elapse between the volume reduction and this
     *        call to ensure that the changes have taken effect.
     *        If voices are being stopped, 20ms must occur between the last
     *        stopped voice and this call.
     */
    bool(CARB_ABI* setBusCount)(Context* ctx, size_t count);

    /** sets the [real biological] listener-relative position for all speakers.
     *
     *  @param[in] context  the context object to set the speaker positions for.  This may not be
     *                      nullptr.
     *  @param[in] desc     a descriptor of the new speaker directions to set for the context.
     *                      This may be nullptr to restore the default speaker directions for the
     *                      currently selected device if it supports a standard speaker mode
     *                      channel count.
     *  @returns true if the new speaker layout is successfully set.
     *  @returns false if the call fails or the given speaker mode doesn't match what the device
     *           was opened at.
     *
     *  @remarks This allows a custom user-relative direction to be set for all speakers.  Note
     *           that these represent the positions of the speakers in the real physical world
     *           not an entity in the simulated world.  A speaker direction is specified as a
     *           3-space vector with a left-to-right position, a front-to-back position, and a
     *           top-to-bottom position.  Each coordinate is expected to be within the range
     *           -1.0 (left, back, or bottom) to 1.0 (right, front, or top).  Each speaker
     *           direction is specified with these three explicit positions instead of an
     *           X/Y/Z vector to avoid confusion between various common coordinate systems
     *           (ie: graphics systems often treat negative Z as forward into the screen whereas
     *           audio systems often treat Z as up).
     *
     *  @remarks When a new device is selected with setOutput(), any previous custom speaker
     *           directions will be reset to the implicit positions of the new speaker mode.  The
     *           new speaker directions are defined on a cube where the origin (ie: the
     *           point (0, 0, 0)) represents the position of the user at the center of the
     *           cube.  All values should range from -1.0 to 1.0.  The given speaker direction
     *           vectors do not need to be normalized - this will be done internally.  The
     *           distance from the origin is not important and does not affect spatial sound
     *           calculations.  Only the speaker directions from the user are important.
     *
     *  @remarks These speaker directions are used to affect how spatial audio calculations map
     *           from the simulated world to the physical world.  They will affect the left-to-
     *           right, front-to-back, and top-to-bottom balancing of the different sound channels
     *           in each spatial sound calculation.
     *
     *  @note    For most applications, setting custom speaker directions is not necessary.  This
     *           would be necessary in situations that use custom speaker modes, situations
     *           where the expected speaker layout is drastically different from any of the
     *           standard speaker modes that may be chosen (ie: a car speaker system), or when
     *           the speaker count does not map to a standard kSpeakerMode* speaker mode.
     *
     *  @note    An output must have been successfully selected on the context before this call
     *           can succeed.  This can be checked by verifying that the context caps block's
     *           @a selectedDevice.flags member is not @ref fDeviceFlagNotOpen.
     */
    bool(CARB_ABI* setSpeakerDirections)(Context* context, const SpeakerDirectionDesc* desc);

    /** opens a requested audio output device and begins output.
     *
     *  @param[in] context  the context object that will have its device index set.  This is
     *                      returned from a previous call to createContext().  This must not
     *                      be nullptr.
     *  @param[in] desc     the descriptor of the output to open.  This may be nullptr to
     *                      use the default system output device.
     *  @retval AudioResult::eOk if the requested device is successfully opened.
     *  @retval AudioResult::eOutOfRange if the device index is invalid.
     *  @returns an AudioResult::* error code if the operation fails for any other reason.  See
     *           the notes below for more information on failures.
     *
     *  @remarks This sets the index of the audio output device that will be opened and attempts
     *           to open it.  An index of 0 will open the system's default audio output device.
     *           Note that the device index value is volatile and could change at any time due
     *           to user activity.  It is suggested that the device index be chosen as closely
     *           as possible to when the device will be opened.
     *
     *  @remarks This allows an existing audio context object to switch to using a different audio
     *           device for its output after creation without having to destroy and recreate the
     *           current state of the context.  Once a new device is selected, it will be
     *           available to start processing and outputting audio.  Note that switching audio
     *           devices dynamically may cause an audible pop to occur on both the old device and
     *           new device depending on the current state of the context.  To avoid this, all
     *           active voices could be muted or paused briefly during the device switch, then
     *           resumed or un-muted after it completes.
     *
     *  @note If selecting the new device fails, the context will be left without a device to
     *        play its output on.  Upon failure, an attempt to open the system's default device
     *        may be made by the caller to restore the playback.  Note that even choosing the
     *        system default device may fail for various reasons (ie: the speaker mode of the
     *        new device cannot be mapped properly, the device is no longer available in the
     *        system, etc).  In this case, all audio processing will still continue but the
     *        final audio data will just be dropped until a valid output target is connected.
     */
    AudioResult(CARB_ABI* setOutput)(Context* context, const OutputDesc* desc);

    /** performs required frequent updates for the audio context.
     *
     *  @param[in] context  the context object to perform update tasks on.
     *  @retval AudioResult::eOk if the update is performed successfully.
     *  @retval AudioResult::eDeviceDisconnected or @ref AudioResult::eDeviceLost if the
     *           currently selected device has been removed from the system.
     *  @retval AudioResult::eDeviceNotOpen if no device has been opened.
     *  @retval AudioResult::eInvalidParameter if an invalid context is passed in.
     *  @returns an AudioResult::* error code if the update fails for any other reason.
     *
     *  @remarks This performs common update tasks.  Updates need to be performed frequently.
     *           This will perform any pending callbacks and will ensure that all pending
     *           parameter changes have been updated in the engine context.  This should still
     *           be called periodically even if no object parameter changes occur.
     *
     *  @remarks All non-realtime voice callbacks will be performed during this call.  All
     *           device change callbacks on the context will also be performed here.  Failing
     *           to call this periodically may cause events to be lost.
     */
    AudioResult(CARB_ABI* update)(Context* context);

    /** starts the audio processing engine for a context.
     *
     *  @param[in] context  the context to start the processing engine for.  This must not be
     *                      nullptr.
     *  @returns no return value.
     *
     *  @remarks This starts the audio processing engine for a context.  When creating a playback
     *           context, the processing engine will always be automatically started.  When
     *           creating a baking context, the processing engine must always be started
     *           explicitly.  This allows the baking operation to be fully setup (ie: queue all
     *           sounds to be played, set all parameters, etc) before any audio is processed.
     *           This prevents the baked result from containing an indeterminate amount of silence
     *           at the start of its stream.
     *
     *  @remarks When using a playback context, this does not need to be called unless the engine
     *           is being restarted after calling stopProcessing().
     */
    void(CARB_ABI* startProcessing)(Context* context);

    /** stops the processing engine for a context.
     *
     *  @param[in] context  the context to stop the processing engine for.  This must not be
     *                      nullptr.
     *  @returns no return value.
     *
     *  @remarks This stops the audio processing engine for a context.  For a playback context,
     *           this is not necessary unless all processing needs to be halted for some reason.
     *           For a baking context, this is only necessary if the fContextFlagManualStop flag
     *           was used when creating the context.  If that flags is used, the processing
     *           engine will be automatically stopped any time it runs out of data to process.
     *
     *  @note Stopping the engine is not the same as pausing the output.  Stopping the engine
     *        will cause any open streamers to be closed and will likely cause an audible pop
     *        when restarting the engine with startProcessing().
     */
    void(CARB_ABI* stopProcessing)(Context* context);


    /********************************** sound management functions *******************************/
    /** schedules a sound to be played on a voice.
     *
     *  @param[in] context  the context to play the new sound on.  This must not be nullptr.
     *  @param[in] desc     the descriptor of the play task to be performed.  This may not be
     *                      nullptr.
     *  @returns a new voice handle representing the playing sound.  Note that if no buses are
     *           currently available to play on or the voice's initial parameters indicated that
     *           it is not currently audible, the voice will be virtual and will not be played.
     *           The voice handle will still be valid in this case and can be operated on, but
     *           no sound will be heard from it until it is determined that it should be converted
     *           to a real voice.  This can only occur when the update() function is called.
     *           This voice handle does not need to be closed or destroyed.  If the voice finishes
     *           its play task, any future calls attempting to modify the voice will simply fail.
     *  @returns nullptr if the requested sound is already at or above its instance limit and the
     *           @ref fPlayFlagMaxInstancesSimulate flag is not used.
     *  @returns nullptr if the play task was invalid or could not be started properly.  This can
     *           most often occur in the case of streaming sounds if the sound's original data
     *           could not be opened or decoded properly.
     *
     *  @remarks This schedules a sound object to be played on a voice.  The sounds current
     *           settings (ie: volume, pitch, playback frame rate, pan, etc) will be assigned to
     *           the voice as 'defaults' before playing.  Further changes can be made to the
     *           voice's state at a later time without affecting the sound's default settings.
     *
     *  @remarks Once the sound finishes playing, it will be implicitly unassigned from the
     *           voice.  If the sound or voice have a callback set, a notification will be
     *           received for the sound having ended.
     *
     *  @remarks If the playback of this sound needs to be stopped, it must be explicitly stopped
     *           from the returned voice object using stopVoice().  This can be called on a
     *           single voice or a voice group.
     */
    Voice*(CARB_ABI* playSound)(Context* context, const PlaySoundDesc* desc);

    /** stops playback on a voice.
     *
     *  @param[in] voice    the voice to stop.
     *  @returns no return value.
     *
     *  @remarks This stops a voice from playing its current sound.  This will be silently
     *           ignored for any voice that is already stopped or for an invalid voice handle.
     *           Once stopped, the voice will be returned to a 'free' state and its sound data
     *           object unassigned from it.  The voice will be immediately available to be
     *           assigned a new sound object to play from.
     *
     *  @note This will only schedule the voice to be stopped.  Its volume will be implicitly
     *        set to silence to avoid a popping artifact on stop.  The voice will continue to
     *        play for one more engine cycle until the volume level reaches zero, then the voice
     *        will be fully stopped and recycled.  At most, 1ms of additional audio will be
     *        played from the voice's sound.
     */
    void(CARB_ABI* stopVoice)(Voice* voice);


    /********************************** voice management functions *******************************/
    /** retrieves the sound (if any) that is currently active on a voice.
     *
     *  @param[in] voice    the voice to retrieve the active sound for.
     *  @returns the sound object that is currently playing on the requested voice.
     *  @returns nullptr if the voice is not currently playing any sound object.
     *  @returns nullptr if the given voice handle is no longer valid.
     *
     *  @remarks This retrieves the sound data object that is currently being processed on a
     *           voice.  This can be used to make a connection between a voice and the sound
     *           data object it is currently processing.
     *
     *  @note If @p voice ends, the returned sound will be freed in the next
     *        update() call.  It is the caller's responsibility to call
     *        IAudioData::acquire() if the object is going to be held until
     *        after that update() call.  It is also the caller's responsibility
     *        to ensure acquiring this reference is done in a thread safe
     *        manner with respect to the update() call.
     */
    SoundData*(CARB_ABI* getPlayingSound)(Voice* voice);

    /** checks the playing state of a voice.
     *
     *  @param[in] voice    the voice to check the playing state for.
     *  @returns true if the requested voice is playing.
     *  @returns false if the requested voice is not playing or is paused.
     *  @returns false if the given voice handle is no longer valid.
     *
     *  @remarks This checks if a voice is currently playing.  A voice is considered playing if
     *           it has a currently active sound data object assigned to it, it is not paused,
     *           and its play task has not completed yet.  This will start failing immediately
     *           upon the voice completing its play task instead of waiting for the voice to
     *           be recycled on the next update() call.
     */
    bool(CARB_ABI* isPlaying)(Voice* voice);

    /** sets a new loop point as current on a voice.
     *
     *  @param[in] voice    the voice to set the loop point on.  This may not be nullptr.
     *  @param[in] desc     descriptor of the new loop point to set.  This may contain a loop
     *                      or event point from the sound itself or an explicitly specified
     *                      loop point.  This may be nullptr to indicate that the current loop
     *                      point should be removed and the current loop broken.  Similarly,
     *                      an empty loop point descriptor could be passed in to remove the
     *                      current loop point.
     *  @returns true if the new loop point is successfully set.
     *  @returns false if the voice handle is invalid or the voice has already stopped on its own.
     *  @returns false if the new loop point is invalid, not found in the sound data object, or
     *           specifies a starting point or length that is outside the range of the sound data
     *           object's buffer.
     *
     *  @remarks This sets a new loop point for a playing voice.  This allows for behavior such
     *           as sound atlases or sound playlists to be played out on a single voice.  Ideally
     *           this should be called from a @ref VoiceCallbackType::eLoopPoint callback to
     *           ensure the new buffer is queued without allowing the voice to finish on its own
     *           and to prevent a potential race condition between the engine and the host app
     *           setting the new loop point.  Calling this in a real-time callback would guarantee
     *           no race would occur, however it could still be done safely in a non-real-time
     *           callback if the voice's current loop region is long enough and the update()
     *           function is called frequently enough.
     *
     *  @remarks This could also be called from a @ref VoiceCallbackType::eSoundEnded callback,
     *           but only if it is done in a real-time callback.  In a non-real-time callback
     *           the voice handle will already have been invalidated by the time the update()
     *           function performs the callback.
     *
     *  @remarks When @p desc is nullptr or the contents of the descriptor do not specify a new
     *           loop point, this will immediately break the loop that is currently playing on
     *           the voice.  This will have the effect of setting the voice's current loop count
     *           to zero.  The sound on the voice will continue to play out its current loop
     *           iteration, but will not loop again when it reaches its end.  This is useful for
     *           stopping a voice that is playing an infinite loop or to prematurely stop a voice
     *           that was set to loop a specific number of times.  This call will effectively be
     *           ignored if passed in a voice that is not currently looping.
     *
     *  @note For streaming voices, updating a loop point will have a delay due to buffering
     *        the decoded data. The sound will loop an extra time if the loop point is changed
     *        after the buffering has started to consume another loop. The default buffer time
     *        for streaming sounds is currently 200 milliseconds, so this is the minimum slack
     *        time that needs to be given for a loop change.  This means that changing
     *        a loop point in a @ref VoiceCallbackType::eLoopPoint callback will result in an
     *        extra loop occurring for a streaming sound.
     */
    bool(CARB_ABI* setLoopPoint)(Voice* voice, const LoopPointDesc* desc);

    /** retrieves the current play cursor position of a voice.
     *
     *  @param[in] voice    the voice to retrieve the play position for.
     *  @param[in] type     the units to retrieve the current position in.
     *  @returns the current position of the voice in the requested units.
     *  @returns 0 if the voice does not have a sound assigned to it.
     *  @returns the last play cursor position if the voice is paused.
     *
     *  @remarks This retrieves the current play position for a voice.  This is not necessarily
     *           the position in the buffer being played, but rather the position in the sound
     *           data object's stream.  For streaming sounds, this will be the offset from the
     *           start of the stream.  For non-streaming sounds, this will be the offset from
     *           the beginning of the sound data object's buffer.
     *
     *  @note If the loop point for the voice changes during playback, the results of this
     *        call can be unexpected.  Once the loop point changes, there is no longer a
     *        consistent time base for the voice and the results will reflect the current
     *        position based off of the original loop's time base.  As long as the voice's
     *        original loop point remains (ie: setLoopPoint() is never called on the voice),
     *        the calculated position should be correct.
     *
     *  @note It is the caller's responsibility to ensure that this is not called at the same
     *        time as changing the loop point on the voice or stopping the voice.
     */
    size_t(CARB_ABI* getPlayCursor)(Voice* voice, UnitType type);

    /** sets one or more parameters on a voice.
     *
     *  @param[in] voice        the voice to set the parameter(s) on.
     *  @param[in] paramsToSet  flags to indicate which of the parameters need to be updated.
     *                          This may be one or more of the fVoiceParam* flags.  If this is
     *                          0, this will simply be a no-op.
     *  @param[in] params       the parameter(s) to be set on the voice.  The flags indicating
     *                          which parameters need to be set must be set in
     *                          @p paramsToSet by the caller.  Undefined behavior
     *                          may occur if a flag is set but its corresponding value(s) have
     *                          not been properly initialized.  This may not be nullptr.
     *  @returns no return value.
     *
     *  @remarks This sets one or more voice parameters in a single call.
     *           Only parameters that have their corresponding flag set in @p
     *           paramsToSet will be modified.
     *           If a change is to be relative to the voice's current parameter value, the current
     *           value should be retrieved first, modified, then set.
     *
     *  @note only one of @ref fPlaybackModeSimulatePosition or
     *        @ref fPlaybackModeNoPositionSimulation can be set in the playback
     *        mode. If none or both of the flags are set, the previous value of
     *        those two bits will be used.
     */
    void(CARB_ABI* setVoiceParameters)(Voice* voice, VoiceParamFlags paramsToSet, const VoiceParams* params);

    /** retrieves one or more parameters for a voice.
     *
     *  @param[in] voice        the voice to retrieve parameters for.
     *  @param[in] paramsToGet  flags indicating which parameter values need to be retrieved.
     *  @param[out] params      receives the requested parameter values.  This may not be nullptr.
     *  @returns no return value.
     *
     *  @remarks This retrieves the current values of one or more of a voice's parameters.  Only
     *           the parameter values listed in @p paramsToGet flags will be guaranteed to be
     *           valid upon return.
     */
    void(CARB_ABI* getVoiceParameters)(Voice* voice, VoiceParamFlags paramsToGet, VoiceParams* params);

    /** stops playback on all voices in a given context.
     *
     *  @param[in] context     the context where to stop playback the voices.
     *
     *  @remarks The input context parameter can be nullptr, meaning to stop playback on all
     *           voices in all contexts in the context table.
     */
    void(CARB_ABI* stopAllVoices)(Context* context);
};

} // namespace audio
} // namespace carb
