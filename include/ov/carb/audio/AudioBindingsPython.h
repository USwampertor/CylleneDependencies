// Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../BindingsPythonUtils.h"
#include "IAudioPlayback.h"
#include "IAudioData.h"
#include "IAudioUtils.h"
#include "AudioUtils.h"

namespace carb
{
namespace audio
{

/** A helper class to wrap the Voice* object for python.
 *  There is no way to actually create Voices from these bindings yet.
 *
 *  @note This wrapper only exposes functions to alter voice parameters at the
 *        moment. This is done because IAudioPlayback is not thread-safe yet
 *        and allowing other changes would cause thread safety issues with
 *        Omniverse Kit.
 */
class PythonVoice
{
public:
    PythonVoice(IAudioPlayback* iface, audio::Voice* voice)
    {
        m_iface = iface;
        m_voice = voice;
    }

    void stop()
    {
        m_iface->stopVoice(m_voice);
    }

    bool isPlaying()
    {
        return m_iface->isPlaying(m_voice);
    }

    bool setLoopPoint(const LoopPointDesc* desc)
    {
        return m_iface->setLoopPoint(m_voice, desc);
    }

    size_t getPlayCursor(UnitType type)
    {
        return m_iface->getPlayCursor(m_voice, type);
    }

    void setParameters(VoiceParamFlags paramsToSet, const VoiceParams* params)
    {
        m_iface->setVoiceParameters(m_voice, paramsToSet, params);
    }

    void getParameters(VoiceParamFlags paramsToGet, VoiceParams* params)
    {
        m_iface->getVoiceParameters(m_voice, paramsToGet, params);
    }

    void setPlaybackMode(PlaybackModeFlags mode)
    {
        VoiceParams params = {};
        params.playbackMode = mode;
        m_iface->setVoiceParameters(m_voice, fVoiceParamPlaybackMode, &params);
    }

    void setVolume(float volume)
    {
        VoiceParams params = {};
        params.volume = volume;
        m_iface->setVoiceParameters(m_voice, fVoiceParamVolume, &params);
    }

    void setMute(bool muted)
    {
        VoiceParams params = {};
        params.playbackMode = muted ? fPlaybackModeMuted : 0;
        m_iface->setVoiceParameters(m_voice, fVoiceParamMute, &params);
    }

    void setBalance(float pan, float fade)
    {
        VoiceParams params = {};
        params.balance.pan = pan;
        params.balance.fade = fade;
        m_iface->setVoiceParameters(m_voice, fVoiceParamBalance, &params);
    }

    void setFrequencyRatio(float ratio)
    {
        VoiceParams params = {};
        params.frequencyRatio = ratio;
        m_iface->setVoiceParameters(m_voice, fVoiceParamFrequencyRatio, &params);
    }

    void setPriority(int32_t priority)
    {
        VoiceParams params = {};
        params.priority = priority;
        m_iface->setVoiceParameters(m_voice, fVoiceParamPriority, &params);
    }

    void setSpatialMixLevel(float level)
    {
        VoiceParams params = {};
        params.spatialMixLevel = level;
        m_iface->setVoiceParameters(m_voice, fVoiceParamSpatialMixLevel, &params);
    }

    void setDopplerScale(float scale)
    {
        VoiceParams params = {};
        params.dopplerScale = scale;
        m_iface->setVoiceParameters(m_voice, fVoiceParamDopplerScale, &params);
    }

    void setOcclusion(float direct, float reverb)
    {
        VoiceParams params = {};
        params.occlusion.direct = direct;
        params.occlusion.reverb = reverb;
        m_iface->setVoiceParameters(m_voice, fVoiceParamOcclusionFactor, &params);
    }

    void setMatrix(std::vector<float> matrix)
    {
        // FIXME: This should probably validate the source/destination channels.
        //        We can get the source channels by retrieving the currently playing sound,
        //        but we have no way to retrieve the context's channel count here.
        VoiceParams params = {};
        params.matrix = matrix.data();
        m_iface->setVoiceParameters(m_voice, fVoiceParamMatrix, &params);
    }

    void setPosition(Float3 position)
    {
        VoiceParams params = {};
        params.emitter.flags = fEntityFlagPosition;
        params.emitter.position = position;
        m_iface->setVoiceParameters(m_voice, fVoiceParamEmitter, &params);
    }

    void setVelocity(Float3 velocity)
    {
        VoiceParams params = {};
        params.emitter.flags = fEntityFlagVelocity;
        params.emitter.velocity = velocity;
        m_iface->setVoiceParameters(m_voice, fVoiceParamEmitter, &params);
    }

    void setRolloffCurve(RolloffType type,
                         float nearDistance,
                         float farDistance,
                         std::vector<Float2>& volume,
                         std::vector<Float2>& lowFrequency,
                         std::vector<Float2>& lowPassDirect,
                         std::vector<Float2>& lowPassReverb,
                         std::vector<Float2>& reverb)
    {
        RolloffCurve volumeCurve = {};
        RolloffCurve lowFrequencyCurve = {};
        RolloffCurve lowPassDirectCurve = {};
        RolloffCurve lowPassReverbCurve = {};
        RolloffCurve reverbCurve = {};
        VoiceParams params = {};
        params.emitter.flags = fEntityFlagRolloff;
        params.emitter.rolloff.type = type;
        params.emitter.rolloff.nearDistance = nearDistance;
        params.emitter.rolloff.farDistance = farDistance;
        if (!volume.empty())
        {
            volumeCurve.points = volume.data();
            volumeCurve.pointCount = volume.size();
            params.emitter.rolloff.volume = &volumeCurve;
        }
        if (!lowFrequency.empty())
        {
            lowFrequencyCurve.points = lowFrequency.data();
            lowFrequencyCurve.pointCount = lowFrequency.size();
            params.emitter.rolloff.lowFrequency = &lowFrequencyCurve;
        }
        if (!lowPassDirect.empty())
        {
            lowPassDirectCurve.points = lowPassDirect.data();
            lowPassDirectCurve.pointCount = lowPassDirect.size();
            params.emitter.rolloff.lowPassDirect = &lowPassDirectCurve;
        }
        if (!lowPassReverb.empty())
        {
            lowPassReverbCurve.points = lowPassReverb.data();
            lowPassReverbCurve.pointCount = lowPassReverb.size();
            params.emitter.rolloff.lowPassReverb = &lowPassReverbCurve;
        }
        if (!reverb.empty())
        {
            reverbCurve.points = reverb.data();
            reverbCurve.pointCount = reverb.size();
            params.emitter.rolloff.reverb = &reverbCurve;
        }
        m_iface->setVoiceParameters(m_voice, fVoiceParamEmitter, &params);
    }

private:
    Voice* m_voice;
    IAudioPlayback* m_iface;
};

class PythonSoundData
{
public:
    PythonSoundData(IAudioData* iface, SoundData* data) noexcept
    {
        m_iface = iface;
        m_data = data;
        m_utils = carb::getFramework()->acquireInterface<IAudioUtils>();
    }

    ~PythonSoundData() noexcept
    {
        m_iface->release(m_data);
    }

    static PythonSoundData* fromRawBlob(IAudioData* iface,
                                        const void* blob,
                                        size_t samples,
                                        SampleFormat format,
                                        size_t channels,
                                        size_t frameRate,
                                        SpeakerMode channelMask)
    {
        SoundFormat fmt;
        size_t bytes = samples * sampleFormatToBitsPerSample(format) / CHAR_BIT;
        SoundData* tmp;

        generateSoundFormat(&fmt, format, channels, frameRate, channelMask);

        tmp = createSoundFromRawPcmBlob(iface, blob, bytes, bytesToFrames(bytes, &fmt), &fmt);
        if (tmp == nullptr)
            throw std::runtime_error("failed to create a SoundData object");
        return new PythonSoundData(iface, tmp);
    }

    const char* getName()
    {
        return m_iface->getName(m_data);
    }

    bool isDecoded()
    {
        return (m_iface->getFlags(m_data) & fDataFlagStream) == 0;
    }

    SoundFormat getFormat()
    {
        SoundFormat fmt;
        m_iface->getFormat(m_data, CodecPart::eEncoder, &fmt);
        return fmt;
    }

    size_t getLength(UnitType units)
    {
        return m_iface->getLength(m_data, units);
    }

    void setValidLength(size_t length, UnitType units)
    {
        m_iface->setValidLength(m_data, length, units);
    }

    size_t getValidLength(UnitType units)
    {
        return m_iface->getValidLength(m_data, units);
    }

    std::vector<uint8_t> getBufferU8(size_t offset, size_t length, UnitType units)
    {
        return getBuffer<uint8_t>(offset, length, units);
    }

    std::vector<int16_t> getBufferS16(size_t offset, size_t length, UnitType units)
    {
        return getBuffer<int16_t>(offset, length, units);
    }

    std::vector<int32_t> getBufferS32(size_t offset, size_t length, UnitType units)
    {
        return getBuffer<int32_t>(offset, length, units);
    }

    std::vector<float> getBufferFloat(size_t offset, size_t length, UnitType units)
    {
        return getBuffer<float>(offset, length, units);
    }

    void writeBufferU8(const std::vector<uint8_t>& data, size_t offset, UnitType units)
    {
        return writeBuffer<uint8_t>(data, offset, units);
    }

    void writeBufferS16(const std::vector<int16_t>& data, size_t offset, UnitType units)
    {
        return writeBuffer<int16_t>(data, offset, units);
    }

    void writeBufferS32(const std::vector<int32_t>& data, size_t offset, UnitType units)
    {
        return writeBuffer<int32_t>(data, offset, units);
    }

    void writeBufferFloat(const std::vector<float>& data, size_t offset, UnitType units)
    {
        return writeBuffer<float>(data, offset, units);
    }

    size_t getMemoryUsed()
    {
        return m_iface->getMemoryUsed(m_data);
    }

    uint32_t getMaxInstances()
    {
        return m_iface->getMaxInstances(m_data);
    }

    void setMaxInstances(uint32_t limit)
    {
        m_iface->setMaxInstances(m_data, limit);
    }

    PeakVolumes getPeakLevel()
    {
        PeakVolumes vol;
        if (!m_iface->getPeakLevel(m_data, &vol))
        {
            throw std::runtime_error("this sound has no peak volume information");
        }

        return vol;
    }

    std::vector<EventPoint> getEventPoints()
    {
        size_t count = m_iface->getEventPoints(m_data, nullptr, 0);
        size_t retrieved;
        std::vector<EventPoint> out;
        out.resize(count);
        retrieved = m_iface->getEventPoints(m_data, out.data(), count);
        if (retrieved < count)
        {
            CARB_LOG_ERROR("retrieved fewer event points than expected (%zu < %zu)\n", retrieved, count);
            out.resize(retrieved);
        }
        return out;
    }

    const EventPoint* getEventPointById(EventPointId id)
    {
        return wrapEventPoint(m_iface->getEventPointById(m_data, id));
    }

    const EventPoint* getEventPointByIndex(size_t index)
    {
        return wrapEventPoint(m_iface->getEventPointByIndex(m_data, index));
    }

    const EventPoint* getEventPointByPlayIndex(size_t index)
    {
        return wrapEventPoint(m_iface->getEventPointByPlayIndex(m_data, index));
    }

    size_t getEventPointMaxPlayIndex()
    {
        return m_iface->getEventPointMaxPlayIndex(m_data);
    }

    bool setEventPoints(std::vector<EventPoint> eventPoints)
    {
        // text does not work properly in bindings
        for (size_t i = 0; i < eventPoints.size(); i++)
        {
            eventPoints[i].label = nullptr;
            eventPoints[i].text = nullptr;
        }

        return m_iface->setEventPoints(m_data, eventPoints.data(), eventPoints.size());
    }

    void clearEventPoints()
    {
        m_iface->setEventPoints(m_data, kEventPointTableClear, 0);
    }

    std::pair<const char*, const char*> getMetaDataByIndex(size_t index)
    {
        const char* value;
        const char* key = m_iface->getMetaDataTagName(m_data, index, &value);
        return std::pair<const char*, const char*>(key, value);
    }

    const char* getMetaData(const char* tagName)
    {
        return m_iface->getMetaData(m_data, tagName);
    }

    bool setMetaData(const char* tagName, const char* tagValue)
    {
        return m_iface->setMetaData(m_data, tagName, tagValue);
    }

    bool saveToFile(const char* fileName, SampleFormat format = SampleFormat::eDefault, SaveFlags flags = 0)
    {
        return saveSoundToDisk(m_utils, m_data, fileName, format, flags);
    }

    SoundData* getNativeObject()
    {
        return m_data;
    }

private:
    EventPoint* wrapEventPoint(const EventPoint* point)
    {
        if (point == nullptr)
            return nullptr;

        EventPoint* out = new EventPoint;
        *out = *point;
        return out;
    }

    template <typename T>
    SampleFormat getFormatFromType()
    {
        // I'd do this with template specialization but GCC won't accept that
        // for some reason
        if (std::is_same<T, uint8_t>::value)
        {
            return SampleFormat::ePcm8;
        }
        else if (std::is_same<T, int16_t>::value)
        {
            return SampleFormat::ePcm16;
        }
        else if (std::is_same<T, int32_t>::value)
        {
            return SampleFormat::ePcm32;
        }
        else if (std::is_same<T, float>::value)
        {
            return SampleFormat::ePcmFloat;
        }
        else
        {
            return SampleFormat::eDefault;
        }
    }

    template <typename T>
    std::vector<T> getBuffer(size_t offset, size_t length, UnitType units)
    {
        size_t samples;
        size_t soundLength = getValidLength(UnitType::eFrames);
        SoundFormat fmt = getFormat();
        std::vector<T> out;

        if (units == UnitType::eBytes && offset % fmt.frameSize != 0)
        {
            throw std::runtime_error("byte offset was not aligned correctly for the data type");
        }

        length = convertUnits(length, units, UnitType::eFrames, &fmt);
        offset = convertUnits(offset, units, UnitType::eFrames, &fmt);

        if (length == 0 || length > soundLength)
            length = soundLength;

        if (length == 0)
            return {};

        offset = CARB_MIN(soundLength - 1, offset);

        if (offset + length > soundLength)
        {
            length = soundLength - offset;
        }

        samples = length * fmt.channels;
        out.resize(samples);

        if (isDecoded())
        {
            size_t byteOffset = convertUnits(offset, UnitType::eFrames, UnitType::eBytes, &fmt);
            const void* buffer = m_iface->getReadBuffer(m_data);

            writeGenericBuffer(static_cast<const uint8_t*>(buffer) + byteOffset, out.data(), fmt.format,
                               getFormatFromType<T>(), samples);
        }
        else
        {
            size_t decoded = 0;
            CodecState* state = nullptr;
            CodecStateDesc desc = {};

            desc.part = CodecPart::eDecoder;
            desc.decode.flags =
                fDecodeStateFlagCoarseSeek | fDecodeStateFlagSkipMetaData | fDecodeStateFlagSkipEventPoints;
            desc.decode.soundData = m_data;
            desc.decode.outputFormat = getFormatFromType<T>();
            desc.decode.readCallbackContext = nullptr;
            desc.decode.ext = nullptr;

            state = m_iface->createCodecState(&desc);
            if (state == nullptr)
            {
                m_iface->destroyCodecState(state);
                throw std::runtime_error("failed to initialize the decoder");
            }

            if (offset != 0 && !m_iface->setCodecPosition(state, offset, UnitType::eFrames))
            {
                m_iface->destroyCodecState(state);
                throw std::runtime_error("failed to seek into the sound");
            }

            if (m_iface->decodeData(state, out.data(), length, &decoded) == nullptr)
            {
                m_iface->destroyCodecState(state);
                throw std::runtime_error("failed to decode the sound");
            }

            if (decoded < length)
            {
                CARB_LOG_ERROR("decoded fewer frames that expected (%zu < %zu)\n", decoded, length);
                out.resize(decoded * fmt.channels);
            }

            m_iface->destroyCodecState(state);
        }

        return out;
    }

    template <typename T>
    void writeBuffer(const std::vector<T>& data, size_t offset, UnitType units)
    {
        SoundFormat fmt = getFormat();
        void* buffer = m_iface->getBuffer(m_data);
        size_t length = data.size();
        size_t maxLength = getLength(UnitType::eBytes);

        if (!isDecoded())
        {
            throw std::runtime_error("this SoundData object is read-only");
        }

        if (units == UnitType::eBytes && offset % fmt.frameSize != 0)
        {
            throw std::runtime_error("byte offset was not aligned correctly for the data type");
        }

        offset = convertUnits(offset, units, UnitType::eBytes, &fmt);
        if (offset + length > maxLength)
        {
            length = maxLength - offset;
        }

        writeGenericBuffer(
            data.data(), static_cast<uint8_t*>(buffer) + offset, getFormatFromType<T>(), fmt.format, length);
    }

    void writeGenericBuffer(const void* in, void* out, SampleFormat inFmt, SampleFormat outFmt, size_t samples)
    {
        if (inFmt != outFmt)
        {
            TranscodeDesc desc = {};
            desc.inFormat = inFmt;
            desc.outFormat = outFmt;
            desc.inBuffer = in;
            desc.outBuffer = out;
            desc.samples = samples;
            if (!m_utils->transcodePcm(&desc))
                throw std::runtime_error("PCM transcoding failed unexpectedly");
        }
        else
        {
            memcpy(out, in, samples * sampleFormatToBitsPerSample(inFmt) / CHAR_BIT);
        }
    }

    IAudioData* m_iface;
    IAudioUtils* m_utils;
    SoundData* m_data;
};

/** A helper class to wrap the Context* object for python.
 *  There is no way to actually create or destroy the Context* object in these
 *  bindings yet.
 *
 *  @note This wrapper only exposes functions to alter Context parameters at the
 *        moment. This is done because IAudioPlayback is not thread-safe yet
 *        and allowing other changes would cause thread safety issues with
 *        Omniverse Kit.
 */
class PythonContext
{
public:
    PythonContext(IAudioPlayback* iface, Context* context) : m_context(context), m_iface(iface)
    {
    }

    /** Wrappers for IAudioPlayback functions @{ */
    const ContextCaps* getContextCaps()
    {
        return m_iface->getContextCaps(m_context);
    }

    void setContextParameters(ContextParamFlags paramsToSet, const ContextParams* params)
    {
        m_iface->setContextParameters(m_context, paramsToSet, params);
    }

    void getContextParameters(ContextParamFlags paramsToGet, ContextParams* params)
    {
        m_iface->getContextParameters(m_context, paramsToGet, params);
    }

    PythonVoice* playSound(PythonSoundData* sound,
                           PlayFlags flags,
                           VoiceParamFlags validParams,
                           VoiceParams* params,
                           EventPoint* loopPoint,
                           size_t playStart,
                           size_t playLength,
                           UnitType playUnits)
    {
        PlaySoundDesc desc = {};
        desc.flags = flags;
        desc.sound = sound->getNativeObject();
        desc.validParams = validParams;
        desc.params = params;
        desc.loopPoint.loopPoint = loopPoint;
        if (loopPoint != nullptr)
        {
            desc.loopPoint.loopPointIndex = 0;
        }

        desc.playStart = playStart;
        desc.playLength = playLength;
        desc.playUnits = playUnits;
        return new PythonVoice(m_iface, m_iface->playSound(m_context, &desc));
    }
    /** @} */

private:
    Context* m_context;
    IAudioPlayback* m_iface;
};

inline void definePythonModule(py::module& m)
{
    const char* docString;

    /////// AudioTypes.h ///////
    m.attr("MAX_NAME_LENGTH") = py::int_(kMaxNameLength);
    m.attr("MAX_CHANNELS") = py::int_(kMaxChannels);
    m.attr("MIN_CHANNELS") = py::int_(kMinChannels);
    m.attr("MAX_FRAMERATE") = py::int_(kMaxFrameRate);
    m.attr("MIN_FRAMERATE") = py::int_(kMinFrameRate);

    py::enum_<AudioResult>(m, "AudioResult")
        .value("OK", AudioResult::eOk)
        .value("DEVICE_DISCONNECTED", AudioResult::eDeviceDisconnected)
        .value("DEVICE_LOST", AudioResult::eDeviceLost)
        .value("DEVICE_NOT_OPEN", AudioResult::eDeviceNotOpen)
        .value("DEVICE_OPEN", AudioResult::eDeviceOpen)
        .value("OUT_OF_RANGE", AudioResult::eOutOfRange)
        .value("TRY_AGAIN", AudioResult::eTryAgain)
        .value("OUT_OF_MEMORY", AudioResult::eOutOfMemory)
        .value("INVALID_PARAMETER", AudioResult::eInvalidParameter)
        .value("NOT_ALLOWED", AudioResult::eNotAllowed)
        .value("NOT_FOUND", AudioResult::eNotFound)
        .value("IO_ERROR", AudioResult::eIoError)
        .value("INVALID_FORMAT", AudioResult::eInvalidFormat)
        .value("NOT_SUPPORTED", AudioResult::eNotSupported);

    py::enum_<Speaker>(m, "Speaker")
        .value("FRONT_LEFT", Speaker::eFrontLeft)
        .value("FRONT_RIGHT", Speaker::eFrontRight)
        .value("FRONT_CENTER", Speaker::eFrontCenter)
        .value("LOW_FREQUENCY_EFFECT", Speaker::eLowFrequencyEffect)
        .value("BACK_LEFT", Speaker::eBackLeft)
        .value("BACK_RIGHT", Speaker::eBackRight)
        .value("BACK_CENTER", Speaker::eBackCenter)
        .value("SIDE_LEFT", Speaker::eSideLeft)
        .value("SIDE_RIGHT", Speaker::eSideRight)
        .value("TOP_FRONT_LEFT", Speaker::eTopFrontLeft)
        .value("TOP_FRONT_RIGHT", Speaker::eTopFrontRight)
        .value("TOP_BACK_LEFT", Speaker::eTopBackLeft)
        .value("TOP_BACK_RIGHT", Speaker::eTopBackRight)
        .value("FRONT_LEFT_WIDE", Speaker::eFrontLeftWide)
        .value("FRONT_RIGHT_WIDE", Speaker::eFrontRightWide)
        .value("TOP_LEFT", Speaker::eTopLeft)
        .value("TOP_RIGHT", Speaker::eTopRight)
        .value("COUNT", Speaker::eCount);

    m.attr("SPEAKER_FLAG_FRONT_LEFT") = py::int_(fSpeakerFlagFrontLeft);
    m.attr("SPEAKER_FLAG_FRONT_RIGHT") = py::int_(fSpeakerFlagFrontRight);
    m.attr("SPEAKER_FLAG_FRONT_CENTER") = py::int_(fSpeakerFlagFrontCenter);
    m.attr("SPEAKER_FLAG_LOW_FREQUENCY_EFFECT") = py::int_(fSpeakerFlagLowFrequencyEffect);
    m.attr("SPEAKER_FLAG_BACK_LEFT") = py::int_(fSpeakerFlagSideLeft);
    m.attr("SPEAKER_FLAG_BACK_RIGHT") = py::int_(fSpeakerFlagSideRight);
    m.attr("SPEAKER_FLAG_BACK_CENTER") = py::int_(fSpeakerFlagBackLeft);
    m.attr("SPEAKER_FLAG_SIDE_LEFT") = py::int_(fSpeakerFlagBackRight);
    m.attr("SPEAKER_FLAG_SIDE_RIGHT") = py::int_(fSpeakerFlagBackCenter);
    m.attr("SPEAKER_FLAG_TOP_FRONT_LEFT") = py::int_(fSpeakerFlagTopFrontLeft);
    m.attr("SPEAKER_FLAG_TOP_FRONT_RIGHT") = py::int_(fSpeakerFlagTopFrontRight);
    m.attr("SPEAKER_FLAG_TOP_BACK_LEFT") = py::int_(fSpeakerFlagTopBackLeft);
    m.attr("SPEAKER_FLAG_TOP_BACK_RIGHT") = py::int_(fSpeakerFlagTopBackRight);
    m.attr("SPEAKER_FLAG_FRONT_LEFT_WIDE") = py::int_(fSpeakerFlagFrontLeftWide);
    m.attr("SPEAKER_FLAG_FRONT_RIGHT_WIDE") = py::int_(fSpeakerFlagFrontRightWide);
    m.attr("SPEAKER_FLAG_TOP_LEFT") = py::int_(fSpeakerFlagTopLeft);
    m.attr("SPEAKER_FLAG_TOP_RIGHT") = py::int_(fSpeakerFlagTopRight);

    m.attr("INVALID_SPEAKER_NAME") = py::int_(kInvalidSpeakerName);
    m.attr("SPEAKER_MODE_DEFAULT") = py::int_(kSpeakerModeDefault);
    m.attr("SPEAKER_MODE_MONO") = py::int_(kSpeakerModeMono);
    m.attr("SPEAKER_MODE_STEREO") = py::int_(kSpeakerModeStereo);
    m.attr("SPEAKER_MODE_TWO_POINT_ONE") = py::int_(kSpeakerModeTwoPointOne);
    m.attr("SPEAKER_MODE_QUAD") = py::int_(kSpeakerModeQuad);
    m.attr("SPEAKER_MODE_FOUR_POINT_ONE") = py::int_(kSpeakerModeFourPointOne);
    m.attr("SPEAKER_MODE_FIVE_POINT_ONE") = py::int_(kSpeakerModeFivePointOne);
    m.attr("SPEAKER_MODE_SIX_POINT_ONE") = py::int_(kSpeakerModeSixPointOne);
    m.attr("SPEAKER_MODE_SEVEN_POINT_ONE") = py::int_(kSpeakerModeSevenPointOne);
    m.attr("SPEAKER_MODE_NINE_POINT_ONE") = py::int_(kSpeakerModeNinePointOne);
    m.attr("SPEAKER_MODE_SEVEN_POINT_ONE_POINT_FOUR") = py::int_(kSpeakerModeSevenPointOnePointFour);
    m.attr("SPEAKER_MODE_NINE_POINT_ONE_POINT_FOUR") = py::int_(kSpeakerModeNinePointOnePointFour);
    m.attr("SPEAKER_MODE_NINE_POINT_ONE_POINT_SIX") = py::int_(kSpeakerModeNinePointOnePointSix);
    m.attr("SPEAKER_MODE_THREE_POINT_ZERO") = py::int_(kSpeakerModeThreePointZero);
    m.attr("SPEAKER_MODE_FIVE_POINT_ZERO") = py::int_(kSpeakerModeFivePointZero);
    m.attr("SPEAKER_MODE_COUNT") = py::int_(kSpeakerModeCount);
    m.attr("SPEAKER_MODE_VALID_BITS") = py::int_(fSpeakerModeValidBits);

    m.attr("DEVICE_FLAG_NOT_OPEN") = py::int_(fDeviceFlagNotOpen);
    m.attr("DEVICE_FLAG_CONNECTED") = py::int_(fDeviceFlagConnected);
    m.attr("DEVICE_FLAG_DEFAULT") = py::int_(fDeviceFlagDefault);
    m.attr("DEVICE_FLAG_STREAMER") = py::int_(fDeviceFlagStreamer);

    // TODO: bind UserData

    py::enum_<SampleFormat>(m, "SampleFormat")
        .value("PCM8", SampleFormat::ePcm8)
        .value("PCM16", SampleFormat::ePcm16)
        .value("PCM24", SampleFormat::ePcm24)
        .value("PCM32", SampleFormat::ePcm32)
        .value("PCM_FLOAT", SampleFormat::ePcmFloat)
        .value("PCM_COUNT", SampleFormat::ePcmCount)
        .value("VORBIS", SampleFormat::eVorbis)
        .value("FLAC", SampleFormat::eFlac)
        .value("OPUS", SampleFormat::eOpus)
        .value("MP3", SampleFormat::eMp3)
        .value("RAW", SampleFormat::eRaw)
        .value("DEFAULT", SampleFormat::eDefault)
        .value("COUNT", SampleFormat::eCount);

    // this is intentionally not bound since there is currently no python
    // functionality that could make use of this behavior
    // m.attr("AUDIO_IMAGE_FLAG_NO_CLEAR") = py::int_(fAudioImageNoClear);
    m.attr("AUDIO_IMAGE_FLAG_USE_LINES") = py::int_(fAudioImageUseLines);
    m.attr("AUDIO_IMAGE_FLAG_NOISE_COLOR") = py::int_(fAudioImageNoiseColor);
    m.attr("AUDIO_IMAGE_FLAG_MULTI_CHANNEL") = py::int_(fAudioImageMultiChannel);
    m.attr("AUDIO_IMAGE_FLAG_ALPHA_BLEND") = py::int_(fAudioImageAlphaBlend);
    m.attr("AUDIO_IMAGE_FLAG_SPLIT_CHANNELS") = py::int_(fAudioImageSplitChannels);

    py::class_<SoundFormat>(m, "SoundFormat")
        .def_readwrite("channels", &SoundFormat::channels)
        .def_readwrite("bits_per_sample", &SoundFormat::bitsPerSample)
        .def_readwrite("frame_size", &SoundFormat::frameSize)
        .def_readwrite("block_size", &SoundFormat::blockSize)
        .def_readwrite("frames_per_block", &SoundFormat::framesPerBlock)
        .def_readwrite("frame_rate", &SoundFormat::frameRate)
        .def_readwrite("channel_mask", &SoundFormat::channelMask)
        .def_readwrite("valid_bits_per_sample", &SoundFormat::validBitsPerSample)
        .def_readwrite("format", &SoundFormat::format);

    m.attr("INVALID_DEVICE_INDEX") = py::int_(kInvalidDeviceIndex);

    // DeviceCaps::thisSize isn't readable and is always constructed to sizeof(DeviceCaps)
    py::class_<DeviceCaps>(m, "DeviceCaps")
        .def_readwrite("index", &DeviceCaps::index)
        .def_readwrite("flags", &DeviceCaps::flags)
        .def_readwrite("guid", &DeviceCaps::guid)
        .def_readwrite("channels", &DeviceCaps::channels)
        .def_readwrite("frame_rate", &DeviceCaps::frameRate)
        .def_readwrite("format", &DeviceCaps::format)
        // python doesn't seem to like it when an array member is exposed
        .def("get_name", [](const DeviceCaps* self) -> const char* { return self->name; });

    m.attr("DEFAULT_FRAME_RATE") = py::int_(kDefaultFrameRate);
    m.attr("DEFAULT_CHANNEL_COUNT") = py::int_(kDefaultChannelCount);

    // FIXME: this doesn't work
    // m.attr("DEFAULT_FORMAT") = py::enum_<SampleFormat>(kDefaultFormat);

    /////// IAudioPlayback.h ///////
    m.attr("CONTEXT_PARAM_ALL") = py::int_(fContextParamAll);
    m.attr("CONTEXT_PARAM_SPEED_OF_SOUND") = py::int_(fContextParamSpeedOfSound);
    m.attr("CONTEXT_PARAM_WORLD_UNIT_SCALE") = py::int_(fContextParamWorldUnitScale);
    m.attr("CONTEXT_PARAM_LISTENER") = py::int_(fContextParamListener);
    m.attr("CONTEXT_PARAM_DOPPLER_SCALE") = py::int_(fContextParamDopplerScale);
    m.attr("CONTEXT_PARAM_VIRTUALIZATION_THRESHOLD") = py::int_(fContextParamVirtualizationThreshold);
    m.attr("CONTEXT_PARAM_SPATIAL_FREQUENCY_RATIO") = py::int_(fContextParamSpatialFrequencyRatio);
    m.attr("CONTEXT_PARAM_NON_SPATIAL_FREQUENCY_RATIO") = py::int_(fContextParamNonSpatialFrequencyRatio);
    m.attr("CONTEXT_PARAM_MASTER_VOLUME") = py::int_(fContextParamMasterVolume);
    m.attr("CONTEXT_PARAM_SPATIAL_VOLUME") = py::int_(fContextParamSpatialVolume);
    m.attr("CONTEXT_PARAM_NON_SPATIAL_VOLUME") = py::int_(fContextParamNonSpatialVolume);
    m.attr("CONTEXT_PARAM_DOPPLER_LIMIT") = py::int_(fContextParamDopplerLimit);
    m.attr("CONTEXT_PARAM_DEFAULT_PLAYBACK_MODE") = py::int_(fContextParamDefaultPlaybackMode);
    m.attr("CONTEXT_PARAM_VIDEO_LATENCY") = py::int_(fContextParamVideoLatency);

    m.attr("DEFAULT_SPEED_OF_SOUND") = py::float_(kDefaultSpeedOfSound);

    m.attr("VOICE_PARAM_ALL") = py::int_(fVoiceParamAll);
    m.attr("VOICE_PARAM_PLAYBACK_MODE") = py::int_(fVoiceParamPlaybackMode);
    m.attr("VOICE_PARAM_VOLUME") = py::int_(fVoiceParamVolume);
    m.attr("VOICE_PARAM_MUTE") = py::int_(fVoiceParamMute);
    m.attr("VOICE_PARAM_BALANCE") = py::int_(fVoiceParamBalance);
    m.attr("VOICE_PARAM_FREQUENCY_RATIO") = py::int_(fVoiceParamFrequencyRatio);
    m.attr("VOICE_PARAM_PRIORITY") = py::int_(fVoiceParamPriority);
    m.attr("VOICE_PARAM_PAUSE") = py::int_(fVoiceParamPause);
    m.attr("VOICE_PARAM_SPATIAL_MIX_LEVEL") = py::int_(fVoiceParamSpatialMixLevel);
    m.attr("VOICE_PARAM_DOPPLER_SCALE") = py::int_(fVoiceParamDopplerScale);
    m.attr("VOICE_PARAM_OCCLUSION_FACTOR") = py::int_(fVoiceParamOcclusionFactor);
    m.attr("VOICE_PARAM_EMITTER") = py::int_(fVoiceParamEmitter);
    m.attr("VOICE_PARAM_MATRIX") = py::int_(fVoiceParamMatrix);

    m.attr("PLAYBACK_MODE_SPATIAL") = py::int_(fPlaybackModeSpatial);
    m.attr("PLAYBACK_MODE_LISTENER_RELATIVE") = py::int_(fPlaybackModeListenerRelative);
    m.attr("PLAYBACK_MODE_DISTANCE_DELAY") = py::int_(fPlaybackModeDistanceDelay);
    m.attr("PLAYBACK_MODE_INTERAURAL_DELAY") = py::int_(fPlaybackModeInterauralDelay);
    m.attr("PLAYBACK_MODE_USE_DOPPLER") = py::int_(fPlaybackModeUseDoppler);
    m.attr("PLAYBACK_MODE_USE_REVERB") = py::int_(fPlaybackModeUseReverb);
    m.attr("PLAYBACK_MODE_USE_FILTERS") = py::int_(fPlaybackModeUseFilters);
    m.attr("PLAYBACK_MODE_MUTED") = py::int_(fPlaybackModeMuted);
    m.attr("PLAYBACK_MODE_PAUSED") = py::int_(fPlaybackModePaused);
    m.attr("PLAYBACK_MODE_FADE_IN") = py::int_(fPlaybackModeFadeIn);
    m.attr("PLAYBACK_MODE_SIMULATE_POSITION") = py::int_(fPlaybackModeSimulatePosition);
    m.attr("PLAYBACK_MODE_NO_POSITION_SIMULATION") = py::int_(fPlaybackModeNoPositionSimulation);
    m.attr("PLAYBACK_MODE_SPATIAL_MIX_LEVEL_MATRIX") = py::int_(fPlaybackModeSpatialMixLevelMatrix);
    m.attr("PLAYBACK_MODE_NO_SPATIAL_LOW_FREQUENCY_EFFECT") = py::int_(fPlaybackModeNoSpatialLowFrequencyEffect);
    m.attr("PLAYBACK_MODE_STOP_ON_SIMULATION") = py::int_(fPlaybackModeStopOnSimulation);
    m.attr("PLAYBACK_MODE_DEFAULT_USE_DOPPLER") = py::int_(fPlaybackModeDefaultUseDoppler);
    m.attr("PLAYBACK_MODE_DEFAULT_DISTANCE_DELAY") = py::int_(fPlaybackModeDefaultDistanceDelay);
    m.attr("PLAYBACK_MODE_DEFAULT_INTERAURAL_DELAY") = py::int_(fPlaybackModeDefaultInterauralDelay);
    m.attr("PLAYBACK_MODE_DEFAULT_USE_REVERB") = py::int_(fPlaybackModeDefaultUseReverb);
    m.attr("PLAYBACK_MODE_DEFAULT_USE_FILTERS") = py::int_(fPlaybackModeDefaultUseFilters);

    m.attr("ENTITY_FLAG_ALL") = py::int_(fEntityFlagAll);
    m.attr("ENTITY_FLAG_POSITION") = py::int_(fEntityFlagPosition);
    m.attr("ENTITY_FLAG_VELOCITY") = py::int_(fEntityFlagVelocity);
    m.attr("ENTITY_FLAG_FORWARD") = py::int_(fEntityFlagForward);
    m.attr("ENTITY_FLAG_UP") = py::int_(fEntityFlagUp);
    m.attr("ENTITY_FLAG_CONE") = py::int_(fEntityFlagCone);
    m.attr("ENTITY_FLAG_ROLLOFF") = py::int_(fEntityFlagRolloff);

    m.attr("ENTITY_FLAG_MAKE_PERP") = py::int_(fEntityFlagMakePerp);
    m.attr("ENTITY_FLAG_NORMALIZE") = py::int_(fEntityFlagNormalize);

    py::enum_<RolloffType>(m, "RolloffType")
        .value("INVERSE", RolloffType::eInverse)
        .value("LINEAR", RolloffType::eLinear)
        .value("LINEAR_SQUARE", RolloffType::eLinearSquare);

    m.attr("INSTANCES_UNLIMITED") = py::int_(kInstancesUnlimited);

    // this won't be done through flags in python
    // m.attr("DATA_FLAG_FORMAT_MASK") = py::int_(fDataFlagFormatMask);
    // m.attr("DATA_FLAG_FORMAT_AUTO") = py::int_(fDataFlagFormatAuto);
    // m.attr("DATA_FLAG_FORMAT_RAW") = py::int_(fDataFlagFormatRaw);
    // m.attr("DATA_FLAG_IN_MEMORY") = py::int_(fDataFlagInMemory);
    // m.attr("DATA_FLAG_STREAM") = py::int_(fDataFlagStream);
    // m.attr("DATA_FLAG_DECODE") = py::int_(fDataFlagDecode);
    // m.attr("DATA_FLAG_FORMAT_PCM") = py::int_(fDataFlagFormatPcm);
    // m.attr("DATA_FLAG_EMPTY") = py::int_(fDataFlagEmpty);
    // m.attr("DATA_FLAG_NO_NAME") = py::int_(fDataFlagNoName);
    // this won't ever be supported in python
    // m.attr("DATA_FLAG_USER_MEMORY") = py::int_(fDataFlagUserMemory);
    // not supported yet - we may not support it
    // m.attr("DATA_FLAG_USER_DECODE") = py::int_(fDataFlagUserDecode);
    m.attr("DATA_FLAG_SKIP_METADATA") = py::int_(fDataFlagSkipMetaData);
    m.attr("DATA_FLAG_SKIP_EVENT_POINTS") = py::int_(fDataFlagSkipEventPoints);
    m.attr("DATA_FLAG_CALC_PEAKS") = py::int_(fDataFlagCalcPeaks);

    m.attr("SAVE_FLAG_DEFAULT") = py::int_(fSaveFlagDefault);
    m.attr("SAVE_FLAG_STRIP_METADATA") = py::int_(fSaveFlagStripMetaData);
    m.attr("SAVE_FLAG_STRIP_EVENT_POINTS") = py::int_(fSaveFlagStripEventPoints);
    m.attr("SAVE_FLAG_STRIP_PEAKS") = py::int_(fSaveFlagStripPeaks);

    m.attr("MEMORY_LIMIT_THRESHOLD") = py::int_(kMemoryLimitThreshold);

    m.attr("META_DATA_TAG_ARCHIVAL_LOCATION") = std::string(kMetaDataTagArchivalLocation);
    m.attr("META_DATA_TAG_COMMISSIONED") = std::string(kMetaDataTagCommissioned);
    m.attr("META_DATA_TAG_CROPPED") = std::string(kMetaDataTagCropped);
    m.attr("META_DATA_TAG_DIMENSIONS") = std::string(kMetaDataTagDimensions);
    m.attr("META_DATA_TAG_DISC") = std::string(kMetaDataTagDisc);
    m.attr("META_DATA_TAG_DPI") = std::string(kMetaDataTagDpi);
    m.attr("META_DATA_TAG_EDITOR") = std::string(kMetaDataTagEditor);
    m.attr("META_DATA_TAG_ENGINEER") = std::string(kMetaDataTagEngineer);
    m.attr("META_DATA_TAG_KEYWORDS") = std::string(kMetaDataTagKeywords);
    m.attr("META_DATA_TAG_LANGUAGE") = std::string(kMetaDataTagLanguage);
    m.attr("META_DATA_TAG_LIGHTNESS") = std::string(kMetaDataTagLightness);
    m.attr("META_DATA_TAG_MEDIUM") = std::string(kMetaDataTagMedium);
    m.attr("META_DATA_TAG_PALETTE_SETTING") = std::string(kMetaDataTagPaletteSetting);
    m.attr("META_DATA_TAG_SUBJECT") = std::string(kMetaDataTagSubject);
    m.attr("META_DATA_TAG_SOURCE_FORM") = std::string(kMetaDataTagSourceForm);
    m.attr("META_DATA_TAG_SHARPNESS") = std::string(kMetaDataTagSharpness);
    m.attr("META_DATA_TAG_TECHNICIAN") = std::string(kMetaDataTagTechnician);
    m.attr("META_DATA_TAG_WRITER") = std::string(kMetaDataTagWriter);
    m.attr("META_DATA_TAG_ALBUM") = std::string(kMetaDataTagAlbum);
    m.attr("META_DATA_TAG_ARTIST") = std::string(kMetaDataTagArtist);
    m.attr("META_DATA_TAG_COPYRIGHT") = std::string(kMetaDataTagCopyright);
    m.attr("META_DATA_TAG_CREATION_DATE") = std::string(kMetaDataTagCreationDate);
    m.attr("META_DATA_TAG_DESCRIPTION") = std::string(kMetaDataTagDescription);
    m.attr("META_DATA_TAG_GENRE") = std::string(kMetaDataTagGenre);
    m.attr("META_DATA_TAG_ORGANIZATION") = std::string(kMetaDataTagOrganization);
    m.attr("META_DATA_TAG_TITLE") = std::string(kMetaDataTagTitle);
    m.attr("META_DATA_TAG_TRACK_NUMBER") = std::string(kMetaDataTagTrackNumber);
    m.attr("META_DATA_TAG_ENCODER") = std::string(kMetaDataTagEncoder);
    m.attr("META_DATA_TAG_ISRC") = std::string(kMetaDataTagISRC);
    m.attr("META_DATA_TAG_LICENSE") = std::string(kMetaDataTagLicense);
    m.attr("META_DATA_TAG_PERFORMER") = std::string(kMetaDataTagPerformer);
    m.attr("META_DATA_TAG_VERSION") = std::string(kMetaDataTagVersion);
    m.attr("META_DATA_TAG_LOCATION") = std::string(kMetaDataTagLocation);
    m.attr("META_DATA_TAG_CONTACT") = std::string(kMetaDataTagContact);
    m.attr("META_DATA_TAG_COMMENT") = std::string(kMetaDataTagComment);
    m.attr("META_DATA_TAG_SPEED") = std::string(kMetaDataTagSpeed);
    m.attr("META_DATA_TAG_START_TIME") = std::string(kMetaDataTagStartTime);
    m.attr("META_DATA_TAG_END_TIME") = std::string(kMetaDataTagEndTime);
    m.attr("META_DATA_TAG_SUBGENRE") = std::string(kMetaDataTagSubGenre);
    m.attr("META_DATA_TAG_BPM") = std::string(kMetaDataTagBpm);
    m.attr("META_DATA_TAG_PLAYLIST_DELAY") = std::string(kMetaDataTagPlaylistDelay);
    m.attr("META_DATA_TAG_FILE_NAME") = std::string(kMetaDataTagFileName);
    m.attr("META_DATA_TAG_ALBUM") = std::string(kMetaDataTagOriginalAlbum);
    m.attr("META_DATA_TAG_WRITER") = std::string(kMetaDataTagOriginalWriter);
    m.attr("META_DATA_TAG_PERFORMER") = std::string(kMetaDataTagOriginalPerformer);
    m.attr("META_DATA_TAG_ORIGINAL_YEAR") = std::string(kMetaDataTagOriginalYear);
    m.attr("META_DATA_TAG_PUBLISHER") = std::string(kMetaDataTagPublisher);
    m.attr("META_DATA_TAG_RECORDING_DATE") = std::string(kMetaDataTagRecordingDate);
    m.attr("META_DATA_TAG_INTERNET_RADIO_STATION_NAME") = std::string(kMetaDataTagInternetRadioStationName);
    m.attr("META_DATA_TAG_INTERNET_RADIO_STATION_OWNER") = std::string(kMetaDataTagInternetRadioStationOwner);
    m.attr("META_DATA_TAG_INTERNET_RADIO_STATION_URL") = std::string(kMetaDataTagInternetRadioStationUrl);
    m.attr("META_DATA_TAG_PAYMENT_URL") = std::string(kMetaDataTagPaymentUrl);
    m.attr("META_DATA_TAG_INTERNET_COMMERCIAL_INFORMATION_URL") =
        std::string(kMetaDataTagInternetCommercialInformationUrl);
    m.attr("META_DATA_TAG_INTERNET_COPYRIGHT_URL") = std::string(kMetaDataTagInternetCopyrightUrl);
    m.attr("META_DATA_TAG_WEBSITE") = std::string(kMetaDataTagWebsite);
    m.attr("META_DATA_TAG_INTERNET_ARTIST_WEBSITE") = std::string(kMetaDataTagInternetArtistWebsite);
    m.attr("META_DATA_TAG_AUDIO_SOURCE_WEBSITE") = std::string(kMetaDataTagAudioSourceWebsite);
    m.attr("META_DATA_TAG_COMPOSER") = std::string(kMetaDataTagComposer);
    m.attr("META_DATA_TAG_OWNER") = std::string(kMetaDataTagOwner);
    m.attr("META_DATA_TAG_TERMS_OF_USE") = std::string(kMetaDataTagTermsOfUse);
    m.attr("META_DATA_TAG_INITIAL_KEY") = std::string(kMetaDataTagInitialKey);

    m.attr("META_DATA_TAG_CLEAR_ALL_TAGS") = kMetaDataTagClearAllTags;

    py::class_<PeakVolumes>(m, "PeakVolumes")
        .def_readwrite("channels", &PeakVolumes::channels)
        //.def_readwrite("frame", &PeakVolumes::frame)
        //.def_readwrite("peak", &PeakVolumes::peak)
        .def_readwrite("peak_frame", &PeakVolumes::peakFrame)
        .def_readwrite("peak_volume", &PeakVolumes::peakVolume);

    m.attr("EVENT_POINT_INVALID_FRAME") = py::int_(kEventPointInvalidFrame);
    m.attr("EVENT_POINT_LOOP_INFINITE") = py::int_(kEventPointLoopInfinite);

    py::class_<EventPoint>(m, "EventPoint")
        .def(py::init<>())
        .def_readwrite("id", &EventPoint::id)
        .def_readwrite("frame", &EventPoint::frame)
        //.def_readwrite("label", &EventPoint::label)
        //.def_readwrite("text", &EventPoint::text)
        .def_readwrite("length", &EventPoint::length)
        .def_readwrite("loop_count", &EventPoint::loopCount)
        .def_readwrite("play_index", &EventPoint::playIndex);
    // FIXME: maybe we want to bind userdata

    py::enum_<UnitType>(m, "UnitType")
        .value("BYTES", UnitType::eBytes)
        .value("FRAMES", UnitType::eFrames)
        .value("MILLISECONDS", UnitType::eMilliseconds)
        .value("MICROSECONDS", UnitType::eMicroseconds);

    m.doc() = R"(
        This module contains bindings for the IAudioPlayback interface.
        This is the low-level audio playback interface for Carbonite.
    )";

    py::class_<ContextCaps> ctxCaps(m, "ContextCaps");
    ctxCaps.doc() = R"(
        The capabilities of the context object.  Some of these values are set
        at the creation time of the context object.  Others are updated when
        speaker positions are set or an output device is opened.
    )";

    py::class_<ContextParams> ctxParams(m, "ContextParams");
    ctxParams.doc() = R"(
        Context parameters block.  This can potentially contain all of a
        context's parameters and their current values.  This is used to both
        set and retrieve one or more of a context's parameters in a single
        call.  The set of fContextParam* flags that are passed to
        getContextParameter() or setContextParameter() indicates which values
        in the block are guaranteed to be valid.
    )";

    py::class_<ContextParams2> ctxParams2(m, "ContextParams2");
    ctxParams2.doc() = R"(
        Extended context parameters block.  This is used to set and retrieve
        extended context parameters and their current values.  This object
        must be attached to the 'ContextParams.ext' value and the
        'ContextParams.flags' value must have one or more flags related
        to the extended parameters set for them to be modified or retrieved.
    )";

    py::class_<LoopPointDesc> loopPointDesc(m, "LoopPointDesc");
    loopPointDesc.doc() = R"(
        Descriptor of a loop point to set on a voice.  This may be specified
        to change the current loop point on a voice with set_Loop_Point().
    )";

    py::class_<DspValuePair> dspValuePair(m, "DspValuePair");
    dspValuePair.def_readwrite("inner", &DspValuePair::inner);
    dspValuePair.def_readwrite("outer", &DspValuePair::outer);

    py::class_<EntityCone> cone(m, "EntityCone");
    cone.def_readwrite("inside_angle", &EntityCone::insideAngle);
    cone.def_readwrite("outside_angle", &EntityCone::outsideAngle);
    cone.def_readwrite("volume", &EntityCone::volume);
    cone.def_readwrite("low_pass_filter", &EntityCone::lowPassFilter);
    cone.def_readwrite("reverb", &EntityCone::reverb);
    cone.doc() = R"(
        defines a sound cone relative to an entity's front vector.  It is defined by two angles -
        the inner and outer angles.  When the angle between an emitter and the listener (relative
        to the entity's front vector) is smaller than the inner angle, the resulting DSP value
        will be the 'inner' value.  When the emitter-listener angle is larger than the outer
        angle, the resulting DSP value will be the 'outer' value.  For emitter-listener angles
        that are between the inner and outer angles, the DSP value will be interpolated between
        the inner and outer angles.  If a cone is valid for an entity, the @ref fEntityFlagCone
        flag should be set in @ref EntityAttributes::flags.

        Note that a cone's effect on the spatial volume of a sound is purely related to the angle
        between the emitter and listener.  Any distance attenuation is handled separately.
    )";

    py::class_<EntityAttributes> entityAttributes(m, "EntityAttributes");
    entityAttributes.def_readwrite("flags", &EntityAttributes::flags);
    entityAttributes.def_readwrite("position", &EntityAttributes::position);
    entityAttributes.def_readwrite("velocity", &EntityAttributes::velocity);
    entityAttributes.def_readwrite("forward", &EntityAttributes::forward);
    entityAttributes.def_readwrite("up", &EntityAttributes::up);
    entityAttributes.def_readwrite("cone", &EntityAttributes::cone);
    entityAttributes.doc() = R"(
        base spatial attributes of the entity.  This includes its position, orientation, and velocity
        and an optional cone.
    )";

    py::class_<RolloffDesc> rolloffDesc(m, "RolloffDesc");
    rolloffDesc.def_readwrite("type", &RolloffDesc::type);
    rolloffDesc.def_readwrite("near_distance", &RolloffDesc::nearDistance);
    rolloffDesc.def_readwrite("far_distance", &RolloffDesc::farDistance);
    rolloffDesc.doc() = R"(
        Descriptor of the rolloff mode and range.
        The C++ API allows rolloff curves to be set through this struct, but in
        python you need to use voice.set_rolloff_curve() to do this instead.
    )";

    py::class_<EmitterAttributes> emitterAttributes(m, "EmitterAttributes");
    emitterAttributes.def_readwrite("flags", &EmitterAttributes::flags);
    emitterAttributes.def_readwrite("position", &EmitterAttributes::position);
    emitterAttributes.def_readwrite("velocity", &EmitterAttributes::velocity);
    emitterAttributes.def_readwrite("forward", &EmitterAttributes::forward);
    emitterAttributes.def_readwrite("up", &EmitterAttributes::up);
    emitterAttributes.def_readwrite("cone", &EmitterAttributes::cone);
    emitterAttributes.def_readwrite("rolloff", &EmitterAttributes::rolloff);

    py::class_<VoiceParams::VoiceParamBalance> voiceParamBalance(m, "VoiceParamBalance");
    voiceParamBalance.def_readwrite("pan", &VoiceParams::VoiceParamBalance::pan);
    voiceParamBalance.def_readwrite("fade", &VoiceParams::VoiceParamBalance::fade);

    py::class_<VoiceParams::VoiceParamOcclusion> voiceParamOcclusion(m, "VoiceParamOcclusion");
    voiceParamOcclusion.def_readwrite("direct", &VoiceParams::VoiceParamOcclusion::direct);
    voiceParamOcclusion.def_readwrite("reverb", &VoiceParams::VoiceParamOcclusion::reverb);

    py::class_<VoiceParams> voiceParams(m, "VoiceParams");
    voiceParams.def_readwrite("playback_mode", &VoiceParams::playbackMode);
    voiceParams.def_readwrite("volume", &VoiceParams::volume);
    voiceParams.def_readwrite("balance", &VoiceParams::balance);
    voiceParams.def_readwrite("frequency_ratio", &VoiceParams::frequencyRatio);
    voiceParams.def_readwrite("priority", &VoiceParams::priority);
    voiceParams.def_readwrite("spatial_mix_level", &VoiceParams::spatialMixLevel);
    voiceParams.def_readwrite("doppler_scale", &VoiceParams::dopplerScale);
    voiceParams.def_readwrite("occlusion", &VoiceParams::occlusion);
    voiceParams.def_readwrite("emitter", &VoiceParams::emitter);
    voiceParams.doc() = R"(
        Voice parameters block.  This can potentially contain all of a voice's
        parameters and their current values.  This is used to both set and
        retrieve one or more of a voice's parameters in a single call.  The
        VOICE_PARAM_* flags that are passed to set_voice_parameters() or
        get_voice_parameters() determine which values in this block are
        guaranteed to be valid.
        The matrix parameter isn't available from this struct due to limitations
        in python; use voice.set_matrix() instead.
    )";

    py::class_<PythonContext> ctx(m, "Context");
    ctx.doc() = R"(
        The Context object for the audio system.
        Each individual Context represents an instance of the IAudioPlayback
        interface, as well as an individual connection to the system audio
        mixer/device. Only a small number of these can be opened for a given
        process.
    )";

    docString = R"(
        retrieves the current capabilities and settings for a context object.

        This retrieves the current capabilities and settings for a context
        object.  Some of these settings may change depending on whether the
        context has opened an output device or not.

        Args:
            No arguments.

        Returns:
            the context's current capabilities and settings.  This includes the
            speaker mode, speaker positions, maximum bus count, and information
            about the output device that is opened (if any).

    )";
    ctx.def("get_caps", [](PythonContext* self) -> ContextCaps { return *self->getContextCaps(); }, docString,
            py::call_guard<py::gil_scoped_release>());

    docString = R"(
        Sets one or more parameters on a context.

        This sets one or more context parameters in a single call.  Only
        parameters that have their corresponding flag set in paramsToSet will
        be modified.  If a change is to be relative to the context's current
        parameter value, the current value should be retrieved first, modified,
        then set.

        Args:
            paramsToSet: The set of flags to indicate which parameters in the
                         parameter block params are valid and should be set
                         on the context.  This may be zero or more of the
                         CONTEXT_PARAM_* bitflags.  If this is 0, the call will be
                         treated as a no-op.
            params:      The parameter(s) to be set on the context.  The flags
                         indicating which parameters need to be set are given
                         in paramsToSet.  Undefined behavior may occur if
                         a flag is set but its corresponding value(s) have not
                         been properly initialized.  This may not be None.

        Returns:
            No return value.
    )";
    ctx.def("set_parameters", &PythonContext::setContextParameters, docString, py::arg("paramsToSet"),
            py::arg("params"), py::call_guard<py::gil_scoped_release>());

    docString = R"(
        Retrieves one or more parameters for a context.

        This retrieves the current values of one or more of a context's
        parameters.  Only the parameter values listed in paramsToGet flags
        will be guaranteed to be valid upon return.

        Args:
            ParamsToGet: Flags indicating which parameter values need to be retrieved.
                         This should be a combination of the CONTEXT_PARAM_* bitflags.

        Returns:
            The requested parameters in a ContextParams struct. Everything else is default-initialized.
    )";
    ctx.def("get_parameters",
            [](PythonContext* self, ContextParamFlags paramsToGet) -> ContextParams {
                ContextParams tmp = {};
                self->getContextParameters(paramsToGet, &tmp);
                return tmp;
            },
            docString, py::arg("paramsToGet"), py::call_guard<py::gil_scoped_release>());
    docString = R"(
        Schedules a sound to be played on a voice.

        This schedules a sound object to be played on a voice.  The sounds current
        settings (ie: volume, pitch, playback frame rate, pan, etc) will be assigned to
        the voice as 'defaults' before playing.  Further changes can be made to the
        voice's state at a later time without affecting the sound's default settings.

        Once the sound finishes playing, it will be implicitly unassigned from the
        voice.  If the sound or voice have a callback set, a notification will be
        received for the sound having ended.

        If the playback of this sound needs to be stopped, it must be explicitly stopped
        from the returned voice object using stopVoice().  This can be called on a
        single voice or a voice group.

        Args:
             sound: The sound to schedule.
             flags: Flags that alter playback behavior. Must be a combination of PLAY_FLAG_* constants.
             valid_params: Which parameters in the params argument to use.
             params: The starting parameters for the voice. This conditionally used based on valid_params.
             loop_point: A descriptor for how to repeatedly play the sound. This can be None to only play once.
             play_start: The start offset to begin playing the sound at. This is measured in play_units.
             play_end: The stop offset to finish playing the sound at. This is measured in play_units.
             play_units: The units in which play_start and play_stop are measured.

         Returns:
             A new voice handle representing the playing sound.  Note that if no buses are
             currently available to play on or the voice's initial parameters indicated that
             it is not currently audible, the voice will be virtual and will not be played.
             The voice handle will still be valid in this case and can be operated on, but
             no sound will be heard from it until it is determined that it should be converted
             to a real voice.  This can only occur when the update() function is called.
             This voice handle does not need to be closed or destroyed.  If the voice finishes
             its play task, any future calls attempting to modify the voice will simply fail.

             None if the requested sound is already at or above its instance limit and the
             PLAY_FLAG_MAX_INSTANCES_SIMULATE flag is not used.

             None if the play task was invalid or could not be started properly.  This can
             most often occur in the case of streaming sounds if the sound's original data
             could not be opened or decoded properly.
    )";
    ctx.def("play_sound", &PythonContext::playSound, docString, py::arg("sound"), py::arg("flags") = 0,
            py::arg("valid_params") = 0, py::arg("params") = nullptr, py::arg("loop_point") = nullptr,
            py::arg("play_start") = 0, py::arg("play_end") = 0, py::arg("play_units") = UnitType::eFrames,
            py::call_guard<py::gil_scoped_release>());


    py::class_<carb::audio::PythonVoice> voice(m, "Voice");
    voice.doc() = R"(
        Represents a single instance of a playing sound.  A single sound object
        may be playing on multiple voices at the same time, however each voice
        may only be playing a single sound at any given time.
    )";

    docString = R"(
        Stops playback on a voice.

        This stops a voice from playing its current sound.  This will be
        silently ignored for any voice that is already stopped or for an
        invalid voice handle.  Once stopped, the voice will be returned to a
        'free' state and its sound data object unassigned from it.  The voice
        will be immediately available to be assigned a new sound object to play
        from.

        This will only schedule the voice to be stopped.  Its volume will be
        implicitly set to silence to avoid a popping artifact on stop.  The
        voice will continue to play for one more engine cycle until the volume
        level reaches zero, then the voice will be fully stopped and recycled.
        At most, 1ms of additional audio will be played from the voice's sound.

        Args:
            No Arguments.

        Returns:
            No return value.
    )";
    voice.def("stop", &PythonVoice::stop, docString, py::call_guard<py::gil_scoped_release>());

    docString = R"(
        Checks the playing state of a voice.

        This checks if a voice is currently playing.  A voice is considered
        playing if it has a currently active sound data object assigned to it
        and it is not paused.

        Args:
            Voice: The voice to check the playing state for.

        Returns:
            True if the requested voice is playing.
            False if the requested voice is not playing or is paused.
            False if the given voice handle is no longer valid.
    )";
    voice.def("is_playing", &PythonVoice::isPlaying, docString, py::call_guard<py::gil_scoped_release>());

    docString = R"(
    Sets a new loop point as current on a voice.

    This sets a new loop point for a playing voice.  This allows for behavior
    such as sound atlases or sound playlists to be played out on a single
    voice.

    When desc is None or the contents of the descriptor do not specify a new
    loop point, this will immediately break the loop that is currently playing
    on the voice.  This will have the effect of setting the voice's current
    loop count to zero.  The sound on the voice will continue to play out its
    current loop iteration, but will not loop again when it reaches its end.
    This is useful for stopping a voice that is playing an infinite loop or to
    prematurely stop a voice that was set to loop a specific number of times.
    This call will effectively be ignored if passed in a voice that is not
    currently looping.

    For streaming voices, updating a loop point will have a delay due to
    buffering the decoded data. The sound will loop an extra time if the loop
    point is changed after the buffering has started to consume another loop.
    The default buffer time for streaming sounds is currently 200 milliseconds,
    so this is the minimum slack time that needs to be given for a loop change.

    Args:
        voice: the voice to set the loop point on.  This may not be nullptr.
        point: descriptor of the new loop point to set.  This may contain a loop
               or event point from the sound itself or an explicitly specified
               loop point.  This may be nullptr to indicate that the current loop
               point should be removed and the current loop broken.  Similarly,
               an empty loop point descriptor could be passed in to remove the
               current loop point.

    Returns:
        True if the new loop point is successfully set.

        False if the voice handle is invalid or the voice has already stopped
        on its own.

        False if the new loop point is invalid, not found in the sound data
        object, or specifies a starting point or length that is outside the
        range of the sound data object's buffer.
    )";
    voice.def("set_loop_point", &PythonVoice::setLoopPoint, docString, py::arg("point"),
              py::call_guard<py::gil_scoped_release>());

    docString = R"(
    Retrieves the current play cursor position of a voice.

    This retrieves the current play position for a voice.  This is not
    necessarily the position in the buffer being played, but rather the
    position in the sound data object's stream.  For streaming sounds, this
    will be the offset from the start of the stream.  For non-streaming sounds,
    this will be the offset from the beginning of the sound data object's
    buffer.

    If the loop point for the voice changes during playback, the results of
    this call can be unexpected.  Once the loop point changes, there is no
    longer a consistent time base for the voice and the results will reflect
    the current position based off of the original loop's time base.  As long
    as the voice's original loop point remains (ie: setLoopPoint() is never
    called on the voice), the calculated position should be correct.

    It is the caller's responsibility to ensure that this is not called at the
    same time as changing the loop point on the voice or stopping the voice.

    Args:
        type:  The units to retrieve the current position in.

    Returns:
        The current position of the voice in the requested units.

        0 if the voice has stopped playing.

        The last play cursor position if the voice is paused.
    )";
    voice.def("get_play_cursor", &PythonVoice::getPlayCursor, docString, py::arg("type"),
              py::call_guard<py::gil_scoped_release>());

    docString = R"(
    Sets one or more parameters on a voice.

    This sets one or more voice parameters in a single call.  Only parameters that
    have their corresponding flag set in paramToSet will be modified.
    If a change is to be relative to the voice's current parameter value, the current
    value should be retrieved first, modified, then set.

    Args:
        paramsToSet:  Flags to indicate which of the parameters need to be updated.
                      This may be one or more of the fVoiceParam* flags.  If this is
                      0, this will simply be a no-op.
        params:       The parameter(s) to be set on the voice.  The flags indicating
                      which parameters need to be set must be set in
                      paramToSet by the caller.  Undefined behavior
                      may occur if a flag is set but its corresponding value(s) have
                      not been properly initialized.  This may not be None.

    Returns:
        No return value.
    )";
    voice.def("set_parameters", &PythonVoice::setParameters, docString, py::arg("params_to_set"), py::arg("params"),
              py::call_guard<py::gil_scoped_release>());

    docString = R"(
    Retrieves one or more parameters for a voice.

    This retrieves the current values of one or more of a voice's parameters.
    Only the parameter values listed in paramsToGet flags will be guaranteed
    to be valid upon return.

    Args:
        paramsToGet: Flags indicating which parameter values need to be retrieved.

    Returns:
        The requested parameters in a VoiceParams struct. Everything else is default-initialized.
    )";
    voice.def("get_parameters",
              [](PythonVoice* self, VoiceParamFlags paramsToGet) -> VoiceParams {
                  VoiceParams tmp = {};
                  self->getParameters(paramsToGet, &tmp);
                  return tmp;
              },
              docString, py::arg("params_to_get") = fVoiceParamAll, py::call_guard<py::gil_scoped_release>());
    docString = R"(
        Set flags to indicate how a sound is to be played back.
        This controls whether the sound is played as a spatial or non-spatial
        sound and how the emitter's attributes will be interpreted (ie: either
        world coordinates or listener relative).

        PLAYBACK_MODE_MUTED and PLAYBACK_MODE_PAUSED are ignored here; you'll
        need to use voice.set_mute() or voice.pause() to mute or pause the voice.

        Args:
            playback_mode: The playback mode flag set to set.

        Returns:
            None
    )";
    voice.def("set_playback_mode", &PythonVoice::setPlaybackMode, docString, py::arg("playback_mode"),
              py::call_guard<py::gil_scoped_release>());
    docString = R"(
        The volume level for the voice.

        Args:
            volume: The volume to set.
                    This should be 0.0 for silence or 1.0 for normal volume.
                    A negative value may be used to invert the signal.
                    A value greater than 1.0 will amplify the signal.
                    The volume level can be interpreted as a linear scale where
                    a value of 0.5 is half volume and 2.0 is double volume.
                    Any volume values in decibels must first be converted to a
                    linear volume scale before setting this value.

        Returns:
            None
    )";
    voice.def(
        "set_volume", &PythonVoice::setVolume, docString, py::arg("volume"), py::call_guard<py::gil_scoped_release>());
    docString = R"(
        Sets the mute state for a voice.

        Args:
            mute: When this is set to true, the voice's output will be muted.
                  When this is set to false, the voice's volume will be
                  restored to its previous level.
                  This is useful for temporarily silencing a voice without
                  having to clobber its current volume level or affect its
                  emitter attributes.

        Returns:
            None
    )";
    voice.def("set_mute", &PythonVoice::setMute, docString, py::arg("mute"), py::call_guard<py::gil_scoped_release>());
    docString = R"(
        Non-spatial sound positioning.
        These provide pan and fade values for the voice to give the impression
        that the sound is located closer to one of the quadrants of the
        acoustic space versus the others.
        These values are ignored for spatial sounds.

        Args:
            pan: The non-spatial panning value for a voice.
                 This is 0.0 to have the sound "centered" in all speakers.
                 This is -1.0 to have the sound balanced to the left side.
                 This is 1.0 to have the sound balanced to the right side.
                 The way the sound is balanced depends on  the number of channels.
                 For example, a mono sound will be balanced between the left
                 and right sides according to the panning value, but a stereo
                 sound will just have the left or right channels' volumes
                 turned down according to the panning value.
                 This value is ignored for spatial sounds.
                 The default value is 0.0.

                 Note that panning on non-spatial sounds should only be used
                 for mono or stereo sounds.
                 When it is applied to sounds with more channels, the results
                 are often undefined or may sound odd.

            fade: The non-spatial fade value for a voice.
                  This is 0.0 to have the sound "centered" in all speakers.
                  This is -1.0 to have the sound balanced to the back side.
                  This is 1.0 to have the sound balanced to the front side.
                  The way the sound is balanced depends on the number of channels.
                  For example, a mono sound will be balanced between the front
                  and back speakers according to the fade value, but a 5.1
                  sound will just have the front or back channels' volumes
                  turned down according to the fade value.
                  This value is ignored for spatial sounds.
                  The default value is 0.0.

                  Note that using fade on non-spatial sounds should only be
                  used for mono or stereo sounds.
                  When it is applied to sounds with more channels, the results
                  are often undefined or may sound odd.

        Returns:
            None
    )";
    voice.def("set_balance", &PythonVoice::setBalance, docString, py::arg("pan"), py::arg("fade"),
              py::call_guard<py::gil_scoped_release>());
    docString = R"(
        Set The frequency ratio for a voice.

        Args:
            ratio: This will be 1.0 to play back a sound at its normal rate, a
                   value less than 1.0 to lower the pitch and play it back more
                   slowly, and a value higher than 1.0 to increase the pitch
                   and play it back faster.
                   For example, a pitch scale of 0.5 will play back at half the
                   pitch (ie: lower frequency, takes twice the time to play
                   versus normal), and a pitch scale of 2.0 will play back at
                   double the pitch (ie: higher frequency, takes half the time
                   to play versus normal).
                   The default value is 1.0.

                   On some platforms, the frequency ratio may be silently
                   clamped to an acceptable range internally.
                   For example, a value of 0.0 is not allowed.
                   This will be clamped to the minimum supported value instead.

                   Note that the even though the frequency ratio *can* be set
                   to any value in the range from 1/1024 to 1024, this very
                   large range should only be used in cases where it is well
                   known that the particular sound being operated on will still
                   sound valid after the change.
                   In
                   the real world, some of these extreme frequency ratios may
                   make sense, but in the digital world, extreme frequency
                   ratios can result in audio corruption or even silence.
                   This
                   happens because the new frequency falls outside of the range
                   that is faithfully representable by either the audio device
                   or sound data itself.
                   For example, a 4KHz tone being played at a frequency ratio
                   larger than 6.0 will be above the maximum representable
                   frequency for a 48KHz device or sound file.
                   This case will result in a form of corruption known as
                   aliasing, where the frequency components above the maximum
                   representable frequency will become audio artifacts.
                   Similarly, an 800Hz tone being played at a frequency ratio
                   smaller than 1/40 will be inaudible because it falls below
                   the frequency range of the human ear.

                   In general, most use cases will find that the frequency
                   ratio range of [0.1, 10] is more than sufficient for their
                   needs.
                   Further, for many cases, the range from [0.2, 4] would
                   suffice.
                   Care should be taken to appropriately cap the used range for
                   this value.

        Returns:
            None
    )";
    voice.def("set_frequency_ratio", &PythonVoice::setFrequencyRatio, docString, py::arg("ratio"),
              py::call_guard<py::gil_scoped_release>());
    docString = R"(
        Set the playback priority of this voice.

        Args:
            priority: This is an arbitrary value whose scale is defined by the
                      host app.
                      A value of 0 is the default priority.
                      Negative values indicate lower priorities and positive
                      values indicate higher priorities.
                      This priority value helps to determine which voices are
                      the most important to be audible at any given time.
                      When all buses are busy, this value will be used to
                      compare against other playing voices to see if it should
                      steal a bus from another lower priority sound or if it
                      can wait until another bus finishes first.
                      Higher priority sounds will be ensured a bus to play on
                      over lower priority sounds.
                      If multiple sounds have the same priority levels, the
                      louder sound(s) will take priority.
                      When a higher priority sound is queued, it will try to
                      steal a bus from the quietest sound with lower or equal
                      priority.

        Returns:
            None
    )";
    voice.def("set_priority", &PythonVoice::setPriority, docString, py::arg("priority"),
              py::call_guard<py::gil_scoped_release>());
    docString = R"(
        Sets the spatial mix level for the voice.

        Args:
            level: The mix between the results of a voice's spatial sound
                   calculations and its non-spatial calculations.  When this is
                   set to 1.0, only the spatial sound calculations will affect
                   the voice's playback.
                   This is the default when state.
                   When set to 0.0, only the non-spatial sound calculations
                   will affect the voice's
                   playback.
                   When set to a value between 0.0 and 1.0, the results of the
                   spatial and non-spatial sound calculations will be mixed
                   with the weighting according to this value.
                   This value will be ignored if PLAYBACK_MODE_SPATIAL is not
                   set.
                   The default value is 1.0.  Values above 1.0 will be treated
                   as 1.0.
                   Values below 0.0 will be treated as 0.0.

                   PLAYBACK_MODE_SPATIAL_MIX_LEVEL_MATRIX affects the
                   non-spatial mixing behavior of this parameter for
                   multi-channel voices.
                   By default, a multi-channel spatial voice's non-spatial
                   component will treat each channel as a separate mono voice.
                   With the PLAYBACK_MODE_SPATIAL_MIX_LEVEL_MATRIX flag set,
                   the non-spatial component will be set with the specified
                   output matrix or the default output matrix.

        Returns:
            None
    )";
    voice.def("set_spatial_mix_level", &PythonVoice::setSpatialMixLevel, docString, py::arg("level"),
              py::call_guard<py::gil_scoped_release>());
    docString = R"(
        Sets the doppler scale value for the voice.
        This allows the result of internal doppler calculations to be scaled to emulate
        a time warping effect.


        Args:
            scale: This should be near 0.0 to greatly reduce the effect of the
                   doppler calculations, and up to 5.0 to exaggerate the
                   doppler effect.
                   A value of 1.0 will leave the calculated doppler factors
                   unmodified.
                   The default value is 1.0.

        Returns:
            None
    )";
    voice.def("set_doppler_scale", &PythonVoice::setDopplerScale, docString, py::arg("scale"),
              py::call_guard<py::gil_scoped_release>());
    docString = R"(
        Sets the occlusion factors for a voice.
        These values control automatic low pass filters that get applied to the
        Sets spatial sounds to simulate object occlusion between the emitter
        and listener positions.

        Args:
            direct: The occlusion factor for the direct path of the sound.
                    This is the path directly from the emitter to the listener.
                    This factor describes how occluded the sound's path
                    actually is.
                    A value of 1.0 means that the sound is fully occluded by an
                    object between the voice and the listener.
                    A value of 0.0 means that the sound is not occluded by any
                    object at all.
                    This defaults to 0.0.
                    This factor multiplies by EntityCone.low_pass_filter, if a
                    cone with a non 1.0 lowPassFilter value is specified.
                    Setting this to a value outside of [0.0, 1.0] will result
                    in an undefined low pass filter value being used.
            reverb: The occlusion factor for the reverb path of the sound.
                    This is the path taken for sounds reflecting back to the
                    listener after hitting a wall or other object.
                    A value of 1.0 means that the sound is fully occluded by an
                    object between the listener and the object that the sound
                    reflected off of.
                    A value of 0.0 means that the sound is not occluded by any
                    object at all.
                    This defaults to 1.0.

        Returns:
            None
    )";
    voice.def("set_occlusion", &PythonVoice::setOcclusion, docString, py::arg("direct"), py::arg("reverb"),
              py::call_guard<py::gil_scoped_release>());
    docString = R"(
        Set the channel mixing matrix to use for this voice.

        Args:
            matrix: The matrix to set.
                    The rows of this matrix represent
                    each output channel from this voice and the columns of this
                    matrix represent the input channels of this voice (e.g.
                    this is a inputChannels x outputChannels matrix).
                    Note that setting the matrix to be smaller than needed will
                    result in undefined behavior.
                    The output channel count will always be the number of audio
                    channels set on the context.
                    Each cell in the matrix should be a value from 0.0-1.0 to
                    specify the volume that this input channel should be mixed
                    into the output channel.
                    Setting negative values will invert the signal.
                    Setting values above 1.0 will amplify the signal past unity
                    gain when being mixed.

                    This setting is mutually exclusive with balance; setting
                    one will disable the other.
                    This setting is only available for spatial sounds if
                    PLAYBACK_MODE_SPATIAL_MIX_LEVEL_MATRIX if set in the
                    playback mode parameter.
                    Multi-channel spatial audio is interpreted as multiple
                    emitters existing at the same point in space, so a purely
                    spatial voice cannot have an output matrix specified.

                    Setting this to None will reset the matrix to the default
                    for the given channel count.
                    The following table shows the speaker modes that are used
                    for the default output matrices.
                    Voices with a speaker mode that is not in the following
                    table will use the default output matrix for the speaker
                    mode in the following table that has the same number of
                    channels.
                    If there is no default matrix for the channel count of the
                    @ref Voice, the output matrix will have 1.0 in the any cell
                    (i, j) where i == j and 0.0 in all other cells.

        Returns:
            None
    )";
    voice.def(
        "set_matrix", &PythonVoice::setMatrix, docString, py::arg("matrix"), py::call_guard<py::gil_scoped_release>());
    docString = R"(
        Set the voice's position.

        Args:
            position: The current position of the voice in world units.
                      This should only be expressed in meters if the world
                      units scale is set to 1.0 for this context.

        Returns:
            None
    )";
    voice.def("set_position", &PythonVoice::setPosition, docString, py::arg("position"),
              py::call_guard<py::gil_scoped_release>());
    docString = R"(
        Set the voice's velocity.

        Args:
            velocity: The current velocity of the voice in world units per second.
                      This should only be expressed in meters per second if the
                      world units scale is set to 1.0 with for the context.
                      The magnitude of this vector will be taken as the
                      listener's current speed and the vector's direction will
                      indicate the listener's current direction.  This vector
                      should not be normalized unless the listener's speed is
                      actually 1.0 units per second.
                      This may be a zero vector if the listener is not moving.

        Returns:
            None
    )";
    voice.def("set_velocity", &PythonVoice::setVelocity, docString, py::arg("velocity"),
              py::call_guard<py::gil_scoped_release>());
    docString = R"(
        Set custom rolloff curves on the voice.

        Args:
            type: The default type of rolloff calculation to use for all DSP
                  values that are not overridden by a custom curve.
            near_distance: The near distance range for the sound.
                           This is specified in arbitrary world units.
                           When a custom curve is used, this near distance will
                           map to a distance of 0.0 on the curve.
                           This must be less than the far_distance distance.
                           The near distance is the closest distance that the
                           emitter's attributes start to rolloff at.
                           At distances closer than this value, the calculated
                           DSP values will always be the same as if they were
                           at the near distance.
            far_distance: The far distance range for the sound.
                          This is specified in arbitrary world units.
                          When a custom curve is used, this far distance will
                          map to a distance of 1.0 on the curve.  This must be
                          greater than the @ref nearDistance distance.
                          The far distance is the furthest distance that the
                          emitters attributes will rolloff at.
                          At distances further than this value, the calculated
                          DSP values will always be the same as if they were at
                          the far distance (usually silence).
                          Emitters further than this distance will often become
                          inactive in the scene since they cannot be heard any
                          more.
            volume: The custom curve used to calculate volume attenuation over
                    distance.
                    This must be a normalized curve such that a distance of 0.0
                    maps to the near_distance distance and a distance of 1.0
                    maps to the far_distance distance.
                    When specified, this overrides the rolloff calculation
                    specified by type when calculating volume attenuation.
                    If this is an empty array, the parameter will be ignored.
            low_frequency: The custom curve used to calculate low frequency
                           effect volume over distance.
                           This must be a normalized curve such that a distance
                           of 0.0 maps to the near_distance distance and a
                           distance of 1.0 maps to the far_distance distance.
                           When specified, this overrides the rolloff
                           calculation specified by type when calculating
                           the low frequency effect volume.
                           If this is an empty array, the parameter will be
                           ignored.
            low_pass_reverb: The custom curve used to calculate low pass filter
                             parameter on the direct path over distance.
                             This must be a normalized curve such that a
                             distance of 0.0 maps to the near_distance distance
                             and a distance of 1.0 maps to the far_distance
                             distance.
                             When specified, this overrides the rolloff
                             calculation specified by type when calculating the
                             low pass filter parameter.
                             If this is an empty array, the parameter will be
                             ignored.
            low_pass_reverb: The custom curve used to calculate low pass filter
                             parameter on the reverb path over distance.
                             This must be a normalized curve such that a
                             distance of 0.0 maps to the near_distance distance
                             and a distance of 1.0 maps to the far_distance
                             distance.
                             When specified, this overrides the rolloff
                             calculation specified by type when calculating the
                             low pass filter parameter.
                             If this is an empty array, the parameter will be
                             ignored.
            reverb: The custom curve used to calculate reverb mix level over
                    distance.
                    This must be a normalized curve such that a distance of 0.0
                    maps to the near_distance distance and a distance of 1.0
                    maps to the @ref farDistance distance.
                    When specified, this overrides the rolloff calculation
                    specified by type when calculating the low pass filter
                    parameter.
                    If this is an empty array, the parameter will be ignored.

        returns:
            None
    )";
    voice.def("set_rolloff_curve", &PythonVoice::setRolloffCurve, docString, py::arg("type"), py::arg("near_distance"),
              py::arg("far_distance"), py::arg("volume") = std::vector<Float2>(),
              py::arg("low_frequency") = std::vector<Float2>(), py::arg("low_pass_direct") = std::vector<Float2>(),
              py::arg("low_pass_reverb") = std::vector<Float2>(), py::arg("reverb") = std::vector<Float2>());


    py::class_<carb::audio::PlaybackContextDesc> playbackContextDesc(m, "PlaybackContextDesc");

    carb::defineInterfaceClass<IAudioPlayback>(m, "IAudioPlayback", "acquire_playback_interface")
        .def("create_context",
             [](IAudioPlayback* self, PlaybackContextDesc desc) -> PythonContext {
                 Context* ctx = self->createContext(&desc);
                 return PythonContext(self, ctx);
             },
             py::arg("desc") = PlaybackContextDesc());


    carb::defineInterfaceClass<IAudioData>(m, "IAudioData", "acquire_data_interface")
        .def("create_sound_from_file",
             [](IAudioData* self, const char* fileName, SampleFormat decodedFormat, DataFlags flags, bool streaming,
                size_t autoStream) -> PythonSoundData* {
                 SoundData* tmp = createSoundFromFile(self, fileName, streaming, autoStream, decodedFormat, flags);
                 if (tmp == nullptr)
                     throw std::runtime_error("failed to create a SoundData object");
                 return new PythonSoundData(self, tmp);
             },
             R"(   Create a SoundData object from a file on disk.

    Args:
        filename      The name of the file on disk to create the new sound
                      data object from.
        decodedFormat The format you want the audio to be decoded into.
                      Although you can retrieve the sound's data through python
                      in any format, the data will be internally stored as this
                      format.  This is only important if you aren't creating a
                      decoded sound.  This defaults to SampleFormat.DEFAULT,
                      which will decode the sound to float for now.
        flags         Optional flags to change the behavior.  This can be any
                      of: DATA_FLAG_SKIP_METADATA, DATA_FLAG_SKIP_EVENT_POINTS
                      or DATA_FLAG_CALC_PEAKS.
        streaming     Set to True to create a streaming sound.  Streaming
                      sounds aren't loaded into memory; they remain on disk and
                      are decoded in chunks as needed.  This defaults to False.
        autoStream    The threshold in bytes at which the new sound data
                      object will decide to stream instead of decode into
                      memory.  If the decoded size of the sound will be
                      larger than this value, it will be streamed from its
                      original source instead of decoded.  Set this to 0
                      to disable auto-streaming. This defaults to 0.

    Returns:
        The new sound data if successfully created and loaded.

        An exception is thrown if the sound could not be loaded. This could
        happen if the file does not exist, the file is not a supported type,
        the file is corrupt or some other error occurred during decode.
)",
             py::arg("fileName"), py::arg("decodedFormat") = SampleFormat::eDefault, py::arg("flags") = 0,
             py::arg("streaming") = false, py::arg("autoStream") = 0, py::call_guard<py::gil_scoped_release>())
        .def("create_sound_from_blob",
             [](IAudioData* self, const py::bytes blob, SampleFormat decodedFormat, DataFlags flags, bool streaming,
                size_t autoStream) -> PythonSoundData* {
                 // this is extremely inefficient, but this appears to be the only way to get the data
                 std::string s = blob;
                 SoundData* tmp =
                     createSoundFromBlob(self, s.c_str(), s.length(), streaming, autoStream, decodedFormat, flags);
                 if (tmp == nullptr)
                     throw std::runtime_error("failed to create a SoundData object");
                 return new PythonSoundData(self, tmp);
             },
             R"(   Create a SoundData object from a data blob in memory

    Args:
        blob          A bytes object which contains the raw data for an audio
                      file which has some sort of header.  Raw PCM data will
                      not work with this function.  Note that due to the way
                      python works, these bytes will be copied into the
                      SoundData object's internal buffer if the sound is
                      streaming.
        decodedFormat The format you want the audio to be decoded into.
                      Although you can retrieve the sound's data through python
                      in any format, the data will be internally stored as this
                      format.  This is only important if you aren't creating a
                      decoded sound.  This defaults to SampleFormat.DEFAULT,
                      which will decode the sound to float for now.
        flags         Optional flags to change the behavior.  This can be any
                      of: DATA_FLAG_SKIP_METADATA, DATA_FLAG_SKIP_EVENT_POINTS
                      or DATA_FLAG_CALC_PEAKS.
        streaming     Set to True to create a streaming sound.  Streaming
                      sounds aren't loaded into memory; the audio data remains
                      in its encoded form in memory and are decoded in chunks
                      as needed. This is mainly useful for compressed formats
                      which will expand when decoded. This defaults to False.
        autoStream    The threshold in bytes at which the new sound data
                      object will decide to stream instead of decode into
                      memory.  If the decoded size of the sound will be
                      larger than this value, it will be streamed from its
                      original source instead of decoded.  Set this to 0
                      to disable auto-streaming. This defaults to 0.

    Returns:
        The new sound data if successfully created and loaded.

        An exception is thrown if the sound could not be loaded. This could
        happen if the blob is an unsupported audio format, the blob is corrupt
        or some other error during decoding.

)",
             py::arg("blob"), py::arg("decodedFormat") = SampleFormat::eDefault, py::arg("flags") = 0,
             py::arg("streaming") = false, py::arg("autoStream") = 0, py::call_guard<py::gil_scoped_release>())
        .def("create_sound_from_uint8_pcm",
             [](IAudioData* self, const std::vector<uint8_t>& pcm, size_t channels, size_t frameRate,
                SpeakerMode channelMask) -> PythonSoundData* {
                 return PythonSoundData::fromRawBlob(
                     self, pcm.data(), pcm.size(), SampleFormat::ePcm8, channels, frameRate, channelMask);
             },
             R"(    Create a SoundData object from raw 8 bit unsigned integer PCM data.

    Args:
        pcm          The audio data to load into the SoundData object.
                     This will be copied to an internal buffer in the object.
        channels     The number of channels of data in each frame of the audio
                     data.
        frame_rate   The number of frames per second that must be played back
                     for the audio data to sound 'normal' (ie: the way it was
                     recorded or produced).
        channel_mask the channel mask for the audio data.  This specifies which
                     speakers the stream is intended for and will be a
                     combination of one or more of the Speaker names or a
                     SpeakerMode name.  The channel mapping will be set to the
                     defaults if set to SPEAKER_MODE_DEFAULT, which is the
                     default value for this parameter.

    Returns:
        The new sound data if successfully created and loaded.

        An exception may be thrown if an out-of-memory situation occurs or some
        other error occurs while creating the object.
)",
             py::arg("pcm"), py::arg("channels"), py::arg("frame_rate"), py::arg("channel_mask") = kSpeakerModeDefault,
             py::call_guard<py::gil_scoped_release>())
        .def("create_sound_from_int16_pcm",
             [](IAudioData* self, const std::vector<int16_t>& pcm, size_t channels, size_t frameRate,
                SpeakerMode channelMask) -> PythonSoundData* {
                 return PythonSoundData::fromRawBlob(
                     self, pcm.data(), pcm.size(), SampleFormat::ePcm16, channels, frameRate, channelMask);
             },
             R"(    Create a SoundData object from raw 16 bit signed integer PCM data.

    Args:
        pcm          The audio data to load into the SoundData object.
                     This will be copied to an internal buffer in the object.
        channels     The number of channels of data in each frame of the audio
                     data.
        frame_rate   The number of frames per second that must be played back
                     for the audio data to sound 'normal' (ie: the way it was
                     recorded or produced).
        channel_mask the channel mask for the audio data.  This specifies which
                     speakers the stream is intended for and will be a
                     combination of one or more of the Speaker names or a
                     SpeakerMode name.  The channel mapping will be set to the
                     defaults if set to SPEAKER_MODE_DEFAULT, which is the
                     default value for this parameter.

    Returns:
        The new sound data if successfully created and loaded.

        An exception may be thrown if an out-of-memory situation occurs or some
        other error occurs while creating the object.
)",
             py::arg("pcm"), py::arg("channels"), py::arg("frame_rate"), py::arg("channel_mask") = kSpeakerModeDefault,
             py::call_guard<py::gil_scoped_release>())
        .def("create_sound_from_int32_pcm",
             [](IAudioData* self, const std::vector<int32_t>& pcm, size_t channels, size_t frameRate,
                SpeakerMode channelMask) -> PythonSoundData* {
                 return PythonSoundData::fromRawBlob(
                     self, pcm.data(), pcm.size(), SampleFormat::ePcm32, channels, frameRate, channelMask);
             },
             R"(    Create a SoundData object from raw 32 bit signed integer PCM data.

    Args:
        pcm          The audio data to load into the SoundData object.
                     This will be copied to an internal buffer in the object.
        channels     The number of channels of data in each frame of the audio
                     data.
        frame_rate   The number of frames per second that must be played back
                     for the audio data to sound 'normal' (ie: the way it was
                     recorded or produced).
        channel_mask the channel mask for the audio data.  This specifies which
                     speakers the stream is intended for and will be a
                     combination of one or more of the Speaker names or a
                     SpeakerMode name.  The channel mapping will be set to the
                     defaults if set to SPEAKER_MODE_DEFAULT, which is the
                     default value for this parameter.

    Returns:
        The new sound data if successfully created and loaded.

        An exception may be thrown if an out-of-memory situation occurs or some
        other error occurs while creating the object.
)",
             py::arg("pcm"), py::arg("channels"), py::arg("frame_rate"), py::arg("channel_mask") = kSpeakerModeDefault,
             py::call_guard<py::gil_scoped_release>())
        .def("create_sound_from_float_pcm",
             [](IAudioData* self, const std::vector<float>& pcm, size_t channels, size_t frameRate,
                SpeakerMode channelMask) -> PythonSoundData* {
                 return PythonSoundData::fromRawBlob(
                     self, pcm.data(), pcm.size(), SampleFormat::ePcmFloat, channels, frameRate, channelMask);
             },
             R"(    Create a SoundData object from raw 32 bit float PCM data.

    Args:
        pcm          The audio data to load into the SoundData object.
                     This will be copied to an internal buffer in the object.
        channels     The number of channels of data in each frame of the audio
                     data.
        frame_rate   The number of frames per second that must be played back
                     for the audio data to sound 'normal' (ie: the way it was
                     recorded or produced).
        channel_mask the channel mask for the audio data.  This specifies which
                     speakers the stream is intended for and will be a
                     combination of one or more of the Speaker names or a
                     SpeakerMode name.  The channel mapping will be set to the
                     defaults if set to SPEAKER_MODE_DEFAULT, which is the
                     default value for this parameter.

    Returns:
        The new sound data if successfully created and loaded.

        An exception may be thrown if an out-of-memory situation occurs or some
        other error occurs while creating the object.
)",
             py::arg("pcm"), py::arg("channels"), py::arg("frame_rate"), py::arg("channel_mask") = kSpeakerModeDefault,
             py::call_guard<py::gil_scoped_release>())
        .def("create_empty_sound",
             [](IAudioData* self, SampleFormat format, size_t channels, size_t frameRate, size_t bufferLength,
                UnitType units, const char* name, SpeakerMode channelMask) -> PythonSoundData* {
                 (void)channelMask; // FIXME: channelMask!?
                 SoundData* tmp = createEmptySound(self, format, frameRate, channels, bufferLength, units, name);
                 if (tmp == nullptr)
                     throw std::runtime_error("failed to create a SoundData object");
                 return new PythonSoundData(self, tmp);
             },
             R"(    Create a SoundData object with an empty buffer that can be written to.

    After creating a SoundData object with this, you will need to call one of
    the write_buffer_*() functions to load your data into the object, then you
    will need to call set_valid_length() to indicate how much of the sound now
    contains valid data.


    Args:
        decodedFormat The format for the SoundData object's buffer.
                      Although you can retrieve the sound's data through python
                      in any format, the data will be internally stored as this
                      format. This defaults to SampleFormat.DEFAULT, which will
                      use float for the buffer.
        channels      The number of channels of data in each frame of the audio
                      data.
        frame_rate    The number of frames per second that must be played back
                      for the audio data to sound 'normal' (ie: the way it was
                      recorded or produced).
        buffer_length How long you want the buffer to be.
        units         How buffer_length will be interpreted.  This defaults to
                      UnitType.FRAMES.
        name          An optional name that can be given to the SoundData
                      object to make it easier to track. This can be None if it
                      is not needed. This defaults to None.
        channel_mask  The channel mask for the audio data.  This specifies which
                      speakers the stream is intended for and will be a
                      combination of one or more of the Speaker names or a
                      SpeakerMode name.  The channel mapping will be set to the
                      defaults if set to SPEAKER_MODE_DEFAULT, which is the
                      default value for this parameter.

    Returns:
        The new sound data if successfully created and loaded.

        An exception may be thrown if an out-of-memory situation occurs or some
        other error occurs while creating the object.
)",
             py::arg("format"), py::arg("channels"), py::arg("frame_rate"), py::arg("buffer_length"),
             py::arg("units") = UnitType::eFrames, py::arg("name") = nullptr,
             py::arg("channel_mask") = kSpeakerModeDefault, py::call_guard<py::gil_scoped_release>());

    py::class_<PythonSoundData> sound(m, "SoundData");
    sound.def("get_name", &PythonSoundData::getName,
              R"(    Retrieve the name of a SoundData object.

    Returns:
        The name that was given to the object.

        This will return None if the object has no name.
)",
              py::call_guard<py::gil_scoped_release>());
    sound.def("is_decoded", &PythonSoundData::isDecoded,
              R"(    Query if the SoundData object is decoded or streaming.

    Returns:
        True if the object is decoded.
        False if the object is streaming.
)",
              py::call_guard<py::gil_scoped_release>());
    sound.def("get_format", &PythonSoundData::getFormat,
              R"(    Query the SoundData object's format.

    Returns:
        For a sound that was decoded on load, this represents the format of the
        audio data in the SoundData object's buffer.

        For a streaming sound, this returns the format of the underlying sound
        asset that is being streamed.
)",
              py::call_guard<py::gil_scoped_release>());
    sound.def("get_length", &PythonSoundData::getLength,
              R"(    Query the SoundData object's buffer length.

    Args:
        units The unit type that will be returned. This defaults to UnitType.FRAMES.

    Returns:
        The length of the SoundData object's buffer.
)",
              py::arg("units") = UnitType::eFrames, py::call_guard<py::gil_scoped_release>());
    sound.def("set_valid_length", &PythonSoundData::setValidLength,
              R"(    Set the length of the valid portion of the SoundData object's buffer.

    Args:
        length The new valid length to be set.
        units  How length will be interpreted. This defaults to UnitType.FRAMES.

    Returns:
        The length of the SoundData object's buffer.
)",
              py::arg("length"), py::arg("units") = UnitType::eFrames, py::call_guard<py::gil_scoped_release>());
    sound.def("get_valid_length", &PythonSoundData::getValidLength,
              R"(    Query the SoundData object's buffer length.

    Args:
        units The unit type that will be returned. This defaults to
              UnitType.FRAMES.

    Returns:
        The length of the SoundData object's buffer.
)",
              py::arg("units") = UnitType::eFrames, py::call_guard<py::gil_scoped_release>());
    sound.def("get_buffer_as_uint8", &PythonSoundData::getBufferU8,
              R"(    Retrieve a buffer of audio from the SoundData object in unsigned 8 bit
    integer PCM.

    Args:
        length The length of the buffer you want to retrieve.
               This will be clamped if the SoundData object does not have this
               much data available.
        offset The offset in the SoundData object to start reading from.
        units  How length and offset will be interpreted. This defaults to
               UnitType.FRAMES.

    Returns:
        A buffer of audio data from the SoundData object in unsigned 8 bit
        integer format. The format is a list containing integer data with
        values in the range [0, 255].
)",
              py::arg("length") = 0, py::arg("offset") = 0, py::arg("units") = UnitType::eFrames,
              py::call_guard<py::gil_scoped_release>());
    sound.def("get_buffer_as_int16", &PythonSoundData::getBufferS16,
              R"(    Retrieve a buffer of audio from the SoundData object in signed 16 bit
    integer PCM.

    Args:
        length The length of the buffer you want to retrieve.
               This will be clamped if the SoundData object does not have this
               much data available.
        offset The offset in the SoundData object to start reading from.
        units  How length and offset will be interpreted. This defaults to
               UnitType.FRAMES.

    Returns:
        A buffer of audio data from the SoundData object in signed 16 bit
        integer format. The format is a list containing integer data with
        values in the range [-32768, 32767].
)",
              py::arg("length") = 0, py::arg("offset") = 0, py::arg("units") = UnitType::eFrames,
              py::call_guard<py::gil_scoped_release>());
    sound.def("get_buffer_as_int32", &PythonSoundData::getBufferS32,
              R"(    Retrieve a buffer of audio from the SoundData object in signed 32 bit
    integer PCM.

    Args:
        length The length of the buffer you want to retrieve.
               This will be clamped if the SoundData object does not have this
               much data available.
        offset The offset in the SoundData object to start reading from.
        units  How length and offset will be interpreted. This defaults to
               UnitType.FRAMES.

    Returns:
        A buffer of audio data from the SoundData object in signed 32 bit
        integer format. The format is a list containing integer data with
        values in the range [-2147483648, 2147483647].
)",
              py::arg("length") = 0, py::arg("offset") = 0, py::arg("units") = UnitType::eFrames,
              py::call_guard<py::gil_scoped_release>());
    sound.def("get_buffer_as_float", &PythonSoundData::getBufferFloat,
              R"(    Retrieve a buffer of audio from the SoundData object in 32 bit float PCM.

    Args:
        length The length of the buffer you want to retrieve.
               This will be clamped if the SoundData object does not have this
               much data available.
        offset The offset in the SoundData object to start reading from.
        units  How length and offset will be interpreted. This defaults to
               UnitType.FRAMES.

    Returns:
        A buffer of audio data from the SoundData object in signed 32 bit
        integer format. The format is a list containing integer data with
        values in the range [-1.0, 1.0].
)",
              py::arg("length") = 0, py::arg("offset") = 0, py::arg("units") = UnitType::eFrames,
              py::call_guard<py::gil_scoped_release>());
    sound.def("write_buffer_with_uint8", &PythonSoundData::writeBufferU8,
              R"(    Write a buffer of audio to the SoundData object with unsigned 8 bit PCM
    data.

    Args:
        data   The buffer of data to write to the SoundData object.
               This must be a list of integer values representable as uint8_t.
        offset The offset in the SoundData object to start reading from.
        units  How offset will be interpreted. This defaults to
               UnitType.FRAMES.

    Returns:
        No return value.

        This will throw an exception if this is not a writable sound object.
        Only sounds that were created empty or from raw PCM data are writable.
)",
              py::arg("data"), py::arg("offset") = 0, py::arg("units") = UnitType::eFrames,
              py::call_guard<py::gil_scoped_release>());
    sound.def("write_buffer_with_int16", &PythonSoundData::writeBufferS16,
              R"(    Write a buffer of audio to the SoundData object with signed 16 bit PCM
    data.

    Args:
        data   The buffer of data to write to the SoundData object.
               This must be a list of integer values representable as int16_t.
        offset The offset in the SoundData object to start reading from.
        units  How offset will be interpreted. This defaults to
               UnitType.FRAMES.

    Returns:
        No return value.

        This will throw an exception if this is not a writable sound object.
        Only sounds that were created empty or from raw PCM data are writable.
)",
              py::arg("data"), py::arg("offset") = 0, py::arg("units") = UnitType::eFrames,
              py::call_guard<py::gil_scoped_release>());
    sound.def("write_buffer_with_int32", &PythonSoundData::writeBufferS32,
              R"(    Write a buffer of audio to the SoundData object with signed 32 bit PCM
    data.

    Args:
        data   The buffer of data to write to the SoundData object.
               This must be a list of integer values representable as int32_t.
        offset The offset in the SoundData object to start reading from.
        units  How offset will be interpreted. This defaults to
               UnitType.FRAMES.

    Returns:
        No return value.

        This will throw an exception if this is not a writable sound object.
        Only sounds that were created empty or from raw PCM data are writable.
)",
              py::arg("data"), py::arg("offset") = 0, py::arg("units") = UnitType::eFrames,
              py::call_guard<py::gil_scoped_release>());
    sound.def("write_buffer_with_float", &PythonSoundData::writeBufferFloat,
              R"(    Write a buffer of audio to the SoundData object with 32 bit float PCM
    data.

    Args:
        data   The buffer of data to write to the SoundData object.
               This must be a list of integer values representable as float.
        offset The offset in the SoundData object to start reading from.
        units  How offset will be interpreted. This defaults to
               UnitType.FRAMES.

    Returns:
        No return value.

        This will throw an exception if this is not a writable sound object.
        Only sounds that were created empty or from raw PCM data are writable.
)",
              py::arg("data"), py::arg("offset") = 0, py::arg("units") = UnitType::eFrames,
              py::call_guard<py::gil_scoped_release>());
    sound.def("get_memory_used", &PythonSoundData::getMemoryUsed,
              R"(    Query the amount of memory that's in use by a SoundData object.

    This retrieves the amount of memory used by a single sound data object.  This
    will include all memory required to store the audio data itself, to store the
    object and all its parameters, and the original filename (if any).  This
    information is useful for profiling purposes to investigate how much memory
    the audio system is using for a particular scene.

    Returns:
        The amount of memory in use by this sound, in bytes.
)",
              py::call_guard<py::gil_scoped_release>());
    sound.def("get_max_instances", &PythonSoundData::getMaxInstances,
              R"(    Query the SoundData object's max instance count.

    This retrieves the current maximum instance count for a sound.  This limit
    is used to prevent too many instances of a sound from being played
    simultaneously.  With the limit set to unlimited, playing too many
    instances can result in serious performance penalties and serious clipping
    artifacts caused by too much constructive interference.

    Returns:
        The SoundData object's max instance count.
)",
              py::call_guard<py::gil_scoped_release>());
    sound.def("set_max_instances", &PythonSoundData::setMaxInstances,
              R"(    Set the SoundData object's max instance count.

    This sets the new maximum playing instance count for a sound.  This limit will
    prevent the sound from being played until another instance of it finishes playing
    or simply cause the play request to be ignored completely.  This should be used
    to limit the use of frequently played sounds so that they do not cause too much
    of a processing burden in a scene or cause too much constructive interference
    that could lead to clipping artifacts.  This is especially useful for short
    sounds that are played often (ie: gun shots, foot steps, etc).  At some [small]
    number of instances, most users will not be able to tell if a new copy of the
    sound played or not.

    Args:
        limit The max instance count to set.
)",
              py::arg("limit"), py::call_guard<py::gil_scoped_release>());
    sound.def("get_peak_level", &PythonSoundData::getPeakLevel,
              R"(    Retrieves or calculates the peak volume levels for a sound if possible.

    This retrieves the peak volume level information for a sound.  This information
    is either loaded from the sound's original source file or is calculated if
    the sound is decoded into memory at load time.  This information will not be
    calculated if the sound is streamed from disk or memory.

    Returns:
        The peak level information from the SoundData object.

        This will throw if peak level information is not embedded in the sound.
)",
              py::call_guard<py::gil_scoped_release>());
    sound.def("get_event_points", &PythonSoundData::getEventPoints,
              R"(    Retrieves embedded event point information from a sound data object.

    This retrieves event point information that was embedded in the sound file that
    was used to create a sound data object.  The event points are optional in the
    data file and may not be present.  If they are parsed from the file, they will
    also be saved out to any destination file that the same sound data object is
    written to, provided the destination format supports embedded event point
    information.

    Returns:
        The list of event points that are embedded in this SoundData object.
)",
              py::call_guard<py::gil_scoped_release>());
    sound.def("get_event_point_by_id", &PythonSoundData::getEventPointById,
              R"(    Retrieves a single event point object by its identifier.

    Args:
        id The ID of the event point to retrieve.

    Returns:
        The event point is retrieved if it exists.

        None is returned if there was no event point found.
)",
              py::arg("id"), py::call_guard<py::gil_scoped_release>());
    sound.def("get_event_point_by_index", &PythonSoundData::getEventPointByIndex,
              R"(    Retrieves a single event point object by its index.

    Event point indices are contiguous, so this can be used to enumerate event
    points alternatively.

    Args:
        index The index of the event point to retrieve.

    Returns:
        The event point is retrieved if it exists.

        None is returned if there was no event point found.
)",
              py::arg("index"), py::call_guard<py::gil_scoped_release>());
    sound.def("get_event_point_by_play_index", &PythonSoundData::getEventPointByPlayIndex,
              R"(    Retrieves a single event point object by its playlist index.

    Event point playlist indices are contiguous, so this can be used to
    enumerate the playlist.

    Args:
        index The playlist index of the event point to retrieve.

    Returns:
        The event point is retrieved if it exists.

        None is returned if there was no event point found.
)",
              py::arg("index"), py::call_guard<py::gil_scoped_release>());
    sound.def("get_event_point_max_play_index", &PythonSoundData::getEventPointMaxPlayIndex,
              R"(    Retrieve the maximum play index value for the sound.

    Returns:
        This returns the max play index for this SoundData object.
        This will be 0 if no event points have a play index.
        This is also the number of event points with playlist indexes,
        since the playlist index range is contiguous.
)",
              py::call_guard<py::gil_scoped_release>());
    sound.def("set_event_points", &PythonSoundData::setEventPoints,
              R"(    Modifies, adds or removes event points in a SoundData object.

    This modifies, adds or removed one or more event points in a sound data
    object.  An event point will be modified if one with the same ID already
    exists.  A new event point will be added if it has an ID that is not
    already present in the sound data object and its frame offset is valid.  An
    event point will be removed if it has an ID that is present in the sound
    data object but the frame offset for it is set to
    EVENT_POINT_INVALID_FRAME.  Any other event points with invalid frame
    offsets (ie: out of the bounds of the stream) will be skipped and cause the
    function to fail.

    If an event point is modified or removed such that the playlist
    indexes of the event points are no longer contiguous, this function
    will adjust the play indexes of all event points to prevent any
    gaps.

    Args:
        eventPoints: The event point(s) to be modified or added.  The
                     operation that is performed for each event point in the
                     table depends on whether an event point with the same ID
                     already exists in the sound data object.  The event points
                     in this table do not need to be sorted in any order.

    Returns:
        True if all of the event points in the table are updated successfully.
        False if not all event points could be updated.  This includes a
        failure to allocate memory or an event point with an invalid frame
        offset.  Note that this failing doesn't mean that all the event points
        failed.  This just means that at least failed to be set properly.  The
        new set of event points may be retrieved and compared to the list set
        here to determine which one failed to be updated.
)",
              py::arg("eventPoints"), py::call_guard<py::gil_scoped_release>());
    sound.def("clear_event_points", &PythonSoundData::clearEventPoints,
              R"(    Removes all event points from a SoundData object.

    Returns:
        No return value.
)",
              py::call_guard<py::gil_scoped_release>());
    sound.def("get_metadata_by_index", &PythonSoundData::getMetaDataByIndex,
              R"(    Retrieve a metadata tag from a SoundData object by its index.

    Args:
        index The index of the metadata tag.

    Returns:
        This returns a tuple: (metadata tag name, metadata tag value).

        This returns (None, None) if there was no tag at the specified index.
)",
              py::arg("index"), py::call_guard<py::gil_scoped_release>());
    sound.def("get_metadata", &PythonSoundData::getMetaData,
              R"(    Retrieve a metadata value from a SoundData object by its tag name.

    Args:
        tag_name The metadata tag's name.

    Returns:
        This returns the metadata tag value for tag_name.

        This returns None if there is no tag under tag_name.
)",
              py::arg("tag_name"), py::call_guard<py::gil_scoped_release>());
    sound.def("set_metadata", &PythonSoundData::setMetaData,
              R"(    Add a metadata tag to a SoundData object.

    Metadata tag names are not case sensitive.

    It is not guaranteed that a given file type will be able to store arbitrary
    key-value pairs. RIFF files (.wav), for example, store metadata tags under
    4 character codes, so only metadata tags that are known to this plugin,
    such as META_DATA_TAG_ARTIST or tags that are 4 characters in length can be
    stored. Note this means that storing 4 character tags beginning with 'I'
    runs the risk of colliding with the known tag names (e.g. 'IART' will
    collide with META_DATA_TAG_ARTIST when writing a RIFF file).

    tag_name must not contain the character '=' when the output format encodes
    its metadata in the Vorbis Comment format (SampleFormat.VORBIS and
    SampleFormat.FLAC do this).  '=' will be replaced with '_' when
    encoding these formats to avoid the metadata being encoded incorrectly.
    Additionally, the Vorbis Comment standard states that tag names must only
    contain characters from 0x20 to 0x7D (excluding '=') when encoding these
    formats.

    Args:
        tag_name  The metadata tag's name.
        tag_value The metadata tag's value.

    Returns:
        No return value.
)",
              py::arg("tag_name"), py::arg("tag_value"), py::call_guard<py::gil_scoped_release>());
    sound.def("save_to_file", &PythonSoundData::saveToFile,
              R"(    Save a SoundData object to disk as a playable audio file.

    Args:
        file_name The path to save this file as.
        format    The audio format to use when saving this file.
                  PCM formats will save as a WAVE file (.wav).
        flags     Flags to alter the behavior of this function.
                  This is a bitmask of SAVE_FLAG_* flags.

    Returns:
        True if the SoundData object was saved to disk successfully.
        False if saving to disk failed.
)",
              py::arg("file_name"), py::arg("format") = SampleFormat::eDefault, py::arg("flags") = 0);
}

} // namespace audio
} // namespace carb
