// Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"

#include "../Types.h"
#include "../dictionary/IDictionary.h"

#include <cstddef>

namespace carb
{
namespace settings
{

struct Transaction;

struct ISettings
{
    CARB_PLUGIN_INTERFACE("carb::settings::ISettings", 1, 0)

    /**
     * Returns original item type. If the item is not a valid item, returns eCount.
     *
     * @param path Child path, separated with forward slash ('/'), can be nullptr.
     * @return Original item type if item is valid, eCount otherwise.
     */
    dictionary::ItemType(CARB_ABI* getItemType)(const char* path);

    /**
     * Checks if the item could be accessible as the provided type, either directly, or via a cast.
     *
     * @param itemType Item type to check for.
     * @param path Path to the value, separated with forward slash ('/').
     * @return True if accessible, or false otherwise.
     */
    bool(CARB_ABI* isAccessibleAs)(dictionary::ItemType itemType, const char* path);

    /**
     * Makes empty dictionary at the supplied path. If an item was already present,
     * changes its original type to dictionary.
     * If the item doesn't exist, creates dictionary item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     */
    void(CARB_ABI* setDictionary)(const char* path);

    /**
     * Attempts to get the supplied item as integer, either directly or via a cast.
     * Default value is returned if value by path doesn't exist, or there is a conversion failure.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @return Integer value, either raw value or cast from the real item type.
     */
    int64_t(CARB_ABI* getAsInt64)(const char* path);
    /**
     * Sets the integer value at the supplied path. If an item was already present,
     * changes its original type to integer. If the present item has children,
     * destroys all its children.
     * If the item doesn't exist, creates integer item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param value Integer value that will be stored to the supplied path.
     */
    void(CARB_ABI* setInt64)(const char* path, int64_t value);


    /**
     * Attempts to get the supplied item as integer, either directly or via a cast.
     * Default value is returned if value by path doesn't exist, or there is a conversion failure.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @return Integer value, either raw value or cast from the real item type.
     */
    int32_t getAsInt(const char* path);
    /**
     * Sets the integer value at the supplied path. If an item was already present,
     * changes its original type to integer. If the present item has children,
     * destroys all its children.
     * If the item doesn't exist, creates integer item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param value Integer value that will be stored to the supplied path.
     */
    void setInt(const char* path, int32_t value);

    /**
     * Attempts to get the supplied item as float, either directly or via a cast.
     * Default value is returned if value by path doesn't exist, or there is a conversion failure.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @return Floating point value, either raw value or cast from the real item type.
     */
    double(CARB_ABI* getAsFloat64)(const char* path);
    /**
     * Sets the floating point value at the supplied path. If an item was already present,
     * changes its original type to floating point. If the present item has children,
     * destroys all its children.
     * If the item doesn't exist, creates floating point item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param value Floating point value that will be stored to the supplied path.
     */
    void(CARB_ABI* setFloat64)(const char* path, double value);

    /**
     * Attempts to get the supplied item as float, either directly or via a cast.
     * Default value is returned if value by path doesn't exist, or there is a conversion failure.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @return Floating point value, either raw value or cast from the real item type.
     */
    float getAsFloat(const char* path);
    /**
     * Sets the floating point value at the supplied path. If an item was already present,
     * changes its original type to floating point. If the present item has children,
     * destroys all its children.
     * If the item doesn't exist, creates floating point item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param value Floating point value that will be stored to the supplied path.
     */
    void setFloat(const char* path, float value);

    /**
     * Attempts to get the supplied item as boolean, either directly or via a cast.
     * Default value is returned if value by path doesn't exist, or there is a conversion failure.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @return Boolean value, either raw value or cast from the real item type.
     */
    bool(CARB_ABI* getAsBool)(const char* path);
    /**
     * Sets the boolean value at the supplied path. If an item was already present,
     * changes its original type to boolean. If the present item has children,
     * destroys all its children.
     * If the item doesn't exist, creates boolean item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param value Boolean value that will be stored to the supplied path.
     */
    void(CARB_ABI* setBool)(const char* path, bool value);

    const char*(CARB_ABI* internalCreateStringBufferFromItemValue)(const char* path, size_t* pStringLen);

    /**
     * Attempts to create a new string buffer with a value, either the real string value or a string resulting
     * from the stringifying the item value.
     * Please use destroyStringBuffer to free the created buffer.
     *
     * @param path Child path, separated with forward slash ('/'), can be nullptr.
     * @param pStringLen (optional) Receives the length of the string. This can be useful if the string contains NUL
     * characters (i.e. byte data).
     * @return Pointer to the create string buffer if applicable, nullptr otherwise.
     *
     * @note It is undefined to create std::string out of nullptr. For using
     * the value as std::string, @see carb::settings::getStringFromItemValue()
     */
    const char* createStringBufferFromItemValue(const char* path, size_t* pStringLen = nullptr) const
    {
        return internalCreateStringBufferFromItemValue(path, pStringLen);
    }

    const char*(CARB_ABI* internalGetStringBuffer)(const char* path, size_t* pStringLen);

    /**
     * Returns internal raw data pointer to the string value of an item. Thus, doesn't perform any
     * conversions.
     * Dangerous function which only guarantees safety of the data when dictionary is not changing.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param pStringLen (optional) Receives the length of the string. This can be useful if the string contains NUL
     * characters (i.e. byte data).
     * @return Raw pointer to the internal string value if applicable, nullptr otherwise.
     *
     * @note It is undefined to create std::string out of nullptr. For using the value as std::string, @see
     * carb::settings::getString()
     */
    const char* getStringBuffer(const char* path, size_t* pStringLen = nullptr) const
    {
        return internalGetStringBuffer(path, pStringLen);
    }

    void(CARB_ABI* internalSetString)(const char* path, const char* value, size_t stringLen);

    /**
     * Sets the string value at the supplied path. If an item was already present,
     * changes its original type to string. If the present item has children,
     * destroys all its children.
     * If the item doesn't exist, creates string item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param value String value that will be stored to the supplied path.
     * @param stringLen (optional) The length of the string at \ref value to copy. This can be useful if only a portion
     * of the string should be copied, or if \ref value contains NUL characters (i.e. byte data). The default value of
     * size_t(-1) treats \ref value as a NUL-terminated string.
     */
    void setString(const char* path, const char* value, size_t stringLen = size_t(-1)) const
    {
        internalSetString(path, value, stringLen);
    }

    template <typename SettingType>
    SettingType get(const char* path);

    template <typename SettingType>
    void set(const char* path, SettingType value);

    /**
     * Checks if the item could be accessible as array, i.e. all child items names are valid
     * contiguous non-negative integers starting with zero.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @return True if accessible, or false otherwise.
     */
    bool(CARB_ABI* isAccessibleAsArray)(const char* path);

    /**
     * Checks if the item could be accessible as the array of items of provided type,
     * either directly, or via a cast of all elements.
     *
     * @param itemType Item type to check for.
     * @param path Path to the value, separated with forward slash ('/').
     * @return True if a valid array and with all items accessible, or false otherwise.
     */
    bool(CARB_ABI* isAccessibleAsArrayOf)(dictionary::ItemType itemType, const char* path);

    /**
     * Checks if the all the children of the item have array-style indices. If yes, returns the number
     * of children (array elements).
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @return Number of array elements if applicable, or 0 otherwise.
     */
    size_t(CARB_ABI* getArrayLength)(const char* path);

    /**
     * Runs through all the array elements and infers a type that is most suitable for the
     * array.
     *
     * The rules are thus:
     * - Strings attempt to convert to float or bool if possible.
     * - The converted type of the first element is the initial type.
     * - If the initial type is a \ref eBool and later elements can be converted to eBool without losing precision,
     *   eBool is kept. (String variants of "true"/"false", or values equal to 0/1)
     * - Elements of type \ref eFloat can convert to \ref eInt if they don't lose precision.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @return Item type that is most suitable for the array, or ItemType::eCount on failure.
     */
    dictionary::ItemType(CARB_ABI* getPreferredArrayType)(const char* path);

    /**
     * Attempts to get the supplied item as integer, either directly or via a cast, considering the
     * item at path to be an array, and using the supplied index to access its child.
     * Default value is returned if value by path doesn't exist, or there is a conversion failure.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param index Index of the element in array.
     * @return Integer value, either raw value or cast from the real item type.
     */
    int64_t(CARB_ABI* getAsInt64At)(const char* path, size_t index);
    /**
     * Sets the integer value for the supplied item. If an item was already present,
     * changes its original type to integer. If the present item has children,
     * destroys all its children. Considers the item at path to be an array, and uses the
     * supplied index to access its child.
     * If the item doesn't exist, creates integer item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param index Index of the element in array.
     * @param value Integer value that will be stored to the supplied path.
     */
    void(CARB_ABI* setInt64At)(const char* path, size_t index, int64_t value);

    /**
     * Attempts to get the supplied item as 32-bit integer, either directly or via a cast, considering the
     * item at path to be an array, and using the supplied index to access its child.
     * Default value is returned if value by path doesn't exist, or there is a conversion failure.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param index Index of the element in array.
     * @return Integer value, either raw value or cast from the real item type.
     */
    int32_t getAsIntAt(const char* path, size_t index);
    /**
     * Sets the 32-bit integer value for the supplied item. If an item was already present,
     * changes its original type to integer. If the present item has children,
     * destroys all its children. Considers the item at path to be an array, and uses the
     * supplied index to access its child.
     * If the item doesn't exist, creates integer item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param index Index of the element in array.
     * @param value Integer value that will be stored to the supplied path.
     */
    void setIntAt(const char* path, size_t index, int32_t value);

    /**
     * Attempts to securely fill the supplied arrayOut buffer with values, either raw values or
     * via a cast of required item values to the integer type. arrayBufferLength is used
     * as a buffer overflow detection.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param arrayOut Array buffer to fill with integer values.
     * @param arrayBufferLength Size of the supplied array buffer.
     */
    void(CARB_ABI* getAsInt64Array)(const char* path, int64_t* arrayOut, size_t arrayBufferLength);
    /**
     * Sets the integer array at the supplied path. If an item was already present, changes its type,
     * and destroys all its children.
     * If the item doesn't exist, creates an array item, children items, and all the required
     * items along the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param array Integer array to copy values from.
     * @param arrayLength Array length.
     */
    void(CARB_ABI* setInt64Array)(const char* path, const int64_t* array, size_t arrayLength);

    /**
     * Attempts to securely fill the supplied arrayOut buffer with values, either raw values or
     * via a cast of required item values to the integer type. arrayBufferLength is used
     * as a buffer overflow detection.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param arrayOut Array buffer to fill with integer values.
     * @param arrayBufferLength Size of the supplied array buffer.
     */
    void(CARB_ABI* getAsIntArray)(const char* path, int32_t* arrayOut, size_t arrayBufferLength);
    /**
     * Sets the integer array at the supplied path. If an item was already present, changes its type,
     * and destroys all its children.
     * If the item doesn't exist, creates an array item, children items, and all the required
     * items along the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param array Integer array to copy values from.
     * @param arrayLength Array length.
     */
    void(CARB_ABI* setIntArray)(const char* path, const int32_t* array, size_t arrayLength);

    /**
     * Attempts to get the supplied item as float, either directly or via a cast, considering the
     * item at path to be an array, and using the supplied index to access its child.
     * Default value is returned if value by path doesn't exist, or there is a conversion failure.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @return Float value, either raw value or cast from the real item type.
     */
    double(CARB_ABI* getAsFloat64At)(const char* path, size_t index);
    /**
     * Sets the floating point value at the supplied path. If an item was already present,
     * changes its original type to floating point. If the present item has children,
     * destroys all its children. Considers the item at path to be an array, and uses the
     * supplied index to access its child.
     * If the item doesn't exist, creates floating point item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param index Index of the element in array.
     * @param value Floating point value that will be stored to the supplied path.
     */
    void(CARB_ABI* setFloat64At)(const char* path, size_t index, double value);

    /**
     * Attempts to get the supplied item as 32-bit float, either directly or via a cast, considering the
     * item at path to be an array, and using the supplied index to access its child.
     * Default value is returned if value by path doesn't exist, or there is a conversion failure.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @return Float value, either raw value or cast from the real item type.
     */
    float getAsFloatAt(const char* path, size_t index);
    /**
     * Sets the 32-bit floating point value at the supplied path. If an item was already present,
     * changes its original type to floating point. If the present item has children,
     * destroys all its children. Considers the item at path to be an array, and uses the
     * supplied index to access its child.
     * If the item doesn't exist, creates floating point item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param index Index of the element in array.
     * @param value Floating point value that will be stored to the supplied path.
     */
    void setFloatAt(const char* path, size_t index, float value);

    /**
     * Attempts to securely fill the supplied arrayOut buffer with values, either raw values or
     * via a cast of required item values to the floating point type. arrayBufferLength is used
     * as a buffer overflow detection.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param arrayOut Array buffer to fill with floating point values.
     * @param arrayBufferLength Size of the supplied array buffer.
     */
    void(CARB_ABI* getAsFloat64Array)(const char* path, double* arrayOut, size_t arrayBufferLength);
    /**
     * Sets the floating point array at the supplied path. If an item was already present, changes its type,
     * and destroys all its children.
     * If the item doesn't exist, creates an array item, children items, and all the required
     * items along the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param array Floating point array to copy values from.
     * @param arrayLength Array length.
     */
    void(CARB_ABI* setFloat64Array)(const char* path, const double* array, size_t arrayLength);

    /**
     * Attempts to securely fill the supplied arrayOut buffer with values, either raw values or
     * via a cast of required item values to the floating point type. arrayBufferLength is used
     * as a buffer overflow detection.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param arrayOut Array buffer to fill with floating point values.
     * @param arrayBufferLength Size of the supplied array buffer.
     */
    void(CARB_ABI* getAsFloatArray)(const char* path, float* arrayOut, size_t arrayBufferLength);
    /**
     * Sets the floating point array at the supplied path. If an item was already present, changes its type,
     * and destroys all its children.
     * If the item doesn't exist, creates an array item, children items, and all the required
     * items along the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param array Floating point array to copy values from.
     * @param arrayLength Array length.
     */
    void(CARB_ABI* setFloatArray)(const char* path, const float* array, size_t arrayLength);

    /**
     * Attempts to get the supplied item as boolean, either directly or via a cast, considering the
     * item at path to be an array, and using the supplied index to access its child.
     * Default value is returned if value by path doesn't exist, or there is a conversion failure.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @return Boolean value, either raw value or cast from the real item type.
     */
    bool(CARB_ABI* getAsBoolAt)(const char* path, size_t index);
    /**
     * Sets the boolean value at the supplied path. If an item was already present,
     * changes its original type to boolean. If the present item has children,
     * destroys all its children. Considers the item at path to be an array, and uses the
     * supplied index to access its child.
     * If the item doesn't exist, creates boolean item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param index Index of the element in array.
     * @param value Boolean value that will be stored to the supplied path.
     */
    void(CARB_ABI* setBoolAt)(const char* path, size_t index, bool value);
    /**
     * Attempts to securely fill the supplied arrayOut buffer with values, either raw values or
     * via a cast of required item values to the boolean type. arrayBufferLength is used
     * as a buffer overflow detection.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param arrayOut Array buffer to fill with boolean values.
     * @param arrayBufferLength Size of the supplied array buffer.
     */
    void(CARB_ABI* getAsBoolArray)(const char* path, bool* arrayOut, size_t arrayBufferLength);
    /**
     * Sets the boolean array at the supplied path. If an item was already present, changes its type,
     * and destroys all its children.
     * If the item doesn't exist, creates an array item, children items, and all the required
     * items along the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param array Boolean array to copy values from.
     * @param arrayLength Array length.
     */
    void(CARB_ABI* setBoolArray)(const char* path, const bool* array, size_t arrayLength);

    const char*(CARB_ABI* internalCreateStringBufferFromItemValueAt)(const char* path, size_t index, size_t* pStringLen);

    /**
     * Attempts to create new string buffer with a value, either the real string value or a string resulting from
     * the stringifying the item value. Considers the item at path to be an array, and using the supplied index
     * to access its child.
     * Please use destroyStringBuffer to free the created buffer.
     *
     * @param relativeRoot Base item to apply path from.
     * @param path Child path, separated with forward slash ('/'), can be nullptr.
     * @param index Index of the element in array.
     * @param pStringLen (optional) Receives the length of the string. This can be useful if the string contains NUL
     * characters (i.e. byte data). Undefined if the function returns nullptr.
     * @return Pointer to the created string buffer if applicable, nullptr otherwise.
     *
     * @note It is undefined to create std::string out of nullptr. For using the value as std::string, @see
     * carb::settings::getStringFromItemValueAt()
     */
    const char* createStringBufferFromItemValueAt(const char* path, size_t index, size_t* pStringLen = nullptr) const
    {
        return internalCreateStringBufferFromItemValueAt(path, index, pStringLen);
    }

    const char*(CARB_ABI* internalGetStringBufferAt)(const char* path, size_t index, size_t* pStringLen);

    /**
     * Attempts to get the supplied item as string, either directly or via a cast, considering the
     * item at path to be an array, and using the supplied index to access its child.
     * Default value is returned if value by path doesn't exist, or there is a conversion failure.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param index Index of the element in array.
     * @param pStringLen (optional) Receives the length of the string. This can be useful if the string contains NUL
     * characters (i.e. byte data). Undefined if the function returns nullptr.
     * @return Raw pointer to the internal string value if applicable, nullptr otherwise.
     *
     * @note It is undefined to create std::string out of nullptr. For using the value as std::string, @see
     * carb::settings::getStringAt()
     */
    const char* getStringBufferAt(const char* path, size_t index, size_t* pStringLen = nullptr) const
    {
        return internalGetStringBufferAt(path, index, pStringLen);
    }

    void(CARB_ABI* internalSetStringAt)(const char* path, size_t index, const char* value, size_t stringLen);

    /**
     * Sets the string value at the supplied path. If an item was already present,
     * changes its original type to string. If the present item has children,
     * destroys all its children. Considers the item at path to be an array, and uses the
     * supplied index to access its child.
     * If the item doesn't exist, creates string item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param index Index of the element in array.
     * @param value String value that will be stored to the supplied path.
     * @param stringLen (optional) The length of the string at \ref value to copy. This can be useful if only a portion
     * of the string should be copied, or if \ref value contains NUL characters (i.e. byte data). The default value of
     * size_t(-1) treats \ref value as a NUL-terminated string.
     */
    void setStringAt(const char* path, size_t index, const char* value, size_t stringLen = size_t(-1)) const
    {
        internalSetStringAt(path, index, value, stringLen);
    }

    /**
     * Attempts to securely fill the supplied arrayOut buffer with values internal string raw pointers.
     * arrayBufferLength is used as a buffer overflow detection.
     * Similarly to getStringBuffer - doesn't support casts.
     * Dangerous function which only guarantees safety of the data when dictionary is not changing.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param arrayOut Array buffer to fill with char buffer pointer values.
     * @param arrayBufferLength Size of the supplied array buffer.
     *
     * @note It is undefined to create std::string out of nullptr. For using the value as std::string, @see
     * carb::settings::getStringArray()
     */
    void(CARB_ABI* getStringBufferArray)(const char* path, const char** arrayOut, size_t arrayBufferLength);

    /**
     * Sets the string array at the supplied path. If an item was already present, changes its type,
     * and destroys all its children.
     * If the item doesn't exist, creates an array item, children items, and all the required
     * items along the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param array String array to copy values from.
     * @param arrayLength Array length.
     */
    void(CARB_ABI* setStringArray)(const char* path, const char* const* array, size_t arrayLength);

    template <typename SettingArrayType>
    void setArray(const char* path, const SettingArrayType* array, size_t arrayLength);

    // To reset transaction, destroy and recreate it.
    Transaction*(CARB_ABI* createTransaction)();
    void(CARB_ABI* destroyTransaction)(Transaction* transaction);
    void(CARB_ABI* commitTransaction)(Transaction* transaction);

    void(CARB_ABI* setInt64Async)(Transaction* transaction, const char* path, int64_t value);
    void(CARB_ABI* setFloat64Async)(Transaction* transaction, const char* path, double value);
    void(CARB_ABI* setBoolAsync)(Transaction* transaction, const char* path, bool value);
    void(CARB_ABI* setStringAsync)(Transaction* transaction, const char* path, const char* value);

    dictionary::SubscriptionId*(CARB_ABI* subscribeToNodeChangeEvents)(const char* path,
                                                                       dictionary::OnNodeChangeEventFn onChangeEventFn,
                                                                       void* userData);
    dictionary::SubscriptionId*(CARB_ABI* subscribeToTreeChangeEvents)(const char* path,
                                                                       dictionary::OnTreeChangeEventFn onChangeEventFn,
                                                                       void* userData);
    void(CARB_ABI* unsubscribeToChangeEvents)(dictionary::SubscriptionId* subscriptionId);

    void(CARB_ABI* update)(const char* path,
                           const dictionary::Item* dictionary,
                           const char* dictionaryPath,
                           dictionary::OnUpdateItemFn onUpdateItemFn,
                           void* userData);

    const dictionary::Item*(CARB_ABI* getSettingsDictionary)(const char* path);
    dictionary::Item*(CARB_ABI* createDictionaryFromSettings)(const char* path);

    void(CARB_ABI* destroyItem)(const char* path);

    /**
     * Frees buffer, created by "createStringBuffer*" functions.
     *
     * @param stringBuffer Buffer returned by one of the "createStringBuffer*" functions.
     */
    void(CARB_ABI* destroyStringBuffer)(const char* stringBuffer);

    void(CARB_ABI* initializeFromDictionary)(const dictionary::Item* dictionary);

    /**
     * Sets the integer value at the supplied path. If an item was already present,
     * does nothing.
     * If the item doesn't exist, creates integer item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param value Integer value that will be stored to the supplied path.
     */
    void setDefaultInt64(const char* path, int64_t value);
    /**
     * Sets the 32-bit integer value at the supplied path. If an item was already present,
     * does nothing.
     * If the item doesn't exist, creates integer item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param value Integer value that will be stored to the supplied path.
     */
    void setDefaultInt(const char* path, int32_t value);
    /**
     * Sets the floating point value at the supplied path. If an item was already present,
     * does nothing.
     * If the item doesn't exist, creates floating point item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param value Floating point value that will be stored to the supplied path.
     */
    void setDefaultFloat64(const char* path, double value);
    /**
     * Sets the single precision floating point value at the supplied path. If an item was already present,
     * does nothing.
     * If the item doesn't exist, creates floating point item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param value Floating point value that will be stored to the supplied path.
     */
    void setDefaultFloat(const char* path, float value);
    /**
     * Sets the boolean value at the supplied path. If an item was already present,
     * does nothing.
     * If the item doesn't exist, creates boolean item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param value Boolean value that will be stored to the supplied path.
     */
    void setDefaultBool(const char* path, bool value);
    /**
     * Sets the string value at the supplied path. If an item was already present,
     * does nothing.
     * If the item doesn't exist, creates string item, and all the required items along
     * the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param value String value that will be stored to the supplied path.
     */
    void setDefaultString(const char* path, const char* value);

    template <typename SettingType>
    void setDefault(const char* path, SettingType value);

    void setDefaultsFromDictionary(const char* path, const dictionary::Item* dictionary);

    /**
     * Sets the integer array at the supplied path. If an item was already present,
     * does nothing.
     * If the item doesn't exist, creates an array item, children items, and all the required
     * items along the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param array Integer array to copy values from.
     * @param arrayLength Array length.
     */
    void setDefaultInt64Array(const char* path, const int64_t* array, size_t arrayLength);
    /**
     * Sets the integer array at the supplied path. If an item was already present,
     * does nothing.
     * If the item doesn't exist, creates an array item, children items, and all the required
     * items along the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param array Integer array to copy values from.
     * @param arrayLength Array length.
     */
    void setDefaultIntArray(const char* path, const int32_t* array, size_t arrayLength);
    /**
     * Sets the floating point array at the supplied path. If an item was already present,
     * does nothing.
     * If the item doesn't exist, creates an array item, children items, and all the required
     * items along the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param array Floating point array to copy values from.
     * @param arrayLength Array length.
     */
    void setDefaultFloat64Array(const char* path, const double* array, size_t arrayLength);
    /**
     * Sets the floating point array at the supplied path. If an item was already present,
     * does nothing.
     * If the item doesn't exist, creates an array item, children items, and all the required
     * items along the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param array Floating point array to copy values from.
     * @param arrayLength Array length.
     */
    void setDefaultFloatArray(const char* path, const float* array, size_t arrayLength);
    /**
     * Sets the boolean array at the supplied path. If an item was already present,
     * does nothing.
     * If the item doesn't exist, creates an array item, children items, and all the required
     * items along the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param array Boolean array to copy values from.
     * @param arrayLength Array length.
     */
    void setDefaultBoolArray(const char* path, const bool* array, size_t arrayLength);
    /**
     * Sets the string array at the supplied path. If an item was already present,
     * does nothing.
     * If the item doesn't exist, creates an array item, children items, and all the required
     * items along the path if necessary.
     *
     * @param path Path to the value, separated with forward slash ('/').
     * @param array String array to copy values from.
     * @param arrayLength Array length.
     */
    void setDefaultStringArray(const char* path, const char* const* array, size_t arrayLength);

    template <typename SettingArrayType>
    void setDefaultArray(const char* path, const SettingArrayType* array, size_t arrayLength);
};

inline int32_t ISettings::getAsInt(const char* path)
{
    return (int32_t)getAsInt64(path);
}

inline void ISettings::setInt(const char* path, int32_t value)
{
    setInt64(path, (int64_t)value);
}

inline float ISettings::getAsFloat(const char* path)
{
    return (float)getAsFloat64(path);
}

inline void ISettings::setFloat(const char* path, float value)
{
    setFloat64(path, (double)value);
}

inline int32_t ISettings::getAsIntAt(const char* path, size_t index)
{
    return (int32_t)getAsInt64At(path, index);
}
inline void ISettings::setIntAt(const char* path, size_t index, int32_t value)
{
    setInt64At(path, index, (int64_t)value);
}

inline float ISettings::getAsFloatAt(const char* path, size_t index)
{
    return (float)getAsFloat64At(path, index);
}
inline void ISettings::setFloatAt(const char* path, size_t index, float value)
{
    setFloat64At(path, index, (double)value);
}

inline void ISettings::setDefaultInt64(const char* path, int64_t value)
{
    dictionary::ItemType itemType = getItemType(path);
    if (itemType == dictionary::ItemType::eCount)
    {
        setInt64(path, value);
    }
}
inline void ISettings::setDefaultInt(const char* path, int32_t value)
{
    setDefaultInt64(path, (int64_t)value);
}

inline void ISettings::setDefaultFloat64(const char* path, double value)
{
    dictionary::ItemType itemType = getItemType(path);
    if (itemType == dictionary::ItemType::eCount)
    {
        setFloat64(path, value);
    }
}
inline void ISettings::setDefaultFloat(const char* path, float value)
{
    setDefaultFloat64(path, (double)value);
}

inline void ISettings::setDefaultBool(const char* path, bool value)
{
    dictionary::ItemType itemType = getItemType(path);
    if (itemType == dictionary::ItemType::eCount)
    {
        setBool(path, value);
    }
}

inline void ISettings::setDefaultString(const char* path, const char* value)
{
    dictionary::ItemType itemType = getItemType(path);
    if (itemType == dictionary::ItemType::eCount)
    {
        setString(path, value);
    }
}

inline void ISettings::setDefaultsFromDictionary(const char* path, const dictionary::Item* dictionary)
{
    if (dictionary)
    {
        update(path, dictionary, nullptr, dictionary::kUpdateItemKeepOriginal, nullptr);
    }
}

inline void ISettings::setDefaultInt64Array(const char* path, const int64_t* array, size_t arrayLength)
{
    dictionary::ItemType itemType = getItemType(path);
    if (itemType == dictionary::ItemType::eCount)
    {
        setInt64Array(path, array, arrayLength);
    }
}
inline void ISettings::setDefaultIntArray(const char* path, const int32_t* array, size_t arrayLength)
{
    dictionary::ItemType itemType = getItemType(path);
    if (itemType == dictionary::ItemType::eCount)
    {
        setIntArray(path, array, arrayLength);
    }
}
inline void ISettings::setDefaultFloat64Array(const char* path, const double* array, size_t arrayLength)
{
    dictionary::ItemType itemType = getItemType(path);
    if (itemType == dictionary::ItemType::eCount)
    {
        setFloat64Array(path, array, arrayLength);
    }
}
inline void ISettings::setDefaultFloatArray(const char* path, const float* array, size_t arrayLength)
{
    dictionary::ItemType itemType = getItemType(path);
    if (itemType == dictionary::ItemType::eCount)
    {
        setFloatArray(path, array, arrayLength);
    }
}
inline void ISettings::setDefaultBoolArray(const char* path, const bool* array, size_t arrayLength)
{
    dictionary::ItemType itemType = getItemType(path);
    if (itemType == dictionary::ItemType::eCount)
    {
        setBoolArray(path, array, arrayLength);
    }
}
inline void ISettings::setDefaultStringArray(const char* path, const char* const* array, size_t arrayLength)
{
    dictionary::ItemType itemType = getItemType(path);
    if (itemType == dictionary::ItemType::eCount)
    {
        setStringArray(path, array, arrayLength);
    }
}

template <>
inline int32_t ISettings::get<int>(const char* path)
{
    return getAsInt(path);
}

template <>
inline int64_t ISettings::get<int64_t>(const char* path)
{
    return getAsInt64(path);
}

template <>
inline float ISettings::get<float>(const char* path)
{
    return getAsFloat(path);
}

template <>
inline double ISettings::get<double>(const char* path)
{
    return getAsFloat64(path);
}

template <>
inline bool ISettings::get<bool>(const char* path)
{
    return getAsBool(path);
}

template <>
inline const char* ISettings::get<const char*>(const char* path)
{
    return getStringBuffer(path);
}

template <>
inline void ISettings::set<int32_t>(const char* path, int32_t value)
{
    setInt(path, value);
}

template <>
inline void ISettings::set<int64_t>(const char* path, int64_t value)
{
    setInt64(path, value);
}

template <>
inline void ISettings::set<float>(const char* path, float value)
{
    setFloat(path, value);
}

template <>
inline void ISettings::set<double>(const char* path, double value)
{
    setFloat64(path, value);
}

template <>
inline void ISettings::set<bool>(const char* path, bool value)
{
    setBool(path, value);
}

template <>
inline void ISettings::set<const char*>(const char* path, const char* value)
{
    setString(path, value);
}

template <>
inline void ISettings::setArray(const char* path, const bool* array, size_t arrayLength)
{
    setBoolArray(path, array, arrayLength);
}

template <>
inline void ISettings::setArray(const char* path, const int32_t* array, size_t arrayLength)
{
    setIntArray(path, array, arrayLength);
}

template <>
inline void ISettings::setArray(const char* path, const int64_t* array, size_t arrayLength)
{
    setInt64Array(path, array, arrayLength);
}

template <>
inline void ISettings::setArray(const char* path, const float* array, size_t arrayLength)
{
    setFloatArray(path, array, arrayLength);
}

template <>
inline void ISettings::setArray(const char* path, const double* array, size_t arrayLength)
{
    setFloat64Array(path, array, arrayLength);
}

template <>
inline void ISettings::setArray(const char* path, const char* const* array, size_t arrayLength)
{
    setStringArray(path, array, arrayLength);
}

template <>
inline void ISettings::setDefault(const char* path, bool value)
{
    setDefaultBool(path, value);
}

template <>
inline void ISettings::setDefault(const char* path, int32_t value)
{
    setDefaultInt(path, value);
}

template <>
inline void ISettings::setDefault(const char* path, int64_t value)
{
    setDefaultInt64(path, value);
}

template <>
inline void ISettings::setDefault(const char* path, float value)
{
    setDefaultFloat(path, value);
}

template <>
inline void ISettings::setDefault(const char* path, double value)
{
    setDefaultFloat64(path, value);
}

template <>
inline void ISettings::setDefault(const char* path, const char* value)
{
    setDefaultString(path, value);
}


template <>
inline void ISettings::setDefaultArray(const char* settingsPath, const bool* array, size_t arrayLength)
{
    setDefaultBoolArray(settingsPath, array, arrayLength);
}

template <>
inline void ISettings::setDefaultArray(const char* settingsPath, const int32_t* array, size_t arrayLength)
{
    setDefaultIntArray(settingsPath, array, arrayLength);
}

template <>
inline void ISettings::setDefaultArray(const char* settingsPath, const int64_t* array, size_t arrayLength)
{
    setDefaultInt64Array(settingsPath, array, arrayLength);
}

template <>
inline void ISettings::setDefaultArray(const char* settingsPath, const float* array, size_t arrayLength)
{
    setDefaultFloatArray(settingsPath, array, arrayLength);
}

template <>
inline void ISettings::setDefaultArray(const char* settingsPath, const double* array, size_t arrayLength)
{
    setDefaultFloat64Array(settingsPath, array, arrayLength);
}

template <>
inline void ISettings::setDefaultArray(const char* settingsPath, const char* const* array, size_t arrayLength)
{
    setDefaultStringArray(settingsPath, array, arrayLength);
}

} // namespace settings
} // namespace carb
