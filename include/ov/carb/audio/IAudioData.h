// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief The audio data management interface.
 */
#pragma once

#include "../Interface.h"
#include "../assets/IAssets.h"
#include "AudioTypes.h"


namespace carb
{
namespace audio
{

/************************************* Interface Objects *****************************************/
/** a buffer of sound data.  This includes all of the information about the data's format and the
 *  sound data itself.  This data may be in a decoded PCM stream or an encoded/compressed format.
 *  Note that much of the information in this object can be accessed through the IAudioData interface.
 *  This includes (but is not limited to) extra decoding information about the compression format.
 */
struct SoundData DOXYGEN_EMPTY_CLASS;

/** stores information on the current decoding or encoding state of a @ref SoundData object.
 *  This object is kept separate from the sound data to avoid the limitation that streaming from
 *  a SoundData object or encoding a single sound to multiple targets can only have one
 *  simultaneous instance.  The information stored in this object determines how the sound
 *  data is decoded (ie: streamed from disk, streamed from memory, etc) and holds state
 *  information about the decoding process itself.
 */
struct CodecState DOXYGEN_EMPTY_CLASS;

/********************************* Sound Data Object Creation ************************************/
/** special value to indicate that the maximum instance count for a sound or sound group is
 *  unlimited.  This can be passed to setMaxInstances() or can be returned from getMaxInstances().
 */
constexpr uint32_t kInstancesUnlimited = 0;

/** flags used for the createData() function.  These control how the the sound data object is
 *  created or loaded.  Zero or more of these flags may be combined to change the way the audio
 *  data is loaded.  Only one of the fDataFlagFormat* flags may be used since they are all mutually
 *  exclusive.
 *
 *  Note that not all of these flags can be used when loading a sound object through the asset
 *  system.  Some flags require additional information in order to function properly and that
 *  information cannot be passed in through the asset system's loadAsset() function.
 *
 *  @{
 */
/** base type used to specify the fDataFlag* flags for createData(). */
typedef uint32_t DataFlags;

/** mask to indicate which flag bits are reserved to specify the file format flags.  These flags
 *  allow the loaded format of file data to be forced instead of auto-detected from the file's
 *  header.  All of the format flags except for @ref fDataFlagFormatRaw may be used with the
 *  asset system loader.  Note that the format values within this mask are all mutually exclusive,
 *  not individual flag bits.  This mask can be used to determine the loaded format of a sound
 *  data object after it is loaded and the load-time flags retrieved with getFlags().
 */
constexpr DataFlags fDataFlagFormatMask = 0x000000ff;

/** auto detect the format from the file header data.  This may only be used when the data is
 *  coming from a full audio file (on disk or in memory).  The format information in the file's
 *  header will always be used regardless of the filename extension.  This format flag is mutually
 *  exclusive from all other fDataFlagFormat* flags.  Once a sound data object is successfully
 *  created, this flag will be replaced with one that better represents the actual encoded data
 *  in the sound.
 */
constexpr DataFlags fDataFlagFormatAuto = 0x00000000;

/** force raw PCM data to be loaded.  This flag must be specified if the data stream does not
 *  have any format information present in it.  When this format flag is used the data stream
 *  is expected to just be the raw decodable data for the specified format.  There should not
 *  be any kind of header or chunk signature before the data.  This format flag is mutually
 *  exclusive from all other fDataFlagFormat* flags.
 */
constexpr DataFlags fDataFlagFormatRaw = 0x00000001;

/** the data was loaded as WAV PCM.  This flag will be added to the sound data object upon
 *  load to indicate that the original data was loaded from a PCM WAV/RIFF file.  If specified
 *  before load, this flag will be ignored and the load will behave as though the data format
 *  flag were specified as @ref fDataFlagFormatAuto.  This format flag is mutually exclusive
 *  from all other fDataFlagFormat* flags.
 */
constexpr DataFlags fDataFlagFormatPcm = 0x00000002;

/** This flag indicates that the metadata should be ignored when opening the sound.
 *  This is only relevant on sounds that need to be decoded from a file format that
 *  can store metadata.
 *  This is intended to be used in cases where the metadata won't be needed.
 *  Note that subsequent calls to createCodecState() which decode the loaded
 *  sound will not decode the metadata unless the @ref fDecodeStateFlagForceParse
 *  flag is used.
 */
constexpr DataFlags fDataFlagSkipMetaData = 0x00200000;

/** This flag indicates that the event points should be ignored when decoding the sound.
 *  This is only relevant on sounds that need to be decoded from a file format that
 *  can store event points
 *  This is intended to be used in cases where the event points won't be needed.
 *  Note that subsequent calls to createCodecState() which decode the loaded
 *  sound will not decode the event points unless the @ref fDecodeStateFlagForceParse
 *  flag is used.
 */
constexpr DataFlags fDataFlagSkipEventPoints = 0x00400000;

/** flag to indicate that the peak volumes for each channel should be calculated for the sound
 *  data object as its data is decoded at creation time or when streaming into the sound data
 *  object.  This does not have any affect on decode operations that occur while playing back
 *  the sound data.  This may be specified when creating an empty sound.  This may be specified
 *  when the sound data object is loaded through the asset loader system.
 */
constexpr DataFlags fDataFlagCalcPeaks = 0x01000000;

/** load the file data from a blob in memory.  The blob of file data is specified in the
 *  @ref SoundDataLoadDesc::dataBlob value and the blob's size is specified in the
 *  @ref SoundDataLoadDesc::dataBlobLengthInBytes value.  Depending on the other flags used,
 *  this blob may be copied into the new sound data object or it may be decoded into the
 *  new object.  As long as the @ref fDataFlagUserMemory flag is not also used, the blob
 *  data may be discarded upon return from createData().  This flag is always implied
 *  when loading a sound data object through the asset loader system.
 */
constexpr DataFlags fDataFlagInMemory = 0x02000000;

/** when the @ref fDataFlagInMemory flag is also used, this indicates that the original memory
 *  blob should be directly referenced in the new sound data object instead of copying it.  When
 *  this flag is used, it is the caller's responsibility to ensure the memory blob remains valid
 *  for the entire lifetime of the sound data object. Note that if the @ref fDataFlagDecode flag
 *  is specified and the sound is encoded as a PCM format (either in a WAVE file or raw PCM loaded
 *  with @ref fDataFlagFormatRaw), the original memory blob will still be referenced. Using
 *  @ref fDataFlagDecode with any other format, such as @ref SampleFormat::eVorbis, will decode
 *  the audio into a new buffer and the original blob will no longer be needed.
 *
 *  This flag is useful for creating sound data objects that reference audio data in a sound
 *  bank or sound atlas type object that exists for the lifetime of a scene.  The original
 *  data in the bank or atlas can be referenced directly instead of having to copy it and
 *  use twice the memory (and time to copy it).
 */
constexpr DataFlags fDataFlagUserMemory = 0x04000000;

/** create the sound data object as empty.  The buffer will be allocated to the size
 *  specified in @ref SoundDataLoadDesc::bufferLength and will be filled with silence.
 *  The data format information also must be filled out in the SoundDataLoadDesc
 *  descriptor before calling createData().  All other flags except for the
 *  @ref fDataFlagNoName and @ref fDataFlagCalcPeaks flags will be ignored when this
 *  flag is used.  This flag is not allowed if specified through the asset loader
 *  system since it requires extra information.
 */
constexpr DataFlags fDataFlagEmpty = 0x08000000;

/** use the user-decode callbacks when loading or streaming this data.  In this case, the
 *  format of the original sound is unspecified and unknown.  The decode callback will be
 *  used to convert all of the object's data to PCM data when streaming or loading (depending
 *  on the other flags used).  When this flag is used, the decoded format information in the
 *  SoundDataLoadDesc descriptor must be specified.  This flag is not allowed if specified
 *  through the asset loader system since it requires extra information.
 *
 *  In addition to allowing additional audio formats to be decoded, the user decode callbacks
 *  can also act as a simple abstract datasource; this may be useful when wanting to read data
 *  from a pack file without having to copy the full file blob out to memory.
 */
constexpr DataFlags fDataFlagUserDecode = 0x10000000;

/** stream the audio data at runtime.  The behavior when using this flag greatly depends
 *  on some of the other flags and the format of the source data.  For example, if the
 *  @ref fDataFlagInMemory flag is not used, the data will be streamed from disk.  If
 *  that flag is used, the encoded/compressed data will be loaded into the sound data
 *  object and it will be decoded at runtime as it is needed.  This flag may not be
 *  combined with the @ref fDataFlagDecode flag.  If it is, this flag will be ignored
 *  and the full data will be decoded into PCM at load time.  If neither this flag nor
 *  @ref fDataFlagDecode is specified, the @ref fDataFlagDecode flag will be implied.
 *  This flag is valid to specify when loading a sound data object through the asset
 *  loader system.
 */
constexpr DataFlags fDataFlagStream = 0x20000000;

/** decode the sound's full data into PCM at load time.  The full stream will be converted
 *  to PCM data immediately when the new sound data object is created.  The destination
 *  PCM format will be chosen by the decoder if the @ref SoundDataLoadDesc::pcmFormat value
 *  is set to @ref SampleFormat::eDefault.  If it is set to one of the SampleFormat::ePcm*
 *  formats, the stream will be decoded into that format instead.  This flag is valid to
 *  specify when loading a sound data object through the asset loader system.  However,
 *  if it is used when loading an asset, the original asset data will only be referenced
 *  if it was already in a PCM format.  Otherwise, it will be decoded into a new buffer
 *  in the new sound data object.  If both this flag and @ref fDataFlagStream are specified,
 *  this flag will take precedence.  If neither flag is specified, this one will be implied.
 */
constexpr DataFlags fDataFlagDecode = 0x40000000;

/** don't store the asset name or filename in the new sound data object.  This allows some
 *  memory to to be saved by not storing the original filename or asset name when loading a
 *  sound data object from file, through the asset system, or when creating an empty object.
 *  This will also be ignored if the @ref fDataFlagStream flag is used when streaming from
 *  file since the original filename will be needed to reopen the stream for each new playing
 *  instance.  This flag is valid to specify when loading a sound data object through the
 *  asset loader system.
 */
constexpr DataFlags fDataFlagNoName = 0x80000000;
/** @} */


/**
 *  callback function prototype for reading data for fDataFlagUserDecode sound data objects.
 *
 *  @param[in] soundData    the sound object to read the sound data for.  This object will be
 *                          valid and can be accessed to get information about the decoding
 *                          format.  The object's data buffer should not be accessed from
 *                          the callback, but the provided @p data buffer should be used instead.
 *                          This may not be nullptr.
 *  @param[out] data    the buffer that will receive the decoded audio data.  This buffer will be
 *                      large enough to hold @p dataLength bytes.  This may be nullptr to indicate
 *                      that the remaining number of bytes in the stream should be returned in
 *                      @p dataLength instead of the number of bytes read.
 *  @param[inout] dataLength    on input, this contains the length of the @p data buffer in bytes.
 *                              On output, if @p data was not nullptr, this will contain the
 *                              number of bytes actually written to the buffer.  If @p data
 *                              was nullptr, this will contain the number of bytes remaining to be
 *                              read in the stream.  All data written to the buffer must be frame
 *                              aligned.
 *  @param[in] context  the callback context value specified in the SoundDataLoadDesc object.
 *                      This is passed in unmodified.
 *  @returns AudioResult.eOk if the read operation is successful.
 *  @returns AudioResult.eTryAgain if the read operation was not able to fill an entire buffer and
 *           should be called again.  This return code should be used  when new data is not yet
 *           available but is expected soon.
 *  @returns AudioResult.eOutOfMemory if the full audio stream has been decoded (if it decides
 *           not to loop).  This indicates that there is nothing left to decode.
 *  @returns an AudioResult.* error code if the callback could not produce its data for any
 *           other reason.
 *
 *  @remarks This is used to either decode data that is in a proprietary format or to produce
 *           dynamic data as needed.  The time and frequency at which this callback is performed
 *           depends on the flags that were originally passed to createData() when the sound
 *           data object was created.  If the @ref fDataFlagDecode flag is used, this would
 *           only be performed at load time to decode the entire stream.
 *
 *  @remarks When using a decoding callback, the data written to the buffer must be PCM data in
 *           the format expected by the sound data object.  It is the host app's responsibility
 *           to know the sound format information before calling createData() and to fill
 *           that information into the @ref SoundDataLoadDesc object.
 */
typedef AudioResult(CARB_ABI* SoundDataReadCallback)(const SoundData* soundData,
                                                     void* data,
                                                     size_t* dataLength,
                                                     void* context);

/**
 *  an optional callback to reposition the data pointer for a user decoded stream.
 *
 *  @param[in] soundData    the sound data object to set the position for.  This object will be
 *                          valid and can be used to read data format information.  Note that the
 *                          host app is expected to know how to convert the requested decoded
 *                          position into an encoded position.  This may not be nullptr.
 *  @param[in] position the new position to set for the stream.  This value must be greater than
 *                      or equal to 0, and less than the length of the sound (as returned from
 *                      getLength()).  This value is interpreted according to the @p type value.
 *  @param[in] type     the units to interpret the new read cursor position in.  Note that if this
 *                      is specified in milliseconds, the actual position that it seeks to may not
 *                      be accurate.  Similarly, if a position in bytes is given, it will be
 *                      rounded up to the next frame boundary.
 *  @param[in] context  the callback context value specified in the @ref SoundDataLoadDesc object.
 *                      This is passed in unmodified.
 *  @returns AudioResult.eOk if the positioning operation was successful.
 *  @returns AudioResult.eInvalidParameter if the requested offset is outside the range of the
 *           active sound.
 *  @returns an AudioResult.* error code if the operation fails for any other reason.
 *
 *  @remarks This is used to handle operations to reposition the read cursor for user decoded
 *           sounds.  This callback occurs when a sound being decoded loops or when the current
 *           playback/decode position is explicitly changed.  The callback will perform the actual
 *           work of positioning the decode cursor and the new decoding state information should
 *           be updated on the host app side.  The return value may be returned directly from
 *           the function that caused the read cursor position to change in the first place.
 */
typedef AudioResult(CARB_ABI* SoundDataSetPosCallback)(const SoundData* soundData,
                                                       size_t position,
                                                       UnitType type,
                                                       void* context);

/** An optional callback that gets fired when the SoundData's final reference is released.
 *  @param[in] soundData    The sound data object to set the destructor for.
 *                          This object will still be valid during this callback,
 *                          but immediately after this callback returns, @p soundData
 *                          will be invalid.
 *  @param[in] context      the callback context value specified in the @ref SoundDataLoadDesc object.
 *                          This is passed in unmodified.
 */
typedef void(CARB_ABI* SoundDataDestructionCallback)(const SoundData* soundData, void* context);


/** the memory limit threshold for determining if a sound should be decoded into memory.
 *   When the fDataFlagDecode flag is used and the size of the decoded sound is over this limit,
 *   the sound will not be decoded into memory.
 */
constexpr size_t kMemoryLimitThreshold = 1ull << 31;

/** a descriptor for the sound data to be loaded.  This is a flexible loading method that allows
 *  sound data to be loaded from file, memory, streamed from disk, loaded as raw PCM data, loaded
 *  from a proprietary data format, decoded or decompressed at load time, or even created as an
 *  empty sound buffer.  The loading method depends on the flags used.  For data loaded from file
 *  or a blob in memory, the data format can be auto detected for known supported formats.
 *
 *  Not all members in this object are used on each loading path.  For example, the data format
 *  information will be ignored when loading from a file that already contains format information
 *  in its header.  Regardless of whether a particular value is ignored, it is still the caller's
 *  responsibility to appropriately initialize all members of this object.
 *
 *  Sound data is loaded using this descriptor through a single loader function.  Because there
 *  are more than 60 possible combinations of flags that can be used when loading sound data,
 *  it's not feasible to create a separate loader function for each possible method.
 */
struct SoundDataLoadDesc
{
    /** flags to control how the sound data is loaded and decoded (if at all).  This is a
     *  combination of zero or more of the fDataFlag* flags.  By default, the sound data's
     *  format will attempt to be auto detected and will be fully loaded and decoded into memory.
     *  This value must be initialized before calling createData().
     */
    DataFlags flags = 0;

    /** Dummy member to enforce padding. */
    uint32_t padding1{ 0 };

    /** filename or asset name for the new object.  This may be specified regardless of whether
     *  the @ref fDataFlagInMemory flag is used.  When that flag is used, this can be used to
     *  give an asset name to the sound object.  The name will not be used for any purpose
     *  except as a way to identify it to a user in that case.  When loading the data from
     *  a file, this represents the filename to load from.  When the @ref fDataFlagInMemory
     *  flag is not used, this must be the filename to load from.  This may be nullptr only
     *  if the audio data is being loaded from a blob in memory.
     */
    const char* name = nullptr;

    /** when the @ref fDataFlagInMemory flag is used, this is the blob of data to load from
     *  memory.  If the flag is not specified, this value will be ignored.  When loading from
     *  memory, the @ref dataBlobLengthInBytes will indicate the size of the data blob in bytes.
     *  Specifying a data blob with @ref fDataFlagFormatRaw with a pointer that is misaligned for
     *  its sample type is allowed; the effects of @ref fDataFlagUserMemory will be disabled so
     *  a properly aligned local buffer can be allocated.
     *  The effects of @ref fDataFlagUserMemory will also be disabled when specifying a wave file
     *  blob where the data chunk is misaligned for its sample type (this is only possible for 32
     *  bit formats).
     */
    const void* dataBlob = nullptr;

    /** when the @ref fDataFlagInMemory flag is used, this value specifies the size of
     *  the data blob to load in bytes.  When the flag is not used, this value is ignored.
     */
    size_t dataBlobLengthInBytes = 0;

    /** the number of channels to create the sound data with.  This value is ignored if the sound
     *  data itself contains an embedded channel count (ie: when loading from file).  This must be
     *  initialized to a non-zero value when the @ref fDataFlagFormatRaw, @ref fDataFlagEmpty,
     *  or @ref fDataFlagUserDecode flags are used.
     *  If @ref fDataFlagUserDecode is used and @ref encodedFormat is a non-PCM format, this will
     *  be ignored.
     */
    size_t channels = kDefaultChannelCount;

    /** a mask that maps speaker channels to speakers.  All channels in the stream are interleaved
     *  according to standard SMPTE order.  This mask indicates which of those channels are
     *  present in the stream.  This may be @ref kSpeakerModeDefault to allow a standard speaker
     *  mode to be chosen from the given channel count.
     */
    SpeakerMode channelMask = kSpeakerModeDefault;

    /** the rate in frames per second that the sound was originally mastered at.  This will be the
     *  default rate that it is processed at.  This value is ignored if the sound data itself
     *  contains an embedded frame rate value (ie: when loading from file).  This must be
     *  initialized to a non-zero value when the @ref fDataFlagFormatRaw, @ref fDataFlagEmpty,
     *  or @ref fDataFlagUserDecode flags are used.
     *  If @ref fDataFlagUserDecode is used and @ref encodedFormat is a non-PCM format, this will
     *  be ignored.
     */
    size_t frameRate = kDefaultFrameRate;

    /** the data format of each sample in the sound.  This value is ignored if the sound data
     *  itself contains an embedded data format value (ie: when loading from file).  This must be
     *  initialized to a non-zero value when the @ref fDataFlagFormatRaw, @ref fDataFlagEmpty,
     *  or @ref fDataFlagUserDecode flags are used.  This represents the encoded sample format
     *  of the sound.
     *
     *  If the @ref fDataFlagUserDecode flag is used, this will be the format produced by the
     *  user decode callback.
     *  Note that PCM data produced from a user decode callback must be raw PCM data rather than
     *  a WAVE file blob.
     *  The user decode callback does not need to provide whole frames/blocks of this sample type,
     *  since this effectively acts as an arbitrary data source.
     *  This allows you to specify that the user decode callback returns data in a non-PCM format
     *  and have it decoded to the PCM format specified by @ref pcmFormat.
     *
     *  If the @ref fDataFlagEmpty flag is used and this is set to @ref SampleFormat::eDefault,
     *  this will be set to the same sample format as the @ref pcmFormat format.
     */
    SampleFormat encodedFormat = SampleFormat::eDefault;

    /** the decoded or preferred intermediate PCM format of the sound.  This value should be set
     *  to @ref SampleFormat::eDefault to allow the intermediate format to be chosen by the
     *  decoder.  Otherwise, this should be set to one of the SampleFormat::ePcm* formats to
     *  force the decoder to use a specific intermediate or internal representation of the sound.
     *  This is useful for saving memory on large decoded sounds by forcing a smaller format.
     *
     *  When the @ref fDataFlagDecode flag is used, this will be the PCM format that the data is
     *  decoded into.
     *
     *  When the @ref fDataFlagEmpty flag is used and this is set to @ref SampleFormat::eDefault,
     *  the decoder will choose the PCM format.  If the @ref encodedFormat value is also set to
     *  @ref SampleFormat::eDefault, it will also use the decoder's preferred PCM format.
     */
    SampleFormat pcmFormat = SampleFormat::eDefault;

    /** specifies the desired length of an empty sound data buffer, a raw buffer, or user decode
     *  buffer.  This value is interpreted according to the units in @ref bufferLengthType.  This
     *  value is ignored if the sound data itself contains embedded length information (ie: when
     *  loading from file).  This must be initialized to a non-zero value when either the
     *  @ref fDataFlagFormatRaw, @ref fDataFlagEmpty, or @ref fDataFlagUserDecode flags are used.
     *  When using this with @ref fDataFlagEmpty, the sound data object will initially be marked
     *  as containing zero valid frames of data.  If played, this will always decode silence.
     *  If the host app writes new data into the buffer, it must also update the valid data size
     *  with setValidLength() so that the new data can be played.
     */
    size_t bufferLength = 0;

    /** determines how the @ref bufferLength value should be interpreted.  This value is ignored
     *  in the same cases @ref bufferLength are ignored in.  For @ref fDataFlagEmpty, this may be
     *  any valid unit type.  For @ref fDataFlagFormatRaw and @ref fDataFlagUserDecode, this may
     *  only be @ref UnitType::eFrames or @ref UnitType::eBytes.
     */
    UnitType bufferLengthType = UnitType::eFrames;

    /** Dummy member to enforce padding. */
    uint32_t padding2{ 0 };

    /** a callback function to provide decoded PCM data from a user-decoded data format.  This
     *  value is ignored unless the @ref fDataFlagUserDecode flag is used.  This callback
     *  is responsible for decoding its data into the PCM format specified by the rest of the
     *  information in this descriptor.  The callback function or caller are responsible for
     *  knowing the decoded format before calling createData() and providing it in this
     *  object.
     */
    SoundDataReadCallback readCallback = nullptr;

    /** an optional callback function to provide a way to reposition the decoder in a user
     *  decoded stream.  This value is ignored unless the @ref fDataFlagUserDecode flag
     *  is used.  Even when the flag is used, this callback is only necessary if the
     *  @ref fDataFlagStream flag is also used and the voice playing it expects to either
     *  loop the sound or be able to reposition it on command during playback.  If this callback
     *  is not provided, attempts to play this sound on a looping voice or attempts to change
     *  the streaming playback position will simply fail.
     */
    SoundDataSetPosCallback setPosCallback = nullptr;

    /** an opaque context value that will be passed to the readCallback and setPosCallback
     *  functions each time they are called.  This value is a caller-specified object that
     *  is expected to contain the necessary decoding state for a user decoded stream.  This
     *  value is only necessary if the @ref fDataFlagUserDecode flag is used.  This value
     *  will only be used at load time on a user decoded stream if the @ref fDataFlagDecode
     *  flag is used (ie: causing the full sound to be decoded into memory at load time).  If
     *  the sound is created to be streamed, this will not be used.
     */
    void* readCallbackContext = nullptr;

    /** An optional callback that gets fired when the SoundData's final
     *  reference is released. This is intended to make it easier to perform
     *  cleanup of a SoundData in cases where @ref fDataFlagUserMemory is used.
     */
    SoundDataDestructionCallback destructionCallback = nullptr;

    /** An opaque context value that will be passed to @ref destructionCallback
     *  when the last reference to the SoundData is released.
     *  This will not be called if the SoundData is not created successfully.
     */
    void* destructionCallbackContext = nullptr;

    /** Reserved for future expansion for options to be used when @ref fDataFlagDecode
     *  is specified.
     */
    void* encoderSettings = nullptr;

    /** the maximum number of simultaneous playing instances that this sound can have.  This
     *  can be @ref kInstancesUnlimited to indicate that there should not be a play limit.
     *  This can be any other value to limit the number of times this sound can be played
     *  at any one time.
     */
    uint32_t maxInstances = kInstancesUnlimited;

    /** Dummy member to enforce padding. */
    uint32_t padding3{ 0 };

    /** the size in bytes at which to decide whether to decode or stream this sound.  This
     *  will only affect compressed non-PCM sound formats.  This value will be ignored for
     *  any PCM format regardless of size.  This can be zero to just decide to stream or
     *  decode based on the @ref fDataFlagDecode or @ref fDataFlagStream flags.  If this
     *  is non-zero, the sound will be streamed if its PCM size is larger than this limit.
     *  The sound will be fully decoded if its PCM size is smaller than this limit.  In
     *  this case, the @ref fDataFlagDecode flag and @ref fDataFlagStream flag will be ignored.
     *
     *  Note that if this is non-zero, this will always override the stream and decode flags'
     *  behavior.
     */
    size_t autoStreamThreshold = 0;

    /** reserved for future expansion.  This must be set to nullptr. */
    void* ext = nullptr;
};

/** additional load parameters for sound data objects.  These are passed through to the asset
 *  loader as a way of passing additional options beyond just the filename and flags.  These
 *  additional options will persist for the lifetime of the loaded asset and will be passed
 *  to the loader function each time that asset needs to be reloaded from its original data
 *  source.  Any shallow copied objects in here must be guaranteed persistent by the caller
 *  for the entire period the asset is valid.  It is the host app's responsibility to clean
 *  up any resources in this object once the asset it was used for has been unloaded.
 *
 *  In general, it is best practice not to fill in any of the pointer members of this struct
 *  and to allow them to just use their default behavior.
 */
struct SoundLoadParameters : public carb::assets::LoadParameters
{
    /** additional parameters to pass to the asset loader.  The values in here will follow
     *  all the same rules as using the @ref SoundDataLoadDesc structure to directly load
     *  a sound data object, except that the @a dataBlob and @a dataBlobLengthInBytes values
     *  will be ignored (since they are provided by the asset loader system).  The other
     *  behavior that will be ignored will be that the @ref fDataFlagInMemory flag will
     *  always be used.  Loading a sound data object through the asset system does not
     *  support loading from a disk filename (the asset system itself will handle that
     *  if the data source supports it).
     *
     *  @note most of the values in this parameter block are still optional.  Whether
     *        each value is needed or not often depends on the flags that are specified.
     *
     *  @note most the pointer members in this parameter block should be set to nullptr
     *        for safety and ease of cleanup.  This includes the @a name, @a dataBlob, and
     *        @a encoderSettings values.  Setting the @a readCallbackContext and
     *        @a destructionCallbackContext values is acceptable because the host app is
     *        always expected to manage those objects' lifetimes anyway.
     */
    SoundDataLoadDesc params = {};
};


/************************************* Codec State Objects ***************************************/
/** names to identify the different parts of a codec.  These are used to indicate which type of
 *  codec state needs to be created or to indicate which type of sound format to retrieve.
 */
enum class CodecPart
{
    /** identifies the decoder part of the codec or that the decoded sound format should be
     *  retrieved.  When retrieving a format, this will be the information for the PCM format
     *  for the sound.  When creating a codec state, this will expect that the decoder descriptor
     *  information has been filled in.
     */
    eDecoder,

    /** identifies the encoder part of the codec or that the encoded sound format should be
     *  retrieved.  When retrieving a format, this will be the information for the encoded
     *  format for the sound.  When creating a codec state, this will expect that the encoder
     *  descriptor information has been filled in.
     */
    eEncoder,
};

/** Flags that alter the decoding behavior for SoundData objects.
 */
typedef uint64_t DecodeStateFlags;

/** If this flag is set, the header information of the file will be parsed
 *  every time createCodecState() is called.
 *  If this flag is not set, the header information of the file will be cached
 *  if possible.
 */
constexpr DecodeStateFlags fDecodeStateFlagForceParse = 0x00000001;

/** If this flag is set and the encoded format supports this behavior, indexes
 *  for seek optimization will be generated when the CodecState is created.
 *  For a streaming sound on disk, this means that the entire sound will be
 *  read off disk when creating this index; the sound will not be decoded or
 *  fully loaded into memory, however.
 *  This will reduce the time spent when seeking within a SoundData object.
 *  This will increase the time spent initializing the decoding stream, and this
 *  will use some additional memory.
 *  This option currently only affects @ref SampleFormat::eVorbis and
 *  @ref SampleFormat::eOpus.
 *  This will clear the metadata and event points from the sound being decoded
 *  unless the corresponding flag is used to skip the parsing of those elements.
 */
constexpr DecodeStateFlags fDecodeStateFlagOptimizeSeek = 0x00000002;

/** This flag indicates that frame accurate seeking is not needed and the decoder
 *  may skip additional work that is required for frame-accurate seeking.
 *  An example usage of this would be a music player; seeking is required, but
 *  frame-accurate seeking is not required.
 *  Additionally, this may be useful in cases where the only seeking needed is
 *  to seek back to the beginning of the sound, since that can always be done
 *  with perfect accuracy.
 *
 *  This only affects @ref SampleFormat::eVorbis, @ref SampleFormat::eOpus and
 *  @ref SampleFormat::eMp3.
 *  For @ref SampleFormat::eVorbis, @ref SampleFormat::eOpus, this will cause
 *  the decoder to seek to the start of the page containing the target frame,
 *  rather than trying to skip through that page to find the exact target frame.
 *
 *  For @ref SampleFormat::eMp3, this flag will skip the generation of an index
 *  upon opening the file.
 *  This may result in the file length being reported incorrectly, depending on
 *  how the file was encoded.
 *  This will also result in seeking being performed by estimating the target
 *  frame's location (this will be very inaccurate for variable bitrate files).
 */
constexpr DecodeStateFlags fDecodeStateFlagCoarseSeek = 0x00000004;

/** This flag indicates that the metadata should be ignored when decoding the
 *  sound. This is intended to be used in cases where the metadata won't be
 *  used, such as decoding audio for playback.
 *  Note that this only takes effect when @ref fDecodeStateFlagForceParse is
 *  used.
 */
constexpr DecodeStateFlags fDecodeStateFlagSkipMetaData = 0x00000008;

/** This flag indicates that the event points should be ignored when decoding the
 *  sound. This is intended to be used in cases where the event points won't be
 *  used, such as decoding audio for playback.
 *  Note that this only takes effect when @ref fDecodeStateFlagForceParse is
 *  used.
 */
constexpr DecodeStateFlags fDecodeStateFlagSkipEventPoints = 0x00000010;

/** a descriptor of how to create a sound decode state object with createCodecState().  By
 *  separating this object from the sound data itself, this allows the sound to be trivially
 *  streamed or decoded to multiple voices simultaneously without having to worry about
 *  managing access to the sound data or loading it multiple times.
 */
struct DecodeStateDesc
{
    /** flags to control the behavior of the decoder. This may be 0 or a
     *  combination of the kDecodeState* flags.
     */
    DecodeStateFlags flags;

    /** the sound data object to create the decoder state object for.  The size and content
     *  of the decoder object depends on the type of data contained within this object.
     *  This may not be nullptr.  Note that in some cases, the format and length information
     *  in this object may be updated by the decoder.  This would only occur in cases where
     *  the data were being streamed from disk.  If streaming from memory the cached header
     *  information will be used instead.  If this is used at load time (internally), the
     *  sound data object will always be modified to cache all the important information about
     *  the sound's format and length.
     */
    SoundData* soundData;

    /** the desired output format from the decoder.
     *  This can be SampleFormat::eDefault to use the format from @p soundData;
     *  otherwise, this must be one of the SampleFormat::ePcm* formats.
     */
    SampleFormat outputFormat;

    /** an opaque context value that will be passed to the readCallback and setPosCallback
     *  functions each time they are called.  This value is a caller-specified object that
     *  is expected to contain the necessary decoding state for a user decoded stream.  This
     *  value is only necessary if the @ref fDataFlagUserDecode flag was used when the
     *  sound data object was created.  By specifying this separately from the sound data,
     *  this allows multiple voices to be able to play a user decoded stream simultaneously.
     *  It is up to the caller to provide a unique decode state object here for each playing
     *  instance of the user decoded stream if there is an expectation of multiple instances.
     */
    void* readCallbackContext;

    /** reserved for future expansion.  This must be set to nullptr. */
    void* ext;
};

/** flags to control the behavior of the encoder.
 *
 *  @{
 */
/** base type for the encoder descriptor flags. */
typedef uint64_t EncodeStateFlags;

/** Avoid expanding the target @ref SoundData if it runs out of space.  The
 *  encoder will simply start to fail when the buffer is full if this flag is
 *  used.  Note that for some formats this may cause the last block in the
 *  stream to be missing if the buffer is not block aligned in size.
 */
constexpr EncodeStateFlags fEncodeStateFlagNoExpandBuffer = 0x00000001;

/** Don't copy the metadata information into the target @ref SoundData.
 */
constexpr EncodeStateFlags fEncodeStateFlagStripMetaData = 0x00000002;

/** Don't copy the event point information into the target @ref SoundData.
 */
constexpr EncodeStateFlags fEncodeStateFlagStripEventPoints = 0x00000004;

/** Don't copy the peaks information into the target @ref SoundData.
 */
constexpr EncodeStateFlags fEncodeStateFlagStripPeaks = 0x00000008;
/** @} */


/** a descriptor for creating an encoder state object.  This can encode the data into either a
 *  stream object or a sound data object.  Additional encoder settings depend on the output
 *  format that is chosen.
 */
struct EncodeStateDesc
{
    /** flags to control the behavior of the encoder.  At least one of the kEncodeStateTarget*
     *  flags must be specified.
     */
    EncodeStateFlags flags;

    /** The SoundData this encoding is associated with, if any.
     *  The Metadata and event points will be copied from this to the header
     *  of the encoded data.
     *  This can be set to nullptr if there is no SoundData associated with this
     *  encoding.
     */
    const SoundData* soundData;

    /** The target for the encoder.
     *  This may not be nullptr.
     *  Note that the target's format information will be retrieved to
     *  determine the expected format for the encoder's output.  At least for
     *  the channel count and frame rate, this information must also match that
     *  of the encoder's input stream.  The sample format is the only part of
     *  the format information that the encoder may change.
     *  @ref target is treated as if it were empty. Any existing valid
     *  length will be ignored and the encoder will begin writing at the
     *  start of the buffer. If the metadata or event points are set to be
     *  copied, from @ref soundData, then those elements of @ref target will
     *  be cleared first. Passing @ref fEncodeStateFlagStripMetaData or
     *  @ref fEncodeStateFlagStripEventPoints will also clear the metadata
     *  and event points, respectively.
     */
    SoundData* target;

    /** the expected input format to the encoder.  This must be one of the SampleFormat::ePcm*
     *  formats.
     */
    SampleFormat inputFormat;

    /** additional output format dependent encoder settings.  This should be nullptr for PCM
     *  data formats.  Additional objects will be defined for encoder formats that require
     *  additional parameters (optional or otherwise).  For formats that require additional
     *  settings, this may not be nullptr.  Use getCodecFormatInfo() to retrieve the info
     *  for the codec to find out if the additional settings are required or not.
     */
    void* encoderSettings;

    /** reserved for future expansion.  This must be set to nullptr. */
    void* ext;
};

/** a descriptor for the codec state that should be created.  This contains the state information
 *  descriptors for both the encoder and decoder parts of the codec.  Only one part may be valid
 *  at any given point.  The part that is specified will indicate which kind of codec state object
 *  is created.
 */
struct CodecStateDesc
{
    /** the codec part that indicates both which type of state object will be created and which
     *  part of the descriptor is valid.
     */
    CodecPart part;

    /** the specific codec state descriptors. */
    union
    {
        DecodeStateDesc decode; ///< filled in when creating a decoder state.
        EncodeStateDesc encode; ///< filled in when creating an encoder state.
    };

    /** reserved for future expansion.  This must be set to nullptr. */
    void* ext;
};

/** Settings specific to wave file encoding.
 *  This is not required when encoding wave audio.
 *  This can optionally be specified when encoding into any PCM format.
 */
struct WaveEncoderSettings
{
    /** If this is specified, up to 10 bytes of padding will be added to align
     *  the data chunk for its data format, so that decoding will be more efficient.
     *  This is done with a 'JUNK' chunk.
     *  The data chunk can only be misaligned for @ref SampleFormat::ePcm32 and
     *  @ref SampleFormat::ePcmFloat.
     */
    bool alignDataChunk = true;
};

/** Settings specific to Vorbis file encoding.
 *  This is not required when encoding Vorbis audio.
 */
struct VorbisEncoderSettings
{
    /** Reserved for future expansion.
     *  Must be set to 0.
     */
    uint32_t flags = 0;

    /** The encoding quality of the compressed audio.
     *  This may be within the range of -0.1 to 1.0.
     *  Vorbis is a lossy codec with variable bitrate, so this doesn't correlate
     *  to an exact bitrate for the output audio.
     *  A lower quality increases encode time and decreases decode time.
     *  0.8-0.9 is suitable for cases where near-perfect reproduction of the
     *  original audio is desired, such as music that will be listened to on
     *  its own.
     *  Lower quality values for the audio should be acceptable for most use
     *  cases, but the quality value at which artifacts become obvious will
     *  depend on the content of the audio, the use case and the quality of the
     *  speakers used.
     *  With very low quality settings, such as -0.1, audio artifacts will be
     *  fairly obvious in music, but for simpler audio, such as voice
     *  recordings, the quality loss may not be as noticeable (especially in
     *  scenes with background noise).
     *  This is 0.9 by default.
     */
    float quality = 0.9f;

    /** If this is true, the encoder will expect its input to be in Vorbis
     *  channel order. Otherwise WAVE channel order will be expected.
     *  All codecs use WAVE channel order by default, so this should be set to
     *  false in most cases.
     *  This is false by default.
     */
    bool nativeChannelOrder = false;
};

/** The file type used to store FLAC encoded audio. */
enum class FlacFileType
{
    /** A .flac container.
     *  This is the most common container type for FLAC encoded audio.
     *  This is the default format.
     */
    eFlac,

    /** A .ogg container.
     *  This allows FLAC to take advantage of all of the features of the Ogg
     *  container format.
     *  FLAC data encoded in Ogg containers will be slightly larger and slower
     *  to decode than the same data stored in a .flac container.
     */
    eOgg,
};

/** Settings specific to FLAC file encoding.
 *  This is not required when encoding FLAC audio.
 */
struct FlacEncoderSettings
{
    /** Reserved for future expansion. */
    uint32_t flags = 0;

    /** The file container type which will be used.
     *  The default value is @ref FlacFileType::eFlac.
     */
    FlacFileType fileType = FlacFileType::eFlac;

    /** The number of bits per sample to store.
     *  This can be used to truncate the audio to a smaller value, such as 16.
     *  This must be a value within the range of 4-24. Using values other than
     *  8, 12, 16, 20 and 24 requires that @ref streamableSubset is set to
     *  false.
     *  Although FLAC supports up to 32 bits per sample, the encoder used
     *  only supports up to 24 bits per sample.
     *  The default value for this will be the bit width of the input format,
     *  except for SampleFormat::ePcm32 and SampleFormat::ePcmFloat, which are
     *  reduced to 24 bit.
     *  This can be set to 0 to use the default for the input type.
     */
    uint32_t bitsPerSample = 0;

    /** Set the compression level preset.
     *  This must be in the range [0-8], where 8 is the maximum compression level.
     *  A higher level will have a better compression ratio at the cost of
     *  compression time.
     *  The default value is 5.
     */
    uint32_t compressionLevel = 5;

    /** Set the block size for the encoder to use.
     *  Set this to 0 to let the encoder choose.
     *  It is recommended to leave this at 0.
     *  The default value is 0.
     */
    uint32_t blockSize = 0;

    /** The FLAC 'streamable subset' is a subset of the FLAC encoding that is
     *  intended to allow decoders that cannot seek to begin playing from the
     *  middle of a stream.
     *  If this is set to true, the codec state creation will fail if the
     *  following conditions are not met:
     *    - If the frame rate is above 65536, the frame rate must be divisible
     *      by 10. (see the FLAC standard for an explanation of this).
     *    - @ref bitsPerSample must be 8, 12, 16, 20 or 24.
     *    - Specific restrictions are placed on @ref blockSize.
     *      Please read the FLAC standard if you need to tune that parameter.
     *  Setting this to false may improve the compression ratio and decoding
     *  speed. Testing has shown only slight improvement from setting this
     *  option to false.
     *  The default value for this is true.
     */
    bool streamableSubset = true;

    /** Decode the encoded audio to verify that the encoding was performed
     *  correctly. The encoding will fail if a chunk does not verify.
     *  The default value for this is false.
     */
    bool verifyOutput = false;
};

/** The intended usage for audio.
 *  This is used to optimize the Opus encoding for certain applications.
 */
enum class OpusCodecUsage
{
    /** General purpose codec usage. Don't optimize for any specific signal type. */
    eGeneral,

    /** Optimize for the best possible reproduction of music. */
    eMusic,

    /** Optimize to ensure that speech is as recognizable as possible for a
     *  given bitrate.
     *  This should be used for applications such as voice chat, which require
     *  a low bitrate to be used.
     */
    eVoice,
};

/** Encode @ref SampleFormat::eOpus with the maximum possible bitrate. */
const uint32_t kOpusBitrateMax = 512001;

/** Flags to use when encoding audio in @ref SampleFormat::eOpus. */
using OpusEncoderFlags = uint32_t;

/** Optimize the encoder for minimal latency at the cost of quality.
 *  This will disable the LPC and hybrid modules, which will disable
 *  voice-optimized modes and forward error correction.
 *  This also disables some functionality within the MDCT module.
 *  This reduces the codec lookahead to 2.5ms, rather than the default of 6.5ms.
 */
constexpr OpusEncoderFlags fOpusEncoderFlagLowLatency = 0x00000001;

/** Specify whether the encoder is prevented from producing variable bitrate audio.
 *  This flag should only be set if there is a specific need for constant bitrate audio.
 */
constexpr OpusEncoderFlags fOpusEncoderFlagConstantBitrate = 0x00000002;

/** This enables a mode in the encoder where silence will only produce
 *  one frame every 400ms. This is intended for applications such as voice
 *  chat that will continuously send audio, but long periods of silence
 *  will be produced.
 *  This is often referred to as DTX.
 */
constexpr OpusEncoderFlags fOpusEncoderFlagDiscontinuousTransmission = 0x00000004;

/** Disable prediction so that any two blocks of Opus data are (almost
 *  completely) independent.
 *  This will reduce audio quality.
 *  This will disable forward error correction.
 *  This should only be set if there is a specific need for independent
 *  frames.
 */
constexpr OpusEncoderFlags fOpusEncoderFlagDisablePrediction = 0x00000008;

/** If this is true, the encoder will expect its input to be in Vorbis
 *  channel order. Otherwise WAVE channel order will be expected.
 *  All codecs use WAVE channel order by default, so this should be set to
 *  false in most cases.
 *  This is only valid for a stream with 1-8 channels.
 */
constexpr OpusEncoderFlags fOpusEncoderFlagNativeChannelOrder = 0x00000010;


/** Settings specific to @ref SampleFormat::eOpus audio encoding.
 *  This is not required when encoding Opus audio.
 */
struct OpusEncoderSettings
{
    /** The flags to use when encoding.
     *  These are not necessary to set for general purpose use cases.
     */
    OpusEncoderFlags flags = 0;

    /** The intended usage of the encoded audio.
     *  This allows the encoder to optimize for the specific usage.
     */
    OpusCodecUsage usage = OpusCodecUsage::eGeneral;

    /** The number of frames in the audio stream.
     *  This can to be set so that the audio stream length isn't increased when
     *  encoding into Opus.
     *  Set this to 0 if the encoding stream length is unknown in advance or
     *  if you don't care about the extra padding.
     *  Setting this to non-zero when calling @ref IAudioUtils::saveToFile() is
     *  not allowed.
     *  Setting this incorrectly will result in padding still appearing at the
     *  end of the audio stream.
     */
    size_t frames = 0;

    /** The bitrate to target. Higher bitrates will result in a higher quality.
     *  This can be from 500 to 512000.
     *  Use @ref kOpusBitrateMax for the maximum possible quality.
     *  Setting this to 0 will let the encoder choose.
     *  If variable bitrate encoding is enabled, this is only a target bitrate.
     */
    uint32_t bitrate = 0;

    /** The packet size to use for encoding.
     *  This value is a multiple of 2.5ms that is used for the block size.
     *  This setting is important to modify when performing latency-sensitive
     *  tasks, such as voice communication.
     *  Using a block size less than 10ms disables the LPC and hybrid modules,
     *  which will disable voice-optimized modes and forward error correction.
     *  Accepted values are:
     *   * 1:  2.5ms
     *   * 2:  5ms
     *   * 4:  10ms
     *   * 8:  20ms
     *   * 16: 40ms
     *   * 24: 60ms
     *   * 32: 80ms
     *   * 48: 120ms
     *  Setting this to an invalid value will result in 60ms being used.
     */
    uint8_t blockSize = 48;

    /** Set the estimated packet loss during transmission.
     *  Setting this to a non-zero value will encode some redundant data to
     *  enable forward error correction in the decoded stream.
     *  Forward error correction only takes effect in the LPC and hybrid
     *  modules, so it's more effective on voice data and will be disabled
     *  when the LPC and hybrid modes are disabled.
     *  This is a value from 0-100, where 0 is no packet loss and 100 is heavy
     *  packet loss.
     *  Setting this to a higher value will reduce the quality at a given
     *  bitrate due to the redundant data that has to be included.
     *  This should be set to 0 when encoding to a file or transmitting over a
     *  reliable medium.
     *  @note packet loss compensation is not handled in the decoder yet.
     */
    uint8_t packetLoss = 0;

    /** Set the computational complexity of the encoder.
     *  This can be from 0 to 10, with 10 being the maximum complexity.
     *  More complexity will improve compression, but increase encoding time.
     *  Set this to -1 for the default.
     */
    int8_t complexity = -1;

    /** The upper bound on bandwidth to specify for the encoder.
     *  This only sets the upper bound; the encoder will use lower bandwidths
     *  as needed.
     *  Accepted values are:
     *   * 4:  4KHz - narrow band
     *   * 6:  6KHz - medium band
     *   * 8:  8KHz - wide band
     *   * 12: 12 KHz - superwide band
     *   * 20: 20 KHz - full band
     */
    uint8_t bandwidth = 20;

    /** A hint for the encoder on the bit depth of the input audio.
     *  The maximum bit depth of 24 bits is used if this is set to 0.
     *  This should only be used in cases where you are sending audio into the
     *  encoder which was previously encoded from a smaller data type.
     *  For example, when encoding @ref SampleFormat::ePcmFloat data that was
     *  previously converted from @ref SampleFormat::ePcm16, this should be
     *  set to 16.
     */
    uint8_t bitDepth = 0;

    /** The gain to apply to the output audio.
     *  Set this to 0 for unity gain.
     *  This is a fixed point value with 8 fractional bits.
     *  calculateOpusGain() can be used to calculate this parameter from a
     *  floating point gain value.
     *  calculateGainFromLinearScale() can be used if a linear volume scale is
     *  desired, rather than a gain.
     */
    int16_t outputGain = 0;
};

/** capabilities flags for codecs.  One or more of these may be set in the codec info block
 *  to indicate the various features a particular codec may support or require.
 *
 *  @{
 */
/** base type for the codec capabilities flags. */
typedef uint32_t CodecCaps;

/** capabilities flag to indicate that the codec supports encoding to the given format. */
constexpr CodecCaps fCodecCapsSupportsEncode = 0x00000001;

/** capabilities flag to indicate that the codec supports decoding from the given format. */
constexpr CodecCaps fCodecCapsSupportsDecode = 0x00000002;

/** capabilities flag to indicate that the format is compressed data (ie: block oriented or
 *  otherwise).  If this flag is not set, the format is a PCM variant (ie: one of the
 *  SampleFormat::ePcm* formats).
 */
constexpr CodecCaps fCodecCapsCompressed = 0x00000004;

/** capabilities flag to indicate that the codec supports the use of additional parameters
 *  through the @a encoderSettings value in the encoder state descriptor object.  If this
 *  flag is not set, there are no additional parameters defined for the format.
 */
constexpr CodecCaps fCodecCapsSupportsAdditionalParameters = 0x00000008;

/** capabilities flag to indicate that the codec requires the use of additional parameters
 *  through the @a encoderSettings value in the encoder state descriptor object.  If this
 *  flag is not set, the additional parameters are optional and the codec is able to choose
 *  appropriate default.
 */
constexpr CodecCaps fCodecCapsRequiresAdditionalParameters = 0x00000010;

/** capabilities flag to indicate that the codec supports setting the position within the
 *  stream.  If this flag is not set, calls to setCodecPosition() will fail when using
 *  the codec.
 */
constexpr CodecCaps fCodecCapsSupportsSetPosition = 0x00000020;

/** capabilities flag to indicate that the codec can calculate and set a frame accurate
 *  position.  If this flag is not set, the codec can only handle setting block aligned
 *  positions.  Note that this flag will never be set if @ref fCodecCapsSupportsSetPosition
 *  is not also set.
 */
constexpr CodecCaps fCodecCapsHasFrameAccuratePosition = 0x00000040;

/** capabilities flag to indicate that the codec can calculate a frame accurate count of
 *  remaining data.  If this flag is not set, the codec can only handle calculating block
 *  aligned estimates.
 */
constexpr CodecCaps fCodecCapsHasAccurateAvailableValue = 0x00000080;
/** @} */

/** information about a codec for a single sample format.  This includes information that is both
 *  suitable for display and that can be used to determine if it is safe or possible to perform a
 *  certain conversion operation.
 */
struct CodecInfo
{
    /** the encoded sample format that this codec information describes. */
    SampleFormat encodedFormat;

    /** the PCM sample format that the decoder prefers to decode to and the encoder prefers to encode from. */
    SampleFormat preferredFormat;

    /** the friendly name of this codec. */
    char name[256];

    /** the library, system service, or author that provides the functionality of this codec. */
    char provider[256];

    /** the owner and developer information for this codec. */
    char copyright[256];

    /** capabilities flags for this codec. */
    CodecCaps capabilities;

    /** minimum block size in frames supported by this codec. */
    size_t minBlockSize;

    /** maximum block size in frames supported by this codec. */
    size_t maxBlockSize;

    /** the minimum number of channels per frame supported by this codec. */
    size_t minChannels;

    /** the maximum number of channels per frame supported by this codec. */
    size_t maxChannels;
};


/*********************************** Metadata Definitions ***********************************/
/** These are the metadata tags that can be written to RIFF (.wav) files.
 *  Some of these tags were intended to be used on Video or Image data, rather
 *  than audio data, but all of these are still technically valid to use in
 *  .wav files.
 *  These are not case sensitive.
 *  @{
 */
constexpr char kMetaDataTagArchivalLocation[] = "Archival Location"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagCommissioned[] = "Commissioned"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagCropped[] = "Cropped"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagDimensions[] = "Dimensions"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagDisc[] = "Disc"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagDpi[] = "Dots Per Inch"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagEditor[] = "Editor"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagEngineer[] = "Engineer"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagKeywords[] = "Keywords"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagLanguage[] = "Language"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagLightness[] = "Lightness"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagMedium[] = "Medium"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagPaletteSetting[] = "Palette Setting"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagSubject[] = "Subject"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagSourceForm[] = "Source Form"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagSharpness[] = "Sharpness"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagTechnician[] = "Technician"; /**< Standard RIFF metadata tag. */
constexpr char kMetaDataTagWriter[] = "Writer"; /**< Standard RIFF metadata tag. */

/** These are the metadata tags that can be written to RIFF (.wav) files and
 *  also have specified usage under the Vorbis Comment metadata format standard
 *  (used by .ogg and .flac).
 *  Vorbis Comment supports any metadata tag name, but these ones should be
 *  preferred as they have a standardized usage.
 *  @{
 */
constexpr char kMetaDataTagAlbum[] = "Album"; /**< Standard Vorbis metadata tag. */
constexpr char kMetaDataTagArtist[] = "Artist"; /**< Standard Vorbis metadata tag. */
constexpr char kMetaDataTagCopyright[] = "Copyright"; /**< Standard Vorbis metadata tag. */
constexpr char kMetaDataTagCreationDate[] = "Date"; /**< Standard Vorbis metadata tag. */
constexpr char kMetaDataTagDescription[] = "Description"; /**< Standard Vorbis metadata tag. */
constexpr char kMetaDataTagGenre[] = "Genre"; /**< Standard Vorbis metadata tag. */
constexpr char kMetaDataTagOrganization[] = "Organization"; /**< Standard Vorbis metadata tag. */
constexpr char kMetaDataTagTitle[] = "Title"; /**< Standard Vorbis metadata tag. */
constexpr char kMetaDataTagTrackNumber[] = "TrackNumber"; /**< Standard Vorbis metadata tag. */

/** If a SoundData is being encoded with metadata present, this tag will
 *  automatically be added, with the value being the encoder software used.
 *  Some file formats, such as Ogg Vorbis, require a metadata section and
 *  the encoder will automatically add this tag.
 *  Under the Vorbis Comment metadata format, the 'Encoder' tag represents the
 *  vendor string.
 */
constexpr char kMetaDataTagEncoder[] = "Encoder";

/** This tag unfortunately has a different meaning in the two formats.
 *  In RIFF metadata tags, this is the 'Source' of the audio.
 *  In Vorbis Comment metadata tags, this is the International Standard
 *  Recording Code track number.
 */
constexpr char kMetaDataTagISRC[] = "ISRC";
/** @} */
/** @} */

/** These are metadata tags specified usage under the Vorbis Comment
 *  metadata format standard (used by .ogg and .flac), but are not supported
 *  on RIFF (.wav) files.
 *  Vorbis Comment supports any metadata tag name, but these ones should be
 *  preferred as they have a standardized usage.
 *  These are not case sensitive.
 *  @{
 */
constexpr char kMetaDataTagLicense[] = "License"; /**< Standard metadata tag. */
constexpr char kMetaDataTagPerformer[] = "Performer"; /**< Standard metadata tag. */
constexpr char kMetaDataTagVersion[] = "Version"; /**< Standard metadata tag. */
constexpr char kMetaDataTagLocation[] = "Location"; /**< Standard metadata tag. */
constexpr char kMetaDataTagContact[] = "Contact"; /**< Standard metadata tag. */
/** @} */

/** These are metadata tags specified as part of the ID3v1 comment format (used
 *  by some .mp3 files).
 *  These are not supported on RIFF (.wav) files.
 *  @{
 */

/** This is a generic comment field in the ID3v1 tag. */
constexpr char kMetaDataTagComment[] = "Comment";

/** Speed or tempo of the music.
 *  This is specified in the ID3v1 extended data tag.
 */
constexpr char kMetaDataTagSpeed[] = "Speed";

/** Start time of the music.
 *  The ID3v1 extended data tag specifies this as "mmm:ss"
 */
constexpr char kMetaDataTagStartTime[] = "StartTime";

/** End time of the music.
 *  The ID3v1 extended data tag specifies this as "mmm:ss"
 */
constexpr char kMetaDataTagEndTime[] = "EndTime";

/** This is part of the ID3v1.2 tag. */
constexpr char kMetaDataTagSubGenre[] = "SubGenre";

/** @} */

/** These are extra metadata tags that are available with the ID3v2 metadata
 *  tag (used by some .mp3 files).
 *  These are not supported on RIFF (.wav) files.
 *  @{
 */

/** Beats per minute. */
constexpr char kMetaDataTagBpm[] = "BPM";

/** Delay between songs in a playlist in milliseconds. */
constexpr char kMetaDataTagPlaylistDelay[] = "PlaylistDelay";

/** The original file name for this file.
 *  This may be used if the file name had to be truncated or otherwise changed.
 */
constexpr char kMetaDataTagFileName[] = "FileName";

constexpr char kMetaDataTagOriginalAlbum[] = "OriginalTitle"; /**< Standard ID3v2 metadata tag. */
constexpr char kMetaDataTagOriginalWriter[] = "OriginalWriter"; /**< Standard ID3v2 metadata tag. */
constexpr char kMetaDataTagOriginalPerformer[] = "OriginalPerformer"; /**< Standard ID3v2 metadata tag. */
constexpr char kMetaDataTagOriginalYear[] = "OriginalYear"; /**< Standard ID3v2 metadata tag. */

constexpr char kMetaDataTagPublisher[] = "Publisher"; /**< Standard ID3v2 metadata tag. */
constexpr char kMetaDataTagRecordingDate[] = "RecordingDate"; /**< Standard ID3v2 metadata tag. */
constexpr char kMetaDataTagInternetRadioStationName[] = "InternetRadioStationName"; /**< Standard ID3v2 metadata tag. */
constexpr char kMetaDataTagInternetRadioStationOwner[] = "InternetRadioStationOwner"; /**< Standard ID3v2 metadata tag.
                                                                                       */
constexpr char kMetaDataTagInternetRadioStationUrl[] = "InternetRadioStationUrl"; /**< Standard ID3v2 metadata tag. */
constexpr char kMetaDataTagPaymentUrl[] = "PaymentUrl"; /**< Standard ID3v2 metadata tag. */

constexpr char kMetaDataTagInternetCommercialInformationUrl[] = "CommercialInformationUrl"; /**< Standard ID3v2 metadata
                                                                                               tag. */
constexpr char kMetaDataTagInternetCopyrightUrl[] = "CopyrightUrl"; /**< Standard ID3v2 metadata tag. */
constexpr char kMetaDataTagWebsite[] = "Website"; /**< Standard ID3v2 metadata tag. */
constexpr char kMetaDataTagInternetArtistWebsite[] = "ArtistWebsite"; /**< Standard ID3v2 metadata tag. */
constexpr char kMetaDataTagAudioSourceWebsite[] = "AudioSourceWebsite"; /**< Standard ID3v2 metadata tag. */
constexpr char kMetaDataTagComposer[] = "Composer"; /**< Standard ID3v2 metadata tag. */
constexpr char kMetaDataTagOwner[] = "Owner"; /**< Standard ID3v2 metadata tag. */
constexpr char kMetaDataTagTermsOfUse[] = "TermsOfUse"; /**< Standard ID3v2 metadata tag. */

/** The musical key that the audio starts with */
constexpr char kMetaDataTagInitialKey[] = "InitialKey";


/** @} */

/** This is a magic value that can be passed to setMetaData() to remove all
 *  tags from the metadata table for that sound.
 */
constexpr const char* const kMetaDataTagClearAllTags = nullptr;


/** used to retrieve the peak volume information for a sound data object.  This contains one
 *  volume level per channel in the stream and the frame in the stream at which the peak
 *  occurs.
 */
struct PeakVolumes
{
    /** the number of channels with valid peak data in the arrays below. */
    size_t channels;

    /** the frame that each peak volume level occurs at for each channel.  This will be the
     *  first frame this peak volume level occurs at if it is reached multiple times in the
     *  stream.
     */
    size_t frame[kMaxChannels];

    /** the peak volume level that is reached for each channel in the stream.  This will be
     *  in the range [0.0, 1.0].  This information can be used to normalize the volume level
     *  for a sound.
     */
    float peak[kMaxChannels];

    /** the frame that the overall peak volume occurs at in the sound. */
    size_t peakFrame;

    /** the peak volume among all channels of data.  This is simply the maximum value found in
     *  the @ref peak table.
     */
    float peakVolume;
};


/** base type for an event point identifier. */
typedef uint32_t EventPointId;

/** an invalid frame offset for an event point.  This value should be set if an event point is
 *  to be removed from a sound data object.
 */
constexpr size_t kEventPointInvalidFrame = ~0ull;

/** This indicates that an event point should loop infinitely.
 */
constexpr size_t kEventPointLoopInfinite = SIZE_MAX;

/** a event point parsed from a data file.  This contains the ID of the event point, its name
 *  label (optional), and the frame in the stream at which it should occur.
 */
struct EventPoint
{
    /** the ID of the event point.  This is used to identify it in the file information but is
     *  not used internally except to match up labels or loop points to the event point.
     */
    EventPointId id;

    /** the frame that the event point occurs at.  This is relative to the start of the stream
     *  for the sound.  When updating event points with setEventPoints(), this can be set
     *  to @ref kEventPointInvalidFrame to indicate that the event point with the ID @ref id
     *  should be removed from the sound data object.  Otherwise, this frame index must be within
     *  the bounds of the sound data object's stream.
     */
    size_t frame;

    /** the user-friendly label given to this event point.  This may be parsed from a different
     *  information chunk in the file and will be matched up later based on the event point ID.
     *  This value is optional and may be nullptr.
     */
    const char* label = nullptr;

    /** optional text associated with this event point.  This may be additional information
     *  related to the event point's position in the stream such as closed captioning text
     *  or a message of some sort.  It is the host app's responsibility to interpret and use
     *  this text appropriately.  This text will always be UTF-8 encoded.
     */
    const char* text = nullptr;

    /** Length of the segment of audio referred to by this event point.
     *  If @ref length is non-zero, then @ref length is the number of frames
     *  after @ref frame that this event point refers to.
     *  If @ref length is zero, then this event point refers to the segment
     *  from @ref frame to the end of the sound.
     *  If @ref loopCount is non-zero, then the region specified will refer to
     *  a looping region.
     *  If @ref playIndex is non-zero, then the region can additionally specify
     *  the length of audio to play.
     */
    size_t length = 0;

    /** Number of times this section of audio in the playlist should be played.
     *  The region of audio to play in a loop is specified by @ref length.
     *  if @ref loopCount is 0, then this is a non-looping segment.
     *  If @ref loopCount is set to @ref kEventPointLoopInfinite, this
     *  specifies that this region should be looped infinitely.
     */
    size_t loopCount = 0;

    /** An optional method to specify an ordering for the event points or a
     *  subset of event points.
     *  A value of 0 indicates that there is no intended ordering for this
     *  event point.
     *  The playlist indexes will always be a contiguous range starting from 1.
     *  If a user attempts to set a non-contiguous range of event point
     *  playlist indexes on a SoundData, the event point system will correct
     *  this and make the range contiguous.
     */
    size_t playIndex = 0;

    /** user data object attached to this event point.  This can have an optional destructor
     *  to clean up the user data object when the event point is removed, the user data object
     *  is replaced with a new one, or the sound data object containing the event point is
     *  destroyed.  Note that when the user data pointer is replaced with a new one, it is the
     *  caller's responsibility to ensure that an appropriate destructor is always paired with
     *  it.
     */
    UserData userData = {};

    /** reserved for future expansion.  This must be set to nullptr. */
    void* ext = nullptr;
};

/** special value for setEventPoints() to indicate that the event point table should be
 *  cleared instead of adding or removing individual event points.
 */
constexpr EventPoint* const kEventPointTableClear = nullptr;

/******************************** Sound Data Management Interface ********************************/
/** interface to manage audio data in general.  This includes loading audio data from multiple
 *  sources (ie: file, memory, raw data, user-decoded, etc), writing audio data to file, streaming
 *  audio data to file, decoding audio data to PCM, and changing its sample format.  All audio
 *  data management should go through this interface.
 *
 *  See these pages for more detail:
 *  @rst
    * :ref:`carbonite-audio-label`
    * :ref:`carbonite-audio-data-label`
    @endrst
 */
struct IAudioData
{
    CARB_PLUGIN_INTERFACE("carb::audio::IAudioData", 1, 0)

    /*************************** Sound Data Creation and Management ******************************/
    /** creates a new sound data object.
     *
     *  @param[in] desc     a descriptor of how and from where the audio data should be loaded.
     *                      This may not be nullptr.
     *  @returns the newly created sound data object if it was successfully loaded or parsed.
     *           When this object is no longer needed, it must be freed with release().
     *  @returns nullptr if the sound data could not be successfully loaded.
     *
     *  @remarks This creates a new sound data object from a requested data source.  This single
     *           creation point manages the loading of all types of sound data from all sources.
     *           Depending on the flags used, the loaded sound data may or may not be decoded
     *           into PCM data.
     */
    SoundData*(CARB_ABI* createData)(const SoundDataLoadDesc* desc);

    /** acquires a new reference to a sound data object.
     *
     *  @param[in] sound    the sound data object to take a reference to.  This may not be
     *                      nullptr.
     *  @returns the sound data object with an additional reference taken on it.  This new
     *           reference must later be released with release().
     *
     *  @remarks This grabs a new reference to a sound data object.  Each reference that is
     *           taken must be released at some point when it is no longer needed.  Note that
     *           the createData() function returns the new sound data object with a single
     *           reference on it.  This final reference must also be released at some point to
     *           destroy the object.
     */
    SoundData*(CARB_ABI* acquire)(SoundData* sound);

    /** releases a reference to a sound data object.
     *
     *  @param[in] sound    the sound data object to release a reference on.  This may not be
     *                      nullptr.
     *  @returns the new reference count for the sound data object.
     *  @returns 0 if the sound data object was destroyed (ie: all references were released).
     *
     *  @remarks This releases a single reference to sound data object.  If all references have
     *           been released, the object will be destroyed.  Each call to grab a new reference
     *           with acquire() must be balanced by a call to release that reference.  The
     *           object's final reference that came from createData() must also be released
     *           in order to destroy it.
     */
    size_t(CARB_ABI* release)(SoundData* sound);


    /*************************** Sound Data Information Accessors ********************************/
    /** retrieves the creation time flags for a sound data object.
     *
     *  @param[in] sound    the sound data object to retrieve the creation time flags for.
     *  @returns the flags that were used when creating the sound object.  Note that if the sound
     *           data object was duplicated through a conversion operation, the data format flags
     *           may no longer be accurate.
     *  @returns 0 if @p sound is nullptr.
     */
    DataFlags(CARB_ABI* getFlags)(const SoundData* sound);

    /** retrieves the name of the file this object was loaded from (if any).
     *
     *  @param[in] sound    the sound data object to retrieve the filename for.
     *  @returns the original filename if the object was loaded from a file.
     *  @returns nullptr if the object does not have a name.
     *  @returns nullptr @p sound is nullptr.
     */
    const char*(CARB_ABI* getName)(const SoundData* sound);

    /** retrieves the length of a sound data object's buffer.
     *
     *  @param[in] sound    the sound to retrieve the buffer length for.  This may not be nullptr.
     *  @param[in] units    the units to retrieve the buffer length in.  Note that if the buffer
     *                      length in milliseconds is requested, the length may not be precise.
     *  @returns the length of the sound data object's buffer in the requested units.
     *
     *  @remarks This retrieves the length of a sound data object's buffer in the requested units.
     *           The length of the buffer represents the total amount of audio data that is
     *           represented by the object.  Note that if this object was created to stream data
     *           from file or the data is stored still encoded or compressed, this will not
     *           reflect the amount of memory actually used by the object.  Only non-streaming
     *           PCM formats will be able to convert their length into an amount of memory used.
     */
    size_t(CARB_ABI* getLength)(const SoundData* sound, UnitType units);

    /** sets the current 'valid' size of an empty buffer.
     *
     *  @param[in] sound    the sound data object to set the new valid length for.  This may
     *                      not be nullptr.
     *  @param[in] length   the new length of valid data in the units specified by @p units.
     *                      This valid data length may not be specified in time units (ie:
     *                      @ref UnitType::eMilliseconds or @ref UnitType::eMicroseconds)
     *                      since it would not be an exact amount and would be likely to
     *                      corrupt the end of the stream.  This length must be less than or
                            equal to the creation time length of the buffer.
     *  @param[in] units    the units to interpret @p length in.  This must be in frames or
     *                      bytes.
     *  @returns true if the new valid data length is successfully updated.
     *  @returns false if the new length value was out of range of the buffer size or the
     *           sound data object was not created as empty.
     *
     *  @remarks This sets the current amount of data in the sound data object buffer that is
     *           considered 'valid' by the caller.  This should only be used on sound data objects
     *           that were created with the @ref fDataFlagEmpty flag.  If the host app decides
     *           to write data to the empty buffer, it must also set the amount of valid data
     *           in the buffer before that new data can be decoded successfully.  When the
     *           object's encoded format is not a PCM format, it is the caller's responsibility
     *           to set both the valid byte and valid frame count since that may not be able
     *           to be calculated without creating a decoder state for the sound.  When the
     *           object's encoded format is a PCM format, both the frames and byte counts
     *           will be updated in a single call regardless of which one is specified.
     */
    bool(CARB_ABI* setValidLength)(SoundData* sound, size_t length, UnitType units);

    /** retrieves the current 'valid' size of an empty buffer.
     *
     *  @param[in] sound    the sound data object to retrieve the valid data length for.  This
     *                      may not be nullptr.
     *  @param[in] units    the units to retrieve the current valid data length in.  Note that
     *                      if a time unit is requested, the returned length may not be accurate.
     *  @returns the valid data length for the object in the specified units.
     *  @returns 0 if the buffer does not have any valid data.
     *
     *  @remarks This retrieves the current valid data length for a sound data object.  For sound
     *           data objects that were created without the @ref fDataFlagEmpty flag, this will be
     *           the same as the value returned from getLength().  For an object that was created
     *           as empty, this will be the length that was last on the object with a call to
     *           setValidLength().
     */
    size_t(CARB_ABI* getValidLength)(const SoundData* sound, UnitType units);

    /** retrieves the data buffer for a sound data object.
     *
     *  @param[in] sound    the sound to retrieve the data buffer for.  This may not be nullptr.
     *  @returns the data buffer for the sound data object.
     *  @returns nullptr if @p sound does not have a writable buffer.
     *           This can occur for sounds created with @ref fDataFlagUserMemory.
     *           In that case, the caller either already has the buffer address
     *           (ie: shared the memory block to save on memory or memory copy
     *           operations), or the memory exists in a location that should
     *           not be modified (ie: a sound bank or sound atlas).
     *  @returns nullptr if the @p sound object is invalid.
     *  @returns nullptr if @p sound is streaming from disk, since a sound
     *          streaming from disk will not have a buffer.
     *
     *  @remarks This retrieves the data buffer for a sound data object.
     *           This is intended for cases such as empty sounds where data
     *           needs to be written into the buffer of @p sound.
     *           getReadBuffer() should be used for cases where writing to the
     *           buffer is not necessary, since not all sound will have a
     *           writable buffer.
     *           In-memory streaming sounds without @ref fDataFlagUserMemory
     *           will return a buffer here; that buffer contains the full
     *           in-memory file, so writing to it will most likely corrupt the
     *           sound.
     */
    void*(CARB_ABI* getBuffer)(const SoundData* sound);

    /** Retrieves the read-only data buffer for a sound data object.
     *
     *  @param[in] sound    the sound to retrieve the data buffer for.  This may not be nullptr.
     *  @returns the data buffer for the sound data object.
     *  @returns nullptr if the @p sound object is invalid.
     *  @returns nullptr if @p sound is streaming from disk, since a sound
     *          streaming from disk will not have a buffer.
     *
     *  @remarks This retrieves the data buffer for a sound data object.
     *           Any decoded @ref SoundData will return a buffer of raw PCM
     *           data that can be directly played.
     *           getValidLength() should be used to determine the length of a
     *           decoded buffer.
     *           Any in-memory streaming @ref SoundData will also return the
     *           raw file blob; this needs to be decoded before it can be
     *           played.
     */
    const void*(CARB_ABI* getReadBuffer)(const SoundData* sound);

    /** retrieves the amount of memory used by a sound data object.
     *
     *  @param[in] sound    the sound data object to retrieve the memory usage for.
     *  @returns the total number of bytes used to store the sound data object.
     *  @returns 0 if @p sound is nullptr.
     *
     *  @remarks This retrieves the amount of memory used by a single sound data object.  This
     *           will include all memory required to store the audio data itself, to store the
     *           object and all its parameters, and the original filename (if any).  This
     *           information is useful for profiling purposes to investigate how much memory
     *           the audio system is using for a particular scene.
     */
    size_t(CARB_ABI* getMemoryUsed)(const SoundData* sound);

    /** Retrieves the format information for a sound data object.
     *
     *  @param[in] sound    The sound data object to retrieve the format information for.  This
     *                      may not be nullptr.
     *  @param[in] type     The type of format information to retrieve.
     *                      For sounds that were decoded on load, this
     *                      parameter doesn't have any effect, so it can be
     *                      set to either value.
     *                      For streaming sounds, @ref CodecPart::eDecoder will
     *                      cause the returned format to be the format that the
     *                      audio will be decoded into (e.g. when decoding Vorbis
     *                      to float PCM, this will return @ref SampleFormat::ePcmFloat).
     *                      For streaming sounds, @ref CodecPart::eEncoder will
     *                      cause the returned format to be the format that the
     *                      audio is being decoded from (e.g. when decoding Vorbis
     *                      to float PCM, this will return @ref SampleFormat::eVorbis).
     *                      In short, when you are working with decoded audio data,
     *                      you should be using @ref CodecPart::eDecoder; when you
     *                      are displaying audio file properties to a user, you
     *                      should be using @ref CodecPart::eEncoder.
     *  @param[out] format  Receives the format information for the sound data object.  This
     *                      format information will remain constant for the lifetime of the
     *                      sound data object.
     *  @returns No return value.
     *
     *  @remarks This retrieves the format information for a sound data object.  The format
     *           information will remain constant for the object's lifetime so it can be
     *           safely cached once retrieved.  Note that the encoded format information may
     *           not be sufficient to do all calculations on sound data of non-PCM formats.
     */
    void(CARB_ABI* getFormat)(const SoundData* sound, CodecPart type, SoundFormat* format);

    /** retrieves or calculates the peak volume levels for a sound if possible.
     *
     *  @param[in] sound    the sound data object to retrieve the peak information for.  This may
     *                      not be nullptr.
     *  @param[out] peaks   receives the peak volume information for the sound data object
     *                      @p sound.  Note that only the entries corresponding to the number
     *                      of channels in the sound data object will be written.  The contents
     *                      of the remaining channels is undefined.
     *  @returns true if the peak volume levels are available or could be calculated.
     *  @returns false if the peak volume levels were not calculated or loaded when the
     *           sound was created.
     *
     *  @remarks This retrieves the peak volume level information for a sound.  This information
     *           is either loaded from the sound's original source file or is calculated if
     *           the sound is decoded into memory at load time.  This information will not be
     *           calculated if the sound is streamed from disk or memory.
     */
    bool(CARB_ABI* getPeakLevel)(const SoundData* sound, PeakVolumes* peaks);

    /** retrieves embedded event point information from a sound data object.
     *
     *  @param[in] sound        the sound data object to retrieve the event point information
     *                          from.  This may not be nullptr.
     *  @param[out] events      receives the event point information.  This may be nullptr if
     *                          only the number of event points is required.
     *  @param[in] maxEvents    the maximum number of event points that will fit in the @p events
     *                          buffer.  This must be 0 if @p events is nullptr.
     *  @returns the number of event points written to the buffer @p events if it was not nullptr.
     *  @returns if the buffer is not large enough to store all the event points, the maximum
     *           number that will fit is written to the buffer and the total number of event
     *           points is returned.  This case can be detected by checking if the return value
     *           is larger than @p maxEvents.
     *  @returns the number of event points contained in the sound object if the buffer is
     *           nullptr.
     *
     *  @remarks This retrieves event point information that was embedded in the sound file that
     *           was used to create a sound data object.  The event points are optional in the
     *           data file and may not be present.  If they are parsed from the file, they will
     *           also be saved out to any destination file that the same sound data object is
     *           written to, provided the destination format supports embedded event point
     *           information.
     */
    size_t(CARB_ABI* getEventPoints)(const SoundData* sound, EventPoint* events, size_t maxEvents);

    /** retrieves a single event point object by its identifier.
     *
     *  @param[in] sound    the sound data object to retrieve the named event point from.  This
     *                      may not be nullptr.
     *  @param[in] id       the identifier of the event point to be retrieved.
     *  @returns the information for the event point with the requested identifier if found.
     *           The returned object is only valid until the event point list for the sound is
     *           modified.  This should not be stored for extended periods since its contents
     *           may be invalidated at any time.
     *  @returns nullptr if no event point with the requested identifier is found.
     *
     *  @note Access to this event point information is not thread safe.  It is the caller's
     *        responsibility to ensure access to the event points on a sound data object is
     *        appropriately locked.
     */
    const EventPoint*(CARB_ABI* getEventPointById)(const SoundData* sound, EventPointId id);

    /** retrieves a single event point object by its index.
     *
     *  @param[in] sound    the sound data object to retrieve the event point from.  This may not
     *                      be nullptr.
     *  @param[in] index    the zero based index of the event point to retrieve.
     *  @returns the information for the event point at the requested index.  The returned object
     *           is only valid until the event point list for the sound is modified.  This should
     *           not be stored for extended periods since its contents may be invalidated at any
     *           time.
     *  @returns nullptr if the requested index is out of range of the number of event points in
     *           the sound.
     *
     *  @note Access to this event point information is not thread safe.  It is the caller's
     *        responsibility to ensure access to the event points on a sound data object is
     *        appropriately locked.
     */
    const EventPoint*(CARB_ABI* getEventPointByIndex)(const SoundData* sound, size_t index);

    /** retrieves a single event point object by its playlist index.
     *
     *  @param[in] sound        The sound data object to retrieve the event
     *                          point from.  This may not be nullptr.
     *  @param[in] playIndex    The playlist index of the event point to retrieve.
     *                          Playlist indexes may range from 1 to SIZE_MAX.
     *                          0 is not a valid playlist index.
     *                          This function is intended to be called in a loop
     *                          with values of @p playIndex between 1 and the
     *                          return value of getEventPointMaxPlayIndex().
     *                          The range of valid event points will always be
     *                          contiguous, so nullptr should not be returned
     *                          within this range.
     *
     *  @returns the information for the event point at the requested playlist
     *           index.  The returned object is only valid until the event
     *           point list for the sound is modified.  This should not be
     *           stored for extended periods since its contents may be
     *           invalidated at any time.
     *  @returns nullptr if @p playIndex is 0.
     *  @returns nullptr if no event point has a playlist index of @p playIndex.
     *
     *  @note Access to this event point information is not thread safe.  It is
     *        the caller's responsibility to ensure access to the event points
     *        on a sound data object is appropriately locked.
     */
    const EventPoint*(CARB_ABI* getEventPointByPlayIndex)(const SoundData* sound, size_t playIndex);

    /** Retrieve the maximum play index value for the sound.
     *
     *  @param[in] sound The sound data object to retrieve the event
     *                   point index from.  This may not be nullptr.
     *
     *  @returns This returns the max play index for this sound.
     *           This will be 0 if no event points have a play index.
     *           This is also the number of event points with playlist indexes,
     *           since the playlist index range is contiguous.
     */
    size_t(CARB_ABI* getEventPointMaxPlayIndex)(const SoundData* sound);

    /** modifies, adds, or removes event points in a sound data object.
     *
     *  @param[inout] sound     the sound data object to update the event point(s) in.  This may
     *                          not be nullptr.
     *  @param[in] eventPoints  the event point(s) to be modified or added.  The operation that is
     *                          performed for each event point in the table depends on whether
     *                          an event point with the same ID already exists in the sound data
     *                          object.  The event points in this table do not need to be sorted
     *                          in any order.  This may be @ref kEventPointTableClear to indicate
     *                          that all event points should be removed.
     *  @param[in] count        the total number of event points in the @p eventPoint table.  This
     *                          must be 0 if @p eventPoints is nullptr.
     *  @returns true if all of the event points in the table are updated successfully.
     *  @returns false if not all event points could be updated.  This includes a failure to
     *           allocate memory or an event point with an invalid frame offset.  Note that this
     *           failing doesn't mean that all the event points failed.  This just means that at
     *           least failed to be set properly.  The new set of event points may be retrieved
     *           and compared to the list set here to determine which one failed to be updated.
     *
     *  @remarks This modifies, adds, or removes one or more event points in a sound data object.
     *           An event point will be modified if one with the same ID already exists.  A new
     *           event point will be added if it has an ID that is not already present in the
     *           sound data object and its frame offset is valid.  An event point will be removed
     *           if it has an ID that is present in the sound data object but the frame offset for
     *           it is set to @ref kEventPointInvalidFrame.  Any other event points with invalid
     *           frame offsets (ie: out of the bounds of the stream) will be skipped and cause the
     *           function to fail.
     *
     *  @note When adding a new event point or changing a string in an event point, the strings
     *        will always be copied internally instead of referencing the caller's original
     *        buffer.  The caller can therefore clean up its string buffers immediately upon
     *        return.  The user data object (if any) however must persist since it will be
     *        referenced instead of copied.  If the user data object needs to be cleaned up,
     *        an appropriate destructor function for it must also be provided.
     *
     *  @note If an event point is modified or removed such that the playlist
     *        indexes of the event points are no longer contiguous, this function
     *        will adjust the play indexes of all event points to prevent any
     *        gaps.
     *
     *  @note The playIndex fields on @p eventPoints must be within the region
     *        of [0, @p count + getEventPoints(@p sound, nullptr, 0)].
     *        Trying to set playlist indexes outside this range is an error.
     */
    bool(CARB_ABI* setEventPoints)(SoundData* sound, const EventPoint* eventPoints, size_t count);

    /** retrieves the maximum simultaneously playing instance count for a sound.
     *
     *  @param[in] sound    the sound to retrieve the maximum instance count for.  This may not
     *                      be nullptr.
     *  @returns the maximum instance count for the sound if it is limited.
     *  @retval kInstancesUnlimited if the instance count is unlimited.
     *
     *  @remarks This retrieves the current maximum instance count for a sound.  This limit is
     *           used to prevent too many instances of a sound from being played simultaneously.
     *           With the limit set to unlimited, playing too many instances can result in serious
     *           performance penalties and serious clipping artifacts caused by too much
     *           constructive interference.
     */
    uint32_t(CARB_ABI* getMaxInstances)(const SoundData* sound);

    /** sets the maximum simultaneously playing instance count for a sound.
     *
     *  @param[in] sound    the sound to change the maximum instance count for.  This may not be
     *                      nullptr.
     *  @param[in] limit    the new maximum instance limit for the sound.  This may be
     *                      @ref kInstancesUnlimited to remove the limit entirely.
     *  @returns no return value.
     *
     *  @remarks This sets the new maximum playing instance count for a sound.  This limit will
     *           prevent the sound from being played until another instance of it finishes playing
     *           or simply cause the play request to be ignored completely.  This should be used
     *           to limit the use of frequently played sounds so that they do not cause too much
     *           of a processing burden in a scene or cause too much constructive interference
     *           that could lead to clipping artifacts.  This is especially useful for short
     *           sounds that are played often (ie: gun shots, foot steps, etc).  At some [small]
     *           number of instances, most users will not be able to tell if a new copy of the
     *           sound played or not.
     */
    void(CARB_ABI* setMaxInstances)(SoundData* sound, uint32_t limit);

    /** retrieves the user data pointer for a sound data object.
     *
     *  @param[in] sound    the sound data object to retrieve the user data pointer for.  This may
     *                      not be nullptr.
     *  @returns the stored user data pointer.
     *  @returns nullptr if no user data has been set on the requested sound.
     *
     *  @remarks This retrieves the user data pointer for the requested sound data object.  This
     *           is used to associate any arbitrary data with a sound data object.  It is the
     *           caller's responsibility to ensure access to data is done in a thread safe
     *           manner.
     */
    void*(CARB_ABI* getUserData)(const SoundData* sound);

    /** sets the user data pointer for a sound data object.
     *
     *  @param[in] sound    the sound data object to set the user data pointer for.  This may
     *                      not be nullptr.
     *  @param[in] userData the new user data pointer to set.  This may include an optional
     *                      destructor if the user data object needs to be cleaned up.  This
     *                      may be nullptr to indicate that the user data pointer should be
     *                      cleared out.
     *  @returns no return value.
     *
     *  @remarks This sets the user data pointer for this sound data object.  This is used to
     *           associate any arbitrary data with a sound data object.  It is the caller's
     *           responsibility to ensure access to this table is done in a thread safe manner.
     *
     *  @note The user data object must not hold a reference to the sound data object that it is
     *        attached to.  Doing so will cause a cyclical reference and prevent the sound data
     *        object itself from being destroyed.
     *
     *  @note The sound data object that this user data object is attached to must not be accessed
     *        from the destructor.  If the sound data object is being destroyed when the user data
     *        object's destructor is being called, its contents will be undefined.
     */
    void(CARB_ABI* setUserData)(SoundData* sound, const UserData* userData);

    /************************************ Sound Data Codec ***************************************/
    /** retrieves information about a supported codec.
     *
     *  @param[in] encodedFormat    the encoded format to retrieve the codec information for.
     *                              This may not be @ref SampleFormat::eDefault or
     *                              @ref SampleFormat::eRaw.  This is the format that the codec
     *                              either decodes from or encodes to.
     *  @param[in] pcmFormat        the PCM format for the codec that the information would be
     *                              retrieved for.  This may be @ref SampleFormat::eDefault to
     *                              retrieve the information for the codec for the requested
     *                              encoded format that decodes to the preferred PCM format.
     *                              This may not be @ref SampleFormat::eRaw.
     *  @returns the info block for the codec that can handle the requested operation if found.
     *  @returns nullptr if no matching codec for @p encodedFormat and @p pcmFormat could be
     *           found.
     *
     *  @remarks This retrieves the information about a single codec.  This can be used to check
     *           if an encoding or decoding operation to or from a requested format pair is
     *           possible and to retrieve some information suitable for display or UI use for the
     *           format.
     */
    const CodecInfo*(CARB_ABI* getCodecFormatInfo)(SampleFormat encodedFormat, SampleFormat pcmFormat);

    /** creates a new decoder or encoder state for a sound data object.
     *
     *  @param[in] desc     a descriptor of the decoding or encoding operation that will be
     *                      performed.  This may not be nullptr.
     *  @returns the new state object if the operation is valid and the state was successfully
     *           initialized.  This must be destroyed with destroyCodecState() when it is
     *           no longer needed.
     *  @returns nullptr if the operation is not valid or the state could not be created or
     *           initialized.
     *
     *  @remarks This creates a new decoder or encoder state instance for a sound object.  This
     *           will encapsulate all the information needed to perform the operation on the
     *           stream as efficiently as possible.  Note that the output format of the decoder
     *           will always be a PCM variant (ie: one of the SampleFormat::ePcm* formats).
     *           Similarly, the input of the encoder will always be a PCM variant.  The input
     *           of the decoder and output of the encoder may be any format.
     *
     *  @remarks The decoder will treat the sound data object as a stream and will decode it
     *           in chunks from start to end.  The decoder's read cursor will initially be placed
     *           at the start of the stream.  The current read cursor can be changed at any time
     *           by calling setCodecPosition().  Some compressed or block based formats may
     *           adjust the new requested position to the start of the nearest block.
     *
     *  @remarks The state is separated from the sound data object so that multiple playing
     *           instances of each sound data object may be decoded and played simultaneously
     *           regardless of the sample format or decoder used.  Similarly, when encoding
     *           this prevents any limitation on the number of targets a sound could be streamed
     *           or written to.
     *
     *  @remarks The encoder state is used to manage the encoding of a single stream of data to
     *           a single target.  An encoder will always be able to be created for an operation
     *           where the source and destination formats match.  For formats that do not support
     *           encoding, this will fail.  More info about each encoder format can be queried
     *           with getCodecFormatInfo().
     *
     *  @remarks The stream being encoded is expected to have the same number of channels as the
     *           chosen output target.
     */
    CodecState*(CARB_ABI* createCodecState)(const CodecStateDesc* desc);

    /** destroys a codec state object.
     *
     *  @param[in] state    the codec state to destroy.  This call will be ignored if this is
     *                      nullptr.
     *  @returns no return value.
     *
     *  @remarks This destroys a decoder or encoder state object that was previously returned
     *           from createCodecState().  For a decoder state, any partially decoded data stored
     *           in the state object will be lost.  For an encoder state, all pending data will
     *           be written to the output target (padded with silence if needed).  If the encoder
     *           was targeting an output stream, the stream will not be closed.  If the encoder
     *           was targeting a sound data object, the stream size information will be updated.
     *           The buffer will not be trimmed in size if it is longer than the actual stream.
     */
    void(CARB_ABI* destroyCodecState)(CodecState* decodeState);

    /** decodes a number of frames of data into a PCM format.
     *
     *  @param[in] decodeState      the decoder state to use for the decoding operation.  This may
     *                              not be nullptr.
     *  @param[out] buffer          receives the decoded PCM data.  This buffer must be at least
     *                              large enough to hold @p framesToDecode frames of data in the
     *                              sound data object's stream.  This may not be nullptr.
     *  @param[in] framesToDecode   the requested number of frames to decode.  This is taken as a
     *                              suggestion.  Up to this many frames will be decoded if it is
     *                              available in the stream.  If the stream ends before this
     *                              number of frames is read, the remainder of the buffer will be
     *                              left unmodified.
     *  @param[out] framesDecoded   receives the number of frames that were actually decoded into
     *                              the output buffer.  This will never be larger than the
     *                              @p framesToDecode value.  This may not be nullptr.
     *  @returns @p buffer if the decode operation is successful and the decoded data was copied
     *           into the output buffer.
     *  @returns a non-nullptr value if the sound data object already contains PCM data in the
     *           requested decoded format.
     *  @returns nullptr if the decode operation failed for any reason.
     *  @returns nullptr if @p framesToDecode is 0.
     *
     *  @remarks This decodes a requested number of frames of data into an output buffer.  The
     *           data will always be decoded into a PCM format specified by the decoder when the
     *           sound data object is first created.  If the sound data object already contains
     *           PCM data in the requested format, nothing will be written to the destination
     *           buffer, but a pointer into the data buffer itself will be returned instead.
     *           The returned pointer must always be used instead of assuming that the decoded
     *           data was written to the output buffer.  Similarly, the @p framesToDecode count
     *           must be used instead of assuming that exactly the requested number of frames
     *           were successfully decoded.
     */
    const void*(CARB_ABI* decodeData)(CodecState* decodeState, void* buffer, size_t framesToDecode, size_t* framesDecoded);

    /** retrieves the amount of data available to decode in a sound data object.
     *
     *  @param[in] decodeState  the decode state to retrieve the amount of data that is available
     *                          from the current read cursor position to the end of the stream.
     *                          This may not be nullptr.
     *  @param[in] units        the units to retrieve the available data count in.  Note that if
     *                          time units are requested (ie: milliseconds), the returned value
     *                          will only be an estimate of the available data.
     *  @returns the amount of available data in the requested units.
     *  @returns 0 if no data is available or it could not be calculated.
     *
     *  @remarks This retrieves the amount of data left to decode from the current read cursor
     *           to the end of the stream.  Some formats may not be able to calculate the amount
     *           of available data.
     */
    size_t(CARB_ABI* getDecodeAvailable)(const CodecState* decodeState, UnitType units);

    /** retrieves the current cursor position for a codec state.
     *
     *  @param[in] state    the codec state to retrieve the current read cursor position for.
     *                      This may not be nullptr.
     *  @param[in] units    the units to retrieve the current read cursor position in.  Note that
     *                      if time units are requested (ie: milliseconds), the returned value
     *                      will only be an estimate of the current position since time units are
     *                      not accurate.
     *                      @ref UnitType::eBytes is invalid if the codec being used specifies
     *                      @ref fCodecCapsCompressed.
     *  @returns the current cursor position in the requested units.  For a decoder state, this is
     *           the location in the sound's data where the next decoding operation will start
     *           from.  For an encoder state, this is effectively the amount of data that has been
     *           successfully encoded and written to the target.
     *  @returns 0 if the cursor is at the start of the buffer or output target.
     *  @returns 0 if the decode position could not be calculated.
     *  @returns 0 if no data has been successfully written to an output target.
     *
     *  @remarks This retrieves the current cursor position for a codec state.  Some formats may
     *           not be able to calculate an accurate cursor position and may end up aligning it
     *           to the nearest block boundary instead.
     *
     *  @note Even though the write cursor for an encoder state can be retrieved, setting it is
     *        not possible since that would cause a discontinuity in the stream and corrupt it.
     *        If the stream position needs to be rewound to the beginning, the encoder state
     *        should be recreated and the stream started again on a new output target.
     */
    size_t(CARB_ABI* getCodecPosition)(const CodecState* decodeState, UnitType units);

    /** sets the new decoder position.
     *
     *  @param[in] decodeState  the decoder state to set the position for.  This may not be
     *                          nullptr.  This must be a decoder state.
     *  @param[in] newPosition  the new offset into the sound data object's buffer to set the
     *                          read cursor to.  The units of this offset depend on the value
     *                          in @p units.
     *  @param[in] units        the units to interpret the @p newPosition offset in.  Note that
     *                          if time units are requested (ie: milliseconds), the new position
     *                          may not be accurate in the buffer.  The only offset that can be
     *                          guaranteed accurate in time units is 0.
     *  @returns true if the new decoding read cursor position was successfully set.
     *  @returns false if the new position could not be set or an encoder state was used.
     *
     *  @remarks This attempts to set the decoder's read cursor position to a new offset in the
     *           sound buffer.  The new position may not be accurately set depending on the
     *           capabilities of the codec.  The position may be aligned to the nearest block
     *           boundary for sound codecs and may fail for others.
     */
    bool(CARB_ABI* setCodecPosition)(CodecState* decodeState, size_t newPosition, UnitType units);

    /** calculates the maximum amount of data that a codec produce for a given input size.
     *
     *  @param[in] state        the codec state to estimate the buffer size for.  This may not
     *                          be nullptr.  This may be either an encoder or decoder state.
     *  @param[in] inputSize    for a decoder state, this is the number of bytes of input to
     *                          estimate the output frame count for.  For an encoder state, this
     *                          is the number of frames of data that will be submitted to the
     *                          encoder during the encoding operation.
     *  @returns an upper limit on the number of frames that can be decoded from the given input
     *           buffer size for decoder states.
     *  @returns an upper limit on the size of the output buffer in bytes that will be needed to
     *           hold the output for an encoder state.
     *  @returns 0 if the frame count could not be calculated or the requested size was 0.
     *
     *  @remarks This calculates the maximum buffer size that would be needed to hold the output
     *           of the codec operation specified by the state object.  This can be used to
     *           allocate or prepare a destination that is large enough to receive the operation's
     *           full result.  Note that the units of both the inputs and outputs are different
     *           depending on the type of codec state that is used.  This is necessary because the
     *           size of an encoded buffer in frames cannot always be calculated for a given byte
     *           size and vice versa.  Some sample formats only allow for an upper limit to be
     *           calculated for such cases.
     *
     *  @remarks For a decoder state, this calculates the maximum number of frames of PCM data
     *           that could be produced given a number of input bytes in the decoder state's
     *           output format.  This is used to be able to allocate a decoding buffer that is
     *           large enough to hold the results for a given input request.
     *
     *  @remarks For an encoder state, this calculates an estimate of the buffer size needed in
     *           order to store the encoder output for a number of input frames.  For PCM formats,
     *           the returned size will be exact.  For compressed formats, the returned size will
     *           be an upper limit on the size of the output stream.  Note that this value is not
     *           always fully predictable ahead of time for all formats since some depend on
     *           the actual content of the stream to adapt their compression (ie: variable
     *           bit rate formats, frequency domain compression, etc).
     */
    size_t(CARB_ABI* getCodecDataSizeEstimate)(const CodecState* decodeState, size_t inputBytes);

    /** encodes a simple buffer of data into the output target for the operation.
     *
     *  @param[in] encodeState      the encoder state object that is managing the stream encoding
     *                              operation.  This may not be nullptr.
     *  @param[in] buffer           the buffer of data to be encoded.  This is expected to be in
     *                              the input data format specified when the encoder state object
     *                              was created.  This may not be nullptr.
     *  @param[in] lengthInFrames   the size of the input buffer in frames.
     *  @returns the number of bytes that were successfully encoded and written to the
     *           output target.
     *  @returns 0 if the buffer could not be encoded or the output target has become full or
     *           or fails to write (ie: the sound data object is full and is not allowed or able
     *           to expand, or writing to the output stream failed).
     *
     *  @remarks This encodes a single buffer of data into an output target.  The buffer is
     *           expected to be in the input sample format for the encoder.  The buffer is also
     *           expected to be the logical continuation of any previous buffers in the stream.
     */
    size_t(CARB_ABI* encodeData)(CodecState* encodeState, const void* buffer, size_t lengthInFrames);

    /***************************** Sound Data Metadata Information ********************************/
    /** Retrieve the names of the metadata tags in a SoundData.
     *
     *  @param[in]  sound   The sound to retrieve the metadata tag names from.
     *  @param[in]  index   The index of the metadata tag in the sound object.
     *                      To enumerate all tags in @p sound, one should call
     *                      this with @p index == 0, then increment until nullptr
     *                      is returned from this function. Note that adding or
     *                      removing tags may alter the ordering of this table,
     *                      but changing the value of a tag will not.
     *  @param[out] value   If this is non-null and a metadata tag exists at
     *                      index @p index, the contents of the metadata tag
     *                      under the returned name is assigned to @p value.
     *                      If this is non-null and no metadata tag exists at
     *                      index @p index, @p value is assigned to nullptr.
     *                      This string is valid until \p sound is destroyed or
     *                      this entry in the metadata table is changed or
     *                      removed.
     *
     *  @returns This returns a null terminated string for the tag name, if a
     *           tag at index @p index exists.
     *           The returned string is valid until \p sound is destroyed or
     *           this entry in the metadata table is changed or removed.
     *  @returns This returns nullptr if no tag at @p index exists.
     *
     *  @remarks This function allows the metadata of a @ref SoundData object
     *           to be enumerated. This function can be called with incrementing
     *           indices, starting from 0, to retrieve all of the metadata tag
     *           names. @p value can be used to retrieve the contents of each
     *           metadata tag, if the contents of each tag is needed.
     *
     *  @note If setMetaData() is called, the order of the tags is not
     *        guaranteed to remain the same.
     */
    const char*(CARB_ABI* getMetaDataTagName)(const SoundData* sound, size_t index, const char** value);

    /** Retrieve a metadata tag from a SoundData.
     *
     *  @param[in] sound    The sound to retrieve the metadata tag from.
     *  @param[in] tagName  The name of the metadata tag to retrieve.
     *                      For example "artist" may retrieve the name of the
     *                      artist who created the SoundData object's contents.
     *                      This may not be nullptr.
     *
     *  @returns This returns a null terminated string if a metadata tag under
     *           the name @p tagName exited in @p sound.
     *           The returned string is valid until \p sound is destroyed or
     *           this entry in the metadata table is changed or removed.
     *  @returns This returns nullptr if no tag under the name @p tagName was
     *           found in @p sound.
     */
    const char*(CARB_ABI* getMetaData)(const SoundData* sound, const char* tagName);

    /** Set a metadata tag on a SoundData.
     *
     *  @param[in] sound     The sound to retrieve the metadata tag from.
     *  @param[in] tagName   The name of the metadata tag to set.
     *                       For example, one may set a tag with @p tagName
     *                       "artist" to specify the creator of the SoundData's
     *                       contents. This can be set to @ref kMetaDataTagClearAllTags
     *                       to remove all metadata tags on the sound object.
     *  @param[in] tagValue  A null terminated string to set as the value for
     *                       @p tagName. This can be set to nullptr to remove
     *                       the tag under @p tagName from the object.
     *
     *  @returns This returns true if the tags was successfully added or changed.
     *  @returns This returns false if @p tagValue is nullptr and no tag was
     *           found under the name @p tagName.
     *  @returns This returns false if an error occurred which prevented the
     *           tag from being set.
     *
     *  @note @p tagName and @p tagValue are copied internally, so it is safe
     *        to immediately deallocate them after calling this.
     *  @note Metadata tag names are not case sensitive.
     *  @note It is not guaranteed that a given file type will be able to store
     *        arbitrary key-value pairs. RIFF files (.wav), for example, store
     *        metadata tags under 4 character codes, so only metadata tags
     *        that are known to this plugin, such as @ref kMetaDataTagArtist
     *        or tags that are 4 characters in length can be stored. Note this
     *        means that storing 4 character tags beginning with 'I' runs the
     *        risk of colliding with the known tag names (e.g. 'IART' will
     *        collide with @ref kMetaDataTagArtist when writing a RIFF file).
     *  @note @p tagName must not contain the character '=' when the output format
     *        encodes its metadata in the Vorbis Comment format
     *        (@ref SampleFormat::eVorbis and @ref SampleFormat::eFlac do this).
     *        '=' will be replaced with '_' when encoding these formats to avoid
     *        the metadata being encoded incorrectly.
     *        Additionally, the Vorbis Comment standard states that tag names
     *        must only contain characters from 0x20 to 0x7D (excluding '=')
     *        when encoding these formats.
     */
    bool(CARB_ABI* setMetaData)(SoundData* sound, const char* tagName, const char* tagValue);
};

} // namespace audio
} // namespace carb

#ifndef DOXYGEN_SHOULD_SKIP_THIS
CARB_ASSET(carb::audio::SoundData, 0, 1);
#endif
