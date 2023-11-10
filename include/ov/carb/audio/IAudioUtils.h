// Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief General audio utilities.
 */
#pragma once

#include "../Interface.h"
#include "AudioTypes.h"
#include "IAudioData.h"

namespace carb
{
namespace audio
{

/************************************* Interface Objects *****************************************/
/** a handle to an open output stream.  This is created by openOutputStream().  This holds the
 *  current state of the output stream and allows it to be written out in multiple chunks.
 */
struct OutputStream;

/********************************* Sound Data Conversion Objects *********************************/
/** flags to control the behavior of a conversion operation.
 *
 *  @{
 */
/** container type for conversion operation flags. */
typedef uint32_t ConvertFlags;

/** convert the sound data object in-place.  The old buffer data will be replaced with the
 *  converted data and all of the object's format information will be updated accordingly.
 *  This is the default behavior if no flags are given.  Note that if the source and
 *  destination formats are the same and this flag is used, a new reference will be taken
 *  on the original sound data object.  The returned object will be the same as the input
 *  object, but both will need to be released (just the same as if a new object had been
 *  returned).
 */
constexpr ConvertFlags fConvertFlagInPlace = 0x00000001;

/** convert the sound data object and return a new copy of the data.  The previous sound
 *  data object will be unmodified and still valid.  The new object will contain the same
 *  audio data, just converted to the new format.  The new object needs to be destroyed
 *  with destroySoundData() when it is no longer needed.
 */
constexpr ConvertFlags fConvertFlagCopy = 0x00000002;

/** when duplicating a sound data object and no conversion is necessary, this allows the
 *  new object to reference the same data pointer as the original object.  It is the
 *  caller's responsibility to ensure that the original object remains valid for the life
 *  time of the copied object.  This flag will be ignored if a conversion needs to occur.
 *  This flag is useful when the original sound data object already references user memory
 *  instead of copying the data.  If this flag is not used, the data buffer will always
 *  be copied from the original buffer.
 */
constexpr ConvertFlags fConvertFlagReferenceData = 0x00000004;


/** forces an operation to copy or decode the input data
 *  into a new sound data object.
 *  If the @ref fConvertFlagInPlace is specified and the sound data object is in memory,
 *  then the object is decoded in place. If the sound is in a file, then this creates a
 *  new sound data object containing the decoded sound.
 *  If the @ref fConvertFlagCopy is specified, then a new sound data object
 *  will be created to contain the converted sound.
 *  If neither the @ref fConvertFlagCopy nor the @ref fConvertFlagInPlace are specified,
 *  then the @ref fConvertFlagCopy flag will be implied.
 *
 *  @note Using this flag on a compressed format will cause a re-encode and that
 *        could cause quality degradation.
 */
constexpr ConvertFlags fConvertFlagForceCopy = 0x00000008;
/** @} */

/** a descriptor of a data type conversion operation.  This provides the information needed to
 *  convert a sound data object from its current format to another data format.  Not all data
 *  formats may be supported as destination formats.  The conversion operation will fail if the
 *  destination format is not supported for encoding.  The conversion operation may either be
 *  performed in-place on the sound data object itself or it may output a copy of the sound
 *  data object converted to the new format.
 *
 *  Note that this conversion operation will not change the length (mostly), frame rate, or
 *  channel count of the data, just its sample format.  The length of the stream may increase
 *  by a few frames for some block oriented compression or encoding formats so that the stream
 *  can be block aligned in length.  PCM data will always remain the same length as the input
 *  since the frames per block count for PCM data is always 1.
 */
struct ConversionDesc
{
    /** flags to control how the conversion proceeds.  This may be zero or more of the
     *  fConvertFlag* flags.
     */
    ConvertFlags flags = 0;

    /** the sound data to be converted.  This object may or may not be modified depending on
     *  which flags are used.  The converted data will be equivalent to the original data,
     *  just in the new requested format.  Note that some destination formats may cause some
     *  information to be lost due to their compression or encoding methods.  The converted
     *  data will contain at least the same number of frames and channels as the original data.
     *  Some block oriented compression formats may pad the stream with silent frames so that
     *  a full block can be written out.  This may not be nullptr.
     */
    SoundData* soundData;

    /** the requested destination format for the conversion operation.  For some formats,
     *  this may result in data or quality loss.  If this format is not supported for
     *  encoding, the operation will fail.  This can be @ref SampleFormat::eDefault to
     *  use the same as the original format.  This is useful when also using the
     *  @ref fConvertFlagCopy to duplicate a sound data object.
     *
     *  Note that if this new format matches the existing format this will be a no-op
     *  unless the @ref fConvertFlagCopy flag is specified.  If the 'copy' flag is used,
     *  this will simply duplicate the existing object.  The new object will still need
     *  to be destroyed with release() when it is no longer needed.
     */
    SampleFormat newFormat = SampleFormat::eDefault;

    /** additional output format dependent encoder settings.  This should be nullptr for PCM
     *  data formats.  Additional objects will be defined for encoder formats that require
     *  additional parameters (optional or otherwise).  For formats that require additional
     *  settings, this may not be nullptr.  Use getCodecFormatInfo() to retrieve the info
     *  for the codec to find out if the additional settings are required or not.
     */
    void* encoderSettings = nullptr;

    /** an opaque context value that will be passed to the readCallback and setPosCallback
     *  functions each time they are called.  This value is a caller-specified object that
     *  is expected to contain the necessary decoding state for a user decoded stream.  This
     *  value is only necessary if the @ref fDataFlagUserDecode flag was used when creating
     *  the sound data object being converted.
     */
    void* readCallbackContext = nullptr;

    /** An optional callback that gets fired when the SoundData's final
     *  reference is released. This is intended to make it easier to perform
     *  cleanup of a SoundData in cases where @ref fDataFlagUserMemory is used.
     *  This is intended to be used in cases where the SoundData is using some
     *  resource that needs to be released after the SoundData is destroyed.
     */
    SoundDataDestructionCallback destructionCallback = nullptr;

    /** An opaque context value that will be passed to @ref destructionCallback
     *  when the last reference to the SoundData is released.
     *  This will not be called if the SoundData is not created successfully.
     */
    void* destructionCallbackContext = nullptr;

    /** reserved for future expansion.  This must be set to nullptr. */
    void* ext = nullptr;
};

/** Flags that alter the behavior of a PCM transcoding operation.
 */
typedef uint32_t TranscodeFlags;

/** A descriptor for transcoding between PCM formats, which is used for the
 *  transcodePcm() function
 */
struct TranscodeDesc
{
    /** Flags for the transcoding operation.
     *  This must be 0 as no flags are currently defined.
     */
    TranscodeFlags flags = 0;

    /** The format of the input data.
     *  This must be a PCM format.
     */
    SampleFormat inFormat;

    /** The data format that will be written into @p outBuffer.
     *  This must be a PCM format.
     */
    SampleFormat outFormat;

    /** The input buffer to be transcoded.
     *  Audio in this buffer is interpreted as @ref inFormat.
     *  This must be long enough to hold @ref samples samples of audio data in
     *  @ref inFormat.
     */
    const void* inBuffer;

    /** The output buffer to receive the transcoded data.
     *  Audio will be transcoded from @ref inBuffer into @ref outBuffer in
     *  @ref outFormat.
     *  This must be long enough to hold @ref samples samples of audio data in
     *  @ref outFormat.
     *  This may not alias or overlap @ref inBuffer.
     */
    void* outBuffer;

    /** The number of samples of audio to transcode.
     *  Note that for multichannel audio, this is the number of frames
     *  multiplied by the channel count.
     */
    size_t samples;

    /** reserved for future expansion.  This must be set to nullptr. */
    void* ext = nullptr;
};

/*********************************** Sound Data Output Objects ***********************************/
/** Flags used for the saveToFile() function.  These control how the the sound data object
 *  is written to the file. Zero or more of these flags may be combined to alter the behavior.
 *  @{
 */
typedef uint32_t SaveFlags;

/** Default save behavior. */
constexpr SaveFlags fSaveFlagDefault = 0x00000000;

/** Don't write the metadata information into the file.
 */
constexpr SaveFlags fSaveFlagStripMetaData = 0x00000001;

/** Don't write the event point information into the file.
 */
constexpr SaveFlags fSaveFlagStripEventPoints = 0x00000002;

/** Don't write the peaks information into the file.
 */
constexpr SaveFlags fSaveFlagStripPeaks = 0x00000004;


/** a descriptor of how a sound data object should be written out to file.  This can optionally
 *  convert the audio data to a different format.  Note that transcoding the audio data could
 *  result in a loss in quality depending on both the source and destination formats.
 */
struct SoundDataSaveDesc
{
    /** Flags that alter the behavior of saving the file.
     *  These may indicate to the file writer that certain elements in the file
     *  should be stripped, for example.
     */
    SaveFlags flags = 0;

    /** the format that the sound data object should be saved in.  Note that if the data was
     *  fully decoded on load, this may still result in some quality loss if the data needs to
     *  be re-encoded.  This may be @ref SampleFormat::eDefault to write the sound to file in
     *  the sound's encoded format.
     */
    SampleFormat format = SampleFormat::eDefault;

    /** the sound data to be written out to file.  This may not be nullptr.  Depending on
     *  the data's original format and flags and the requested destination format, there
     *  may be some quality loss if the data needs to be decoded or re-encoded.
     *  This may not be a streaming sound.
     */
    const SoundData* soundData;

    /** the destination filename for the sound data.  This may be a relative or absolute
     *  path.  For relative paths, these will be resolved according to the rules of the
     *  IFileSystem interface.  This may not be nullptr.
     */
    const char* filename;

    /** additional output format dependent encoder settings.  This should be nullptr for PCM
     *  data formats.  Additional objects will be defined for encoder formats that require
     *  additional parameters (optional or otherwise).  For formats that require additional
     *  settings, this may not be nullptr.  Use getCodecFormatInfo() to retrieve the info
     *  for the codec to find out if the additional settings are required or not.
     */
    void* encoderSettings = nullptr;

    /** reserved for future expansion.  This must be set to nullptr. */
    void* ext = nullptr;
};


/** base type for all output stream flags. */
typedef uint32_t OutputStreamFlags;

/** flag to indicate that an output stream should flush its file after each buffer is successfully
 *  written to it.  By default, the stream will not be forced to be flushed until it is closed.
 */
constexpr OutputStreamFlags fStreamFlagFlushAfterWrite = 0x00000001;

/** flag to indicate that the stream should disable itself if an error is encountered writing a
 *  buffer of audio to the output.  And example of a failure could be that the output file fails
 *  to be opened (ie: permissions issue, path doesn't exist, etc), or there was an encoding error
 *  with the chosen output format (extremely rare but possible).  If such a failure occurs, the
 *  output stream will simply ignore new incoming data until the stream is closed.  If this flag
 *  is not used, the default behavior is to continue trying to write to the stream.  In this
 *  case, it is possible that the stream could recover and continue writing output again (ie:
 *  the folder containing the file suddenly was created), however doing so could lead to an
 *  audible artifact being introduced to the output stream.
 */
constexpr OutputStreamFlags fStreamFlagDisableOnFailure = 0x00000002;


/** a descriptor for opening an output file stream.  This allows sound data to be written to a
 *  file in multiple chunks.  The output stream will remain open and able to accept more input
 *  until it is closed.  The output data can be encoded as it is written to the file for certain
 *  formats.  Attempting to open the stream with a format that doesn't support encoding will
 *  cause the stream to fail.
 */
struct OutputStreamDesc
{
    /** flags to control the behavior of the output stream.  This may be 0 to specify default
     *  behavior.
     */
    OutputStreamFlags flags = 0;

    /** the filename to write the stream to.  This may be a relative or absolute path.  If a
     *  relative path is used, it will be resolved according to the rules of the IFileSystem
     *  interface.  If the filename does not include a file extension, one will be added
     *  according to the requested output format.  If no file extension is desired, the
     *  filename should end with a period ('.').  This may not be nullptr.
     */
    const char* filename;

    /** the input sample format for the stream.  This will be the format of the data that is
     *  passed in the buffers to writeDataToStream().
     *  This must be a PCM format (one of SampleFormat::ePcm*).
     */
    SampleFormat inputFormat;

    /** the output sample format for the stream.  This will be the format of the data that is
     *  written to the output file.  If this matches the input data format, the buffer will
     *  simply be written to the file stream.  This may be @ref SampleFormat::eDefault to use
     *  the same format as @ref inputFormat for the output.
     */
    SampleFormat outputFormat = SampleFormat::eDefault;

    /** the data rate of the stream in frames per second.  This value is recorded to the
     *  stream but does not affect the actual consumption of data from the buffers.
     */
    size_t frameRate;

    /** the number of channels in each frame of the stream. */
    size_t channels;

    /** additional output format dependent encoder settings.  This should be nullptr for PCM
     *  data formats.  Additional objects will be defined for encoder formats that require
     *  additional parameters (optional or otherwise).  For formats that require additional
     *  settings, this may not be nullptr.  Use getCodecFormatInfo() to retrieve the info
     *  for the codec to find out if the additional settings are required or not.
     */
    void* encoderSettings = nullptr;

    /** reserved for future expansion.  This must be set to nullptr. */
    void* ext = nullptr;
};

/********************************* Audio Visualization Objects ***********************************/
/** Flags for @ref AudioImageDesc. */
using AudioImageFlags = uint32_t;

/** Don't clear out the image buffer with the background color before drawing.
 *  This is useful when drawing waveforms onto the same image buffer over
 *  multiple calls.
 */
constexpr AudioImageFlags fAudioImageNoClear = 0x01;

/** Draw lines between the individual samples when rendering. */
constexpr AudioImageFlags fAudioImageUseLines = 0x02;

/** Randomize The colors used for each sample. */
constexpr AudioImageFlags fAudioImageNoiseColor = 0x04;

/** Draw all the audio channels in the image on top of each other, rather than
 *  drawing one individual channel.
 */
constexpr AudioImageFlags fAudioImageMultiChannel = 0x08;

/** Perform alpha blending when drawing the samples/lines, rather than
 *  overwriting the pixels.
 */
constexpr AudioImageFlags fAudioImageAlphaBlend = 0x10;

/** Draw each audio channel as a separate waveform, organized vertically. */
constexpr AudioImageFlags fAudioImageSplitChannels = 0x20;


/** A descriptor for IAudioData::drawWaveform(). */
struct AudioImageDesc
{
    /** Flags that alter the drawing style. */
    AudioImageFlags flags;

    /** The sound to render into the waveform. */
    const SoundData* sound;

    /** The length of @ref sound to render as an image.
     *  This may be 0 to render the entire sound.
     */
    size_t length;

    /** The offset into the sound to start visualizing.
     *  The region visualized will start at @ref offset and end at @ref offset
     *  + @ref length. If the region extends beyond the end of the sound, it
     *  will be internally clamped to the end of the sound.
     *  If this value is negative, then this is treated as an offset relative
     *  to the end of the file, rather than the start.
     *  This may be 0 to render the entire sound.
     */
    int64_t offset;

    /** The unit type of @ref length and @ref offset.
     *  Note that using @ref UnitType::eBytes with a variable bitrate format will
     *  not provide very accurate results.
     */
    UnitType lengthType;

    /** This specifies which audio channel from @ref sound will be rendered.
     *  This is ignored when @ref fAudioImageMultiChannel is set on @ref flags.
     */
    size_t channel;

    /** The buffer that holds the image data.
     *  The image format is RGBA8888.
     *  This must be @ref height * @ref pitch bytes long.
     *  This may not be nullptr.
     */
    void* image;

    /** The width of the image in pixels. */
    size_t width;

    /** The width of the image buffer in bytes.
     *  This can be set to 0 to use 4 * @ref width as the pitch.
     *  This may be used for applications such as writing a subimage or an
     *  image that needs some specific alignment.
     */
    size_t pitch;

    /** The height of the image in pixels. */
    size_t height;

    /** The background color to write to the image in normalized RGBA color.
     *  The alpha channel in this color is not used to blend this color with
     *  the existing data in @ref image; use @ref fAudioImageNoClear if you
     *  want to render on top of an existing image.
     *  This value is ignored when @ref fAudioImageNoClear is set on @ref flags.
     */
    Float4 background;

    /** The colors to use for the image in normalized RGBA colors.
     *  If @ref fAudioImageMultiChannel, each element in this array maps to each
     *  channel in the output audio data; otherwise, element 0 is used as the
     *  color for the single channel.
     */
    Float4 colors[kMaxChannels];
};

/** General audio utilities.
 *  This interface contains a bunch of miscellaneous audio functionality that
 *  many audio applications can make use of.
 */
struct IAudioUtils
{
    CARB_PLUGIN_INTERFACE("carb::audio::IAudioUtils", 1, 0)


    /*************************** Sound Data Object Modifications ********************************/
    /** clears a sound data object to silence.
     *
     *  @param[in] sound    the sound data object to clear.  This may not be nullptr.
     *  @returns true if the clearing operation was successful.
     *  @returns false if the clearing operation was not successful.

     *  @note this will remove the SDO from user memory.
     *  @note this will clear the entire buffer, not just the valid portion.
     *  @note this will be a lossy operation for some formats.
     */
    bool(CARB_ABI* clearToSilence)(SoundData* sound);


    /**************************** Sound Data Saving and Streaming ********************************/
    /** save a sound data object to a file.
     *
     *  @param[in] desc     a descriptor of how the sound data should be saved to file and which
     *                      data format it should be written in.  This may not be nullptr.
     *  @returns true if the sound data is successfully written out to file.
     *  @returns false if the sound data could not be written to file.  This may include being
     *           unable to open or create the file, or if the requested output format could
     *           not be supported by the encoder.
     *
     *  @remarks This attempts to save a sound data object to file.  The destination data format
     *           in the file does not necessarily have to match the original sound data object.
     *           However, if the destination format does not match, the encoder for that format
     *           must be supported otherwise the operation will fail.  Support for the requested
     *           encoder format may be queried with isCodecFormatSupported() to avoid exposing
     *           user facing functionality for formats that cannot be encoded.
     */
    bool(CARB_ABI* saveToFile)(const SoundDataSaveDesc* desc);

    /** opens a new output stream object.
     *
     *  @param[in] desc     a descriptor of how the stream should be opened.  This may not be
     *                      nullptr.
     *  @returns a new output stream handle if successfully created.  This object must be closed
     *           with closeOutputStream() when it is no longer needed.
     *  @returns nullptr if the output stream could not be created.  This may include being unable
     *           to open or create the file, or if the requested output format could not be
     *           supported by the encoder.
     *
     *  @remarks This opens a new output stream and prepares it to receive buffers of data from
     *           the stream.  The header will be written to the file, but it will initially
     *           represent an empty stream.  The destination data format in the file does not
     *           necessarily have to match the original sound data object.  However, if the
     *           destination format does not match, the encoder for that format must be supported
     *           otherwise the operation will fail.  Support for the requested encoder format may
     *           be queried with isCodecFormatSupported() to avoid exposing user facing
     *           functionality for formats that cannot be encoded.
     */
    OutputStream*(CARB_ABI* openOutputStream)(const OutputStreamDesc* desc);

    /** closes an output stream.
     *
     *  @param[in] stream   the stream to be closed.  This may not be nullptr.  This must have
     *                      been returned from a previous call to openOutputStream().  This
     *                      object will no longer be valid upon return.
     *  @returns no return value.
     *
     *  @remarks This closes an output stream object.  The header for the file will always be
     *           updated so that it reflects the actual written stream size.  Any additional
     *           updates for the chosen data format will be written to the file before closing
     *           as well.
     */
    void(CARB_ABI* closeOutputStream)(OutputStream* stream);

    /** writes a single buffer of data to an output stream.
     *
     *  @param[in] stream           the stream to write the buffer to.  This handle must have
     *                              been returned by a previous call to openOutputStream() and
     *                              must not have been closed yet.  This may not be nullptr.
     *  @param[in] data             the buffer of data to write to the file.  The data in this
     *                              buffer is expected to be in data format specified when the
     *                              output stream was opened.  This buffer must be block aligned
     *                              for the given input format.  This may not be nullptr.
     *  @param[in] lengthInFrames   the size of the buffer to write in frames.  All frames in
     *                              the buffer must be complete.  Partial frames will neither
     *                              be detected nor handled.
     *  @returns true if the buffer is successfully encoded and written to the stream.
     *  @returns false if the buffer could not be encoded or an error occurs writing it to the
     *           stream.
     *
     *  @remarks This writes a single buffer of data to an open output stream.  It is the caller's
     *           responsibility to ensure this new buffer is the logical continuation of any of
     *           the previous buffers that were written to the stream.  The buffer will always be
     *           encoded and written to the stream in its entirety.  If any extra frames of data
     *           do not fit into one of the output format's blocks, the remaining data will be
     *           cached in the encoder and added to by the next buffer.  If the stream ends and
     *           the encoder still has a partial block waiting, it will be padded with silence
     *           and written to the stream when it is closed.
     */
    bool(CARB_ABI* writeDataToStream)(OutputStream* stream, const void* data, size_t lengthInFrames);


    /***************************** Sound Data Format Conversion **********************************/
    /** converts a sound data object from one format to another.
     *
     *  @param[in] desc     the descriptor of how the conversion operation should be performed.
     *                      This may not be nullptr.
     *  @returns the converted sound data object.
     *  @returns nullptr if the conversion could not occur.
     *
     *  @remarks This converts a sound data object from one format to another or duplicates an
     *           object.  The conversion operation may be performed on the same sound data object
     *           or it may create a new object.  The returned sound data object always needs to
     *           be released with release() when it is no longer needed.  This is true
     *           whether the original object was copied or not.
     *
     *  @note The destruction callback is not copied to the returned SoundData
     *        even if an in-place conversion is requested.
     *
     *  @note If @ref fConvertFlagInPlace is passed and the internal buffer
     *        of the input SoundData is being replaced, the original
     *        destruction callback on the input SoundData will be called.
     */
    carb::audio::SoundData*(CARB_ABI* convert)(const ConversionDesc* desc);

    /** duplicates a sound data object.
     *
     *  @param[in] sound    the sound data object to duplicate.  This may not be nullptr.
     *  @returns the duplicated sound data object.  This must be destroyed when it is no longer
     *           needed with a call to release().
     *
     *  @remarks This duplicates a sound data object.  The new object will have the same format
     *           and data content as the original.  If the original referenced user memory, the
     *           new object will get a copy of its data, not the original pointer.  If the new
     *           object should reference the original data instead, convert() should be
     *           used instead.
     */
    carb::audio::SoundData*(CARB_ABI* duplicate)(const SoundData* sound);

    /** A helper function to transcode between PCM formats.
     *
     *  @param[in] desc The descriptor of how the conversion operation should be
     *                  performed.
     *                  This may not be nullptr.
     *  @returns true if the data is successfully transcoded.
     *  @returns false if an invalid parameter is passed in or the conversion was not possible.
     *  @returns false if the input buffer or the output buffer are misaligned for their
     *           specified sample format.
     *           createData() be used in cases where a misaligned buffer needs to be used
     *           (for example when reading raw PCM data from a memory-mapped file).
     *
     *  @remarks This function is a simpler alternative to decodeData() for
     *           cases where it is known that both the input and output formats
     *           are PCM formats.
     *
     *  @note There is no requirement for the alignment of @ref TranscodeDesc::inBuffer
     *        or @ref TranscodeDesc::outBuffer, but the operation is most
     *        efficient when both are 32 byte aligned
     *        (e.g. `(static_cast<uintptr_t>(inBuffer) & 0x1F) == 0`).
     *
     *  @note It is valid for @ref TranscodeDesc::inFormat to be the same as
     *        @ref TranscodeDesc::outFormat; this is equivalent to calling
     *        memcpy().
     */
    bool(CARB_ABI* transcodePcm)(const TranscodeDesc* desc);

    /***************************** Audio Visualization *******************************************/
    /** Render a SoundData's waveform as an image.
     *  @param[in] desc The descriptor for the audio waveform input and output.
     *
     *  @returns true if visualization was successful.
     *  @returns false if no sound was specified.
     *  @returns false if the input image dimensions corresponded to a 0-size image
     *                 or the buffer for the image was nullptr.
     *  @returns false if the region of the sound to visualize was 0 length.
     *  @returns false if the image pitch specified was non-zero and too small
     *                 to fit an RGBA888 image of the desired width.
     *  @returns false if the specified channel to visualize was invalid.
     *
     *  @remarks This function can be used to visualize the audio samples in a
     *           sound buffer as an uncompressed RGBA8888 image.
     */
    bool(CARB_ABI* drawWaveform)(const AudioImageDesc* desc);
};


} // namespace audio

} // namespace carb
