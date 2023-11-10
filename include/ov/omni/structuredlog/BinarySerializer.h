// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Module for serializing data into the structured logging queue.
 */
#pragma once

#include "../structuredlog/StringView.h"
#include "../../carb/extras/StringSafe.h"
#include "../../carb/Defines.h"

#include <stdint.h>


namespace omni
{
namespace structuredlog
{

/** A helper class to calculate the required size of a binary blob.
 *  To use this, just track all of the data that you want to insert into your
 *  binary blob.
 */
class BinaryBlobSizeCalculator
{
public:
    /** The version of binary blob ABI
     *  Headers that use these binary blobs should static assert on its version.
     *  Do not modify the layout of the binary blob without incrementing this.
     */
    static constexpr uint32_t kVersion = 0;

    /** Retrieve the tracked blob size.
     *  @returns The memory size that is needed to store this blob.
     */
    size_t getSize()
    {
        return m_counter;
    }

    /** Track a primitive type.
     *  @param[in] v The value to track.
     *               The actual value doesn't matter; only the type matters.
     */
    template <typename T>
    void track(T v)
    {
        CARB_UNUSED(v);
        static_assert(std::is_arithmetic<T>::value, "this is only valid for primitive types");
        m_counter = alignOffset<T>(m_counter);
        m_counter += sizeof(T);
    }

    /** Track an array type.
     *  @param[in] v   The array of values to track.
     *                 This may also be a string.
     *  @param[in] len The number of elements in array @p v.
     *                 If this is a string, this length *includes* the null terminator.
     */
    template <typename T>
    void track(T* v, uint16_t len)
    {
        CARB_UNUSED(v);
        static_assert(std::is_arithmetic<T>::value, "this is only valid for primitive types");
        m_counter = alignOffset<uint16_t>(m_counter);
        m_counter += sizeof(uint16_t);
        if (len > 0)
        {
            m_counter = alignOffset<T>(m_counter);
            m_counter += sizeof(T) * len;
        }
    }

    /** Track a @ref StringView.
     *  @param[in] v The string view to track.
     */
    void track(const StringView& v)
    {
        m_counter = alignOffset<uint16_t>(m_counter);
        m_counter += sizeof(uint16_t);
        m_counter += v.length() + 1;
    }

    /** Track a @ref StringView.
     *  @param[in] v The string view to track.
     */
    void track(StringView& v)
    {
        track(static_cast<const StringView&>(v));
    }

    /** Track an array of strings into the buffer with pre-calculated lengths
     *  @param[in] v             The array of strings to track.
     *                           This may not be nullptr.
     *                           The elements of this array may be nullptr; each element
     *                           of this must be a null terminated string otherwise.
     *  @param[in] stringLengths The length of each string in @p v.
     *                           These length must include the null  terminator of the string.
     *                           This may not be nullptr.
     *  @param[in] len           The number of elements in array @p v and array @p stringLengths.
     *  @returns `true` if the value was successfully written.
     *  @returns `false` if the blob ran out of buffer space.
     *
     *  @note This overload exists to avoid having to perform a strlen() on each
     *        string in the array twice (once when calculating the buffer size
     *        and once when writing the buffer).
     */
    void track(const char* const* v, const uint16_t* stringLengths, uint16_t len)
    {
        CARB_UNUSED(v);
        m_counter = alignOffset<uint16_t>(m_counter);
        m_counter += sizeof(uint16_t);
        for (uint16_t i = 0; i < len; i++)
        {
            m_counter = alignOffset<uint16_t>(m_counter);
            m_counter += sizeof(uint16_t);
            m_counter += stringLengths[i];
        }
    }

    /** Track an array of strings type.
     *  @param[in] v   The array of strings to track.
     *                 This may not be nullptr.
     *                 The elements of this array may be nullptr; each element
     *                 of this must be a null terminated string otherwise.
     *  @param[in] len The number of elements in array @p v.
     */
    void track(const char* const* v, uint16_t len)
    {
        m_counter = alignOffset<uint16_t>(m_counter);
        m_counter += sizeof(uint16_t);
        for (uint16_t i = 0; i < len; i++)
        {
            m_counter = alignOffset<uint16_t>(m_counter);
            m_counter += sizeof(uint16_t);
            if (v[i] != nullptr)
            {
                size_t size = strlen(v[i]) + 1;
                m_counter += CARB_MIN(size_t(UINT16_MAX), size);
            }
        }
    }

    /** Track an array of fixed size.
     *  @param[in] v   The array of values to track.
     *                 This may also be a string.
     *  @param[in] len The fixed length of this data array as specified by
     *                 the data schema. This can be larger than @p v.
     */
    template <typename T>
    void trackFixed(T* v, uint16_t len)
    {
        CARB_UNUSED(v);
        static_assert(std::is_arithmetic<T>::value, "this is only valid for primitive types");
        m_counter = alignOffset<T>(m_counter);
        m_counter += sizeof(T) * len;
    }

    /** Round an offset up to be aligned for a given type.
     *  @param[in] offset The offset to align.
     *  @returns @p offset rounded up to be aligned for the specified type.
     */
    template <typename T>
    static size_t alignOffset(size_t offset)
    {
        size_t misalign = offset & (sizeof(T) - 1);
        if (misalign != 0)
            offset += sizeof(T) - misalign;
        return offset;
    }

private:
    /** The length of the blob that's been tracked so far. */
    size_t m_counter = 0;
};

/** @{
 *  Constants to make the template specialization more readable.
 */
static constexpr bool kBlobWriterValidate = true;
static constexpr bool kBlobWriterNoValidate = false;
/** @} */

/** Anonymous namespace for a helper function */
namespace
{
void ignoreBlobWriterValidationError(const char* s) noexcept
{
    CARB_UNUSED(s);
}
} // namespace

/** The prototype of the function to call when a validation error occurs.
 *  @param[in] message The error message explaining what went wrong.
 */
using OnBlobWriterValidationErrorFunc = void (*)(const char* message);

/** A class to build a binary blob.
 *  The binary blob only has internal markers for variable length fields; to
 *  decode the binary blob, you will need some sort of external schema or fixed
 *  layout to decode the binary blob.
 *
 *  @param validate If this parameter is true, then the length of the blob will
 *                  be tracked while writing the blob and attempting to write
 *                  past the end of the buffer will cause methods to fail.
 *                  This is needed for the tests.
 *
 *  @param onValidationError This is a callback that gets executed when a validation
 *                           error is triggered, so logging can be called.
 *                           Logging cannot be called directly from this class
 *                           because it is used inside the logging system.
 */
template <bool validate = false, OnBlobWriterValidationErrorFunc onValidationError = ignoreBlobWriterValidationError>
class BlobWriter
{
public:
    /** The version of binary blob ABI
     *  Headers that use these binary blobs should static assert on its version.
     *  Do not modify the layout of the binary blob without incrementing this.
     */
    static constexpr uint32_t kVersion = 0;


    /** Constructor.
     *  @param[in] buffer The buffer to write into.
     *                    This buffer must be aligned to sizeof(void*).
     *  @param[in] bytes  The length of @p buffer.
     */
    BlobWriter(void* buffer, size_t bytes)
    {
        CARB_ASSERT(buffer != nullptr);
        CARB_ASSERT((uintptr_t(buffer) & (sizeof(void*) - 1)) == 0);
        m_buffer = static_cast<uint8_t*>(buffer);
        m_bufferLen = bytes;
    }

    /** Copy a primitive type element into the buffer.
     *  @param[in] v The value to copy into the buffer.
     *  @returns `true` if the value was successfully written.
     *  @returns `false` if the blob ran out of buffer space.
     */
    template <typename T>
    bool copy(T v)
    {
        static_assert(std::is_arithmetic<T>::value, "this is only valid for primitive types");

        alignBuffer<T>();
        if (validate && m_bufferLen < m_written + sizeof(v))
        {
            outOfMemoryErrorMessage(sizeof(v));
            return false;
        }

        reinterpret_cast<T*>(m_buffer + m_written)[0] = v;
        m_written += sizeof(v);
        return true;
    }

    /** Copy an array of strings into the buffer with pre-calculated lengths
     *  @param[in] v             The array of strings to write into the buffer.
     *                           This may be nullptr if @p len is 0.
     *                           The elements of this array may be nullptr; each element
     *                           of this must be a null terminated string otherwise.
     *  @param[in] stringLengths The length of each string in @p v.
     *                           These length must include the null terminator of the string.
     *                           This may be nullptr if @p len is 0.
     *  @param[in] len           The number of elements in array @p v and array @p stringLengths.
     *  @returns `true` if the value was successfully written.
     *  @returns `false` if the blob ran out of buffer space.
     *
     *  @note This overload exists to avoid having to perform a strlen() on each
     *        string in the array twice (once when calculating the buffer size
     *        and once when writing the buffer).
     */
    bool copy(const char* const* v, const uint16_t* stringLengths, uint16_t len)
    {
        CARB_ASSERT(v != nullptr || len == 0);
        CARB_ASSERT(stringLengths != nullptr || len == 0);

        alignBuffer<decltype(len)>();
        if (validate && m_bufferLen < m_written + sizeof(len))
        {
            outOfMemoryErrorMessage(sizeof(len));
            return false;
        }

        reinterpret_cast<decltype(len)*>(m_buffer + m_written)[0] = len;
        m_written += sizeof(len);
        for (uint16_t i = 0; i < len; i++)
        {
            alignBuffer<decltype(stringLengths[i])>();
            if (validate && m_bufferLen < m_written + sizeof(stringLengths[i]) + stringLengths[i])
            {
                outOfMemoryErrorMessage(sizeof(stringLengths[i]) + stringLengths[i]);
                return false;
            }

            reinterpret_cast<uint16_t*>(m_buffer + m_written)[0] = stringLengths[i];
            m_written += sizeof(stringLengths[i]);

            memcpy(m_buffer + m_written, v[i], stringLengths[i]);
            m_written += stringLengths[i];
        }
        return true;
    }

    /** Copy an array of strings into the buffer.
     *  @param[in] v   The array of strings to write into the buffer.
     *                 This may be nullptr if @p len is 0.
     *                 The elements of this array may be nullptr; each element
     *                 of this must be a null terminated string otherwise.
     *  @param[in] len The number of elements in array @p v.
     *  @returns `true` if the value was successfully written.
     *  @returns `false` if the blob ran out of buffer space.
     */
    bool copy(const char* const* v, uint16_t len)
    {
        CARB_ASSERT(v != nullptr || len == 0);

        alignBuffer<decltype(len)>();
        if (validate && m_bufferLen < m_written + sizeof(len))
        {
            outOfMemoryErrorMessage(sizeof(len));
            return false;
        }

        reinterpret_cast<decltype(len)*>(m_buffer + m_written)[0] = len;
        m_written += sizeof(len);
        for (uint16_t i = 0; i < len; i++)
        {
            size_t size;
            uint16_t s = 0;

            if (v[i] == nullptr)
            {
                alignBuffer<uint16_t>();
                if (validate && m_bufferLen < m_written + sizeof(uint16_t))
                {
                    outOfMemoryErrorMessage(sizeof(uint16_t));
                    return false;
                }

                reinterpret_cast<uint16_t*>(m_buffer + m_written)[0] = 0;
                m_written += sizeof(uint16_t);
                continue;
            }

            // this might silently truncate if a really long string is passed
            size = strlen(v[i]) + 1;
            s = uint16_t(CARB_MIN(size_t(UINT16_MAX), size));

            alignBuffer<decltype(s)>();
            if (validate && m_bufferLen < m_written + sizeof(s) + s)
            {
                outOfMemoryErrorMessage(sizeof(s) + s);
                return false;
            }

            reinterpret_cast<decltype(s)*>(m_buffer + m_written)[0] = s;
            m_written += sizeof(s);

            memcpy(m_buffer + m_written, v[i], s);
            m_written += s;
        }
        return true;
    }

    /** Copy an array of data into the buffer.
     *  @param[in] v   The array of values to copy.
     *                 This may also be a string.
     *                 This may be nullptr if @p len is 0.
     *  @param[in] len The number of elements in array @p v.
     *                 If this is a string, this length *includes* the null terminator.
     *  @returns `true` if the value was successfully written.
     *  @returns `false` if the blob ran out of buffer space.
     */
    template <typename T>
    bool copy(T* v, uint16_t len)
    {
        static_assert(std::is_arithmetic<T>::value, "this is only valid for primitive types");

        CARB_ASSERT(v != nullptr || len == 0);

        alignBuffer<decltype(len)>();
        if (validate && m_bufferLen < m_written + sizeof(len))
        {
            outOfMemoryErrorMessage(sizeof(len));
            return false;
        }

        reinterpret_cast<decltype(len)*>(m_buffer + m_written)[0] = len;
        m_written += sizeof(len);

        if (len == 0)
            return true;

        alignBuffer<T>();
        if (validate && m_bufferLen < m_written + sizeof(T) * len)
        {
            outOfMemoryErrorMessage(sizeof(T) * len);
            return false;
        }

        memcpy(m_buffer + m_written, v, sizeof(T) * len);
        m_written += sizeof(T) * len;
        return true;
    }

    /** Copy a @ref StringView into the blob.
     *  @param[in] v The @ref StringView to copy in.
     *  @returns `true` if the value was successfully written.
     *  @returns `false` if the blob ran out of buffer space.
     */
    bool copy(const StringView& v)
    {
        uint16_t len = uint16_t(CARB_MIN(size_t(UINT16_MAX), v.length() + 1));
        alignBuffer<decltype(len)>();
        if (validate && m_bufferLen < m_written + sizeof(len))
        {
            outOfMemoryErrorMessage(sizeof(len));
            return false;
        }

        reinterpret_cast<decltype(len)*>(m_buffer + m_written)[0] = len;
        m_written += sizeof(len);

        if (len == 0)
            return true;

        if (validate && m_bufferLen < m_written + len)
        {
            outOfMemoryErrorMessage(len);
            return false;
        }

        // the string view may not be null terminated, so we need to write the
        // terminator separately
        if (len > 1)
        {
            memcpy(m_buffer + m_written, v.data(), len - 1);
            m_written += len - 1;
        }

        m_buffer[m_written++] = '\0';
        return true;
    }

    /** Copy a @ref StringView into the buffer.
     *  @param[in] v The string view to copy.
     */
    bool copy(StringView& v)
    {
        return copy(static_cast<const StringView&>(v));
    }

    /** Copy an array of data into the buffer.
     *  @param[in] v         The array of values to copy.
     *                       This may also be a string.
     *                       This may not be nullptr because that would imply
     *                       @p fixedLen is 0, which is not a valid use case.
     *  @param[in] actualLen The length of array @p v.
     *                       This must be less than or equal to @p fixedLen.
     *  @param[in] fixedLen  The fixed length as specified by your data schema.
     *                       If this is greater than @p actualLen, then the
     *                       excess size at the end of the array will be zeroed.
     *  @returns `true` if the value was successfully written.
     *  @returns `false` if the blob ran out of buffer space.
     */
    template <typename T>
    bool copy(T* v, uint16_t actualLen, uint16_t fixedLen)
    {
        const size_t total = sizeof(T) * fixedLen;
        const size_t written = sizeof(T) * actualLen;

        static_assert(std::is_arithmetic<T>::value, "this is only valid for primitive types");
        CARB_ASSERT(v != nullptr);
        CARB_ASSERT(fixedLen >= actualLen);

        alignBuffer<T>();
        if (validate && m_bufferLen < m_written + total)
        {
            outOfMemoryErrorMessage(total);
            return false;
        }

        // write out the actual values
        memcpy(m_buffer + m_written, v, written);

        // zero the padding at the end
        memset(m_buffer + m_written + written, 0, total - written);

        m_written += total;
        return true;
    }

    /** Align the buffer for a given type.
     *  @remarks This will advance the buffer so that the next write is aligned
     *           for the specified type.
     */
    template <typename T>
    void alignBuffer()
    {
        size_t next = BinaryBlobSizeCalculator::alignOffset<T>(m_written);

        // there's no strict requirement for the padding to be 0
        if (validate)
            memset(m_buffer + m_written, 0, next - m_written);

        m_written = next;
    }

private:
    void outOfMemoryErrorMessage(size_t size)
    {
        char tmp[256];
        carb::extras::formatString(tmp, sizeof(tmp),
                                   "hit end of buffer while writing"
                                   " (tried to write %zu bytes, with %zd available)",
                                   size, m_bufferLen - m_written);
        onValidationError(tmp);
    }

    /** The buffer being written. */
    uint8_t* m_buffer = nullptr;

    /** The length of @p m_buffer. */
    size_t m_bufferLen = 0;

    /** The amount that has been written to @p m_buffer. */
    size_t m_written = 0;
};

/** @{
 *  Constants to make the template specialization more readable.
 */
static constexpr bool kBlobReaderValidate = true;
static constexpr bool kBlobReaderNoValidate = false;
/** @} */

/** A class to read binary blobs produced by the @ref BlobWriter.
 *  You'll need some sort of external schema or fixed layout to be able to read
 *  the blob.
 *
 *  @param validate If this parameter is true, then the read position will
 *                  be tracked while reading the blob and attempting to read
 *                  past the end of the buffer will cause methods to fail.
 *                  Note that a message will not be logged because this class
 *                  is used in the log system.
 *                  This is needed for the tests.
 */
template <bool validate = false, OnBlobWriterValidationErrorFunc onValidationError = ignoreBlobWriterValidationError>
class BlobReader
{
public:
    /** The version of binary blob ABI that this reader was built to read. */
    static constexpr uint32_t kVersion = 0;

    /** Constructor.
     *  @param[in] blob     The buffer to read from.
     *  @param[in] blobSize The length of @p buffer.
     */
    BlobReader(const void* blob, size_t blobSize)
    {
        CARB_ASSERT(blob != nullptr || blobSize == 0);
        m_buffer = static_cast<const uint8_t*>(blob);
        m_bufferLen = blobSize;
    }

    /** read a primitive type element out of the buffer.
     *  @param[out] out The value will be written to this.
     *                  This may not be nullptr.
     *  @returns `true` if the value was successfully read.
     *  @returns `false` if the end of the buffer was reached.
     */
    template <typename T>
    bool read(T* out)
    {
        static_assert(std::is_arithmetic<T>::value, "this is only valid for primitive types");
        CARB_ASSERT(out != nullptr);

        alignBuffer<T>();
        if (validate && m_bufferLen < m_read + sizeof(T))
        {
            outOfMemoryErrorMessage(sizeof(T));
            return false;
        }

        *out = reinterpret_cast<const T*>(m_buffer + m_read)[0];
        m_read += sizeof(T);
        return true;
    }

    /** Read an array of strings out of the buffer.
     *  @param[out] out    Receives the array of strings from the blob if @p maxLen is not 0.
     *                     This may be nullptr if @p maxLen is 0.
     *                     Note that the strings pointed to will point into the
     *                     data blob that's being read from.
     *  @param[out] outLen Receives the length of the output array.
     *                     This may not be nullptr.
     *  @param[in]  maxLen The maximum length that can be read into @p out in this call.
     *                     If this is set to 0, then the array length with be
     *                     read into @p outLen and the buffer pointer will not
     *                     increment to point at the next member, so that an
     *                     array can be allocated and read in the next call to
     *                     this function.
     *                     An exception to this is the case where @p outLen
     *                     is set to 0; the buffer pointer will be incremented
     *                     to point at the next member in this case.
     *
     *  @returns `true` if the value was successfully read.
     *  @returns `false` if the end of the buffer was reached.
     */
    bool read(const char** out, uint16_t* outLen, uint16_t maxLen)
    {
        uint16_t len = 0;

        alignBuffer<decltype(len)>();
        if (validate && m_bufferLen < m_read + sizeof(uint16_t))
        {
            outOfMemoryErrorMessage(sizeof(uint16_t));
            return false;
        }

        len = reinterpret_cast<const decltype(len)*>(m_buffer + m_read)[0];
        *outLen = len;
        if (maxLen == 0 && len != 0)
            return true;

        m_read += sizeof(len);
        if (validate && len > maxLen)
        {
            char tmp[256];
            carb::extras::formatString(tmp, sizeof(tmp),
                                       "buffer is too small to read the data"
                                       " (length = %" PRIu16 ", needed = %" PRIu16 ")",
                                       maxLen, len);
            onValidationError(tmp);
        }

        for (uint16_t i = 0; i < len; i++)
        {
            uint16_t s = 0;

            alignBuffer<decltype(s)>();
            if (validate && m_bufferLen < m_read + sizeof(uint16_t))
            {
                outOfMemoryErrorMessage(sizeof(uint16_t));
                return false;
            }

            s = reinterpret_cast<const decltype(s)*>(m_buffer + m_read)[0];
            m_read += sizeof(len);
            if (validate && m_bufferLen < m_read + s)
            {
                outOfMemoryErrorMessage(s);
                return false;
            }

            if (s == 0)
            {
                out[i] = nullptr;
                continue;
            }

            out[i] = reinterpret_cast<const char*>(m_buffer + m_read);

            m_read += s;
        }

        return true;
    }

    /** Read a string out of the buffer.
     *  @param[out] out    Receives the array from the binary blob.
     *                     This will be pointing to data from within the blob.
     *                     This may not be nullptr.
     *  @param[out] outLen Receives the length of the array that was read.
     *                     This includes the null terminator if this was as string.
     *                     This may not be nullptr.
     *
     *  @returns `true` if the value was successfully read.
     *  @returns `false` if the end of the buffer was reached.
     */
    template <typename T>
    bool read(const T** out, uint16_t* outLen)
    {
        CARB_ASSERT(out != nullptr);
        CARB_ASSERT(outLen != nullptr);

        alignBuffer<uint16_t>();
        if (validate && m_bufferLen < m_read + sizeof(uint16_t))
        {
            outOfMemoryErrorMessage(sizeof(uint16_t));
            return false;
        }

        *outLen = reinterpret_cast<const uint16_t*>(m_buffer + m_read)[0];
        m_read += sizeof(*outLen);

        if (*outLen == 0)
        {
            *out = nullptr;
            return true;
        }

        alignBuffer<T>();
        if (validate && m_bufferLen < m_read + *outLen)
        {
            outOfMemoryErrorMessage(*outLen);
            return false;
        }

        *out = reinterpret_cast<const T*>(m_buffer + m_read);
        m_read += *outLen * sizeof(T);
        return true;
    }

    /** Read a fixed length array out of the buffer.
     *  @param[out] out    Receives the array from the binary blob.
     *                     This will be pointing to data from within the blob.
     *                     This may not be nullptr.
     *  @param[out] fixedLen The fixed length of the array/string.
     *                       This should not be 0.
     *
     *  @returns `true` if the value was successfully read.
     *  @returns `false` if the end of the buffer was reached.
     */
    template <typename T>
    bool read(const T** out, uint16_t fixedLen)
    {
        CARB_ASSERT(out != nullptr);

        alignBuffer<T>();
        if (validate && m_bufferLen < m_read + fixedLen)
        {
            outOfMemoryErrorMessage(fixedLen);
            return false;
        }

        *out = reinterpret_cast<const T*>(m_buffer + m_read);
        m_read += fixedLen * sizeof(T);
        return true;
    }

    /** Align the buffer for a given type.
     *  @remarks This will advance the buffer so that the next write is aligned
     *           for the specified type.
     */
    template <typename T>
    void alignBuffer()
    {
        m_read = BinaryBlobSizeCalculator::alignOffset<T>(m_read);
    }

protected:
    /** Send an error message to the onValidationError callback when the end of the buffer has been reached.
     *  @param[in] size The size that was requested that would have extended past the end of the buffer.
     */
    void outOfMemoryErrorMessage(size_t size)
    {
        char tmp[256];
        carb::extras::formatString(tmp, sizeof(tmp),
                                   "hit end of buffer while reading"
                                   " (tried to read %zu bytes, with %zd available)",
                                   size, m_bufferLen - m_read);
        onValidationError(tmp);
    }

    /** The buffer being written. */
    const uint8_t* m_buffer = nullptr;

    /** The length of @p m_buffer. */
    size_t m_bufferLen = 0;

    /** The amount that has been read from @p m_buffer. */
    size_t m_read = 0;
};

} // namespace structuredlog
} // namespace omni
