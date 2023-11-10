// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//! @brief carb.dictionary interface definition file.
#pragma once

#include "../Framework.h"
#include "../Interface.h"
#include "../Types.h"
#include "../extras/Hash.h"
#include "../cpp17/StringView.h"
#include "../../omni/String.h"

#include <cstdint>

namespace carb
{
namespace dictionary
{

/**
 * Supported item types. Other types need to be converted from the string item.
 */
enum class ItemType
{
    eBool, //!< Boolean type
    eInt, //!< 64-bit integer type
    eFloat, //!< 64-bit floating-point type
    eString, //!< String type
    eDictionary, //!< Dictionary type (may act as either an array type or a map type)
    eCount //! Number of ItemTypes, not a valid item type
};

//! Structure used in opaque pointers to each dictionary node
struct Item DOXYGEN_EMPTY_CLASS;

//! Actions that may be returned by OnUpdateItemFn.
//! @see IDictionary::update() OnUpdateItemFn
enum class UpdateAction
{
    eOverwrite, //!< The target item should be overwritten by the source item
    eKeep, //!< The target item item should be retained, ignoring the source item
    eReplaceSubtree //!< The entire subtree should be replaced (dictionary source item only)
};

//! Item flags that can be specified by the user.
//! @see IDictionary::getItemFlag() IDictionary::setItemFlag() IDictionary::copyItemFlags()
enum class ItemFlag
{
    eUnitSubtree, //!< Indicates that this \ref Item is a subtree
};

/**
 * Function that will tell whether the merger should overwrite the destination item
 * with the source item. dstItem could be nullptr, meaning that the destination item
 * doesn't exist. This function will be triggered not only for the leaf item, but also
 * for the intermediate eDictionary items that need to be created.
 * @see IDictionary::update()
 */
typedef UpdateAction (*OnUpdateItemFn)(
    const Item* dstItem, ItemType dstItemType, const Item* srcItem, ItemType srcItemType, void* userData);

/**
 * Note that this function does not properly handle overwriting of arrays due to
 * overwriting array being shorter, potentially leaving part of the older array in-place
 * after the merge
 * Use \ref overwriteOriginalWithArrayHandling() if dictionaries are expected to contain array data.
 * @see IDictionary::update()
 */
inline UpdateAction overwriteOriginal(
    const Item* dstItem, ItemType dstItemType, const Item* srcItem, ItemType srcItemType, void* userData)
{
    CARB_UNUSED(dstItem, dstItemType, srcItem, srcItemType, userData);
    return UpdateAction::eOverwrite;
}

/**
 * Function that indicates that the merger should retain the existing destination item.
 * @see IDictionary::update()
 */
inline UpdateAction keepOriginal(
    const Item* dstItem, ItemType dstItemType, const Item* srcItem, ItemType srcItemType, void* userData)
{
    CARB_UNUSED(dstItemType, srcItem, srcItemType, userData);
    if (!dstItem)
    {
        // If the destination item doesn't exist - allow to create a new one
        return UpdateAction::eOverwrite;
    }
    return UpdateAction::eKeep;
}

//! Alias for \ref overwriteOriginal().
constexpr OnUpdateItemFn kUpdateItemOverwriteOriginal = overwriteOriginal;
//! Alias for \ref keepOriginal().
constexpr OnUpdateItemFn kUpdateItemKeepOriginal = keepOriginal;

//! Opaque value representing a subscription.
//! @see IDictionary::subscribeToNodeChangeEvents() IDictionary::subscribeToTreeChangeEvents()
struct SubscriptionId DOXYGEN_EMPTY_CLASS;

//! Type of a change passed to a subscription callback.
//! @see IDictionary::subscribeToNodeChangeEvents() IDictionary::subscribeToTreeChangeEvents() OnNodeChangeEventFn
//! OnTreeChangeEventFn
enum class ChangeEventType
{
    eCreated, //!< An \ref Item was created.
    eChanged, //!< An \ref Item was changed.
    eDestroyed //!< An \ref Item was destroyed.
};

/**
 * A callback that, once registered with subscribeToNodeChangeEvents(), receives callbacks when Items change.
 *
 * @note The callbacks happen in the context of the thread performing the change. It is safe to call back into the
 * IDictionary and unsubscribe or otherwise make changes. For \ref ChangeEventType::eCreated and
 * \ref ChangeEventType::eChanged types, no internal locks are held; for \ref ChangeEventType::eDestroyed internal locks
 * are held which can cause thread-synchronization issues with locking order.
 *
 * @param changedItem The \ref Item that is changing
 * @param eventType The event occurring on \p changedItem
 * @param userData The user data given to \ref IDictionary::subscribeToNodeChangeEvents()
 */
using OnNodeChangeEventFn = void (*)(const Item* changedItem, ChangeEventType eventType, void* userData);

/**
 * A callback that, once registered with subscribeToTreeChangeEvents(), receives callbacks when Items change.
 *
 * @note The callbacks happen in the context of the thread performing the change. It is safe to call back into the
 * IDictionary and unsubscribe or otherwise make changes. For \ref ChangeEventType::eCreated and
 * \ref ChangeEventType::eChanged types, no internal locks are held; for \ref ChangeEventType::eDestroyed internal locks
 * are held which can cause thread-synchronization issues with locking order.
 *
 * @param treeItem The tree \ref Item given to \ref IDictionary::subscribeToTreeChangeEvents()
 * @param changedItem The \ref Item that is changing. May be \p treeItem or a descendant,
 * @param eventType The event occurring on \p changedItem
 * @param userData The user data given to \ref IDictionary::subscribeToTreeChangeEvents()
 */
using OnTreeChangeEventFn = void (*)(const Item* treeItem,
                                     const Item* changedItem,
                                     ChangeEventType eventType,
                                     void* userData);

/**
 * DOM-style dictionary (keeps the whole structure in-memory).
 *
 * In most functions, item is specified using the relative root index and path from
 * the relative root. Path can be nullptr, meaning that baseItem will be considered
 * a specified item.
 *
 * Thread-safety:
 * IDictionary functions are internally thread-safe. Where possible, a shared ("read") lock is held so that multiple
 * threads may query data from a carb::dictionary::Item without blocking each other. Functions that contain `Mutable` in
 * the name, and functions that exchange non-const carb::dictionary::Item pointers will hold an exclusive ("write")
 * lock, which will block any other threads attempting to perform read/write operations on the carb::dictionary::Item.
 * These locks are held at the root level of the Item hierarchy which ensures safety across the entire hierarchy.
 *
 * In some cases, a read or write lock must be held across multiple function calls. An example of this would be a call
 * to getItemChildCount() followed by one or more calls to getItemChildByIndex(). Since you would not want another
 * thread to change the dictionary between the two calls, you must use a lock to keep the state consistent. In this case
 * @ref ScopedWrite and @ref ScopedRead exist to maintain a lock across multiple calls. However, uses of @ref ScopedRead
 * and @ref ScopedWrite should be rare.
 *
 * Ordering:
 * Dictionary items of type @ref ItemType::eDictionary can function as either an array type (sequential integer keys
 * starting with `0`), or a map type. For map types, dictionary attempts to order child items in the order that they
 * were created, provided that the items were created by @ref createItem(), @ref update(), or @ref duplicateItem() (the
 * "creation functions"). This order is reflected when retrieving child items via @ref getItemChildByIndex(). Performing
 * one of the creation functions on a key that already exists will move it to the end of the order, but merely setting a
 * value on an existing item will not change its order.
 */
struct IDictionary
{
    // Version 1.0: Initial
    // Version 1.1: Ordering guarantees for createItem(), update(), duplicateItem().
    CARB_PLUGIN_INTERFACE("carb::dictionary::IDictionary", 1, 1)

    /**
     * Returns opaque pointer to read-only item.
     *
     * @param baseItem Base item to apply path from.
     * @param path Child path, separated with forward slash ('/'), can be nullptr.
     * @return Opaque item pointer if the item is valid, or nullptr otherwise.
     */
    const Item*(CARB_ABI* getItem)(const Item* baseItem, const char* path);

    /**
     * Returns opaque pointer to mutable item.
     *
     * @param baseItem Base item to apply path from.
     * @param path Child path, separated with forward slash ('/'), can be nullptr.
     * @return Opaque item pointer if the item is valid, or nullptr otherwise.
     */
    Item*(CARB_ABI* getItemMutable)(Item* baseItem, const char* path);

    /**
     * Returns number of children that belong to the specified item, if this item
     * is a dictionary. Returns 0 if item is not a dictionary, or doesn't exist.
     *
     * @param  item Item to query number of children from.
     * @return Number of children if applicable, 0 otherwise.
     */
    size_t(CARB_ABI* getItemChildCount)(const Item* item);

    /**
     * Returns opaque pointer to a read-only child item by its index. Mostly for dynamic dictionary processing.
     * This function is different from getItemAt function, in a sense that this function doesn't work with
     * the array view of the supplied item - for example if the item has children named "A", "B", "0", this
     * function will return all of them in an undefined succession. While getItemAt functions work only with
     * items which has array-like names (e.g. "0", "1", "2", etc.).
     *
     * @param item Item to query child from.
     * @param childIndex Child index.
     * @return Opaque const item pointer if the item and index are valid, or nullptr otherwise.
     */
    const Item*(CARB_ABI* getItemChildByIndex)(const Item* item, size_t childIndex);

    /**
     * Returns opaque pointer to a mutable child item by its index. Mostly for dynamic dictionary processing.
     * This function is different from getItemAtMutable function, in a sense that this function doesn't work with
     * the array view of the supplied item - for example if the item has children named "A", "B", "0", this
     * function will return all of them in an undefined succession. While getItemAt functions work only with
     * items which has array-like names (e.g. "0", "1", "2", etc.).
     *
     * @param item Item to query child from.
     * @param childIndex Child index.
     * @return Opaque item pointer if the item and index are valid, or nullptr otherwise.
     */
    Item*(CARB_ABI* getItemChildByIndexMutable)(Item* item, size_t childIndex);

    /**
     * Returns read-only parent item, or nullptr if the supplied item is true root item.
     *
     * @param item Item to get parent for.
     * @return Opaque item pointer if the item is valid and has parent, or nullptr otherwise.
     */
    const Item*(CARB_ABI* getItemParent)(const Item* item);

    /**
     * Returns opaque pointer to a mutable parent item, or nullptr if the supplied item is true root item.
     *
     * @param item Item to get parent for.
     * @return Opaque item pointer if the item is valid and has parent, or nullptr otherwise.
     */
    Item*(CARB_ABI* getItemParentMutable)(Item* item);

    /**
     * Returns original item type. If the item is not a valid item, returns eCount.
     *
     * @param item Item.
     * @return Original item type if item is valid, eCount otherwise.
     */
    ItemType(CARB_ABI* getItemType)(const Item* item);

    /**
     * Securely string buffer filled with the item name.
     *
     * @note Please use \ref destroyStringBuffer() to free the created buffer.
     *
     * @param item Item.
     * @return Pointer to the created item name buffer if applicable, nullptr otherwise.
     */
    const char*(CARB_ABI* createStringBufferFromItemName)(const Item* item);

    /**
     * Returns pointer to an item name, if the item is valid.
     * Dangerous function which only guarantees safety of the data when item is not changing.
     *
     * @param item Item.
     * @return Pointer to an internal item name string if the item is valid, nullptr otherwise.
     */
    const char*(CARB_ABI* getItemName)(const Item* item);

    /**
     * Creates item, and all the required items along the path if necessary.
     * If baseItem supplied is nullptr, the created item is created as a true root.
     *
     * @param baseItem Base item to apply path from.
     * @param path Path to the new item.
     * @param itemType Item type to create.
     * @return Opaque item pointer if it was successfully created, or nullptr otherwise.
     */
    Item*(CARB_ABI* createItem)(Item* baseItem, const char* path, ItemType itemType);


    /**
     * Checks if the item could be accessible as the provided type, either directly, or via a cast.
     *
     * @param itemType Item type to check for.
     * @param item Item.
     * @return True if accessible, or false otherwise.
     */
    bool(CARB_ABI* isAccessibleAs)(ItemType itemType, const Item* item);


    /**
     * Attempts to get the supplied item as integer, either directly or via a cast.
     *
     * @param[in] item The item to retrieve the integer value from.
     * @return The 64 bit integer value of @p item. This will be converted from
     *         the existing type if this @ref Item is not a 64 bit integer.
     *         For an @ref Item of type `int32_t`, `float` or `double`,
     *         this conversion will be a direct cast.
     *         Note that overly large `double` values convert to `INT64_MIN`
     *         on x86_64.
     *         For an @ref Item of string type, the string data will be interpreted
     *         as a number.
     *         If the string is not a valid number, 0 will be returned.
     *         If the string is a valid number, but exceeds the range of an `int64_t`,
     *         it will be parsed as a `double` and converted to an `int64_t`.
     *
     * @note carb.dictionary.serializer-toml.plugin cannot handle integers that
     *       exceed `INT64_MAX`.
     *
     * @note When carb.dictionary.serializer-json.plugin reads integers exceeding `UINT64_MAX`,
     *       it will store the result as a double, so you may get unexpected 64 bit integer values.
     *       Unsigned 64 bit values are still retrievable with full precision though;
     *       they will just be wrapped around when returned from this function.
     */
    int64_t(CARB_ABI* getAsInt64)(const Item* item);

    /**
     * Sets the integer value for the supplied item. If an item was already present,
     * changes its original type to eInt. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eInt item, and all the required items along
     * the path if necessary.
     *
     * @param item Item.
     * @param value Integer value that will be set to the supplied item.
     */
    void(CARB_ABI* setInt64)(Item* item, int64_t value);

    /**
     * Attempts to get the supplied item as 32-bit integer, either directly or via a cast.
     *
     * @param[in] item The item to retrieve the integer value from.
     * @return The 64 bit integer value of @p item. This will be converted from
     *         the existing type if this @ref Item is not a 64 bit integer.
     *         The conversion semantics are the same as for getAsInt64().
     *         Note that the string conversion behavior has the same clamping
     *         limits as getAsInt64(), which may result in unexpected wraparound
     *         behavior; you should use getAsInt64() instead if you may be reading
     *         string values.
     */
    int32_t getAsInt(const Item* item);

    /**
     * Sets the 32-bit integer value for the supplied item. If an item was already present,
     * changes its original type to eInt. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eInt item, and all the required items along
     * the path if necessary.
     *
     * @param item Item.
     * @param value Integer value that will be set to the supplied item.
     */
    void setInt(Item* item, int32_t value);

    /**
     * Helper function that sets the value to an item at path. Creates item at path if
     * it doesn't exist.
     *
     * @param baseItem Base item to apply path from.
     * @param path Path.
     * @param value Integer value that will be set to the supplied item.
     */
    Item* makeInt64AtPath(Item* baseItem, const char* path, int64_t value);

    /**
     * Helper function that sets the value to an item at path. Creates item at path if
     * it doesn't exist.
     *
     * @param baseItem Base item to apply path from.
     * @param path Path.
     * @param value Integer value that will be set to the supplied item.
     */
    Item* makeIntAtPath(Item* baseItem, const char* path, int32_t value);

    /**
     * Attempts to get the supplied item as float, either directly or via a cast.
     *
     * @param[in] item The item to retrieve the floating point value from.
     * @return The 64 bit floating point value of @p item.
     *         This will be converted from the existing type if this @ref Item
     *         is not a `double`.
     *         For an @ref Item of type `int32_t`, `int64_t` or `float`,
     *         this conversion will be a direct cast.
     *         For an @ref Item of string type, the string data will be interpreted
     *         as a number.
     *         If the string is not a valid number, 0 will be returned.
     *         If the string is a valid number, but exceeds the range of a `double`
     *         `INFINITY` or `-INFINITY` will be returned.
     *         Some precision may be lost on overly precise strings.
     */
    double(CARB_ABI* getAsFloat64)(const Item* item);

    /**
     * Sets the floating point value for the supplied item. If an item was already present,
     * changes its original type to eFloat. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eFloat item, and all the required items along
     * the path if necessary.
     *
     * @param item Item.
     * @param value Floating point value that will be set to the supplied item.
     */
    void(CARB_ABI* setFloat64)(Item* item, double value);

    /**
     * Attempts to get the supplied item as 32-bit float, either directly or via a cast.
     *
     * @param[in] item The item to retrieve the floating point value from.
     * @return The 64 bit floating point value of @p item.
     *         This will be converted from the existing type if this @ref Item
     *         is not a `double`.
     *         The conversion semantics are the same as with getAsFloat64().
     */
    float getAsFloat(const Item* item);

    /**
     * Sets the 32-bit floating point value for the supplied item. If an item was already present,
     * changes its original type to eFloat. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eFloat item, and all the required items along
     * the path if necessary.
     *
     * @param item Item.
     * @param value Floating point value that will be set to the supplied item.
     */
    void setFloat(Item* item, float value);

    /**
     * Helper function that sets the value to an item at path. Creates item at path if
     * it doesn't exist.
     *
     * @param baseItem Base item to apply path from.
     * @param path Path.
     * @param value Float value that will be set to the supplied item.
     */
    Item* makeFloat64AtPath(Item* baseItem, const char* path, double value);

    /**
     * Helper function that sets the value to an item at path. Creates item at path if
     * it doesn't exist.
     *
     * @param baseItem Base item to apply path from.
     * @param path Path.
     * @param value Float value that will be set to the supplied item.
     */
    Item* makeFloatAtPath(Item* baseItem, const char* path, float value);

    /**
     * Attempts to get the supplied item as eBool, either directly or via a cast.
     *
     * @param item Item.
     * @return Boolean value, either value directly or cast from the real item type.
     */
    bool(CARB_ABI* getAsBool)(const Item* item);

    /**
     * Sets the boolean value for the supplied item. If an item was already present,
     * changes its original type to eBool. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eBool item, and all the required items along
     * the path if necessary.
     *
     * @param item Item.
     * @param value Boolean value that will be set to the supplied item.
     */
    void(CARB_ABI* setBool)(Item* item, bool value);

    /**
     * Helper function that sets the value to an item at path. Creates item at path if
     * it doesn't exist.
     *
     * @param baseItem Base item to apply path from.
     * @param path Path.
     * @param value Bool value that will be set to the supplied item.
     */
    Item* makeBoolAtPath(Item* baseItem, const char* path, bool value);

    //! @private
    const char*(CARB_ABI* internalCreateStringBufferFromItemValue)(const Item* item, size_t* pStringLen);

    /**
     * Attempts to create a new string buffer with a value, either the real string value or a string resulting
     * from converting the item value to a string.
     *
     * @note Please use \ref destroyStringBuffer() to free the created buffer.
     *
     * @param item Item.
     * @param pStringLen (optional) Receives the length of the string. This can be useful if the string contains NUL
     * characters (i.e. byte data).
     * @return Pointer to the created string buffer if applicable, nullptr otherwise.
     */
    const char* createStringBufferFromItemValue(const Item* item, size_t* pStringLen = nullptr) const
    {
        return internalCreateStringBufferFromItemValue(item, pStringLen);
    }

    //! @private
    const char*(CARB_ABI* internalGetStringBuffer)(const Item* item, size_t* pStringLen);

    /**
     * Returns internal raw data pointer to the string value of an item. Thus, doesn't perform any
     * conversions.
     * Dangerous function which only guarantees safety of the data when dictionary is not changing.
     *
     * @param item Item.
     * @param pStringLen (optional) Receives the length of the string. This can be useful if the string contains NUL
     * characters (i.e. byte data).
     * @return Raw pointer to the internal string value if applicable, nullptr otherwise.
     */
    const char* getStringBuffer(const Item* item, size_t* pStringLen = nullptr)
    {
        return internalGetStringBuffer(item, pStringLen);
    }

    //! @private
    void(CARB_ABI* internalSetString)(Item* item, const char* value, size_t stringLen);

    /**
     * Sets the string value for the supplied item. If an item was already present,
     * changes its original type to eString. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eString item, and all the required items along
     * the path if necessary.
     *
     * @param item Item.
     * @param value String value that will be set to the supplied item.
     * @param stringLen (optional) The length of the string at \p value to copy. This can be useful if only a portion
     * of the string should be copied, or if \p value contains NUL characters (i.e. byte data). The default value of
     * size_t(-1) treats \p value as a NUL-terminated string.
     */
    void setString(Item* item, const char* value, size_t stringLen = size_t(-1)) const
    {
        internalSetString(item, value, stringLen);
    }

    /**
     * Helper function that sets the value to an item at path. Creates item at path if
     * it doesn't exist.
     *
     * @param baseItem Base item to apply path from.
     * @param path Path.
     * @param value String value that will be set to the supplied item.
     * @param stringLen (optional) The length of the string at \p value to copy. This can be useful if only a portion
     * of the string should be copied, or if \p value contains NUL characters (i.e. byte data). The default value of
     * size_t(-1) treats \p value as a NUL-terminated string.
     * @return The item at the given \p path
     */
    Item* makeStringAtPath(Item* baseItem, const char* path, const char* value, size_t stringLen = size_t(-1));

    /**
     * Helper function that ensures the item at path is a dictionary. Creates item at path if
     * it doesn't exist.
     *
     * @param parentItem Base item to apply path from.
     * @param path Path.
     */
    Item* makeDictionaryAtPath(Item* parentItem, const char* path);

    /**
     * Reads the value from an Item, converting if necessary.
     * @tparam T The type of item to read. Must be a supported type.
     * @param item The \ref Item to read from.
     * @returns The value read from the \ref Item.
     */
    template <typename T>
    T get(const dictionary::Item* item);

    /**
     * Reads the value from an Item, converting if necessary.
     * @tparam T The type of item to read. Must be a supported type.
     * @param baseItem The base \ref Item to read from. Must not be `nullptr`.
     * @param path The path from \p baseItem to determine the child \ref Item. May be an empty string.
     * @returns The value read from the \ref Item.
     */
    template <typename T>
    T get(const dictionary::Item* baseItem, const char* path);

    /**
     * Constructs a new Item with the given value or changes an existing item to hold the given value.
     * @tparam T The type of the value stored. Must be a supported type.
     * @param baseItem The base \ref Item to read from. Must not be `nullptr`.
     * @param path The path from \p baseItem where the child \ref Item will be constructed.
     * @param value The value to assign to the new \ref Item.
     */
    template <typename T>
    void makeAtPath(dictionary::Item* baseItem, const char* path, T value);

    /**
     * Checks if the item could be accessible as array, i.e. all child items names are valid
     * contiguous non-negative integers starting with zero.
     *
     * @param item Item.
     * @return True if accessible, or false otherwise.
     */
    bool(CARB_ABI* isAccessibleAsArray)(const Item* item);

    /**
     * Checks if the item could be accessible as the array of items of provided type,
     * either directly, or via a cast of all elements.
     *
     * @param itemType Item type to check for.
     * @param item Item.
     * @return True if a valid array and with all items accessible, or false otherwise.
     */
    bool(CARB_ABI* isAccessibleAsArrayOf)(ItemType itemType, const Item* item);

    /**
     * Checks if the all the children of the item have array-style indices. If yes, returns the number
     * of children (array elements).
     *
     * @param item Item.
     * @return Number of array elements if applicable, or 0 otherwise.
     */
    size_t(CARB_ABI* getArrayLength)(const Item* item);

    /**
     * Runs through all the array elements and infers a type that is most suitable for the
     * array.
     *
     * @param item Item.
     * @return Item type that is most suitable for the array, or ItemType::eCount on failure.
     */
    ItemType(CARB_ABI* getPreferredArrayType)(const Item* item);

    /**
     * Attempts to get the child item as integer, either directly or via a cast, considering the item at path
     * to be an array, and using the supplied index to access its child.
     *
     * @param item Item.
     * @param index Index of the element in array.
     * @return Integer value, either value directly or cast from the real item type.
     */
    int64_t(CARB_ABI* getAsInt64At)(const Item* item, size_t index);

    /**
     * Sets the integer value for the supplied item. If an item was already present,
     * changes its original type to eInt. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eInt item, and all the required items along
     * the path if necessary.
     * Considering the item at path to be an array, and using the supplied index to access its child.
     *
     * @param item Item.
     * @param index Index of the element in array.
     * @param value Integer value that will be set to the supplied item.
     */
    void(CARB_ABI* setInt64At)(Item* item, size_t index, int64_t value);

    /**
     * Attempts to get the child item as 32-bit integer, either directly or via a cast, considering the item at path
     * to be an array, and using the supplied index to access its child.
     *
     * @param item Item.
     * @param index Index of the element in array.
     * @return Integer value, either value directly or cast from the real item type.
     */
    int32_t getAsIntAt(const Item* item, size_t index);

    /**
     * Sets the 32-bit integer value for the supplied item. If an item was already present,
     * changes its original type to eInt. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eInt item, and all the required items along
     * the path if necessary.
     * Considering the item at path to be an array, and using the supplied index to access its child.
     *
     * @param item Item.
     * @param index Index of the element in array.
     * @param value Integer value that will be set to the supplied item.
     */
    void setIntAt(Item* item, size_t index, int32_t value);

    /**
     * Attempts to securely fill the supplied arrayOut buffer with values, either raw values or
     * via a cast of required item values to the integer type. arrayBufferLength is used
     * as a buffer overflow detection.
     *
     * @param item Item.
     * @param arrayOut Array buffer to fill with integer values.
     * @param arrayBufferLength Size of the supplied array buffer.
     */
    void(CARB_ABI* getAsInt64Array)(const Item* item, int64_t* arrayOut, size_t arrayBufferLength);

    /**
     * Sets the integer array for the supplied item. If an item was already present,
     * changes its original type to eDictionary. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eDictionary item, children items, and all the required
     * items along the path if necessary.
     *
     * @param item Item.
     * @param array Integer array to copy values from.
     * @param arrayLength Array length.
     */
    void(CARB_ABI* setInt64Array)(Item* item, const int64_t* array, size_t arrayLength);

    /**
     * Attempts to securely fill the supplied arrayOut buffer with values, either raw values or
     * via a cast of required item values to the 32-bit integer type. arrayBufferLength is used
     * as a buffer overflow detection.
     *
     * @param item Item.
     * @param arrayOut Array buffer to fill with integer values.
     * @param arrayBufferLength Size of the supplied array buffer.
     */
    void(CARB_ABI* getAsIntArray)(const Item* item, int32_t* arrayOut, size_t arrayBufferLength);

    /**
     * Sets the 32-bit integer array for the supplied item. If an item was already present,
     * changes its original type to eDictionary. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eDictionary item, children items, and all the required
     * items along the path if necessary.
     *
     * @param item Item.
     * @param array Integer array to copy values from.
     * @param arrayLength Array length.
     */
    void(CARB_ABI* setIntArray)(Item* item, const int32_t* array, size_t arrayLength);

    /**
     * Attempts to get the child item as float, either directly or via a cast, considering the item at path
     * to be an array, and using the supplied index to access its child.
     *
     * @param index Index of the element in array.
     * @return Floating point value, either value directly or cast from the real item type.
     */
    double(CARB_ABI* getAsFloat64At)(const Item* item, size_t index);

    /**
     * Sets the floating point value for the supplied item. If an item was already present,
     * changes its original type to eFloat. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eFloat item, and all the required items along
     * the path if necessary.
     * Considering the item at path to be an array, and using the supplied index to access its child.
     *
     * @param item Item.
     * @param index Index of the element in array.
     * @param value Floating point value that will be set to the supplied item.
     */
    void(CARB_ABI* setFloat64At)(Item* item, size_t index, double value);

    /**
     * Attempts to get the child item as 32-bit float, either directly or via a cast, considering the item at path
     * to be an array, and using the supplied index to access its child.
     *
     * @param item An \ref Item
     * @param index Index of the element in array
     * @return Floating point value, either value directly or cast from the real item type.
     */
    float getAsFloatAt(const Item* item, size_t index);

    /**
     * Sets the 32-bit floating point value for the supplied item. If an item was already present,
     * changes its original type to eFloat. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eFloat item, and all the required items along
     * the path if necessary.
     * Considering the item at path to be an array, and using the supplied index to access its child.
     *
     * @param item Item.
     * @param index Index of the element in array.
     * @param value Floating point value that will be set to the supplied item.
     */
    void setFloatAt(Item* item, size_t index, float value);

    /**
     * Attempts to securely fill the supplied arrayOut buffer with values, either raw values or
     * via a cast of required item values to the floating point type. arrayBufferLength is used
     * as a buffer overflow detection.
     *
     * @param item Item.
     * @param arrayOut Array buffer to fill with floating point values.
     * @param arrayBufferLength Size of the supplied array buffer.
     */
    void(CARB_ABI* getAsFloat64Array)(const Item* item, double* arrayOut, size_t arrayBufferLength);

    /**
     * Sets the floating point array for the supplied item. If an item was already present,
     * changes its original type to eDictionary. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eDictionary item, children items, and all the required
     * items along the path if necessary.
     *
     * @param item Item.
     * @param array Floating point array to copy values from.
     * @param arrayLength Array length.
     */
    void(CARB_ABI* setFloat64Array)(Item* item, const double* array, size_t arrayLength);

    /**
     * Attempts to securely fill the supplied arrayOut buffer with values, either raw values or
     * via a cast of required item values to the 32-bit floating point type. arrayBufferLength is used
     * as a buffer overflow detection.
     *
     * @param item Item.
     * @param arrayOut Array buffer to fill with floating point values.
     * @param arrayBufferLength Size of the supplied array buffer.
     */
    void(CARB_ABI* getAsFloatArray)(const Item* item, float* arrayOut, size_t arrayBufferLength);

    /**
     * Sets the 32-bit floating point array for the supplied item. If an item was already present,
     * changes its original type to eDictionary. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eDictionary item, children items, and all the required
     * items along the path if necessary.
     *
     * @param item Item.
     * @param array Floating point array to copy values from.
     * @param arrayLength Array length.
     */
    void(CARB_ABI* setFloatArray)(Item* item, const float* array, size_t arrayLength);

    /**
     * Attempts to get the child item as boolean, either directly or via a cast, considering the item at path
     * to be an array, and using the supplied index to access its child.
     *
     * @param item Item.
     * @param index Index of the element in array.
     * @return Boolean value, either value directly or cast from the real item type.
     */
    bool(CARB_ABI* getAsBoolAt)(const Item* item, size_t index);

    /**
     * Sets the boolean value for the supplied item. If an item was already present,
     * changes its original type to eBool. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eBool item, and all the required items along
     * the path if necessary.
     * Considering the item at path to be an array, and using the supplied index to access its child.
     *
     * @param item Item.
     * @param index Index of the element in array.
     * @param value Boolean value that will be set to the supplied item.
     */
    void(CARB_ABI* setBoolAt)(Item* item, size_t index, bool value);

    /**
     * Attempts to securely fill the supplied arrayOut buffer with values, either raw values or
     * via a cast of required item values to the boolean type. arrayBufferLength is used
     * as a buffer overflow detection.
     *
     * @param item Item.
     * @param arrayOut Array buffer to fill with boolean values.
     * @param arrayBufferLength Size of the supplied array buffer.
     */
    void(CARB_ABI* getAsBoolArray)(const Item* item, bool* arrayOut, size_t arrayBufferLength);

    /**
     * Sets the boolean array for the supplied item. If an item was already present,
     * changes its original type to eDictionary. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eDictionary item, children items, and all the required
     * items along the path if necessary.
     *
     * @param item Item.
     * @param array Boolean array to copy values from.
     * @param arrayLength Array length.
     */
    void(CARB_ABI* setBoolArray)(Item* item, const bool* array, size_t arrayLength);

    //! @private
    const char*(CARB_ABI* internalCreateStringBufferFromItemValueAt)(const Item* item, size_t index, size_t* pStringLen);

    /**
     * Attempts to create new string buffer with a value, either the real string value or a string resulting from
     * converting the item value to a string. Considers the item to be an array, and using the supplied index
     * to access its child.
     *
     * @note Please use destroyStringBuffer to free the created buffer.
     *
     * @param item Item.
     * @param index Index of the element in array.
     * @param pStringLen (optional) Receives the length of the string. This can be useful if the string contains NUL
     * characters (i.e. byte data). Undefined if the function returns nullptr.
     * @return Pointer to the created string buffer if applicable, nullptr otherwise.
     */
    const char* createStringBufferFromItemValueAt(const Item* item, size_t index, size_t* pStringLen = nullptr) const
    {
        return internalCreateStringBufferFromItemValueAt(item, index, pStringLen);
    }

    //! @private
    const char*(CARB_ABI* internalGetStringBufferAt)(const Item* item, size_t index, size_t* pStringLen);

    /**
     * Returns internal raw data pointer to the string value of an item. Thus, doesn't perform any
     * conversions. Considers the item at path to be an array, and using the supplied index to access
     * its child.
     * Dangerous function which only guarantees safety of the data when dictionary is not changing.
     *
     * @param item Item.
     * @param index Index of the element in array.
     * @param pStringLen (optional) Receives the length of the string. This can be useful if the string contains NUL
     * characters (i.e. byte data). Undefined if the function returns nullptr.
     * @return Raw pointer to the internal string value if applicable, nullptr otherwise.
     */
    const char* getStringBufferAt(const Item* item, size_t index, size_t* pStringLen = nullptr) const
    {
        return internalGetStringBufferAt(item, index, pStringLen);
    }

    //! @private
    void(CARB_ABI* internalSetStringAt)(Item* item, size_t index, const char* value, size_t stringLen);

    /**
     * Sets the string value for the supplied item. If an item was already present,
     * changes its original type to eString. If the present item is a eDictionary item,
     * destroys all its children.
     * If the item doesn't exist, creates eString item, and all the required items along
     * the path if necessary.
     * Considering the item at path to be an array, and using the supplied index to access its child.
     *
     * @param item Item.
     * @param index Index of the element in array.
     * @param value String value that will be set to the supplied item.
     * @param stringLen (optional) The length of the string at \p value to copy. This can be useful if only a portion
     * of the string should be copied, or if \p value contains NUL characters (i.e. byte data). The default value of
     * size_t(-1) treats \p value as a NUL-terminated string.
     */
    void setStringAt(Item* item, size_t index, const char* value, size_t stringLen = size_t(-1)) const
    {
        internalSetStringAt(item, index, value, stringLen);
    }

    /**
     * Attempts to securely fill the supplied arrayOut buffer with values internal string raw pointers.
     * arrayBufferLength is used as a buffer overflow detection.
     * Similarly to getStringBuffer - doesn't support casts.
     *
     * @param item Item.
     * @param arrayOut Array buffer to fill with integer values.
     * @param arrayBufferLength Size of the supplied array buffer.
     */
    void(CARB_ABI* getStringBufferArray)(const Item* item, const char** arrayOut, size_t arrayBufferLength);

    /**
     * Sets the string array for the supplied item. If an item was already present,
     * changes its original type to eDictionary. If the present item is a eDictionary item,
     * destroys all its children.
     *
     * @param item Item.
     * @param array String array to copy values from.
     * @param arrayLength Array length.
     */
    void(CARB_ABI* setStringArray)(Item* item, const char* const* array, size_t arrayLength);

    /**
     * Returns opaque pointer to a read-only child item by its index. Mostly for dynamic dictionary processing.
     * This function is designed for array view access, if you just want to enumerate all children, use
     * getItemChildCount and getItemChildByIndex instead.
     *
     * @param item Item.
     * @return Opaque const item pointer if the item and index are valid, or nullptr otherwise.
     */
    const Item*(CARB_ABI* getItemAt)(const Item* item, size_t index);

    /**
     * Returns opaque pointer to a mutable item by its index. Mostly for dynamic dictionary processing.
     * This function is designed for array view access, if you just want to enumerate all children, use
     * getItemChildCount and getItemChildByIndex instead.
     *
     * @param item Item.
     * @return Opaque item pointer if the item and index are valid, or nullptr otherwise.
     */
    Item*(CARB_ABI* getItemAtMutable)(Item* item, size_t index);

    /**
     * Attempts to securely fill the supplied arrayOut buffer with read-only opaque pointers to
     * the items that are array elements.
     * arrayBufferLength is used as a buffer overflow detection.
     * This function should not be used for simple dynamic processing purposes, use
     * getItemChildCount and getItemChildByIndex instead.
     *
     * @param item The \ref Item.
     * @param arrayOut Array buffer to fill with opaque item pointers.
     * @param arrayBufferLength Size of the supplied array buffer.
     */
    void(CARB_ABI* getItemArray)(const Item* item, const Item** arrayOut, size_t arrayBufferLength);

    /**
     * Changes an Item to be an array type (dictionary with keys as 0-based consecutive integers) with the given data.
     * @note Any existing children in \p item are removed.
     * @tparam ArrayElementType The type of elements to fill the array with.
     * @param item The \ref Item that will be changed to an array type
     * @param array An array that is used to fill \p item. The values are copied.
     * @param arrayLength The length of \p array as a count of the items.
     */
    template <typename ArrayElementType>
    void setArray(Item* item, const ArrayElementType* array, size_t arrayLength);

    /**
     * Merges the source item into a destination item. Destination path may be non-existing, then
     * missing items in the path will be created as dictionaries.
     *
     * @param dstBaseItem Destination base item to apply path from.
     * @param dstPath Destination Child path, separated with forward slash ('/'), can be nullptr.
     * @param srcBaseItem Source base item to apply path from.
     * @param srcPath Source Child path, separated with forward slash ('/'), can be nullptr.
     * @param onUpdateItemFn Function that will tell whether the update should
     * overwrite the destination item with the source item.
     * @param userData User data pointer that will be passed into the onUpdateItemFn.
     */
    void(CARB_ABI* update)(Item* dstBaseItem,
                           const char* dstPath,
                           const Item* srcBaseItem,
                           const char* srcPath,
                           OnUpdateItemFn onUpdateItemFn,
                           void* userData);

    /**
     * Destroys supplied item, and all of its children, if any.
     *
     * @param item Item.
     */
    void(CARB_ABI* destroyItem)(Item* item);

    /**
     * Frees buffer, created by "createBuffer*" functions.
     *
     * @param stringBuffer Buffer returned by one of the "createBuffer*" functions.
     */
    void(CARB_ABI* destroyStringBuffer)(const char* stringBuffer);

    /**
     * Delete all children of a specified item.
     *
     * @param item Item.
     */
    void deleteChildren(Item* item);

    /**
     * Retrieves the value of an item flag.
     * @param item Item.
     * @param flag ItemFlag.
     * @return Original item type if item is valid, eCount otherwise.
     */
    bool(CARB_ABI* getItemFlag)(const Item* item, ItemFlag flag);

    /**
     * Sets the value of an item flag.
     * @param item Item.
     * @param flag ItemFlag.
     * @return Original item type if item is valid, eCount otherwise.
     */
    void(CARB_ABI* setItemFlag)(Item* item, ItemFlag flag, bool flagValue);

    /**
     * Copies all item flags from one Item to another.
     * @param dstItem The destination \ref Item that will receive the flags.
     * @param srcItem The source \ref Item from which flags are copied.
     */
    void copyItemFlags(Item* dstItem, const Item* srcItem);

    /**
     * Subscribes to change events about a specific item.
     *
     * @param baseItem An item
     * @param path The subpath from \p baseItem or `nullptr`
     * @param onChangeEventFn The function to call when a change event occurs
     * @param userData User-specific data that will be provided to \p onChangeEventFn
     * @return A subscription handle that can be used with \ref unsubscribeToChangeEvents()
     */
    SubscriptionId*(CARB_ABI* subscribeToNodeChangeEvents)(Item* baseItem,
                                                           const char* path,
                                                           OnNodeChangeEventFn onChangeEventFn,
                                                           void* userData);
    /**
     * Subscribes to change events for all items in a subtree.
     *
     * All items under `<baseItem>/<path>` will trigger change notifications.
     *
     * @param baseItem An item
     * @param path The subpath from \p baseItem or `nullptr`
     * @param onChangeEventFn The function to call when a change event occurs
     * @param userData User-specific data that will be provided to \p onChangeEventFn
     * @return A subscription handle that can be used with \ref unsubscribeToChangeEvents()
     */
    SubscriptionId*(CARB_ABI* subscribeToTreeChangeEvents)(Item* baseItem,
                                                           const char* path,
                                                           OnTreeChangeEventFn onChangeEventFn,
                                                           void* userData);

    /**
     * Unsubscribes from change events
     *
     * @param subscriptionId The subscription handle
     */
    void(CARB_ABI* unsubscribeToChangeEvents)(SubscriptionId* subscriptionId);

    /**
     * Unsubscribes all node change handles for a specific Item.
     *
     * @param item An item
     */
    void(CARB_ABI* unsubscribeItemFromNodeChangeEvents)(Item* item);

    /**
     * Unsubscribes all subtree change handles for a specific Item.
     *
     * @param item An item
     */
    void(CARB_ABI* unsubscribeItemFromTreeChangeEvents)(Item* item);

    /**
     * Locks an Item for reading.
     *
     * Mutex-locks `item` for reading. This is only necessary if you are doing multiple read operations and require
     * thread-safe consistency across the multiple operations. May be called multiple times within a thread, but
     * unlock() must be called for each readLock() call, once the read lock is no longer needed. Do not use
     * readLock() directly; prefer @ref ScopedRead instead.
     * @param item The item to read-lock. `item`'s entire hierarchy will be read-locked.
     */
    void(CARB_ABI* readLock)(const Item* item);

    /**
     * Locks an Item for writing.
     *
     * Mutex-locks `item` for writing (exclusive access). This is only necessary if you are doing multiple read/write
     * operations and require thread-safe consistency across the multiple operations. May be called multiple times
     * within a thread, but unlock() must be called for each writeLock() call, once the write lock is no longer
     * needed. Calling writeLock() when a readLock() is already held will release the lock and wait until exclusive
     * write lock can be gained. Do not use writeLock() directly; prefer @ref ScopedWrite instead.
     * @param item The item to write-lock. `item`'s entire hierarchy will be write-locked.
     */
    void(CARB_ABI* writeLock)(Item* item);

    /**
     * Releases a lock from a prior readLock() or writeLock().
     *
     * Releases a held read- or write-lock. Must be called once for each read- or write-lock that is held. Must be
     * called in the same thread that initiated the read- or write-lock. Do not use unlock() directly; prefer using
     * @ref ScopedWrite or @ref ScopedRead instead.
     * @param item The item to unlock. `item`'s entire hierarchy will be unlocked
     */
    void(CARB_ABI* unlock)(const Item* item);

    /**
     * Returns a 128-bit hash representing the value.
     *
     * This hash is invariant to the order of keys and values.
     *
     * We guarantee that the hashing algorithm will not change unless the version number
     * of the interface changes.
     *
     * @param item The item to take the hash of.
     * @return The 128-bit hash of the item.
     */
    const extras::hash128_t(CARB_ABI* getHash)(const Item* item);

    /**
     * Duplicates a dictionary
     *
     * @note The function duplicateItem should be preferred to this internal function.
     *
     * @param srcItem The item to duplicate
     * @param newParent The new parent of the dictionary, if nullptr, makes a new root.
     * @param newKey If new parent is given, then newKey is new the key of that parent. If, the key already exists it is
     * overwritten.
     * @returns The created item.
     */
    Item*(CARB_ABI* duplicateItemInternal)(const Item* item, Item* newParent, const char* newKey);

    /**
     * Lexicographically compares two Items.
     *
     * The items being compared do not need to be root items. If the items are a key of a parent object, that key is not
     * included in the comparison.
     *
     * The rules match https://en.cppreference.com/w/cpp/algorithm/lexicographical_compare, treating the key and values
     * in the dictionary as an ordered set. The "type" is included in the comparison, however the rules of what types
     * are ordered differently than others may change on a version change of this interface.
     *
     * The function will return a negative, zero, or positive number. The magnitude of the number bears no importance,
     * and shouldn't be assumed to. A negative number means that itemA < itemB, zero itemA = itemB, and positive itemA >
     * itemB.
     *
     * @param itemA The first item to compare.
     * @param itemB The second item to compare.
     * @returns The result of a lexicographical compare of itemA and itemB.
     */
    int(CARB_ABI* lexicographicalCompare)(const Item* itemA, const Item* itemB);

    /**
     * Duplicates a given item as a new root.
     *
     * @param item The item to duplicate.
     * @returns A new item that where item is to root.
     */
    Item* duplicateItem(const Item* item)
    {
        return duplicateItemInternal(item, nullptr, nullptr);
    }

    /**
     * Duplicates a item where another item is the parent.
     *
     * If the key already exists, the item will be overridden.
     *
     * @param item The item to duplicate.
     * @param newParent The parent item to own the duplicated item.
     * @param newKey the key in the parent.
     * @returns The newly duplicated item.
     */
    Item* duplicateItem(const Item* item, Item* newParent, const char* newKey)
    {
        return duplicateItemInternal(item, newParent, newKey);
    }
};

/**
 * A helper class for calling writeLock() and unlock(). Similar to `std::unique_lock`.
 */
class ScopedWrite
{
    const IDictionary* m_pDictionary;
    Item* m_pItem;

public:
    /**
     * RAII constructor. Immediately takes a write lock until *this is destroyed.
     * @param dictionary A reference to \ref IDictionary.
     * @param item The item in the subtree to lock. The root of this item is found and the entire subtree from the root
     * is locked.
     */
    ScopedWrite(const IDictionary& dictionary, Item* item) : m_pDictionary(std::addressof(dictionary)), m_pItem(item)
    {
        m_pDictionary->writeLock(m_pItem);
    }
    //! Destructor. Releases the write lock.
    ~ScopedWrite()
    {
        m_pDictionary->unlock(m_pItem);
    }
    CARB_PREVENT_COPY_AND_MOVE(ScopedWrite);
};

/**
 * A helper class for calling readLock() and unlock(). Similar to `std::shared_lock`.
 */
class ScopedRead
{
    const IDictionary* m_pDictionary;
    const Item* m_pItem;

public:
    /**
     * RAII constructor. Immediately takes a read lock until *this is destroyed.
     * @param dictionary A reference to \ref IDictionary.
     * @param item The item in the subtree to lock. The root of this item is found and the entire subtree from the root
     * is locked.
     */
    ScopedRead(const IDictionary& dictionary, const Item* item)
        : m_pDictionary(std::addressof(dictionary)), m_pItem(item)
    {
        m_pDictionary->readLock(m_pItem);
    }
    //! Destructor. Releases the read lock.
    ~ScopedRead()
    {
        m_pDictionary->unlock(m_pItem);
    }
    CARB_PREVENT_COPY_AND_MOVE(ScopedRead);
};

inline int32_t IDictionary::getAsInt(const Item* item)
{
    return static_cast<int32_t>(getAsInt64(item));
}
inline void IDictionary::setInt(Item* item, int32_t value)
{
    setInt64(item, static_cast<int64_t>(value));
}
inline Item* IDictionary::makeIntAtPath(Item* baseItem, const char* path, int32_t value)
{
    return makeInt64AtPath(baseItem, path, static_cast<int64_t>(value));
}

inline float IDictionary::getAsFloat(const Item* item)
{
    return static_cast<float>(getAsFloat64(item));
}
inline void IDictionary::setFloat(Item* item, float value)
{
    setFloat64(item, static_cast<double>(value));
}
inline Item* IDictionary::makeFloatAtPath(Item* baseItem, const char* path, float value)
{
    return makeFloat64AtPath(baseItem, path, static_cast<double>(value));
}

inline int32_t IDictionary::getAsIntAt(const Item* item, size_t index)
{
    return static_cast<int32_t>(getAsInt64At(item, index));
}

inline void IDictionary::setIntAt(Item* item, size_t index, int32_t value)
{
    setInt64At(item, index, static_cast<int64_t>(value));
}

inline float IDictionary::getAsFloatAt(const Item* item, size_t index)
{
    return static_cast<float>(getAsFloat64At(item, index));
}

inline void IDictionary::setFloatAt(Item* item, size_t index, float value)
{
    setFloat64At(item, index, static_cast<double>(value));
}

inline Item* IDictionary::makeInt64AtPath(Item* parentItem, const char* path, int64_t value)
{
    ScopedWrite g(*this, parentItem);
    Item* item = getItemMutable(parentItem, path);
    if (!item)
    {
        item = createItem(parentItem, path, ItemType::eInt);
    }
    setInt64(item, value);
    return item;
}

inline Item* IDictionary::makeFloat64AtPath(Item* parentItem, const char* path, double value)
{
    ScopedWrite g(*this, parentItem);
    Item* item = getItemMutable(parentItem, path);
    if (!item)
    {
        item = createItem(parentItem, path, ItemType::eFloat);
    }
    setFloat64(item, value);
    return item;
}

inline Item* IDictionary::makeBoolAtPath(Item* parentItem, const char* path, bool value)
{
    ScopedWrite g(*this, parentItem);
    Item* item = getItemMutable(parentItem, path);
    if (!item)
    {
        item = createItem(parentItem, path, ItemType::eBool);
    }
    setBool(item, value);
    return item;
}

inline Item* IDictionary::makeStringAtPath(Item* parentItem, const char* path, const char* value, size_t stringLen)
{
    ScopedWrite g(*this, parentItem);
    Item* item = getItemMutable(parentItem, path);
    if (!item)
    {
        item = createItem(parentItem, path, ItemType::eString);
    }
    setString(item, value, stringLen);
    return item;
}

inline Item* IDictionary::makeDictionaryAtPath(Item* parentItem, const char* path)
{
    ScopedWrite g(*this, parentItem);
    Item* item = getItemMutable(parentItem, path);
    if (!item)
    {
        item = createItem(parentItem, path, ItemType::eDictionary);
        return item;
    }
    ItemType itemType = getItemType(item);
    if (itemType != ItemType::eDictionary)
    {
        destroyItem(item);
        item = createItem(parentItem, path, ItemType::eDictionary);
    }
    return item;
}

#ifndef DOXYGEN_BUILD
template <>
inline int32_t IDictionary::get<int32_t>(const dictionary::Item* item)
{
    return getAsInt(item);
}

template <>
inline int64_t IDictionary::get<int64_t>(const dictionary::Item* item)
{
    return getAsInt64(item);
}

template <>
inline float IDictionary::get<float>(const dictionary::Item* item)
{
    return getAsFloat(item);
}

template <>
inline double IDictionary::get<double>(const dictionary::Item* item)
{
    return getAsFloat64(item);
}

template <>
inline bool IDictionary::get<bool>(const dictionary::Item* item)
{
    return getAsBool(item);
}

template <>
inline const char* IDictionary::get<const char*>(const dictionary::Item* item)
{
    return getStringBuffer(item);
}

template <>
inline Int2 IDictionary::get<Int2>(const dictionary::Item* item)
{
    Int2 value;
    getAsIntArray(item, &value.x, 2);
    return value;
}
template <>
inline Int3 IDictionary::get<Int3>(const dictionary::Item* item)
{
    Int3 value;
    getAsIntArray(item, &value.x, 3);
    return value;
}
template <>
inline Int4 IDictionary::get<Int4>(const dictionary::Item* item)
{
    Int4 value;
    getAsIntArray(item, &value.x, 4);
    return value;
}

template <>
inline Uint2 IDictionary::get<Uint2>(const dictionary::Item* item)
{
    int64_t value[2];
    getAsInt64Array(item, value, 2);
    return { static_cast<uint32_t>(value[0]), static_cast<uint32_t>(value[1]) };
}
template <>
inline Uint3 IDictionary::get<Uint3>(const dictionary::Item* item)
{
    int64_t value[3];
    getAsInt64Array(item, value, 3);
    return { static_cast<uint32_t>(value[0]), static_cast<uint32_t>(value[1]), static_cast<uint32_t>(value[2]) };
}
template <>
inline Uint4 IDictionary::get<Uint4>(const dictionary::Item* item)
{
    int64_t value[4];
    getAsInt64Array(item, value, 4);
    return { static_cast<uint32_t>(value[0]), static_cast<uint32_t>(value[1]), static_cast<uint32_t>(value[2]),
             static_cast<uint32_t>(value[3]) };
}

template <>
inline Float2 IDictionary::get<Float2>(const dictionary::Item* item)
{
    Float2 value;
    getAsFloatArray(item, &value.x, 2);
    return value;
}
template <>
inline Float3 IDictionary::get<Float3>(const dictionary::Item* item)
{
    Float3 value;
    getAsFloatArray(item, &value.x, 3);
    return value;
}
template <>
inline Float4 IDictionary::get<Float4>(const dictionary::Item* item)
{
    Float4 value;
    getAsFloatArray(item, &value.x, 4);
    return value;
}

template <>
inline Double2 IDictionary::get<Double2>(const dictionary::Item* item)
{
    Double2 value;
    getAsFloat64Array(item, &value.x, 2);
    return value;
}
template <>
inline Double3 IDictionary::get<Double3>(const dictionary::Item* item)
{
    Double3 value;
    getAsFloat64Array(item, &value.x, 3);
    return value;
}
template <>
inline Double4 IDictionary::get<Double4>(const dictionary::Item* item)
{
    Double4 value;
    getAsFloat64Array(item, &value.x, 4);
    return value;
}


template <class T>
inline T IDictionary::get(const dictionary::Item* baseItem, const char* path)
{
    return get<T>(getItem(baseItem, path));
}

template <>
inline void IDictionary::makeAtPath<int32_t>(dictionary::Item* baseItem, const char* path, int32_t value)
{
    makeIntAtPath(baseItem, path, value);
}

template <>
inline void IDictionary::makeAtPath<int64_t>(dictionary::Item* baseItem, const char* path, int64_t value)
{
    makeInt64AtPath(baseItem, path, value);
}

template <>
inline void IDictionary::makeAtPath<float>(dictionary::Item* baseItem, const char* path, float value)
{
    makeFloatAtPath(baseItem, path, value);
}

template <>
inline void IDictionary::makeAtPath<double>(dictionary::Item* baseItem, const char* path, double value)
{
    makeFloat64AtPath(baseItem, path, value);
}

template <>
inline void IDictionary::makeAtPath<bool>(dictionary::Item* baseItem, const char* path, bool value)
{
    makeBoolAtPath(baseItem, path, value);
}

template <>
inline void IDictionary::makeAtPath<const char*>(dictionary::Item* baseItem, const char* path, const char* value)
{
    makeStringAtPath(baseItem, path, value);
}

template <>
inline void IDictionary::makeAtPath<std::string>(dictionary::Item* baseItem, const char* path, std::string value)
{
    makeStringAtPath(baseItem, path, value.data(), value.size());
}

template <>
inline void IDictionary::makeAtPath<cpp17::string_view>(dictionary::Item* baseItem,
                                                        const char* path,
                                                        cpp17::string_view value)
{
    makeStringAtPath(baseItem, path, value.data(), value.length());
}

template <>
inline void IDictionary::makeAtPath<omni::string>(dictionary::Item* baseItem, const char* path, omni::string value)
{
    makeStringAtPath(baseItem, path, value.data(), value.size());
}

template <>
inline void IDictionary::setArray(Item* baseItem, const bool* array, size_t arrayLength)
{
    setBoolArray(baseItem, array, arrayLength);
}

template <>
inline void IDictionary::setArray(Item* baseItem, const int32_t* array, size_t arrayLength)
{
    setIntArray(baseItem, array, arrayLength);
}

template <>
inline void IDictionary::setArray(Item* baseItem, const int64_t* array, size_t arrayLength)
{
    setInt64Array(baseItem, array, arrayLength);
}

template <>
inline void IDictionary::setArray(Item* baseItem, const float* array, size_t arrayLength)
{
    setFloatArray(baseItem, array, arrayLength);
}

template <>
inline void IDictionary::setArray(Item* baseItem, const double* array, size_t arrayLength)
{
    setFloat64Array(baseItem, array, arrayLength);
}

template <>
inline void IDictionary::setArray(Item* baseItem, const char* const* array, size_t arrayLength)
{
    setStringArray(baseItem, array, arrayLength);
}

template <>
inline void IDictionary::makeAtPath<Int2>(dictionary::Item* baseItem, const char* path, Int2 value)
{
    dictionary::Item* item = baseItem;
    if (path && path[0] != '\0')
    {
        item = makeDictionaryAtPath(baseItem, path);
    }
    setArray<int32_t>(item, &value.x, 2);
}
template <>
inline void IDictionary::makeAtPath<Int3>(dictionary::Item* baseItem, const char* path, Int3 value)
{
    dictionary::Item* item = baseItem;
    if (path && path[0] != '\0')
    {
        item = makeDictionaryAtPath(baseItem, path);
    }
    setArray<int32_t>(item, &value.x, 3);
}
template <>
inline void IDictionary::makeAtPath<Int4>(dictionary::Item* baseItem, const char* path, Int4 value)
{
    dictionary::Item* item = baseItem;
    if (path && path[0] != '\0')
    {
        item = makeDictionaryAtPath(baseItem, path);
    }
    setArray<int32_t>(item, &value.x, 4);
}

template <>
inline void IDictionary::makeAtPath<Uint2>(dictionary::Item* baseItem, const char* path, Uint2 value)
{
    dictionary::Item* item = baseItem;
    if (path && path[0] != '\0')
    {
        item = makeDictionaryAtPath(baseItem, path);
    }

    int64_t values64[] = { value.x, value.y };

    setArray<int64_t>(item, values64, 2);
}
template <>
inline void IDictionary::makeAtPath<Uint3>(dictionary::Item* baseItem, const char* path, Uint3 value)
{
    dictionary::Item* item = baseItem;
    if (path && path[0] != '\0')
    {
        item = makeDictionaryAtPath(baseItem, path);
    }

    int64_t values64[] = { value.x, value.y, value.z };

    setArray<int64_t>(item, values64, 3);
}
template <>
inline void IDictionary::makeAtPath<Uint4>(dictionary::Item* baseItem, const char* path, Uint4 value)
{
    dictionary::Item* item = baseItem;
    if (path && path[0] != '\0')
    {
        item = makeDictionaryAtPath(baseItem, path);
    }

    int64_t values64[] = { value.x, value.y, value.z, value.w };

    setArray<int64_t>(item, values64, 4);
}


template <>
inline void IDictionary::makeAtPath<Float2>(dictionary::Item* baseItem, const char* path, Float2 value)
{
    dictionary::Item* item = baseItem;
    if (path && path[0] != '\0')
    {
        item = makeDictionaryAtPath(baseItem, path);
    }
    setArray<float>(item, &value.x, 2);
}
template <>
inline void IDictionary::makeAtPath<Float3>(dictionary::Item* baseItem, const char* path, Float3 value)
{
    dictionary::Item* item = baseItem;
    if (path && path[0] != '\0')
    {
        item = makeDictionaryAtPath(baseItem, path);
    }
    setArray<float>(item, &value.x, 3);
}
template <>
inline void IDictionary::makeAtPath<Float4>(dictionary::Item* baseItem, const char* path, Float4 value)
{
    dictionary::Item* item = baseItem;
    if (path && path[0] != '\0')
    {
        item = makeDictionaryAtPath(baseItem, path);
    }
    setArray<float>(item, &value.x, 4);
}

template <>
inline void IDictionary::makeAtPath<Double2>(dictionary::Item* baseItem, const char* path, Double2 value)
{
    dictionary::Item* item = baseItem;
    if (path && path[0] != '\0')
    {
        item = makeDictionaryAtPath(baseItem, path);
    }
    setArray<double>(item, &value.x, 2);
}
template <>
inline void IDictionary::makeAtPath<Double3>(dictionary::Item* baseItem, const char* path, Double3 value)
{
    dictionary::Item* item = baseItem;
    if (path && path[0] != '\0')
    {
        item = makeDictionaryAtPath(baseItem, path);
    }
    setArray<double>(item, &value.x, 3);
}
template <>
inline void IDictionary::makeAtPath<Double4>(dictionary::Item* baseItem, const char* path, Double4 value)
{
    dictionary::Item* item = baseItem;
    if (path && path[0] != '\0')
    {
        item = makeDictionaryAtPath(baseItem, path);
    }
    setArray<double>(item, &value.x, 4);
}
#endif

inline void IDictionary::deleteChildren(Item* item)
{
    ScopedWrite g(*this, item);
    size_t childCount = getItemChildCount(item);
    while (childCount != 0)
        destroyItem(getItemChildByIndexMutable(item, --childCount));
}

inline void IDictionary::copyItemFlags(Item* dstItem, const Item* srcItem)
{
    setItemFlag(dstItem, ItemFlag::eUnitSubtree, getItemFlag(srcItem, ItemFlag::eUnitSubtree));
}

} // namespace dictionary
} // namespace carb
