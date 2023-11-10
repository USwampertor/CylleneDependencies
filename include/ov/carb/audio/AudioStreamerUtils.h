// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Helper classes for streaming data from @ref carb::audio::IAudioPlayback.
 */
#pragma once

#include "AudioUtils.h"
#include "IAudioData.h"
#include "IAudioPlayback.h"
#include "IAudioUtils.h"
#include "../Framework.h"
#include "../cpp20/Atomic.h"
#include "../events/IEvents.h"
#include "../../omni/extras/DataStreamer.h"

#include <atomic>
#include <string.h>

#if CARB_PLATFORM_WINDOWS
#    define strdup _strdup
#endif


namespace carb
{
namespace audio
{

/** wrapper base class to handle defining new streamer objects in C++ classes.  This base class
 *  handles all reference counting for the Streamer interface.  Objects that inherit from this
 *  class should never be explicitly deleted.  They should always be destroyed through the
 *  reference counting system.  Each object that inherits from this will be created with a
 *  reference count of 1 meaning that the creator owns a single reference.  When that reference
 *  is no longer needed, it should be released with release().  When all references are
 *  released, the object will be destroyed.
 *
 *  Direct instantiations of this wrapper class are not allowed because it contains pure
 *  virtual methods.  Classes that inherit from this must override these methods in order to
 *  be used.
 *
 *  See these pages for more information:
 *  @rst
    * :ref:`carbonite-audio-label`
    * :ref:`carbonite-streamer-label`
    @endrst
 */
class StreamerWrapper : public Streamer
{
public:
    StreamerWrapper()
    {
        m_refCount = 1;
        acquireReference = streamerAcquire;
        releaseReference = streamerRelease;
        openStream = streamerOpen;
        writeStreamData = streamerWriteData;
        closeStream = streamerClose;
    }

    /** acquires a single reference to this streamer object.
     *
     *  @returns no return value.
     *
     *  @remarks This acquires a new reference to this streamer object.  The reference must be
     *           released later with release() when it is no longer needed.  Each call to
     *           acquire() on an object must be balanced with a call to release().  When a new
     *           streamer object is created, it should be given a reference count of 1.
     */
    void acquire()
    {
        m_refCount.fetch_add(1, std::memory_order_relaxed);
    }

    /** releases a single reference to this streamer object.
     *
     *  @returns no return value.
     *
     *  @remarks This releases a single reference to this streamer object.  If the reference count
     *           reaches zero, the object will be destroyed.  The caller should assume the object
     *           to have been destroyed unless it is well known that other local references still
     *           exist.
     */
    void release()
    {
        if (m_refCount.fetch_sub(1, std::memory_order_release) == 1)
        {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete this;
        }
    }

    /** Wait until the close() call has been given.
     *  @param[in] duration The duration to wait before timing out.
     *  @returns `true` if the close call has been given
     *  @returns `false` if the timeout was reached.
     *
     *  @remarks If you disconnect a streamer via IAudioPlayback::setOutput(),
     *           the engine may not be stopped, so the streamer won't be
     *           immediately disconnected.  In cases like this, you should call
     *           waitForClose() if you need to access the streamer's written data
     *           but don't have access to the close() call (e.g. if you're using
     *           an @ref OutputStreamer).
     */
    template <class Rep, class Period>
    bool waitForClose(const std::chrono::duration<Rep, Period>& duration) noexcept
    {
        return m_open.wait_for(true, duration);
    }

    /** sets the suggested format for this stream output.
     *
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
     *
     *  @note This should not be called directly.  This will be called by the audio processing
     *        engine when this streamer object is first assigned as an output on an audio context.
     */
    virtual bool open(SoundFormat* format) = 0;

    /** writes a buffer of data to the stream.
     *
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
     *  @retval StreamState::eCritical if the data was written successfully to the streamer
     *           and more data needs to be provided as soon as possible.
     *  @retval StreamState::eMuchLess if the data was written successfully to the streamer
     *           and the data rate needs to be halved.
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
     *
     *  @note This should not be called directly.  This will be called by the audio processing
     *        engine when a buffer of new data is produced.
     */
    virtual StreamState writeData(const void* data, size_t bytes) = 0;

    /** closes the stream.
     *
     *  @returns no return value.
     *
     *  @remarks This signals that a stream has been finished.  This occurs when the engine is
     *           stopped or the audio context is destroyed.  No more calls to writeData() should
     *           be expected until the streamer is opened again.
     *
     *  @note This should not be called directly.  This will be called by the audio processing
     *        engine when audio processing engine is stopped or the context is destroyed.
     */
    virtual void close() = 0;

protected:
    virtual ~StreamerWrapper()
    {
        auto refCount = m_refCount.load(std::memory_order_relaxed);
        CARB_UNUSED(refCount);
        CARB_ASSERT(refCount == 0,
                    "deleting the streamer with refcount %zd - was it destroyed by a method other than calling release()?",
                    refCount);
    }

private:
    static void CARB_ABI streamerAcquire(Streamer* self)
    {
        StreamerWrapper* ctxt = static_cast<StreamerWrapper*>(self);
        ctxt->acquire();
    }

    static void CARB_ABI streamerRelease(Streamer* self)
    {
        StreamerWrapper* ctxt = static_cast<StreamerWrapper*>(self);
        ctxt->release();
    }

    static bool CARB_ABI streamerOpen(Streamer* self, SoundFormat* format)
    {
        StreamerWrapper* ctxt = static_cast<StreamerWrapper*>(self);
        ctxt->m_open = true;
        return ctxt->open(format);
    }

    static StreamState CARB_ABI streamerWriteData(Streamer* self, const void* data, size_t bytes)
    {
        StreamerWrapper* ctxt = static_cast<StreamerWrapper*>(self);
        return ctxt->writeData(data, bytes);
    }

    static void CARB_ABI streamerClose(Streamer* self)
    {
        StreamerWrapper* ctxt = static_cast<StreamerWrapper*>(self);
        ctxt->close();
        ctxt->m_open = false;
        ctxt->m_open.notify_all();
    }

    /** the current reference count to this object.  This will be 1 on object creation.  This
     *  object will be destroyed when the reference count reaches zero.
     */
    std::atomic<size_t> m_refCount;

    /** Flag that marks if the streamer is still open. */
    carb::cpp20::atomic<bool> m_open{ false };
};

/** a streamer object to write to a stream to a file.  The stream will be output in realtime by
 *  default (ie: writing to file at the same rate as the sound would play back on an audio
 *  device).  This can be sped up by not specifying the @ref fFlagRealtime flag.  When this flag
 *  is not set, the stream data will be produced as fast as possible.
 *
 *  An output filename must be set with setFilename() before the streamer can be opened.  All
 *  other parameters will work properly as their defaults.
 */
class OutputStreamer : public StreamerWrapper
{
public:
    /** Type definition for the behavioral flags for this streamer. */
    typedef uint32_t Flags;

    /** flag to indicate that the audio data should be produced for the streamer at the same
     *  rate as it would be produced for a real audio device.  If this flag is not set, the
     *  data will be produced as quickly as possible.
     */
    static constexpr Flags fFlagRealtime = 0x00000001;

    /** flag to indicate that the output stream should be flushed to disk after each buffer is
     *  written to it.  If this flag is not present, flushing to disk will not be guaranteed
     *  until the stream is closed.
     */
    static constexpr Flags fFlagFlush = 0x00000002;

    /** Constructor.
     *  @param[in] outputFormat The encoded format for the output file.
     *  @param[in] flags        Behavioral flags for this instance.
     */
    OutputStreamer(SampleFormat outputFormat = SampleFormat::eDefault, Flags flags = fFlagRealtime)
    {
        m_desc.flags = 0;
        m_desc.filename = nullptr;
        m_desc.inputFormat = SampleFormat::eDefault;
        m_desc.outputFormat = outputFormat;
        m_desc.frameRate = 0;
        m_desc.channels = 0;
        m_desc.encoderSettings = nullptr;
        m_desc.ext = nullptr;
        m_filename = nullptr;
        m_encoderSettings = nullptr;
        m_stream = nullptr;
        m_utils = nullptr;
        m_flags = flags;
    }

    /** retrieves the descriptor that will be used to open the output stream.
     *
     *  @returns the descriptor object.  This will never be nullptr.  This can be used to
     *           manually fill in the descriptor if need be, or to just verify the settings
     *           that will be used to open the output stream.
     */
    OutputStreamDesc* getDescriptor()
    {
        return &m_desc;
    }

    /** sets the flags that will control how data is written to the stream.
     *
     *  @param[in] flags    the flags that will control how data is written to the stream.  This
     *                      is zero or more of the kFlag* flags.
     *  @returns no return value.
     */
    void setFlags(Flags flags)
    {
        m_flags = flags;
    }

    /** retrieves the flags that are control how data is written to the stream.
     *
     *  @returns the flags that will control how data is written to the stream.  This is zero
     *           or more of the kFlag* flags.
     */
    Flags getFlags() const
    {
        return m_flags;
    }

    /** sets the output format for the stream.
     *
     *  @param[in] format   the output format for the stream.  This can be SampleFormat::eDefault
     *                      to use the same format as the input.  If this is to be changed, this
     *                      must be done before open() is called.
     *  @returns no return value.
     */
    void setOutputFormat(SampleFormat format)
    {
        m_desc.outputFormat = format;
    }

    /** sets the filename for the output stream.
     *
     *  @param[in] filename     the filename to use for the output stream.  This must be set
     *                          before open() is called.  This may not be nullptr.
     *  @returns no return value.
     */
    void setFilename(const char* filename)
    {
        char* temp;


        temp = strdup(filename);

        if (temp == nullptr)
            return;

        if (m_filename != nullptr)
            free(m_filename);

        m_filename = temp;
        m_desc.filename = m_filename;
    }

    /** retrieves the filename assigned to this streamer.
     *
     *  @returns the filename assigned to this streamer.  This will be nullptr if no filename
     *           has been set yet.
     */
    const char* getFilename() const
    {
        return m_filename;
    }

    /** sets the additional encoder settings to use for the output stream.
     *
     *  @param[in] settings     the encoder settings block to use to open the output stream.
     *                          This may be nullptr to clear any previously set encoder settings
     *                          block.
     *  @param[in] sizeInBytes  the size of the encoder settings block in bytes.
     *  @returns no return value.
     *
     *  @remarks This sets the additional encoder settings block to use for the output stream.
     *           This block will be copied to be stored internally.  This will replace any
     *           previous encoder settings block.
     */
    void setEncoderSettings(const void* settings, size_t sizeInBytes)
    {
        void* temp;


        if (settings == nullptr)
        {
            if (m_encoderSettings != nullptr)
                free(m_encoderSettings);

            m_encoderSettings = nullptr;
            m_desc.encoderSettings = nullptr;
            return;
        }

        temp = malloc(sizeInBytes);

        if (temp == nullptr)
            return;

        if (m_encoderSettings != nullptr)
            free(m_encoderSettings);

        memcpy(temp, settings, sizeInBytes);
        m_encoderSettings = temp;
        m_desc.encoderSettings = m_encoderSettings;
    }

    bool open(SoundFormat* format) override
    {
        m_utils = getFramework()->acquireInterface<carb::audio::IAudioUtils>();
        CARB_ASSERT(m_utils != nullptr, "the IAudioData interface was not successfully acquired!");
        CARB_ASSERT(m_desc.filename != nullptr, "call setFilename() first!");

        // update the output stream descriptor with the given format information and flags.
        if ((m_flags & fFlagFlush) != 0)
            m_desc.flags |= fStreamFlagFlushAfterWrite;

        m_desc.channels = format->channels;
        m_desc.frameRate = format->frameRate;
        m_desc.inputFormat = format->format;

        m_stream = m_utils->openOutputStream(getDescriptor());
        return m_stream != nullptr;
    }

    StreamState writeData(const void* data, size_t bytes) override
    {
        CARB_ASSERT(m_utils != nullptr);
        CARB_ASSERT(m_stream != nullptr);
        m_utils->writeDataToStream(m_stream, data, bytesToFrames(bytes, m_desc.channels, m_desc.inputFormat));
        return (m_flags & fFlagRealtime) != 0 ? StreamState::eNormal : StreamState::eCritical;
    }

    void close() override
    {
        CARB_ASSERT(m_utils != nullptr);

        if (m_stream == nullptr)
            return;

        m_utils->closeOutputStream(m_stream);
        m_stream = nullptr;
    }

protected:
    ~OutputStreamer() override
    {
        if (m_stream != nullptr)
            m_utils->closeOutputStream(m_stream);

        if (m_filename != nullptr)
            free(m_filename);

        if (m_encoderSettings != nullptr)
            free(m_encoderSettings);
    }

private:
    /** the current filename for the output stream.  This must be a valid path before open()
     *  is called.
     */
    char* m_filename;

    /** the current encoder settings for the output stream.  This may be nullptr if no extra
     *  encoder settings are needed.
     */
    void* m_encoderSettings;

    /** the flags describing how the stream should be written.  This is a combination of zero
     *  or more of the kFlag* flags.
     */
    Flags m_flags;

    /** the descriptor to open the output stream with.  The information in this descriptor is
     *  collected by making various set*() calls on this object, or by retrieving and editing
     *  the descriptor directly with getDescriptor().
     */
    OutputStreamDesc m_desc;

    /** the output stream to be operated on.  This is created in the open() call when this
     *  streamer is first set as an output on an audio context.
     */
    OutputStream* m_stream;

    /** the data interface object. */
    IAudioUtils* m_utils;
};

/** a null streamer implementation.  This will accept all incoming audio data but will simply
 *  ignore it.  The audio processing engine will be told to continue producing data at the
 *  current rate after each buffer is written.  All data formats will be accepted.
 *
 *  This is useful for silencing an output while still allowing audio processing based events to
 *  occur as scheduled.
 */
class NullStreamer : public StreamerWrapper
{
public:
    NullStreamer()
    {
    }

    bool open(SoundFormat* format) override
    {
        CARB_UNUSED(format);
        return true;
    }

    StreamState writeData(const void* data, size_t bytes) override
    {
        CARB_UNUSED(data, bytes);
        return m_state;
    }

    void close() override
    {
    }

    /** sets the stream state that will be returned from writeData().
     *
     *  @param[in] state    the stream state to return from each writeData() call.  This will
     *                      affect the behavior of the audio processing engine and its rate
     *                      of running new cycles.  The default is @ref StreamState::eNormal.
     *  @returns no return value.
     */
    void setStreamState(StreamState state)
    {
        m_state = state;
    }

protected:
    ~NullStreamer() override
    {
    }

    /** the stream state to be returned from each writeData() call. */
    StreamState m_state = StreamState::eNormal;
};

/** An event that is sent when the audio stream opens.
 *  This will inform the listener of the stream's format and version.
 */
constexpr carb::events::EventType kAudioStreamEventOpen = 1;

/** An event that is sent when the audio stream closes. */
constexpr carb::events::EventType kAudioStreamEventClose = 2;

/** Version tag to mark @rstref{ABI breaks <abi-compatibility>}. */
constexpr int32_t kEventStreamVersion = 1;

/** A listener for data from an @ref EventStreamer.
 *  This allows an easy way to bind the necessary callbacks to receive audio
 *  data from the stream.
 */
class EventListener : omni::extras::DataListener
{
public:
    /** Constructor.
     *  @param[inout] p         The event stream that was returned from the
     *                          getEventStream() call from an @ref EventStreamer.
     *  @param[in]    open      The callback which is sent when the audio stream
     *                          is first opened.
     *                          This is used to provide information about the
     *                          data in the audio stream.
     *  @param[in]    writeData The callback which is sent when a buffer of data
     *                          is sent from the stream.
     *                          These callbacks are only sent after an @p open()
     *                          callback has been sent.
     *                          Note that the data sent here may not be properly
     *                          aligned for its data type due to the nature of
     *                          @ref events::IEvents, so you should memcpy the
     *                          data somewhere that's aligned for safety.
     *  @param[in]    close     This is called when the audio stream is closed.
     *
     *  @remarks All that needs to be done to start receiving data is to create
     *           this class. Once the class is created, the callbacks will start
     *           being sent.
     *           Note that you must create the listener before the audio stream
     *           opens, otherwise the open event will never be received, so you
     *           will not receive data until the stream closes and re-opens.
     */
    EventListener(carb::events::IEventStreamPtr p,
                  std::function<void(const carb::audio::SoundFormat* fmt)> open,
                  std::function<void(const void* data, size_t bytes)> writeData,
                  std::function<void()> close)
        : omni::extras::DataListener(p)
    {
        OMNI_ASSERT(open, "this callback is not optional");
        OMNI_ASSERT(writeData, "this callback is not optional");
        OMNI_ASSERT(close, "this callback is not optional");
        m_openCallback = open;
        m_writeDataCallback = writeData;
        m_closeCallback = close;
    }

protected:
    /** Function to pass received data to this audio streamer's destination.
     *
     *  @param[in] payload The packet of data that was received.  This is expected to contain
     *                     the next group of audio frames in the stream on each call.
     *  @param[in] bytes   The length of @p payload in bytes.
     *  @param[in] type    The data type ID of the data contained in @p payload.  This is
     *                     ignored since calls to this handler are always expected to be plain
     *                     data for the stream.
     *  @returns No return value.
     */
    void onDataReceived(const void* payload, size_t bytes, omni::extras::DataStreamType type) noexcept override
    {
        CARB_UNUSED(type);
        if (m_open)
        {
            m_writeDataCallback(payload, bytes);
        }
    }

    /** Handler function for non-data events on the stream.
     *
     *  @param[in] e    The event that was received.  This will either be an 'open' or 'close'
     *                  event depending on the value of this event's 'type' member.
     *  @returns No return value.
     */
    void onEventReceived(const carb::events::IEvent* e) noexcept override
    {
        carb::audio::SoundFormat fmt = {};
        auto getIntVal = [this](const carb::dictionary::Item* root, const char* name) -> size_t {
            const carb::dictionary::Item* child = m_dict->getItem(root, name);
            return (child == nullptr) ? 0 : m_dict->getAsInt64(child);
        };
        switch (e->type)
        {
            case kAudioStreamEventOpen:
            {
                int32_t ver = int32_t(getIntVal(e->payload, "version"));
                if (ver != kEventStreamVersion)
                {
                    CARB_LOG_ERROR("EventListener version %" PRId32 " tried to attach to data stream version  %" PRId32,
                                   kEventStreamVersion, ver);
                    disconnect();
                    return;
                }

                fmt.channels = getIntVal(e->payload, "channels");
                fmt.bitsPerSample = getIntVal(e->payload, "bitsPerSample");
                fmt.frameSize = getIntVal(e->payload, "frameSize");
                fmt.blockSize = getIntVal(e->payload, "blockSize");
                fmt.framesPerBlock = getIntVal(e->payload, "framesPerBlock");
                fmt.frameRate = getIntVal(e->payload, "frameRate");
                fmt.channelMask = getIntVal(e->payload, "channelMask");
                fmt.validBitsPerSample = getIntVal(e->payload, "validBitsPerSample");
                fmt.format = SampleFormat(getIntVal(e->payload, "format"));
                m_openCallback(&fmt);
                m_open = true;
                break;
            }

            case kAudioStreamEventClose:
                if (m_open)
                {
                    m_closeCallback();
                    m_open = false;
                }
                break;

            default:
                OMNI_LOG_ERROR("unknown event received %zd", size_t(e->type));
        }
    }

private:
    std::function<void(const carb::audio::SoundFormat* fmt)> m_openCallback;
    std::function<void(const void* data, size_t bytes)> m_writeDataCallback;
    std::function<void()> m_closeCallback;
    bool m_open = false;
};

/** A @ref events::IEvents based audio streamer.
 *  This will send a stream of audio data through @ref events::IEvents then
 *  pumps the event stream asynchronously.
 *  This is ideal for use cases where audio streaming is needed, but the
 *  component receiving audio is unable to meet the latency requirements of
 *  other audio streamers.
 *
 *  To receive data from this, you will need to create an @ref EventListener
 *  with the event stream returned from the getEventStream() call on this class.
 */
class EventStreamer : public StreamerWrapper
{
public:
    EventStreamer()
    {
    }

    /** Check if the class actually initialized successfully.
     *  @returns whether the class actually initialized successfully.
     */
    bool isWorking() noexcept
    {
        return m_streamer.isWorking();
    }

    /** Specify a desired format for the audio stream.
     *  @param[in] format The format that you want to be used.
     *                    This can be `nullptr` to just use the default format.
     */
    void setFormat(const SoundFormat* format) noexcept
    {
        if (format != nullptr)
        {
            m_desiredFormat = *format;
        }
        else
        {
            m_desiredFormat = {};
        }
    }

    /** Create an @ref EventListener for this streamer.
     *
     *  @param[in]    open      The callback which is sent when the audio stream
     *                          is first opened.
     *                          This is used to provide information about the
     *                          data in the audio stream.
     *  @param[in]    writeData The callback which is sent when a buffer of data
     *                          is sent from the stream.
     *                          These callbacks are only sent after an @p open()
     *                          callback has been sent.
     *                          Note that the data sent here may not be properly
     *                          aligned for its data type due to the nature of
     *                          @ref events::IEvents, so you should memcpy the
     *                          data somewhere that's aligned for safety.
     *  @param[in]    close     This is called when the audio stream is closed.
     *
     *  @returns An event listener.
     *  @returns `nullptr` if an out of memory error occurs.
     *
     *  @remarks These callbacks will be fired until the @ref EventListener
     *           is deleted.
     *           Note that you must create the listener before the audio stream
     *           opens, otherwise the open event will never be received, so you
     *           will not receive data until the stream closes and re-opens.
     */
    EventListener* createListener(std::function<void(const carb::audio::SoundFormat* fmt)> open,
                                  std::function<void(const void* data, size_t bytes)> writeData,
                                  std::function<void()> close)
    {
        return new (std::nothrow) EventListener(m_streamer.getEventStream(), open, writeData, close);
    }

    /** Retrieve the event stream used by the data streamer.
     *  @returns The event stream used by the data streamer.
     *
     *  @remarks This event stream is exposed to be subscribed to.
     *           Sending other events into this stream will cause errors.
     */
    carb::events::IEventStreamPtr getEventStream() noexcept
    {
        return m_streamer.getEventStream();
    }

    /** Wait for all asynchronous tasks created by this stream to finish. */
    void flush() noexcept
    {
        m_streamer.flush();
    }

private:
    bool open(carb::audio::SoundFormat* format) noexcept override
    {
        if (!m_streamer.isWorking())
        {
            return false;
        }

        if (m_desiredFormat.channels != 0)
        {
            format->channels = m_desiredFormat.channels;
            format->channelMask = kSpeakerModeDefault;
        }
        if (m_desiredFormat.frameRate != 0)
        {
            format->frameRate = m_desiredFormat.frameRate;
        }
        if (m_desiredFormat.channelMask != kSpeakerModeDefault)
        {
            format->channelMask = m_desiredFormat.channelMask;
        }
        if (m_desiredFormat.format != SampleFormat::eDefault)
        {
            format->format = m_desiredFormat.format;
        }

        m_streamer.getEventStream()->push(kAudioStreamEventOpen, std::make_pair("version", kEventStreamVersion),
                                          std::make_pair("channels", int64_t(format->channels)),
                                          std::make_pair("bitsPerSample", int64_t(format->bitsPerSample)),
                                          std::make_pair("frameSize", int64_t(format->frameSize)),
                                          std::make_pair("blockSize", int64_t(format->blockSize)),
                                          std::make_pair("framesPerBlock", int64_t(format->framesPerBlock)),
                                          std::make_pair("frameRate", int64_t(format->frameRate)),
                                          std::make_pair("channelMask", int64_t(format->channelMask)),
                                          std::make_pair("validBitsPerSample", int64_t(format->validBitsPerSample)),
                                          std::make_pair("format", int32_t(format->format)));
        m_streamer.pumpAsync();
        return true;
    }

    void close() noexcept override
    {
        if (!m_streamer.isWorking())
        {
            return;
        }
        m_streamer.getEventStream()->push(kAudioStreamEventClose);
        m_streamer.pumpAsync();
    }

    StreamState writeData(const void* data, size_t bytes) noexcept override
    {
        if (!m_streamer.isWorking())
        {
            return StreamState::eNormal;
        }
        // just push as bytes here, we'll clean up the type later
        m_streamer.pushData(static_cast<const uint8_t*>(data), bytes);
        m_streamer.pumpAsync();
        return StreamState::eNormal;
    }


    SoundFormat m_desiredFormat = {};

    omni::extras::DataStreamer m_streamer;
};

} // namespace audio
} // namespace carb
