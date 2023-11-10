// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief The L10n interface.
 */
#pragma once
#include "../Defines.h"

#include "../logging/Log.h"

namespace carb
{
/** Utilities for localizing text. */
namespace l10n
{

/** The return type for @ref IL10n::getHashFromKeyString(). */
using StringIdentifier = uint64_t;

/** An opaque struct representing a localization table. */
struct LanguageTable
{
};

/** An opaque struct representing a language ID. */
struct LanguageIdentifier
{
};

/** Use the main language table for the process if this is passed. */
const LanguageTable* const kLanguageTableMain = nullptr;

/** The currently set language will be used when this is passed. */
const LanguageIdentifier* const kLanguageCurrent = nullptr;

/** The entry point to getLocalizedStringFromHash().
 *  @copydoc IL10n::getLocalizedStringFromHash
 */
using localizeStringFn = const char*(CARB_ABI*)(const LanguageTable* table,
                                                StringIdentifier id,
                                                const LanguageIdentifier* language);

/** The default language will be used when this is passed.
 *  The default language will always be US English.
 */
const LanguageIdentifier* const kLanguageDefault = reinterpret_cast<const LanguageIdentifier*>(0xFFFFFFFFFFFFFFFF);

/** This is returned from some interface functions when an unknown language is
 *  requested.
 */
const LanguageIdentifier* const kLanguageUnknown = reinterpret_cast<const LanguageIdentifier*>(0xFFFFFFFFFFFFFFFE);

/** A definition that can be used for loading a language table embedded in C++ code. */
struct LanguageTableData
{
    /** The number of languages in the table. */
    size_t languagesLength;

    /** The number of translation entries in the table.
     *  Any valid language table will have at least 4 rows, since the first 4
     *  rows have special meanings.
     */
    size_t keysLength;

    /** The list of translation languages. These are specified as POSIX locale identifiers.
     *  The length of this array is @ref languagesLength.
     *  The first language in this array must be "en_US*"
     */
    const char* const* languages;

    /** The hashes of the key strings for the translations.
     *  The length of this array is @ref keysLength.
     *  Note that this contains keys for the first 4 rows in the table, even
     *  though the first 4 rows have a special purpose. The first 4 keys are
     *  never read.
     */
    const uint64_t* keys;

    /** The translation table.
     *  This is a matrix with @ref languagesLength columns and @ref keysLength rows.
     *  languageTable[i][j] refers to the translation of keys[i] in languages[j].
     *  The first 4 rows have special usages:
     *   0: The language names for each column in US English
     *   1: The territory names for each column in US English
     *   2: The language names for each column in the language for that column
     *   3: The territory names for each column in the language for that column
     */
    const char* const* languageTable;
};

/** Boolean value tags for the getLanguageName() and getTerritoryName()
 *  functions.  These determine how the language and territory names will be
 *  returned.  Note, returning the name of the language in any other arbitrary
 *  supported language is beyond the scope of the automatic behavior of the
 *  tables.  If such an arbitrary translation is needed, the language's name
 *  would have to be added to each table and translated into each target
 *  language.  Accessing the arbitrary translations in that case would end up
 *  as a lookupString() call.
 */
enum class LocalizedName
{
    /** Retrieve the name in US English (ie: "Polish").  Note that this will
     *  always be the first or second string entries in any given translation
     *  table.
     */
    eUsEnglish,

    /** Retrieve the name in the language the identifier specifies (ie:
     *  "Polski").  Note that this will always be the third or fourth string
     *  entries in any given translation table.
     */
    eLocalized,
};

/** The localization interface. */
struct IL10n
{
    CARB_PLUGIN_INTERFACE("carb::l10n::IL10n", 1, 0)


    /** Calculates the lookup hash for a US English key string.
     *
     *  @param[in] keyString The string to calculate the hash identifier for.
     *                       This may not be nullptr or an empty string.
     *  @returns The calculated hash of the string.  This will be the same
     *           algorithm that is used by the `String Table Conversion Tool`
     *           to generate the table and mapping structure.
     *  @returns 0 if the input string is nullptr or empty.
     *
     *  @remarks This calculates the hash value for a string.  This is useful
     *           for scripts to be able to pre-hash and cache their string
     *           identifiers for quicker lookups later.
     *
     *  @note This is not intended to be directly used in most situations.
     *        Typical C++ code should use CARB_LOCALIZE() and typical python
     *        code should use carb_localize() or carb_localize_hashed().
     */
    StringIdentifier(CARB_ABI* getHashFromKeyString)(const char* keyString) noexcept;

    /** Looks up a string's translation in the localization system.
     *
     *  @param[in] table    The optional local language table to search first
     *                      for the requested key string.  If this is
     *                      non-nullptr and the key string is not found in this
     *                      table or the requested language is not supported by
     *                      this table, the framework's registered main table
     *                      will be checked as well.  This may be nullptr to
     *                      only search the framework's main table.
     *  @param[in] id       The hashed string identifier of the string to look
     *                      up.
     *  @param[in] language The language to retrieve the translated string in.
     *                      This may be set to @ref kLanguageCurrent to use the
     *                      current language for the localization system (this
     *                      is the default behavior).  This can also be any
     *                      specific language identifier to retrieve the string
     *                      in another supported language.  This may also be
     *                      @ref kLanguageDefault to retrieve the string in the
     *                      system's default language if a translation is
     *                      available.
     *  @returns The translated string is a supported language is requested and
     *           the string with the requested hash is found in the table.
     *  @returns nullptr if no translation is found in the table, if an
     *           unsupported language is requested, or if the key string has no
     *           mapping in the table.
     *  @returns An error message if the config setting to return noticeable
     *           failure strings is enabled.
     *
     *  @note This is not intended to be directly used in most situations.
     *        Typical C++ code should use CARB_LOCALIZE() and typical python
     *        code should use carb_localize() or carb_localize_hashed().
     */
    const char*(CARB_ABI* getLocalizedStringFromHash)(const LanguageTable* table,
                                                      StringIdentifier id,
                                                      const LanguageIdentifier* language) noexcept;

    /** Retrieves the current system locale information.
     *
     *  @returns A language identifier for the current system language if it
     *           matches one or more of the supported translation tables.
     *  @returns The language identifier for US English if no matching
     *           translation tables are found.
     */
    const LanguageIdentifier*(CARB_ABI* getSystemLanguage)() noexcept;


    /** Enumerates available/supported language identifiers in the localization system.
     *
     *  @param[in] table The optional local table to also search for unique
     *                   language identifiers to return.  If this is
     *                   non-nullptr, the supported language identifiers in
     *                   this table will be enumerated first, followed by any
     *                   new unique language identifiers in the framework's
     *                   registered main table.  This may be nullptr to only
     *                   enumerate identifiers in the main table.
     *  @param[in] index The index of the language identifier number to be
     *                   returned.  Set this to 0 to retrieve the first
     *                   supported language (this will always return the
     *                   language identifier corresponding to US English as the
     *                   first supported language identifier).  Set this to
     *                   increasing consecutive indices to retrieve following
     *                   supported language codes.
     *  @returns The language identifier corresponding to the supported
     *           language at index @p index.
     *  @retval kLanguageUnknown if the given index is out of range of
     *           the supported languages.
     */
    const LanguageIdentifier*(CARB_ABI* enumLanguageIdentifiers)(const LanguageTable* table, size_t index) noexcept;

    /** Retrieves the language identifier for a given locale name.
     *
     *  @param[in] table     The optional local table to also search for a
     *                       matching language identifier in.  This may be
     *                       nullptr to only search the framework's 'main'
     *                       table.
     *  @param[in] language  The standard Unix locale name in the format
     *                       "<language>_<territory>" where "<language>" is a
     *                       two character ISO-639-1 language code and
     *                       "<territory>" is a two-character ISO-3166-1
     *                       Alpha-2 territory code.  An optional encoding
     *                       string may follow this but will be ignored.  This
     *                       must not be nullptr or an empty string.
     *  @returns The language identifier corresponding to the selected Unix
     *           locale name if a table for the requested language and
     *           territory is found.  If multiple matching supported tables are
     *           found for the requested language (ie: Canadian French, France
     *           French, Swiss French, etc), the one for the matching territory
     *           will be returned instead.  If no table exists for the
     *           requested territory in the given language, the language
     *           identifier for an arbitrary table for the requested language
     *           will be returned instead.  This behavior may be modified by a
     *           runtime config setting that instead causes @ref
     *           kLanguageUnknown to be returned if no exact language/territory
     *           match exists.
     *  @retval kLanguageUnknown if the requested language does not have
     *           a translation table for it in the localization system, or if
     *           the config setting to only allow exact matches is enabled and
     *           no exact language/territory match could be found.
     */
    const LanguageIdentifier*(CARB_ABI* getLanguageIdentifier)(const LanguageTable* table, const char* language) noexcept;

    /** Retrieves a language's or territory's name as a friendly string.
     *
     *  @param[in] table        The optional local language table to check for
     *                          the requested name first.  If this is nullptr
     *                          or the requested language identifier is not
     *                          supported by the given table, the framework's
     *                          main registered table will be checked.
     *  @param[in] language     The language identifier of the language or
     *                          territory name to retrieve.  This may not be
     *                          @ref kLanguageUnknown.  This may be @ref
     *                          kLanguageCurrent to retrieve the name for the
     *                          currently selected language.
     *  @param[in] retrieveIn   The language to return the string in.  This can
     *                          be used to force the language's or territory's
     *                          name to be returned in US English or the name
     *                          of @p language in @p language.
     *  @returns The name of the language or territory in the specified
     *           localization.
     *  @returns An empty string if the no translation table exists for the
     *           requested language or an invalid language identifier is given.
     *  @returns An error message if the config setting to return noticeable
     *           failure strings is enabled.
     *
     *  @note This will simply return the strings in the second and third, or
     *        fourth and fifth rows of the CSV table (which should have become
     *        properties of the table once loaded).
     */
    const char*(CARB_ABI* getLanguageName)(const LanguageTable* table,
                                           const LanguageIdentifier* language,
                                           LocalizedName retrieveIn) noexcept;

    /** @copydoc getLanguageName */
    const char*(CARB_ABI* getTerritoryName)(const LanguageTable* table,
                                            const LanguageIdentifier* language,
                                            LocalizedName retrieveIn) noexcept;

    /** Retrieves the standard Unix locale name for the requested language identifier.
     *
     *  @param[in] table    The optional local language table to retrieve the
     *                      locale identifier from.  This may be nullptr to
     *                      only search the framework's registered main
     *                      language table.
     *  @param[in] language The language identifier to retrieve the Unix locale
     *                      name for.  This may not be @ref kLanguageUnknown.
     *                      This may be @ref kLanguageCurrent to retrieve the
     *                      locale name for the currently selected language.
     *  @returns The standard Unix locale name for the requested language
     *           identifier.
     *  @returns an empty string if the language identifier is invalid or no translation table exist
     *           for it.
     *  @returns an error message if the config setting to return noticeable failure string is
     *           enabled.
     */
    const char*(CARB_ABI* getLocaleIdentifierName)(const LanguageTable* table,
                                                   const LanguageIdentifier* language) noexcept;

    /** Sets the new current language from a standard Unix locale name or language identifier.
     *
     *  @param[in] table    The optional local language table to check to see
     *                      if the requested language is supported or not.
     *                      This may be nullptr to only search the framework's
     *                      registered main table.  If the local table doesn't
     *                      support the requested language, the framework's
     *                      main table will still be searched.
     *  @param[in] language Either the locale name or identifier for the new
     *                      language to set as current for the calling process.
     *                      For the string version, this may be nullptr or an
     *                      empty string to switch back to the system default
     *                      language.  For the language identifier version,
     *                      this may be set to @ref kLanguageDefault to switch
     *                      back to the system default language.
     *  @returns true if the requested language is supported and is
     *           successfully set.
     *  @returns false if the requested language is not supported.
     *           In this case, the current language will not be modified.
     *
     *  @note the variant that takes a string locale identifier will just be a
     *        convenience helper function that first looks up the language
     *        identifier for the locale then passes it to the other variant.
     *        If the locale lookup fails, the call will fail since it would be
     *        requesting an unsupported language.
     */
    bool(CARB_ABI* setCurrentLanguage)(const LanguageTable* table, const LanguageIdentifier* language) noexcept;

    /** @copydoc setCurrentLanguage */
    bool(CARB_ABI* setCurrentLanguageFromString)(const LanguageTable* table, const char* language) noexcept;

    /** Retrieves the language identifier for the current language.
     *
     *  @returns The identifier for the current language.
     *  @retval kLanguageDefault if an error occurs.
     */
    const LanguageIdentifier*(CARB_ABI* getCurrentLanguage)() noexcept;

    /** Registers the host app's main language translation table.
     *
     *  @param[in] table The table to register as the app's main lookup table.
     *                   This may be nullptr to indicate that no language table
     *                   should be used and that only US English strings will
     *                   be used by the app.
     *  @returns true if the new main language table is successfully set.
     *  @returns false if the new main language table could not be set.
     *
     *  @note This is a per-process setting.
     */
    bool(CARB_ABI* setMainLanguageTable)(const LanguageTable* table) noexcept;


    /** Creates a new local language translation table.
     *
     *  @param[in] data The language table to load.
     *                  This language table must remain valid and constant
     *                  until unloadLanguageTable() is called.
     *                  The intended use of this function is to load a static
     *                  constant data table.
     *  @returns The newly loaded and created language table if the data file
     *           exists and was successfully loaded.  This must be destroyed
     *           with unloadLanguageTable() when it is no longer needed.
     *  @returns nullptr if an unrecoverable error occurred.
     */
    LanguageTable*(CARB_ABI* loadLanguageTable)(const LanguageTableData* data) noexcept;

    /** Creates a new local language translation table from a data file.
     *
     *  @param[in] filename The name of the data file to load as a language
     *                      translation table.  This may not be nullptr or an
     *                      empty string.  If this does not have an extension,
     *                      both the given filename and one ending in ".lang"
     *                      will be tried.
     *  @returns The newly loaded and created language table if the data file
     *           exists and was successfully loaded.  This must be destroyed
     *           with unloadLanguageTable() when it is no longer needed.
     *  @returns nullptr if the data file was not found with or without the
     *           ".lang" extension, or the file was detected as corrupt while
     *           loading.
     *
     *  @note The format of the localization file is as follows:
     *        byte count | segment description
     *        [0-13]     | File signature. The exact UTF-8 text: "nvlocalization".
     *        [14-15]    | File format version. Current version is 00.
     *                   | This version number is 2 hex characters.
     *        [16-19]    | Number of languages.
     *                   | This corresponds to @ref LanguageTableData::languagesLength.
     *        [20-23]    | Number of keys.
     *                   | This corresponds to @ref LanguageTableData::keysLength.
     *        [24-..]    | Table of @ref LanguageTableData::keysLength 64 bit keys.
     *                   | This is @ref LanguageTableData::keysLength * 8 bytes long.
     *                   | This corresponds to @ref LanguageTableData::keys.
     *        [..-..]    | Block of @ref LanguageTableData::languagesLength null
     *                   | terminated language names.
     *                   | This will contain exactly @ref LanguageTableData::languagesLength
     *                   | 0x00 bytes; each of those bytes indicates the end of a string.
     *                   | The length of this segment depends on the data within it;
     *                   | the full segment must be read to find the start of the
     *                   | next section.
     *                   | This corresponds to @ref LanguageTableData::languages.
     *        [..-..]    | Block of @ref LanguageTableData::languagesLength *
     *                   | @ref LanguageTableData::keysLength
     *                   | null terminated translations.
     *                   | This will contain exactly @ref LanguageTableData::languagesLength *
     *                   | @ref LanguageTableData::keysLength 0x00 bytes; each of those bytes
     *                   | indicates the end of a string.
     *                   | The last byte of the file should be the null terminator of the last
     *                   | string in the file.
     *                   | The length of this section also depends on the length of
     *                   | the data contained within these strings.
     *                   | If the end of the file is past the final 0x00 byte in this
     *                   | segment, the reader will assume the file is corrupt.
     *                   | This corresponds to @ref LanguageTableData::languageTable.
     */
    LanguageTable*(CARB_ABI* loadLanguageTableFromFile)(const char* fileName) noexcept;

    /** The language table to be destroyed.
     *
     *  @param[in] table The language table to be destroyed.
     *                   This must not be nullptr.
     *                   This should be a table that was previously returned
     *                   from loadLanguageTable().
     *                   It is the caller's responsibility to ensure this table
     *                   will no longer be needed or accessed.
     */
    void(CARB_ABI* unloadLanguageTable)(LanguageTable* table) noexcept;

    /** Sets the current search path for finding localization files for a module.
     *
     *  @param[in] searchPath   The search path for where to look for
     *                          localization data files.
     *                          This can be an absolute or relative path.
     *  @returns true if the new search path is successfully set.
     *  @returns false if the new search path could not be set.
     *
     *  @remarks This sets the search path to use for finding localization
     *           files when modules load.  By default, only the same directory
     *           as the loaded module or script will be searched.  This can be
     *           used to specify additional directories to search for
     *           localization files in.  For example, the localization files
     *           may be stored in the 'lang/' folder for the app instead of in
     *           the 'bin/' folder.
     */
    bool(CARB_ABI* addLanguageSearchPath)(const char* searchPath) noexcept;

    /** Sets the current search path for finding localization files for a module.
     *
     *  @param[in] searchPath   The search path to remove from the search path
     *                          list.
     *  @returns true if the search path was successfully removed.
     *  @returns false if the search path was not found.
     *
     *  @remarks This removes a search path added by addLanguageSearchPath().
     *           If the same path was added multiple times, it will have to be
     *           removed multiple times.
     *
     *  @note The executable directory can be removed from the search path
     *        list, if that is desired.
     */
    bool(CARB_ABI* removeLanguageSearchPath)(const char* searchPath) noexcept;

    /** Enumerate the search paths that are currently set.
     *  @param[in] index The index of the search path to retrieve.
     *                   The first search path index will always be 0.
     *                   The valid search paths are a contiguous range of
     *                   indices, so the caller can pass incrementing values
     *                   beginning at 0 for @p index to enumerate all of the
     *                   search paths.
     *
     *  @returns The search path corresponding to @p index if one exists at index.
     *  @returns nullptr if there is no search path corresponding to @p index.
     *
     *  @remarks The example usage of this function would be to call this in a
     *           loop where @p index starts at 0 and increments until a call to
     *           enumLanguageSearchPaths(@p index) returns nullptr. This would
     *           enumerate all search paths that are currently set.
     *           The index is no longer valid if the search path list has been
     *           modified.
     */
    const char*(CARB_ABI* enumLanguageSearchPaths)(size_t index) noexcept;
};

/** A version of getLocalizedStringFromHash() for when the localization plugin is unloaded.
 *  @param[in] table    The localization table to use for the lookup.
 *  @param[in] id       The hash of @p string.
 *  @param[in] language The language to perform the lookup in.
 *  @returns nullptr.
 */
inline const char* CARB_ABI getLocalizedStringFromHashNoPlugin(const LanguageTable* table,
                                                               StringIdentifier id,
                                                               const LanguageIdentifier* language) noexcept
{
    CARB_UNUSED(table, id, language);
    CARB_LOG_ERROR("localization is being used with carb.l10n.plugin not loaded");
    return nullptr;
}

} // namespace l10n
} // namespace carb

/** Pointer to the interface for use from CARB_LOCALIZE(). Defined in @ref CARB_LOCALIZATION_GLOBALS. */
CARB_WEAKLINK CARB_HIDDEN carb::l10n::IL10n* g_carbLocalization;

/** Pointer to the function called by CARB_LOCALIZE(). Defined in @ref CARB_LOCALIZATION_GLOBALS. */
CARB_WEAKLINK CARB_HIDDEN carb::l10n::localizeStringFn g_localizationFn = carb::l10n::getLocalizedStringFromHashNoPlugin;

/* Exhale can't handle these for some reason and they aren't meant to be used directly */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace carb
{
namespace l10n
{
/** An internal helper for CARB_LOCALIZE()
 *  @param[in] id     The hash of @p string.
 *  @param[in] string The localization keystring.
 *  @returns The translated string is a supported language is requested and
 *           the string with the requested hash is found in the table.
 *  @returns @p string if no translation is found in the table, if an
 *           unsupported language is requested, or if the key string has no
 *           mapping in the table.
 *  @returns An error message if the config setting to return noticeable
 *           failure strings is enabled.
 *
 *  @note This is an internal implementation for CARB_LOCALIZE() as well as the
 *        script bindings. Do not directly call this function.
 */
inline const char* getLocalizedString(StringIdentifier id, const char* string) noexcept
{
    const char* s = g_localizationFn(kLanguageTableMain, id, kLanguageCurrent);
    return (s != nullptr) ? s : string;
}

} // namespace l10n
} // namespace carb
#endif

/** Look up a string from the localization database for the current plugin.
 *  @param string A string literal.
 *                This must not be nullptr.
 *                This is the key string to look up in the database.
 *
 *  @returns The localized string for the keystring @p string, given the current
 *           localization that has been set for the process.
 *  @returns If there is no localized string for the given keystring @p string,
 *           the US English string will be returned.
 *  @returns If @p string is not found in the localization database at all,
 *           @p string will be returned.
 *  @returns An error message if the localized string is found and the config
 *           setting to return noticeable failure strings is enabled.
 */
#define CARB_LOCALIZE(string) carb::l10n::getLocalizedString(CARB_HASH_STRING(string), string)
