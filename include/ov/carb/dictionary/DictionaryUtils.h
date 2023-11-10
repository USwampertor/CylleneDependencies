// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//! @brief Utility helper functions for common dictionary operations.
#pragma once

#include "../Framework.h"
#include "../InterfaceUtils.h"
#include "../datasource/IDataSource.h"
#include "../extras/CmdLineParser.h"
#include "../filesystem/IFileSystem.h"
#include "../logging/Log.h"
#include "IDictionary.h"
#include "ISerializer.h"

#include <algorithm>
#include <string>

namespace carb
{
/** Namespace for @ref carb::dictionary::IDictionary related interfaces and helpers. */
namespace dictionary
{

/** helper function to retrieve the IDictionary interface.
 *
 *  @returns The cached @ref carb::dictionary::IDictionary interface.  This will be cached
 *           until the plugin is unloaded.
 */
inline IDictionary* getCachedDictionaryInterface()
{
    return getCachedInterface<IDictionary>();
}

/** Prototype for a callback function used to walk items in a dictionary.
 *
 *  @tparam ElementData     An arbitrary data type used as both a parameter and the return
 *                          value of the callback.  The callback itself is assumed to know
 *                          how to interpret and use this value.
 *  @param[in] srcItem      The current item being visited.  This will never be `nullptr`.
 *  @param[in] elementData  An arbitrary data object passed into the callback by the caller of
 *                          walkDictionary().  The callback is assumed that it knows how to
 *                          interpret and use this value.
 *  @param[in] userData     An opaque data object passed by the caller of walkDictionary().
 *                          The callback is assumed that it knows how to interpret and use
 *                          this object.
 *  @returns An \a ElementData object or value to pass back to the dictionary walker.  When
 *           the callback returns from passing in a new dictionary value (ie: a child of the
 *           original dictionary), this value is stored and passed on to following callbacks.
 */
template <typename ElementData>
using OnItemFn = ElementData (*)(const Item* srcItem, ElementData elementData, void* userData);

/** Prototype for a callback function used to walk children in a dictionary.
 *
 *  @tparam ItemPtrType     The data type of the dictionary item in the dictionary being walked.
 *                          This should be either `Item` or `const Item`.
 *  @param[in] dict         The @ref IDictionary interface being used to access the items in the
 *                          dictionary during the walk.  This must not be `nullptr`.
 *  @param[in] item         The dictionary item to retrieve one of the child items from.  This
 *                          must not be `nullptr`.  This is assumed to be an item of type
 *                          @ref ItemType::eDictionary.
 *  @param[in] idx          The zero based index of the child item of @p item to retrieve.  This
 *                          is expected to be within the range of the number of children in the
 *                          given dictionary item.
 *  @returns The child item at the requested index in the given dictionary item @p item.  Returns
 *           `nullptr` if the given index is out of range of the number of children in the given
 *           dictionary item.
 *
 *  @remarks This callback provides a way to control the order in which the items in a dictionary
 *           are walked.  An basic implementation is provided below.
 */
template <typename ItemPtrType>
inline ItemPtrType* getChildByIndex(IDictionary* dict, ItemPtrType* item, size_t idx);

/** Specialization for the getChildByIndex() callback that implements a simple retrieval of the
 *  requested child item using @ref IDictionary::getItemChildByIndex().
 *
 *  @sa getChildByIndex(IDictionary*,ItemPtrType*,size_t).
 */
template <>
inline const Item* getChildByIndex(IDictionary* dict, const Item* item, size_t idx)
{
    return dict->getItemChildByIndex(item, idx);
}

/** Mode names for the ways to walk the requested dictionary. */
enum class WalkerMode
{
    /** When walking the dictionary, include the root item itself. */
    eIncludeRoot,

    /** When walking the dictionary, skip the root item and start with the enumeration with
     *  the immediate children of the root item.
     */
    eSkipRoot
};

/** Walk a dictionary item to enumerate all of its values.
 *
 *  @tparam ElementData             The data type for the per-item element data that is maintained
 *                                  during the walk.  This can be used for example to track which
 *                                  level of the dictionary a given item is at by using an `int`
 *                                  type here.
 *  @tparam OnItemFnType            The type for the @p onItemFn callback function.
 *  @tparam ItemPtrType             The type used for the item type in the @p GetChildByIndexFuncType
 *                                  callback.  This must either be `const Item` or `Item`.  This
 *                                  defaults to `const Item`.  If a non-const type is used here
 *                                  it is possible that items' values could be modified during the
 *                                  walk.  Using a non-const value is discouraged however since it
 *                                  can lead to unsafe use or undefined behavior.
 *  @tparam GetChildByIndexFuncType The type for the @p getChildByIndexFunc callback function.
 *  @param[in] dict                 The @ref IDictionary interface to use to access the items
 *                                  in the dictionary.  This must not be `nullptr`.  This must
 *                                  be the same interface that was originally used to create
 *                                  the dictionary @p root being walked.
 *  @param[in] walkerMode           The mode to walk the given dictionary in.
 *  @param[in] root                 The root dictionary to walk.  This must not be `nullptr`.
 *  @param[in] rootElementData      The user specified element data value that is to be associated
 *                                  with the @p root element.  This value can be changed during
 *                                  the walk by the @p onItemFn callback function.
 *  @param[in] onItemFn             The callback function that is performed for each value in the
 *                                  given dictionary.  The user specified element data value can
 *                                  be modified on each non-leaf item.  This modified element data
 *                                  value is then passed to all further children of the given
 *                                  item.  The element data value returned for leaf items is
 *                                  discarded.  This must not be `nullptr`.
 *  @param[in] userData             Opaque user data object that is passed to each @p onItemFn
 *                                  callback.  The caller is responsible for knowing how to
 *                                  interpret and access this value.
 *  @param[in] getChildByIndexFunc  Callback function to enumerate the children of a given
 *                                  item in the dictionary being walked.  This must not be
 *                                  `nullptr`.  This can be used to either control the order
 *                                  in which the child items are enumerated (ie: sort them
 *                                  before returning), or to return them as non-const objects
 *                                  so that the item's value can be changed during enumeration.
 *                                  Attempting to insert or remove items by using a non-const
 *                                  child enumerator is unsafe and will generally result in
 *                                  undefined behavior.
 *  @returns No return value.
 *
 *  @remarks This walks a dictionary and enumerates all of its values of all types.  This
 *           includes even @ref ItemType::eDictionary items.  Non-leaf items in the walk will
 *           be passed to the @p onItemFn callback before walking through its children.
 *           The @p getChildByIndexFunc callback function can be used to control the order in
 *           which the children of each level of the dictionary are enumerated.  The default
 *           implementation simply enumerates the items in the order they are stored in (which
 *           is generally arbitrary).  The dictionary's full tree is walked in a depth first
 *           manner so sibling items are not guaranteed to be enumerated consecutively.
 *
 *  @thread_safety This function is thread safe as long as nothing else is concurrently modifying
 *                 the dictionary being walked.  It is the caller's responsibility to ensure that
 *                 neither the dictionary nor any of its children will be modified until the walk
 *                 is complete.
 */
template <typename ElementData,
          typename OnItemFnType,
          typename ItemPtrType = const Item,
          typename GetChildByIndexFuncType CARB_NO_DOC(= decltype(getChildByIndex<ItemPtrType>))>
inline void walkDictionary(IDictionary* dict,
                           WalkerMode walkerMode,
                           ItemPtrType* root,
                           ElementData rootElementData,
                           OnItemFnType onItemFn,
                           void* userData,
                           GetChildByIndexFuncType getChildByIndexFunc = getChildByIndex<ItemPtrType>)
{
    if (!root)
    {
        return;
    }

    struct ValueToParse
    {
        ItemPtrType* srcItem;
        ElementData elementData;
    };

    std::vector<ValueToParse> valuesToParse;
    valuesToParse.reserve(100);

    if (walkerMode == WalkerMode::eSkipRoot)
    {
        size_t numChildren = dict->getItemChildCount(root);
        for (size_t chIdx = 0; chIdx < numChildren; ++chIdx)
        {
            valuesToParse.push_back({ getChildByIndexFunc(dict, root, numChildren - chIdx - 1), rootElementData });
        }
    }
    else
    {
        valuesToParse.push_back({ root, rootElementData });
    }

    while (valuesToParse.size())
    {
        const ValueToParse valueToParse = valuesToParse.back();
        ItemPtrType* curItem = valueToParse.srcItem;
        ItemType curItemType = dict->getItemType(curItem);
        valuesToParse.pop_back();

        if (curItemType == ItemType::eDictionary)
        {
            size_t numChildren = dict->getItemChildCount(curItem);
            ElementData elementData = onItemFn(curItem, valueToParse.elementData, userData);
            for (size_t chIdx = 0; chIdx < numChildren; ++chIdx)
            {
                valuesToParse.push_back({ getChildByIndexFunc(dict, curItem, numChildren - chIdx - 1), elementData });
            }
        }
        else
        {
            onItemFn(curItem, valueToParse.elementData, userData);
        }
    }
}

/** Attempts to retrieve the name of an item from a given path in a dictionary.
 *
 *  @param[in] dict     The @ref IDictionary interface to use to access the items in the
 *                      dictionary.  This must not be `nullptr`.  This must be the same
 *                      interface that was originally used to create the dictionary
 *                      @p baseItem.
 *  @param[in] baseItem The base item to retrieve the item name relative to.  This is expected
 *                      to contain the child path @p path.  This may not be `nullptr`.
 *  @param[in] path     The item path relative to @p baseItem that indicates where to find the
 *                      item whose name should be retrieved.  This may be `nullptr` to retrieve
 *                      the name of @p baseItem itself.
 *  @returns A string containing the name of the item at the given path relative to @p baseItem
 *           if it exists.  Returns an empty string if no item could be found at the requested
 *           path or a string buffer could not be allocated for its name.
 *
 *  @thread_safety This call is thread safe.
 */
inline std::string getStringFromItemName(const IDictionary* dict, const Item* baseItem, const char* path = nullptr)
{
    const Item* item = dict->getItem(baseItem, path);
    if (!item)
    {
        return std::string();
    }
    const char* itemNameBuf = dict->createStringBufferFromItemName(item);
    std::string returnString = itemNameBuf;
    dict->destroyStringBuffer(itemNameBuf);
    return returnString;
}

/** Attempts to retrieve the value of an item from a given path in a dictionary.
 *
 *  @param[in] dict     The @ref IDictionary interface to use to access the items in the
 *                      dictionary.  This must not be `nullptr`.  This must be the same
 *                      interface that was originally used to create the dictionary
 *                      @p baseItem.
 *  @param[in] baseItem The base item to retrieve the item value relative to.  This is expected
 *                      to contain the child path @p path.  This may not be `nullptr`.
 *  @param[in] path     The item path relative to @p baseItem that indicates where to find the
 *                      item whose value should be retrieved.  This may be `nullptr` to retrieve
 *                      the value of @p baseItem itself.
 *  @returns A string containing the value of the item at the given path relative to @p baseItem
 *           if it exists.  If the requested item was not of type @ref ItemType::eString, the
 *           value will be converted to a string as best it can.  Returns an empty string if no
 *           item could be found at the requested path or a string buffer could not be allocated
 *           for its name.
 *
 *  @thread_safety This call is thread safe.
 */
inline std::string getStringFromItemValue(const IDictionary* dict, const Item* baseItem, const char* path = nullptr)
{
    const Item* item = dict->getItem(baseItem, path);
    if (!item)
    {
        return std::string();
    }
    const char* stringBuf = dict->createStringBufferFromItemValue(item);
    std::string returnString = stringBuf;
    dict->destroyStringBuffer(stringBuf);
    return returnString;
}

/** Attempts to retrieve an array of string values from a given dictionary path.
 *
 *  @param[in] dict     The @ref IDictionary interface to use to access the items in the
 *                      dictionary.  This must not be `nullptr`.  This must be the same
 *                      interface that was originally used to create the dictionary
 *                      @p baseItem.
 *  @param[in] baseItem The base item to retrieve the item values relative to.  This is expected
 *                      to contain the child path @p path.  This may not be `nullptr`.
 *  @param[in] path     The item path relative to @p baseItem that indicates where to find the
 *                      item whose value should be retrieved.  This may be `nullptr` to retrieve
 *                      the values of @p baseItem itself.  The value at this path is expected to
 *                      be an array of strings.
 *  @returns A vector of string values for the array at the path @p path relative to @p baseItem.
 *           If the given path is not an array item, a vector containing a single value will be
 *           returned.  If @p path points to an item that is an array of something other than
 *           strings, a vector of empty strings will be returned instead.
 *
 *  @thread_safety This call in itself is thread safe, however the retrieved array may contain
 *                 unexpected or incorrect values if another thread is modifying the same item
 *                 in the dictionary simultaneously.
 */
inline std::vector<std::string> getStringArray(const IDictionary* dict, const Item* baseItem, const char* path)
{
    const Item* itemAtKey = dict->getItem(baseItem, path);
    std::vector<std::string> stringArray(dict->getArrayLength(itemAtKey));
    for (size_t i = 0; i < stringArray.size(); i++)
    {
        stringArray[i] = dict->getStringBufferAt(itemAtKey, i);
    }
    return stringArray;
}

/** Attempts to retrieve an array of string values from a given dictionary path.
 *
 *  @param[in] dict     The @ref IDictionary interface to use to access the items in the
 *                      dictionary.  This must not be `nullptr`.  This must be the same
 *                      interface that was originally used to create the dictionary
 *                      @p baseItem.
 *  @param[in] item     The base item to retrieve the item values relative to.  This is expected
 *                      to contain the child path @p path.  This may not be `nullptr`.
 *  @returns A vector of string values for the array at the path @p path relative to @p baseItem.
 *           If the given path is not an array item, a vector containing a single value will be
 *           returned.  If @p path points to an item that is an array of something other than
 *           strings, a vector of empty strings will be returned instead.
 *
 *  @thread_safety This call in itself is thread safe, however the retrieved array may contain
 *                 unexpected or incorrect values if another thread is modifying the same item
 *                 in the dictionary simultaneously.
 */
inline std::vector<std::string> getStringArray(const IDictionary* dict, const Item* item)
{
    return getStringArray(dict, item, nullptr);
}

/** Sets an array of values at a given path relative to a dictionary item.
 *
 *  @param[in] dict         The @ref IDictionary interface to use to access the items in the
 *                          dictionary.  This must not be `nullptr`.  This must be the same
 *                          interface that was originally used to create the dictionary
 *                          @p baseItem.
 *  @param[in] baseItem     The base item to act as the root of where to set the values relative
 *                          to.  This is expected to contain the child path @p path.  This may not
 *                          be `nullptr`.
 *  @param[in] path         The path to the item to set the array of strings in.  This path must
 *                          either already exist as an array of strings or be an empty item in the
 *                          dictionary.  This may be `nullptr` to set the string array into the
 *                          item @p baseItem itself.
 *  @param[in] stringArray  The array of strings to set in the dictionary.  This may contain a
 *                          different number of items than the existing array.  If the number of
 *                          items differs, this new array of values will replace the existing
 *                          item at @p path entirely.  If the count is the same as the previous
 *                          item, values will simply be replaced.
 *  @returns No return value.
 *
 *  @thread_safety This call itself is thread safe as long as no other call is trying to
 *                 concurrently modify the same item in the dictionary.  Results are undefined
 *                 if another thread is modifying the same item in the dictionary.  Similarly,
 *                 undefined behavior may result if another thread is concurrently trying to
 *                 retrieve the items from this same dictionary.
 */
inline void setStringArray(IDictionary* dict, Item* baseItem, const char* path, const std::vector<std::string>& stringArray)
{
    Item* itemAtKey = dict->getItemMutable(baseItem, path);
    if (dict->getItemType(itemAtKey) != dictionary::ItemType::eCount)
    {
        dict->destroyItem(itemAtKey);
    }
    for (size_t i = 0, stringCount = stringArray.size(); i < stringCount; ++i)
    {
        dict->setStringAt(itemAtKey, i, stringArray[i].c_str());
    }
}

/** Sets an array of values at a given path relative to a dictionary item.
 *
 *  @param[in] dict         The @ref IDictionary interface to use to access the items in the
 *                          dictionary.  This must not be `nullptr`.  This must be the same
 *                          interface that was originally used to create the dictionary
 *                          @p baseItem.
 *  @param[in] item         The base item to act as the root of where to set the values relative
 *                          to.  This is expected to contain the child path @p path.  This may not
 *                          be `nullptr`.
 *  @param[in] stringArray  The array of strings to set in the dictionary.  This may contain a
 *                          different number of items than the existing array.  If the number of
 *                          items differs, this new array of values will replace the existing
 *                          item at @p path entirely.  If the count is the same as the previous
 *                          item, values will simply be replaced.
 *  @returns No return value.
 *
 *  @thread_safety This call itself is thread safe as long as no other call is trying to
 *                 concurrently modify the same item in the dictionary.  Results are undefined
 *                 if another thread is modifying the same item in the dictionary.  Similarly,
 *                 undefined behavior may result if another thread is concurrently trying to
 *                 retrieve the items from this same dictionary.
 */
inline void setStringArray(IDictionary* dict, Item* item, const std::vector<std::string>& stringArray)
{
    setStringArray(dict, item, nullptr, stringArray);
}

/** Attempts to set a value in a dictionary with an attempt to detect the value type.
 *
 *  @param[in] id       The @ref IDictionary interface to use to access the items in the
 *                      dictionary.  This must not be `nullptr`.  This must be the same
 *                      interface that was originally used to create the dictionary
 *                      @p dict.
 *  @param[in] dict     The base item to act as the root of where to set the value relative
 *                      to.  This may not be `nullptr`.
 *  @param[in] path     The path to the item to set the value in.  This path does not need to
 *                      exist yet in the dictionary.  This may be `nullptr` to create the new
 *                      value in the @p dict item itself.
 *  @param[in] value    The new value to set expressed as a string.  An attempt will be made to
 *                      detect the type of the data from the contents of the string.  If there
 *                      are surrounding quotation marks, it will be treated as a string.  If the
 *                      value is a case insensitive variant on `FALSE` or `TRUE`, it will be
 *                      treated as a boolean value.  If the value fully converts to an integer or
 *                      floating point value, it will be treated as those types.  Otherwise the
 *                      value is stored unmodified as a string.
 *  @returns No return value.
 *
 *  @thread_safety This call is thread safe.
 */
inline void setDictionaryElementAutoType(IDictionary* id, Item* dict, const std::string& path, const std::string& value)
{
    if (!path.empty())
    {
        // We should validate that provided path is a proper path but for now we just use it
        //
        // Simple rules to support basic values:
        // if the value starts and with quotes (" or ') then it's the string inside the quotes
        // else if we can parse the value as a bool, int or float then we read it
        // according to the type. Otherwise we consider it to be a string.

        // Special case, if the string is empty, write an empty string early
        if (value.empty())
        {
            constexpr const char* kEmptyString = "";
            id->makeStringAtPath(dict, path.c_str(), kEmptyString);
            return;
        }

        if (value.size() > 1 &&
            ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\'')))
        {
            // string value - chop off quotes
            id->makeStringAtPath(dict, path.c_str(), value.substr(1, value.size() - 2).c_str());
            return;
        }

        // Convert the value to upper case to simplify checks
        std::string uppercaseValue = value;
        std::transform(value.begin(), value.end(), uppercaseValue.begin(),
                       [](const char c) { return static_cast<char>(::toupper(c)); });

        // let's see if it's a boolean
        if (uppercaseValue == "TRUE")
        {
            id->makeBoolAtPath(dict, path.c_str(), true);
            return;
        }
        if (uppercaseValue == "FALSE")
        {
            id->makeBoolAtPath(dict, path.c_str(), false);
            return;
        }

        // let's see if it's an integer
        size_t valueLen = value.length();
        char* endptr;
        // Use a radix of 0 to allow for decimal, octal, and hexadecimal values to all be parsed.
        const long long int valueAsInt = strtoll(value.c_str(), &endptr, 0);

        if (endptr - value.c_str() == (ptrdiff_t)valueLen)
        {
            id->makeInt64AtPath(dict, path.c_str(), valueAsInt);
            return;
        }
        // let's see if it's a float
        const double valueAsFloat = strtod(value.c_str(), &endptr);
        if (endptr - value.c_str() == (ptrdiff_t)valueLen)
        {
            id->makeFloat64AtPath(dict, path.c_str(), valueAsFloat);
            return;
        }

        // consider the value to be a string even if it's empty
        id->makeStringAtPath(dict, path.c_str(), value.c_str());
    }
}

/** Sets a series of values in a dictionary based on keys and values in a map object.
 *
 *  @param[in] id       The @ref IDictionary interface to use to access the items in the
 *                      dictionary.  This must not be `nullptr`.  This must be the same
 *                      interface that was originally used to create the dictionary
 *                      @p dict.
 *  @param[in] dict     The base item to act as the root of where to set the values relative
 *                      to.  This may not be `nullptr`.
 *  @param[in] mapping  A map containing item paths (as the map keys) and their values to be
 *                      set in the dictionary.
 *  @returns No return value.
 *
 *  @remarks This takes a map of path and value pairs and sets those values into the given
 *           dictionary @p dict.  Each entry in the map identifies a potential new value to
 *           create in the dictionary.  The paths to each of the new values do not have to
 *           already exist in the dictionary.  The new items will be created as needed.  If
 *           a given path already exists in the dictionary, its value is replaced with the
 *           one from the map.  All values will attempt to auto-detect their type based on
 *           the content of the string value.  See setDictionaryElementAutoType() for more
 *           info on how the types are detected.
 *
 *  @note If the map contains entries for an array and the array also exists in the dictionary,
 *        the resulting dictionary could have more or fewer elements in the array entries if the
 *        map either contained fewer items than the previous array's size or contained a
 *        non-consecutive set of numbered elements in the array.  If the array already exists in
 *        the dictionary, it will not be destroyed or removed before adding the new values.
 *
 *  @thread_safety This itself operation is thread safe, but a race condition may still exist
 *                 if multiple threads are trying to set the values for the same set of items
 *                 simultaneously.  The operation will succeed, but the value that gets set in
 *                 each item in the end is undefined.
 */
inline void setDictionaryFromStringMapping(IDictionary* id, Item* dict, const std::map<std::string, std::string>& mapping)
{
    for (const auto& kv : mapping)
    {
        setDictionaryElementAutoType(id, dict, kv.first, kv.second);
    }
}

/** Parses a set of command line arguments for dictionary items arguments and sets them.
 *
 *  @param[in] id       The @ref IDictionary interface to use to access the items in the
 *                      dictionary.  This must not be `nullptr`.  This must be the same
 *                      interface that was originally used to create the dictionary
 *                      @p dict.
 *  @param[in] dict     The base item to act as the root of where to set the values relative
 *                      to.  This may not be `nullptr`.
 *  @param[in] argv     The Unix style argument array for the command line to the process.  This
 *                      must not be `nullptr`.  The first entry in this array is expected to be
 *                      the process's name.  Only the arguments starting with @p prefix will be
 *                      parsed here.
 *  @param[in] argc     The Unix style argument count for the total number of items in @p argv
 *                      to parse.
 *  @param[in] prefix   A string indicating the prefix of arguments that should be parsed by this
 *                      operation.  This may not be `nullptr`.  Arguments that do not start with
 *                      this prefix will simply be ignored.
 *  @returns No return value.
 *
 *  @remarks This parses command line arguments to find ones that should be added to a settings
 *           dictionary.  Only arguments beginning with the given prefix will be added.  The
 *           type of each individual item added to the dictionary will be automatically detected
 *           based on the same criteria used for setDictionaryElementAutoType().
 *
 *  @thread_safety This operation itself is thread safe, but a race condition may still exist
 *                 if multiple threads are trying to set the values for the same set of items
 *                 simultaneously.  The operation will succeed, but the value that gets set in
 *                 each item in the end is undefined.
 */
inline void setDictionaryFromCmdLine(IDictionary* id, Item* dict, char** argv, int argc, const char* prefix = "--/")
{
    carb::extras::CmdLineParser cmdLineParser(prefix);
    cmdLineParser.parse(argv, argc);

    const std::map<std::string, std::string>& opts = cmdLineParser.getOptions();
    setDictionaryFromStringMapping(id, dict, opts);
}

/** Parses a string representation of an array and sets it relative to a dictionary path.
 *
 *  @param[in] dictionaryInterface  The @ref IDictionary interface to use to access the items in
 *                                  the dictionary.  This must not be `nullptr`.  This must be the
 *                                  same interface that was originally used to create the
 *                                  dictionary @p targetDictionary.
 *  @param[in] targetDictionary     The base item to act as the root of where to set the values
 *                                  relative to.  This may not be `nullptr`.
 *  @param[in] elementPath          The path to the item to set the values in.  This path does not
 *                                  need to exist yet in the dictionary.  This may be an empty
 *                                  string to create the new values in the @p dict item itself.
 *                                  Any item at this path will be completely overwritten by this
 *                                  operation.
 *  @param[in] elementValue         The string containing the values to parse for the new array
 *                                  value.  These are expected to be expressed in the format
 *                                  "[<value1>, <value2>, <value3>, ...]" (ie: all values enclosed
 *                                  in a single set of square brackets with each individual value
 *                                  separated by commas).  Individual value strings may not
 *                                  contain a comma (even if escaped or surrounded by quotation
 *                                  marks) otherwise they will be seen as separate values and
 *                                  likely not set appropriately in the dictionary.  This must
 *                                  not be an empty string and must contain at least the square
 *                                  brackets at either end of the string.
 *  @returns No return value.
 *
 *  @remarks This parses an array of values from a string into a dictionary array item.  The array
 *           string is expected to have the format "[<value1>, <value2>, <value3>, ...]".  Quoted
 *           values are not respected if they contain internal commas (the comma is still seen as
 *           a value separator in this case).  Each value parsed from the array will be set in the
 *           dictionary item with its data type detected from its content.  This detection is done
 *           in the same manner as in setDictionaryElementAutoType().
 *
 *  @thread_safety This call itself is thread safe.  However, if another thread is simultaneously
 *                 attempting to modify, retrieve, or delete items or values in the same branch of
 *                 the dictionary, the results may be undefined.
 */
inline void setDictionaryArrayElementFromStringValue(dictionary::IDictionary* dictionaryInterface,
                                                     dictionary::Item* targetDictionary,
                                                     const std::string& elementPath,
                                                     const std::string& elementValue)
{
    if (elementPath.empty())
    {
        return;
    }

    CARB_ASSERT(elementValue.size() >= 2 && elementValue.front() == '[' && elementValue.back() == ']');

    // Force delete item if it exists before creating a new array
    dictionary::Item* arrayItem = dictionaryInterface->getItemMutable(targetDictionary, elementPath.c_str());
    if (arrayItem)
    {
        dictionaryInterface->destroyItem(arrayItem);
    }

    // Creating a new dictionary element at the required path
    arrayItem = dictionaryInterface->makeDictionaryAtPath(targetDictionary, elementPath.c_str());
    // Setting necessary flag to make it a proper empty array
    // This will result in correct item replacement in case of dictionary merging
    dictionaryInterface->setItemFlag(arrayItem, dictionary::ItemFlag::eUnitSubtree, true);

    // Skip initial and the last square brackets and consider all elements separated by commas one by one
    // For each value create corresponding new path including index
    // Ex. "/some/path=[10,20]" will be processed as "/some/path/0=10" and "/some/path/1=20"

    const std::string commonElementPath = elementPath + '/';
    size_t curElementIndex = 0;

    // Helper adds provided value into the dictionary and increases index for the next addition
    auto dictElementAddHelper = [&](std::string value) {
        carb::extras::trimStringInplace(value);
        // Processing only non empty strings, empty string values should be stated as "": [ "a", "", "b" ]
        if (value.empty())
        {
            CARB_LOG_WARN(
                "Encountered and skipped an empty value for dictionary array element '%s' while parsing value '%s'",
                elementPath.c_str(), elementValue.c_str());
            return;
        }
        carb::dictionary::setDictionaryElementAutoType(
            dictionaryInterface, targetDictionary, commonElementPath + std::to_string(curElementIndex), value);
        ++curElementIndex;
    };

    std::string::size_type curValueStartPos = 1;

    // Add comma-separated values (except for the last one)
    for (std::string::size_type curCommaPos = elementValue.find(',', curValueStartPos);
         curCommaPos != std::string::npos; curCommaPos = elementValue.find(',', curValueStartPos))
    {
        dictElementAddHelper(elementValue.substr(curValueStartPos, curCommaPos - curValueStartPos));
        curValueStartPos = curCommaPos + 1;
    }

    // Now only the last value is left for addition
    std::string lastValue = elementValue.substr(curValueStartPos, elementValue.size() - curValueStartPos - 1);
    carb::extras::trimStringInplace(lastValue);
    // Do nothing if it's just a trailing comma: [ 1, 2, 3, ]
    if (!lastValue.empty())
    {
        carb::dictionary::setDictionaryElementAutoType(
            dictionaryInterface, targetDictionary, commonElementPath + std::to_string(curElementIndex), lastValue);
    }
}

/** Attempts to read the contents of a file into a dictionary.
 *
 *  @param[in] serializer   The @ref ISerializer interface to use to parse the data in the file.
 *                          This serializer must be able to parse the assumed format of the named
 *                          file (ie: JSON or TOML).  This must not be `nullptr`.  Note that this
 *                          interface will internally depend on only a single implementation of
 *                          the @ref IDictionary interface having been loaded.  If multiple
 *                          implementations are available, this operation will be likely to
 *                          result in undefined behavior.
 *  @param[in] filename     The name of the file to parse in the format assumed by the serializer
 *                          interface @p serializer.  This must not be `nullptr`.
 *  @returns A new dictionary item containing the information parsed from the file @p filename if
 *           it is successfully opened, read, and parsed.  When no longer needed, this must be
 *           passed to @ref IDictionary::destroyItem() to destroy it.  Returns `nullptr` if the
 *           file does not exit, could not be opened, or a parsing error occurred.
 *
 *  @remarks This attempts to parse a dictionary data from a file.  The format that the file's
 *           data is parsed from will be implied by the specific implementation of the @ref
 *           ISerializer interface that is passed in.  The data found in the file will be parsed
 *           into a full dictionary hierarchy if it is parsed correctly.  If the file contains
 *           a syntax error, the specific result (ie: full failure vs partial success) is
 *           determined by the specific behavior of the @ref ISerializer interface.
 *
 *  @thread_safety This operation is thread safe.
 */
inline Item* createDictionaryFromFile(ISerializer* serializer, const char* filename)
{
    carb::filesystem::IFileSystem* fs = carb::getCachedInterface<carb::filesystem::IFileSystem>();

    auto file = fs->openFileToRead(filename);
    if (!file)
        return nullptr;

    const size_t fileSize = fs->getFileSize(file);
    const size_t contentLen = fileSize + 1;

    std::unique_ptr<char[]> heap;
    char* content;

    if (contentLen <= 4096)
    {
        content = CARB_STACK_ALLOC(char, contentLen);
    }
    else
    {
        heap.reset(new char[contentLen]);
        content = heap.get();
    }

    const size_t readBytes = fs->readFileChunk(file, content, contentLen);
    fs->closeFile(file);

    if (readBytes != fileSize)
    {
        CARB_LOG_ERROR("Only read %zu bytes of a total of %zu bytes from file '%s'", readBytes, fileSize, filename);
    }

    // NUL terminate
    content[readBytes] = '\0';

    return serializer->createDictionaryFromStringBuffer(content, readBytes, fDeserializerOptionInSitu);
}

/** Writes the contents of a dictionary to a file.
 *
 *  @param[in] serializer           The @ref ISerializer interface to use to format the dictionary
 *                                  data before writing it to file.  This may not be `nullptr`.
 *  @param[in] dictionary           The dictionary root item to format and write to file.  This
 *                                  not be `nullptr`.  This must have been created by the same
 *                                  @ref IDictionary interface that @p serializer uses internally.
 *  @param[in] filename             The name of the file to write the formatted dictionary data
 *                                  to.  This file will be unconditionally overwritten.  It is the
 *                                  caller's responsibility to ensure any previous file at this
 *                                  location can be safely overwritten.  This must not be
 *                                  `nullptr`.
 *  @param[in] serializerOptions    Option flags passed to the @p serializer interface when
 *                                  formatting the dictionary data.
 *  @returns No return value.
 *
 *  @remarks This formats the contents of a dictionary to a string and writes it to a file.  The
 *           file will be formatted according to the serializer interface that is used.  The
 *           extra flag options passed to the serializer in @p serializerOptions control the
 *           specifics of the formatting.  The formatted data will be written to the file as
 *           long as it can be opened successfully for writing.
 *
 *  @thread_safety This operation itself is thread safe.  However, if another thread is attempting
 *                 to modify the dictionary @p dictionary at the same time, a race condition may
 *                 exist and undefined behavior could occur.  It won't crash but the written data
 *                 may be unexpected.
 */
inline void saveFileFromDictionary(ISerializer* serializer,
                                   const dictionary::Item* dictionary,
                                   const char* filename,
                                   SerializerOptions serializerOptions)
{
    const char* serializedString = serializer->createStringBufferFromDictionary(dictionary, serializerOptions);
    filesystem::IFileSystem* fs = getFramework()->acquireInterface<filesystem::IFileSystem>();
    filesystem::File* sFile = fs->openFileToWrite(filename);
    if (sFile == nullptr)
    {
        CARB_LOG_ERROR("failed to open file '%s' - unable to save the dictionary", filename);
        return;
    }
    fs->writeFileChunk(sFile, serializedString, strlen(serializedString));
    fs->closeFile(sFile);
    serializer->destroyStringBuffer(serializedString);
}

/** Writes a dictionary to a string.
 *
 *  @param[in] c                The dictionary to be serialized.  This must not be `nullptr`.
 *                              This dictionary must have been created by the same dictionary
 *                              interface that the serializer will use.  The serializer that is
 *                              used controls how the string is formatted.
 *  @param[in] serializerName   The name of the serializer plugin to use.  This must be the name
 *                              of the serializer plugin to be potentially loaded and used.  For
 *                              example, "carb.dictionary.serializer-json.plugin" to serialize to
 *                              use the JSON serializer.  The serializer plugin must already be
 *                              known to the framework.  This may be `nullptr` to pick the first
 *                              or best serializer plugin instead.  If the serializer plugin with
 *                              this name cannot be found or the @ref ISerializer interface
 *                              cannot be acquired from it, the first loaded serializer interface
 *                              will be acquired and used instead.
 *  @returns A string containing the human readable contents of the dictionary @p c.  If the
 *           dictionary failed to be written, an empty string will be returned but the operation
 *           will still be considered successful.  The dictionary will always be formatted using
 *           the @ref fSerializerOptionMakePretty flag so that it is as human-readable as
 *           possible.
 *
 *  @thread_safety This operation itself is thread safe.  However, if the dictionary @p c is
 *                 being modified concurrently by another thread, the output contents may be
 *                 unexpected.
 */
inline std::string dumpToString(const dictionary::Item* c, const char* serializerName = nullptr)
{
    std::string serializedDictionary;

    Framework* framework = carb::getFramework();
    dictionary::ISerializer* configSerializer = nullptr;

    // First, try to acquire interface with provided plugin name, if any
    if (serializerName)
    {
        configSerializer = framework->tryAcquireInterface<dictionary::ISerializer>(serializerName);
    }
    // If not available, or plugin name is not provided, try to acquire any serializer interface
    if (!configSerializer)
    {
        configSerializer = framework->tryAcquireInterface<dictionary::ISerializer>();
    }

    const char* configString =
        configSerializer->createStringBufferFromDictionary(c, dictionary::fSerializerOptionMakePretty);

    if (configString != nullptr)
    {
        serializedDictionary = configString;

        configSerializer->destroyStringBuffer(configString);
    }

    return serializedDictionary;
};

/** Retrieves the full path to dictionary item from its top-most ancestor.
 *
 *  @param[in] dict The @ref IDictionary interface to use to retrieve the full path to the
 *                  requested dictionary item.  This must not be `nullptr`.
 *  @param[in] item The dictionary item to retrieve the full path to.  This may not be `nullptr`.
 *                  This item must have been created by the same @ref IDictionary interface passed
 *                  in as @p dict.
 *  @returns A string containing the full path to the dictionary item @p item.  This path will be
 *           relative to its top-most ancestor.  On failure, an empty string is returned.
 *
 *  @thread_safety This operation itself is thread safe.  However, if the item or its chain of
 *                 ancestors is being modified concurrently, undefined behavior may result.
 */
inline std::string getItemFullPath(dictionary::IDictionary* dict, const carb::dictionary::Item* item)
{
    if (!item)
    {
        return std::string();
    }
    std::vector<const char*> pathElementsNames;

    while (item)
    {
        pathElementsNames.push_back(dict->getItemName(item));
        item = dict->getItemParent(item);
    }

    size_t totalSize = 0;
    for (const auto& elementName : pathElementsNames)
    {
        totalSize += 1; // the '/' separator
        if (elementName)
        {
            totalSize += std::strlen(elementName);
        }
    }

    std::string result;
    result.reserve(totalSize);

    for (size_t idx = 0, elementCount = pathElementsNames.size(); idx < elementCount; ++idx)
    {
        const char* elementName = pathElementsNames[elementCount - idx - 1];
        result += '/';
        if (elementName)
        {
            result += elementName;
        }
    }
    return result;
}

/** Helper function to convert a data type to a corresponding dictionary item type.
 *
 *  @tparam Type    The primitive data type to convert to a dictionary item type.  This operation
 *                  is undefined for types other than the handful of primitive types it is
 *                  explicitly specialized for.  If a another data type is used here, a link
 *                  link error will occur.
 *  @returns The dictionary item type corresponding to the templated primitive data type.
 *
 *  @thread_safety This operation is thread safe.
 */
template <typename Type>
inline ItemType toItemType();

/** Specialization for an `int32_t` item value.
 *  @copydoc toItemType().
 */
template <>
inline ItemType toItemType<int32_t>()
{
    return ItemType::eInt;
}

/** Specialization for an `int64_t` item value.
 *  @copydoc toItemType().
 */
template <>
inline ItemType toItemType<int64_t>()
{
    return ItemType::eInt;
}

/** Specialization for an `float` item value.
 *  @copydoc toItemType().
 */
template <>
inline ItemType toItemType<float>()
{
    return ItemType::eFloat;
}

/** Specialization for an `double` item value.
 *  @copydoc toItemType().
 */
template <>
inline ItemType toItemType<double>()
{
    return ItemType::eFloat;
}

/** Specialization for an `bool` item value.
 *  @copydoc toItemType().
 */
template <>
inline ItemType toItemType<bool>()
{
    return ItemType::eBool;
}

/** Specialization for an `char*` item value.
 *  @copydoc toItemType().
 */
template <>
inline ItemType toItemType<char*>()
{
    return ItemType::eString;
}

/** Specialization for an `const char*` item value.
 *  @copydoc toItemType().
 */
template <>
inline ItemType toItemType<const char*>()
{
    return ItemType::eString;
}

/** Unsubscribes all items in a dictionary tree from change notifications.
 *
 *  @param[in] dict The @ref IDictionary interface to use when walking the dictionary.  This must
 *                  not be `nullptr`.  This must be the same @ref IDictionary interface that was
 *                  used to create the dictionary item @p item.
 *  @param[in] item The dictionary item to unsubscribe all nodes from change notifications.  This
 *                  must not be `nullptr`.  Each item in this dictionary's tree will have all of
 *                  its tree and node change subscriptions removed.
 *  @returns No return value.
 *
 *  @remarks This removes all change notification subscriptions for an entire tree in a
 *           dictionary.  This should only be used as a last cleanup effort to prevent potential
 *           shutdown crashes since it will even remove subscriptions that the caller didn't
 *           necessarily setup.
 *
 *  @thread_safety This operation is thread safe.
 */
inline void unsubscribeTreeFromAllEvents(IDictionary* dict, Item* item)
{
    auto unsubscribeItem = [](Item* srcItem, uint32_t elementData, void* userData) -> uint32_t {
        IDictionary* dict = (IDictionary*)userData;
        dict->unsubscribeItemFromNodeChangeEvents(srcItem);
        dict->unsubscribeItemFromTreeChangeEvents(srcItem);
        return elementData;
    };

    const auto getChildByIndexMutable = [](IDictionary* dict, Item* item, size_t index) {
        return dict->getItemChildByIndexMutable(item, index);
    };

    walkDictionary(dict, WalkerMode::eIncludeRoot, item, 0, unsubscribeItem, dict, getChildByIndexMutable);
}

/** Helper function for IDictionary::update() that ensures arrays are properly overwritten.
 *
 *  @param[in] dstItem              The destination dictionary item for the merge operation.  This
 *                                  may be `nullptr` if the destination item doesn't already exist
 *                                  in the tree (ie: a new item is being merged into the tree).
 *  @param[in] dstItemType          The data type of the item @p dstItem.  If @p dstItem is
 *                                  `nullptr`, this will be @ref ItemType::eCount.
 *  @param[in] srcItem              The source dictionary item for the merge operation.  This will
 *                                  never be `nullptr`.  This will be the new value or node that
 *                                  is being merged into the dictionary tree.
 *  @param[in] srcItemType          The data type of the item @p srcItem.
 *  @param[in] dictionaryInterface  The @ref IDictionary interface to use when merging the new
 *                                  item into the dictionary tree.  This is expected to be passed
 *                                  into the @a userData parameter for IDictionary::update().
 *  @returns an @ref UpdateAction value indicating how the merge operation should proceed.
 *
 *  @remarks This is intended to be used as the @ref OnUpdateItemFn callback function for the
 *           @ref IDictionary::update() function when the handling of merging array items is
 *           potentially needed.  When this is used, the @ref IDictionary interface object must
 *           be passed into the @a userData parameter of IDictionary::update().
 *
 *  @thread_safety This operation is thread safe.  However, this call isn't expected to be used
 *                 directly, but rather through the IDictionary::update() function.  The overall
 *                 thread safety of that operation should be noted instead.
 */
inline UpdateAction overwriteOriginalWithArrayHandling(
    const Item* dstItem, ItemType dstItemType, const Item* srcItem, ItemType srcItemType, void* dictionaryInterface)
{
    CARB_UNUSED(dstItemType, srcItemType);
    if (dstItem && dictionaryInterface)
    {
        carb::dictionary::IDictionary* dictInt = static_cast<carb::dictionary::IDictionary*>(dictionaryInterface);

        if (dictInt->getItemFlag(srcItem, carb::dictionary::ItemFlag::eUnitSubtree))
        {
            return carb::dictionary::UpdateAction::eReplaceSubtree;
        }
    }
    return carb::dictionary::UpdateAction::eOverwrite;
}

} // namespace dictionary
} // namespace carb
