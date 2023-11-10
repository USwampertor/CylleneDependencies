// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Module for Serializing the @ref omni::structuredlog::JsonNode tree structures.
 */
#pragma once
#include "JsonSerializer.h"
#include "BinarySerializer.h"
#include "JsonTree.h"

namespace omni
{
namespace structuredlog
{

/** Default value for the onValidationError template parameter.
 *  @param[in] s The validation error message. This is ignored.
 */
static inline void ignoreJsonTreeSerializerValidationError(const char* s)
{
    CARB_UNUSED(s);
}

/** Serialize a scalar type from a JSON tree.
 *  @param[inout] serial   The JSON serializer to serialize the data into.
 *  @param[in]    root     The node in the schema that represents this scalar value.
 *  @param[in]    constVal The constant value from the data union of @p root.
 *                         This is only read if root is marked as constant.
 *  @param[inout] reader   The blob reader, which will be read if @p root is not
 *                         marked as constant.
 *
 *  @returns `true` if no validation error occurred.
 *  @returns `false` if any form of validation error occurred.
 */
template <bool validate = false,
          typename T,
          typename JsonSerializerType = JsonSerializer<false, false, ignoreJsonTreeSerializerValidationError>,
          typename JsonNodeType = JsonNode,
          typename BlobReaderType = BlobReader<false, ignoreJsonTreeSerializerValidationError>>
static inline bool serializeScalar(JsonSerializerType* serial, const JsonNodeType* root, T constVal, BlobReaderType* reader)
{
    if ((root->flags & JsonNode::fFlagConst) != 0)
    {
        return serial->writeValue(constVal);
    }
    else
    {
        T b = {};
        bool result = reader->read(&b);
        if (validate && !result)
            return false;

        return serial->writeValue(b);
    }
}

/** Serialize an array type from a JSON tree.
 *  @param[inout] serial   The JSON serializer to serialize the data into.
 *  @param[in]    root     The node in the schema that represents this scalar value.
 *  @param[in]    constVal The constant array value from the data union of @p root.
 *                         This is only read if root is marked as constant;
 *                         in that case, this is of length @p root->len.
 *  @param[inout] reader   The blob reader, which will be read if @p root is not
 *                         marked as constant.
 *
 *  @returns `true` if no validation error occurred.
 *  @returns `false` if any form of validation error occurred.
 */
template <bool validate = false,
          typename T,
          typename JsonSerializerType = JsonSerializer<false, false, ignoreJsonTreeSerializerValidationError>,
          typename JsonNodeType = JsonNode,
          typename BlobReaderType = BlobReader<false, ignoreJsonTreeSerializerValidationError>>
static inline bool serializeArray(JsonSerializerType* serial,
                                  const JsonNodeType* root,
                                  const T* constVal,
                                  BlobReaderType* reader)
{
    bool result = true;

    result = serial->openArray();
    if (validate && !result)
        return false;

    if ((root->flags & JsonNode::fFlagConst) != 0)
    {
        for (uint16_t i = 0; i < root->len; i++)
        {
            result = serial->writeValue(constVal[i]);
            if (validate && !result)
                return false;
        }
    }
    else
    {
        const T* b = nullptr;
        uint16_t len = 0;

        if ((root->flags & JsonNode::fFlagFixedLength) != 0)
        {
            len = root->len;
            result = reader->read(&b, len);
            if (validate && !result)
                return false;
        }
        else
        {
            result = reader->read(&b, &len);
            if (validate && !result)
                return false;
        }

        for (uint16_t i = 0; i < len; i++)
        {
            result = serial->writeValue(b[i]);
            if (validate && !result)
                return false;
        }
    }

    return serial->closeArray();
}

template <bool validate = false,
          typename T,
          typename JsonSerializerType = JsonSerializer<false, false, ignoreJsonTreeSerializerValidationError>,
          typename JsonNodeType = JsonNode,
          typename BlobReaderType = BlobReader<false, ignoreJsonTreeSerializerValidationError>>
static inline bool serializeEnum(JsonSerializerType* serial, const JsonNodeType* root, T* enumChoices, BlobReaderType* reader)
{
    JsonNode::EnumBase b = {};
    bool result = reader->read(&b);
    if (validate && !result)
        return false;

    if (b > root->len)
    {
        char tmp[256];
        carb::extras::formatString(tmp, sizeof(tmp),
                                   "enum value is out of range"
                                   " {value = %" PRIu16 ", max = %" PRIu16 "}",
                                   b, root->len);
        serial->m_onValidationError(tmp);
        return false;
    }

    return serial->writeValue(enumChoices[b]);
}

/** Serialize JSON using a @ref JsonNode as the schema and a binary blob to read data.
 *
 *  @remarks This overload uses a @ref BlobReader instead of the binary blob
 *           directly, so that the read position from within the blob can be
 *           tracked across recursive calls.
 *           External code should use the other overload.
 *
 *  @note If you use this overload, you must call @p serial->finish(), since this
 *        is the recursive overload so there's no obvious point to finish at.
 */
template <bool validate = false,
          typename JsonSerializerType = JsonSerializer<false, false, ignoreJsonTreeSerializerValidationError>,
          typename JsonNodeType = JsonNode,
          typename BlobReaderType = BlobReader<false, ignoreJsonTreeSerializerValidationError>>
static inline bool serializeJsonTree(JsonSerializerType* serial, const JsonNodeType* root, BlobReaderType* reader)
{
    bool result = true;

    if (root->name != nullptr)
    {
        result = serial->writeKey(root->name, root->nameLen - 1);
        if (validate && !result)
            return false;
    }

    switch (root->type)
    {
        case NodeType::eNull:
            return serial->writeValue();

        case NodeType::eBool:
            return serializeScalar<validate>(serial, root, root->data.boolVal, reader);

        case NodeType::eInt32:
            return serializeScalar<validate>(serial, root, int32_t(root->data.intVal), reader);

        case NodeType::eUint32:
            return serializeScalar<validate>(serial, root, uint32_t(root->data.uintVal), reader);

        case NodeType::eInt64:
            return serializeScalar<validate>(serial, root, root->data.intVal, reader);

        case NodeType::eUint64:
            return serializeScalar<validate>(serial, root, root->data.uintVal, reader);

        case NodeType::eFloat32:
            return serializeScalar<validate>(serial, root, float(root->data.floatVal), reader);

        case NodeType::eFloat64:
            return serializeScalar<validate>(serial, root, root->data.floatVal, reader);

        case NodeType::eBinary:
            if ((root->flags & JsonNode::fFlagConst) != 0)
            {
                return serial->writeValueWithBase64Encoding(root->data.binaryVal, root->len);
            }
            else
            {
                const uint8_t* b = nullptr;
                uint16_t len = 0;

                if ((root->flags & JsonNode::fFlagFixedLength) != 0)
                {
                    len = root->len;
                    result = reader->read(&b, len);
                }
                else
                {
                    result = reader->read(&b, &len);
                }

                if (validate && !result)
                    return false;

                // null terminator is included in the length
                return serial->writeValueWithBase64Encoding(b, len);
            }

        case NodeType::eBoolArray:
            if ((root->flags & JsonNode::fFlagEnum) != 0)
                return serializeEnum<validate>(serial, root, root->data.boolArrayVal, reader);
            else
                return serializeArray<validate>(serial, root, root->data.boolArrayVal, reader);

        case NodeType::eInt32Array:
            if ((root->flags & JsonNode::fFlagEnum) != 0)
                return serializeEnum<validate>(serial, root, root->data.int32ArrayVal, reader);
            else
                return serializeArray<validate>(serial, root, root->data.int32ArrayVal, reader);

        case NodeType::eUint32Array:
            if ((root->flags & JsonNode::fFlagEnum) != 0)
                return serializeEnum<validate>(serial, root, root->data.uint32ArrayVal, reader);
            else
                return serializeArray<validate>(serial, root, root->data.uint32ArrayVal, reader);

        case NodeType::eInt64Array:
            if ((root->flags & JsonNode::fFlagEnum) != 0)
                return serializeEnum<validate>(serial, root, root->data.int64ArrayVal, reader);
            else
                return serializeArray<validate>(serial, root, root->data.int64ArrayVal, reader);

        case NodeType::eUint64Array:
            if ((root->flags & JsonNode::fFlagEnum) != 0)
                return serializeEnum<validate>(serial, root, root->data.uint64ArrayVal, reader);
            else
                return serializeArray<validate>(serial, root, root->data.uint64ArrayVal, reader);

        case NodeType::eFloat32Array:
            if ((root->flags & JsonNode::fFlagEnum) != 0)
                return serializeEnum<validate>(serial, root, root->data.float32ArrayVal, reader);
            else
                return serializeArray<validate>(serial, root, root->data.float32ArrayVal, reader);

        case NodeType::eFloat64Array:
            if ((root->flags & JsonNode::fFlagEnum) != 0)
                return serializeEnum(serial, root, root->data.float64ArrayVal, reader);
            else
                return serializeArray(serial, root, root->data.float64ArrayVal, reader);

        case NodeType::eString:
            if ((root->flags & JsonNode::fFlagConst) != 0)
            {
                return serial->writeValue(root->data.strVal, (root->len == 0) ? 0 : root->len - 1);
            }
            else
            {
                const char* b = nullptr;
                uint16_t len = 0;

                if ((root->flags & JsonNode::fFlagFixedLength) != 0)
                {
                    len = root->len;
                    result = reader->read(&b, len);
                }
                else
                {
                    result = reader->read(&b, &len);
                }

                if (validate && !result)
                    return false;

                // null terminator is included in the length
                return serial->writeValue(b, (len == 0) ? 0 : len - 1);
            }

        case NodeType::eStringArray:
            if ((root->flags & JsonNode::fFlagEnum) != 0)
                return serializeEnum<validate>(serial, root, root->data.strArrayVal, reader);

            result = serial->openArray();
            if (validate && !result)
                return false;

            if ((root->flags & JsonNode::fFlagConst) != 0)
            {
                for (uint16_t i = 0; i < root->len; i++)
                {
                    result = serial->writeValue(root->data.strArrayVal[i]);
                    if (validate && !result)
                        return false;
                }
            }
            else
            {
                const char** b = nullptr;
                uint16_t len = 0;

                // fixed length isn't supported here
                result = reader->read(b, &len, 0);
                if (validate && !result)
                    return false;

                // FIXME: dangerous
                b = static_cast<const char**>(alloca(len * sizeof(*b)));
                result = reader->read(b, &len, len);
                if (validate && !result)
                    return false;

                for (uint16_t i = 0; i < len; i++)
                {
                    result = serial->writeValue(b[i]);

                    if (validate && !result)
                        return false;
                }
            }

            return serial->closeArray();

        case NodeType::eObject:
            result = serial->openObject();
            if (validate && !result)
                return false;

            for (uint16_t i = 0; i < root->len; i++)
            {
                result = serializeJsonTree<validate>(serial, &root->data.objVal[i], reader);
                if (validate && !result)
                    return false;
            }

            return serial->closeObject();

        case NodeType::eObjectArray:
            result = serial->openArray();
            if (validate && !result)
                return false;

            if ((root->flags & JsonNode::fFlagFixedLength) != 0)
            {
                for (uint16_t i = 0; i < root->len; i++)
                {
                    result = serializeJsonTree<validate>(serial, &root->data.objVal[i], reader);
                    if (validate && !result)
                        return false;
                }
            }
            else
            {
                uint16_t len = 0;

                // read the array length
                result = reader->read(&len);
                if (validate && !result)
                    return false;

                // a variable length object array uses the same object schema for
                // each object in the array, so we just pass the 0th element here
                for (uint16_t i = 0; i < len; i++)
                {
                    result = serializeJsonTree<validate>(serial, &root->data.objVal[0], reader);
                    if (validate && !result)
                        return false;
                }
            }

            return serial->closeArray();
    }

    return false;
}

/** Serialize JSON using a @ref JsonNode as the schema and a binary blob to read data.
 *  @param[inout] serial   The JSON serializer to serialize the data into.
 *  @param[in]    root     The JSON tree to use for the data schema.
 *  @param[in]    blob     The binary blob to read data directly from.
 *  @param[in]    blobSize The length of @p blob in bytes.
 *
 *  @tparam validate          If this is true, validation will be performed.
 *                            This ensures the output JSON will be valid.
 *                            This also will add bounds checking onto @p blob.
 *                            If this is false, out of bounds reading is possible
 *                            when the blob was generated incorrectly.
 *  @tparam prettyPrint       If this is set to false, the output will be printed
 *                            with minimal spacing.
 *                            Human-readable spacing will be used otherwise.
 *  @tparam onValidationError This callback will be used when a validation error
 *                            occurs, so that logging can be performed.
 *
 *  @returns `true` if no validation error occurred.
 *  @returns `false` if any form of validation error occurred.
 */
template <bool validate = false,
          typename JsonSerializerType = JsonSerializer<false, false, ignoreJsonTreeSerializerValidationError>,
          typename JsonNodeType = JsonNode,
          typename BlobReaderType = BlobReader<false, ignoreJsonTreeSerializerValidationError>>
static inline bool serializeJsonTree(JsonSerializerType* serial, const JsonNodeType* root, const void* blob, size_t blobSize)
{
    BlobReaderType reader(blob, blobSize);
    return serializeJsonTree<validate>(serial, root, &reader) && serial->finish();
}

/* we don't extract static symbols so this breaks exhale somehow */
#ifndef DOXYGEN_SHOULD_SKIP_THIS

/** @copydoc serializeJsonSchema */
template <typename JsonSerializerType = JsonSerializer<true, true>, typename JsonNodeType = JsonNode>
static inline void serializeJsonSchema_(JsonSerializerType* serial, const JsonNodeType* root)
{
    auto nodeTypeString = [](NodeType n) -> const char* {
        switch (n)
        {
            case NodeType::eNull:
                return "null";
            case NodeType::eBool:
                return "boolean";
            case NodeType::eInt32:
                return "integer";
            case NodeType::eUint32:
                return "uint32";
            case NodeType::eInt64:
                return "int64";
            case NodeType::eUint64:
                return "uint64";
            case NodeType::eFloat32:
                return "float";
            case NodeType::eFloat64:
                return "double";
            case NodeType::eBinary:
                return "binary";
            case NodeType::eBoolArray:
                return "bool[]";
            case NodeType::eInt32Array:
                return "integer[]";
            case NodeType::eUint32Array:
                return "uint32[]";
            case NodeType::eInt64Array:
                return "int64[]";
            case NodeType::eUint64Array:
                return "uint64[]";
            case NodeType::eFloat32Array:
                return "float[]";
            case NodeType::eFloat64Array:
                return "double[]";
            case NodeType::eString:
                return "string";
            case NodeType::eStringArray:
                return "string[]";
            case NodeType::eObject:
                return "object";
            case NodeType::eObjectArray:
                return "object[]";
        }
        return "unknown";
    };

    if (root->name != nullptr)
    {
        serial->writeKey(root->name, root->nameLen - 1);
    }

    serial->openObject();
    serial->writeKey("type");
    serial->writeValue(nodeTypeString(root->type));

    serial->writeKey("flags");
    serial->writeValue(root->flags);

    if ((root->flags & JsonNode::fFlagConst) != 0)
    {
        serial->writeKey("const");
        switch (root->type)
        {
            case NodeType::eNull:
                serial->writeValue();
                break;
            case NodeType::eBool:
                serial->writeValue(root->data.boolVal);
                break;
            case NodeType::eInt32:
                serial->writeValue(root->data.intVal);
                break;
            case NodeType::eUint32:
                serial->writeValue(root->data.uintVal);
                break;
            case NodeType::eInt64:
                serial->writeValue(root->data.intVal);
                break;
            case NodeType::eUint64:
                serial->writeValue(root->data.uintVal);
                break;
            case NodeType::eFloat32:
                serial->writeValue(root->data.floatVal);
                break;
            case NodeType::eFloat64:
                serial->writeValue(root->data.floatVal);
                break;
            case NodeType::eBinary:
                serial->writeValueWithBase64Encoding(root->data.binaryVal, root->len);
                break;
            case NodeType::eBoolArray:
                serial->openArray();
                for (uint16_t i = 0; i < root->len; i++)
                {
                    serial->writeValue(root->data.boolArrayVal[i]);
                }
                serial->closeArray();
                break;
            case NodeType::eInt32Array:
                serial->openArray();
                for (uint16_t i = 0; i < root->len; i++)
                {
                    serial->writeValue(root->data.int32ArrayVal[i]);
                }
                serial->closeArray();
                break;
            case NodeType::eUint32Array:
                serial->openArray();
                for (uint16_t i = 0; i < root->len; i++)
                {
                    serial->writeValue(root->data.uint32ArrayVal[i]);
                }
                serial->closeArray();
                break;
            case NodeType::eInt64Array:
                serial->openArray();
                for (uint16_t i = 0; i < root->len; i++)
                {
                    serial->writeValue(root->data.int64ArrayVal[i]);
                }
                serial->closeArray();
                break;
            case NodeType::eUint64Array:
                serial->openArray();
                for (uint16_t i = 0; i < root->len; i++)
                {
                    serial->writeValue(root->data.uint64ArrayVal[i]);
                }
                serial->closeArray();
                break;
            case NodeType::eFloat32Array:
                serial->openArray();
                for (uint16_t i = 0; i < root->len; i++)
                {
                    serial->writeValue(root->data.float32ArrayVal[i]);
                }
                serial->closeArray();
                break;
            case NodeType::eFloat64Array:
                serial->openArray();
                for (uint16_t i = 0; i < root->len; i++)
                {
                    serial->writeValue(root->data.float64ArrayVal[i]);
                }
                serial->closeArray();
                break;
            case NodeType::eString:
                serial->writeValue(root->data.strVal, (root->len == 0) ? 0 : root->len - 1);
                break;
            case NodeType::eStringArray:
                serial->openArray();
                for (uint16_t i = 0; i < root->len; i++)
                {
                    serial->writeValue(root->data.strArrayVal[i]);
                }
                serial->closeArray();
                break;
            case NodeType::eObject:
                serial->writeValue();
                break;
            case NodeType::eObjectArray:
                serial->openArray();
                serial->closeArray();
                break;
        }
    }

    if ((root->flags & JsonNode::fFlagEnum) != 0)
    {
        serial->writeKey("enum");
        serial->writeValue(true);
    }

    if (root->type == NodeType::eObject || root->type == NodeType::eObjectArray)
    {
        serial->writeKey("properties");
        serial->openObject();
        for (size_t i = 0; i < root->len; i++)
        {
            serializeJsonSchema_(serial, &root->data.objVal[i]);
        }
        serial->closeObject();
    }

    serial->closeObject();
}

/** Serialize a JSON schema to JSON.
 *  @param[in] serial The serializer object to use.
 *  @param[in] root   The schema being serialized.
 *  @remarks This function will serialize a JSON schema to JSON.
 *           This is mainly intended to be used for debugging.
 *           serializeJsonTree() can't be used for serializing the schema because
 *           a binary blob is needed for the variable values in the schema.
 */
template <typename JsonSerializerType = JsonSerializer<true, true>, typename JsonNodeType = JsonNode>
static inline void serializeJsonSchema(JsonSerializerType* serial, const JsonNodeType* root)
{
    serializeJsonSchema_(serial, root);
    serial->finish();
}
#endif

} // namespace structuredlog
} // namespace omni
