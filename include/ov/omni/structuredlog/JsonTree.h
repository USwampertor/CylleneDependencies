// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief ABI safe structure for specifying structured log schemas.
 */
#pragma once

#include "../../carb/logging/Log.h"
#include "../extras/ScratchBuffer.h"

#include <cstdint>
#include <cstddef>

namespace omni
{
namespace structuredlog
{

/** The data type contained within a JsonNode.
 *  @note For future maintainability, do not use `default` when switching on
 *        this enum; we'll be able to catch all the places where the new type
 *        needs to be handled in that case.
 */
enum class NodeType : uint8_t
{
    eNull, /**< No type has been set. */
    eBool, /**< bool type. */
    eBoolArray, /**< bool array type. */

    eInt32, /**< int32_t type. This corresponds to the JSON integer type. */
    eInt32Array, /**< int32_t array type. This corresponds to the JSON integer type. */
    eUint32, /**< uint32_t type. This corresponds to the JSON integer type. */
    eUint32Array, /**< uint32_t array type. This corresponds to the JSON integer type. */

    /** int64_t type. For interoperability, we cannot store a 64 bit int directly
     *  in JSON, so the high and low 32 bits get stores in an array [high, low].
     */
    eInt64,

    /** int64_t array type. For interoperability, we cannot store a 64 bit int
     *  directly in JSON, so each element of this array is itself an array
     *  [high, low], where high is the top 32 bits and low is the bottom 32 bits.
     */
    eInt64Array,

    eUint64, /**< uint64_t type. stored identically to @ref NodeType::eInt64. */
    eUint64Array, /**< uint64_t  array type. stored identically to @ref NodeType::eInt64Array. */

    eFloat64, /**< double type. This corresponds to the JSON number type. */
    eFloat64Array, /**< double array type. This corresponds to the JSON number type. */
    eFloat32, /**< float type. This corresponds to the JSON number type. */
    eFloat32Array, /**< float array type. This corresponds to the JSON number type. */

    eBinary, /**< array of bytes that will be base64 encoded into JSON. */

    eString, /**< char* type. */
    eStringArray, /**< char ** type. */
    eObject, /**< object type. */
    eObjectArray, /**< array of objects type. */
};

/** A node in a JSON structure.
 *  This is a standard layout type for @rstref{ABI safety <abi-compatibility>}.
 *  Do not directly write this struct; use @ref JsonBuilder to set the members
 *  to ensure the layout is as-expected by the consumers of this struct.
 */
struct JsonNode
{
    /** The version of the structure.
     *  Headers that use this struct should static assert on its version.
     *  Do not modify the layout of this struct without incrementing this.
     */
    static constexpr uint32_t kVersion = 0;

    /** The base type to be used for enums. */
    using EnumBase = uint16_t;

    /** The type of the @ref flags member. */
    using Flag = uint8_t;

    /** This specifies that the value is constant.
     *  This is used for schemas to specify whether a property is constant.
     *  This flag has no meaning when @p type is @ref NodeType::eObject or
     *  @ref NodeType::eObjectArray; for an object to be constant, each
     *  property of that object must be constant.
     */
    static constexpr Flag fFlagConst = 0x01;

    /** This specifies that an array has a fixed length.
     *  This is used for a schema node in a tree to specify that an array in
     *  the tree has a fixed length (that length is specified by @p len).
     *  This is only valid for array types and string.
     *  This is ignored if combined with @ref fFlagConst.
     */
    static constexpr Flag fFlagFixedLength = 0x02;

    /** This specifies that the parameter is an enum type.
     *  An enum type is stored in the data blob as an @ref EnumBase.
     *  The @ref EnumBase is used as an index into the array of values stored
     *  in this node.
     *  This flag is only valid for a @ref NodeType that is an array type other
     *  than @ref NodeType::eObjectArray.
     */
    static constexpr Flag fFlagEnum = 0x04;

    /** The type of this node.
     *  This and @ref len decide which member of @ref data is in use.
     */
    NodeType type = NodeType::eNull;

    /** Behavioral flags for this node. */
    Flag flags = 0;

    /** The length of the data array.
     *  This is ignored for non-array and non-object types.
     *  For @ref NodeType::eString, this is the length of the stored string (as
     *  an optimization).
     *  For other values of @ref type, this is the length of the array stored
     *  in @ref data.
     */
    uint16_t len = 0;

    /** The length of @ref name in bytes. */
    uint16_t nameLen = 0;

    /* 2 bytes of padding exists here (on x86_64 at least), which may be used
     * for something in the future without breaking the ABI. */

    /** The JSON node name.
     *  This will be nullptr when @ref type is @ref NodeType::eObjectArray.
     */
    char* name = nullptr;

    /** The union of possible values that can be used.
     *  This may not be accessed if @ref type is @ref NodeType::eNull.
     */
    union
    {
        /** This is in use when @ref type is @ref NodeType::eBool. */
        bool boolVal;

        /** This is in use when @ref type is @ref NodeType::eInt32 or @ref NodeType::eInt64. */
        int64_t intVal;

        /** This is in use when @ref type is @ref NodeType::eUint32 or @ref NodeType::eUint64. */
        uint64_t uintVal;

        /** This is in use when @ref type is @ref NodeType::eFloat32 or @ref NodeType::eFloat64. */
        double floatVal;

        /** This is used when @ref type is @ref NodeType::eBinary. */
        uint8_t* binaryVal;

        /** This is in use when @ref type is @ref NodeType::eString. */
        char* strVal;

        /** This is in use when @ref type is @ref NodeType::eBoolArray. */
        bool* boolArrayVal;

        /** This is in use when @ref type is @ref NodeType::eInt32Array. */
        int32_t* int32ArrayVal;

        /** This is in use when @ref type is @ref NodeType::eInt64Array. */
        int64_t* int64ArrayVal;

        /** This is in use when @ref type is @ref NodeType::eUint32Array */
        uint32_t* uint32ArrayVal;

        /** This is in use when @ref type is @ref NodeType::eUint64Array */
        uint64_t* uint64ArrayVal;

        /** This is in use when @ref type is @ref NodeType::eFloat32Array. */
        float* float32ArrayVal;

        /** This is in use when @ref type is @ref NodeType::eFloat64Array. */
        double* float64ArrayVal;

        /** This is in use when @ref type is @ref NodeType::eStringArray. */
        char** strArrayVal;

        /** This is in use when @ref type is @ref NodeType::eObject or
         *  @ref NodeType::eObjectArray.
         *  In the case where @ref type is @ref NodeType::eObject, this node
         *  is an object and each element in @ref objVal is a property of that
         *  object.
         *  In the case where @ref type is @ref NodeType::eObjectArray, this
         *  node is an object array and each element is an entry in the object
         *  array (each entry should have type @ref NodeType::eObject).
         */
        JsonNode* objVal;
    } data;
};

static_assert(sizeof(JsonNode) == 24, "unexpected size");
static_assert(std::is_standard_layout<JsonNode>::value, "this type needs to be ABI safe");
static_assert(offsetof(JsonNode, type) == 0, "struct layout changed");
static_assert(offsetof(JsonNode, flags) == 1, "struct layout changed");
static_assert(offsetof(JsonNode, len) == 2, "struct layout changed");
static_assert(offsetof(JsonNode, nameLen) == 4, "struct layout changed");
static_assert(offsetof(JsonNode, name) == 8, "struct layout changed");
static_assert(offsetof(JsonNode, data) == 16, "struct layout changed");

/** Options to do a less of a strict comparison when comparing trees. */
enum class JsonTreeCompareFuzz
{
    /** Strict comparison: trees must be identical.
     *  Ordering of all elements in the tree must be identical.
     */
    eStrict,

    /** Ignore the ordering of constant elements. */
    eNoConstOrder,

    /** Ignore ordering of all elements. */
    eNoOrder,
};

/** Perform a deep comparison of two nodes.
 *  @param[in] a The first node to compare.
 *  @param[in] b The second node to compare.
 *  @param[in] fuzz How strict this comparison will be.
 *  @returns `true` of the objects are equal.
 *  @returns `false` if the objects differ in any ways.
 *
 *  @note This assumes a properly formed @ref JsonNode. An incorrectly set len
 *        member will likely result in a crash.
 */
inline bool compareJsonTrees(const JsonNode* a, const JsonNode* b, JsonTreeCompareFuzz fuzz = JsonTreeCompareFuzz::eStrict)
{
    if (a->flags != b->flags || a->type != b->type || a->len != b->len)
        return false;

    if ((a->name == nullptr && b->name != nullptr) || (a->name != nullptr && b->name == nullptr))
        return false;

    if (a->name != nullptr && strcmp(a->name, b->name) != 0)
        return false;

    switch (a->type)
    {
        case NodeType::eNull:
            return true;

        case NodeType::eBool:
            return a->data.boolVal == b->data.boolVal;

        case NodeType::eInt32:
        case NodeType::eInt64:
            return a->data.intVal == b->data.intVal;

        case NodeType::eUint32:
        case NodeType::eUint64:
            return a->data.uintVal == b->data.uintVal;

        case NodeType::eFloat32:
        case NodeType::eFloat64:
            return a->data.floatVal == b->data.floatVal;

        case NodeType::eBoolArray:
            return a->len == 0 || memcmp(b->data.int32ArrayVal, a->data.int32ArrayVal, a->len * sizeof(bool)) == 0;

        case NodeType::eUint32Array:
            return a->len == 0 || memcmp(b->data.uint32ArrayVal, a->data.uint32ArrayVal, a->len * sizeof(uint32_t)) == 0;

        case NodeType::eInt32Array:
            return a->len == 0 || memcmp(b->data.int32ArrayVal, a->data.int32ArrayVal, a->len * sizeof(int32_t)) == 0;

        case NodeType::eFloat32Array:
            return a->len == 0 || memcmp(b->data.float32ArrayVal, a->data.float32ArrayVal, a->len * sizeof(float)) == 0;

        case NodeType::eInt64Array:
            return a->len == 0 || memcmp(b->data.int64ArrayVal, a->data.int64ArrayVal, a->len * sizeof(int64_t)) == 0;

        case NodeType::eUint64Array:
            return a->len == 0 || memcmp(b->data.uint64ArrayVal, a->data.uint64ArrayVal, a->len * sizeof(uint64_t)) == 0;

        case NodeType::eFloat64Array:
            return a->len == 0 || memcmp(b->data.float64ArrayVal, a->data.float64ArrayVal, a->len * sizeof(double)) == 0;

        case NodeType::eBinary:
        case NodeType::eString:
            return a->len == 0 || memcmp(b->data.binaryVal, a->data.binaryVal, a->len) == 0;

        case NodeType::eStringArray:
            for (uint16_t i = 0; i < a->len; i++)
            {
                if ((a->data.strArrayVal[i] == nullptr && b->data.strArrayVal[i] != nullptr) ||
                    (a->data.strArrayVal[i] != nullptr && b->data.strArrayVal[i] == nullptr))
                    return false;

                if (a->data.strArrayVal[i] != nullptr && strcmp(a->data.strArrayVal[i], b->data.strArrayVal[i]) != 0)
                    return false;
            }
            return true;

        case NodeType::eObject:
        case NodeType::eObjectArray:
            switch (fuzz)
            {
                case JsonTreeCompareFuzz::eStrict:
                    for (uint16_t i = 0; i < a->len; i++)
                    {
                        if (!compareJsonTrees(&a->data.objVal[i], &b->data.objVal[i]))
                            return false;
                    }
                    break;

                case JsonTreeCompareFuzz::eNoConstOrder:
                {
                    extras::ScratchBuffer<bool, 256> hits;
                    if (!hits.resize(a->len))
                    {
                        // shouldn't ever happen
                        fprintf(stderr, "failed to allocate %u bytes\n", a->len);
                        return false;
                    }

                    for (size_t i = 0; i < a->len; i++)
                    {
                        hits[i] = false;
                    }

                    // first compare the variable fields in order
                    for (uint16_t i = 0, j = 0; i < a->len; i++, j++)
                    {
                        // skip to the next non-constant member
                        while ((a->data.objVal[i].flags & JsonNode::fFlagConst) != 0)
                        {
                            i++;
                        }
                        if (i >= a->len)
                            break;

                        // skip to the next non-constant member
                        while ((b->data.objVal[j].flags & JsonNode::fFlagConst) != 0)
                        {
                            j++;
                        }
                        if (j >= b->len)
                            return false;

                        if (!compareJsonTrees(&a->data.objVal[i], &b->data.objVal[j]))
                            return false;
                    }

                    // compare the constants
                    for (uint16_t i = 0; i < a->len; i++)
                    {
                        if ((a->data.objVal[i].flags & JsonNode::fFlagConst) == 0)
                            continue;

                        for (uint16_t j = 0; j < a->len; j++)
                        {
                            if (!hits[j] && (b->data.objVal[j].flags & JsonNode::fFlagConst) != 0 &&
                                compareJsonTrees(&a->data.objVal[i], &b->data.objVal[j]))
                            {
                                hits[j] = true;
                                break;
                            }
                        }
                    }

                    for (uint16_t i = 0; i < a->len; i++)
                    {
                        if ((b->data.objVal[i].flags & JsonNode::fFlagConst) != 0 && !hits[i])
                            return false;
                    }
                }
                break;

                case JsonTreeCompareFuzz::eNoOrder:
                {
                    extras::ScratchBuffer<bool, 256> hits;
                    if (!hits.resize(a->len))
                    {
                        // shouldn't ever happen
                        fprintf(stderr, "failed to allocate %u bytes\n", a->len);
                        return false;
                    }

                    for (size_t i = 0; i < a->len; i++)
                    {
                        hits[i] = false;
                    }

                    for (uint16_t i = 0; i < a->len; i++)
                    {
                        for (uint16_t j = 0; j < a->len; j++)
                        {
                            if (!hits[j] && compareJsonTrees(&a->data.objVal[i], &b->data.objVal[j]))
                            {
                                hits[j] = true;
                                break;
                            }
                        }
                    }

                    for (uint16_t i = 0; i < a->len; i++)
                    {
                        if (!hits[i])
                            return false;
                    }
                }
                break;
            }
            return true;
    }
    return false;
}


/** A memory allocator interface, which can be overwritten with your custom allocator */
class Allocator
{
public:
    /** The alignment that each allocation must be */
    static constexpr size_t kAlignment = alignof(void*);

    virtual ~Allocator()
    {
    }

    /** Allocated memory.
     *  @param[in] size The number of bytes to allocate.
     *  @returns The allocated memory.
     *  @returns nullptr if memory was not available.
     *  @remarks This should be overwritten by custom memory allocators to use
     *           another allocation mechanism.
     */
    virtual void* alloc(size_t size)
    {
        return malloc(size);
    }

    /** Deallocate a previously allocated block.
     *  @param[in] mem A block previously allocated by alloc().
     */
    virtual void dealloc(void* mem)
    {
        free(mem);
    }

    /** Round a size up to be aligned to @ref kAlignment.
     *  @param[in] size The size to align.
     *  @returns @p size rounded up to the next multiple of @ref kAlignment.
     */
    static size_t fixupAlignment(size_t size)
    {
        if (size % Allocator::kAlignment != 0)
            return size + (kAlignment - (size % Allocator::kAlignment));
        else
            return size;
    }
};

/** An implementation of @ref Allocator which will just allocate from a
 *  preallocated block of memory and never deallocate memory until the full
 *  preallocated block is freed.
 *  This is useful for something like a structured log event, where the required
 *  size of the tree can be preallocated.
 */
class BlockAllocator : public Allocator
{
public:
    /** Create the allocator from a preallocated block.
     *  @param[in] block The block of memory to allocate from.
     *  @param[in] len   The length of @p block in bytes.
     */
    BlockAllocator(void* block, size_t len)
    {
        m_block = static_cast<uint8_t*>(block);
        m_left = len;
    }

    void* alloc(size_t size) override
    {
        void* m = m_block;

        size = fixupAlignment(size);
        if (size > m_left)
            return nullptr;

        m_block += size;
        m_left -= size;
        return m;
    }

    void dealloc(void* mem) override
    {
        CARB_UNUSED(mem);
        // leak it - m_block will be freed when we're done with this allocator
    }

private:
    /** The block being allocated from.
     *  This has m_left bytes available.
     */
    uint8_t* m_block = nullptr;

    /** The number of bytes available in m_left. */
    size_t m_left = 0;
};

/** Free any memory allocated to a @ref JsonNode and clear it out to an empty node.
 *  @param[inout] node  The node to clear out.
 *                      This node must be {} or have had its contents set by
 *                      createObject(), createObjectArray() or setNode().
 *  @param[in]    alloc The allocator used to allocate @p node.
 */
static inline void clearJsonTree(JsonNode* node, Allocator* alloc)
{
    switch (node->type)
    {
        case NodeType::eNull:
        case NodeType::eBool:
        case NodeType::eInt32:
        case NodeType::eUint32:
        case NodeType::eInt64:
        case NodeType::eUint64:
        case NodeType::eFloat32:
        case NodeType::eFloat64:
            break;

        case NodeType::eString:
        case NodeType::eBinary:
        case NodeType::eBoolArray:
        case NodeType::eInt32Array:
        case NodeType::eUint32Array:
        case NodeType::eInt64Array:
        case NodeType::eUint64Array:
        case NodeType::eFloat32Array:
        case NodeType::eFloat64Array:
            alloc->dealloc(node->data.strVal);
            node->data.strVal = nullptr;
            break;

        case NodeType::eStringArray:
            for (uint16_t i = 0; i < node->len; i++)
            {
                alloc->dealloc(node->data.strArrayVal[i]);
            }
            alloc->dealloc(node->data.strArrayVal);
            node->data.strArrayVal = nullptr;
            break;

        case NodeType::eObjectArray:
            // object arrays allocate their elements and their elements'
            // properties in the same allocation
            for (uint16_t i = 0; i < node->len; i++)
            {
                for (uint16_t j = 0; j < node->data.objVal[i].len; j++)
                    clearJsonTree(&node->data.objVal[i].data.objVal[j], alloc);
            }

            alloc->dealloc(node->data.objVal);
            node->data.objVal = nullptr;
            break;

        case NodeType::eObject:
            for (uint16_t i = 0; i < node->len; i++)
                clearJsonTree(&node->data.objVal[i], alloc);

            alloc->dealloc(node->data.objVal);
            node->data.objVal = nullptr;
            break;
    }
    alloc->dealloc(node->name);

    node = {};
}

/** A temporary JsonNode object that will be cleaned up at the end of a scope. */
class TempJsonNode : public JsonNode
{
public:
    /** Create a temporary @ref JsonNode.
     *  @param[in] alloc The allocator used to allocate this tree.
     */
    TempJsonNode(Allocator* alloc)
    {
        m_alloc = alloc;
    }

    ~TempJsonNode()
    {
        clearJsonTree(this, m_alloc);
    }

private:
    Allocator* m_alloc;
};

/** Class for determining the allocation size required to build a JSON tree in a
 *  single block of memory.
 *  To use this, you simply track all of the items that you will store in your
 *  tree, then you can retrieve the size that is required to store this tree.
 *  This final size can be used to allocate a block for a @ref BlockAllocator
 *  to allocate a tree of the exact correct size.
 *
 *  @note This size calculator rounds up all sizes to the nearest alignment so
 *        that allocator can always return properly aligned allocations.
 *        Because of this, ordering of the track calls does not have to exactly
 *        match the ordering of the setNode() calls.
 */
class JsonTreeSizeCalculator
{
public:
    /** Get the size required for the tree.
     *  @returns The size in bytes.
     */
    size_t getSize()
    {
        return m_count;
    }

    /** Track the root node in the tree.
     *  @remarks Call this if you're planning on allocating the root node of the
     *           tree, rather than keeping it as a local variable or something
     *           like that.
     */
    void trackRoot()
    {
        m_count += sizeof(JsonNode);
    }

    /** Track the size of a JSON object node.
     *  @param[in] propertyCount The number of properties that JSON node has.
     *                           For example `{"a": 0, "b": 2}` is an object
     *                           with 2 properties.
     */
    void trackObject(size_t propertyCount)
    {
        m_count += Allocator::fixupAlignment(sizeof(JsonNode) * propertyCount);
    }

    /** Track the size for a JSON array of objects
     *  @param[in] propertyCount The number of properties that each object
     *                           element has. This implies that each element
     *                           will be an object with the same layout.
     *                           For an array of objects with varying layouts,
     *                           trackObject() would need to be called for each
     *                           element.
     *  @param[in] len           The length of the object array.
     *                           (e.g. [{}, {}] is length 2).
     */
    void trackObjectArray(size_t propertyCount, size_t len)
    {
        m_count += Allocator::fixupAlignment(sizeof(JsonNode) * (propertyCount + 1) * len);
    }


    /** Track the size occupied by the node name.
     *  @param[in] name    The node name.
     *  @param[in] nameLen The length of @p name including the null terminator.
     */
    void trackName(const char* name, uint16_t nameLen)
    {
        track(name, nameLen);
    }

    /** Track the size occupied by the node name.
     *  @param[in] name The node name.
     *  @returns The number of bytes required to encode @p name.
     */
    void trackName(const char* name)
    {
        track(name);
    }

    /** Track the size of a node without any associated data.
     *  @remarks This is useful when using the @ref JsonNode structure for
     *           defining a schema, so each node may not store a value, just
     *           type information and a name.
     */
    void track()
    {
    }

    /** Track the size of an arithmetic type node.
     *  @param[in] value The value that will be encoded in the future.
     */
    template <typename T>
    void track(T value)
    {
        CARB_UNUSED(value);
        static_assert(std::is_arithmetic<T>::value, "this is only valid for primitive types");
    }

    /** track the size of a string array node.
     *  @param[in] str The array of strings that will be encoded.
     *  @param[in] len The length of array @p b.
     */
    void track(const char* const* str, uint16_t len)
    {
        size_t size = 0;
        if (len == 0)
            return;

        for (uint16_t i = 0; i < len; i++)
        {
            if (str[i] != nullptr)
                size += Allocator::fixupAlignment(strlen(str[i]) + 1);
        }

        m_count += size + Allocator::fixupAlignment(sizeof(const char*) * len);
    }

    /** Track the size of an array node with the string length pre-calculated.
     *  @param[in] value The array that will be used for the node.
     *  @param[in] len   The length of @p value.
     *                   If @p value is a string, this includes the null terminator.
     */
    template <typename T>
    void track(const T* value, uint16_t len)
    {
        CARB_UNUSED(value);
        static_assert(std::is_arithmetic<T>::value, "this is only valid for primitive types");
        m_count += Allocator::fixupAlignment(len * sizeof(T));
    }

    /** Track the size of a binary blob node.
     *  @param[in] value The array of binary data that will be used for the node.
     *  @param[in] len   The length of @p value.
     */
    void track(const void* value, uint16_t len)
    {
        track(static_cast<const uint8_t*>(value), len);
    }

    /** Track the size of a binary blob node.
     *  @param[in] value The array of binary data that will be used for the node.
     *  @param[in] len   The length of @p value.
     */
    void track(void* value, uint16_t len)
    {
        track(static_cast<const uint8_t*>(value), len);
    }

    /** track the size of a string node.
     *  @param[in] str The string to be encoded.
     *                 This string must be less than 64KiB.
     */
    void track(const char* str)
    {
        if (str != nullptr)
            track(str, uint16_t(strlen(str)) + 1);
    }

    /** track the size of a string node.
     *  @param[in] str The string to be encoded.
     */
    void track(char* str)
    {
        track(static_cast<const char*>(str));
    }

    /** Track the size required for a deep copy of a node.
     *  @param[in] node The node to calculate the size for.
     *  @note This includes the size required for the root node.
     */
    void track(const JsonNode* node)
    {
        trackName(node->name);
        switch (node->type)
        {
            case NodeType::eNull:
                track();
                break;

            case NodeType::eBool:
                track(node->data.boolVal);
                break;

            case NodeType::eInt32:
            case NodeType::eInt64:
                track(node->data.intVal);
                break;

            case NodeType::eUint32:
            case NodeType::eUint64:
                track(node->data.uintVal);
                break;

            case NodeType::eFloat32:
            case NodeType::eFloat64:
                track(node->data.floatVal);
                break;

            case NodeType::eBinary:
                track(node->data.binaryVal, node->len);
                break;

            case NodeType::eBoolArray:
                track(node->data.boolArrayVal, node->len);
                break;

            case NodeType::eInt32Array:
                track(node->data.uint32ArrayVal, node->len);
                break;

            case NodeType::eUint32Array:
                track(node->data.int32ArrayVal, node->len);
                break;

            case NodeType::eInt64Array:
                track(node->data.uint64ArrayVal, node->len);
                break;

            case NodeType::eUint64Array:
                track(node->data.int64ArrayVal, node->len);
                break;

            case NodeType::eFloat32Array:
                track(node->data.float32ArrayVal, node->len);
                break;

            case NodeType::eFloat64Array:
                track(node->data.float64ArrayVal, node->len);
                break;

            case NodeType::eString:
                track(node->data.strVal, node->len);
                break;

            case NodeType::eStringArray:
                // if you don't cast this, it doesn't infer the template correctly
                track(static_cast<const char* const*>(node->data.strArrayVal), node->len);
                break;

            case NodeType::eObjectArray:
                if (node->len > 0)
                    trackObjectArray(node->data.objVal[0].len, node->len);

                for (size_t i = 0; i < node->len; i++)
                {
                    for (size_t j = 0; j < node->data.objVal[i].len; j++)
                        // if you don't cast this, it doesn't infer the template correctly
                        track(static_cast<const JsonNode*>(&node->data.objVal[i].data.objVal[j]));
                }
                break;

            case NodeType::eObject:
                trackObject(node->len);
                for (size_t i = 0; i < node->len; i++)
                    // if you don't cast this, it doesn't infer the template correctly
                    track(static_cast<const JsonNode*>(&node->data.objVal[i]));
                break;
        }
    }

    /** Track the size required for a deep copy of a node.
     *  @param[in] node The node to calculate the size for.
     *  @note This includes the size required for the root node.
     */
    void track(JsonNode* node)
    {
        return track(static_cast<const JsonNode*>(node));
    }

private:
    size_t m_count = 0;
};

/** A class to build JSON trees using @ref JsonNode structs.
 *  These functions all expect an empty node to have been passed in, which
 *  speeds up tree creation by avoiding unnecessary clearNode() calls.
 *
 *  @note These functions do expect that memory allocation may fail.
 *        This is used in the unit tests to verify that the node size
 *        calculator is correct.
 */
class JsonBuilder
{
public:
    /** Constructor.
     *  @param[in] alloc The allocator that will be used to create new @ref JsonNode objects.
     */
    JsonBuilder(Allocator* alloc)
    {
        m_alloc = alloc;
    }

    /** Create a JSON object node.
     *  @param[inout] node          The node to create.
     *                              This node must be equal to {} when passed in.
     *  @param[in]    propertyCount The number of properties that JSON node has.
     *                              For example `{"a": 0, "b": 2}` is an object
     *                              with 2 properties.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool createObject(JsonNode* node, uint16_t propertyCount)
    {
        void* b;

        CARB_ASSERT(node->type == NodeType::eNull);
        CARB_ASSERT(node->len == 0);

        b = m_alloc->alloc(propertyCount * sizeof(*node->data.objVal));
        if (b == nullptr)
        {
            CARB_LOG_ERROR("allocator ran out of memory (node = '%s', requested %zu bytes)", node->name,
                           propertyCount * sizeof(*node->data.objVal));
            return false;
        }

        node->data.objVal = new (b) JsonNode[propertyCount];
        node->len = propertyCount;
        node->type = NodeType::eObject;

        CARB_ASSERT((uintptr_t(node->data.objVal) & (alignof(JsonNode) - 1)) == 0);

        return true;
    }

    /** Create a JSON node that is an array of objects.
     *  @param[inout] node          The node to create.
     *                              This node must be equal to {} when passed in.
     *  @param[in]    propertyCount The number of properties that each object
     *                              element has. This implies that each element
     *                              will be an object with the same layout.
     *                              For an array of objects with varying layouts,
     *                              calcJsonObjectSize() would need to be called
     *                              for each element.
     *  @param[in]    len           The length of the object array.
     *                              When defining an object array in a schema,
     *                              you should set this length to 1, then
     *                              specify the object layout in that element.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool createObjectArray(JsonNode* node, uint16_t propertyCount, uint16_t len)
    {
        void* b;

        CARB_ASSERT(node->type == NodeType::eNull);
        CARB_ASSERT(node->len == 0);

        // allocate it all as one array
        b = m_alloc->alloc((len * (1 + propertyCount)) * sizeof(*node->data.objVal));
        if (b == nullptr)
        {
            CARB_LOG_ERROR("allocator ran out of memory (node = '%s', requested %zu bytes)", node->name,
                           (len * (1 + propertyCount)) * sizeof(*node->data.objVal));
            return false;
        }

        node->data.objVal = new (b) JsonNode[len];
        CARB_ASSERT((uintptr_t(node->data.objVal) & (alignof(JsonNode) - 1)) == 0);
        b = static_cast<uint8_t*>(b) + sizeof(JsonNode) * len;


        for (size_t i = 0; i < len; i++)
        {
            node->data.objVal[i].data.objVal = new (b) JsonNode[propertyCount];
            CARB_ASSERT((uintptr_t(node->data.objVal[i].data.objVal) & (alignof(JsonNode) - 1)) == 0);
            b = static_cast<uint8_t*>(b) + sizeof(JsonNode) * propertyCount;

            node->data.objVal[i].len = propertyCount;
            node->data.objVal[i].type = NodeType::eObject;
        }

        node->len = len;
        node->type = NodeType::eObjectArray;

        return true;
    }

    /** Set a bool node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    b     The boolean value to set on this node.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, bool b)
    {
        return setNode(node, b, &node->data.boolVal, NodeType::eBool);
    }

    /** Set a bool array node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    data  The boolean array to set on this node.
     *                      This is copied into the node.
     *  @param[in]    len   The length of the array @p data.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, const bool* data, uint16_t len)
    {
        return setNode(node, data, len, &node->data.boolArrayVal, NodeType::eBoolArray);
    }

    /** Set a 32 bit integer node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    i     The integer value to set on this node.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, int32_t i)
    {
        return setNode(node, i, &node->data.intVal, NodeType::eInt32);
    }

    /** Set a 32 bit integer array node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    data  The integer array to set on this node.
     *                      This is copied into the node.
     *  @param[in]    len   The length of the array @p data.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, const int32_t* data, uint16_t len)
    {
        return setNode(node, data, len, &node->data.int32ArrayVal, NodeType::eInt32Array);
    }

    /** Set an unsigned 32 bit integer node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    u     The integer value to set on this node.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, uint32_t u)
    {
        return setNode(node, u, &node->data.uintVal, NodeType::eUint32);
    }

    /** Set an unsigned 32 bit integer array node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    data  The integer array to set on this node.
     *                      This is copied into the node.
     *  @param[in]    len   The length of the array @p data.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, const uint32_t* data, uint16_t len)
    {
        return setNode(node, data, len, &node->data.uint32ArrayVal, NodeType::eUint32Array);
    }

    /** Set a 64 bit integer node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    i     The integer value to set on this node.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, int64_t i)
    {
        return setNode(node, i, &node->data.intVal, NodeType::eInt64);
    }

    /** Set a 64 bit integer array node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    data  The integer array to set on this node.
     *                      This is copied into the node.
     *  @param[in]    len   The length of the array @p data.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, const int64_t* data, uint16_t len)
    {
        return setNode(node, data, len, &node->data.int64ArrayVal, NodeType::eInt64Array);
    }

    /** Set an unsigned 64 bit integer node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    u     The integer value to set on this node.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, uint64_t u)
    {
        return setNode(node, u, &node->data.uintVal, NodeType::eUint64);
    }

    /** Set an unsigned 64 bit integer array node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    data  The integer array to set on this node.
     *                      This is copied into the node.
     *  @param[in]    len   The length of the array @p data.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, const uint64_t* data, uint16_t len)
    {
        return setNode(node, data, len, &node->data.uint64ArrayVal, NodeType::eUint64Array);
    }

    /** Set a float node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    f     The float value to set on this node.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, float f)
    {
        return setNode(node, f, &node->data.floatVal, NodeType::eFloat32);
    }

    /** Set a float array node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    data  The double array to set on this node.
     *                      This is copied into the node.
     *  @param[in]    len   The length of the array @p data.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, const float* data, uint16_t len)
    {
        return setNode(node, data, len, &node->data.float32ArrayVal, NodeType::eFloat32Array);
    }

    /** Set a double node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    f     The double value to set on this node.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, double f)
    {
        return setNode(node, f, &node->data.floatVal, NodeType::eFloat64);
    }

    /** Set a double array node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    data  The double array to set on this node.
     *                      This is copied into the node.
     *  @param[in]    len   The length of the array @p data.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, const double* data, uint16_t len)
    {
        return setNode(node, data, len, &node->data.float64ArrayVal, NodeType::eFloat64Array);
    }

    /** Set a string array node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    data  The string array to set on this node.
     *                      This is copied into the node.
     *  @param[in]    len   The length of the array @p data.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, const char* const* data, uint16_t len)
    {
        CARB_ASSERT(node->type == NodeType::eNull);
        CARB_ASSERT(node->len == 0);

        if (len == 0)
        {
            node->data.strArrayVal = nullptr;
            node->type = NodeType::eStringArray;
            node->len = len;
            return true;
        }

        // allocate each of the strings individually to avoid having to create
        // a temporary buffer of string lengths or call strlen() multiple times
        // on each string
        node->data.strArrayVal = static_cast<char**>(m_alloc->alloc(len * sizeof(*node->data.strArrayVal)));
        CARB_ASSERT((uintptr_t(node->data.strArrayVal) & (alignof(char*) - 1)) == 0);
        if (node->data.strArrayVal == nullptr)
        {
            CARB_LOG_ERROR("allocator ran out of memory (node = '%s', requested %zu bytes)", node->name,
                           len * sizeof(*node->data.strArrayVal));
            return false;
        }

        for (uint16_t i = 0; i < len; i++)
        {
            if (data[i] == nullptr)
            {
                node->data.strArrayVal[i] = nullptr;
            }
            else
            {
                size_t s = strlen(data[i]) + 1;
                node->data.strArrayVal[i] = static_cast<char*>(m_alloc->alloc(s));
                if (node->data.strArrayVal[i] == nullptr)
                {
                    CARB_LOG_ERROR("allocator ran out of memory (requested %zu bytes)", s);
                    for (uint16_t j = 0; j < i; j++)
                        m_alloc->dealloc(node->data.strArrayVal[j]);
                    m_alloc->dealloc(node->data.strArrayVal);
                    node->data.strArrayVal = nullptr;
                    return false;
                }

                memcpy(node->data.strArrayVal[i], data[i], s);
            }
        }

        node->type = NodeType::eStringArray;
        node->len = len;

        return true;
    }

    /** Set a string node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    str   The string to copy into this node.
     *  @param[in]    len   The length of @p str including the null terminator.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, const char* str, uint16_t len)
    {
        return setNode(node, str, len, &node->data.strVal, NodeType::eString);
    }

    /** Set a string node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    str   The string to copy into this node.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     *  @returns `false` if @p str was longer than 64KiB.
     */
    bool setNode(JsonNode* node, const char* str)
    {
        size_t len;

        CARB_ASSERT(node->type == NodeType::eNull);
        CARB_ASSERT(node->len == 0);

        if (str == nullptr)
            return setNode(node, str, 0);

        len = strlen(str) + 1;
        if (len > UINT16_MAX)
        {
            CARB_LOG_ERROR("string length exceeds 64KiB maximum (node = '%s', %zu characters, str = '%64s...')",
                           node->name, len, str);
            return false;
        }

        return setNode(node, str, uint16_t(len));
    }

    /** Set a binary blob node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    blob  The data to copy into this node.
     *  @param[in]    len   The number of bytes of data in @p blob.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, const void* blob, uint16_t len)
    {
        return setNode<uint8_t>(node, static_cast<const uint8_t*>(blob), len, &node->data.binaryVal, NodeType::eBinary);
    }

    /** Set a binary blob node.
     *  @param[inout] node  The node to create.
     *                      This node must be equal to {} when passed in.
     *  @param[in]    blob  The data to copy into this node.
     *  @param[in]    len   The number of bytes of data in @p blob.
     *
     *  @returns `true` if the node was successfully created.
     *  @returns `false` if a memory allocation failed.
     */
    bool setNode(JsonNode* node, const uint8_t* blob, uint16_t len)
    {
        return setNode<uint8_t>(node, blob, len, &node->data.binaryVal, NodeType::eBinary);
    }

    /** Set the name of a JSON node.
     *  @param[inout] node     The node to modify.
     *                         The previous name will be cleared out, if any.
     *  @param[in]    name     The name to copy into the node.
     *                         This may be nullptr to just free the previous name.
     *  @param[in]    nameLen  The length of @p name, including the null terminator.
     *                         This must be 0 if @p name is nullptr.
     *
     *  @returns `true` if the node's name was successfully copied.
     *  @returns `false` if a memory allocation failed.
     */
    bool setName(JsonNode* node, const char* name, uint16_t nameLen)
    {
        m_alloc->dealloc(node->name);
        node->name = nullptr;

        if (nameLen == 0)
            return true;

        node->name = static_cast<char*>(m_alloc->alloc(nameLen));
        if (node->name == nullptr)
        {
            CARB_LOG_ERROR("allocator ran out of memory (name = '%s', requested %" PRIu16 " bytes)", name, nameLen);
            return false;
        }

        node->nameLen = nameLen;
        memcpy(node->name, name, nameLen);
        return true;
    }

    /** Set the name of a JSON node.
     *  @param[inout] node     The node to modify.
     *                         The previous name will be cleared out, if any.
     *  @param[in]    name     The name to copy into the node.
     *                         This may be nullptr to just free the previous name.
     *
     *  @returns `true` if the node's name was successfully copied.
     *  @returns `false` if a memory allocation failed.
     */
    bool setName(JsonNode* node, const char* name)
    {
        if (name == nullptr)
            return setName(node, nullptr, 0);

        return setName(node, name, uint16_t(strlen(name) + 1));
    }

    /** Perform a deep copy of a node.
     *  @param[in]    node  The node to deep copy.
     *                      This node must be equal to {} when passed in.
     *  @param[inout] out   The new node that was created.
     *  @returns The deep copied node.
     */
    bool deepCopy(const JsonNode* node, JsonNode* out)
    {
        bool result = true;

        CARB_ASSERT(out->type == NodeType::eNull);
        CARB_ASSERT(out->len == 0);

        if (!setName(out, node->name))
            return false;

        out->flags = node->flags;
        switch (node->type)
        {
            case NodeType::eNull:
                break;

            case NodeType::eBool:
                result = setNode(out, node->data.boolVal);
                break;

            case NodeType::eBoolArray:
                result = setNode(out, node->data.boolArrayVal, node->len);
                break;

            case NodeType::eInt32:
                result = setNode(out, int32_t(node->data.intVal));
                break;

            case NodeType::eInt64:
                result = setNode(out, node->data.intVal);
                break;

            case NodeType::eInt32Array:
                result = setNode(out, node->data.int32ArrayVal, node->len);
                break;

            case NodeType::eUint32:
                result = setNode(out, uint32_t(node->data.uintVal));
                break;

            case NodeType::eUint64:
                result = setNode(out, node->data.uintVal);
                break;

            case NodeType::eUint32Array:
                result = setNode(out, node->data.uint32ArrayVal, node->len);
                break;

            case NodeType::eInt64Array:
                result = setNode(out, node->data.int64ArrayVal, node->len);
                break;

            case NodeType::eUint64Array:
                result = setNode(out, node->data.uint64ArrayVal, node->len);
                break;

            case NodeType::eFloat32:
                result = setNode(out, float(node->data.floatVal));
                break;

            case NodeType::eFloat64:
                result = setNode(out, node->data.floatVal);
                break;

            case NodeType::eFloat32Array:
                result = setNode(out, node->data.float32ArrayVal, node->len);
                break;

            case NodeType::eFloat64Array:
                result = setNode(out, node->data.float64ArrayVal, node->len);
                break;

            case NodeType::eString:
                result = setNode(out, node->data.strVal, node->len);
                break;

            case NodeType::eStringArray:
                result = setNode(out, node->data.strArrayVal, node->len);
                break;

            case NodeType::eBinary:
                result = setNode(out, node->data.binaryVal, node->len);
                break;

            case NodeType::eObjectArray:
                if (node->len == 0)
                    break;

                result = createObjectArray(out, node->data.objVal[0].len, node->len);
                if (!result)
                    break;

                for (uint16_t i = 0; i < node->len; i++)
                {
                    out->data.objVal[i].flags = node->data.objVal[i].flags;
                    for (uint16_t j = 0; result && j < node->data.objVal[i].len; j++)
                    {
                        result = deepCopy(&node->data.objVal[i].data.objVal[j], &out->data.objVal[i].data.objVal[j]);
                    }
                }
                break;

            case NodeType::eObject:
                result = createObject(out, node->len);
                if (!result)
                    break;

                for (size_t i = 0; i < node->len; i++)
                {
                    deepCopy(&node->data.objVal[i], &out->data.objVal[i]);
                }
                break;
        }

        if (!result)
            clearJsonTree(out, m_alloc);

        return result;
    }

    /** Set the flags on a @ref JsonNode.
     *  @param[in] node  The node to update the flags on.
     *                   This node must have already had its value and length set.
     *                   This may not be nullptr.
     *  @param[in] flags The flags to set on @p node.
     *
     *  @tparam failOnNonFatal If this is set to true, the function will return
     *                         false when a non-fatal flag error occurred.
     *                         This is intended for testing.
     *                         The behavior is not changed otherwise by this flag.
     *                         The only fatal cases are: passing in unknown
     *                         flags, setting an enum flag on an incorrect type
     *                         and setting an enum flag on an empty array.
     *
     *  @returns `true` if the flags set were valid.
     *  @returns `false` if the flags set were invalid for the given node.
     */
    template <bool failOnNonFatal = false>
    static bool setFlags(JsonNode* node, JsonNode::Flag flags)
    {
        constexpr JsonNode::Flag fixedLengthConst = JsonNode::fFlagConst | JsonNode::fFlagFixedLength;
        constexpr JsonNode::Flag constEnum = JsonNode::fFlagEnum | JsonNode::fFlagConst;
        constexpr JsonNode::Flag validFlags = JsonNode::fFlagConst | JsonNode::fFlagFixedLength | JsonNode::fFlagEnum;
        bool result = true;

        CARB_ASSERT(node != nullptr);

        // filter out unknown flags because they pose an ABI risk
        if ((flags & ~validFlags) != 0)
        {
            CARB_LOG_ERROR("unknown flags were used %02x (node = '%s')", flags & ~validFlags, node->name);
            return false;
        }

        // a node cannot be fixed length and constant
        if ((flags & ~fixedLengthConst) == fixedLengthConst)
        {
            CARB_LOG_ERROR("attempted to set node to be both const and fixed length (node = '%s')", node->name);
            result = !failOnNonFatal;
        }

        if ((flags & constEnum) == constEnum)
        {
            CARB_LOG_ERROR("a node cannot be both constant and an enum (node = '%s')", node->name);
            flags &= ~JsonNode::fFlagConst;
            result = !failOnNonFatal;
        }

        if ((flags & JsonNode::fFlagEnum) != 0 && node->len == 0)
        {
            CARB_LOG_ERROR("an empty array can not be made into an enum (node = '%s')", node->name);
            return false;
        }

        // check for invalid enum type usage
        switch (node->type)
        {
            case NodeType::eNull:
            case NodeType::eBool:
            case NodeType::eInt32:
            case NodeType::eUint32:
            case NodeType::eInt64:
            case NodeType::eUint64:
            case NodeType::eFloat64:
            case NodeType::eFloat32:
            case NodeType::eObject:
            case NodeType::eObjectArray:
            case NodeType::eBinary:
            case NodeType::eString:
                if ((flags & JsonNode::fFlagEnum) != 0)
                {
                    CARB_LOG_ERROR("an enum type must be on a non-object array type (node = '%s')", node->name);
                    return false;
                }
                break;

            case NodeType::eBoolArray:
            case NodeType::eInt32Array:
            case NodeType::eUint32Array:
            case NodeType::eInt64Array:
            case NodeType::eUint64Array:
            case NodeType::eFloat64Array:
            case NodeType::eFloat32Array:
            case NodeType::eStringArray:
                // arrays can be const or fixed length
                break;
        }

        // check for invalid const or fixed length usage
        switch (node->type)
        {
            case NodeType::eNull:
            case NodeType::eBool:
            case NodeType::eInt32:
            case NodeType::eUint32:
            case NodeType::eInt64:
            case NodeType::eUint64:
            case NodeType::eFloat64:
            case NodeType::eFloat32:
                // fixed length is only valid on arrays
                if ((flags & JsonNode::fFlagFixedLength) != 0)
                {
                    CARB_LOG_ERROR("fixed length cannot be set on a scalar node (node = '%s')", node->name);
                    result = !failOnNonFatal;
                }
                break;

            case NodeType::eObject:
                if ((flags & JsonNode::fFlagConst) != 0)
                {
                    CARB_LOG_ERROR("const is meaningless on an object node (node = '%s')", node->name);
                    result = !failOnNonFatal;
                }

                if ((flags & JsonNode::fFlagFixedLength) != 0)
                {
                    CARB_LOG_ERROR("const is meaningless on an object node (node = '%s')", node->name);
                    result = !failOnNonFatal;
                }
                break;

            case NodeType::eObjectArray:
                if ((flags & JsonNode::fFlagConst) != 0)
                {
                    CARB_LOG_ERROR("const is meaningless on an object array (node = '%s')", node->name);
                    result = !failOnNonFatal;
                }
                break;

            case NodeType::eBinary:
            case NodeType::eString:

            case NodeType::eBoolArray:
            case NodeType::eInt32Array:
            case NodeType::eUint32Array:
            case NodeType::eInt64Array:
            case NodeType::eUint64Array:
            case NodeType::eFloat64Array:
            case NodeType::eFloat32Array:
            case NodeType::eStringArray:
                // arrays can be const or fixed length
                break;
        }

        node->flags = flags;
        return result;
    }

private:
    template <typename T0, typename T1>
    bool setNode(JsonNode* node, T0 value, T1* dest, NodeType type)
    {
        CARB_ASSERT(node->type == NodeType::eNull);
        CARB_ASSERT(node->len == 0);

        node->len = 1;
        node->type = type;
        *dest = value;

        return true;
    }

    template <typename T>
    bool setNode(JsonNode* node, const T* data, uint16_t len, T** dest, NodeType type)
    {
        CARB_ASSERT(node->type == NodeType::eNull);
        CARB_ASSERT(node->len == 0);

        if (len == 0)
        {
            *dest = nullptr;
            node->type = type;
            node->len = len;
            return true;
        }

        *dest = static_cast<T*>(m_alloc->alloc(len * sizeof(**dest)));
        if (*dest == nullptr)
        {
            CARB_LOG_ERROR("allocator ran out of memory (requested %zu bytes)", len * sizeof(**dest));
            return false;
        }

        CARB_ASSERT((uintptr_t(*dest) & (alignof(bool) - 1)) == 0);

        node->type = type;
        node->len = len;
        memcpy(*dest, data, len * sizeof(*data));
        return true;
    }

    Allocator* m_alloc;
};

} // namespace structuredlog
} // namespace omni
