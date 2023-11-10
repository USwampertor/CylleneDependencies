// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief Interface to handle serializing data from a file format into an
//!        @ref carb::dictionary::IDictionary item.  This interface is
//!        currently implemented in two plugins, each offering a different
//!        input/output format - JSON and TOML.  The plugins are called
//!        `carb.dictionary.serializer-json.plugin` and
//!        `carb.dictionary.serializer-toml.plugin`.  The caller must ensure
//!        they are using the appropriate one for their needs when loading,
//!        the plugin, acquiring the interface, and performing serialization
//!        operations (both to and from strings).
#pragma once

#include "../Framework.h"
#include "../Interface.h"
#include "../datasource/IDataSource.h"
#include "../dictionary/IDictionary.h"

#include <cstdint>

namespace carb
{
/** Namespace for @ref carb::dictionary::IDictionary related interfaces and helpers. */
namespace dictionary
{

/** Base type for flags for the ISerializer::createStringBufferFromDictionary() function. */
using SerializerOptions = uint32_t;

/** Flags to affect the behavior of the ISerializer::createStringBufferFromDictionary()
 *  function.  Zero or more flags may be combined with the `|` operator.  Use `0` to
 *  specify no flags.
 *  @{
 */
/** Flag to indicate that the generated string should include the name of the root node of the
 *  dictionary that is being serialized.  If this flag is not used, the name of the root node
 *  will be skipped and only the children of the node will be serialized.  This is only used
 *  when serializing to JSON with the `carb.dictionary.serializer-json.plugin` plugin.  This
 *  flag will be ignored otherwise.
 */
constexpr SerializerOptions fSerializerOptionIncludeDictionaryName = 1;

/** Flag to indicate that the generated string should be formatted to be human readable and
 *  look 'pretty'.  If this flag is not used, the default behavior is to format the string
 *  as compactly as possible with the aim of it being machine consumable.  This flag may be
 *  used for both the JSON and TOML serializer plugins.
 */
constexpr SerializerOptions fSerializerOptionMakePretty = (1 << 1);

/** Flag to indicate that if an empty dictionary item is found while walking the dictionary
 *  that is being serialized, it should be represented by an empty array.  If this flag is
 *  not used, the default behavior is to write out the empty dictionary item as an empty
 *  object.  This flag may be used for both the JSON and TOML serializer plugins.
 */
constexpr SerializerOptions fSerializerOptionEmptyDictionaryIsArray = (1 << 2);

/** Flag to indicate that the JSON serializer should write out infinity and NaN floating point
 *  values as a null object.  If this flag is not used, the default behavior is to write out
 *  the value as a special string that can later be serialized back to an infinite value by
 *  the same serializer plugin.  This flag is only used in the JSON serializer plugin since
 *  infinite values are not supported by the JSON standard itself.
 */
constexpr SerializerOptions fSerializerOptionSerializeInfinityAsNull = (1 << 3);

/** Deprecated flag name for @ref fSerializerOptionIncludeDictionaryName.  This flag should
 *  no longer be used for new code.  Please use @ref fSerializerOptionIncludeDictionaryName
 *  instead.
 */
CARB_DEPRECATED("Use fSerializerOptionIncludeDictionaryName instead.")
constexpr SerializerOptions fSerializerOptionIncludeCollectionName = fSerializerOptionIncludeDictionaryName;
/** @} */

/** Deprecated serializer option flag names.  Please use the `fSerializerOption*` flags instead.
 *  @{
 */
/** Deprecated flag.  Please use @ref fSerializerOptionIncludeDictionaryName instead. */
CARB_DEPRECATED("Use fSerializerOptionIncludeDictionaryName instead.")
constexpr SerializerOptions kSerializerOptionIncludeDictionaryName = fSerializerOptionIncludeDictionaryName;
/** Deprecated flag.  Please use @ref fSerializerOptionMakePretty instead. */
CARB_DEPRECATED("Use fSerializerOptionMakePretty instead.")
constexpr SerializerOptions kSerializerOptionMakePretty = fSerializerOptionMakePretty;
/** Deprecated flag.  Please use @ref fSerializerOptionEmptyDictionaryIsArray instead. */
CARB_DEPRECATED("Use fSerializerOptionEmptyDictionaryIsArray instead.")
constexpr SerializerOptions kSerializerOptionEmptyDictionaryIsArray = fSerializerOptionEmptyDictionaryIsArray;
/** Deprecated flag.  Please use @ref fSerializerOptionSerializeInfinityAsNull instead. */
CARB_DEPRECATED("Use fSerializerOptionSerializeInfinityAsNull instead.")
constexpr SerializerOptions kSerializerOptionSerializeInfinityAsNull = fSerializerOptionSerializeInfinityAsNull;
/** Deprecated flag.  Please use @ref fSerializerOptionIncludeDictionaryName instead. */
CARB_DEPRECATED("Use fSerializerOptionIncludeDictionaryName instead.")
constexpr SerializerOptions kSerializerOptionIncludeCollectionName = fSerializerOptionIncludeDictionaryName;
/* @} **/

//! Flags for deserializing a string (for @ref ISerializer::createDictionaryFromStringBuffer())
using DeserializerOptions = uint32_t;

//! Default value for @ref DeserializerOptions that specifies no options.
constexpr DeserializerOptions kDeserializerOptionNone = 0;

//! Flag that indicates that the `const char* string` value can actually be considered as `char*` and treated
//! destructively (allow in-situ modification by the deserializer).
constexpr DeserializerOptions fDeserializerOptionInSitu = (1 << 0);

/** Interface intended to serialize dictionary objects to and from plain C strings.  Each
 *  implementation of this interface is intended to handle a different format for the string
 *  data.  The current implementations include support for JSON and TOML strings.  It is left
 *  as an exercise for the caller to handle reading the string from a file before serializing
 *  or writing it to a file after serializing.  Each implementation is assumed that it will be
 *  passed a dictionary item object has been created by the @ref carb::dictionary::IDictionary
 *  interface implemented by the `carb.dictionary.plugin` plugin or that the only IDictionary
 *  interface that can be acquired is also the one that created the item object.
 *
 *  @note If multiple plugins that implement IDictionary are loaded, behavior may be undefined.
 */
struct ISerializer
{
    CARB_PLUGIN_INTERFACE("carb::dictionary::ISerializer", 1, 1)

    //! @private
    CARB_DEPRECATED("use the new createDictionaryFromStringBuffer")
    dictionary::Item*(CARB_ABI* deprecatedCreateDictionaryFromStringBuffer)(const char* serializedString);

    /** Creates a new string representation of a dictionary.
     *
     *  @param[in] dictionary           The dictionary to be serialized.  This must not be
     *                                  `nullptr` but may be an empty dictionary.  The entire
     *                                  contents of the dictionary and all its children will be
     *                                  serialized to the output string.
     *  @param[in] serializerOptions    Option flags to control how the output string is created.
     *                                  These flags can affect both the formatting and the content
     *                                  of the string.
     *  @returns On success, this returns a string containing the serialized dictionary.  When
     *           this string is no longer needed, it must be destroyed using a call to
     *           ISerializer::destroyStringBuffer().
     *
     *           On failure, `nullptr` is returned.  This call can fail if an allocation error
     *           occurs, if a bad dictionary item object is encountered, or an error occurs
     *           formatting the output to the string.
     */
    const char*(CARB_ABI* createStringBufferFromDictionary)(const dictionary::Item* dictionary,
                                                            SerializerOptions serializerOptions);

    /** Destroys a string buffer returned from ISerializer::createStringBufferFromDictionary().
     *
     *  @param[in] serializedString     The string buffer to be destroyed.  This must have been
     *                                  returned from a previous successful call to
     *                                  createStringBufferFromDictionary().
     *  @returns No return value.
     */
    void(CARB_ABI* destroyStringBuffer)(const char* serializedString);

    //! @private
    dictionary::Item*(CARB_ABI* internalCreateDictionaryFromStringBuffer)(const char* string,
                                                                          size_t len,
                                                                          DeserializerOptions options);

    /** Creates a new dictionary object from the contents of a string.
     *
     *  @param[in] string The string containing the data to be serialized into a new
     *                    dictionary object.  This is assumed to be in the format that
     *                    is supported by this specific interface object's
     *                    implementation (for example, JSON or TOML for the default
     *                    built-in implementations).  If this string is not formatted
     *                    correctly for the implementation, the operation will fail. Must
     *                    be `NUL`-terminated even if the length is known.
     *  @param[in] len The length of the string data, if known. If not known, provide `size_t(-1)`
     *                 as the length, which is also the default. The length should not include the
     *                 `NUL` terminator.
     *  @param[in] options Options, if any, to pass to the deserializer. If no options are desired,
     *                     pass @ref kDeserializerOptionNone.
     *  @returns On success, this returns a new dictionary item object containing the serialized
     *           data from the string.  When this dictionary is no longer needed, it must be
     *           destroyed using the carb::dictionary::IDictionary::destroyItem() function.
     *
     *           On failure, `nullptr` is returned.  This call can fail if the input string is not
     *           in the correct format (ie: in TOML format when using the JSON serializer or
     *           vice versa), if the string is malformed, or has a syntax error in it.
     */
    dictionary::Item* createDictionaryFromStringBuffer(const char* string,
                                                       size_t len = size_t(-1),
                                                       DeserializerOptions options = kDeserializerOptionNone)
    {
        return internalCreateDictionaryFromStringBuffer(string, len, options);
    }
};

} // namespace dictionary
} // namespace carb
