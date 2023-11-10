// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Inline utility functions for audio processing.
 */
#pragma once

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#    define _USE_MATH_DEFINES
#endif

#include "../Framework.h"
#include "../logging/Log.h"
#include "../math/Util.h"
#include "AudioTypes.h"
#include "IAudioData.h"
#include "IAudioPlayback.h"
#include "IAudioUtils.h"
#include "IAudioCapture.h"

#include <atomic>
#include <limits.h>
#include <math.h>
#include <string.h>

#if CARB_PLATFORM_WINDOWS
#    define strdup _strdup
#endif

namespace carb
{
namespace audio
{

/** converts an angle in degrees to an angle in radians.
 *
 *  @param[in] degrees  the angle in degrees to be converted.
 *  @returns the requested angle in radians.
 */
template <typename T>
constexpr float degreesToRadians(T degrees)
{
    return degrees * (float(M_PI) / 180.f);
}

/** converts an angle in degrees to an angle in radians.
 *
 *  @param[in] degrees  the angle in degrees to be converted.
 *  @returns the requested angle in radians.
 */
constexpr double degreesToRadians(double degrees)
{
    return degrees * (M_PI / 180.0);
}

/** converts an angle in radians to an angle in degrees.
 *
 *  @param[in] radians  the angle in radians to be converted.
 *  @returns the requested angle in degrees.
 */
template <typename T>
constexpr float radiansToDegrees(T radians)
{
    return (radians * (180.f / float(M_PI)));
}

/** converts an angle in radians to an angle in degrees.
 *
 *  @param[in] radians  the angle in radians to be converted.
 *  @returns the requested angle in degrees.
 */
constexpr double radiansToDegrees(double radians)
{
    return (radians * (180.0 / M_PI));
}

/** counts the number of set bits in a bit flag set.
 *
 *  @param[in] value_   the value to count set bits in.
 *  @returns the number of set bits in the given value.
 */
template <typename T>
size_t getSetBitCount(T value_)
{
    return math::popCount(value_);
}

/** Retrieves the total number of speakers for a given speaker mode.
 *
 *  @param[in] mode     the speaker mode to retrieve the speaker count for.
 *  @returns the number of speakers expected for the requested speaker mode.
 *  @returns 0 if an unknown speaker count is passed in.
 *  @returns 0 if @ref kSpeakerModeDefault is passed in.
 */
constexpr size_t getSpeakerCountForMode(SpeakerMode mode)
{
    switch (mode)
    {
        case kSpeakerModeDefault:
            return 0;

        case kSpeakerModeMono:
            return 1;

        case kSpeakerModeStereo:
            return 2;

        case kSpeakerModeQuad:
            return 4;

        case kSpeakerModeFourPointOne:
            return 5;

        case kSpeakerModeFivePointOne:
            return 6;

        case kSpeakerModeSixPointOne:
            return 7;

        case kSpeakerModeSevenPointOne:
            return 8;

        case kSpeakerModeNinePointOne:
            return 10;

        case kSpeakerModeSevenPointOnePointFour:
            return 12;

        case kSpeakerModeNinePointOnePointFour:
            return 14;

        case kSpeakerModeNinePointOnePointSix:
            return 16;

        default:
            return getSetBitCount(mode);
    }
}

/** retrieves a default speaker mode for a given channel count.
 *  @param[in] channels     the number of channels to get the default speaker mode for.
 *  @returns a standard speaker mode with the requested channel count.
 *  @retval kSpeakerModeDefault if no standard speaker mode is defined for the given channel
 *           count.
 */
constexpr SpeakerMode getSpeakerModeForCount(size_t channels)
{
    switch (channels)
    {
        case 1:
            return kSpeakerModeMono;

        case 2:
            return kSpeakerModeStereo;

        case 3:
            return kSpeakerModeTwoPointOne;

        case 4:
            return kSpeakerModeQuad;

        case 5:
            return kSpeakerModeFourPointOne;

        case 6:
            return kSpeakerModeFivePointOne;

        case 7:
            return kSpeakerModeSixPointOne;

        case 8:
            return kSpeakerModeSevenPointOne;

        case 10:
            return kSpeakerModeNinePointOne;

        case 12:
            return kSpeakerModeSevenPointOnePointFour;

        case 14:
            return kSpeakerModeNinePointOnePointFour;

        case 16:
            return kSpeakerModeNinePointOnePointSix;

        default:
            return kSpeakerModeDefault;
    }
}

/** calculates a set of speaker flags for a channel count.
 *
 *  @param[in] channels     the number of channels to calculate the speaker flags for.  This
 *                          should be less than or equal to @ref kMaxChannels.
 *  @returns a set of speaker flags as a SpeakerMode value representing the number of channels
 *           that was requested.  Note that this will not necessarily be a standard speaker
 *           mode layout for the given channel count.  This should only be used in cases where
 *           getSpeakerModeForCount() returns @ref kSpeakerModeDefault and a speaker mode value
 *           other than kSpeakerModeDefault is strictly needed.
 */
constexpr SpeakerMode getSpeakerFlagsForCount(size_t channels)
{
    if (channels >= kMaxChannels)
        return 0xffffffffffffffffull;

    return (1ull << channels) - 1;
}

/** retrieves a speaker name from a single speaker mode flag.
 *
 *  @param[in] flag     a single speaker flag to convert to a speaker name.  This must be one of
 *                      the fSpeakerFlag* speaker flags.
 *  @returns one of the @ref Speaker names if converted successfully.
 *  @retval Speaker::eCount if an invalid speaker flag is passed in.
 */
constexpr Speaker getSpeakerFromSpeakerFlag(SpeakerMode flag)
{
    switch (flag)
    {
        case fSpeakerFlagFrontLeft:
            return Speaker::eFrontLeft;

        case fSpeakerFlagFrontRight:
            return Speaker::eFrontRight;

        case fSpeakerFlagFrontCenter:
            return Speaker::eFrontCenter;

        case fSpeakerFlagLowFrequencyEffect:
            return Speaker::eLowFrequencyEffect;

        case fSpeakerFlagSideLeft:
            return Speaker::eSideLeft;

        case fSpeakerFlagSideRight:
            return Speaker::eSideRight;

        case fSpeakerFlagBackLeft:
            return Speaker::eBackLeft;

        case fSpeakerFlagBackRight:
            return Speaker::eBackRight;

        case fSpeakerFlagBackCenter:
            return Speaker::eBackCenter;

        case fSpeakerFlagTopFrontLeft:
            return Speaker::eTopFrontLeft;

        case fSpeakerFlagTopFrontRight:
            return Speaker::eTopFrontRight;

        case fSpeakerFlagTopBackLeft:
            return Speaker::eTopBackLeft;

        case fSpeakerFlagTopBackRight:
            return Speaker::eTopBackRight;

        case fSpeakerFlagFrontLeftWide:
            return Speaker::eFrontLeftWide;

        case fSpeakerFlagFrontRightWide:
            return Speaker::eFrontRightWide;

        case fSpeakerFlagTopLeft:
            return Speaker::eTopLeft;

        case fSpeakerFlagTopRight:
            return Speaker::eTopRight;

        default:
            return Speaker::eCount;
    }
}

/** retrieves an indexed speaker name from a speaker mode mask.
 *
 *  @param[in] channelMask  the channel mask to retrieve one of the speaker names from.  This must
 *                          be a combination of one or more of the fSpeakerFlag* flags.
 *  @param[in] index        the zero based index of the speaker name to retrieve.  This indicates
 *                          which of the set speaker bits in the channel mask will be converted
 *                          and returned.
 *  @returns the index of the speaker name of the @p index-th speaker set in the given channel
 *           mask.  This may be cast to a @ref Speaker name if it is less than
 *           @ref Speaker::eCount.  If it is greater than or equal to @ref Speaker::eCount, this
 *           would represent a custom unnamed speaker in the channel mask.  This would be the
 *           index of the channel's sample in each frame of output data for the given channel
 *           mask.
 *  @retval kInvalidSpeakerName if the index is out of range of the number of speakers in
 *           the given channel mask.
 */
constexpr size_t getSpeakerFromSpeakerMode(SpeakerMode channelMask, size_t index)
{
    // no bits set in the channel mask -> nothing to do => fail.
    if (channelMask == 0)
        return kInvalidSpeakerName;

    SpeakerMode bit = 1;
    size_t i = 0;

    // walk through the channel mask searching for set bits.
    for (; bit != 0; bit <<= 1, i++)
    {
        // no speaker set for this bit => skip it.
        if ((channelMask & bit) == 0)
            continue;

        if (index == 0)
            return i;

        index--;
    }

    return kInvalidSpeakerName;
}

/**
 *  retrieves the number of bits per channel for a given sample format.
 *
 *  @param[in] fmt  the sample format to retrieve the bit count for.  This may be any of the
 *                  SampleFormat::ePcm* formats.  There is no defined bit count for the raw
 *                  and default formats.
 *  @returns the number of bits per sample associated with the requested sample format.
 */
constexpr size_t sampleFormatToBitsPerSample(SampleFormat fmt)
{
    switch (fmt)
    {
        case SampleFormat::ePcm8:
            return 8;

        case SampleFormat::ePcm16:
            return 16;

        case SampleFormat::ePcm24:
            return 24;

        case SampleFormat::ePcm32:
            return 32;

        case SampleFormat::ePcmFloat:
            return 32;

        default:
            return 0;
    }
}

/** converts a bits per sample count to an integer PCM sample format.
 *
 *  @param[in] bps  the bits per sample to convert.
 *  @returns the integer PCM sample format that corresponds to the requested bit count.
 *  @retval SampleFormat::eCount if no supported sample format matches the requested
 *           bit count.
 */
constexpr SampleFormat bitsPerSampleToIntegerPcmSampleFormat(size_t bps)
{
    switch (bps)
    {
        case 8:
            return SampleFormat::ePcm8;

        case 16:
            return SampleFormat::ePcm16;

        case 24:
            return SampleFormat::ePcm24;

        case 32:
            return SampleFormat::ePcm32;

        default:
            return SampleFormat::eCount;
    }
}

/**
 *  converts a time in milliseconds to a frame count.
 *
 *  @param[in] timeInMilliseconds   the time in milliseconds to be converted to a frame count.
 *  @param[in] frameRate    the frame rate of the audio that needs a frame count calculated.
 *  @returns the minimum number of frames required to cover the requested number of milliseconds
 *           at the requested frame rate.  Note that if the time isn't exactly divisible by
 *           the frame rate, a partial frame may be truncated.
 */
constexpr size_t millisecondsToFrames(size_t timeInMilliseconds, size_t frameRate)
{
    return (frameRate * timeInMilliseconds) / 1000;
}

/**
 *  converts a time in microseconds to a frame count.
 *
 *  @param[in] timeInMicroseconds   the time in microseconds to be converted to a frame count.
 *  @param[in] frameRate    the frame rate of the audio that needs a frame count calculated.
 *  @returns the minimum number of frames required to cover the requested number of microseconds
 *           at the requested frame rate.  Note that if the time isn't exactly divisible by
 *           the frame rate, a partial frame may be truncated.
 */
constexpr size_t microsecondsToFrames(size_t timeInMicroseconds, size_t frameRate)
{
    return (frameRate * timeInMicroseconds) / 1000000;
}

/**
 *  converts a time in milliseconds to a frame count.
 *
 *  @param[in] timeInMilliseconds   the time in milliseconds to be converted to a frame count.
 *  @param[in] format               the format information block for the sound data this time is
 *                                  being converted for.
 *  @returns the minimum number of frames required to cover the requested number of milliseconds
 *           at the requested frame rate.  Note that if the time isn't exactly divisible by
 *           the frame rate, a partial frame may be truncated.
 */
inline size_t millisecondsToFrames(size_t timeInMilliseconds, const SoundFormat* format)
{
    return millisecondsToFrames(timeInMilliseconds, format->frameRate);
}

/**
 *  converts a time in microseconds to a frame count.
 *
 *  @param[in] timeInMicroseconds   the time in microseconds to be converted to a frame count.
 *  @param[in] format               the format information block for the sound data this time is
 *                                  being converted for.
 *  @returns the minimum number of frames required to cover the requested number of microseconds
 *           at the requested frame rate.  Note that if the time isn't exactly divisible by
 *           the frame rate, a partial frame may be truncated.
 */
inline size_t microsecondsToFrames(size_t timeInMicroseconds, const SoundFormat* format)
{
    return microsecondsToFrames(timeInMicroseconds, format->frameRate);
}

/**
 *  converts a time in milliseconds to a byte count.
 *
 *  @param[in] timeInMilliseconds   the time in milliseconds to be converted to a frame count.
 *  @param[in] frameRate    the frame rate of the audio that needs a frame count calculated.
 *  @param[in] channels     the number of channels in the audio data format.
 *  @param[in] bps          the number of bits per sample of audio data.  This must be 8, 16, 24,
 *                          or 32.  This does not properly handle byte offset calculations for
 *                          compressed audio formats.
 *  @returns the approximate number of bytes of audio data required to fill the requested number
 *           of milliseconds.  Note that this will not be an exact value because the data format
 *           may not divide evenly into the requested number of milliseconds.
 */
constexpr size_t millisecondsToBytes(size_t timeInMilliseconds, size_t frameRate, size_t channels, size_t bps)
{
    return (timeInMilliseconds * frameRate * channels * bps) / (1000 * CHAR_BIT);
}

/**
 *  converts a time in microseconds to a byte count.
 *
 *  @param[in] timeInMicroseconds   the time in microseconds to be converted to a frame count.
 *  @param[in] frameRate    the frame rate of the audio that needs a frame count calculated.
 *  @param[in] channels     the number of channels in the audio data format.
 *  @param[in] bps          the number of bits per sample of audio data.  This must be 8, 16, 24,
 *                          or 32.  This does not properly handle byte offset calculations for
 *                          compressed audio formats.
 *  @returns the approximate number of bytes of audio data required to fill the requested number
 *           of microseconds.  Note that this will not be an exact value because the data format
 *           may not divide evenly into the requested number of microseconds.
 */
constexpr size_t microsecondsToBytes(size_t timeInMicroseconds, size_t frameRate, size_t channels, size_t bps)
{
    return (timeInMicroseconds * frameRate * channels * bps) / (1000000 * CHAR_BIT);
}

/**
 *  converts a time in milliseconds to a byte count.
 *
 *  @param[in] timeInMilliseconds   the time in milliseconds to be converted to a frame count.
 *  @param[in] frameRate    the frame rate of the audio that needs a frame count calculated.
 *  @param[in] channels     the number of channels in the audio data format.
 *  @param[in] format       the sample format for the data.  This must be a PCM sample format.
 *  @returns the approximate number of bytes of audio data required to fill the requested number
 *           of milliseconds.  Note that this will not be an exact value because the data format
 *           may not divide evenly into the requested number of milliseconds.
 */
constexpr size_t millisecondsToBytes(size_t timeInMilliseconds, size_t frameRate, size_t channels, SampleFormat format)
{
    return millisecondsToBytes(timeInMilliseconds, frameRate, channels, sampleFormatToBitsPerSample(format));
}

/**
 *  converts a time in microseconds to a byte count.
 *
 *  @param[in] timeInMicroseconds   the time in microseconds to be converted to a frame count.
 *  @param[in] frameRate    the frame rate of the audio that needs a frame count calculated.
 *  @param[in] channels     the number of channels in the audio data format.
 *  @param[in] format       the sample format for the data.  This must be a PCM sample format.
 *  @returns the approximate number of bytes of audio data required to fill the requested number
 *           of microseconds.  Note that this will not be an exact value because the data format
 *           may not divide evenly into the requested number of microseconds.
 */
constexpr size_t microsecondsToBytes(size_t timeInMicroseconds, size_t frameRate, size_t channels, SampleFormat format)
{
    return microsecondsToBytes(timeInMicroseconds, frameRate, channels, sampleFormatToBitsPerSample(format));
}

/**
 *  converts a time in milliseconds to a byte count.
 *
 *  @param[in] timeInMilliseconds   the time in milliseconds to be converted to a frame count.
 *  @param[in] format               the format information block for the sound data this time is
 *                                  being converted for.
 *  @returns the approximate number of bytes of audio data required to fill the requested number
 *           of milliseconds.  Note that this will not be an exact value because the data format
 *           may not divide evenly into the requested number of milliseconds.
 */
inline size_t millisecondsToBytes(size_t timeInMilliseconds, const SoundFormat* format)
{
    return millisecondsToBytes(timeInMilliseconds, format->frameRate, format->channels, format->bitsPerSample);
}

/**
 *  converts a time in microseconds to a byte count.
 *
 *  @param[in] timeInMicroseconds   the time in microseconds to be converted to a frame count.
 *  @param[in] format               the format information block for the sound data this time is
 *                                  being converted for.
 *  @returns the approximate number of bytes of audio data required to fill the requested number
 *           of microseconds.  Note that this will not be an exact value because the data format
 *           may not divide evenly into the requested number of microseconds.
 */
inline size_t microsecondsToBytes(size_t timeInMicroseconds, const SoundFormat* format)
{
    return microsecondsToBytes(timeInMicroseconds, format->frameRate, format->channels, format->bitsPerSample);
}

/**
 *  converts a frame count at a given frame rate to a time in milliseconds.
 *
 *  @param[in] frames       the frame count to be converted.
 *  @param[in] frameRate    the frame rate of the audio that needs a time calculated.
 *  @returns the time in milliseconds associated with the given number of frames playing at the
 *           requested frame rate.
 */
constexpr size_t framesToMilliseconds(size_t frames, size_t frameRate)
{
    return (frames * 1000) / frameRate;
}

/**
 *  converts a frame count at a given frame rate to a time in microseconds.
 *
 *  @param[in] frames       the frame count to be converted.
 *  @param[in] frameRate    the frame rate of the audio that needs a time calculated.
 *  @returns the time in microseconds associated with the given number of frames playing at the
 *           requested frame rate.
 */
constexpr size_t framesToMicroseconds(size_t frames, size_t frameRate)
{
    return (frames * 1000000) / frameRate;
}

/**
 *  converts a frame count at a given frame rate to a time in milliseconds.
 *
 *  @param[in] frames   the frame count to be converted.
 *  @param[in] format   the format information block for the sound data this time is being
 *                      converted for.
 *  @returns the time in milliseconds associated with the given number of frames playing at the
 *           requested frame rate.
 */
inline size_t framesToMilliseconds(size_t frames, const SoundFormat* format)
{
    return framesToMilliseconds(frames, format->frameRate);
}

/**
 *  converts a frame count at a given frame rate to a time in microseconds.
 *
 *  @param[in] frames   the frame count to be converted.
 *  @param[in] format   the format information block for the sound data this time is being
 *                      converted for.
 *  @returns the time in microseconds associated with the given number of frames playing at the
 *           requested frame rate.
 */
inline size_t framesToMicroseconds(size_t frames, const SoundFormat* format)
{
    return framesToMicroseconds(frames, format->frameRate);
}

/**
 *  converts a frame count to a byte offset.
 *
 *  @param[in] frames   the frame count to be converted to a byte count.
 *  @param[in] channels the number of channels in the audio data format.
 *  @param[in] bps      the number of bits per sample of audio data.  This must be 8, 16, 24, or
 *                      32.  This does not properly handle byte offset calculations for compressed
 *                      audio formats.
 *  @returns    the calculated byte offset to the start of the requested frame of audio data.
 */
constexpr size_t framesToBytes(size_t frames, size_t channels, size_t bps)
{
    return (frames * channels * bps) / CHAR_BIT;
}

/**
 *  converts a frame count to a byte offset.
 *
 *  @param[in] frames   the frame count to be converted to a byte count.
 *  @param[in] channels the number of channels in the audio data format.
 *  @param[in] format   the sample format of the data.  This must be a PCM sample format.
 *  @returns    the calculated byte offset to the start of the requested frame of audio data.
 */
constexpr size_t framesToBytes(size_t frames, size_t channels, SampleFormat format)
{
    return framesToBytes(frames, channels, sampleFormatToBitsPerSample(format));
}

/**
 *  converts a frame count to a byte offset.
 *
 *  @param[in] frames   the frame count to be converted to a byte count.
 *  @param[in] format   the format information block for the sound data this time is being
 *                      converted for.
 *  @returns    the calculated byte offset to the start of the requested frame of audio data.
 */
inline size_t framesToBytes(size_t frames, const SoundFormat* format)
{
    return framesToBytes(frames, format->channels, format->bitsPerSample);
}

/**
 *  converts a byte count to a frame count.
 *
 *  @param[in] bytes    the number of bytes to be converted to a frame count.  Note that this byte
 *                      count is expected to be frame aligned.  If it is not frame aligned, the
 *                      return value will be the offset for the frame that includes the requested
 *                      byte offset.
 *  @param[in] channels the number of channels in the audio data format.
 *                      This may not be 0.
 *  @param[in] bps      the number of bits per sample of audio data.  This must be 8, 16, 24, or
 *                      32.  This does not properly handle byte offset calculations for compressed
 *                      audio formats.
 *                      This may not be 0.
 *  @returns    the calculated frame offset that will contain the requested byte offset.
 */
constexpr size_t bytesToFrames(size_t bytes, size_t channels, size_t bps)
{
    return (bytes * CHAR_BIT) / (channels * bps);
}

/**
 *  converts a byte count to a frame count.
 *
 *  @param[in] bytes    the number of bytes to be converted to a frame count.  Note that this byte
 *                      count is expected to be frame aligned.  If it is not frame aligned, the
 *                      return value will be the offset for the frame that includes the requested
 *                      byte offset.
 *  @param[in] channels the number of channels in the audio data format.
 *  @param[in] format   the sample format of the data.  This must be a PCM sample format.
 *  @returns    the calculated frame offset that will contain the requested byte offset.
 */
constexpr size_t bytesToFrames(size_t bytes, size_t channels, SampleFormat format)
{
    size_t bps = sampleFormatToBitsPerSample(format);
    if (bps == 0)
    {
        CARB_LOG_ERROR("attempting to convert bytes to frames in a variable bitrate format (%d), return 0", int(format));
        return 0;
    }

    return bytesToFrames(bytes, channels, bps);
}

/**
 *  converts a byte count to a frame count.
 *
 *  @param[in] bytes    the number of bytes to be converted to a frame count.  Note that this byte
 *                      count is expected to be frame aligned.  If it is not frame aligned, the
 *                      return value will be the offset for the frame that includes the requested
 *                      byte offset.
 *  @param[in] format   the format information block for the sound data this time is being
 *                      converted for.
 *                      This must be a PCM sample format.
 *  @returns    the calculated frame offset that will contain the requested byte offset.
 */
inline size_t bytesToFrames(size_t bytes, const SoundFormat* format)
{
    if (format->bitsPerSample == 0)
    {
        CARB_LOG_ERROR(
            "attempting to convert bytes to frames in a variable bitrate format (%d), return 0", int(format->format));
        return 0;
    }

    return bytesToFrames(bytes, format->channels, format->bitsPerSample);
}

/**
 *  converts a byte count to an approximate time in milliseconds.
 *
 *  @param[in] bytes    the number of bytes to be converted to a time in milliseconds.  Note that
 *                      this byte count is expected to be frame aligned.
 *  @param[in] frameRate    the frame rate of the audio that needs a time calculated.
 *  @param[in] channels the number of channels in the audio data format.
 *  @param[in] bps      the number of bits per sample of audio data.  This must be 8, 16, 24, or
 *                      32.  This does not properly handle byte offset calculations for compressed
 *                      audio formats.
 *  @returns the approximate number of milliseconds of audio data that the requested byte count
 *           represents for the given format.
 */
constexpr size_t bytesToMilliseconds(size_t bytes, size_t frameRate, size_t channels, size_t bps)
{
    return (bytesToFrames(bytes * 1000, channels, bps)) / frameRate;
}

/**
 *  converts a byte count to an approximate time in microseconds.
 *
 *  @param[in] bytes    the number of bytes to be converted to a time in microseconds.  Note that
 *                      this byte count is expected to be frame aligned.
 *  @param[in] frameRate    the frame rate of the audio that needs a time calculated.
 *  @param[in] channels the number of channels in the audio data format.
 *  @param[in] bps      the number of bits per sample of audio data.  This must be 8, 16, 24, or
 *                      32.  This does not properly handle byte offset calculations for compressed
 *                      audio formats.
 *  @returns the approximate number of microseconds of audio data that the requested byte count
 *           represents for the given format.
 */
constexpr size_t bytesToMicroseconds(size_t bytes, size_t frameRate, size_t channels, size_t bps)
{
    return bytesToFrames(bytes * 1000000, channels, bps) / frameRate;
}

/**
 *  converts a byte count to an approximate time in milliseconds.
 *
 *  @param[in] bytes    the number of bytes to be converted to a time in milliseconds.  Note that
 *                      this byte count is expected to be frame aligned.
 *  @param[in] frameRate    the frame rate of the audio that needs a time calculated.
 *  @param[in] channels the number of channels in the audio data format.
 *  @param[in] format   the sample format of the data.  This must be a PCM sample format.
 *  @returns the approximate number of milliseconds of audio data that the requested byte count
 *           represents for the given format.
 */
constexpr size_t bytesToMilliseconds(size_t bytes, size_t frameRate, size_t channels, SampleFormat format)
{
    return bytesToMilliseconds(bytes, frameRate, channels, sampleFormatToBitsPerSample(format));
}

/**
 *  converts a byte count to an approximate time in microseconds.
 *
 *  @param[in] bytes    the number of bytes to be converted to a time in microseconds.  Note that
 *                      this byte count is expected to be frame aligned.
 *  @param[in] frameRate    the frame rate of the audio that needs a time calculated.
 *  @param[in] channels the number of channels in the audio data format.
 *  @param[in] format   the sample format of the data.  This must be a PCM sample format.
 *  @returns the approximate number of microseconds of audio data that the requested byte count
 *           represents for the given format.
 */
constexpr size_t bytesToMicroseconds(size_t bytes, size_t frameRate, size_t channels, SampleFormat format)
{
    return bytesToMicroseconds(bytes, frameRate, channels, sampleFormatToBitsPerSample(format));
}

/**
 *  converts a byte count to an approximate time in milliseconds.
 *
 *  @param[in] bytes    the number of bytes to be converted to a time in milliseconds.  Note that
 *                      this byte count is expected to be frame aligned.
 *  @param[in] format   the format information block for the sound data this time is being
 *                      converted for.
 *  @returns the approximate number of milliseconds of audio data that the requested byte count
 *           represents for the given format.
 */
inline size_t bytesToMilliseconds(size_t bytes, const SoundFormat* format)
{
    return bytesToMilliseconds(bytes, format->frameRate, format->channels, format->bitsPerSample);
}

/**
 *  converts a byte count to an approximate time in microseconds.
 *
 *  @param[in] bytes    the number of bytes to be converted to a time in microseconds.  Note that
 *                      this byte count is expected to be frame aligned.
 *  @param[in] format   the format information block for the sound data this time is being
 *                      converted for.
 *  @returns the approximate number of microseconds of audio data that the requested byte count
 *           represents for the given format.
 */
inline size_t bytesToMicroseconds(size_t bytes, const SoundFormat* format)
{
    return bytesToMicroseconds(bytes, format->frameRate, format->channels, format->bitsPerSample);
}

/**
 *  converts an input value from one unit to another.
 *
 *  @param[in] input        the input value to be converted.
 *  @param[in] inputUnits   the units to convert the @p input value from.
 *  @param[in] outputUnits  the units to convert the @p input value to.
 *  @param[in] format       the format information for the sound that the input value is being
 *                          converted for.  This may not be nullptr.
 *  @returns the converted value in the requested output units.
 *  @returns 0 if an invalid input or output unit value was given.
 */
inline size_t convertUnits(size_t input, UnitType inputUnits, UnitType outputUnits, const SoundFormat* format)
{
    CARB_ASSERT(format != nullptr);

    switch (inputUnits)
    {
        case UnitType::eBytes:
            switch (outputUnits)
            {
                case UnitType::eBytes:
                    return input;

                case UnitType::eFrames:
                    return bytesToFrames(input, format);

                case UnitType::eMilliseconds:
                    return bytesToMilliseconds(input, format);

                case UnitType::eMicroseconds:
                    return bytesToMicroseconds(input, format);

                default:
                    break;
            }

            break;

        case UnitType::eFrames:
            switch (outputUnits)
            {
                case UnitType::eBytes:
                    return framesToBytes(input, format);

                case UnitType::eFrames:
                    return input;

                case UnitType::eMilliseconds:
                    return framesToMilliseconds(input, format);

                case UnitType::eMicroseconds:
                    return framesToMicroseconds(input, format);

                default:
                    break;
            }

            break;

        case UnitType::eMilliseconds:
            switch (outputUnits)
            {
                case UnitType::eBytes:
                    return millisecondsToBytes(input, format);

                case UnitType::eFrames:
                    return millisecondsToFrames(input, format);

                case UnitType::eMilliseconds:
                    return input;

                case UnitType::eMicroseconds:
                    return input * 1000;

                default:
                    break;
            }

            break;

        case UnitType::eMicroseconds:
            switch (outputUnits)
            {
                case UnitType::eBytes:
                    return microsecondsToBytes(input, format);

                case UnitType::eFrames:
                    return microsecondsToFrames(input, format);

                case UnitType::eMilliseconds:
                    return input / 1000;

                case UnitType::eMicroseconds:
                    return input;

                default:
                    break;
            }

            break;

        default:
            break;
    }

    return 0;
}

/**
 *  aligns a byte count to a frame boundary for an audio data format.
 *
 *  @param[in] bytes    the byte count to align to a frame boundary.  This will be aligned to the
 *                      next higher frame boundary if it is not already aligned.
 *  @param[in] channels the number of channels in the audio data format.
 *  @param[in] bps      the number of bits per sample of audio data.  This must be 8, 16, 24, or
 *                      32.  This does not properly handle byte offset calculations for compressed
 *                      audio formats.
 *  @returns the requested byte count aligned to the next frame boundary if it is not already
 *           aligned.
 *  @returns the requested byte count unmodified if it is already aligned to a frame boundary.
 */
constexpr size_t alignBytesToFrameCeil(size_t bytes, size_t channels, size_t bps)
{
    size_t blockSize = (channels * bps) / CHAR_BIT;
    size_t count = bytes + (blockSize - 1);
    return count - (count % blockSize);
}

/**
 *  aligns a byte count to a frame boundary for an audio data format.
 *
 *  @param[in] bytes    the byte count to align to a frame boundary.  This will be aligned to the
 *                      next higher frame boundary if it is not already aligned.
 *  @param[in] channels the number of channels in the audio data format.
 *  @param[in] format   the sample format of the data.  This must be a PCM sample format.
 *  @returns the requested byte count aligned to the next frame boundary if it is not already
 *           aligned.
 *  @returns the requested byte count unmodified if it is already aligned to a frame boundary.
 */
inline size_t alignBytesToFrameCeil(size_t bytes, size_t channels, SampleFormat format)
{
    return alignBytesToFrameCeil(bytes, channels, sampleFormatToBitsPerSample(format));
}

/**
 *  aligns a byte count to a frame boundary for an audio data format.
 *
 *  @param[in] bytes    the byte count to align to a frame boundary.  This will be aligned to the
 *                      next higher frame boundary if it is not already aligned.
 *  @param[in] format   the format information block for the sound data this time is being
 *                      converted for.
 *  @returns the requested byte count aligned to the next frame boundary if it is not already
 *           aligned.
 *  @returns the requested byte count unmodified if it is already aligned to a frame boundary.
 */
inline size_t alignBytesToFrameCeil(size_t bytes, const SoundFormat* format)
{
    return alignBytesToFrameCeil(bytes, format->channels, format->bitsPerSample);
}

/**
 *  aligns a byte count to a frame boundary for an audio data format.
 *
 *  @param[in] bytes    the byte count to align to a frame boundary.  This will be aligned to the
 *                      previous frame boundary if it is not already aligned.
 *  @param[in] channels the number of channels in the audio data format.
 *  @param[in] bps      the number of bits per sample of audio data.  This must be 8, 16, 24, or
 *                      32.  This does not properly handle byte offset calculations for compressed
 *                      audio formats.
 *  @returns the requested byte count aligned to the previous frame boundary if it is not already
 *           aligned.
 *  @returns the requested byte count unmodified if it is already aligned to a frame boundary.
 */
constexpr size_t alignBytesToFrameFloor(size_t bytes, size_t channels, size_t bps)
{
    size_t blockSize = (channels * bps) / CHAR_BIT;
    return bytes - (bytes % blockSize);
}

/**
 *  aligns a byte count to a frame boundary for an audio data format.
 *
 *  @param[in] bytes    the byte count to align to a frame boundary.  This will be aligned to the
 *                      previous frame boundary if it is not already aligned.
 *  @param[in] channels the number of channels in the audio data format.
 *  @param[in] format   the sample format of the data.  This must be a PCM sample format.
 *  @returns the requested byte count aligned to the previous frame boundary if it is not already
 *           aligned.
 *  @returns the requested byte count unmodified if it is already aligned to a frame boundary.
 */
constexpr size_t alignBytesToFrameFloor(size_t bytes, size_t channels, SampleFormat format)
{
    return alignBytesToFrameFloor(bytes, channels, sampleFormatToBitsPerSample(format));
}

/**
 *  aligns a byte count to a frame boundary for an audio data format.
 *
 *  @param[in] bytes    the byte count to align to a frame boundary.  This will be aligned to the
 *                      previous frame boundary if it is not already aligned.
 *  @param[in] format   the format information block for the sound data this time is being
 *                      converted for.
 *  @returns the requested byte count aligned to the previous frame boundary if it is not already
 *           aligned.
 *  @returns the requested byte count unmodified if it is already aligned to a frame boundary.
 */
inline size_t alignBytesToFrameFloor(size_t bytes, const SoundFormat* format)
{
    return alignBytesToFrameFloor(bytes, format->channels, format->bitsPerSample);
}

/**
 *  Generates a SoundFormat based on the 4 parameters given.
 *  @param[out] out       The SoundFormat to generate.
 *  @param[in]  format    The format of the samples in the sound.
 *  @param[in]  frameRate The frame rate of t
 *  @param[in]  channels  The number of channels in the sound.
 *  @param[in]  mask      The speaker mask of the sound.
 */
inline void generateSoundFormat(
    SoundFormat* out, SampleFormat format, size_t channels, size_t frameRate, SpeakerMode mask = kSpeakerModeDefault)
{
    out->channels = channels;
    out->format = format;
    out->frameRate = frameRate;
    out->bitsPerSample = sampleFormatToBitsPerSample(out->format);
    out->frameSize = out->bitsPerSample / CHAR_BIT * out->channels;
    out->blockSize = out->frameSize; // PCM is 1 frame per block
    out->framesPerBlock = 1;
    out->channelMask = mask;
    out->validBitsPerSample = out->bitsPerSample;
}

/**
 *  Initialize a SoundDataLoadDesc to its defaults.
 *  @param[out] desc The desc to initialize.
 *
 *  @remarks This initializes @p desc to a set of default values.
 *           This is useful for cases where only a small subset of members need
 *           to be changed, since this will initialize the entire struct to
 *           no-op values. For example, when loading a sound from a file name,
 *           only @p desc->name and @p desc->flags need to be modified.
 *
 *  @note This function is deprecated and should no longer be used.  This
 *        can be replaced by simply initializing the descriptor with "= {}".
 */
inline void getSoundDataLoadDescDefaults(SoundDataLoadDesc* desc)
{
    *desc = {};
}

/**
 *  Initialize a PlaySoundDesc to its defaults.
 *  @param[out] desc The desc to initialize.
 *
 *  @remarks This initializes @p desc to a set of default values.
 *           This is useful for cases where only a small subset of members need
 *           to be changed, since this will initialize the entire struct to
 *           no-op values. For example, when playing a one shot sound, only
 *           @p desc->sound will need to be modified.
 *
 *  @note This function is deprecated and should no longer be used.  This
 *        can be replaced by simply initializing the descriptor with "= {}".
 */
inline void getPlaySoundDescDefaults(PlaySoundDesc* desc)
{
    *desc = {};
}

/** fills a cone descriptor with the default cone values.
 *
 *  @param[out] cone    the cone descriptor to fill in.
 *  @returns no return value.
 *
 *  @remarks This fills in a cone descriptor with the default values.  Note that the cone
 *           descriptor doesn't have an implicit constructor because it is intended to be
 *           a sparse struct that generally does not need to be fully initialized.
 */
inline void getConeDefaults(EntityCone* cone)
{
    cone->insideAngle = kConeAngleOmnidirectional;
    cone->outsideAngle = kConeAngleOmnidirectional;
    cone->volume = { 1.0f, 0.0f };
    cone->lowPassFilter = { 0.0f, 1.0f };
    cone->reverb = { 0.0f, 1.0f };
    cone->ext = nullptr;
}

/** fills a rolloff descriptor with the default rolloff values.
 *
 *  @param[out] desc    the rolloff descriptor to fill in.
 *  @returns no return value.
 *
 *  @remarks This fills in a rolloff descriptor with the default values.  Note that the
 *           rolloff descriptor doesn't have an implicit constructor because it is intended
 *           to be a sparse struct that generally does not need to be fully initialized.
 */
inline void getRolloffDefaults(RolloffDesc* desc)
{
    desc->type = RolloffType::eInverse;
    desc->nearDistance = 0.0f;
    desc->farDistance = 10000.0f;
    desc->volume = nullptr;
    desc->lowFrequency = nullptr;
    desc->lowPassDirect = nullptr;
    desc->lowPassReverb = nullptr;
    desc->reverb = nullptr;
    desc->ext = nullptr;
}

/** Create an empty SoundData of a specific length.
 *  @param[in] iface        The IAudioData interface to use.
 *  @param[in] fmt          The sample format for the sound.
 *  @param[in] frameRate    The frame rate of the sound.
 *  @param[in] channels     The number of channels for the sound.
 *  @param[in] bufferLength The length of the sound's buffer as a measure of @p unitType.
 *  @param[in] unitType     The unit type to use for @p bufferLength.
 *  @param[in] name         The name to give the sound, if desired.
 *
 *  @returns The created sound with empty buffer with the valid length set to 0.
 *           The valid length should be set after the sound's buffer is filled.
 *  @returns nullptr if @p fmt, @p frameRate or @p channels are invalid or out
 *           of range.
 *  @returns nullptr if creation failed unexpectedly (such as out of memory).
 *
 */
inline SoundData* createEmptySound(const IAudioData* iface,
                                   SampleFormat fmt,
                                   size_t frameRate,
                                   size_t channels,
                                   size_t bufferLength,
                                   UnitType unitType = UnitType::eFrames,
                                   const char* name = nullptr)
{
    SoundDataLoadDesc desc = {};

    desc.flags |= fDataFlagEmpty;
    if (name == nullptr)
        desc.flags |= fDataFlagNoName;
    desc.name = name;
    desc.pcmFormat = fmt;
    desc.frameRate = frameRate;
    desc.channels = channels;
    desc.bufferLength = bufferLength;
    desc.bufferLengthType = unitType;

    return iface->createData(&desc);
}

/** Convert a sound to a new sample format.
 *  @param[in] iface  The IAudioData interface to use.
 *  @param[in] snd    The sound to convert to a new format.
 *                    This may not be nullptr.
 *  @param[in] newFmt The new format to set the sound to.
 *                    This can be any valid format; setting this to a PCM format
 *                    will cause the output to be a blob of PCM data.
 *  @returns The new sound data created. @p snd and the returned value must both
 *           be released after this call once the caller is finished with them.
 *  @returns nullptr if the operation failed or the specified format was invalid.
 *
 *  @note When converting to any format with specific encoder settings, these
 *        will be left at their defaults.
 */
inline SoundData* convertSoundFormat(const IAudioUtils* iface, SoundData* snd, SampleFormat newFmt)
{
    ConversionDesc desc = {};
    desc.flags = fConvertFlagCopy;
    desc.soundData = snd;
    desc.newFormat = newFmt;
    return iface->convert(&desc);
}

/** Convert a sound to Vorbis.
 *  @param[in] iface              The IAudioData interface to use.
 *  @param[in] snd                The sound to convert to a new format.
 *                                This may not be nullptr.
 *  @param[in] quality            @copydoc VorbisEncoderSettings::quality
 *  @param[in] nativeChannelOrder @copydoc VorbisEncoderSettings::nativeChannelOrder
 *
 *  @returns The new sound data created. @p snd and the returned value must both
 *           be released after this call once the caller is finished with them.
 *  @returns nullptr if the operation failed.
 */
inline SoundData* convertToVorbis(const IAudioUtils* iface,
                                  SoundData* snd,
                                  float quality = 0.9f,
                                  bool nativeChannelOrder = false)
{
    VorbisEncoderSettings vorbis = {};
    ConversionDesc desc = {};

    desc.flags = fConvertFlagCopy;
    desc.soundData = snd;
    desc.newFormat = SampleFormat::eVorbis;
    desc.encoderSettings = &vorbis;

    vorbis.quality = quality;
    vorbis.nativeChannelOrder = nativeChannelOrder;

    return iface->convert(&desc);
}

/** Convert a sound to FLAC.
 *  @param[in] iface              The IAudioData interface to use.
 *  @param[in] snd                The sound to convert to a new format.
 *                                This may not be nullptr.
 *  @param[in] compressionLevel   Compression level.
 *                                See @ref FlacEncoderSettings::compressionLevel.
 *  @param[in] bitsPerSample      Bit precision of each audio sample.
 *                                0 will automatically choose the appropriate
 *                                value for the input sample type.
 *                                See @ref FlacEncoderSettings::bitsPerSample.
 *  @param[in] fileType           File container type.
 *                                See @ref FlacEncoderSettings::fileType.
 *  @param[in] streamableSubset   Whether the streamable subset is used.
 *                                Using the default value is recommended.
 *                                See @ref FlacEncoderSettings::streamableSubset.
 *  @param[in] blockSize          Block size used by the encoder.
 *                                0 will let the encoder choose.
 *                                Letting the encoder choose is recommended.
 *                                See @ref FlacEncoderSettings::blockSize.
 *  @param[in] verifyOutput       Whether output Verification should be enabled.
 *                                See @ref FlacEncoderSettings::verifyOutput.
 *
 *  @returns The new sound data created. @p snd and the returned value must both
 *           be released after this call once the caller is finished with them.
 *  @returns nullptr if the operation failed.
 *  @returns nullptr if the encoding parameters were invalid.
 *
 *  @note It is not recommended to set the encoder settings, apart from
 *        @p compressionLevel, to anything other than their defaults under most
 *        circumstances.
 */
inline SoundData* convertToFlac(const IAudioUtils* iface,
                                SoundData* snd,
                                uint32_t compressionLevel = 5,
                                uint32_t bitsPerSample = 0,
                                FlacFileType fileType = FlacFileType::eFlac,
                                bool streamableSubset = true,
                                uint32_t blockSize = 0,
                                bool verifyOutput = false)
{
    FlacEncoderSettings flac = {};
    ConversionDesc desc = {};

    desc.flags = fConvertFlagCopy;
    desc.soundData = snd;
    desc.newFormat = SampleFormat::eFlac;
    desc.encoderSettings = &flac;

    flac.compressionLevel = compressionLevel;
    flac.bitsPerSample = bitsPerSample;
    flac.fileType = fileType;
    flac.streamableSubset = streamableSubset;
    flac.blockSize = blockSize;
    flac.verifyOutput = verifyOutput;

    return iface->convert(&desc);
}

/** Save a sound to disk.
 *  @param[in] iface     The IAudioUtils interface to use.
 *  @param[in] snd       The sound to convert to a new format.
 *                       This may not be nullptr.
 *  @param[in] fileName  The path to the file on disk to save this to.
 *  @param[in] fmt       The format to save the sound as.
 *                       This can be any valid format.
 *  @param[in] flags     Flags to alter the behavior of this function.
 *  @returns true if the sound was successfully saved.
 *  @returns false if the operation failed.
 *
 *  @note When converting to any format with specific encoder settings, these
 *        will be left at their defaults.
 */
inline bool saveSoundToDisk(const IAudioUtils* iface,
                            SoundData* snd,
                            const char* fileName,
                            SampleFormat fmt = SampleFormat::eDefault,
                            SaveFlags flags = 0)
{
    SoundDataSaveDesc desc = {};

    desc.flags = flags;
    desc.format = fmt;
    desc.soundData = snd;
    desc.filename = fileName;

    return iface->saveToFile(&desc);
}

/** Save a sound to disk as Vorbis.
 *  @param[in] iface              The IAudioUtils interface to use.
 *  @param[in] snd                The sound to convert to a new format.
 *                                This may not be nullptr.
 *  @param[in] fileName           The path to the file on disk to save this to.
 *  @param[in] quality            @copydoc VorbisEncoderSettings::quality
 *  @param[in] nativeChannelOrder @copydoc VorbisEncoderSettings::nativeChannelOrder
 *  @param[in] flags              Flags to alter the behavior of this function.
 *
 *  @returns true if the sound was successfully saved.
 *  @returns false if the operation failed.
 */
inline bool saveToDiskAsVorbis(const IAudioUtils* iface,
                               SoundData* snd,
                               const char* fileName,
                               float quality = 0.9f,
                               bool nativeChannelOrder = false,
                               SaveFlags flags = 0)
{
    VorbisEncoderSettings vorbis = {};
    SoundDataSaveDesc desc = {};

    desc.flags = flags;
    desc.format = SampleFormat::eVorbis;
    desc.soundData = snd;
    desc.filename = fileName;
    desc.encoderSettings = &vorbis;

    vorbis.quality = quality;
    vorbis.nativeChannelOrder = nativeChannelOrder;

    return iface->saveToFile(&desc);
}

/** Convert a sound to FLAC.
 *  @param[in] iface              The IAudioData interface to use.
 *  @param[in] snd                The sound to convert to a new format.
 *                                This may not be nullptr.
 *  @param[in] fileName           The name of the file on disk to create the
 *                                new sound data object from.
 *                                This may not be nullptr.
 *  @param[in] compressionLevel   Compression level.
 *                                See @ref FlacEncoderSettings::compressionLevel.
 *  @param[in] bitsPerSample      Bit precision of each audio sample.
 *                                0 will automatically choose the appropriate
 *                                value for the input sample type.
 *                                See @ref FlacEncoderSettings::bitsPerSample.
 *  @param[in] fileType           File container type.
 *                                See @ref FlacEncoderSettings::fileType.
 *  @param[in] streamableSubset   Whether the streamable subset is used.
 *                                Using the default value is recommended.
 *                                See @ref FlacEncoderSettings::streamableSubset.
 *  @param[in] blockSize          Block size used by the encoder.
 *                                0 will let the encoder choose.
 *                                Letting the encoder choose is recommended.
 *                                See @ref FlacEncoderSettings::blockSize.
 *  @param[in] verifyOutput       Whether output Verification should be enabled.
 *                                See @ref FlacEncoderSettings::verifyOutput.
 *  @param[in] flags              Flags to alter the behavior of this function.
 *
 *
 *  @returns true if the sound was successfully saved.
 *  @returns false if the operation failed.
 *
 *  @note It is not recommended to set the encoder settings, apart from
 *        @p compressionLevel, to anything other than their defaults under most
 *        circumstances.
 */
inline bool saveToDiskAsFlac(const IAudioUtils* iface,
                             SoundData* snd,
                             const char* fileName,
                             uint32_t compressionLevel = 5,
                             uint32_t bitsPerSample = 0,
                             FlacFileType fileType = FlacFileType::eFlac,
                             bool streamableSubset = true,
                             uint32_t blockSize = 0,
                             bool verifyOutput = false,
                             SaveFlags flags = 0)
{
    FlacEncoderSettings flac = {};
    carb::audio::SoundDataSaveDesc desc = {};

    desc.flags = flags;
    desc.format = SampleFormat::eFlac;
    desc.soundData = snd;
    desc.filename = fileName;
    desc.encoderSettings = &flac;

    flac.compressionLevel = compressionLevel;
    flac.bitsPerSample = bitsPerSample;
    flac.fileType = fileType;
    flac.streamableSubset = streamableSubset;
    flac.blockSize = blockSize;
    flac.verifyOutput = verifyOutput;

    return iface->saveToFile(&desc);
}

/** Convert a sound to Opus.
 *  @param[in] iface                          The IAudioData interface to use.
 *  @param[in] snd                            The sound to convert to a new format.
 *                                            This may not be nullptr.
 *  @param[in] fileName                       The name of the file on disk to create the
 *                                            new sound data object from.
 *                                            This may not be nullptr.
 *  @param[in] bitrate                        @copydoc OpusEncoderSettings::bitrate
 *  @param[in] usage                          @copydoc OpusEncoderSettings::usage
 *  @param[in] complexity                     @copydoc OpusEncoderSettings::complexity
 *  @param[in] bitDepth                       @copydoc OpusEncoderSettings::bitDepth
 *  @param[in] blockSize                      @copydoc OpusEncoderSettings::blockSize
 *  @param[in] bandwidth                      @copydoc OpusEncoderSettings::bandwidth
 *  @param[in] outputGain                     @copydoc OpusEncoderSettings::outputGain
 *  @param[in] packetLoss                     @copydoc OpusEncoderSettings::packetLoss
 *  @param[in] flags                          @copydoc OpusEncoderSettings::flags
 *  @param[in] saveFlags                      Flags to alter the behavior of this function.
 *
 *  @returns true if the sound was successfully saved.
 *  @returns false if the operation failed.
 *
 *  @note For general purpose audio use (e.g. saving recorded audio to disk for
 *        storage), you should at most modify @p bitrate, @p usage and @p complexity.
 *        For storing very heavily compressed audio, you may also want to set
 *        @p bandwidth and @p bitDepth.
 *        The rest of the options are mainly for encoding you intend to transmit
 *        over a network or miscellaneous purposes.
 */
inline bool saveToDiskAsOpus(const IAudioUtils* iface,
                             SoundData* snd,
                             const char* fileName,
                             uint32_t bitrate = 0,
                             OpusCodecUsage usage = OpusCodecUsage::eGeneral,
                             int8_t complexity = -1,
                             uint8_t blockSize = 48,
                             uint8_t packetLoss = 0,
                             uint8_t bandwidth = 20,
                             uint8_t bitDepth = 0,
                             int16_t outputGain = 0,
                             OpusEncoderFlags flags = 0,
                             SaveFlags saveFlags = 0)
{
    OpusEncoderSettings opus = {};
    carb::audio::SoundDataSaveDesc desc = {};

    desc.flags = saveFlags;
    desc.format = SampleFormat::eOpus;
    desc.soundData = snd;
    desc.filename = fileName;
    desc.encoderSettings = &opus;

    opus.flags = flags;
    opus.bitrate = bitrate;
    opus.usage = usage;
    opus.complexity = complexity;
    opus.blockSize = blockSize;
    opus.packetLoss = packetLoss;
    opus.bandwidth = bandwidth;
    opus.bitDepth = bitDepth;
    opus.outputGain = outputGain;

    return iface->saveToFile(&desc);
}

/** create a sound data object from a file on disk.
 *
 *  @param[in] iface        The IAudioData interface to use.
 *  @param[in] filename     The name of the file on disk to create the new sound
 *                          data object from.  This may not be nullptr.
 *  @param[in] streaming    set to true to create a streaming sound.  This will
 *                          be decoded as it plays.  Set to false to decode the
 *                          sound immediately on load.
 *  @param[in] autoStream   The threshold in bytes at which the new sound data
 *                          object will decide to stream instead of decode into
 *                          memory.  If the decoded size of the sound will be
 *                          larger than this value, it will be streamed from its
 *                          original source instead of decoded.  Set this to 0
 *                          to disable auto-streaming.
 *  @param[in] fmt          The format the sound should be decoded into.  By
 *                          default, the decoder choose its preferred format.
 *  @param[in] flags        Optional flags to change the behavior.
 *                          This can be any of: @ref fDataFlagSkipMetaData,
 *                          @ref fDataFlagSkipEventPoints or @ref fDataFlagCalcPeaks.
 *  @returns The new sound data if successfully created and loaded.  This
 *           object must be released once it is no longer needed.
 *  @returns nullptr if the operation failed.  This may include the file
 *           not being accessible, the file's data not being the correct
 *           format, or a decoding error occurs.
 */
inline SoundData* createSoundFromFile(const IAudioData* iface,
                                      const char* filename,
                                      bool streaming = false,
                                      size_t autoStream = 0,
                                      SampleFormat fmt = SampleFormat::eDefault,
                                      DataFlags flags = 0)
{
    constexpr DataFlags kValidFlags = fDataFlagSkipMetaData | fDataFlagSkipEventPoints | fDataFlagCalcPeaks;
    SoundDataLoadDesc desc = {};

    if ((flags & ~kValidFlags) != 0)
    {
        CARB_LOG_ERROR("invalid flags 0x%08" PRIx32, flags);
        return nullptr;
    }

    desc.flags = flags;
    desc.name = filename;
    desc.pcmFormat = fmt;
    desc.autoStreamThreshold = autoStream;

    if (streaming)
        desc.flags |= fDataFlagStream;

    else
        desc.flags |= fDataFlagDecode;

    return iface->createData(&desc);
}

/** create a sound data object from a blob in memory.
 *
 *  @param[in] iface        The IAudioData interface to use.
 *  @param[in] dataBlob     the blob of data to load the asset from.  This may
 *                          not be nullptr.  This should include the entire
 *                          contents of the original asset file.
 *  @param[in] dataLength   the length of the data blob in bytes.  This may not
 *                          be zero.
 *  @param[in] streaming    set to true to create a streaming sound.  This will
 *                          be decoded as it plays.  Set to false to decode the
 *                          sound immediately on load.
 *  @param[in] autoStream   The threshold in bytes at which the new sound data
 *                          object will decide to stream instead of decode into
 *                          memory.  If the decoded size of the sound will be
 *                          larger than this value, it will be streamed from its
 *                          original source instead of decoded.  Set this to 0
 *                          to disable auto-streaming.  This will be ignored if
 *                          the data is already uncompressed PCM.
 *  @param[in] fmt          The format the sound should be decoded into.  By
 *                          default, the decoder choose its preferred format.
 *  @param[in] flags        Optional flags to change the behavior.
 *                          This can be any of: @ref fDataFlagSkipMetaData,
 *                          @ref fDataFlagSkipEventPoints, @ref fDataFlagCalcPeaks
 *                          or @ref fDataFlagUserMemory.
 *
 *
 *  @returns The new sound data if successfully created and loaded.  This
 *           object must be released once it is no longer needed.
 *  @returns nullptr if the operation failed.  This may include the file
 *           not being accessible, the file's data not being the correct
 *           format, or a decoding error occurs.
 */
inline SoundData* createSoundFromBlob(const IAudioData* iface,
                                      const void* dataBlob,
                                      size_t dataLength,
                                      bool streaming = false,
                                      size_t autoStream = 0,
                                      SampleFormat fmt = SampleFormat::eDefault,
                                      DataFlags flags = 0)
{
    constexpr DataFlags kValidFlags =
        fDataFlagSkipMetaData | fDataFlagSkipEventPoints | fDataFlagCalcPeaks | fDataFlagUserMemory;
    SoundDataLoadDesc desc = {};

    if ((flags & ~kValidFlags) != 0)
    {
        CARB_LOG_ERROR("invalid flags 0x%08" PRIx32, flags);
        return nullptr;
    }

    desc.flags = fDataFlagInMemory | flags;
    desc.dataBlob = dataBlob;
    desc.dataBlobLengthInBytes = dataLength;
    desc.pcmFormat = fmt;
    desc.autoStreamThreshold = autoStream;

    if (streaming)
        desc.flags |= fDataFlagStream;

    else
        desc.flags |= fDataFlagDecode;

    return iface->createData(&desc);
}

/** Creates a sound data object from a blob of memory.
 *  @param[in] iface            The audio data interface to use.
 *  @param[in] dataBlob         The buffer of data to use to create a sound data object.
 *  @param[in] dataLength       The length of @p buffer in bytes.
 *  @param[in] frames           The number of frames of data in @p buffer.
 *  @param[in] format           The data format to use to interpret the data in @p buffer.
 *  @returns A new sound data object containing the data in @p buffer if successfully created.
 *  @returns nullptr if a new sound data object could not be created.
 */
inline SoundData* createSoundFromRawPcmBlob(
    const IAudioData* iface, const void* dataBlob, size_t dataLength, size_t frames, const SoundFormat* format)
{
    SoundDataLoadDesc desc = {};


    desc.flags = carb::audio::fDataFlagFormatRaw | carb::audio::fDataFlagInMemory;
    desc.dataBlob = dataBlob;
    desc.dataBlobLengthInBytes = dataLength;
    desc.channels = format->channels;
    desc.frameRate = format->frameRate;
    desc.encodedFormat = format->format;
    desc.pcmFormat = format->format;
    desc.bufferLength = frames;
    desc.bufferLengthType = carb::audio::UnitType::eFrames;

    return iface->createData(&desc);
}

/** Play a sound with no special parameters.
 *  @param[in] iface   The IAudioPlayback interface to use.
 *  @param[in] ctx     The context to play the sound on.
 *  @param[in] snd     The sound to play.
 *  @param[in] spatial This chooses whether the sound is played as spatial or non-spatial.
 */
inline Voice* playOneShotSound(const IAudioPlayback* iface, Context* ctx, SoundData* snd, bool spatial = false)
{
    PlaySoundDesc desc = {};
    VoiceParams params = {};

    // desc to play the sound once fully in a non-spatial manner
    desc.sound = snd;
    if (spatial)
    {
        desc.validParams = fVoiceParamPlaybackMode;
        desc.params = &params;
        params.playbackMode = fPlaybackModeSpatial;
    }

    return iface->playSound(ctx, &desc);
}

/** Play a sound sound that loops.
 *  @param[in] iface     The IAudioPlayback interface to use.
 *  @param[in] ctx       The context to play the sound on.
 *  @param[in] snd       The sound to play.
 *  @param[in] loopCount The number of times the sound will loop.
 *  @param[in] spatial   This chooses whether the sound is played as spatial or non-spatial.
 *  @remarks This plays a sound which loops through the full sound a given
 *           number of times (or an infinite number of times if desired).
 */
inline Voice* playLoopingSound(const IAudioPlayback* iface,
                               Context* ctx,
                               SoundData* snd,
                               size_t loopCount = kEventPointLoopInfinite,
                               bool spatial = false)
{
    EventPoint loopPoint = {};
    PlaySoundDesc desc = {};
    VoiceParams params = {};

    // desc to play the sound once fully in a non-spatial manner
    desc.sound = snd;
    desc.loopPoint.loopPoint = &loopPoint;
    loopPoint.loopCount = loopCount;
    if (spatial)
    {
        desc.validParams = fVoiceParamPlaybackMode;
        desc.params = &params;
        params.playbackMode = fPlaybackModeSpatial;
    }

    return iface->playSound(ctx, &desc);
}

/** Set the volume of a voice.
 *  @param[in] iface     The IAudioPlayback interface to use.
 *  @param[in] voice     The voice to alter.
 *  @param[in] volume    The new volume to set on @p voice.
 */
inline void setVoiceVolume(const IAudioPlayback* iface, Voice* voice, float volume)
{
    carb::audio::VoiceParams params = {};
    params.volume = volume;
    iface->setVoiceParameters(voice, fVoiceParamVolume, &params);
}

/** Set the frequencyRatio of a voice.
 *  @param[in] iface          The IAudioPlayback interface to use.
 *  @param[in] voice          The voice to alter.
 *  @param[in] frequencyRatio The new volume to set on @p voice.
 */
inline void setVoiceFrequencyRatio(const IAudioPlayback* iface, Voice* voice, float frequencyRatio)
{
    carb::audio::VoiceParams params = {};
    params.frequencyRatio = frequencyRatio;
    iface->setVoiceParameters(voice, fVoiceParamFrequencyRatio, &params);
}

/** Pause a voice.
 *  @param[in] iface     The IAudioPlayback interface to use.
 *  @param[in] voice     The voice to pause.
 */
inline void pauseVoice(const IAudioPlayback* iface, Voice* voice)
{
    carb::audio::VoiceParams params = {};
    params.playbackMode = fPlaybackModePaused;
    iface->setVoiceParameters(voice, fVoiceParamPause, &params);
}

/** Unpause a voice.
 *  @param[in] iface     The IAudioPlayback interface to use.
 *  @param[in] voice     The voice to unpause.
 */
inline void unpauseVoice(const IAudioPlayback* iface, Voice* voice)
{
    carb::audio::VoiceParams params = {};
    iface->setVoiceParameters(voice, fVoiceParamPause, &params);
}

/** Mute a voice.
 *  @param[in] iface     The IAudioPlayback interface to use.
 *  @param[in] voice     The voice to mute.
 */
inline void muteVoice(const IAudioPlayback* iface, Voice* voice)
{
    carb::audio::VoiceParams params = {};
    params.playbackMode = fPlaybackModeMuted;
    iface->setVoiceParameters(voice, fVoiceParamMute, &params);
}

/** Unmute a voice.
 *  @param[in] iface     The IAudioPlayback interface to use.
 *  @param[in] voice     The voice to unmute.
 */
inline void unmuteVoice(const IAudioPlayback* iface, Voice* voice)
{
    carb::audio::VoiceParams params = {};
    iface->setVoiceParameters(voice, fVoiceParamMute, &params);
}

/** Set the matrix of a voice.
 *  @param[in] iface     The IAudioPlayback interface to use.
 *  @param[in] voice     The voice to alter.
 *  @param[in] matrix    The new matrix to set on @p voice.
 *                       This can be nullptr to revert to a default matrix.
 */
inline void setVoiceMatrix(const IAudioPlayback* iface, Voice* voice, const float* matrix)
{
    carb::audio::VoiceParams params = {};
    params.matrix = matrix;
    iface->setVoiceParameters(voice, fVoiceParamMatrix, &params);
}

/** Calculate the gain parameter for an Opus encoder from a floating point gain.
 *  @param[in] gain The floating point gain to convert to an Opus gain parameter.
 *                  This must be between [-128, 128] or it will be clamped.
 *
 *  @returns A gain value that can be used as a parameter to an Opus encoder.
 *           This is a signed 16 bit fixed point value with 8 fractional bits.
 */
inline int16_t calculateOpusGain(float gain)
{
    // multiply by 256 to convert this into a s7.8 fixed point value.
    // IEEE754 float has 23 bits in the mantissa, so we can represent the 16
    // bit range losslessly with a float
    gain *= 256.f;

    // clamp the result in case the gain was too large, then truncate the
    // fractional part
    return int16_t(CARB_CLAMP(gain, float(INT16_MIN), float(INT16_MAX)));
}

/** Calculate a decibel gain value from a linear volume scale.
 *  @param[in] linear The linear float scale value to convert to a gain value.
 *
 *  @returns The gain value that will produce a linear volume scale of @p linear.
 */
inline float calculateGainFromLinearScale(float linear)
{
    // gain is calculated as 20 * log10(linear)
    return 20.f * log10f(linear);
}

/** Calculate the linear volume scale from a decibel gain level.
 *  @param[in] gain The gain value to be converted to a linear scale.
 *                  This parameter should be a fairly small number; for
 *                  example, -186.64 is approximately the decibel gain level
 *                  of the noise floor for 32 bit audio.
 *
 *  @returns The linear volume scale produced by gain @p gain.
 */
inline float calculateLinearScaleFromGain(float gain)
{
    return powf(10, gain * (1.f / 20.f));
}

/** Increment a counter with a non-power-of-2 modulo.
 *  @param[in] counter The counter value to increment.
 *                     This must be less than @p modulo.
 *  @param[in] modulo  The value to perform a modulo by.
 *                     This may not be 0.
 *
 *  @returns @p counter incremented and wrapped around @p modulo.
 *
 *  @remarks This function exists to perform a modulo around a non-power-of-2
 *           modulo without having to duplicate the wrap code in multiple places.
 *           Note that this is considerably more efficient than use of the %
 *           operator where a power-of-2 optimization cannot be made.
 */
inline size_t incrementWithWrap(size_t counter, size_t modulo)
{
    CARB_ASSERT(modulo > 0);
    CARB_ASSERT(counter < modulo);
    return (counter + 1 == modulo) ? 0 : counter + 1;
}

/** Decrement a counter with a non-power-of-2 modulo.
 *  @param[in] counter The counter value to increment.
 *                     This must be less than or equal to @p modulo.
 *                     @p counter == @p modulo is allowed for some edge cases
 *                     where it's useful.
 *  @param[in] modulo  The value to perform a modulo by.
 *                     This may not be 0.
 *
 *  @returns @p counter incremented and wrapped around @p modulo.
 *
 *  @remarks This function exists to perform a modulo around a non-power-of-2
 *           modulo without having to duplicate the wrap code in multiple places.
 *           Note that % does not work for decrementing with a non-power-of-2 modulo.
 */
inline size_t decrementWithWrap(size_t counter, size_t modulo)
{
    CARB_ASSERT(modulo > 0);
    CARB_ASSERT(counter <= modulo);
    return (counter == 0) ? modulo - 1 : counter - 1;
}

/** Calculates an estimate of the current level of video latency.
 *
 *  @param[in] fps              The current video frame rate in frames per second.  The caller
 *                              is responsible for accurately retrieving and calculating this.
 *  @param[in] framesInFlight   The current number of video frames currently in flight.  This
 *                              is the number of frames that have been produced by the renderer
 *                              but have not been displayed to the user yet (or has been presented
 *                              but not realized on screen yet).  The frame being produced would
 *                              represent the simulation time (where a synchronized sound is
 *                              expected to start playing), and the other buffered frames are
 *                              ones that go back further in time (ie: older frames as far as
 *                              the simulation is concerned).  This may need to be an estimate
 *                              the caller can retrieve from the renderer.
 *  @param[in] perceptibleDelay A limit below which a zero latency will be calculated.  If the
 *                              total calculated latency is less than this threshold, the latency
 *                              will be zeroed out.  If total calculated latency is larger than
 *                              this limit, a delay estimate will be calculated.  This value is
 *                              given in microseconds.  This defaults to 200,000 microseconds.
 *  @returns The calculated latency estimate in microseconds.
 *
 *  @remarks This is used to calculate an estimate of the current video latency level.  This
 *           value can be used to set the @ref ContextParams2::videoLatency value based on the
 *           current performance of the video rendering system.  This value is used by the audio
 *           engine to delay the queuing of new voices by a given amount of time.
 */
inline int64_t estimateVideoLatency(double fps, double framesInFlight, int64_t perceptibleDelay = kImperceptibleDelay)
{
    constexpr int64_t kMinLatency = 20'000;
    double usPerFrame;

    if (fps == 0.0)
        return 0;

    usPerFrame = 1'000'000.0 / fps;

    // the current delay is less than the requested perceptible latency time => clamp the
    //   estimated delay down to zero.
    if (usPerFrame * framesInFlight <= perceptibleDelay)
        return 0;

    // calculate the estimated delay in microseconds.  Note that this will fudge the calculated
    // total latency by a small amount because there is an expected minimum small latency in
    // queuing a new voice already.
    return (int64_t)((usPerFrame * framesInFlight) - CARB_MIN(perceptibleDelay / 2, kMinLatency));
}

} // namespace audio
} // namespace carb
