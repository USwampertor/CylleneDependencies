// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Helper classes and functions for the ILauncher interface.
 */
#pragma once

#include "../Defines.h"
#include "../extras/StringSafe.h"
#include "../../omni/extras/PathMap.h"
#include "../settings/SettingsUtils.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>

#if CARB_PLATFORM_LINUX
#    include <unistd.h>
#elif CARB_PLATFORM_MACOS
#    include <crt_externs.h>
#else
#    include "../CarbWindows.h"
#    include "../extras/Unicode.h"
#endif


namespace carb
{
/** Namespace for the Carbonite process launch helper interface. */
namespace launcher
{

/** Base type for the flags used when adding a settings tree to an argument collector object. */
using SettingsEnumFlags = uint32_t;

/** Flag to indicate that the settings in the requested tree should be added recursively to
 *  the argument collector.  If this flag is not present, only the settings directly in the
 *  named path will be added to the object.
 */
constexpr SettingsEnumFlags fSettingsEnumFlagRecursive = 0x01;

/** Prototype for a callback function used to check if a setting should be added.
 *
 *  @param[in] path     The full path to the setting being queried.  This will never be `nullptr`.
 *  @param[in] context  A caller specified context value that is passed to the callback.
 *  @returns `true` if the setting should be added to the argument collector.  Returns `false`
 *           if the setting should not be added to the argument collector.
 *
 *  @remarks Prototype for a callback predicate function that is used to allow the caller to
 *           determine if a particular setting value should be added to the argument collector.
 *           This is called for each value setting (ie: for `int`, `bool`, `float` and `string`
 *           items).  The return value gives the caller an opportunity to provide more fine
 *           grained control over which settings are added to the argument collector than just
 *           the full tree.
 */
using AddSettingPredicateFn = bool (*)(const char* path, void* context);


/** A simple child process argument collector helper class.  This allows arguments of different
 *  types to be accumulated into a list that can then later be retrieved as a Unix style argument
 *  list that can be passed to ILauncher::launchProcess() in its @ref LaunchDesc::argv descriptor
 *  member.  This allows for string arguments and various integer type arguments to be trivially
 *  added to the argument list without needing to locally convert all of them to strings.  The
 *  argument count is also tracked as the arguments are collected.  Once all arguments have been
 *  collected, the final Unix style argument list can be retrieved with getArgs() and the count
 *  with getCount().  All collected arguments will remain in the order they are originally
 *  added in.
 *
 *  The basic usage of this is to create a new object, add one or more arguments of various
 *  types to it using the '+=' operators, then retrieve the Unix style argument list with
 *  getArgs() to assign to @ref LaunchDesc::argv and getCount() to assign to
 *  @ref LaunchDesc::argc before calling ILauncher::launchProcess().  Copy and move operators
 *  and constructors are also provided to make it easier to assign other argument lists to
 *  another object to facilitate more advanced multiple process launches (ie: use a set of
 *  base arguments for each child process then add other child specific arguments to each
 *  one before launching).
 *
 *  This helper class is not thread safe.  It is the caller's responsibility to ensure thread
 *  safe access to objects of this class if needed.
 */
class ArgCollector
{
public:
    ArgCollector() = default;

    /** Copy constructor: copies another argument collector object into this one.
     *
     *  @param[in] rhs  The argument collector object to copy from.  This will be left unmodified.
     */
    ArgCollector(const ArgCollector& rhs)
    {
        *this = rhs;
    }

    /** Move constructor: moves the contents of another argument collector object into this one.
     *
     *  @param[in] rhs  The argument collector object to move from.  This object will be reset to
     *                  an empty state.
     */
    ArgCollector(ArgCollector&& rhs)
    {
        *this = std::move(rhs);
    }

    ~ArgCollector()
    {
        clear();
    }

    /** Clears out this object and resets it back to its initially constructed state.
     *
     *  @returns No return value.
     *
     *  @remarks This clears out all content collected into this object so far.  This object
     *           will be reset back to its original constructed state and will be suitable
     *           for reuse.
     */
    void clear()
    {
        m_argList.reset();
        m_args.clear();
        m_allocCount = 0;
    }

    /** Retrieves the final argument list as a Unix style null terminated list.
     *
     *  @param[out] argCountOut Optionally receives the number of arguments as by via getCount().
     *  @returns A Unix style argument list.  This list will always be terminated by a `nullptr`
     *           entry so that it can be self-counted if needed.  This returned argument list
     *           object is owned by this object and should not be deleted or freed.  See the
     *           remarks below for more information on the lifetime and use of this object.
     *  @returns `nullptr` if the buffer for the argument list could not be allocated.
     *
     *  @remarks This retrieves the final argument list for this object.  The list object is
     *           owned by this object and should not be freed or deleted.  The returned list
     *           will be valid until this object is destroyed or until getArgs() is called
     *           again after adding new arguments.  If the caller needs to keep a copy of the
     *           returned argument list, the caller must perform a deep copy on the returned
     *           object.  This task is out of the scope of this object and is left as an
     *           exercise for the caller.
     */
    const char* const* getArgs(size_t* argCountOut = nullptr)
    {
        if (argCountOut)
        {
            *argCountOut = m_args.size();
        }

        if (m_args.empty())
        {
            return emptyArgList();
        }

        if (m_allocCount < m_args.size())
        {
            m_allocCount = m_args.size();
            m_argList.reset(new (std::nothrow) const char*[m_args.size() + 1]);
            if (CARB_UNLIKELY(m_argList == nullptr))
            {
                if (argCountOut)
                {
                    *argCountOut = 0;
                }
                m_allocCount = 0;
                return nullptr;
            }
        }

        for (size_t i = 0; i < m_args.size(); i++)
        {
            m_argList[i] = m_args[i].c_str();
        }

        // null terminate the list since some platforms and apps expect that behavior.
        m_argList[m_args.size()] = nullptr;
        return m_argList.get();
    }

    /** Retrieves the argument count for this object.
     *
     *  @returns The number of arguments that have been collected into this object so far.  This
     *           is incremented each time the '+=' operator is used.
     */
    size_t getCount() const
    {
        return m_args.size();
    }

    /** Copy assignment operator.
     *
     *  @param[in] rhs  The argument collector object to copy from.  This object will receive a
     *                  copy of all the arguments currently listed in the @p rhs argument
     *                  collector object.  The @p rhs object will not be modified.
     *  @returns A reference to this object suitable for chaining other operators or calls.
     */
    ArgCollector& operator=(const ArgCollector& rhs)
    {
        if (this == &rhs)
            return *this;

        clear();

        if (rhs.m_args.size() == 0)
            return *this;

        m_args = rhs.m_args;
        return *this;
    }

    /** Move assignment operator.
     *
     *  @param[inout] rhs   The argument collector object to move from.  This object will
     *                      steal all arguments from @p rhs and will clear out the other
     *                      object before returning.
     *  @returns A reference to this object suitable for chaining other operators or calls.
     */
    ArgCollector& operator=(ArgCollector&& rhs)
    {
        if (this == &rhs)
            return *this;

        clear();

        m_args = std::move(rhs.m_args);
        return *this;
    }

    /** Compare this object to another argument collector object for equality.
     *
     *  @param[in] rhs  The argument collector object to compare to this one.
     *  @returns `true` if the two objects contain the same list of arguments.  Note that each
     *           object must contain the same arguments in the same order in order for them
     *           to match.
     *  @returns `false` if the argument lists in the two objects differ.
     */
    bool operator==(const ArgCollector& rhs)
    {
        size_t count = m_args.size();


        if (&rhs == this)
            return true;

        if (count != rhs.m_args.size())
            return false;

        for (size_t i = 0; i < count; i++)
        {
            if (m_args[i] != rhs.m_args[i])
                return false;
        }

        return true;
    }

    /** Compare this object to another argument collector object for inequality.
     *
     *  @param[in] rhs  The argument collector object to compare to this one.
     *  @returns `true` if the two objects contain a different list of arguments.  Note that each
     *           object must contain the same arguments in the same order in order for them
     *           to match.  If either the argument count differs or the order of the arguments
     *           differs, this will succeed.
     *  @returns `false` if the argument lists in the two objects match.
     */
    bool operator!=(const ArgCollector& rhs)
    {
        return !(*this == rhs);
    }

    /** Tests whether this argument collector is empty.
     *
     *  @returns `true` if this argument collector object is empty.
     *  @returns `false` if argument collector has at least one argument in its list.
     */
    bool operator!() const
    {
        return m_args.size() == 0;
    }

    /** Tests whether this argument collector is non-empty.
     *
     *  @returns `true` if argument collector has at least one argument in its list.
     *  @returns `false` if this argument collector object is empty.
     */
    explicit operator bool() const
    {
        return !m_args.empty();
    }

    /** Adds a formatted string as an argument.
     *
     *  @param[in] fmt  The printf-style format string to use to create the new argument.  This
     *                  may not be `nullptr`.
     *  @param[in] ...  The arguments required by the format string.
     *  @returns A references to this object suitable for chaining other operators or calls.
     */
    ArgCollector& format(const char* fmt, ...) CARB_PRINTF_FUNCTION(2, 3)
    {
        char bufferLocal[256];
        char* buffer = bufferLocal;
        int count;
        va_list args;


        va_start(args, fmt);
        count = vsnprintf(bufferLocal, CARB_COUNTOF(bufferLocal), fmt, args);
        va_end(args);

        if (count >= (int)CARB_COUNTOF(bufferLocal))
        {
            buffer = new (std::nothrow) char[count + 1];

            if (buffer == nullptr)
                return *this;

            va_start(args, fmt);
            vsnprintf(buffer, count + 1, fmt, args);
            va_end(args);
        }

        add(buffer);

        if (buffer != bufferLocal)
            delete[] buffer;

        return *this;
    }

    /** Adds a new argument or set of arguments to the end of the list.
     *
     *  @param[in] value    The value to add as a new argument.  This may be a string or any
     *                      primitive integer or floating point type value.  For integer and
     *                      floating point values, they will be converted to a string before
     *                      being added to the argument list.  For the variants that add
     *                      another argument collector list or a `nullptr` terminated string
     *                      list, or a vector of strings to this one, all arguments in the
     *                      other object will be copied into this one in the same order.  All
     *                      new argument(s) will always be added at the end of the list.
     *  @returns A reference to this object suitable for chaining other operators or calls.
     */
    ArgCollector& add(const char* value)
    {
        m_args.push_back(value);
        return *this;
    }

    /** @copydoc add(const char*) */
    ArgCollector& add(const std::string& value)
    {
        m_args.push_back(value);
        return *this;
    }

    /** @copydoc add(const char*) */
    ArgCollector& add(const ArgCollector& value)
    {
        for (auto& arg : value.m_args)
            m_args.push_back(arg);
        return *this;
    }

    /** @copydoc add(const char*) */
    ArgCollector& add(const char* const* value)
    {
        for (const char* const* arg = value; arg[0] != nullptr; arg++)
            m_args.push_back(arg[0]);
        return *this;
    }

    /** @copydoc add(const char*) */
    ArgCollector& add(const std::vector<const char*>& value)
    {
        for (auto& v : value)
            add(v);
        return *this;
    }

    /** @copydoc add(const char*) */
    ArgCollector& add(const std::vector<std::string>& value)
    {
        for (auto& v : value)
            add(v);
        return *this;
    }

    /** @copydoc add(const char*) */
    ArgCollector& operator+=(const char* value)
    {
        return add(value);
    }

    /** @copydoc add(const char*) */
    ArgCollector& operator+=(const std::string& value)
    {
        return add(value);
    }

    /** @copydoc add(const char*) */
    ArgCollector& operator+=(const ArgCollector& value)
    {
        return add(value);
    }

    /** @copydoc add(const char*) */
    ArgCollector& operator+=(const char* const* value)
    {
        return add(value);
    }

    /** @copydoc add(const char*) */
    ArgCollector& operator+=(const std::vector<const char*>& value)
    {
        return add(value);
    }

    /** @copydoc add(const char*) */
    ArgCollector& operator+=(const std::vector<std::string>& value)
    {
        return add(value);
    }

    /** Macro to add other [almost identical] variants of the add() and operator+=() functions.
     *  Note that this unfortunately can't be a template because a 'const char*' value is not
     *  allowed as a template argument since it doesn't lead to a unique instantiation.  This
     *  is similar to the reasoning for a float value not being allowed as a template argument.
     *  Using a macro here saves 140+ lines of code (mostly duplicated) and documentation.
     *  @private
     */
#define ADD_PRIMITIVE_HANDLER(type, fmt)                                                                               \
    /** Adds a new primitive value to this argument collector object.                                                  \
        @param[in] value    The primitive numerical value to add to this argument collector                            \
                            object.  This will be converted to a string before adding to the                           \
                            collector.                                                                                 \
        @returns A reference to this argument collector object.                                                        \
     */                                                                                                                \
    ArgCollector& add(type value)                                                                                      \
    {                                                                                                                  \
        char buffer[128];                                                                                              \
        carb::extras::formatString(buffer, CARB_COUNTOF(buffer), fmt, value);                                          \
        m_args.push_back(buffer);                                                                                      \
        return *this;                                                                                                  \
    }                                                                                                                  \
    /** @copydoc add(type) */                                                                                          \
    ArgCollector& operator+=(type value)                                                                               \
    {                                                                                                                  \
        return add(value);                                                                                             \
    }

    // unsigned integer handlers.
    ADD_PRIMITIVE_HANDLER(unsigned char, "%u")
    ADD_PRIMITIVE_HANDLER(unsigned short, "%u")
    ADD_PRIMITIVE_HANDLER(unsigned int, "%u")
    ADD_PRIMITIVE_HANDLER(unsigned long, "%lu")
    ADD_PRIMITIVE_HANDLER(unsigned long long, "%llu")

    // signed integer handlers.
    ADD_PRIMITIVE_HANDLER(char, "%d")
    ADD_PRIMITIVE_HANDLER(short, "%d")
    ADD_PRIMITIVE_HANDLER(int, "%d")
    ADD_PRIMITIVE_HANDLER(long, "%ld")
    ADD_PRIMITIVE_HANDLER(long long, "%lld")

    // other numerical handlers.  Note that some of these can be trivially implicitly cast to
    // other primitive types so we can't define them again.  Specifically the size_t,
    // intmax_t, and uintmax_t types often match other types with handlers defined above.
    // Which handler each of these matches to will differ by platform however.
    ADD_PRIMITIVE_HANDLER(float, "%.10f")
    ADD_PRIMITIVE_HANDLER(double, "%.20f")

#undef ADD_PRIMITIVE_HANDLER

    /** Adds all settings under a branch in the settings registry to this object.
     *
     *  @param[in] root         The root of the settings tree to copy into this argument
     *                          collector.  This may be `nullptr` or an empty string to
     *                          add all settings starting from the root of the settings
     *                          registry.  This string should start with a '/' so that it is
     *                          always an absolute settings path.
     *  @param[in] prefix       The prefix to add to each option before adding it to this
     *                          argument collector.  This may be `nullptr` or an empty string
     *                          to not use any prefix.
     *  @param[in] flags        Flags to control the behavior of this operation.  This may be
     *                          zero or more of the @ref SettingsEnumFlags flags.  This defaults
     *                          to 0.
     *  @param[in] predicate    A predicate function that will be called for each value to give
     *                          the caller a chance to decide whether it should be added to this
     *                          object or not.  This may be `nullptr` if all settings under the
     *                          given root should always be added.  This defaults to `nullptr`.
     *  @param[in] context      An opaque context value that will be passed to the predicate
     *                          function @p predicate.  This will not be accessed or used in
     *                          any way except to pass to the predicate function.  This defaults
     *                          to `nullptr`.
     *  @returns A reference to this argument collector object.
     *
     *  @remarks This adds echoes of all settings under a given root branch as arguments in this
     *           argument collector.  Each setting that is found is given the prefix @p prefix
     *           (typically something like "--/").  This is useful for passing along certain
     *           subsets of a parent process's settings tree to a child process.
     *
     *  @note It is the caller's responsibility to ensure that only expected settings are added
     *        to this argument collector.  A predicate function can be provided to allow per-item
     *        control over which settings get added.  By default, the search is not recursive.
     *        This is intentional since adding a full tree could potentially add a lot of new
     *        arguments to this object.
     */
    ArgCollector& add(const char* root,
                      const char* prefix,
                      SettingsEnumFlags flags = 0,
                      AddSettingPredicateFn predicate = nullptr,
                      void* context = nullptr)
    {
        dictionary::IDictionary* dictionary = getCachedInterface<dictionary::IDictionary>();
        settings::ISettings* settings = getCachedInterface<settings::ISettings>();
        std::string rootPath;

        auto addSetting = [&](const char* path, int32_t elementData, void* context) -> int32_t {
            dictionary::ItemType type;
            CARB_UNUSED(context);

            type = settings->getItemType(path);

            // skip dictionaries since we're only interested in leaves here.
            if (type == dictionary::ItemType::eDictionary)
                return elementData + 1;

            if ((flags & fSettingsEnumFlagRecursive) == 0 && elementData > 1)
                return elementData;

            // verify that the caller wants this setting added.
            if (predicate != nullptr && !predicate(path, context))
                return elementData + 1;

            switch (type)
            {
                case dictionary::ItemType::eBool:
                    format("%s%s=%s", prefix, &path[1], settings->getAsBool(path) ? "true" : "false");
                    break;

                case dictionary::ItemType::eInt:
                    format("%s%s=%" PRId64, prefix, &path[1], settings->getAsInt64(path));
                    break;

                case dictionary::ItemType::eFloat:
                    format("%s%s=%g", prefix, &path[1], settings->getAsFloat64(path));
                    break;

                case dictionary::ItemType::eString:
                    format("%s%s=\"%s\"", prefix, &path[1], settings->getStringBuffer(path));
                    break;

                default:
                    break;
            }

            return elementData;
        };

        // unexpected root prefix (not an absolute settings path) => fail.
        if (root == nullptr || root[0] == 0)
            root = "/";

        // avoid a `nullptr` check later.
        if (prefix == nullptr)
            prefix = "";

        // make sure to strip off any trailing separators since that would break the lookups.
        rootPath = root;

        if (rootPath.size() > 1 && rootPath[rootPath.size() - 1] == '/')
            rootPath = rootPath.substr(0, rootPath.size() - 1);

        // walk the settings tree to collect all the requested settings.
        settings::walkSettings(
            dictionary, settings, dictionary::WalkerMode::eIncludeRoot, rootPath.c_str(), 0, addSetting, context);
        return *this;
    }

    /** Retrieves the argument string at a given index.
     *
     *  @param[in] index    The zero based index of the argument to retrieve.  This must be
     *                      strictly less than the number of arguments in the list as returned
     *                      by getCount().  If this index is out of range, an empty string
     *                      will be returned instead.
     *  @returns A reference to the string contained in the requested argument in the list.
     *           This returned string should not be modified by the caller.
     *
     *  @remarks This retrieves the argument string stored at the given index in the argument
     *           list.  This string will be the one stored in the list itself and should not
     *           be modified.
     */
    const std::string& at(size_t index) const
    {
        if (index >= m_args.size())
            return m_empty;

        return m_args[index];
    }

    /** @copydoc at() */
    const std::string& operator[](size_t index) const
    {
        return at(index);
    }

    /** Removes the last argument from the list.
     *
     *  @returns No return value.
     *
     *  @remarks This removes the last argument from the list.  If this is called, any previous
     *           returned object from getArgs() will no longer be valid.  The updated list object
     *           must be retrieved again with another call to getArgs().
     */
    void pop()
    {
        if (m_args.empty())
            return;

        m_args.pop_back();
    }

    /** Removes an argument from the list by its index.
     *
     *  @returns `true` if the item is successfully removed.
     *  @returns `false` if the given index is out of range of the argument list's size.
     *
     *  @remarks This removes an argument from the list.  If this is called, any previous returned
     *           object from getArgs() will no longer be valid.  The updated list object must be
     *           retrieved again with another call to getArgs().
     */
    bool erase(size_t index)
    {
        if (index >= m_args.size())
            return false;

        m_args.erase(m_args.begin() + index);
        return true;
    }

    /** Returns a string of all arguments for debugging purposes.
     * @returns A `std::string` of all arguments concatenated. This is for debugging/logging purposes only.
     */
    std::string toString() const
    {
        std::ostringstream stream;
        for (auto& arg : m_args)
        {
            size_t index = size_t(-1);
            for (;;)
            {
                size_t start = index + 1;
                index = arg.find_first_of("\\\" '", start); // Characters that must be escaped
                stream << arg.substr(start, index - start);
                if (index == std::string::npos)
                    break;
                stream << '\\' << arg[index];
            }

            // Always add a trailing space. It will be popped before return.
            stream << ' ';
        }

        std::string out = stream.str();
        if (!out.empty())
            out.pop_back(); // Remove the extra space

        return out;
    }

private:
    static const char* const* emptyArgList()
    {
        static const char* const empty{ nullptr };
        return &empty;
    }

    /** An empty string to be retrieved from operator[] in the event of an out of range index.
     *  This is used to avoid the undesirable behavior of having an exception thrown by the
     *  underlying implementation of std::vector<>.
     */
    std::string m_empty;

    /** The vector of collected arguments.  This represents the most current list of arguments
     *  for this object.  The list in @p m_argList may be out of date if new arguments are
     *  added or existing ones removed.  The argument count is always taken from the size of
     *  this vector.
     */
    std::vector<std::string> m_args;

    /** The Unix style list of arguments as last retrieved by getArgs().  This is only updated
     *  when getArgs() is called.  This object may be destroyed and recreated in subsequent
     *  calls to getArgs() if the argument list is modified.
     */
    std::unique_ptr<const char*[]> m_argList;

    /** The last allocation size of the @p m_argList object in items.  This is used to avoid
     *  reallocating the previous object if it is already large enough for a new call to
     *  getArgs().
     */
    size_t m_allocCount = 0;
};


/** A simple environment variable collector helper class.  This provides a way to collect a set of
 *  environment variables and their values for use in ILauncher::launchProcess().  Each variable
 *  in the table will be unique.  Attempting to add a variable multiple times will simply replace
 *  any previous value.  Specifying a variable without a value will remove it from the table.
 *  Values for variables may be specified in any primitive integer or floating point type as
 *  well as string values.  Once all desired variables have been collected into the object, a
 *  Unix style environment table can be retrieved with getEnv().  and the could with getCount().
 *  The order of the variables in the environment block will be undefined.
 *
 *  The basic usage of this is to create a new object, add one or more variables and their
 *  values (of various types) using the various add() or '+=' operators, the retrieve the Unix
 *  style environment block with getEnv() to assign to @ref LaunchDesc::env, and getCount() to
 *  assign to @ref LaunchDesc::envCount.  This environment block is then passed through the
 *  launch descriptor to ILauncher::launchProcess().  Copy and move constructors are also
 *  provided to make it easier to assign and combine other environment lists.
 *
 *  The calling process's current environment can also be added using the add() function without
 *  any arguments.  This can be used to allow a child process to explicitly inherit the parent
 *  process's environment block while also adding other variables or changing existing ones.
 *
 *  On Windows all environment variable names used in this object are treated as case insensitive.
 *  All values set set for the variables will be case preserving.  This matches Windows' native
 *  behavior in handling environment variables.  If multiple casings are used in specifying the
 *  variable name when modifying any given variable, the variable's name will always keep the
 *  casing from when it was first added.  Later changes to that variable regardless of the casing
 *  will only modify the variable's value.
 *
 *  On Linux, all environment variable names used in this object are treated as case sensitive.
 *  All values set for the variables will be case preserving.  This matches Linux's native
 *  behavior in handling environment variables.  It is the caller's responsibility to ensure
 *  the proper casing is used when adding or modifying variables.  Failure to match the case
 *  of an existing variable for example will result in two variables with the same name but
 *  different casing being added.  This can be problematic for example when attempting to
 *  modify a standard variable such as "PATH" or "LD_LIBRARY_PATH".
 *
 *  Also note that using this class does not affect or modify the calling process's environment
 *  variables in any way.  This only collects variables and their values in a format suitable
 *  for setting as a child process's new environment.
 *
 *  This helper class is not thread safe.  It is the caller's responsibility to ensure thread
 *  safe access to objects of this class if needed.
 */
class EnvCollector
{
public:
    EnvCollector() = default;

    /** Copy constructor: copies another environment collector object into this one.
     *
     *  @param[in] rhs  The other environment collector object whose contents will be copied
     *                  into this one.  This other object will not be modified.
     */
    EnvCollector(const EnvCollector& rhs)
    {
        *this = rhs;
    }

    /** Move constructor: moves the contents of an environment collector object into this one.
     *
     *  @param[in] rhs  The other environment collector object whose contents will be moved
     *                  into this one.  Upon return, this other object will be reset to an
     *                  empty state.
     */
    EnvCollector(EnvCollector&& rhs)
    {
        *this = std::move(rhs);
    }

    ~EnvCollector()
    {
        clear();
    }


    /** Clears out this environment block object.
     *
     *  @returns No return value.
     *
     *  @remarks This clears out this environment block object.  Any existing variables and their
     *           values will be lost and the object will be reset to its default constructed
     *           state for reuse.
     */
    void clear()
    {
        m_env.clear();
        m_args.clear();
    }

    /** Retrieves the Unix style environment block representing the variables in this object.
     *
     *  @returns A Unix style environment block.  This will be an array of string pointers.
     *           The last entry in the array will always be a `nullptr` string.  This can be
     *           used to count the length of the environment block without needing to explicitly
     *           pass in its size as well.
     *  @returns `nullptr` if the buffer for the environment block could not be allocated.
     *
     *  @remarks This retrieves the Unix style environment block for this object.  The environment
     *           block object is owned by this object and should not be freed or deleted.  The
     *           returned block will be valid until this object is destroyed or until getEnv()
     *           is called again.  If the caller needs to keep a copy of the returned environment
     *           block object, the caller must perform a deep copy on the returned object.  This
     *           task is out of the scope of this object and is left as an exercise for the caller.
     */
    const char* const* getEnv()
    {
        m_args.clear();

        for (auto& var : m_env)
            m_args.format("%s=%s", var.first.c_str(), var.second.c_str());

        return m_args.getArgs();
    }

    /** Retrieves the number of unique variables in the environment block.
     *
     *  @returns The total number of unique variables in this environment block.
     *  @returns `0` if this environment block object is empty.
     */
    size_t getCount() const
    {
        return m_env.size();
    }


    /** Copy assignment operator.
     *
     *  @param[in] rhs  The environment collector object to copy from.  This object will receive
     *                  a copy of all the variables currently listed in the @p rhs environment
     *                  collector object.  The @p rhs object will not be modified.
     *  @returns A reference to this object suitable for chaining other operators or calls.
     */
    EnvCollector& operator=(const EnvCollector& rhs)
    {
        if (this == &rhs)
            return *this;

        clear();

        if (rhs.m_env.size() == 0)
            return *this;

        m_env = rhs.m_env;
        return *this;
    }

    /** Move assignment operator.
     *
     *  @param[inout] rhs   The argument collector object to move from.  This object will
     *                      steal all arguments from @p rhs and will clear out the other
     *                      object before returning.
     *  @returns A reference to this object suitable for chaining other operators or calls.
     */
    EnvCollector& operator=(EnvCollector&& rhs)
    {
        if (this == &rhs)
            return *this;

        clear();

        m_env = std::move(rhs.m_env);
        return *this;
    }

    /** Compare this object to another argument collector object for equality.
     *
     *  @param[in] rhs  The argument collector object to compare to this one.
     *  @returns `true` if the two objects contain the same list of arguments.  Note that each
     *           object must contain the same arguments in the same order in order for them
     *           to match.
     *  @returns `false` if the argument lists in the two objects differ.
     */
    bool operator==(const EnvCollector& rhs)
    {
        size_t count = m_env.size();


        if (&rhs == this)
            return true;

        if (count != rhs.m_env.size())
            return false;

        for (auto var : m_env)
        {
            auto other = rhs.m_env.find(var.first);

            if (other == rhs.m_env.end())
                return false;

            if (other->second != var.second)
                return false;
        }

        return true;
    }

    /** Compare this object to another argument collector object for inequality.
     *
     *  @param[in] rhs  The argument collector object to compare to this one.
     *  @returns `true` if the two objects contain a different list of arguments.  Note that each
     *           object must contain the same arguments in the same order in order for them
     *           to match.  If either the argument count differs or the order of the arguments
     *           differs, this will succeed.
     *  @returns `false` if the argument lists in the two objects match.
     */
    bool operator!=(const EnvCollector& rhs)
    {
        return !(*this == rhs);
    }

    /** Tests whether this argument collector is empty.
     *
     *  @returns `true` if this argument collector object is empty.
     *  @returns `false` if argument collector has at least one argument in its list.
     */
    bool operator!()
    {
        return m_env.size() == 0;
    }

    /** Tests whether this argument collector is non-empty.
     *
     *  @returns `true` if argument collector has at least one argument in its list.
     *  @returns `false` if this argument collector object is empty.
     */
    operator bool()
    {
        return !m_env.empty();
    }


    /** Adds a new environment variable by name and value.
     *
     *  @param[in] name     The name of the environment variable to add or replace.  This may
     *                      not be an empty string or `nullptr`, and should not contain an '='
     *                      except as the first character.
     *  @param[in] value    The value to assign to the variable @p name.  This may be `nullptr`
     *                      or an empty string to add a variable with no value.
     *  @returns A reference to this object suitable for chaining multiple calls.
     *
     *  @remarks These functions allow various combinations of name and value types to be used
     *           to add new environment variables to this object.  Values may be set as strings,
     *           integers, or floating point values.
     */
    EnvCollector& add(const char* name, const char* value)
    {
        m_env[name] = value == nullptr ? "" : value;
        return *this;
    }

    /** @copydoc add(const char*,const char*) */
    EnvCollector& add(const std::string& name, const std::string& value)
    {
        m_env[name] = value;
        return *this;
    }

    /** @copydoc add(const char*,const char*) */
    EnvCollector& add(const std::string& name, const char* value)
    {
        m_env[name] = value == nullptr ? "" : value;
        return *this;
    }

    /** @copydoc add(const char*,const char*) */
    EnvCollector& add(const char* name, const std::string& value)
    {
        m_env[name] = value;
        return *this;
    }

#define ADD_PRIMITIVE_HANDLER(type, fmt)                                                                               \
    /** Adds a new name and primitive value to this environment collector object.                                      \
        @param[in] name     The name of the new environment variable to be added.  This may                            \
                            not be `nullptr` or an empty string.                                                       \
        @param[in] value    The primitive numerical value to set as the environment variable's                         \
                            value.  This will be converted to a string before adding to the                            \
                            collector.                                                                                 \
        @returns A reference to this environment collector object.                                                     \
     */                                                                                                                \
    EnvCollector& add(const char* name, type value)                                                                    \
    {                                                                                                                  \
        char buffer[128];                                                                                              \
        carb::extras::formatString(buffer, CARB_COUNTOF(buffer), fmt, value);                                          \
        return add(name, buffer);                                                                                      \
    }                                                                                                                  \
    /** @copydoc add(const char*,type) */                                                                              \
    EnvCollector& add(const std::string& name, type value)                                                             \
    {                                                                                                                  \
        return add(name.c_str(), value);                                                                               \
    }

    // unsigned integer handlers.
    ADD_PRIMITIVE_HANDLER(uint8_t, "%" PRIu8)
    ADD_PRIMITIVE_HANDLER(uint16_t, "%" PRIu16)
    ADD_PRIMITIVE_HANDLER(uint32_t, "%" PRIu32)
    ADD_PRIMITIVE_HANDLER(uint64_t, "%" PRIu64)

    // signed integer handlers.
    ADD_PRIMITIVE_HANDLER(int8_t, "%" PRId8)
    ADD_PRIMITIVE_HANDLER(int16_t, "%" PRId16)
    ADD_PRIMITIVE_HANDLER(int32_t, "%" PRId32)
    ADD_PRIMITIVE_HANDLER(int64_t, "%" PRId64)

    // other numerical handlers.  Note that some of these can be trivially implicitly cast to
    // other primitive types so we can't define them again.  Specifically the size_t,
    // intmax_t, and uintmax_t types often match other types with handlers defined above.
    // Which handler each of these matches to will differ by platform however.
    ADD_PRIMITIVE_HANDLER(float, "%.10f")
    ADD_PRIMITIVE_HANDLER(double, "%.20f")

#undef ADD_PRIMITIVE_HANDLER

    /** Adds or replaces a variable specified in a single string.
     *
     *  @param[in] var  The variable name and value to set.  This may not be `nullptr` or an empty
     *                  string.  This must be in the format `<name>=<value>`.  There should not
     *                  be any spaces between the name, '=' and value portions of the string.
     *                  If the '=' is missing or no value is given after the '=', the value of
     *                  the named variable will be cleared out, but the variable will still
     *                  remain valid.
     *  @returns A reference to this object suitable for chaining multiple calls.
     */
    EnvCollector& add(const char* var)
    {
        const char* sep;


#if CARB_PLATFORM_WINDOWS
        // Windows' environment sets variables such as "=C:=C:\".  We need to handle this case.
        // Here the variable's name is "=C:" and its value is "C:\".  This similar behavior is
        // not allowed on Linux however.
        if (var[0] == '=')
            sep = strchr(var + 1, '=');

        else
#endif
            sep = strchr(var, '=');

        // no assignment in the string => clear out that variable.
        if (sep == nullptr)
        {
            m_env[var] = "";
            return *this;
        }

        m_env[std::string(var, sep - var)] = sep + 1;
        return *this;
    }

    /** @copydoc add(const char*) */
    EnvCollector& add(const std::string& var)
    {
        return add(var.c_str());
    }


    /** @copydoc add(const char*) */
    EnvCollector& operator+=(const char* var)
    {
        return add(var);
    }

    /** @copydoc add(const char*) */
    EnvCollector& operator+=(const std::string& var)
    {
        return add(var.c_str());
    }

    /** Adds a set of environment variables to this object.
     *
     *  @param[in] vars     The set of variables to add to this object.  This may not be
     *                      `nullptr`.
     *                      The string table variant is intended to be a Unix style `nullptr`
     *                      terminated string list.  The other variants add the full contents
     *                      of another environment collector object.
     *  @returns A reference to this object suitable for chaining multiple calls.
     */
    EnvCollector& add(const char* const* vars)
    {
        for (const char* const* var = vars; var[0] != nullptr; var++)
            add(var[0]);
        return *this;
    }

    /** @copydoc add(const char* const*) */
    EnvCollector& add(const std::vector<const char*>& vars)
    {
        for (auto& v : vars)
            add(v);
        return *this;
    }

    /** @copydoc add(const char* const*) */
    EnvCollector& add(const std::vector<std::string>& vars)
    {
        for (auto& v : vars)
            add(v);
        return *this;
    }

    /** @copydoc add(const char* const*) */
    EnvCollector& add(const EnvCollector& vars)
    {
        for (auto& var : vars.m_env)
            m_env[var.first] = var.second;
        return *this;
    }

    /** @copydoc add(const char* const*) */
    EnvCollector& operator+=(const char* const* vars)
    {
        return add(vars);
    }

    /** @copydoc add(const char* const*) */
    EnvCollector& operator+=(const std::vector<const char*>& vars)
    {
        return add(vars);
    }

    /** @copydoc add(const char* const*) */
    EnvCollector& operator+=(const std::vector<std::string>& vars)
    {
        return add(vars);
    }

    /** @copydoc add(const char* const*) */
    EnvCollector& operator+=(const EnvCollector& vars)
    {
        return add(vars);
    }

    /** Adds the environment variables from the calling process.
     *
     *  @returns A reference to this object suitable for chaining multiple calls.
     *
     *  @remarks This adds all of the current environment variables of the calling process to
     *           this environment block.  Any variables with the same name that already existed
     *           in this object will be replaced.  This is suitable for inheriting the calling
     *           process's current environment when launching a child process while still allowing
     *           changes or additions before launch.
     */
#if CARB_POSIX
    EnvCollector& add()
    {
#    if CARB_PLATFORM_MACOS
        char*** tmp = _NSGetEnviron(); // see man 7 environ
        if (tmp == nullptr)
        {
            CARB_LOG_ERROR("_NSGetEnviron() returned nullptr");
            return *this;
        }

        char** environ = *tmp;
#    endif

        for (char** env = environ; env[0] != nullptr; env++)
            add(env[0]);
        return *this;
    }
#else
    EnvCollector& add()
    {
        LPWCH origEnv = GetEnvironmentStringsW();
        LPWCH env = origEnv;
        std::string var;
        size_t len;


        // walk the environment strings table and add each variable to this object.
        for (len = wcslen(env); env[0] != 0; env += len + 1, len = wcslen(env))
        {
            var = extras::convertWideToUtf8(env);
            add(var);
        }

        FreeEnvironmentStringsW(origEnv);
        return *this;
    }
#endif

    /** Removes a variable and its value from this object.
     *
     *  @param[in] name     The name of the variable to be removed.  This may not be `nullptr` or
     *                      an empty string.  The named variable will no longer be present in
     *                      this object upon return and its value will be lost.
     *  @returns A reference to this object suitable for chaining multiple calls.
     */
    EnvCollector& remove(const char* name)
    {
        m_env.erase(name);
        return *this;
    }

    /** @copydoc remove(const char*) */
    EnvCollector& remove(const std::string& name)
    {
        return remove(name.c_str());
    }

    /** @copydoc remove(const char*) */
    EnvCollector& operator-=(const char* name)
    {
        return remove(name);
    }

    /** @copydoc remove(const char*) */
    EnvCollector& operator-=(const std::string& name)
    {
        return remove(name.c_str());
    }


    /** Retrieves the value for a named variable in this environment block object.
     *
     *  @param[in] name     The name of the variable to retrieve the value of.  This may not be
     *                      `nullptr` or an empty string.  The value of the variable will not be
     *                      modified by this lookup nor can its value be changed by assigning
     *                      a new string to the returned string.  To change the value, one of
     *                      the add() functions or '+=()' operators should be used instead.
     *  @returns The value of the named variable if present in this environment block.
     *  @returns An empty string if the variable is not present in this environment block.
     */
    const std::string& at(const char* name)
    {
        auto var = m_env.find(name);


        if (var == m_env.end())
            return m_empty;

        return var->second;
    }

    /** @copydoc at(const char*) */
    const std::string& at(const std::string& name)
    {
        return at(name.c_str());
    }

    /** @copydoc at(const char*) */
    const std::string& operator[](const char* name)
    {
        return at(name);
    }

    /** @copydoc at(const char*) */
    const std::string& operator[](const std::string& name)
    {
        return at(name.c_str());
    }


private:
    /** The table of argument names and values.  This behaves differently in terms of case
     *  sensitivity depending on the platform.  On Windows, environment variable names are
     *  treated as case insensitive to match local OS behavior.  In contrast, on Linux
     *  environment variable names are treated as case sensitive.
     */
    omni::extras::UnorderedPathMap<std::string> m_env;

    /** The argument collector used to generate the environment block for getEnv(). */
    ArgCollector m_args;

    /** An empty string to return in case a variable is not found in the table. */
    std::string m_empty;
};


} // namespace launcher
} // namespace carb
