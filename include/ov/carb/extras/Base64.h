// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Provides a Base64 encoding and decoding helper class.
 */
#pragma once

#include <stdint.h>
#include <string.h>

/** Namespace for all low level Carbonite functionality. */
namespace carb
{
/** Common namespace for extra helper functions and classes. */
namespace extras
{

/** Encoder and decoder helper for Base64 data.  This allows for processing Base64 data in multiple
 *  different standard and non-standard variants of the algorithm, plus custom encodings.  Padding
 *  bytes are always optional but will be generated by default by all non-custom variants.  A
 *  custom variant without padding bytes can be specified by setting the padding byte to 0.  No
 *  data verification is done on decoded data - that is left up to the caller.  Any invalid input
 *  data is simply ignored.
 */
class Base64
{
public:
    /** Special value for the @a size parameters to encode() and decode() to indicate that the
     *  input data buffer is a null terminated string.  This is only safe on encode() if the
     *  input to be encoded is known to be a standard C string.  If binary data is provided
     *  to be encoded, an explicit size must always be given.  On decode(), this value should
     *  only be used if the encoded data being passed as input is known to be null terminated.
     *  encoded data produced by this object will always be null terminated.
     */
    static constexpr size_t kNullTerminated = ~0ull;

    /** Various variants of the Base64 algorithm.  The only differences these cause in the
     *  generated encoded data are in the bytes used for the values 62 and 63, as well as
     *  the padding byte.  All other encoded values will always use A-Z, a-z, and 0-9.  The
     *  correct variant must also be known ahead of time when decoding data.  No auto-detection
     *  on the input data will be done.  For most applications, the @ref Variant::eDefault
     *  and @ref Variant::eFilenameSafe variants should be the most useful.
     */
    enum class Variant
    {
        eDefault, ///< Default encoding set.
        ePem, ///< Encoding set for privacy-enhanced mail (RFC 1421, deprecated).
        eMime, ///< Encoding set for standard MIME Base64 (RFC 2045).
        eRfc4648, ///< Encoding set for RFC 4648.
        eFilenameSafe, ///< Encoding set for URL and filename safe Base64 (RFC 4648, section 5).
        eOpenPgp, ///< Encoding set for OpenPGP (RFC 4880).
        eUtf7, ///< Encoding set for UTF-7 (RFC 2152).
        eImap, ///< Encoding set for IMAP mailbox names (RFC 3501).
        eYui, ///< Encoding set for Y64 URL-sage Base64 from the YUI library.
        eProgramId1, ///< Encoding set for Program identifier Base64 variant 1 (non-standard).
        eProgramId2, ///< Encoding set for Program identifier Base64 variant 2 (non-standard).
        eFreenetUrl, ///< Encoding set for Freenet URL-safe Base64 (non-standard).
    };

    /** Constructor: creates a new object supporting the default Base64 encoding. */
    Base64() : Base64(Variant::eDefault)
    {
    }

    /** Constructor: creates a new object supporting a specific known Base64 encoding.
     *
     *  @param[in] variant  The algorithm variant to use.  This controls which additional two
     *                      encoding bytes and which padding byte will be used.  The variant
     *                      specifics about line lengths, checksums, and optional versus
     *                      mandatory padding will be ignored.
     */
    Base64(Variant variant)
    {
        switch (variant)
        {
            default:
            case Variant::eDefault:
            case Variant::ePem:
            case Variant::eMime:
            case Variant::eRfc4648:
            case Variant::eOpenPgp:
            case Variant::eUtf7:
                initCodec('+', '/', '=');
                break;

            case Variant::eFilenameSafe:
                initCodec('-', '_', '=');
                break;

            case Variant::eImap:
                initCodec('+', ',', '=');
                break;

            case Variant::eYui:
                initCodec('.', '_', '-');
                break;

            case Variant::eProgramId1:
                initCodec('_', '-', '=');
                break;

            case Variant::eProgramId2:
                initCodec('.', '_', '=');
                break;

            case Variant::eFreenetUrl:
                initCodec('~', '-', '=');
                break;
        }
    }

    /** Constructor: creates a new object supporting a custom Base64 encoding.
     *
     *  @param[in] byte62   The byte that will be used to represent the encoding for the value
     *                      62 (0x3e).  This may be any non-zero byte outside of the ranges
     *                      [A-Z, a-z, 0-9].  This may not be equal to @p byte63 or @p padding.
     *  @param[in] byte63   The byte that will be used to represent the encoding for the value
     *                      63 (0x3f).  This may be any non-zero byte outside of the ranges
     *                      [A-Z, a-z, 0-9].  This may not be equal to @p byte62 or @p padding.
     *  @param[in] padding  The padding byte to use at the end of an encoded block to identify
     *                      an unaligned block.  This may be 0 to indicate that no specific
     *                      padding byte is used.  This may not be equal to @p byte62 or
     *                      @p byte63 and must not be in the range [A-Z, a-z, 0-9].  The usual
     *                      padding byte for this value in most variants is '='.  This defaults
     *                      to 0.
     */
    Base64(uint8_t byte62, uint8_t byte63, uint8_t padding = 0)
    {
        initCodec(byte62, byte63, padding);
    }

    /** Calculates the required output size for encoding a given number of bytes.
     *
     *  @param[in] inputSize    The size of the input buffer in bytes.  This may not be 0.
     *  @returns The number of bytes required to store the encoded data for the given input
     *           size.  This will always include space for a null terminator byte so that the
     *           encoded data can always be treated as a C string for further processing.
     *           The actual size of the encoded data should always come from the value returned
     *           by encode().
     */
    static size_t getEncodeOutputSize(size_t inputSize)
    {
        return 4 * ((inputSize + 2) / 3) + 1;
    }

    /** Calculates the require input buffer size for a given output buffer length.
     *  @param[in] outputSize The size of the output to write.
     *  @returns The number of bytes that needs to be input to produce a given
     *           output size without padding.
     *           This is useful if you want to split up a base64 encode across
     *           multiple buffers. Breaking up the input into a series of chunks
     *           of this size will allow the output base64 chunks to be
     *           concatenated together without creating an invalid base64 string.
     */
    static size_t getEncodeInputSize(size_t outputSize)
    {
        return ((outputSize - 1) / 4 * 3) / 3 * 3;
    }

    /** Calculates the required output size for decoding a given number of bytes.
     *
     *  @param[in] inputSize    The size of the input buffer in bytes.  This may not be 0.
     *  @returns The number of bytes required to store the decoded data for the given input
     *           size.  This may include some extra space depending on whether the input
     *           buffer is null terminated or not.  The actual size of the decoded data
     *           should always come from the value returned by decode().
     */
    static size_t getDecodeOutputSize(size_t inputSize)
    {
        return 3 * ((inputSize + 3) / 4);
    }

    /** Encodes a block of binary data into Base64.
     *
     *  @param[in] buffer   The input buffer to be encoded.  This may not be `nullptr`.
     *  @param[in] size     The size of the input buffer in bytes.  This may not be `0`.  This may
     *                      be @ref kNullTerminated to indicate that the input data is a null
     *                      terminated C string.  The actual length of the input will be
     *                      calculated by finding the length of the string.
     *  @param[out] output  Receives the encoded output as long as it is large enough to contain
     *                      the entire encoded output.  No work will be done if the output buffer
     *                      is not large enough.  Encoding into the same buffer as @p buffer is
     *                      not safe since the input data will be overwritten as it is processed
     *                      resulting in a corrupted encoding.  This output buffer will always
     *                      be null terminated so that the output data can be processed as a
     *                      standard C string.
     *  @param[in] maxOut   The size of the output buffer in bytes.  This may be larger than is
     *                      strictly required by the encoding operation.  This must be large
     *                      enough to hold the entire encoded result.
     *  @returns The number of bytes of encoded data written to the output buffer, not including
     *           the null terminator.  Note that this byte count may not be aligned to a multiple
     *           of 4 if padding is ignored (ie: this object was initialized to use a padding byte
     *           of 0).
     *  @returns `0` if the output buffer is not large enough to hold the full encoded output.
     *
     *  @note If the encoded data block is expected to be transmitted to a generic destination
     *        (ie: one not controlled by this process or something related to it), the caller
     *        is responsible for ensuring that the data can be properly interpreted by the
     *        receiver regardless of endianness.  Since base64 encoding simply sees the block
     *        of data as a simple string of bytes, it has no knowledge of any internal structure
     *        and can therefore not properly do an kind of network byte order swapping.  The
     *        caller would be the one with the knowledge of internal structure and should
     *        byte swap the incoming data as needed before attempting to encode it.
     */
    size_t encode(const void* buffer, size_t size, char* output, size_t maxOut)
    {
        uint32_t data;
        size_t j = 0;
        size_t stop;
        size_t extra;
        size_t paddingCount = (m_padding == 0) ? 0 : 1;


        // null terminated C string input data => calculate its length.
        if (size == kNullTerminated)
            size = strlen(reinterpret_cast<const char*>(buffer));

        // the output buffer is not large enough => fail.
        if (maxOut < getEncodeOutputSize(size))
            return 0;

        // calculate the number of input bytes that can be bulk processed.
        stop = (size / 3) * 3;
        extra = size - stop;

        // bulk process all aligned input bytes.  Each three byte input block will produce
        // four output bytes.
        for (size_t i = 0; i < stop; i += 3)
        {
            data = (reinterpret_cast<const uint8_t*>(buffer)[i + 0] << 16) |
                   (reinterpret_cast<const uint8_t*>(buffer)[i + 1] << 8) |
                   (reinterpret_cast<const uint8_t*>(buffer)[i + 2] << 0);

            output[j + 0] = m_encode[(data >> 18) & 0x3f];
            output[j + 1] = m_encode[(data >> 12) & 0x3f];
            output[j + 2] = m_encode[(data >> 6) & 0x3f];
            output[j + 3] = m_encode[(data >> 0) & 0x3f];
            j += 4;
        }

        // process any remaining bytes.  Note that a value of 0 indicates that the original
        // input data was a multiple of 3 bytes and no unaligned data needs to be processed.
        switch (extra)
        {
            // one extra unaligned input byte was provided.  This will produce two output bytes
            // followed by two padding bytes.
            case 1:
                data = reinterpret_cast<const uint8_t*>(buffer)[stop + 0] << 16;

                output[j + 0] = m_encode[(data >> 18) & 0x3f];
                output[j + 1] = m_encode[(data >> 12) & 0x3f];
                output[j + 2] = m_padding;
                output[j + 3] = m_padding;
                j += 2 + (paddingCount * 2);
                break;

            // two extra unaligned input bytes were provided.  This will produce three output
            // bytes followed by one padding byte.
            case 2:
                data = (reinterpret_cast<const uint8_t*>(buffer)[stop + 0] << 16) |
                       (reinterpret_cast<const uint8_t*>(buffer)[stop + 1] << 8);

                output[j + 0] = m_encode[(data >> 18) & 0x3f];
                output[j + 1] = m_encode[(data >> 12) & 0x3f];
                output[j + 2] = m_encode[(data >> 6) & 0x3f];
                output[j + 3] = m_padding;
                j += 3 + paddingCount;
                break;

            // another count of destination characters (!?) -> should never happen for any value
            // other than 0 => ignore it.
            default:
                break;
        }

        // always null terminate the output buffer so that it can be treated as a C string by
        // the caller.
        output[j] = 0;

        return j;
    }

    /** Decodes a block of binary data from Base64.
     *
     *  @param[in] buffer   The input buffer to be decoded.  This may not be `nullptr`.  This
     *                      buffer may be optionally null terminated.
     *  @param[in] size     The size of the input buffer in bytes.  This may not be `0`.  This may
     *                      be @ref kNullTerminated to indicate that the input buffer is a null
     *                      terminated C string.  In this case, the length of the input buffer
     *                      will be calculated as the length of the string.
     *  @param[out] output  Receives the decoded data as long as it is large enough to contain
     *                      the entire decoded output.  No work will be done if the output buffer
     *                      is not large enough.  Decoding into the same buffer as @p buffer is
     *                      safe since the input data will be always be longer than the output.
     *                      No null terminator or extra data will ever be written to the decoded
     *                      data buffer.
     *  @param[in] maxOut   The size of the output buffer in bytes.  This may be larger than is
     *                      strictly required by the decoding operation.  This must be large
     *                      enough to hold the entire decoded result.
     *  @returns The number of bytes of decoded data written to the output buffer.  Note that
     *           this byte count will not have a particular alignment and will always match
     *           that of the original encoded data exactly.
     *  @returns `0` if the output buffer is not large enough to hold the full decoded output.
     *
     *  @note This will decode the block exactly as it was encoded and assuming a little endian
     *        byte ordering.  Since base64 always treats the data block as a simple string of
     *        bytes, the caller is responsible for doing any kind of endianness checks and
     *        byte swapping as necessary after decoding.
     */
    size_t decode(const char* buffer, size_t size, void* output, size_t maxOut)
    {
        uint32_t data;
        size_t j = 0;
        size_t stop;
        size_t extra;
        uint8_t* out = reinterpret_cast<uint8_t*>(output);


        // the input buffer is a null terminated C string => calculate its length.
        if (size == kNullTerminated)
            size = strlen(reinterpret_cast<const char*>(buffer));

        // the output buffer is not large enough => fail.
        if (maxOut < getDecodeOutputSize(size))
            return 0;

        // invalid encoding length -> decodes to less than 1 byte => fail.
        if (size < 2)
            return 0;

        // calculate the number of input bytes that can be bulk processed.
        extra = size & 3;
        stop = size - extra;

        // no extra unaligned input bytes were provided -> the input buffer is actually aligned
        // or it contains padding bytes => determine which one is correct and adjust sizes.
        if (extra == 0)
        {
            // two padding bytes were specified => produces one byte in the last block.
            if (buffer[size - 2] == m_padding)
            {
                extra = 2;
                stop -= 4;
            }

            // one padding byte was specified => produces two bytes in the last block.
            else if (buffer[size - 1] == m_padding)
            {
                extra = 3;
                stop -= 4;
            }

            // at this point, we know the input is either block aligned or is corrupt.  Either
            // way we don't care and will just continue decoding.  It is left up to the caller
            // to verify the validity of the decoded data.  All we are interested in here is
            // the actual decoding process.
        }

        // bulk process the aligned input data.  Each four byte block will produce three output
        // bytes.
        for (size_t i = 0; i < stop; i += 4)
        {
            data = (m_decode[static_cast<size_t>(buffer[i + 0])] << 18) |
                   (m_decode[static_cast<size_t>(buffer[i + 1])] << 12) |
                   (m_decode[static_cast<size_t>(buffer[i + 2])] << 6) |
                   (m_decode[static_cast<size_t>(buffer[i + 3])] << 0);

            out[j + 0] = (data >> 16) & 0xff;
            out[j + 1] = (data >> 8) & 0xff;
            out[j + 2] = (data >> 0) & 0xff;
            j += 3;
        }

        // process any extra unaligned bytes.  This will allow extra or optional padding bytes to
        // be ignored in the input buffer.
        switch (extra)
        {
            // two extra bytes (plus two optional padding bytes) were provided.  This produces one
            // output byte.
            case 2:
                data = (m_decode[static_cast<size_t>(buffer[stop + 0])] << 18) |
                       (m_decode[static_cast<size_t>(buffer[stop + 1])] << 12);

                out[j + 0] = (data >> 16) & 0xff;
                j += 1;
                break;

            // three extra bytes (plus one optional padding byte) were provided.  This produces two
            // output bytes.
            case 3:
                data = (m_decode[static_cast<size_t>(buffer[stop + 0])] << 18) |
                       (m_decode[static_cast<size_t>(buffer[stop + 1])] << 12) |
                       (m_decode[static_cast<size_t>(buffer[stop + 2])] << 6);

                out[j + 0] = (data >> 16) & 0xff;
                out[j + 1] = (data >> 8) & 0xff;
                j += 2;
                break;

            // invalid 'extra' count (!?) or no extra bytes => fail.
            default:
                break;
        }

        // return the number of decoded bytes.
        return j;
    }

private:
    /** Initializes the encoding and decoding tables for this object.
     *
     *  @param[in] char62   The byte that will be used to represent the encoding for the value
     *                      62 (0x3e).  This may be any non-zero byte outside of the ranges
     *                      [A-Z, a-z, 0-9].  This may not be equal to @p byte63 or @p padding.
     *  @param[in] char63   The byte that will be used to represent the encoding for the value
     *                      63 (0x3f).  This may be any non-zero byte outside of the ranges
     *                      [A-Z, a-z, 0-9].  This may not be equal to @p byte62 or @p padding.
     *  @param[in] padding  The padding byte to use at the end of an encoded block to identify
     *                      an unaligned block.  This may be 0 to indicate that no specific
     *                      padding byte is used.  This may not be equal to @p byte62 or
     *                      @p byte63 and must not be in the range [A-Z, a-z, 0-9].  The usual
     *                      padding byte for this value in most variants is '='.  This defaults
     *                      to 0.
     *  @returns No return value.
     */
    void initCodec(uint8_t char62, uint8_t char63, uint8_t padding)
    {
        // generate the encoding map.
        for (size_t i = 0; i < 26; i++)
        {
            m_encode[i] = ('A' + i) & 0xff;
            m_encode[i + 26] = ('a' + i) & 0xff;
        }

        for (size_t i = 0; i < 10; i++)
            m_encode[i + 52] = ('0' + i) & 0xff;

        m_encode[62] = char62;
        m_encode[63] = char63;

        // generate the decoding map as a the reverse of the encoding table.
        memset(m_decode, 0, sizeof(m_decode));

        for (size_t i = 0; i < 64; i++)
            m_decode[static_cast<size_t>(m_encode[i])] = i & 0xff;

        // store the padding byte.  Note that the padding byte does not need to be part of the
        // decoding table since it will never be decoded.
        m_padding = padding;
    }


    /** The encoding table for this object.  This specifies all possible encoding values for each
     *  of the 64 input data bytes.
     */
    char m_encode[64];

    /** The decoding table for this object.  This specifies the decoded bit patterns for all
     *  possible encoded input bytes.  This table will contain a zero for all invalid input
     *  bytes.
     */
    char m_decode[256];

    /** The padding byte used by this object.  This will be 0 to indicate that no padding
     *  bytes will be generated (on encode) or expected (on decode).
     */
    char m_padding;
};

} // namespace extras
} // namespace carb
