// Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Provides a set of helper functions to manage the processing of command line arguments.
 */
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Namespace for all low level Carbonite functionality. */
namespace carb
{
/** Namespace for the options processing helper functions. */
namespace options
{

/** The possible result codes of parsing a single option. */
enum class ParseResult
{
    eSuccess, ///< Parsing was successful and the option was consumed.
    eInvalidValue, ///< A token or name was expected but not found.
};

/** Type names for values passed to the parser functions. */
enum class ValueType
{
    eIgnore = -1, ///< Ignore arguments of this type.
    eNone, ///< No type or data.
    eString, ///< Value is a string (ie: const char*).
    eLong, ///< Value is a signed long integer (ie: long).
    eLongLong, ///< Value is a signed long long integer (ie: long long).
    eFloat, ///< Value is a single precision floating point number (ie: float).
    eDouble, ///< Value is a double precision floating point number (ie: double).
};

/** Special failure value for getArgString() indicating that an expected argument was missing. */
constexpr int kArgFailExpectedArgument = -1;

/** Special failure value for getArgString() indicating that an argument's value started with
 *  a single or double quotation mark but did not end with the matching quotation or the ending
 *  quotation mark was missing.
 */
constexpr int kArgFailExpectedQuote = -2;

/** Converts a string to a number.
 *
 *  @param[in] string   The string to convert.
 *  @param[out] value   Receives the converted value of the string if successful.
 *  @returns `true` if the string is successfully converted to a number.
 *  @returns `false` if the string could not be fully converted to a number.
 */
template <typename T>
inline bool stringToNumber(const char* string, T* value)
{
    char* endp = nullptr;
    *value = strtoull(string, &endp, 10);
    return endp == nullptr || endp[0] == 0;
}

/** @copydoc stringToNumber(const char*,T*) */
template <>
inline bool stringToNumber(const char* string, long* value)
{
    char* endp = nullptr;
    *value = strtol(string, &endp, 10);
    return endp == nullptr || endp[0] == 0;
}

/** @copydoc stringToNumber(const char*,T*) */
template <>
inline bool stringToNumber(const char* string, float* value)
{
    char* endp = nullptr;
    *value = strtof(string, &endp);
    return endp == nullptr || endp[0] == 0;
}

/** @copydoc stringToNumber(const char*,T*) */
template <>
inline bool stringToNumber(const char* string, double* value)
{
    char* endp = nullptr;
    *value = strtod(string, &endp);
    return endp == nullptr || endp[0] == 0;
}

/** A multi-value type.  This contains a single value of a specific type.  The
 *  value can only be retrieved if it can be safely converted to the requested
 *  type.
 */
class Value
{
public:
    /** Constructor: initializes an object with no specific value in it. */
    Value()
    {
        clear();
    }

    /** Clears the value in this object.
     *
     *  @returns No return value.
     */
    void clear()
    {
        m_type = ValueType::eNone;
        m_value.string = nullptr;
    }

    /** Sets the value and type in this object.
     *
     *  @param[in] value    The value to set in this object.  The value's type is implied
     *                      from the setter that is used.
     *  @returns No return value.
     */
    void set(const char* value)
    {
        m_type = ValueType::eString;
        m_value.string = value;
    }

    /** @copydoc set(const char*) */
    void set(long value)
    {
        m_type = ValueType::eLong;
        m_value.integer = value;
    }

    /** @copydoc set(const char*) */
    void set(long long value)
    {
        m_type = ValueType::eLongLong;
        m_value.longInteger = value;
    }

    /** @copydoc set(const char*) */
    void set(float value)
    {
        m_type = ValueType::eFloat;
        m_value.floatValue = value;
    }

    /** @copydoc set(const char*) */
    void set(double value)
    {
        m_type = ValueType::eDouble;
        m_value.doubleValue = value;
    }

    /** Retrieves the type of the value in this object.
     *
     *  @returns A type name for the value in this object.
     */
    ValueType getType() const
    {
        return m_type;
    }

    /** Retrieves the value in this object.
     *
     *  @returns The value converted into the requested format if possible.
     */
    const char* getString() const
    {
        if (m_type != ValueType::eString)
            return nullptr;

        return m_value.string;
    }

    /** @copydoc getString() */
    long getLong() const
    {
        return getNumber<long>();
    }

    /** @copydoc getString() */
    long long getLongLong() const
    {
        return getNumber<long long>();
    }

    /** @copydoc getString() */
    float getFloat() const
    {
        return getNumber<float>();
    }

    /** @copydoc getString() */
    double getDouble() const
    {
        return getNumber<double>();
    }

private:
    template <typename T>
    T getNumber() const
    {
        switch (m_type)
        {
            default:
            case ValueType::eString:
                return 0;

            case ValueType::eLong:
                return static_cast<T>(m_value.integer);

            case ValueType::eLongLong:
                return static_cast<T>(m_value.longInteger);

            case ValueType::eFloat:
                return static_cast<T>(m_value.floatValue);

            case ValueType::eDouble:
                return static_cast<T>(m_value.doubleValue);
        }
    }

    ValueType m_type; ///< The type of the value in this object.

    /** The value contained in this object. */
    union
    {
        const char* string;
        long integer;
        long long longInteger;
        float floatValue;
        double doubleValue;
    } m_value;
};

/** Receives the results of parsing an options string.  This represents the base class that
 *  specific option parsers should inherit from.
 */
class Options
{
public:
    /** The original argument count from the caller. */
    int argc = 0;

    /** The original argument list from the caller. */
    char** argv = nullptr;

    /** The index of the first argument that was not consumed as a parsed option. */
    int firstCommandArgument = -1;

    /** Helper function to cast the @a args parameter to a parser function.
     *
     *  @returns This object cast to the requested type.
     */
    template <typename T>
    T* cast()
    {
        return reinterpret_cast<T*>(this);
    }
};

/** Prototype of a parser function to handle a single option.
 *
 *  @param[in] name     The name of the option being parsed.  If the name and value was in a
 *                      pair separated by an equal sign ('='), both the name and value will
 *                      be present in the string.  This will not be `nullptr`.
 *  @param[in] value    The value of the option if one is expected, or `nullptr` if no option
 *                      is expected.  This will be the value to be passed in with the option.
 *  @param[out] args    Receives the results of parsing the option.  It is the handler's
 *                      responsibility to ensure this object is filled in or initialized
 *                      properly.
 *  @returns A @ref ParseResult result code indicating whether the parsing was successful.
 */
using ArgParserFunc = ParseResult (*)(const char* name, const Value* value, Options* args);

/** Information about a single option and its parser. */
struct Option
{
    /** The short name for the option.  This is usually a one letter option preceded by
     *  a dash character.
     */
    const char* shortName;

    /** The long name for the option.  This is usually a multi-word option preceded by
     *  two dash characters.
     */
    const char* longName;

    /** The number of arguments to be expected associated with this option.  This should
     *  either be 0 or 1.
     */
    int expectedArgs;

    /** The expected argument type. */
    ValueType expectedType;

    /** The parser function that will handle consuming the option and its argument. */
    ArgParserFunc parser;

    /** Documentation for this option.  This string should be formatted to fit on a 72
     *  character line.  Each line of text should end with a newline character ('\n').
     *  The last line of text must also end in a newline character otherwise it will
     *  be omitted from any documentation output.
     */
    const char* documentation;
};


/** Retrieves a single argument value from the next argument.
 *
 *  @param[in] argc     The number of arguments in the argument vector.  This must be at least 1.
 *  @param[in] argv     The full argument vector to parse.  This may not be `nullptr`.
 *  @param[in] argIndex The zero-based index of the argument to parse.
 *  @param[out] value   Receives the start of the value's string.
 *  @returns The number of arguments that were consumed.  This may be 0 or 1.
 *  @returns `-1` if no value could be found for the option.
 *
 *  @remarks This parses a single option's value from the argument list.  The value may either
 *           be in the same argument as the option name itself, separated by an equal sign ('='),
 *           or in the following argument in the list.  If an argument cannot be found (ie:
 *           no arguments remain and an equal sign is not found), this will fail.
 *
 *  @note If the argument's value is quoted (single or double quotes), the entry in the original
 *        argument string will be modified to strip off the terminating quotation mark character.
 *        If the argument's value is not quoted, nothing will be modified in the argument string.
 */
inline int getArgString(int argc, char** argv, int argIndex, const char** value)
{
    char* equal;
    int argsConsumed = 0;
    char* valueOut;


    equal = strchr(argv[argIndex], '=');

    // the argument is of the form "name=value" => parse the number from the arg itself.
    if (equal != nullptr)
        valueOut = equal + 1;

    // the argument is of the form "name value" => parse the number from the next arg.
    else if (argIndex + 1 < argc)
    {
        valueOut = argv[argIndex + 1];
        argsConsumed = 1;
    }

    // not enough args given => fail.
    else
    {
        fprintf(stderr, "expected another argument after '%s'.\n", argv[argIndex]);
        return kArgFailExpectedArgument;
    }

    if (valueOut[0] == '\"' || valueOut[0] == '\'')
    {
        char openQuote = valueOut[0];
        size_t len = strlen(valueOut);


        if (valueOut[len - 1] != openQuote)
            return kArgFailExpectedQuote;

        valueOut[len - 1] = 0;
        valueOut++;
    }

    *value = valueOut;
    return argsConsumed;
}

/** Parses a set of options from an argument vector.
 *
 *  @param[in] supportedArgs    The table of options that will be used to parse the program
 *                              arguments.  This table must be terminated by an empty entry
 *                              in the list (ie: all values `nullptr` or `0`).  This may not
 *                              be `nullptr`.
 *  @param[in] argc             The argument count for the program.  This must be at least 1.
 *  @param[in] argv             The vector of arguments to e be parsed.  This must not be `nullptr`.
 *  @param[out] args            Receives the parsed option state.  It is the caller's
 *                              responsibility to ensure this is appropriately initialized before
 *                              calling.  This object must inherit from the @ref Options class.
 *  @returns `true` if all arguments are successfully parsed.
 *  @returns `false` if an option fails to be parsed.
 */
inline bool parseOptions(const Option* supportedArgs, int argc, char** argv, Options* args)
{
    const char* valueStr;
    bool handled;
    int argsConsumed;
    ParseResult result;
    Value value;
    Value* valueToSend;


    auto argMatches = [](const char* string, const char* arg, bool terminated) {
        size_t len = strlen(arg);

        if (!terminated)
            return strncmp(string, arg, len) == 0;

        return strncmp(string, arg, len) == 0 && (string[len] == 0 || string[len] == '=');
    };

    for (int i = 1; i < argc; i++)
    {
        handled = false;

        for (size_t j = 0; supportedArgs[j].parser != nullptr; j++)
        {
            bool checkTermination = supportedArgs[j].expectedType != ValueType::eIgnore;

            if ((supportedArgs[j].shortName != nullptr &&
                 argMatches(argv[i], supportedArgs[j].shortName, checkTermination)) ||
                (supportedArgs[j].longName != nullptr && argMatches(argv[i], supportedArgs[j].longName, checkTermination)))
            {
                valueToSend = nullptr;
                valueStr = nullptr;
                argsConsumed = 0;
                value.clear();

                if (supportedArgs[j].expectedArgs > 0)
                {
                    argsConsumed = getArgString(argc, argv, i, &valueStr);

                    if (argsConsumed < 0)
                    {
                        switch (argsConsumed)
                        {
                            case kArgFailExpectedArgument:
                                fprintf(
                                    stderr, "ERROR-> expected an extra argument after argument %d ('%s').", i, argv[i]);
                                break;

                            case kArgFailExpectedQuote:
                                fprintf(
                                    stderr,
                                    "ERROR-> expected a matching quotation mark at the end of the argument value for argument %d ('%s').",
                                    i, argv[i]);
                                break;

                            default:
                                break;
                        }

                        return false;
                    }

                    if (supportedArgs[j].expectedType == ValueType::eIgnore)
                    {
                        i += argsConsumed;
                        handled = true;
                        break;
                    }

#if !defined(DOXYGEN_SHOULD_SKIP_THIS)
#    define SETVALUE(value, str, type)                                                                                 \
        do                                                                                                             \
        {                                                                                                              \
            type convertedValue;                                                                                       \
            if (!stringToNumber(str, &convertedValue))                                                                 \
            {                                                                                                          \
                fprintf(stderr, "ERROR-> expected a %s value after '%s'.\n", #type, argv[i]);                          \
                return false;                                                                                          \
            }                                                                                                          \
            value.set(convertedValue);                                                                                 \
        } while (0)
#endif

                    switch (supportedArgs[j].expectedType)
                    {
                        default:
                        case ValueType::eString:
                            value.set(valueStr);
                            break;

                        case ValueType::eLong:
                            SETVALUE(value, valueStr, long);
                            break;

                        case ValueType::eLongLong:
                            SETVALUE(value, valueStr, long long);
                            break;

                        case ValueType::eFloat:
                            SETVALUE(value, valueStr, float);
                            break;

                        case ValueType::eDouble:
                            SETVALUE(value, valueStr, double);
                            break;
                    }

                    valueToSend = &value;
#undef SETVALUE
                }

                result = supportedArgs[j].parser(argv[i], valueToSend, args);

                switch (result)
                {
                    default:
                    case ParseResult::eSuccess:
                        break;

                    case ParseResult::eInvalidValue:
                        fprintf(stderr, "ERROR-> unknown or invalid value in '%s'.\n", argv[i]);
                        return false;
                }

                i += argsConsumed;
                handled = true;
                break;
            }
        }

        if (!handled)
        {
            if (args->firstCommandArgument < 0)
                args->firstCommandArgument = i;

            break;
        }
    }

    args->argc = argc;
    args->argv = argv;
    return true;
}

/** Prints out the documentation for an option table.
 *
 *  @param[in] supportedArgs    The table of options that this program can parse.  This table
 *                              must be terminated by an empty entry in the list (ie: all values
 *                              `nullptr` or `0`).  This may not be nullptr.  The documentation
 *                              for each of the options in this table will be written out to the
 *                              selected stream.
 *  @param[in] helpString       The help string describing this program, its command line
 *                              syntax, and its commands.  This should not include any
 *                              documentation for the supported options.
 *  @param[in] stream           The stream to output the documentation to.  This may not be
 *                              `nullptr`.
 *  @returns No return value.
 */
inline void printOptionUsage(const Option* supportedArgs, const char* helpString, FILE* stream)
{
    const char* str;
    const char* newline;
    const char* argStr;


    fputs(helpString, stream);
    fputs("Supported options:\n", stream);

    for (size_t i = 0; supportedArgs[i].parser != nullptr; i++)
    {
        str = supportedArgs[i].documentation;
        argStr = "";

        if (supportedArgs[i].expectedArgs > 0)
            argStr = " [value]";

        if (supportedArgs[i].shortName != nullptr)
            fprintf(stream, "    %s%s:\n", supportedArgs[i].shortName, argStr);

        if (supportedArgs[i].longName != nullptr)
            fprintf(stream, "    %s%s:\n", supportedArgs[i].longName, argStr);

        for (newline = strchr(str, '\n'); newline != nullptr; str = newline + 1, newline = strchr(str + 1, '\n'))
            fprintf(stream, "        %.*s\n", static_cast<int>(newline - str), str);

        fputs("\n", stream);
    }

    fputs("\n", stream);
}

} // namespace options
} // namespace carb
