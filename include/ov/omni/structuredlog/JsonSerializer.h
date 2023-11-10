// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Module for manually serializing JSON data with low performance overhead.
 */
#pragma once

#include "../../carb/extras/StringSafe.h"
#include "../../carb/extras/Utf8Parser.h"
#include "../../carb/extras/Base64.h"

#include <stdint.h>
#include <float.h>

namespace omni
{
namespace structuredlog
{

/** An interface for consuming the output JSON from the @ref JsonSerializer. */
class JsonConsumer
{
public:
    virtual ~JsonConsumer()
    {
    }

    /** The function that will consume strings of JSON.
     *  @param[in] json    The string of JSON.
     *                     This string's lifetime ends after the return from this call.
     *  @param[in] jsonLen The length of @p json, excluding the null terminator.
     *                     The null terminator will be included in the length
     *                     when the final call to this writes out a null terminator.
     *                     It is possible this may be 0 in some edge cases.
     *                     It's possible that @p jsonLen may be used to refer to
     *                     a substring of @p json.
     *  @remarks This will be called when the @ref JsonSerializer wants to write
     *           something to the output.
     *           The @ref JsonSerializer will write very small units of text to
     *           this function, so the implementation should plan accordingly.
     */
    virtual void consume(const char* json, size_t jsonLen) noexcept = 0;

    /** Terminate the output, if needed.
     *  @remarks This will be called to ensure the output is null terminated.
     */
    virtual void terminate() noexcept = 0;
};

/** An implementation of @ref JsonConsumer that just counts the length of the
 *  output string.
 *  You may serialize some JSON through this to find the required buffer length,
 *  then allocate the buffer and serialize the JSON again with @ref JsonPrinter
 *  using that allocated buffer.
 */
class JsonLengthCounter : public JsonConsumer
{
public:
    void consume(const char* json, size_t jsonLen) noexcept override
    {
        CARB_UNUSED(json);
        m_count += jsonLen;
    }

    void terminate() noexcept override
    {
        m_count++;
    }

    /** Get the number of bytes that have been consumed so far.
     *  @returns the number of bytes that have been consumed so far.
     */
    size_t getcount() noexcept
    {
        return m_count;
    }

private:
    size_t m_count = 0;
};

/** An implementation of @ref JsonConsumer that just prints to a fixed string. */
class JsonPrinter : public JsonConsumer
{
public:
    JsonPrinter()
    {
        reset(nullptr, 0);
    }

    /** Create a printer from a fixed string buffer.
     *  @param[out] output    The instance will write to this buffer.
     *  @param[in]  outputLen The number of bytes that can be written to @p output.
     */
    JsonPrinter(char* output, size_t outputLen) noexcept
    {
        reset(output, outputLen);
    }

    /** Reinitialize the printer with a new buffer.
     *  @param[out] output    The instance will write to this buffer.
     *  @param[in]  outputLen The number of bytes that can be written to @p output.
     */
    void reset(char* output, size_t outputLen) noexcept
    {
        m_output = (outputLen == 0) ? nullptr : output;
        m_left = outputLen;
        m_overflowed = false;
    }

    /** Write a string into the buffer.
     *  @param[in] json The data to write into the buffer.
     *  @param[in] jsonLen The length of @p json, excluding any null terminator.
     */
    void consume(const char* json, size_t jsonLen) noexcept override
    {
        size_t w = CARB_MIN(m_left, jsonLen);
        memcpy(m_output, json, w);
        m_left -= w;
        m_output += w;
        m_overflowed = m_overflowed || w < jsonLen;
    }

    void terminate() noexcept override
    {
        if (m_output != nullptr)
        {
            if (m_left == 0)
            {
                m_output[-1] = '\0';
            }
            else
            {
                m_output[0] = '\0';
                m_output++;
                m_left--;
            }
        }
    }

    /** Check whether more data was printed than would fit in the printer's buffer.
     *  @returns `true` if the buffer was too small to fit all the consumed data.
     *  @returns `false` if the buffer was able to fit all the consumed data.
     */
    bool hasOverflowed() noexcept
    {
        return m_overflowed;
    }

    /** Get the pointer to the next char to be written in the buffer.
     *  @returns The pointer to the next character to be written.
     *           If the buffer has overflowed, this will point past the end of
     *           the buffer.
     */
    char* getNextChar() const noexcept
    {
        return m_output;
    }

private:
    char* m_output;
    size_t m_left;
    bool m_overflowed = false;
};

/** Anonymous namespace for a helper function */
namespace
{
void ignoreJsonSerializerValidationError(const char* s) noexcept
{
    CARB_UNUSED(s);
}
} // namespace

/** The prototype of the function to call when a validation error occurs. */
using OnValidationErrorFunc = void (*)(const char*);

/** A utility that allows you to easily encode JSON data.
 *  This class won't allocate any memory unless you use an excessive number of scopes.
 *
 *  @param validate If this is set to true, methods will return false when an
 *                  invalid operation is performed.
 *                  onValidationError is used for logging because this struct
 *                  is used in the logging system, so OMNI_LOG_* cannot be
 *                  directly called from within this class.
 *                  If this is set to false, methods will assume that all calls
 *                  will produce valid JSON data. Invalid calls will write out
 *                  invalid JSON data. Methods will only return false if an
 *                  unavoidable check failed.
 *
 *  @tparam prettyPrint If this is set to true, the output will be pretty-printed.
 *                      If this is set to false, the output will have no added white space.
 *
 *  @tparam onValidationError This is a callback that gets executed when a validation
 *                            error is triggered, so logging can be called.
 */
template <bool validate = false, bool prettyPrint = false, OnValidationErrorFunc onValidationError = ignoreJsonSerializerValidationError>
class JsonSerializer
{
public:
    /** The function that will be called when a validation error occurs. */
    OnValidationErrorFunc m_onValidationError = onValidationError;

    /** Constructor.
     *  @param[in] consumer  The object that will consume the output JSON data.
     *                       This may not be nullptr.
     *  @param[in] indentLen The number of spaces to indent by when pretty printing is enabled.
     */
    JsonSerializer(JsonConsumer* consumer, size_t indentLen = 4) noexcept
    {
        CARB_ASSERT(consumer != nullptr);
        m_consumer = consumer;
        m_indentLen = indentLen;
    }

    ~JsonSerializer()
    {
        // it starts to use heap memory at this point
        if (m_scopes != m_scopesBuffer)
            free(m_scopes);
    }

    /** Reset the internal state back to where it was after construction. */
    void reset()
    {
        m_scopesTop = 0;
        m_firstInScope = true;
        m_hasKey = false;
        m_firstPrint = true;
        m_indentTotal = 0;
    }

    /** Write out a JSON key for an object property.
     *  @param[in] key    The string value for the key.
     *                    This can be nullptr.
     *  @param[in] keyLen The length of @p key, excluding the null terminator.
     *  @returns whether or not validation succeeded.
     */
    bool writeKey(const char* key, size_t keyLen) noexcept
    {
        if (validate)
        {
            if (CARB_UNLIKELY(getCurrentScope() != ScopeType::eObject))
            {
                char tmp[256];
                carb::extras::formatString(tmp, sizeof(tmp),
                                           "attempted to write a key outside an object"
                                           " {key name = '%s', len = %zu}",
                                           key, keyLen);
                onValidationError(tmp);
                return false;
            }

            if (CARB_UNLIKELY(m_hasKey))
            {
                char tmp[256];
                carb::extras::formatString(tmp, sizeof(tmp),
                                           "attempted to write out two key names in a row"
                                           " {key name = '%s', len = %zu}",
                                           key, keyLen);
                onValidationError(tmp);
                return false;
            }
        }

        if (!m_firstInScope)
            m_consumer->consume(",", 1);

        prettyPrintHook();
        m_consumer->consume("\"", 1);
        if (key != nullptr)
            m_consumer->consume(key, keyLen);
        m_consumer->consume("\":", 2);

        m_firstInScope = false;

        if (validate)
            m_hasKey = true;

        return true;
    }

    /** Write out a JSON key for an object property.
     *  @param[in] key The key name for this property.
     *                 This may be nullptr.
     *  @returns whether or not validation succeeded.
     */
    bool writeKey(const char* key) noexcept
    {
        return writeKey(key, key == nullptr ? 0 : strlen(key));
    }

    /** Write out a JSON null value.
     *  @returns whether or not validation succeeded.
     */
    bool writeValue() noexcept
    {
        if (CARB_UNLIKELY(!writeValuePrologue()))
            return false;

        prettyPrintValueHook();
        m_consumer->consume("null", sizeof("null") - 1);

        if (validate)
            m_hasKey = false;

        return true;
    }

    /** Write out a JSON boolean value.
     *  @param[in] value The boolean value.
     *  @returns whether or not validation succeeded.
     */
    bool writeValue(bool value) noexcept
    {
        const char* val = value ? "true" : "false";
        size_t len = (value ? sizeof("true") : sizeof("false")) - 1;

        if (CARB_UNLIKELY(!writeValuePrologue()))
            return false;

        prettyPrintValueHook();
        m_consumer->consume(val, len);

        if (validate)
            m_hasKey = false;

        return true;
    }

    /** Write out a JSON integer value.
     *  @param[in] value The integer value.
     *  @returns whether or not validation succeeded.
     */
    bool writeValue(int32_t value) noexcept
    {
        char buffer[32];
        size_t i = 0;

        if (CARB_UNLIKELY(!writeValuePrologue()))
            return false;

        i = carb::extras::formatString(buffer, CARB_COUNTOF(buffer), "%" PRId32, value);
        prettyPrintValueHook();
        m_consumer->consume(buffer, i);

        if (validate)
            m_hasKey = false;

        return true;
    }

    /** Write out a JSON integer value.
     *  @param[in] value The integer value.
     *  @returns whether or not validation succeeded.
     */
    bool writeValue(uint32_t value) noexcept
    {
        char buffer[32];
        size_t i = 0;

        if (CARB_UNLIKELY(!writeValuePrologue()))
            return false;

        i = carb::extras::formatString(buffer, CARB_COUNTOF(buffer), "%" PRIu32, value);
        prettyPrintValueHook();
        m_consumer->consume(buffer, i);

        if (validate)
            m_hasKey = false;

        return true;
    }

    /** Write out a JSON integer value.
     *  @param[in] value The integer value.
     *  @returns whether or not validation succeeded.
     *  @note 64 bit integers are stored as double precision floats in JavaScript's
     *        JSON library, so a JSON library with BigInt support should be
     *        used instead when reading 64 bit numbers.
     */
    bool writeValue(int64_t value) noexcept
    {
        char buffer[32];
        size_t i = 0;

        if (CARB_UNLIKELY(!writeValuePrologue()))
            return false;

        i = carb::extras::formatString(buffer, CARB_COUNTOF(buffer), "%" PRId64, value);

        prettyPrintValueHook();
        m_consumer->consume(buffer, i);

        if (validate)
            m_hasKey = false;

        return true;
    }

    /** Write out a JSON integer value.
     *  @param[in] value The integer value.
     *  @returns whether or not validation succeeded.
     *  @note 64 bit integers are stored as double precision floats in JavaScript's
     *        JSON library, so a JSON library with BigInt support should be
     *        used instead when reading 64 bit numbers.
     */
    bool writeValue(uint64_t value) noexcept
    {
        char buffer[32];
        size_t i = 0;

        if (CARB_UNLIKELY(!writeValuePrologue()))
            return false;

        i = carb::extras::formatString(buffer, CARB_COUNTOF(buffer), "%" PRIu64, value);

        prettyPrintValueHook();
        m_consumer->consume(buffer, i);

        if (validate)
            m_hasKey = false;

        return true;
    }

    /** Write out a JSON double (aka number) value.
     *  @param[in] value The double value.
     *  @returns whether or not validation succeeded.
     */
    bool writeValue(double value) noexcept
    {
        char buffer[32];
        size_t i = 0;

        if (CARB_UNLIKELY(!writeValuePrologue()))
            return false;

        i = carb::extras::formatString(
            buffer, CARB_COUNTOF(buffer), "%.*g", std::numeric_limits<double>::max_digits10, value);
        prettyPrintValueHook();
        m_consumer->consume(buffer, i);

        if (validate)
            m_hasKey = false;

        return true;
    }

    /** Write out a JSON float (aka number) value.
     *  @param[in] value The double value.
     *  @returns whether or not validation succeeded.
     */
    bool writeValue(float value) noexcept
    {
        return writeValue(double(value));
    }

    /** Write out a JSON string value.
     *  @param[in] value The string value.
     *                   This can be nullptr if @p len is 0.
     *  @param[in] len   The length of @p value, excluding the null terminator.
     *  @returns whether or not validation succeeded.
     */
    bool writeValue(const char* value, size_t len) noexcept
    {
        size_t last = 0;

        if (CARB_UNLIKELY(!writeValuePrologue()))
            return false;

        prettyPrintValueHook();
        m_consumer->consume("\"", 1);
        for (size_t i = 0; i < len;)
        {
            const carb::extras::Utf8Parser::CodeByte* next = carb::extras::Utf8Parser::nextCodePoint(value + i, len - i);
            if (next == nullptr)
            {
                m_consumer->consume(value + last, i - last);
                m_consumer->consume("\\u0000", 6);
                i++;
                last = i;
                continue;
            }

            // early out for non-escape characters
            // multi-byte characters never need to be escaped
            if (size_t(next - value) > i + 1 || (value[i] > 0x1F && value[i] != '"' && value[i] != '\\'))
            {
                i = next - value;
                continue;
            }

            m_consumer->consume(value + last, i - last);
            switch (value[i])
            {
                case '"':
                    m_consumer->consume("\\\"", 2);
                    break;

                case '\\':
                    m_consumer->consume("\\\\", 2);
                    break;

                case '\b':
                    m_consumer->consume("\\b", 2);
                    break;

                case '\f':
                    m_consumer->consume("\\f", 2);
                    break;

                case '\n':
                    m_consumer->consume("\\n", 2);
                    break;

                case '\r':
                    m_consumer->consume("\\r", 2);
                    break;

                case '\t':
                    m_consumer->consume("\\t", 2);
                    break;

                default:
                {
                    char tmp[] = "\\u0000";
                    tmp[4] = getHexChar(uint8_t(value[i]) >> 4);
                    tmp[5] = getHexChar(value[i] & 0x0F);
                    m_consumer->consume(tmp, CARB_COUNTOF(tmp) - 1);
                }
                break;
            }
            i++;
            last = i;
        }

        if (len > last)
            m_consumer->consume(value + last, len - last);

        m_consumer->consume("\"", 1);

        if (validate)
            m_hasKey = false;

        return true;
    }

    /** Write out a JSON string value.
     *  @param[in] value The string value.
     *                   This can be nullptr.
     *  @returns whether or not validation succeeded.
     */
    bool writeValue(const char* value) noexcept
    {
        return writeValue(value, value == nullptr ? 0 : strlen(value));
    }

    /** Write a binary blob into the output JSON as a base64 encoded string.
     *  @param[in] value_ The binary blob to write in.
     *  @param[in] size   The number of bytes of data in @p value_.
     *  @returns whether or not validation succeeded.
     *  @remarks This will take the input string and encode it in base64, then
     *           store that as base64 data in a string.
     */
    bool writeValueWithBase64Encoding(const void* value_, size_t size)
    {
        char buffer[4096];
        const size_t readSize = carb::extras::Base64::getEncodeInputSize(sizeof(buffer));
        const uint8_t* value = static_cast<const uint8_t*>(value_);

        if (CARB_UNLIKELY(!writeValuePrologue()))
            return false;

        m_consumer->consume("\"", 1);
        for (size_t i = 0; i < size; i += readSize)
        {
            size_t written = m_base64Encoder.encode(value + i, CARB_MIN(readSize, size - i), buffer, sizeof(buffer));
            m_consumer->consume(buffer, written);
        }
        m_consumer->consume("\"", 1);

        if (validate)
            m_hasKey = false;

        return true;
    }

    /** Begin a JSON array.
     *  @returns whether or not validation succeeded.
     *  @returns false if a memory allocation failed.
     */
    bool openArray() noexcept
    {
        if (CARB_UNLIKELY(!writeValuePrologue()))
            return false;

        prettyPrintValueHook();
        m_consumer->consume("[", 1);
        m_firstInScope = true;
        if (!pushScope(ScopeType::eArray))
            return false;

        prettyPrintOpenScope();
        if (validate)
            m_hasKey = false;

        return true;
    }

    /** Finish writing a JSON array.
     *  @returns whether or not validation succeeded.
     */
    bool closeArray() noexcept
    {
        if (validate && CARB_UNLIKELY(getCurrentScope() != ScopeType::eArray))
        {
            onValidationError("attempted to close an array that was never opened");
            return false;
        }

        popScope();
        prettyPrintCloseScope();
        prettyPrintHook();
        m_consumer->consume("]", 1);
        m_firstInScope = false;

        if (validate)
            m_hasKey = false;

        return true;
    }

    /** Begin a JSON object.
     *  @returns whether or not validation succeeded.
     *  @returns false if a memory allocation failed.
     */
    bool openObject() noexcept
    {
        if (CARB_UNLIKELY(!writeValuePrologue()))
            return false;

        prettyPrintValueHook();
        m_consumer->consume("{", 1);
        m_firstInScope = true;
        pushScope(ScopeType::eObject);

        prettyPrintOpenScope();
        if (validate)
            m_hasKey = false;

        return true;
    }

    /** Finish writing a JSON object.
     *  @returns whether or not validation succeeded.
     */
    bool closeObject() noexcept
    {
        if (validate && CARB_UNLIKELY(getCurrentScope() != ScopeType::eObject))
        {
            onValidationError("attempted to close an object that was never opened");
            return false;
        }

        popScope();
        prettyPrintCloseScope();
        prettyPrintHook();
        m_consumer->consume("}", 1);
        m_firstInScope = false;

        if (validate)
            m_hasKey = false;

        return true;
    }

    /** Finish writing your JSON.
     *  @returns whether or not validation succeeded.
     */
    bool finish() noexcept
    {
        bool result = true;

        // check whether we are ending in the middle of an object/array
        if (validate && getCurrentScope() != ScopeType::eGlobal)
        {
            char tmp[256];
            carb::extras::formatString(tmp, sizeof(tmp), "finished writing in the middle of an %s",
                                       getCurrentScope() == ScopeType::eArray ? "array" : "object");
            onValidationError(tmp);
            result = false;
        }

        if (prettyPrint)
            m_consumer->consume("\n", 1);

        m_consumer->terminate();

        return result;
    }

private:
    /** The type of a scope in the scope stack. */
    enum class ScopeType : uint8_t
    {
        eGlobal, /**< global scope (not in anything). */
        eArray, /**< JSON array type. (e.g. []) */
        eObject, /**< JSON object type. (e.g. {}) */
    };

    /** A hook used before JSON elements to perform pretty print formatting. */
    void prettyPrintHook() noexcept
    {
        if (prettyPrint)
        {
            const size_t s = CARB_COUNTOF(m_indent) - 1;

            if (!m_firstPrint)
                m_consumer->consume("\n", 1);
            m_firstPrint = false;

            for (size_t i = s; i <= m_indentTotal; i += s)
                m_consumer->consume(m_indent, s);
            m_consumer->consume(m_indent, m_indentTotal % s);
        }
    }

    /** A hook used before printing a value element to perform pretty print formatting. */
    void prettyPrintValueHook() noexcept
    {
        if (prettyPrint)
        {
            if (getCurrentScope() != ScopeType::eObject)
                return prettyPrintHook();
            else
                /* if it's in an object, a key preceded this so this should be on the same line */
                m_consumer->consume(" ", 1);
        }
    }

    /** Track when a scope was opened for pretty print indenting. */
    void prettyPrintOpenScope() noexcept
    {
        if (prettyPrint)
            m_indentTotal += m_indentLen;
    }

    /** Track when a scope was closed for pretty print indenting. */
    void prettyPrintCloseScope() noexcept
    {
        if (prettyPrint)
            m_indentTotal -= m_indentLen;
    }

    /** A common prologue to writeValue() functions.
     *  @returns Whether validation succeeded.
     */
    inline bool writeValuePrologue() noexcept
    {
        if (validate)
        {
            // the global scope is only allowed to have one item in it
            if (CARB_UNLIKELY(getCurrentScope() == ScopeType::eGlobal && !m_firstInScope))
            {
                onValidationError("attempted to put multiple values into the global scope");
                return false;
            }

            // if we're in an object, a key needs to have been written before each value
            if (CARB_UNLIKELY(getCurrentScope() == ScopeType::eObject && !m_hasKey))
            {
                onValidationError("attempted to write a value without a key inside an object");
                return false;
            }
        }

        if (getCurrentScope() == ScopeType::eArray && !m_firstInScope)
            m_consumer->consume(",", 1);

        m_firstInScope = false;

        return true;
    }

    bool pushScope(ScopeType s) noexcept
    {
        if (m_scopesTop == m_scopesLen)
        {
            size_t newLen = m_scopesTop + 64;
            size_t size = sizeof(*m_scopes) * newLen;
            void* tmp;
            if (m_scopes == m_scopesBuffer)
                tmp = malloc(size);
            else
                tmp = realloc(m_scopes, size);
            if (tmp == nullptr)
            {
                char log[256];
                carb::extras::formatString(log, sizeof(log), "failed to allocate %zu bytes", size);
                onValidationError(log);
                return false;
            }
            if (m_scopes == m_scopesBuffer)
                memcpy(tmp, m_scopes, sizeof(*m_scopes) * m_scopesLen);
            m_scopes = static_cast<ScopeType*>(tmp);
            m_scopesLen = newLen;
        }
        m_scopes[m_scopesTop++] = s;
        return true;
    }

    void popScope() noexcept
    {
        m_scopesTop--;
    }

    ScopeType getCurrentScope() noexcept
    {
        if (m_scopesTop == 0)
            return ScopeType::eGlobal;

        return m_scopes[m_scopesTop - 1];
    }

    char getHexChar(uint8_t c) noexcept
    {
        char lookup[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

        CARB_ASSERT(c < CARB_COUNTOF(lookup));
        return lookup[c];
    }

    ScopeType m_scopesBuffer[8];
    ScopeType* m_scopes = m_scopesBuffer;
    size_t m_scopesLen = CARB_COUNTOF(m_scopesBuffer);
    size_t m_scopesTop = 0;

    /** The consumer of the output JSON data. */
    JsonConsumer* m_consumer;

    /** This flag is the current key/value being written is the first one inside
     *  the current scope (this decides comma placement). */
    bool m_firstInScope = true;

    /** This is set when a key has been specified.
     *  This is done for validation.
     */
    bool m_hasKey = false;

    /** This is true when the first character has not been printed yet.
     *  This is used for pretty printing.
     */
    bool m_firstPrint = true;

    /** The indent buffer used for pretty printing. */
    char m_indent[33] = "                                ";

    /** The length into m_indent to print for the pretty-printing indent. */
    size_t m_indentTotal = 0;

    /** The length of an individual indent when pretty-printing. */
    size_t m_indentLen = 4;

    /** The base64 encoder. This is just cached here to improve performance. */
    carb::extras::Base64 m_base64Encoder;
};

} // namespace structuredlog
} // namespace omni
