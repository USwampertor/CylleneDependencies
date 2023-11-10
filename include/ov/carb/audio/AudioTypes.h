// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Data types used by the audio interfaces.
 */
#pragma once

#include "../assets/AssetsTypes.h"
#include "carb/Types.h"
#include "carb/extras/Guid.h"

namespace carb
{
namespace audio
{

/** represents a single audio context object.  This contains the state for a single instance of
 *  one of the low-level audio plugins.  This is to be treated as an opaque handle to an object
 *  and should only passed into the function of the plugin that created it.
 */
struct Context DOXYGEN_EMPTY_CLASS;

/** represents a single instance of a playing sound.  A single sound object may be playing on
 *  multiple voices at the same time, however each voice may only be playing a single sound
 *  at any given time.
 */
struct Voice DOXYGEN_EMPTY_CLASS;


/** various limits values for the audio system.
 *  @{
 */
constexpr size_t kMaxNameLength = 512; ///< maximum length of a device name in characters.
constexpr size_t kMaxChannels = 64; ///< maximum number of channels supported for output.
constexpr size_t kMinChannels = 1; ///< minimum number of channels supported for capture or output.
constexpr size_t kMaxFrameRate = 200000; ///< maximum frame rate of audio that can be processed.
constexpr size_t kMinFrameRate = 1000; ///< minimum frame rate of audio that can be processed.
/** @} */

/** description of how a size or offset value is defined.  This may be given as a  */
enum class UnitType : uint32_t
{
    eBytes, ///< the size or offset is given as a byte count.
    eFrames, ///< the size or offset is given as a frame count.
    eMilliseconds, ///< the size or offset is given as a time in milliseconds.
    eMicroseconds, ///< the size or offset is given as a time in microseconds.
};

/** possible return values from various audio APIs.  These indicate the kind of failure that
 *  occurred.
 */
enum class AudioResult
{
    eOk, ///< the operation was successful.
    eDeviceDisconnected, ///< the device was disconnected from the system.
    eDeviceLost, ///< access to the device was lost.
    eDeviceNotOpen, ///< the device has not been opened yet.
    eDeviceOpen, ///< the device has already been opened.
    eOutOfRange, ///< a requested parameter was out of range.
    eTryAgain, ///< the operation should be retried at a later time.
    eOutOfMemory, ///< the operation failed due to a lack of memory.
    eInvalidParameter, ///< an invalid parameter was passed in.
    eNotAllowed, ///< this operation is not allowed on the object type.
    eNotFound, ///< the resource requested, such as a file, was not found.
    eIoError, ///< an error occurred in an IO operation.
    eInvalidFormat, ///< the format of a resource was invalid.
    eOverrun, ///< An overrun occurred
    eNotSupported, ///< the resource or operation used is not supported.
};


/** speaker names.  Speakers are virtually located on the unit circle with the listener at the
 *  fSpeakerFlagFrontCenter.  Speaker angles are relative to the positive Y axis (ie: forward
 *  from the listener).  Angles increase in the clockwise direction.  The top channels are
 *  located on the unit sphere at an inclination of 45 degrees.
 *  The channel order of these speakers is represented by the ordering of speakers in this
 *  enum (e.g. eSideLeft is after eBackLeft).
 */
enum class Speaker
{
    eFrontLeft, ///< Front left speaker.  Usually located at -45 degrees. Also used for left headphone.
    eFrontRight, ///< Front right speaker.  Usually located at 45 degrees. Also used for right headphone.
    eFrontCenter, ///< Front center speaker.  Usually located at 0 degrees.
    eLowFrequencyEffect, ///< Low frequency effect speaker (subwoofer).  Usually treated as if it is located at the
                         ///< listener.
    eBackLeft, ///< Back left speaker.  Usually located at -135 degrees.
    eBackRight, ///< Back right speaker.  Usually located at 135 degrees.
    eBackCenter, ///< Back center speaker. Usually located at 180 degrees.
    eSideLeft, ///< Side left speaker.  Usually located at -90 degrees.
    eSideRight, ///< Side right speaker.  Usually located at 90 degrees.
    eTopFrontLeft, ///< Top front left speaker.  Usually located at -45 degrees and raised vertically.
    eTopFrontRight, ///< Top front right speaker.  Usually located at 45 degrees and raised vertically.
    eTopBackLeft, ///< Top back left speaker.  Usually located at -135 degrees and raised vertically.
    eTopBackRight, ///< Top back right speaker.  Usually located at 135 degrees and raised vertically.
    eFrontLeftWide, ///< Front left wide speaker. Usually located at -60 degrees.
    eFrontRightWide, ///< Front left wide speaker. Usually located at 60 degrees.
    eTopLeft, ///< Top left speaker. Usually located at -90 degrees and raised vertically.
    eTopRight, ///< Top right speaker. Usually located at 90 degrees and raised vertically.

    eCount, ///< Total number of named speakers.  This is not a valid speaker name.
};

/** the base type for a set of speaker flag masks.  This can be any combination of the
 *  fSpeakerFlag* speaker names, or one of the kSpeakerMode names.
 */
typedef uint64_t SpeakerMode;

/** @brief a conversion function from a Speaker enum to bitflags.
 *  @param[in] s The speaker enum to convert to a bitflag.
 *  @returns The corresponding speaker flag.
 */
constexpr SpeakerMode makeSpeakerFlag(Speaker s)
{
    return 1ull << static_cast<SpeakerMode>(s);
}

/** @brief a conversion function from a Speaker enum to bitflags.
 *  @param[in] s The speaker enum to convert to a bitflag.
 *  @returns The corresponding speaker flag.
 */
constexpr SpeakerMode makeSpeakerFlag(size_t s)
{
    return makeSpeakerFlag(static_cast<Speaker>(s));
}

/** Speaker bitflags that can be used to create speaker modes.
 *  @{
 */

/** @copydoc Speaker::eFrontLeft */
constexpr SpeakerMode fSpeakerFlagFrontLeft = makeSpeakerFlag(Speaker::eFrontLeft);

/** @copydoc Speaker::eFrontRight */
constexpr SpeakerMode fSpeakerFlagFrontRight = makeSpeakerFlag(Speaker::eFrontRight);

/** @copydoc Speaker::eFrontCenter */
constexpr SpeakerMode fSpeakerFlagFrontCenter = makeSpeakerFlag(Speaker::eFrontCenter);

/** @copydoc Speaker::eLowFrequencyEffect */
constexpr SpeakerMode fSpeakerFlagLowFrequencyEffect = makeSpeakerFlag(Speaker::eLowFrequencyEffect);

/** @copydoc Speaker::eSideLeft */
constexpr SpeakerMode fSpeakerFlagSideLeft = makeSpeakerFlag(Speaker::eSideLeft);

/** @copydoc Speaker::eSideRight */
constexpr SpeakerMode fSpeakerFlagSideRight = makeSpeakerFlag(Speaker::eSideRight);

/** @copydoc Speaker::eBackLeft */
constexpr SpeakerMode fSpeakerFlagBackLeft = makeSpeakerFlag(Speaker::eBackLeft);

/** @copydoc Speaker::eBackRight */
constexpr SpeakerMode fSpeakerFlagBackRight = makeSpeakerFlag(Speaker::eBackRight);

/** @copydoc Speaker::eBackCenter */
constexpr SpeakerMode fSpeakerFlagBackCenter = makeSpeakerFlag(Speaker::eBackCenter);

/** @copydoc Speaker::eTopFrontLeft */
constexpr SpeakerMode fSpeakerFlagTopFrontLeft = makeSpeakerFlag(Speaker::eTopFrontLeft);

/** @copydoc Speaker::eTopFrontRight */
constexpr SpeakerMode fSpeakerFlagTopFrontRight = makeSpeakerFlag(Speaker::eTopFrontRight);

/** @copydoc Speaker::eTopBackLeft */
constexpr SpeakerMode fSpeakerFlagTopBackLeft = makeSpeakerFlag(Speaker::eTopBackLeft);

/** @copydoc Speaker::eTopBackRight */
constexpr SpeakerMode fSpeakerFlagTopBackRight = makeSpeakerFlag(Speaker::eTopBackRight);

/** @copydoc Speaker::eFrontLeftWide */
constexpr SpeakerMode fSpeakerFlagFrontLeftWide = makeSpeakerFlag(Speaker::eFrontLeftWide);

/** @copydoc Speaker::eFrontRightWide */
constexpr SpeakerMode fSpeakerFlagFrontRightWide = makeSpeakerFlag(Speaker::eFrontRightWide);

/** @copydoc Speaker::eTopLeft */
constexpr SpeakerMode fSpeakerFlagTopLeft = makeSpeakerFlag(Speaker::eTopLeft);

/** @copydoc Speaker::eTopRight */
constexpr SpeakerMode fSpeakerFlagTopRight = makeSpeakerFlag(Speaker::eTopRight);
/** @} */

/** the special name for an invalid speaker.  Since a speaker mode could also include custom
 *  bits for unnamed speakers, there needs to be a way to represent failure conditions when
 *  converting between speaker flags and speaker names.
 */
constexpr size_t kInvalidSpeakerName = ~0ull;

/** common speaker layout modes.  These put together a specific set of channels that describe how
 *  a speaker mode is laid out around the listener.
 *  @{
 */
/** a special speaker mode that indicates that the audio device's preferred speaker mode
 *  should be used in the mixer.  The individual speaker positions may not be changed
 *  with setSpeakerDirections() when using this mode.
 */
constexpr SpeakerMode kSpeakerModeDefault = 0;

/** a mono speaker mode.  Only a single channel is supported.  The one speaker is often
 *  treated as being positioned at the fSpeakerFlagFrontCenter in front of the listener
 *  even though it is labeled as 'left'.
 */
constexpr SpeakerMode kSpeakerModeMono = fSpeakerFlagFrontLeft;

/** a stereo speaker mode.  This supports two channels.  These are usually located at
 *  -90 degrees and 90 degrees.
 */
constexpr SpeakerMode kSpeakerModeStereo = fSpeakerFlagFrontLeft | fSpeakerFlagFrontRight;

/** A three speaker mode. This has two front speakers and a low frequency effect speaker.
 *  The speakers are usually located at -45 and 45 degrees.
 */
constexpr SpeakerMode kSpeakerModeTwoPointOne =
    fSpeakerFlagFrontLeft | fSpeakerFlagFrontRight | fSpeakerFlagLowFrequencyEffect;

/** a four speaker mode.  This has two front speakers and two side or back speakers.  The
 *  speakers are usually located at -45, 45, -135, and 135 degrees around the listener.
 */
constexpr SpeakerMode kSpeakerModeQuad =
    fSpeakerFlagFrontLeft | fSpeakerFlagFrontRight | fSpeakerFlagBackLeft | fSpeakerFlagBackRight;

/** a five speaker mode.  This has two front speakers and two side or back speakers
 *  and a low frequency effect speaker.  The speakers are usually located at -45, 45,
 *  -135, and 135 degrees around the listener.
 */
constexpr SpeakerMode kSpeakerModeFourPointOne = fSpeakerFlagFrontLeft | fSpeakerFlagFrontRight | fSpeakerFlagBackLeft |
                                                 fSpeakerFlagBackRight | fSpeakerFlagLowFrequencyEffect;

/** a six speaker mode.  This represents a standard 5.1 home theater setup.  Speakers are
 *  usually located at -45, 45, 0, 0, -135, and 135 degrees.
 */
constexpr SpeakerMode kSpeakerModeFivePointOne = fSpeakerFlagFrontLeft | fSpeakerFlagFrontRight |
                                                 fSpeakerFlagFrontCenter | fSpeakerFlagLowFrequencyEffect |
                                                 fSpeakerFlagBackLeft | fSpeakerFlagBackRight;

/** a seven speaker mode. This is an non-standard speaker layout.
 *  Speakers in this layout are located at -45, 45, 0, 0, -90, 90 and 180 degrees.
 */
constexpr SpeakerMode kSpeakerModeSixPointOne = fSpeakerFlagFrontLeft | fSpeakerFlagFrontRight |
                                                fSpeakerFlagFrontCenter | fSpeakerFlagLowFrequencyEffect |
                                                fSpeakerFlagBackCenter | fSpeakerFlagSideLeft | fSpeakerFlagSideRight;

/** an eight speaker mode.  This represents a standard 7.1 home theater setup.  Speakers are
 *  usually located at -45, 45, 0, 0, -90, 90, -135, and 135 degrees.
 */
constexpr SpeakerMode kSpeakerModeSevenPointOne =
    fSpeakerFlagFrontLeft | fSpeakerFlagFrontRight | fSpeakerFlagFrontCenter | fSpeakerFlagLowFrequencyEffect |
    fSpeakerFlagSideLeft | fSpeakerFlagSideRight | fSpeakerFlagBackLeft | fSpeakerFlagBackRight;

/** a ten speaker mode.  This represents a standard 9.1 home theater setup.  Speakers are
 *  usually located at -45, 45, 0, 0, -90, 90, -135, 135, -60 and 60 degrees.
 */
constexpr SpeakerMode kSpeakerModeNinePointOne =
    fSpeakerFlagFrontLeft | fSpeakerFlagFrontRight | fSpeakerFlagFrontCenter | fSpeakerFlagLowFrequencyEffect |
    fSpeakerFlagSideLeft | fSpeakerFlagSideRight | fSpeakerFlagBackLeft | fSpeakerFlagBackRight |
    fSpeakerFlagFrontLeftWide | fSpeakerFlagFrontRightWide;

/** a twelve speaker mode.  This represents a standard 7.1.4 home theater setup.  The lower
 *  speakers are usually located at -45, 45, 0, 0, -90, 90, -135, and 135 degrees.  The upper
 *  speakers are usually located at -45, 45, -135, and 135 at an inclination of 45 degrees.
 */
constexpr SpeakerMode kSpeakerModeSevenPointOnePointFour =
    fSpeakerFlagFrontLeft | fSpeakerFlagFrontRight | fSpeakerFlagFrontCenter | fSpeakerFlagLowFrequencyEffect |
    fSpeakerFlagSideLeft | fSpeakerFlagSideRight | fSpeakerFlagBackLeft | fSpeakerFlagBackRight |
    fSpeakerFlagTopFrontLeft | fSpeakerFlagTopFrontRight | fSpeakerFlagTopBackLeft | fSpeakerFlagTopBackRight;

/** a fourteen speaker mode.  This represents a standard 9.1.4 home theater setup.  The lower
 *  speakers are usually located at -45, 45, 0, 0, -90, 90, -135, 135, -60 and 60 degrees.  The upper
 *  speakers are usually located at -45, 45, -135, and 135 at an inclination of 45 degrees.
 */
constexpr SpeakerMode kSpeakerModeNinePointOnePointFour =
    fSpeakerFlagFrontLeft | fSpeakerFlagFrontRight | fSpeakerFlagFrontCenter | fSpeakerFlagLowFrequencyEffect |
    fSpeakerFlagSideLeft | fSpeakerFlagSideRight | fSpeakerFlagBackLeft | fSpeakerFlagBackRight |
    fSpeakerFlagFrontLeftWide | fSpeakerFlagFrontRightWide | fSpeakerFlagTopFrontLeft | fSpeakerFlagTopFrontRight |
    fSpeakerFlagTopBackLeft | fSpeakerFlagTopBackRight;

/** a sixteen speaker mode.  This represents a standard 9.1.6 home theater setup.  The lower
 *  speakers are usually located at -45, 45, 0, 0, -90, 90, -135, 135, -60 and 60 degrees.  The upper
 *  speakers are usually located at -45, 45, -135, 135, -90 and 90 degrees at an inclination of 45 degrees.
 */
constexpr SpeakerMode kSpeakerModeNinePointOnePointSix =
    fSpeakerFlagFrontLeft | fSpeakerFlagFrontRight | fSpeakerFlagFrontCenter | fSpeakerFlagLowFrequencyEffect |
    fSpeakerFlagSideLeft | fSpeakerFlagSideRight | fSpeakerFlagBackLeft | fSpeakerFlagBackRight |
    fSpeakerFlagFrontLeftWide | fSpeakerFlagFrontRightWide | fSpeakerFlagTopFrontLeft | fSpeakerFlagTopFrontRight |
    fSpeakerFlagTopBackLeft | fSpeakerFlagTopBackRight | fSpeakerFlagTopLeft | fSpeakerFlagTopRight;

/** A linear surround setup.
 *  This is the 3 channel layout in formats using Vorbis channel order.
 */
constexpr SpeakerMode kSpeakerModeThreePointZero =
    fSpeakerFlagFrontLeft | fSpeakerFlagFrontRight | fSpeakerFlagFrontCenter;

/** @ref kSpeakerModeFivePointOne without the low frequency effect speaker.
 *  This is used as the 5 channel layout in formats using Vorbis channel order.
 */
constexpr SpeakerMode kSpeakerModeFivePointZero = fSpeakerFlagFrontLeft | fSpeakerFlagFrontRight |
                                                  fSpeakerFlagFrontCenter | fSpeakerFlagBackLeft | fSpeakerFlagBackRight;

/** the total number of 'standard' speaker modes represented here.  Other custom speaker modes
 *  are still possible however by combining the fSpeakerFlag* names in different ways.
 */
constexpr size_t kSpeakerModeCount = 7;

/** All valid speaker mode bits.
 */
constexpr SpeakerMode fSpeakerModeValidBits =
    fSpeakerFlagFrontLeft | fSpeakerFlagFrontRight | fSpeakerFlagFrontCenter | fSpeakerFlagLowFrequencyEffect |
    fSpeakerFlagSideLeft | fSpeakerFlagSideRight | fSpeakerFlagBackLeft | fSpeakerFlagBackRight |
    fSpeakerFlagFrontLeftWide | fSpeakerFlagFrontRightWide | fSpeakerFlagTopFrontLeft | fSpeakerFlagTopFrontRight |
    fSpeakerFlagTopBackLeft | fSpeakerFlagTopBackRight | fSpeakerFlagTopLeft | fSpeakerFlagTopRight;
/** @} */


/** flags to indicate the current state of a device in the system.  This may be any combination
 *  of the fDeviceFlag* flags.
 */
typedef uint32_t DeviceFlags;

/** flags to indicate the current state of a device.  These are used in the @a flags member of
 *  the @ref DeviceCaps struct.
 *  @{
 */
constexpr DeviceFlags fDeviceFlagNotOpen = 0x00000000; ///< no device is currently open.
constexpr DeviceFlags fDeviceFlagConnected = 0x00000001; ///< the device is currently connected to the system.
constexpr DeviceFlags fDeviceFlagDefault = 0x00000002; ///< the device is the system default or preferred device.
constexpr DeviceFlags fDeviceFlagStreamer = 0x00000004; ///< a streamer is being used as an output.
/** @} */


/** prototype for the optional destructor function for a user data object.
 *
 *  @param[in] userData     the user data object to be destroyed.  This will never be nullptr.
 *  @returns no return value.
 *
 *  @remarks This destroys the user data object associated with an object.  The parent object may
 *           be a sound data object or sound group, but is irrelevant here since it is not passed
 *           into this destructor.  This destructor is optional.  If specified, it will be called
 *           any time the user data object is replaced with a setUserData() function or when the
 *           containing object itself is being destroyed.
 */
typedef void(CARB_ABI* UserDataDestructor)(void* userData);

/** an opaque user data object that can be attached to some objects (ie: sound data objects, sound
 *  groups, etc).
 */
struct UserData
{
    /** the opaque user data pointer associated with this entry.  The caller is responsible for
     *  creating this object and ensuring its contents are valid.
     */
    void* data = nullptr;

    /** the optional destructor that will be used to clean up the user data object whenever it is
     *  replaced or the object containing this user data object is destroyed.  This may be nullptr
     *  if no clean up is needed for the user data object.  It is the host app's responsibility
     *  to ensure that either this destructor is provided or that the user data object is manually
     *  cleaned up before anything it is attached to is destroyed.
     */
    UserDataDestructor destructor = nullptr;
};

/** the data type for a single sample of raw audio data.  This describes how each sample in the
 *  data buffer should be interpreted.  In general, audio data can only be uncompressed Pulse
 *  Code Modulation (PCM) data, or encoded in some kind of compressed format.
 */
enum class SampleFormat : uint32_t
{
    /** 8 bits per sample unsigned integer PCM data.  Sample values will range from 0 to 255
     *  with a value of 128 being 'silence'.
     */
    ePcm8,

    /** 16 bits per sample signed integer PCM data.  Sample values will range from -32768 to
     *  32767 with a value of 0 being 'silence'.
     */
    ePcm16,

    /** 24 bits per sample signed integer PCM data.  Sample values will range from -16777216
     *  to 16777215 with a value of 0 being 'silence'.
     */
    ePcm24,

    /** 32 bits per sample signed integer PCM data.  Sample values will range from -2147483648
     *  to 2147483647 with a value of 0 being 'silence'.
     */
    ePcm32,

    /** 32 bits per sample floating point PCM data.  Sample values will range from -1.0 to 1.0
     *  with a value of 0.0 being 'silence'.  Note that floating point samples can extend out
     *  of their range (-1.0 to 1.0) without a problem during mixing.  However, once the data
     *  reaches the device, any samples beyond the range from -1.0 to 1.0 will clip and cause
     *  distortion artifacts.
     */
    ePcmFloat,

    /** the total number of PCM formats.  This is not a valid format and is only used internally
     *  to determine how many PCM formats are available.
     */
    ePcmCount,

    /** The Vorbis codec.
     *  Vorbis is a lossy compressed codec that is capable of producing high
     *  quality audio that is difficult to differentiate from lossless codecs.
     *  Vorbis is suitable for music and other applications that require
     *  minimal quality loss.
     *  Vorbis is stored in Ogg file containers (.ogg or .oga).
     *  Vorbis has a variable block size, with a maximum of 8192 frames per
     *  block, which makes it non-optimal for low latency audio transfer (e.g.
     *  voice chat); additionally, the Ogg container combines Vorbis blocks
     *  into chunks that can be seconds long.
     *  libvorbis will accept frame rates of 1Hz - 200KHz (Note that IAudioPlayback
     *  does not supports framerates below @ref kMinFrameRate).
     *  Vorbis is able to handle up to 255 channels, but sounds with more than 8
     *  channels have no official ordering. (Note that does not support more than @ref kMaxChannels)
     *
     *  Vorbis has a defined channel mapping for audio with 1-8 channels.
     *  Channel counts 3 and 5 have an incompatible speaker layout with the
     *  default layouts in this plugin.
     *  A 3 channel layout uses @ref kSpeakerModeThreePointZero,
     *  A 5 channel layout uses @ref kSpeakerModeFivePointZero
     *  For streams with more than 8 channels, the mapping is undefined and
     *  must be determined by the application.
     *
     *  These are the results of decoding speed tests run on Vorbis; they are
     *  shown as the decoding time relative to decoding a 16 bit uncompressed
     *  WAVE file to @ref SampleFormat::ePcm32. Clip 1 and 2 are stereo music.
     *  Clip 3 is a mono voice recording.  Clip 1 has low inter-channel
     *  correlation; Clip 2 has high inter-channel correlation.
     *  Note that the bitrates listed here are approximate, since Vorbis is
     *  variable bitrate.
     *    - clip 1, 0.0 quality (64kb/s):   668%
     *    - clip 1, 0.4 quality (128kb/s):  856%
     *    - clip 1, 0.9 quality (320kb/s): 1333%
     *    - clip 2, 0.0 quality (64kb/s):   660%
     *    - clip 2, 0.4 quality (128kb/s):  806%
     *    - clip 2, 0.9 quality (320kb/s): 1286%
     *    - clip 3, 0.0 quality (64kb/s):   682%
     *    - clip 3, 0.4 quality (128kb/s):  841%
     *    - clip 3, 0.9 quality (320kb/s): 1074%
     *
     *  These are the file sizes from the previous tests:
     *    - clip 1, uncompressed:          32.7MiB
     *    - clip 1, 0.0 quality (64kb/s):   1.5MiB
     *    - clip 1, 0.4 quality (128kb/s):  3.0MiB
     *    - clip 1, 0.9 quality (320kb/s):  7.5MiB
     *    - clip 2, uncompressed:          49.6MiB
     *    - clip 2, 0.0 quality (64kb/s):   2.0MiB
     *    - clip 2, 0.4 quality (128kb/s):  4.0MiB
     *    - clip 2, 0.9 quality (320kb/s): 10.4MiB
     *    - clip 3, uncompressed:           9.0MiB
     *    - clip 3, 0.0 quality (64kb/s):   0.9MiB
     *    - clip 3, 0.4 quality (128kb/s):  1.4MiB
     *    - clip 3, 0.9 quality (320kb/s):  2.5MiB
     */
    eVorbis,

    /** The Free Lossless Audio Codec.
     *  This is a codec capable of moderate compression with a perfect
     *  reproduction of the original uncompressed signal.
     *  This encodes and decodes reasonable fast, but the file size is much
     *  larger than the size of a high quality lossy codec.
     *  This is suitable in applications where audio data will be repeatedly
     *  encoded, such as an audio editor. Unlike a lossy codec, repeatedly
     *  encoding the file with FLAC will not degrade the quality.
     *  FLAC is very fast to encode and decode compared to other compressed codecs.
     *  Note that FLAC only stores integer data, so audio of type
     *  SampleFormat::ePcmFloat will lose precision when stored as FLAC.
     *  Additionally, the FLAC encoder used only supports up to 24 bit, so
     *  SampleFormat::ePcm32 will lose some precision when being stored if there
     *  are more than 24 valid bits per sample.
     *  FLAC supports frame rates from 1Hz - 655350Hz (Note that IAudioPlayback
     *  only support framerates of @ref kMinFrameRate to @ref kMaxFrameRate).
     *  FLAC supports up to 8 channels.
     *
     *  These are the results of decoding speed tests run on FLAC; they are
     *  shown as the decoding time relative to decoding a 16 bit uncompressed
     *  WAVE file to @ref SampleFormat::ePcm32. These are the same clips as
     *  used in the decoding speed test for @ref SampleFormat::eVorbis.
     *  has high inter-channel correlation.
     *    - clip 1, compression level 0: 446%
     *    - clip 1, compression level 5: 512%
     *    - clip 1, compression level 8: 541%
     *    - clip 2, compression level 0: 321%
     *    - clip 2, compression level 5: 354%
     *    - clip 2, compression level 8: 388%
     *    - clip 3, compression level 0: 262%
     *    - clip 3, compression level 5: 303%
     *    - clip 3, compression level 8: 338%
     *
     *  These are the file sizes from the previous tests:
     *    - clip 1, uncompressed:        32.7MiB
     *    - clip 1, compression level 0: 25.7MiB
     *    - clip 1, compression level 5: 23.7MiB
     *    - clip 1, compression level 8: 23.4MiB
     *    - clip 2, uncompressed:        49.6MiB
     *    - clip 2, compression level 0: 33.1MiB
     *    - clip 2, compression level 5: 26.8MiB
     *    - clip 2, compression level 8: 26.3MiB
     *    - clip 3, uncompressed:         9.0MiB
     *    - clip 3, compression level 0:  6.2MiB
     *    - clip 3, compression level 5:  6.1MiB
     *    - clip 3, compression level 8:  6.0MiB
     *
     *  @note Before encoding FLAC sounds with unusual framerates, please read
     *        the documentation for @ref FlacEncoderSettings::streamableSubset.
     */
    eFlac,

    /** The Opus codec.
     *  This is a lossy codec that is designed to be suitable for almost any
     *  application.
     *  Opus can encode very high quality lossy audio, similarly to @ref
     *  SampleFormat::eVorbis.
     *  Opus can encode very low bitrate audio at a much higher quality than
     *  @ref SampleFormat::eVorbis.
     *  Opus also offers much lower bitrates than @ref SampleFormat::eVorbis.
     *  Opus is designed for low latency usage, with a minimum latency of 5ms
     *  and a block size configurable between 2.5ms and 60ms.
     *  Opus also offers forward error correction to handle packet loss during
     *  transmission.
     *
     *  Opus is stored in Ogg file containers (.ogg or .oga), but in use cases
     *  such as network transmission, Ogg containers are not necessary.
     *  Opus only supports sample rates of 48000Hz, 24000Hz, 16000Hz, 12000Hz and 8000Hz.
     *  Passing unsupported frame rates below 48KHz to the encoder will result
     *  in the input audio being resampled to the next highest supported frame
     *  rate.
     *  Passing frame rates above 48KHz to the encoder will result in the input
     *  audio being resampled down to 48KHz.
     *  The 'Opus Custom' format, which removes this frame rate restriction, is
     *  not supported.
     *
     *  Opus has a defined channel mapping for audio with 1-8 channels.
     *  The channel mapping is identical to that of @ref SampleFormat::eVorbis.
     *  For streams with more than 8 channels, the mapping is undefined and
     *  must be determined by the application.
     *  Up to 255 audio channels are supported.
     *
     *  Opus has three modes of operation: a linear predictive coding (LPC)
     *  mode, a modified discrete cosine transform (MCDT) mode and a hybrid
     *  mode which combines both the LPC and MCDT mode.
     *  The LPC mode is optimized to encode voice data at low bitrates and has
     *  the ability to use forward error correction and packet loss compensation.
     *  The MCDT mode is suitable for general purpose audio and is optimized
     *  for minimal latency.
     *
     *  Because Opus uses a fixed number of frames per block, additional
     *  padding will be added when encoding with a @ref CodecState, unless the
     *  frame size is specified in advance with @ref OpusEncoderSettings::frames.
     *
     *  These are the results of decoding speed tests run on Opus; they are
     *  shown as the decoding time relative to decoding a 16 bit uncompressed
     *  WAVE file to @ref SampleFormat::ePcm32. Clip 1 and 2 are stereo music.
     *  Clip 3 is a mono voice recording.  Clip 1 has low inter-channel
     *  correlation; Clip 2 has high inter-channel correlation.
     *  Note that the bitrates listed here are approximate, since Opus is
     *  variable bitrate.
     *    - clip 1, 64kb/s:   975%
     *    - clip 1, 128kb/s: 1181%
     *    - clip 1, 320kb/s: 2293%
     *    - clip 2, 64kb/s:   780%
     *    - clip 2, 128kb/s: 1092%
     *    - clip 2, 320kb/s: 2376%
     *    - clip 3, 64kb/s:   850%
     *    - clip 3, 128kb/s:  997%
     *    - clip 3, 320kb/s: 1820%
     *
     *  These are the file sizes from the previous tests:
     *    - clip 1, uncompressed: 32.7MiB
     *    - clip 1, 64kb/s:        1.5MiB
     *    - clip 1, 128kb/s:       3.0MiB
     *    - clip 1, 320kb/s:       7.5MiB
     *    - clip 2, uncompressed: 49.6MiB
     *    - clip 2, 64kb/s:        2.3MiB
     *    - clip 2, 128kb/s:       4.6MiB
     *    - clip 2, 320kb/s:      11.3MiB
     *    - clip 3, uncompressed:  9.0MiB
     *    - clip 3, 64kb/s:        1.7MiB
     *    - clip 3, 128kb/s:       3.3MiB
     *    - clip 3, 320kb/s:       6.7MiB
     */
    eOpus,

    /** MPEG audio layer 3 audio encoding.
     *  This is currently supported for decoding only; to compress audio with a
     *  lossy algorithm, @ref SampleFormat::eVorbis or @ref SampleFormat::eOpus
     *  should be used.
     *
     *  The MP3 decoder currently only has experimental support for seeking;
     *  files encoded by LAME seem to seek with frame-accurate precision, but
     *  results may vary on other encoders.
     *  It is recommended to load the file with @ref fDataFlagDecode,
     *  if you intend to use frame-accurate loops.
     *
     *  MP3 is faster to decode than @ref SampleFormat::eVorbis and @ref
     *  SampleFormat::eOpus.
     *  This is particularly noticeable because MP3's decoding speed is not
     *  affected as significantly by increasing bitrates as @ref
     *  SampleFormat::eVorbis and @ref SampleFormat::eOpus.
     *  The quality degradation of MP3 with low bitrates is much more severe
     *  than with @ref SampleFormat::eVorbis and @ref SampleFormat::eOpus, so
     *  this difference in performance is not as severe as it may appear.
     *  The following are the times needed to decode a sample file that is
     *  about 10 minutes long:
     *  | encoding:       | time (seconds): |
     *  |-----------------|-----------------|
     *  | 45kb/s MP3      | 0.780           |
     *  | 64kb/s MP3      | 0.777           |
     *  | 128kb/s MP3     | 0.904           |
     *  | 320kb/s MP3     | 1.033           |
     *  | 45kb/s Vorbis   | 1.096           |
     *  | 64kb/s Vorbis   | 1.162           |
     *  | 128kb/s Vorbis  | 1.355           |
     *  | 320kb/s Vorbis  | 2.059           |
     *  | 45kb/s Opus     | 1.478           |
     *  | 64kb/s Opus     | 1.647           |
     *  | 128kb/s Opus    | 2.124           |
     *  | 320kb/s Opus    | 2.766           |
     */
    eMp3,

    /** the data is in an unspecified compressed format.  Being able to interpret the data in
     *  the sound requires extra information on the caller's part.
     */
    eRaw,

    /** the default or preferred sample format for a device.  This format name is only valid when
     *  selecting a device or decoding data.
     */
    eDefault,

    /** the number of supported sample formats.  This is not a valid format and is only used
     *  internally to determine how many formats are available.
     */
    eCount,
};

/** provides information about the format of a sound.  This is used both when creating the sound
 *  and when retrieving information about its format.  When a sound is loaded from a file, its
 *  format will be implicitly set on load.  The actual format can then be retrieved later with
 *  getSoundFormat().
 */
struct SoundFormat
{
    /** the number of channels of data in each frame of the audio data. */
    size_t channels;

    /** the number of bits per sample of the audio data.  This is also encoded in the @ref format
     *  value, but it is given here as well for ease of use in calculations.  This represents the
     *  number of bits in the decoded samples of the sound stream.
     *  This will be 0 for variable bitrate compressed formats.
     */
    size_t bitsPerSample;

    /** the size in bytes of each frame of data in the format.  A frame consists of one sample
     *  per channel.  This represents the size of a single frame of decoded data from the sound
     *  stream.
     *  This will be 0 for variable bitrate compressed formats.
     */
    size_t frameSize;

    /** The size in bytes of a single 'block' of encoded data.
     *  For PCM data, this is the same as a frame.
     *  For formats with a fixed bitrate, this is the size of a single unit of
     *  data that can be decoded.
     *  For formats with a variable bitrate, this will be 0.
     *  Note that certain codecs can be fixed or variable bitrate depending on
     *  the encoder settings.
     */
    size_t blockSize;

    /** The number of frames that will be decoded from a single block of data.
     *  For PCM formats, this will be 1.
     *  For formats with a fixed number of frames per block, this will be
     *  number of frames of data that will be produced when decoding a single
     *  block of data. Note that variable bitrate formats can have a fixed
     *  number of frames per block.
     *  For formats with a variable number of frames per block, this will be 0.
     *  Note that certain codecs can have a fixed or variable number of frames
     *  per block depending on the encoder settings.
     */
    size_t framesPerBlock;

    /** the number of frames per second that must be played back for the audio data to sound
     *  'normal' (ie: the way it was recorded or produced).
     */
    size_t frameRate;

    /** the channel mask for the audio data.  This specifies which speakers the stream is intended
     *  for and will be a combination of one or more of the @ref Speaker names or a
     *  @ref SpeakerMode name.  This may be calculated from the number of channels present in the
     *  original audio data or it may be explicitly specified in the original audio data on load.
     */
    SpeakerMode channelMask;

    /** the number of bits of valid data that are present in the audio data.  This may be used to
     *  specify that (for example) a stream of 24-bit sample data is being processed in 32-bit
     *  containers.  Each sample will actually consist of 32-bit data in the buffer, using the
     *  full 32-bit range, but only the top 24 bits of each sample will be valid useful data.
     *  This represents the valid number of bits per sample in the decoded data for the sound
     *  stream.
     */
    size_t validBitsPerSample;

    /** the format of each sample of audio data.  This is given as a symbolic name so
     *  that the data can be interpreted properly.  The size of each sample in bits is also
     *  given in the @ref bitsPerSample value.
     */
    SampleFormat format = SampleFormat::eDefault;
};

/** special value for @ref DeviceCaps::index to indicate that a real audio device is not currently
 *  selected for output.  When this value is present, a streamer output is in use instead.  This
 *  value will only ever be set on the DeviceCaps object returned in the result of the
 *  IAudioPlayback::getContextCaps() function.
 */
constexpr size_t kInvalidDeviceIndex = ~0ull;

/** contains information about a single audio input or output device.  This information can be
 *  retrieved with IAudioPlayback::getDeviceCaps() or IAudioCapture::getDeviceCaps().
 *  Note that this information should not be stored since it can change at any time due to user
 *  activity (ie: unplugging a device, plugging in a new device, changing system default devices,
 *  etc).  Device information should only be queried just before deciding which device to select.
 */
struct DeviceCaps
{
    /** indicates the size of this object to allow for versioning and future expansion.  This
     *  must be set to sizeof(DeviceCaps) before calling getDeviceCaps().
     */
    size_t thisSize = sizeof(DeviceCaps);

    /** the current index of this device in the enumeration order.  Note that this value is highly
     *  volatile and can change at any time due to user action (ie: plugging in or removing a
     *  device from the system).  When a device is added to or removed from the system, the
     *  information for the device at this index may change.  It is the caller's responsibility
     *  to refresh its collected device information if the device list changes.  The device at
     *  index 0 will always be considered the system's 'default' device.
     */
    size_t index;

    /** flags to indicate some attributes about this device.  These may change at any time due
     *  to user action (ie: unplugging a device or switching system defaults).  This may be 0
     *  or any combination of the fDeviceFlag* flags.
     */
    DeviceFlags flags;

    /** a UTF-8 string that describes the name of the audio device.  This will most often be a
     *  'friendly' name for the device that is suitable for display to the user.  This cannot
     *  be guaranteed for all devices or platforms since its contents are defined by the device
     *  driver.  The string will always be null terminated and may have been truncated if it
     *  was too long.
     */
    char name[kMaxNameLength];

    /** a GUID that can be used to uniquely identify the device.  The GUID for a given device
     *  may not be the same from one process to the next, or if the device is removed from the
     *  system and reattached.  The GUID will remain constant for the entire time the device
     *  is connected to the system however.
     */
    carb::extras::Guid guid;

    /** the preferred number of channels of data in each frame of the audio data.  Selecting
     *  a device using a different format than this will result in extra processing overhead
     *  due to the format conversion.
     */
    size_t channels;

    /** the preferred number of frames per second that must be played back for the audio
     *  data to sound 'normal' (ie: the way it was recorded or produced).  Selecting a
     *  device using a different frame rate than this will result in extra processing
     *  overhead due to the frame rate conversion.
     */
    size_t frameRate;

    /** the preferred format of each sample of audio data.  This is given as a symbolic name so
     *  that the data can be interpreted properly.  Selecting a device using a different format
     *  than this will result in extra processing overhead due to the format conversion.
     */
    SampleFormat format = SampleFormat::eDefault;
};

/** various default values for the audio system.
 *  @{
 */
constexpr size_t kDefaultFrameRate = 48000; ///< default frame rate.
constexpr size_t kDefaultChannelCount = 1; ///< default channel count.
constexpr SampleFormat kDefaultFormat = SampleFormat::ePcmFloat; ///< default sample format.
/** @} */

/** An estimate of the time in microseconds below which many users cannot perceive a
 *  synchronization issue between a sound and the visual it should be emitted from.
 *  There are definitely some users that can tell there is a problem with audio/visual
 *  sync timing close to this value, but they may not be able to say which direction
 *  the sync issue goes (ie: audio first vs visual event first).
 */
constexpr int64_t kImperceptibleDelay = 200000;

} // namespace audio
} // namespace carb
