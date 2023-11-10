// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Provides helper classes to parse and convert UTF-8 strings to and from Unicode.
 */
#pragma once

#include "../Defines.h"
#include "../../omni/extras/ScratchBuffer.h"

#include <cstdint>
#include <algorithm>
#include <cmath>


/** Namespace for all low level Carbonite functionality. */
namespace carb
{
/** Common namespace for extra helper functions and classes. */
namespace extras
{

/** Static helper class to allow for the processing of UTF-8 strings.  This can walk individual
 *  codepoints in a string, decode and encode codepoints, and count the number of codepoints in
 *  a UTF-8 string.  Minimal error checking is done in general and there is a common assumption
 *  that the input string is valid.  The only failure points that will be checked are ones that
 *  prevent further decoding (ie: not enough space left in a buffer, unexpected sequences,
 *  misaligned strings, etc).
 */
class Utf8Parser
{
public:
    /** The base type for a single Unicode codepoint value.  This represents a decoded UTF-8
     *  codepoint.
     */
    using CodePoint = char32_t;

    /** The base type for a single UTF-16 Unicode codepoint value.  This represents a decoded
     *  UTF-8 codepoint that fits in 16 bits, or a single member of a surrogate pair.
     */
    using Utf16CodeUnit = char16_t;

    /** The base type for a point in a UTF-8 string.  Ideally these values should point to the
     *  start of an encoded codepoint in a string.
     */
    using CodeByte = char;

    /** Base type for flags to various encoding and decoding functions. */
    using Flags = uint32_t;

    /** Names to classify a decoded codepoint's membership in a UTF-16 surrogate pair.  The
     *  UTF-16 encoding allows a single Unicode codepoint to be represented using two 16-bit
     *  codepoints.  These two codepoints are referred to as the 'high' and 'low' codepoints.
     *  The high codepoint always comes first in the pair, and the low codepoint is expected
     *  to follow immediately after.  The high codepoint is always in the range 0xd800-0xdbff.
     *  The low codepoint is always in the range 0xdc00-0xdfff.  These ranges are reserved
     *  in the Unicode set specifically for the encoding of UTF-16 surrogate pairs and will
     *  never appear in an encoded UTF-8 string that contains UTF-16 encoded pairs.
     */
    enum class SurrogateMember
    {
        /** The codepoint is not part of a UTF-16 surrogate pair.  This codepoint is outside
         *  the 0xd800-0xdfff range and is either represented directly in UTF-8 or is small
         *  enough to not require a UTF-16 encoding.
         */
        eNone,

        /** The codepoint is a high value in the surrogate pair.  This must be in the range
         *  reserved for UTF-16 high surrogate pairs (ie: 0xd800-0xdbff).  This codepoint must
         *  come first in the pair.
         */
        eHigh,

        /** The codepoint is a low value in the surrogate pair.  This must be in the range
         *  reserved for UTF-16 low surrogate pairs (ie: 0xdc00-0xdfff).  This codepoint must
         *  come second in the pair.
         */
        eLow,
    };

    /** Flag to indicate that the default codepoint should be returned instead of just a
     *  zero when attempting to decode an invalid UTF-8 sequence.
     */
    static constexpr Flags fDecodeUseDefault = 0x00000001;

    /** Flag to indicate that invalid code bytes should be skipped over in a string when searching
     *  for the start of the next codepoint.  The default behavior is to fail the operation when
     *  an invalid sequence is encountered.
     */
    static constexpr Flags fDecodeSkipInvalid = 0x00000002;

    /** Flag to indicate that UTF-16 surrogate pairs should be used when encoding large codepoints
     *  instead of directly representing them in UTF-8.  Using this flag makes UTF-8 strings more
     *  friendly to other UTF-8 systems on Windows (though Windows decoding facilities would still
     *  be able to decode the directly stored codepoints as well).
     */
    static constexpr Flags fEncodeUseUtf16 = 0x00000004;

    /** Flag for nextCodePoint() which tells the function to ignore surrogate pairs when decoding
     *  and just return both elements of the pair as code points.
     *  This exists mainly for internal use.
     */
    static constexpr Flags fEncodeIgnoreSurrogatePairs = 0x00000008;

    /** The string buffer is effectively null terminated.  This allows the various decoding
     *  functions to bypass some range checking with the assumption that there is a null
     *  terminating character at some point in the buffer.
     */
    static constexpr size_t kNullTerminated = ~0ull;

    /** An invalid Unicode codepoint. */
    static constexpr CodePoint kInvalidCodePoint = ~0u;

    /** The minimum buffer size that is guaranteed to be large enough to hold an encoded UTF-8
     *  codepoint.  This does not include space for a null terminator codepoint.
     */
    static constexpr size_t kMaxSequenceLength = 7;

    /** The codepoint reserved in the Unicode standard to represent the decoded result of an
     *  invalid UTF-8 sequence.
     */
    static constexpr CodePoint kDefaultCodePoint = 0x0000fffd;


    /** Finds the start of the next UTF-8 codepoint in a string.
     *
     *  @param[in] str              The UTF-8 string to find the next codepoint in.  This may not
     *                              be `nullptr`.  This is assumed to be a valid UTF-8 encoded
     *                              string and the passed in address is assumed to be the start
     *                              of another codepoint.  If the @ref fDecodeSkipInvalid flag is
     *                              used in @p flags, an attempt will be made to find the start of
     *                              the next valid codepoint in the string before failing.  If a
     *                              valid lead byte is found within the bounds of the string, that
     *                              will be returned instead of failing in this case.
     *  @param[in] lengthInBytes    The remaining number of bytes in the string.  This may be
     *                              @ref kNullTerminated if the string is well known to be null
     *                              terminated.  This operation will not walk the string beyond
     *                              this number of bytes.  Note that the operation may still end
     *                              before this many bytes have been scanned if a null terminator
     *                              is encountered.
     *  @param[out] codepoint       Receives the decoded codepoint.  This may be `nullptr` if the
     *                              decoded codepoint is not needed.  If this is `nullptr`, none of
     *                              the work to decode the codepoint will be done.  If the next
     *                              UTF-8 codepoint is part of a UTF-16 surrogate pair, the full
     *                              codepoint will be decoded.
     *  @param[in] flags            Flags to control the behavior of this operation.  This may be
     *                              0 or one or more of the fDecode* flags.
     *  @returns The address of the start of the next codepoint in the string if one is found.
     *  @returns `nullptr` if the string is empty, a null terminator is found, or there are no more
     *           bytes remaining in the string.
     *
     *  @remarks This attempts to walk a UTF-8 encoded string to find the start of the next valid
     *           codepoint.  This can be used to walk from one codepoint to another and modify
     *           the string as needed, or to just count its length in codepoints.
     */
    static const CodeByte* nextCodePoint(const CodeByte* str,
                                         size_t lengthInBytes = kNullTerminated,
                                         CodePoint* codepoint = nullptr,
                                         Flags flags = 0)
    {
        // retrieve the next code point
        CodePoint high = 0;
        const CodeByte* next = nullptr;
        bool r = parseUtf8(str, &next, &high, lengthInBytes, flags);

        if (codepoint != nullptr)
        {
            *codepoint = high;
        }

        // parsing failed => just fail out
        if (!r)
        {
            return next;
        }

        // it's a surrogate pair (and we're allowed to parse those) => parse out the full pair
        if ((flags & fEncodeIgnoreSurrogatePairs) == 0 && classifyUtf16SurrogateMember(high) == SurrogateMember::eHigh)
        {
            // figure out the new length if it's not null terminated
            const size_t newLen = (lengthInBytes == kNullTerminated) ? kNullTerminated : (lengthInBytes - (next - str));

            // parse out the next code point
            CodePoint low = 0;
            r = parseUtf8(next, &next, &low, newLen, flags);

            // invalid surrogate pair => fail
            if (!r || classifyUtf16SurrogateMember(low) != SurrogateMember::eLow)
            {
                if (codepoint != nullptr)
                {
                    *codepoint = getFailureCodepoint(flags);
                }

                return next;
            }

            // valid surrogate pair => calculate the code point
            if (codepoint != nullptr)
            {
                *codepoint = (((high & kSurrogateMask) << kSurrogateBits) | (low & kSurrogateMask)) + kSurrogateBias;
            }

            return next;
        }

        return next;
    }

    /** Finds the start of the last UTF-8 codepoint in a string.
     *
     *  @param[in] str              The UTF-8 string to find the last codepoint in.
     *                              This is assumed to be a valid UTF-8 encoded
     *                              string and the passed in address is assumed to be the start
     *                              of another codepoint.  If the @ref fDecodeSkipInvalid flag is
     *                              used in @p flags, an attempt will be made to find the start of
     *                              the last valid codepoint in the string before failing. If a
     *                              valid lead byte is found within the bounds of the string, that
     *                              will be returned instead of failing in this case.
     *  @param[in] lengthInBytes    The remaining number of bytes in the string.  This may be
     *                              @ref kNullTerminated if the string is well known to be null
     *                              terminated. This operation will not walk the string beyond
     *                              this number of bytes. Note that the operation may still end
     *                              before this many bytes have been scanned if a null terminator
     *                              is encountered.
     *  @param[out] codepoint       Receives the decoded codepoint.  This may be `nullptr` if the
     *                              decoded codepoint is not needed.  If this is `nullptr`, none of
     *                              the work to decode the codepoint will be done.  If the last
     *                              UTF-8 codepoint is part of a UTF-16 surrogate pair, the full
     *                              codepoint will be decoded.
     *  @param[in] flags            Flags to control the behavior of this operation.  This may be
     *                              0 or one or more of the fDecode* flags.
     *  @returns The address of the start of the last codepoint in the string if one is found.
     *  @returns `nullptr` if the string is empty, a null terminator is found, or there are no more
     *           bytes remaining in the string.
     *
     *  @remarks This function attempts to walk a UTF-8 encoded string to find the start of the last
     *           valid codepoint.
     */
    static const CodeByte* lastCodePoint(const CodeByte* str,
                                         size_t lengthInBytes = kNullTerminated,
                                         CodePoint* codepoint = nullptr,
                                         Flags flags = fDecodeUseDefault)
    {
        // Prepare error value result
        if (codepoint != nullptr)
        {
            *codepoint = getFailureCodepoint(flags);
        }

        // Check if it's a null or empty string
        if (!str || *str == 0)
        {
            return nullptr;
        }

        if (lengthInBytes == kNullTerminated)
        {
            lengthInBytes = std::strlen(str);
        }

        // Make sure no unexpected flags pass into used `nextCodePoint` function
        constexpr Flags kErrorHandlingMask = fDecodeSkipInvalid | fDecodeUseDefault;
        const Flags helperParserFlags = (flags & kErrorHandlingMask);

        size_t curCodePointSize = 0; // Keeps max number of bytes for a decoding attempt with `nextCodePoint`
        const bool skipInvalid = (flags & fDecodeSkipInvalid) != 0;

        // Walk the string backwards to find the start of the last CodePoint and decode it
        // Note: it can be a single byte or a sequence, also if not searching for the last valid CodePoint
        // the maximum number of bytes to check is just last `kMaxSequenceLength` bytes instead of the full string
        const CodeByte* const rIterBegin = str - 1 + lengthInBytes;
        const CodeByte* const rIterEnd =
            (flags & fDecodeSkipInvalid) ? str - 1 : CARB_MAX(str - 1, rIterBegin - kMaxSequenceLength);
        for (const CodeByte* rIter = rIterBegin; rIter != rIterEnd; --rIter)
        {
            const uint8_t curByte = static_cast<uint8_t>(*rIter);

            ++curCodePointSize;

            // Check if the current code byte is a direct ASCII character
            if (curByte < k7BitLimit)
            {
                // If parsed more than one byte then it's an error
                if (curCodePointSize > 1 && !skipInvalid)
                {
                    return nullptr;
                }

                if (codepoint != nullptr)
                {
                    *codepoint = curByte;
                }
                return rIter;
            }

            // The current code byte is a continuation byte so step further
            if (curByte < kMinLeadByte)
            {
                continue;
            }

            // The current code byte is a lead byte, decode the sequence and check that all bytes were used
            CodePoint cp{};
            const CodeByte* next = nextCodePoint(rIter, curCodePointSize, &cp, helperParserFlags);

            if (!next)
            {
                if (skipInvalid)
                {
                    curCodePointSize = 0;
                    continue;
                }

                return nullptr;
            }

            // Validate that all bytes till the end were used if expecting no invalid bytes
            // Ex: "\xce\xa6\xa6" is a 2 byte sequence "\xce\xa6" for a 0x03A6 code point followed by excessive
            // follow up byte "\xa6". The first 2 bytes will be decoded by the `nextCodePoint` properly
            // and `next` will be pointing at the last "\xa6" byte
            if (!skipInvalid && curCodePointSize != static_cast<size_t>(next - rIter))
            {
                return nullptr;
            }

            const SurrogateMember surrogateType = classifyUtf16SurrogateMember(cp);

            // Encountered the high surrogate part first which is an error
            if (CARB_UNLIKELY(surrogateType == SurrogateMember::eHigh))
            {
                if (skipInvalid)
                {
                    // Just skip it and search further
                    curCodePointSize = 0;
                    continue;
                }

                return nullptr;
            }
            // Found the low part of a surrogate pair, need to continue parsing to get the high part
            else if (CARB_UNLIKELY(surrogateType == SurrogateMember::eLow))
            {
                constexpr int kSurrogatePartSize = 3;
                constexpr int kFullSurrogatePairSize = 2 * kSurrogatePartSize;

                // To prepare for possible continuation of parsing if skipping invalid bytes and no high surrogate is
                // found reset the possible CodePoint size
                curCodePointSize = 0;

                // For a valid UTF-8 string there are must be high surrogate (3 bytes) preceding low surrogate (3 bytes)
                if (rIter <= rIterEnd + kSurrogatePartSize)
                {
                    if (skipInvalid)
                    {
                        // Skip the low surrogate data and continue to check the preceding byte
                        continue;
                    }
                    return nullptr;
                }

                // Step 3 bytes preceding the low surrogate
                const CodeByte* const possibleHighSurStart = rIter - kSurrogatePartSize;
                // Check if it starts with a lead byte
                if (static_cast<uint8_t>(*possibleHighSurStart) < kMinLeadByte)
                {
                    if (skipInvalid)
                    {
                        continue;
                    }
                    return nullptr;
                }

                // Try to parse 6 bytes (full surrogate pair size) to get the whole CodePoint without skipping invalid
                // bytes
                const CodeByte* const decodedPairEnd =
                    nextCodePoint(possibleHighSurStart, kFullSurrogatePairSize, &cp, 0);

                if (!decodedPairEnd)
                {
                    if (skipInvalid)
                    {
                        continue;
                    }
                    return nullptr;
                }

                // Check if used all 6 bytes (as expected from a surrogate pair)
                if (decodedPairEnd - possibleHighSurStart != kFullSurrogatePairSize)
                {
                    if (skipInvalid)
                    {
                        continue;
                    }
                    return nullptr;
                }

                // A proper surrogate pair was parsed into the `cp`
                // and only the `rIter` has invalid value at this point
                rIter = possibleHighSurStart;
                // Just exit the block so the code below reports the result
            }

            if (codepoint)
            {
                *codepoint = cp;
            }

            // Everything is fine thus return start of the sequence
            return rIter;
        }

        // Didn't find start of a valid CodePoint
        return nullptr;
    }

    /** Calculates the length of a UTF-8 string in codepoints.
     *
     *  @param[in] str              The string to count the number of codepoints in.  This may not
     *                              be `nullptr`.  This is expected to be a valid UTF-8 string.
     *  @param[in] maxLengthInBytes The maximum number of bytes to parse in the string.  This can
     *                              be @ref kNullTerminated if the string is well known to be null
     *                              terminated.
     *  @param[in] flags            Flags to control the behavior of this operation.  This may be
     *                              0 or @ref fDecodeSkipInvalid.
     *  @returns The number of valid codepoints in the given UTF-8 string.
     *  @returns `0` if the string is empty or no valid codepoints are found.
     *
     *  @remarks This can be used to count the number of codepoints in a UTF-8 string.  The count
     *           will only include valid codepoints that are found in the string.  It will not
     *           include a count for any invalid code bytes that are skipped over when the
     *           @ref fDecodeSkipInvalid flag is used.  This means that it's not necessarily safe
     *           to use this result (when the flag is used) to allocate a decode buffer, then
     *           decode the codepoints in the string with getCodePoint() using the
     *           @ref fDecodeUseDefault flag.
     */
    static size_t getLengthInCodePoints(const CodeByte* str, size_t maxLengthInBytes = kNullTerminated, Flags flags = 0)
    {
        const CodeByte* current;
        const CodeByte* next;
        size_t count = 0;


        // get the second codepoint in the string.
        current = str;
        next = nextCodePoint(str, maxLengthInBytes, nullptr, flags);

        // empty or invalid string => fail.
        if (next == nullptr)
            return 0;

        if (maxLengthInBytes != kNullTerminated)
        {
            maxLengthInBytes -= next - current;
        }
        count++;

        do
        {
            current = next;
            next = nextCodePoint(current, maxLengthInBytes, nullptr, flags);

            if (next == nullptr)
                return count;

            if (maxLengthInBytes != kNullTerminated)
            {
                maxLengthInBytes -= next - current;
            }
            count++;
        } while (maxLengthInBytes > 0);

        return count;
    }

    /** Calculates the length of a Unicode string in UTF-8 code bytes.
     *
     *  @param[in] str                      The string to count the number of code bytes that will
     *                                      be required to store it in UTF-8.  This may not be
     *                                      `nullptr`.  This is expected to be a valid Unicode
     *                                      string.
     *  @param[in] maxLengthInCodePoints    The maximum number of codepoints to parse in the
     *                                      string.  This can be @ref kNullTerminated if the
     *                                      string is well known to be null terminated.
     *  @param[in] flags                    Flags to control the behavior of this operation.
     *                                      This may be 0 or @ref fEncodeUseUtf16.
     *  @returns The number of UTF-8 code bytes required to encode this Unicode string, not
     *           including the null terminator.
     *  @returns `0` if the string is empty or no valid codepoints are found.
     *
     *  @remarks This can be used to count the number of UTF-8 code bytes required to encode
     *           the given Unicode string.  The count will only include valid codepoints that
     *           are found in the string.  Note that if the @ref fEncodeUseUtf16 flag is not
     *           used here to calculate the size of a buffer, it should also not be used when
     *           converting codepoints.  Otherwise the buffer could overflow.
     *
     *  @note For the 32-bit codepoint variant of this function, it is assumed that UTF-16
     *        surrogate pairs do not exist in the source string.  In the 16-bit codepoint
     *        variant, surrogate pairs are supported.
     */
    static size_t getLengthInCodeBytes(const CodePoint* str,
                                       size_t maxLengthInCodePoints = kNullTerminated,
                                       Flags flags = 0)
    {
        size_t count = 0;
        size_t largeCodePointSize = 4;


        if ((flags & fEncodeUseUtf16) != 0)
            largeCodePointSize = 6;

        for (size_t i = 0; str[i] != 0 && i < maxLengthInCodePoints; i++)
        {
            if (str[i] < getMaxCodePoint(0))
                count++;

            else if (str[i] < getMaxCodePoint(1))
                count += 2;

            else if (str[i] < getMaxCodePoint(2))
                count += 3;

            else if (str[i] < getMaxCodePoint(3))
                count += largeCodePointSize;

            else if (str[i] < getMaxCodePoint(4))
                count += 5;

            else if (str[i] < getMaxCodePoint(5))
                count += 6;

            else
                count += 7;
        }

        return count;
    }

    /** @copydoc getLengthInCodeBytes */
    static size_t getLengthInCodeBytes(const Utf16CodeUnit* str,
                                       size_t maxLengthInCodePoints = kNullTerminated,
                                       Flags flags = 0)
    {
        size_t count = 0;
        size_t largeCodePointSize = 4;


        if ((flags & fEncodeUseUtf16) != 0)
            largeCodePointSize = 6;

        for (size_t i = 0; str[i] != 0 && i < maxLengthInCodePoints; i++)
        {
            if (str[i] < getMaxCodePoint(0))
                count++;

            else if (str[i] < getMaxCodePoint(1))
                count += 2;

            else
            {
                // found a surrogate pair in the string -> both of these codepoints will decode to
                // a single UTF-32 codepoint => skip the low surrogate and add the size of a
                //   single encoded codepoint.
                if (str[i] >= kSurrogateBaseHigh && str[i] < kSurrogateBaseLow && i + 1 < maxLengthInCodePoints &&
                    str[i + 1] >= kSurrogateBaseLow && str[i + 1] <= kSurrogateMax)
                {
                    i++;
                    count += largeCodePointSize;
                }

                // not part of a UTF-16 surrogate pair => this will encode to 3 bytes in UTF-8.
                else
                    count += 3;
            }
        }

        return count;
    }

    /** Decodes a single codepoint from a UTF-8 string.
     *
     *  @param[in] str             The string to decode the first codepoint from.  This may not
     *                             be `nullptr`.  The string is expected to be aligned to the start
     *                             of a valid codepoint.
     *  @param[in] lengthInBytes   The number of bytes remaining in the string.  This can be set
     *                             to @ref kNullTerminated if the string is well known to be null
     *                             terminated.
     *  @param[in] flags           Flags to control the behavior of this operation.  This may be
     *                             `0` or @ref fDecodeUseDefault.
     *  @returns The decoded codepoint if successful.
     *  @returns `0` if the end of the string is encountered, the string is empty, or there are not
     *           enough bytes left in the string to decode a full codepoint, and the @p flags
     *           parameter is zero.
     *  @retval kDefaultCodePoint if the end of the string is encountered, the string is
     *           empty, or there are not enough bytes left in the string to decode a full
     *           codepoint, and the @ref fDecodeUseDefault flag is used.
     *
     *  @remarks This decodes the next codepoint in a UTF-8 string.  The returned codepoint may be
     *           part of a UTF-16 surrogate pair.  The classifyUtf16SurrogateMember() function can
     *           be used to determine if this is the case.  If this is part of a surrogate pair,
     *           the caller should decode the next codepoint then decode the full pair into a
     *           single codepoint using decodeUtf16CodePoint().
     */
    static CodePoint getCodePoint(const CodeByte* str, size_t lengthInBytes = kNullTerminated, Flags flags = 0)
    {
        char32_t c = 0;
        nextCodePoint(str, lengthInBytes, &c, flags);
        return c;
    }

    /** Encodes a single Unicode codepoint to UTF-8.
     *
     *  @param[in] cp               The codepoint to be encoded into UTF-8.  This may be any valid
     *                              Unicode codepoint.
     *  @param[out] str             Receives the encoded UTF-8 codepoint.  This may not be `nullptr`.
     *                              This could need to be up to seven bytes to encode any possible
     *                              Unicode codepoint.
     *  @param[in] lengthInBytes    The size of the output buffer in bytes.  No more than this many
     *                              bytes will be written to the @p str buffer.
     *  @param[out] bytesWritten    Receives the number of bytes that were written to the output
     *                              buffer.  This may not be `nullptr`.
     *  @param[in] flags            Flags to control the behavior of this operation.  This may be
     *                              `0` or @ref fEncodeUseUtf16.
     *  @returns The output buffer if the codepoint is successfully encoded.
     *  @returns `nullptr` if the output buffer was not large enough to hold the encoded codepoint.
     */
    static CodeByte* getCodeBytes(CodePoint cp, CodeByte* str, size_t lengthInBytes, size_t* bytesWritten, Flags flags = 0)
    {
        size_t sequenceLength = 0;
        size_t continuationLength = 0;
        size_t codePointCount = 1;
        CodePoint codePoint[2] = { cp, 0 };
        CodeByte* result;


        // not enough room in the buffer => fail.
        if (lengthInBytes == 0)
        {
            *bytesWritten = 0;
            return nullptr;
        }

        // a 7-bit ASCII character -> this can be directly stored => store and return.
        if (codePoint[0] < k7BitLimit)
        {
            str[0] = (codePoint[0] & 0xff);
            *bytesWritten = 1;
            return str;
        }

        // at this point we know that the encoding for the codepoint is going to require at least
        // two bytes.  We need to calculate the sequence length and encode the bytes.

        // allowing a UTF-16 surrogate pair encoding in the string and the codepoint is above the
        //   range where a surrogate pair is necessary => calculate the low and high codepoints
        //   for the pair and set the sequence length.
        if ((flags & fEncodeUseUtf16) != 0 && codePoint[0] >= kSurrogateBias)
        {
            sequenceLength = 3;
            continuationLength = 2;
            codePointCount = 2;

            codePoint[0] -= kSurrogateBias;

            codePoint[1] = kSurrogateBaseLow | (codePoint[0] & kSurrogateMask);
            codePoint[0] = kSurrogateBaseHigh | ((codePoint[0] >> kSurrogateBits) & kSurrogateMask);
        }

        // not using a UTF-16 surrogate pair => search for the required length of the sequence.
        else
        {
            // figure out the required sequence length for the given for this codepoint.
            for (size_t i = 1; i < kMaxSequenceBytes; i++)
            {
                if (codePoint[0] < getMaxCodePoint(i))
                {
                    sequenceLength = i + 1;
                    continuationLength = i;
                    break;
                }
            }

            // failed to find a sequence length for the given codepoint (?!?) => fail (this should
            //   never happen).
            if (sequenceLength == 0)
            {
                *bytesWritten = 0;
                return nullptr;
            }
        }


        // not enough space in the buffer to store the entire sequence => fail.
        if (lengthInBytes < sequenceLength * codePointCount)
        {
            *bytesWritten = 0;
            return nullptr;
        }

        result = str;

        // write out each of the codepoints.  If UTF-16 encoding is not being used, there will only
        // be one codepoint and this loop will exit after the first iteration.
        for (size_t j = 0; j < codePointCount; j++)
        {
            cp = codePoint[j];

            // write out the lead byte.
            *str = getLeadByte(continuationLength) |
                   ((cp >> (continuationLength * kContinuationShift)) & getLeadMask(continuationLength));
            str++;

            // write out the continuation bytes.
            for (size_t i = 0; i < continuationLength; i++)
            {
                *str = kContinuationBits |
                       ((cp >> ((continuationLength - i - 1) * kContinuationShift)) & kContinuationMask);
                str++;
            }
        }

        *bytesWritten = sequenceLength * codePointCount;
        return result;
    }

    /** Classifies a codepoint as being part of a UTF-16 surrogate pair or otherwise.
     *
     *  @param[in] cp   The codepoint to classify.  This may be any valid Unicode codepoint.
     *  @retval SurrogateMember::eNone if the codepoint is not part of a UTF-16 surrogate
     *           pair.
     *  @retval SurrogateMember::eHigh if the codepoint is a 'high' UTF-16 surrogate pair
     *           codepoint.
     *  @retval SurrogateMember::eLow if the codepoint is a 'low' UTF-16 surrogate pair
     *           codepoint.
     */
    static SurrogateMember classifyUtf16SurrogateMember(CodePoint cp)
    {
        if (cp >= kSurrogateBaseHigh && cp < kSurrogateBaseLow)
            return SurrogateMember::eHigh;

        if (cp >= kSurrogateBaseLow && cp <= kSurrogateMax)
            return SurrogateMember::eLow;

        return SurrogateMember::eNone;
    }


    /** Decodes a UTF-16 surrogate pair to a Unicode codepoint.
     *
     *  @param[in] high     The codepoint for the 'high' member of the UTF-16 surrogate pair.
     *  @param[in] low      The codepoint for the 'low' member of the UTF-16 surrogate pair.
     *  @returns The decoded codepoint if the two input codepoints were a UTF-16 surrogate pair.
     *  @returns `0` if either of the input codepoints were not part of a UTF-16 surrogate pair.
     */
    static CodePoint decodeUtf16CodePoint(CodePoint high, CodePoint low)
    {
        CodePoint cp;


        // the high and low codepoints are out of the surrogate pair range -> cannot decode => fail.
        if (high < kSurrogateBaseHigh || high >= kSurrogateBaseLow || low < kSurrogateBaseLow || low > kSurrogateMax)
            return 0;

        // decode the surrogate pair into a single Unicode codepoint.
        cp = (((high & kSurrogateMask) << kSurrogateBits) | (low & kSurrogateMask)) + kSurrogateBias;
        return cp;
    }

    /** Encodes a Unicode codepoint into a UTF-16 codepoint.
     *
     *  @param[in]  cp  The UTF-32 codepoint to encode.  This may be any valid codepoint.
     *  @param[out] out Receives the equivalent codepoint encoded in UTF-16.  This will either be
     *                  a single UTF-32 codepoint if its value is less than the UTF-16 encoding
     *                  size of 16 bits, or it will be a UTF-16 surrogate pair for codepoint
     *                  values larger than 16 bits.  In the case of a single codepoint being
     *                  written, it will occupy the lower 16 bits of this buffer.  If two
     *                  codepoints are written, the 'high' surrogate pair member will be in
     *                  the lower 16 bits, and the 'low' surrogate pair member will be in the
     *                  upper 16 bits.  This is suitable for use as a UTF-16 buffer to pass to
     *                  other functions that expect a surrogate pair.  The number of codepoints
     *                  written can be differentiated by the return value of this function.  This
     *                  may be `nullptr` if the encoded UTF-16 codepoint is not needed but only the
     *                  number of codepoints is of interest.  This may safely be the same buffer
     *                  as @p cp.
     *  @returns `1` if the requested codepoint was small enough for a direct encoding into UTF-16.
     *           This can be interpreted as a single codepoint being written to the output
     *           codepoint buffer.
     *  @returns `2` if the requested codepoint was too big for direct encoding in UTF-16 and had
     *           to be encoded as a surrogate pair.  This can be interpreted as two codepoints
     *           being written to the output codepoint buffer.
     */
    static size_t encodeUtf16CodePoint(CodePoint cp, CodePoint* out)
    {
        CodePoint high;
        CodePoint low;


        // small enough for a direct encoding => just store it.
        if (cp < kSurrogateBias)
        {
            if (out != nullptr)
                *out = cp;

            return 1;
        }

        // too big for direct encoding => convert it to a surrogate pair and store both in the
        //   output buffer.
        cp -= kSurrogateBias;
        low = kSurrogateBaseLow | (cp & kSurrogateMask);
        high = kSurrogateBaseHigh | ((cp >> kSurrogateBits) & kSurrogateMask);

        if (out != nullptr)
            *out = high | (low << 16);

        return 2;
    }

    /** Checks if the provided code point corresponds to a whitespace character
     *
     * @param[in]  cp  The UTF-32 codepoint to check
     *
     * @returns true if the codepoint is a whitespace character, false otherwise
     */
    inline static bool isSpaceCodePoint(CodePoint cp)
    {
        // Taken from https://en.wikipedia.org/wiki/Whitespace_character
        // Note: sorted to allow binary search
        static constexpr CodePoint kSpaceCodePoints[] = {
            0x0009, //  character tabulation
            0x000A, //  line feed
            0x000B, //  line tabulation
            0x000C, //  form feed
            0x000D, //  carriage return
            0x0020, //  space
            0x0085, //  next line
            0x00A0, //  no-break space
            0x1680, //  ogham space mark
            0x180E, //  Mongolian vowel separator
            0x2000, //  en quad
            0x2001, //  em quad
            0x2002, //  en space
            0x2003, //  em space
            0x2004, //  three-per-em space
            0x2005, //  four-per-em space
            0x2006, //  six-per-em space
            0x2007, //  figure space
            0x2008, //  punctuation space
            0x2009, //  thin space
            0x200A, //  hair space
            0x200B, //  zero width space
            0x200C, //  zero width non-joiner
            0x200D, //  zero width joiner
            0x2028, //  line separator
            0x2029, //  paragraph separator
            0x202F, //  narrow no-break space
            0x205F, //  medium mathematical space
            0x2060, //  word joiner
            0x3000, //  ideograph space
            0xFEFF, //  zero width non-breaking space
        };
        constexpr size_t kSpaceCodePointsCount = CARB_COUNTOF(kSpaceCodePoints);
        constexpr const CodePoint* const kSpaceCodePointsEnd = kSpaceCodePoints + kSpaceCodePointsCount;
        return std::binary_search(kSpaceCodePoints, kSpaceCodePointsEnd, cp);
    }


private:
    /** The number of valid codepoint bits in the lead byte of a UTF-8 codepoint.  This table is
     *  indexed by the continuation length of the sequence.  The first entry represents a sequence
     *  length of 1 (ie: continuation length of 0).  This table supports up to seven byte UTF-8
     *  sequences.  Currently the UTF-8 standard only requires support for four byte sequences
     *  since that covers the full defined Unicode set.  The extra bits allows for future UTF-8
     *  expansion without needing to change the parser.  Note that seven bytes is the theoretical
     *  limit for a single UTF-8 sequence with the current algorithm.  This allows for 36 bits of
     *  codepoint information (which would already require a new codepoint representation anyway).
     *  The most feasible decoding limit is a six byte sequence which allows for up to 31 bits of
     *  codepoint information.
     */
    static constexpr uint8_t s_leadBits[] = { 7, 5, 4, 3, 2, 1, 0 };

    /** The maximum decoded codepoint currently set for the UTF-8 and Unicode standards.  This
     *  full range is representable with a four byte UTF-8 sequence.  When the Unicode standard
     *  expands beyond this limit, longer UTF-8 sequences will be required to fully encode the
     *  new range.
     */
    static constexpr CodePoint kMaxCodePoint = 0x0010ffff;

    /** The number of bits of valid codepoint information in the low bits of each continuation
     *  byte in a sequence.
     */
    static constexpr uint32_t kContinuationShift = 6;

    /** The lead bits of a continuation byte.  Each continuation byte starts with the high
     *  bit set followed by a clear bit, then the remaining bits contribute to the codepoint.
     */
    static constexpr uint8_t kContinuationBits = 0x80;

    /** The mask of bits in each continuation byte that contribute to the codepoint. */
    static constexpr uint8_t kContinuationMask = (1u << kContinuationShift) - 1;


    /** The base and bias value for a UTF-16 surrogate pair value.  When encoding a surrogate
     *  pair, this is subtracted from the original codepoint so that only 20 bits of relevant
     *  information remain.  These 20 bits are then split across the two surrogate pair values.
     *  When decoding, this value is added to the decoded 20 bits that are extracted from the
     *  code surrogate pair to get the final codepoint value.
     */
    static constexpr CodePoint kSurrogateBias = 0x00010000;

    /** The lowest value that a UTF-16 surrogate pair codepoint can have.  This will be the
     *  low end of the range for the 'high' member of the pair.
     */
    static constexpr CodePoint kSurrogateBaseHigh = 0x0000d800;

    /** The middle value that a UTF-16 surrogate pair codepoint can have.  This will be the
     *  low end of the range for the 'low' member of the pair, and one larger than the high
     *  end of the range for the 'high' member of the pair.
     */
    static constexpr CodePoint kSurrogateBaseLow = 0x0000dc00;

    /** The lowest value that any UTF-16 surrogate pair codepoint can have. */
    static constexpr CodePoint kSurrogateMin = 0x0000d800;

    /** The highest value that any UTF-16 surrogate pair codepoint can have.  This will be the
     *  high end of the range for the 'low' member of the pair.
     */
    static constexpr CodePoint kSurrogateMax = 0x0000dfff;

    /** The number of bits of codepoint information that is extracted from each member of the
     *  UTF-16 surrogate pair.  These will be the lowest bits of each of the two codepoints.
     */
    static constexpr uint32_t kSurrogateBits = 10;

    /** The mask of the bits of codepoint information in each of the UTF-16 surrogate pair
     *  members.
     */
    static constexpr CodePoint kSurrogateMask = ((1 << kSurrogateBits) - 1);


    /** The maximum number of bytes that a UTF-8 codepoint sequence can have.  This is the
     *  mathematical limit of the algorithm.  The current standard only supports up to four
     *  byte sequences right now.  A four byte sequence covers the full Unicode codepoint
     *  set up to and including 0x10ffff.
     */
    static constexpr size_t kMaxSequenceBytes = 7;

    /** The limit of a directly representable ASCII character in a UTF-8 sequence.  All values
     *  below this are simply directly stored in the UTF-8 sequence and can be trivially decoded
     *  as well.
     */
    static constexpr uint8_t k7BitLimit = 0x80;

    /** The minimum value that could possibly represent a lead byte in a multi-byte UTF-8
     *  sequence.  All values below this are either directly representable in UTF-8 or indicate
     *  a continuation byte for the sequence.  Note that some byte values above this range
     *  could still indicate an invalid sequence.  See @a getContinuationLength() for more
     *  information on this.
     */
    static constexpr uint8_t kMinLeadByte = 0xc0;


    /** Retrieves the continuation length of a UTF-8 sequence for a given lead byte.
     *
     *  @param[in] leadByte     The lead byte of a UTF-8 sequence to retrieve the continuation
     *                          length for.  This must be greater than or equal to the value of
     *                          @a kMinLeadByte.  The results are undefined if the lead byte
     *                          is less than @a kMinLeadByte.
     *  @returns The number of continuation bytes in the sequence for the given lead byte.
     *
     *  @remarks This provides a lookup helper to retrieve the expected continuation length for
     *           a given lead byte in a UTF-8 sequence.  A return value of 0 indicates an invalid
     *           sequence (either an overlong sequence or an invalid lead byte).  The lead byte
     *           values 0xc0 and 0xc1 for example represent an overlong sequence (ie: one that
     *           unnecessarily encodes a single byte codepoint using multiple bytes).  The lead
     *           byte 0xff represents an invalid lead byte since it violates the rules of the
     *           UTF-8 encoding.  Each other value represents the number bytes that follow the
     *           lead byte (not including the lead byte) to encode the codepoint.
     */
    static constexpr uint8_t getContinuationLength(size_t leadByte)
    {
        constexpr uint8_t s_continuationSize[] = {
            0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0xc0 - 0xcf */
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0xd0 - 0xdf */
            2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0xe0 - 0xef */
            3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 6, 0, /* 0xf0 - 0xff */
        };
        return s_continuationSize[leadByte - kMinLeadByte];
    }

    /** Retrieves the lead byte mask for a given sequence continuation length.
     *
     *  @param[in] continuationLength   The continuation length for the sequence.  This must be
     *                                  less than @ref kMaxSequenceLength.
     *  @returns The mask of bits in the lead byte that contribute to the decoded codepoint.
     *
     *  @remarks This provides a lookup for the lead byte mask that determines which bits will
     *           contribute to the codepoint.  Each multi-byte sequence lead byte starts with as
     *           many high bits set as the length of the sequence in bytes, followed by a clear
     *           bit.  The remaining bits in the lead byte contribute directly to the highest
     *           bits of the decoded codepoint.
     */
    static constexpr uint8_t getLeadMask(size_t continuationLength)
    {
        constexpr uint8_t s_leadMasks[] = { (1u << s_leadBits[0]) - 1, (1u << s_leadBits[1]) - 1,
                                            (1u << s_leadBits[2]) - 1, (1u << s_leadBits[3]) - 1,
                                            (1u << s_leadBits[4]) - 1, (1u << s_leadBits[5]) - 1,
                                            (1u << s_leadBits[6]) - 1 };
        return s_leadMasks[continuationLength];
    }

    /** Retrieves the lead byte bits that indicate the sequence length.
     *
     *  @param[in] continuationLength   The continuation length for the sequence.  This must
     *                                  be less than @ref kMaxSequenceLength.
     *  @returns The high bits of the lead bytes with the given sequence length.
     *
     *  @remarks This provides a lookup for the lead byte bits that indicate the sequence length
     *           for a UTF-8 codepoint.  This is used when encoding a Unicode codepoint into
     *           UTF-8.
     */
    static constexpr uint8_t getLeadByte(size_t continuationLength)
    {
        constexpr uint8_t s_leadBytes[] = {
            (0xffu << (s_leadBits[0] + 1)) & 0xff, (0xffu << (s_leadBits[1] + 1)) & 0xff,
            (0xffu << (s_leadBits[2] + 1)) & 0xff, (0xffu << (s_leadBits[3] + 1)) & 0xff,
            (0xffu << (s_leadBits[4] + 1)) & 0xff, (0xffu << (s_leadBits[5] + 1)) & 0xff,
            (0xffu << (s_leadBits[6] + 1)) & 0xff
        };
        return s_leadBytes[continuationLength];
    }

    /** Retrieves the maximum codepoint that can be represented by a given continuation length.
     *
     *  @param[in] continuationLength   The continuation length for the sequence.  This must
     *                                  be less than @ref kMaxSequenceLength.
     *  @returns The maximum codepoint value that can be represented by the given continuation
     *           length.  This covers up to sequences with seven bytes.
     *
     *  @remarks This provides a lookup for the maximum codepoint that can be represented by
     *           a UTF-8 sequence of a given continuation length.  This can be used to find the
     *           required length for a sequence when encoding.  Note that the limit for a seven
     *           byte encoding is not currently fully representable here because it can hold
     *           between 32 and 36 bits of codepoint information.
     */
    static constexpr CodePoint getMaxCodePoint(size_t continuationLength)
    {
        constexpr CodePoint s_maxCodePoint[] = { 0x00000080, 0x00000800, 0x00010000, 0x00200000,
                                                 0x04000000, 0x80000000, 0xffffffff };
        return s_maxCodePoint[continuationLength];
    }

    /** Helper function to decode a single UTF-8 continuation byte.
     *
     *  @param[in] byte                 The continuation byte to be decoded.
     *  @param[in] continuationLength   The number of continuation bytes remaining in the sequence
     *                                  including the current continuation byte @p byte.
     *  @returns The decoded codepoint bits shifted to their required position.  This value should
     *           be bitwise OR'd with the results of the previously decoded bytes in the sequence.
     */
    inline static CodePoint decodeContinuationByte(uint8_t byte, size_t continuationLength)
    {
        return (byte & kContinuationMask) << ((continuationLength - 1) * kContinuationShift);
    }

    /** Helper function to retrieve the codepoint in a failure case.
     *
     *  @param[in] flags    The flags passed into the original function.  Only the
     *                      @ref fDecodeUseDefault flag will be checked.
     *  @returns `0` if the @ref fDecodeUseDefault flag is not used.
     *  @retval kDefaultCodePoint if the flag is used.
     */
    static constexpr CodePoint getFailureCodepoint(Flags flags)
    {
        return (flags & fDecodeUseDefault) != 0 ? kDefaultCodePoint : 0;
    }

    /** @brief Parse the next UTF-8 code point.
     *  @param[in]  str           The UTF-8 input string to parse.
     *  @param[out] outNext       The pointer to the start of the next UTF-8
     *                            character in the string.
     *                            This pointer will be set to `nullptr` if there
     *                            is no next character this string can point to
     *                            (e.g. if the string ended).
     *                            This will also be set to `nullptr` if the UTF-8
     *                            character is invalid and @p flags does not contain
     *                            @ref fDecodeSkipInvalid.
     *                            This must not be `nullptr`.
     *  @param[out] outCodePoint  The code point that was parsed out of the UTF-8 string.
     *                            If the call fails, this will be set to a
     *                            default value depending on @p flags.
     *  @param[in]  lengthInBytes The remaining number of bytes in the string.  This may be
     *                            @ref kNullTerminated if the string is well known to be null
     *                            terminated.  This operation will not walk the string beyond
     *                            this number of bytes.  Note that the operation may still end
     *                            before this many bytes have been scanned if a null terminator
     *                            is encountered.
     *  @param[in]  flags         Flags to control the behavior of this operation.  This may be
     *                            0 or one or more of the fDecode* flags.
     *
     *  @returns This returns `true` if the next UTF-8 character was parsed successfully
     *           and `false` if the UTF-8 sequence was invalid.
     *           `false` will also be returned if @p lengthInBytes is 0.
     *           `true` will be returned for a null terminating character.
     *
     *  @note This will not parse UTF-16 surrogate pairs. If one is encountered,
     *        both code points will be returned in subsequent calls.
     */
    static bool parseUtf8(const CodeByte* str,
                          const CodeByte** outNext,
                          CodePoint* outCodePoint,
                          size_t lengthInBytes = kNullTerminated,
                          Flags flags = 0)
    {
        auto fail = [&]() -> bool {
            // we weren't asked to attempt to skip over invalid code sequences => just fail out
            if ((flags & fDecodeSkipInvalid) == 0)
            {
                return false;
            }

            // walk the rest of the string skipping over continuation bytes and invalid lead bytes.
            // Note that we've already tested and rejected the first byte so we just need to continue
            // the search starting at the next byte.
            for (size_t i = 1; i < lengthInBytes; i++)
            {
                const auto b = static_cast<uint8_t>(str[i]);
                // continuation byte => skip it.
                if ((b & ~kContinuationMask) == kContinuationBits)
                    continue;

                // invalid lead byte => skip it.
                if (b >= kMinLeadByte && getContinuationLength(b) == 0)
                    continue;

                // invalid range of bytes
                if (b >= k7BitLimit && b < kMinLeadByte)
                    continue;

                *outNext = str + i;
                return false;
            }

            // We've hit the end of the string.  This mean that the sequence is
            // either invalid, misaligned, or an illegal overlong sequence was
            // used.  We aren't able to write out the next character pointer if
            // we hit this point.
            return false;
        };

        // initialize to failure values;
        *outCodePoint = getFailureCodepoint(flags);
        *outNext = nullptr;


        // the string doesn't have any more bytes in it -> no more codepoints => fail.
        if (lengthInBytes == 0)
        {
            return false;
        }

        const auto byte = static_cast<uint8_t>(*str);

        // the current code byte is at the null terminator -> no more codepoints => finish.
        if (byte == '\0')
        {
            *outCodePoint = byte;
            return true;
        }

        // the current code byte is a direct ASCII character => finish.
        if (byte < k7BitLimit)
        {
            *outCodePoint = byte;
            *outNext = str + 1;
            return true;
        }

        if (byte < kMinLeadByte)
        {
            return fail();
        }

        // the current code byte is a lead byte => calculate the sequence length and return the
        //   start of the next codepoint.
        const size_t continuationLength = getContinuationLength(byte);
        const size_t sequenceLength = continuationLength + 1;

        // not enough bytes left in the string to complete this codepoint => fail.
        // continuationLength of 0 is invalid => fail
        if (lengthInBytes < sequenceLength || continuationLength == 0)
        {
            return fail();
        }

        // decode the codepoint.
        {
            CodePoint cp = (byte & getLeadMask(continuationLength)) << (continuationLength * kContinuationShift);

            for (size_t i = 0; i < continuationLength; i++)
            {
                // validate the continuation byte so we don't walk past the
                // end of a null terminated string
                if ((uint8_t(str[i + 1]) & ~kContinuationMask) != kContinuationBits)
                {
                    return fail();
                }

                cp |= decodeContinuationByte(str[i + 1], continuationLength - i);
            }

            *outCodePoint = cp;
            *outNext = str + sequenceLength;
            return true;
        }
    }
};

/** A simple iterator class for walking a UTF-8 string.  This is built on top of the UTF8Parser
 *  static class and uses its functionality.  Strings can only be walked forward.  Random access
 *  to codepoints in the string is not possible.  If needed, the pointer to the start of the
 *  next codepoint or the codepoint index can be retrieved.
 */
class Utf8Iterator
{
public:
    /** Reference the types used in Utf8Parser for more convenient use locally.
     * @{
     */
    /** @copydoc Utf8Parser::CodeByte */
    using CodeByte = Utf8Parser::CodeByte;
    /** @copydoc Utf8Parser::CodePoint */
    using CodePoint = Utf8Parser::CodePoint;
    /** @copydoc Utf8Parser::Flags */
    using Flags = Utf8Parser::Flags;
    /** @} */

    // Reference the special length value used for null terminated strings.
    /** @copydoc Utf8Parser::kNullTerminated */
    static constexpr size_t kNullTerminated = Utf8Parser::kNullTerminated;

    Utf8Iterator()
        : m_prev(nullptr), m_string(nullptr), m_length(kNullTerminated), m_flags(0), m_lastCodePoint(0), m_index(0)
    {
    }

    /** Constructor: initializes a new iterator for a given string.
     *
     *  @param[in] string           The string to walk.  This should be a UTF-8 encoded string.
     *                              This can be `nullptr`, but the iterator will not be valid if so.
     *  @param[in] lengthInBytes    The maximum number of bytes to walk in the string.  This may
     *                              be kNullTerminated if the string is null terminated.  If the
     *                              string is unterminated or only a portion of it needs to be
     *                              iterated over, this may be the size of the buffer in bytes.
     *  @param[in] flags            Flags to control the behavior of the UTF-8 parser.  This may
     *                              be zero or more of the Utf8Parser::fDecode* flags.
     *  @returns No return value.
     */
    Utf8Iterator(const CodeByte* string, size_t lengthInBytes = kNullTerminated, Flags flags = 0)
        : m_prev(nullptr), m_string(string), m_length(lengthInBytes), m_flags(flags), m_lastCodePoint(0), m_index(0)
    {
        next();
    }

    /** Copy constructor: copies another iterator into this one.
     *
     *  @param[in] it   The iterator to be copied.  Note that if @p it is invalid, this iterator
     *                  will also become invalid.
     *  @returns No return value.
     */
    Utf8Iterator(const Utf8Iterator& it)
    {
        copy(it);
    }

    /** Checks if this iterator is still valid.
     *
     *  @returns `true` if this iterator still has at least one more codepoint to walk.
     *  @returns `false` if there is no more string data to walk and decode.
     */
    operator bool() const
    {
        return isValid();
    }

    /** Check is this iterator is invalid.
     *
     *  @returns `true` if there is no more string data to walk and decode.
     *  @returns `false` if this iterator still has at least one more codepoint to walk.
     */
    bool operator!() const
    {
        return !isValid();
    }

    /** Retrieves the codepoint at this iterator's current location.
     *
     *  @returns The codepoint at the current location in the string.  Calling this multiple
     *           times does not cause the decoding work to be done multiple times.  The decoded
     *           codepoint is cached once decoded.
     *  @returns `0` if there are no more codepoints to walk in the string.
     */
    CodePoint operator*() const
    {
        return m_lastCodePoint;
    }

    /** Retrieves the address of the start of the current codepoint.
     *
     *  @returns The address of the start of the current codepoint for this iterator.  This can be
     *           used as a way of copying, editing, or reworking the string during iteration.  It
     *           is the caller's responsibility to ensure the string is still properly encoded after
     *           any change.
     *  @returns `nullptr` if there is no more string data to walk.
     */
    const CodeByte* operator&() const
    {
        return m_prev;
    }

    /** Pre increment operator: walk to the next codepoint in the string.
     *
     *  @returns A reference to this iterator.  Note that if the end of the string is reached, the
     *           new state of this iterator will first point to the null terminator in the string
     *           (for null terminated strings), then after another increment will return the
     *           address `nullptr` from the '&' operator.  For length limited strings, reaching the
     *           end will immediately return `nullptr` from the '&' operator.
     */
    Utf8Iterator& operator++()
    {
        next();
        return *this;
    }

    /** Post increment operator: walk to the next codepoint in the string.
     *
     *  @returns A new iterator object representing the state of this object before the increment
     *           operation.
     */
    Utf8Iterator operator++(int32_t)
    {
        Utf8Iterator tmp = (*this);
        next();
        return tmp;
    }

    /** Increment operator: skip over zero or more codepoints in the string.
     *
     *  @param[in] count    The number of codepoints to skip over.  This may be zero or larger.
     *                      Negative values will be ignored and the iterator will not advance.
     *  @returns A reference to this iterator.
     */
    template <typename T>
    Utf8Iterator& operator+=(T count)
    {
        for (T i = 0; i < count && m_prev != nullptr; i++)
            next();

        return *this;
    }

    /** Addition operator: create a new iterator that skips zero or more codepoints.
     *
     *  @param[in] count    The number of codepoints to skip over.  This may be zero or larger.
     *                      Negative values will be ignored and the iterator will not advance.
     *  @returns A new iterator that has skipped over the next @p count codepoints in the string
     *           starting from the location of this iterator.
     */
    template <typename T>
    Utf8Iterator operator+(T count) const
    {
        Utf8Iterator tmp = *this;
        return (tmp += count);
    }

    /** Comparison operators.
     *
     *  @param[in] it   The iterator to compare this one to.
     *  @returns `true` if the string position represented by @p it satisfies the requested
     *           comparison versus this object.
     *  @returns `false` if the string position represented by @p it does not satisfy the requested
     *           comparison versus this object.
     *
     *  @remarks This object is treated as the left side of the comparison.  Only the offset into
     *           the string contributes to this result.  It is the caller's responsibility to
     *           ensure both iterators refer to the same string otherwise the results are
     *           undefined.
     */
    bool operator==(const Utf8Iterator& it) const
    {
        return m_string == it.m_string;
    }

    /** @copydoc operator== */
    bool operator!=(const Utf8Iterator& it) const
    {
        return m_string != it.m_string;
    }

    /** @copydoc operator== */
    bool operator<(const Utf8Iterator& it) const
    {
        return m_string < it.m_string;
    }

    /** @copydoc operator== */
    bool operator<=(const Utf8Iterator& it) const
    {
        return m_string <= it.m_string;
    }

    /** @copydoc operator== */
    bool operator>(const Utf8Iterator& it) const
    {
        return m_string > it.m_string;
    }

    /** @copydoc operator== */
    bool operator>=(const Utf8Iterator& it) const
    {
        return m_string >= it.m_string;
    }

    /** Copy assignment operator: copies another iterator into this one.
     *
     *  @param[in] it   The iterator to copy.
     *  @returns A reference to this object.
     */
    Utf8Iterator& operator=(const Utf8Iterator& it)
    {
        // Note: normally we'd check for an identity assignment in this operator overload and
        //       ignore.  Unfortunately we can't do that here since we also override the '&'
        //       operator above.  Since this copy operation should still be safe for an identity
        //       assignment, we'll just let it proceed.
        copy(it);
        return *this;
    }

    /** String assignment operator: resets this iterator to the start of a new string.
     *
     *  @param[in] str  The new string to start walking.  This must be a null terminated string.
     *                  If this is `nullptr`, the iterator will become invalid.  Any previous flags
     *                  and length limits on this iterator will be cleared out.
     *  @returns A reference to this object.
     */
    Utf8Iterator& operator=(const CodeByte* str)
    {
        m_prev = nullptr;
        m_string = str;
        m_length = kNullTerminated;
        m_lastCodePoint = 0;
        m_flags = 0;
        m_index = 0;
        next();
        return *this;
    }

    /** Retrieves the current codepoint index of the iterator.
     *
     *  @returns The number of codepoints that have been walked so far by this iterator in the
     *           current string.  This will always start at 0 and will only increase when a
     *           new codepoint is successfully decoded.
     */
    size_t getIndex() const
    {
        return m_index - 1;
    }

    /** Retrieves the size of the current codepoint in bytes.
     *
     *  @returns The size of the current codepoint (ie: the one returned with the '*' operator)
     *           in bytes.  This can be used along with the results of the '&' operator to copy
     *           this encoded codepoint into another buffer or modify the string in place.
     */
    size_t getCodepointSize() const
    {
        if (m_string == nullptr)
            return m_prev == nullptr ? 0 : 1;

        return m_string - m_prev;
    }

private:
    /** Copies another iterator into this one.
     *
     *  @param[in] it   The iterator to copy from.
     *  @returns No return value.
     */
    void copy(const Utf8Iterator& it)
    {
        m_prev = it.m_prev;
        m_string = it.m_string;
        m_length = it.m_length;
        m_flags = it.m_flags;
        m_lastCodePoint = it.m_lastCodePoint;
        m_index = it.m_index;
    }

    /** Checks whether this iterator is still valid and has more work to do.
     *
     *  @returns `true` if this iterator still has at least one more codepoint to walk.
     *  @returns `false` if there is no more string data to walk and decode.
     */
    bool isValid() const
    {
        return m_string != nullptr && m_lastCodePoint != 0;
    }

    /** Walks to the start of the next codepoint.
     *
     *  @returns No return value.
     *
     *  @remarks This walks this iterator to the start of the next codepoint.  The codepoint
     *           will be decoded and cached so that it can be retrieved.  The length limit
     *           will also be decremented to reflect the amount of string data left to parse.
     */
    void next()
    {
        const CodeByte* ptr;

        if (m_string == nullptr)
        {
            m_prev = nullptr;
            return;
        }

        if (m_length == 0)
        {
            m_string = nullptr;
            m_prev = nullptr;
            m_lastCodePoint = 0;
            return;
        }

        ptr = Utf8Parser::nextCodePoint(m_string, m_length, &m_lastCodePoint, m_flags);

        if (m_length != kNullTerminated)
            m_length -= ptr - m_string;

        m_prev = m_string;
        m_string = ptr;
        m_index++;
    }

    /** A pointer to the start of the codepoint that was last decoded.  This will be the codepoint
     *  that has been cached in @a m_lastCodePoint.  The size of this codepoint can be
     *  calculated by subtracting this address from @a m_string.
     */
    const CodeByte* m_prev;

    /** A pointer to the start of the next codepoint to be decoded.  This will be `nullptr` if the
     *  full string has been decoded.
     */
    const CodeByte* m_string;

    /** The number of bytes remaining in the string or kNullTerminated. */
    size_t m_length;

    /** Flags to control how the codepoints in the string will be decoded. */
    Flags m_flags;

    /** The last codepoint that was successfully decoded.  This is cached to prevent the need to
     *  decode it multiple times.
     */
    CodePoint m_lastCodePoint;

    /** The current codepoint index in the string.  This is incremented each time a codepoint is
     *  successfully decoded.
     */
    size_t m_index;
};

// implementation details used for string conversions - ignore this!
#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace detail
{
/** @brief A generic conversion between text formats.
 *  @tparam T The type of the input string.
 *  @tparam O The type of the output string.
 *  @tparam toCodePoint A function that converts a null terminated string of
 *                      type T into a UTF-32 code point.
 *                      This will return a pair where the first member is the
 *                      length of the input consumed and the second is the
 *                      UTF-32 code point that was calculated.
 *                      Returning a 0 length will be treated as an error and
 *                      input parsing will stop.
 *  @tparam fromCodePoint A function that converts a UTF-32 code point into
 *                        a string of type O.
 *                        This returns the length written to the ``out``.
 *                        This should return 0 if there is not enough space
 *                        in the output buffer; a partial character should
 *                        not be written to the output.
 *                        ``outLen`` is the amount of space available for
 *                        writing ``out``.
 *  @param[in]  str    The input string to convert. This may not be `nullptr`.
 *  @param[out] out    The output buffer to hold the converted data.
 *                     This must be at least @p outLen in length, in elements.
 *                     This can be `nullptr` to calculate the required
 *                     output buffer length.
 *                     The output string written to @p out will always be
 *                     null terminated (unless @p outLen is 0), even if the
 *                     string had to be truncated.
 *  @param[in]  outLen The length of @p out, in elements.
 *                     This should be 1 if @p out is `nullptr`.
 *  @returns If @p out is not `nullptr`, this returns the number of characters
 *           written to @p out. This includes the null terminating character.
 *  @returns If @p out is `nullptr`, this returns the required buffer length to
 *           store the output string.
 */
template <typename T,
          typename O,
          std::pair<size_t, char32_t>(toCodePoint)(const T*),
          size_t(fromCodePoint)(char32_t c, O* out, size_t outLen)>
inline size_t convertBetweenUnicodeFormatsRaw(const T* str, O* out, size_t outLen)
{
    // the last element written to the output
    O* prev = nullptr;
    size_t prevLen = 0;
    size_t written = 0;
    size_t read = 0;
    bool fullyRead = false;

    if (str == nullptr)
    {
        return 0;
    }

    // no output => ignore outLen in the loop
    if (out == nullptr)
    {
        outLen = SIZE_MAX;
    }

    if (outLen == 0)
    {
        return 0;
    }

    for (;;)
    {
        size_t len;

        // decode the input to UTF-32
        std::pair<size_t, char32_t> decoded = toCodePoint(str + read);

        // decode failed
        if (decoded.first == 0)
        {
            break;
        }

        // encode from UTF-32 to the output format
        len = fromCodePoint(decoded.second, (out == nullptr) ? nullptr : out + written, outLen - written);

        // out of buffer space
        if (len == 0)
        {
            break;
        }

        // store the last written character
        if (out != nullptr)
        {
            prev = out + written;
        }
        prevLen = len;

        // advance the indices
        written += len;
        read += decoded.first;

        // hit the null terminator => finished
        if (decoded.second == '\0')
        {
            fullyRead = true;
            break;
        }
    }

    // if the string was truncated, we need to cut out the last character
    // from the written count
    if (!fullyRead)
    {
        if (written == outLen)
        {
            written -= prevLen;
            written += 1;

            if (out != nullptr)
            {
                *prev = '\0';
            }
        }
        else
        {
            if (out != nullptr)
            {
                out[written] = '\0';
            }
            written++;
        }
    }

    return written;
}

inline std::pair<size_t, char32_t> utf8ToCodePoint(const char* str)
{
    char32_t c = 0;
    const char* p = Utf8Parser::nextCodePoint(str, Utf8Parser::kNullTerminated, &c, Utf8Parser::fDecodeUseDefault);
    if (c == '\0')
    {
        return std::pair<size_t, char32_t>(1, '\0');
    }
    else if (p == nullptr)
    {
        // invalid character, skip it
        return std::pair<size_t, char32_t>(1, c);
    }
    else
    {
        return std::pair<size_t, char32_t>(p - str, c);
    }
}

inline size_t utf32FromCodePoint(char32_t c, char32_t* out, size_t outLen)
{
    if (outLen == 0)
    {
        return 0;
    }
    else
    {
        if (out != nullptr)
        {
            out[0] = c;
        }
        return 1;
    }
};

inline std::pair<size_t, char32_t> utf32ToCodePoint(const char32_t* str)
{
    return std::pair<size_t, char32_t>(1, *str);
}

inline size_t utf8FromCodePoint(char32_t c, char* out, size_t outLen)
{
    char dummy[8];
    size_t len = 0;
    char* p;

    if (out == nullptr)
    {
        out = dummy;
        outLen = CARB_MIN(outLen, CARB_COUNTOF(dummy));
    }

    p = Utf8Parser::getCodeBytes(c, out, outLen, &len);
    if (p == nullptr)
    {
        return 0;
    }
    else
    {
        return len;
    }
}

inline std::pair<size_t, char32_t> utf16ToCodePoint(const char16_t* str)
{
    char32_t c;
    switch (Utf8Parser::classifyUtf16SurrogateMember(str[0]))
    {
        case Utf8Parser::SurrogateMember::eHigh:
            c = Utf8Parser::decodeUtf16CodePoint(str[0], str[1]);
            if (c == 0) // invalid surrogate pair
            {
                break;
            }

            return std::pair<size_t, char32_t>(2, c);

        // a stray low surrogate is invalid
        case Utf8Parser::SurrogateMember::eLow:
            break;

        default:
            return std::pair<size_t, char32_t>(1, str[0]);
    }

    // failed to parse => just return the invalid character code point
    constexpr static auto kDefaultCodePoint_ = Utf8Parser::kDefaultCodePoint; // CC-1110
    return std::pair<size_t, char32_t>(1, kDefaultCodePoint_);
}

inline size_t utf16FromCodePoint(char32_t c, char16_t* out, size_t outLen)
{
    char32_t mangled = 0;
    size_t len;

    len = Utf8Parser::encodeUtf16CodePoint(c, &mangled);
    if (outLen < len)
    {
        return 0;
    }

    if (out != nullptr)
    {
        switch (len)
        {
            default:
                break;

            case 1:
                out[0] = char16_t(mangled);
                break;

            case 2:
                out[0] = char16_t(mangled & 0xFFFF);
                out[1] = char16_t(mangled >> 16);
                break;
        }
    }

    return len;
}

/** @brief Helper to perform a conversion using a string.
 *  @tparam T The type of the input string.
 *  @tparam O The type of the output string.
 *  @tparam conv The function that will convert the input type to the output
 *               type using raw pointers.
 *               This should be a wrapped call to convertBetweenUnicodeFormatsRaw().
 *  @param[in] str The null terminated input string to convert.
 *  @returns @p str converted to the desired output format.
 */
template <typename T, typename O, size_t conv(const T* str, O* out, size_t outLen)>
inline std::basic_string<O> convertBetweenUnicodeFormats(const T* str)
{
    omni::extras::ScratchBuffer<O, 4096> buffer;
    size_t len = conv(str, nullptr, 0);
    if (len == 0 || !buffer.resize(len))
    {
        return std::basic_string<O>();
    }
    conv(str, buffer.data(), buffer.size());
    return std::basic_string<O>(buffer.data(), buffer.data() + len - 1);
}
} // namespace detail
#endif

/** Convert a UTF-8 encoded string to UTF-32.
 *  @param[in]  str    The input UTF-8 string to convert. This may not be `nullptr`.
 *  @param[out] out    The output buffer to hold the UTF-32 data.
 *                     This must be at least @p outLen in length, in elements.
 *                     This can be `nullptr` to calculate the required output buffer length.
 *                     The output string written to @p out will always be null terminated
 *                     (unless @p outLen is 0), even if the string had to be truncated.
 *  @param[in]  outLen The length of @p out, in elements.
 *                     This should be 0 if @p out is `nullptr`.
 *  @returns If @p out is not `nullptr`, this returns the number of UTF-32 characters
 *           written to @p out. This includes the null terminating character.
 *  @returns If @p out is `nullptr`, this returns the required buffer length to
 *           store the output string.
 */
inline size_t convertUtf8StringToUtf32(const char* str, char32_t* out, size_t outLen) noexcept
{
    return detail::convertBetweenUnicodeFormatsRaw<char, char32_t, detail::utf8ToCodePoint, detail::utf32FromCodePoint>(
        str, out, outLen);
}


/** Convert a UTF-8 encoded string to UTF-32.
 *  @param[in]  str The input UTF-8 string to convert. This may not be `nullptr`.
 *  @returns @p str converted to UTF-32.
 */
inline std::u32string convertUtf8StringToUtf32(const char* str)
{
    return detail::convertBetweenUnicodeFormats<char, char32_t, convertUtf8StringToUtf32>(str);
}

/** Convert a UTF-8 encoded string to UTF-32.
 *  @param[in]  str The input UTF-8 string to convert.
 *  @returns @p str converted to UTF-32.
 */
inline std::u32string convertUtf8StringToUtf32(std::string str)
{
    return convertUtf8StringToUtf32(str.c_str());
}

/** Convert a UTF-32 encoded string to UTF-8.
 *  @param[in]  str    The input UTF-32 string to convert. This may not be `nullptr`.
 *  @param[out] out    The output buffer to hold the UTF-8 data.
 *                     This must be at least @p outLen bytes in length.
 *                     This can be `nullptr` to calculate the required output buffer length.
 *                     The output string written to @p out will always be null terminated
 *                     (unless @p outLen is 0), even if the string had to be truncated.
 *  @param[in]  outLen The length of @p out, in bytes.
 *                     This should be 0 if @p out is `nullptr`.
 *  @returns If @p out is not `nullptr`, this returns the number of UTF-8 bytes
 *           written to @p out. This includes the null terminating character.
 *  @returns If @p out is `nullptr`, this returns the required buffer length to
 *           store the output string.
 */
inline size_t convertUtf32StringToUtf8(const char32_t* str, char* out, size_t outLen)
{
    return detail::convertBetweenUnicodeFormatsRaw<char32_t, char, detail::utf32ToCodePoint, detail::utf8FromCodePoint>(
        str, out, outLen);
}

/** Convert a UTF-32 encoded string to UTF-8.
 *  @param[in]  str The input UTF-32 string to convert. This may not be `nullptr`.
 *  @returns @p str converted to UTF-8.
 */
inline std::string convertUtf32StringToUtf8(const char32_t* str)
{
    return detail::convertBetweenUnicodeFormats<char32_t, char, convertUtf32StringToUtf8>(str);
}

/** Convert a UTF-8 encoded string to UTF-32.
 *  @param[in]  str The input UTF-8 string to convert.
 *  @returns @p str converted to UTF-32.
 */
inline std::string convertUtf32StringToUtf8(std::u32string str)
{
    return convertUtf32StringToUtf8(str.c_str());
}

/** Convert a UTF-16 encoded string to UTF-8.
 *  @param[in]  str    The input UTF-16 string to convert. This may not be `nullptr`.
 *  @param[out] out    The output buffer to hold the UTF-8 data.
 *                     This must be at least @p outLen bytes in length.
 *                     This can be `nullptr` to calculate the required output buffer length.
 *                     The output string written to @p out will always be null terminated
 *                     (unless @p outLen is 0), even if the string had to be truncated.
 *  @param[in]  outLen The length of @p out, in bytes.
 *                     This should be 0 if @p out is `nullptr`.
 *  @returns If @p out is not `nullptr`, this returns the number of UTF-8 bytes
 *           written to @p out. This includes the null terminating character.
 *  @returns If @p out is `nullptr`, this returns the required buffer length to
 *           store the output string.
 */
inline size_t convertUtf16StringToUtf8(const char16_t* str, char* out, size_t outLen)
{
    return detail::convertBetweenUnicodeFormatsRaw<char16_t, char, detail::utf16ToCodePoint, detail::utf8FromCodePoint>(
        str, out, outLen);
}

/** Convert a UTF-16 encoded string to UTF-8.
 *  @param[in]  str The input UTF-16 string to convert. This may not be `nullptr`.
 *  @returns @p str converted to UTF-8.
 */
inline std::string convertUtf16StringToUtf8(const char16_t* str)
{
    return detail::convertBetweenUnicodeFormats<char16_t, char, convertUtf16StringToUtf8>(str);
}

/** Convert a UTF-8 encoded string to UTF-32.
 *  @param[in]  str The input UTF-8 string to convert.
 *  @returns @p str converted to UTF-32.
 */
inline std::string convertUtf16StringToUtf8(std::u16string str)
{
    return convertUtf16StringToUtf8(str.c_str());
}

/** Convert a UTF-8 encoded string to UTF-16.
 *  @param[in]  str    The input UTF-8 string to convert. This may not be `nullptr`.
 *  @param[out] out    The output buffer to hold the UTF-16 data.
 *                     This must be at least @p outLen in length, in elements.
 *                     This can be `nullptr` to calculate the required output buffer length.
 *                     The output string written to @p out will always be null terminated
 *                     (unless @p outLen is 0), even if the string had to be truncated.
 *  @param[in]  outLen The length of @p out, in elements.
 *                     This should be 1 if @p out is `nullptr`.
 *  @returns If @p out is not `nullptr`, this returns the number of UTF-32 characters
 *           written to @p out. This includes the null terminating character.
 *  @returns If @p out is `nullptr`, this returns the required buffer length to
 *           store the output string.
 */
inline size_t convertUtf8StringToUtf16(const char* str, char16_t* out, size_t outLen) noexcept
{
    return detail::convertBetweenUnicodeFormatsRaw<char, char16_t, detail::utf8ToCodePoint, detail::utf16FromCodePoint>(
        str, out, outLen);
}

/** Convert a UTF-8 encoded string to UTF-16.
 *  @param[in]  str The input UTF-8 string to convert. This may not be `nullptr`.
 *  @returns @p str converted to UTF-16.
 */
inline std::u16string convertUtf8StringToUtf16(const char* str)
{
    return detail::convertBetweenUnicodeFormats<char, char16_t, convertUtf8StringToUtf16>(str);
}

/** Convert a UTF-8 encoded string to UTF-16.
 *  @param[in]  str The input UTF-8 string to convert.
 *  @returns @p str converted to UTF-16.
 */
inline std::u16string convertUtf8StringToUtf16(std::string str)
{
    return convertUtf8StringToUtf16(str.c_str());
}


/** Convert a UTF-8 encoded string to wide string.
 *  @param[in]  str    The input UTF-8 string to convert. This may not be `nullptr`.
 *  @param[out] out    The output buffer to hold the wide data.
 *                     This must be at least @p outLen in length, in elements.
 *                     This can be `nullptr` to calculate the required output buffer length.
 *                     The output string written to @p out will always be null terminated
 *                     (unless @p outLen is 0), even if the string had to be truncated.
 *  @param[in]  outLen The length of @p out, in elements.
 *                     This should be 1 if @p out is `nullptr`.
 *  @returns If @p out is not `nullptr`, this returns the number of wide characters
 *           written to @p out. This includes the null terminating character.
 *  @returns If @p out is `nullptr`, this returns the required buffer length to
 *           store the output string.
 *
 *  @note This is provided for interoperability with older systems that still use
 *        wide strings. Please use UTF-8 or UTF-32 for new systems.
 */
inline size_t convertUtf8StringToWide(const char* str, wchar_t* out, size_t outLen) noexcept
{
#if CARB_PLATFORM_WINDOWS
    static_assert(sizeof(*out) == sizeof(char16_t), "unexpected wchar_t type");
    return detail::convertBetweenUnicodeFormatsRaw<char, char16_t, detail::utf8ToCodePoint, detail::utf16FromCodePoint>(
        str, reinterpret_cast<char16_t*>(out), outLen);
#else
    static_assert(sizeof(*out) == sizeof(char32_t), "unexpected wchar_t type");
    return detail::convertBetweenUnicodeFormatsRaw<char, char32_t, detail::utf8ToCodePoint, detail::utf32FromCodePoint>(
        str, reinterpret_cast<char32_t*>(out), outLen);
#endif
}

/** Convert a UTF-8 encoded string to wide.
 *  @param[in]  str The input UTF-8 string to convert. This may not be `nullptr`.
 *  @returns @p str converted to wide.
 *
 *  @note This is provided for interoperability with older systems that still use
 *        wide strings. Please use UTF-8 or UTF-32 for new systems.
 */
inline std::wstring convertUtf8StringToWide(const char* str)
{
    return detail::convertBetweenUnicodeFormats<char, wchar_t, convertUtf8StringToWide>(str);
}

/** Convert a UTF-8 encoded string to UTF-16.
 *  @param[in]  str The input UTF-8 string to convert.
 *  @returns @p str converted to UTF-16.
 *
 *  @note This is provided for interoperability with older systems that still use
 *        wide strings. Please use UTF-8 or UTF-32 for new systems.
 */
inline std::wstring convertUtf8StringToWide(std::string str)
{
    return convertUtf8StringToWide(str.c_str());
}


/** Convert a wide encoded string to UTF-8 string.
 *  @param[in]  str    The input wide string to convert to UTF-8. This may not be `nullptr`.
 *  @param[out] out    The output buffer to hold the wide data.
 *                     This must be at least @p outLen in length, in elements.
 *                     This can be `nullptr` to calculate the required output buffer length.
 *                     The output string written to @p out will always be null terminated
 *                     (unless @p outLen is 0), even if the string had to be truncated.
 *  @param[in]  outLen The length of @p out, in elements.
 *                     This should be 1 if @p out is `nullptr`.
 *  @returns If @p out is not `nullptr`, this returns the number of wide characters
 *           written to @p out. This includes the null terminating character.
 *  @returns If @p out is `nullptr`, this returns the required buffer length to
 *           store the output string.
 *
 *  @note This is provided for interoperability with older systems that still use
 *        wide strings. Please use UTF-8 or UTF-32 for new systems.
 */
inline size_t convertWideStringToUtf8(const wchar_t* str, char* out, size_t outLen) noexcept
{
#if CARB_PLATFORM_WINDOWS
    static_assert(sizeof(*str) == sizeof(char16_t), "unexpected wchar_t type");
    return detail::convertBetweenUnicodeFormatsRaw<char16_t, char, detail::utf16ToCodePoint, detail::utf8FromCodePoint>(
        reinterpret_cast<const char16_t*>(str), out, outLen);
#else
    static_assert(sizeof(*str) == sizeof(char32_t), "unexpected wchar_t type");
    return detail::convertBetweenUnicodeFormatsRaw<char32_t, char, detail::utf32ToCodePoint, detail::utf8FromCodePoint>(
        reinterpret_cast<const char32_t*>(str), out, outLen);
#endif
}

/** Convert a UTF-8 encoded string to wide.
 *  @param[in]  str The input UTF-8 string to convert. This may not be `nullptr`.
 *  @returns @p str converted to wide.
 *
 *  @note This is provided for interoperability with older systems that still use
 *        wide strings. Please use UTF-8 or UTF-32 for new systems.
 */
inline std::string convertWideStringToUtf8(const wchar_t* str)
{
    return detail::convertBetweenUnicodeFormats<wchar_t, char, convertWideStringToUtf8>(str);
}

/** Convert a UTF-8 encoded string to UTF-16.
 *  @param[in]  str The input UTF-8 string to convert.
 *  @returns @p str converted to UTF-16.
 *
 *  @note This is provided for interoperability with older systems that still use
 *        wide strings. Please use UTF-8 or UTF-32 for new systems.
 */
inline std::string convertWideStringToUtf8(std::wstring str)
{
    return convertWideStringToUtf8(str.c_str());
}


} // namespace extras
} // namespace carb
