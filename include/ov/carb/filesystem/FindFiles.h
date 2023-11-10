// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! \file
//! \brief Utilities for finding files.
#pragma once

#include "../extras/Path.h"
#include "../extras/StringProcessor.h"
#include "../extras/StringUtils.h"
#include "IFileSystem.h"
#include "../Format.h"

#include "../../omni/str/Wildcard.h"

#include <cstdint>
#include <cstring>

namespace carb
{
namespace filesystem
{

//! Flag type for \ref FindFilesArgs
//! @see kFindFilesFlagNone kFindFilesFlagRecursive kFindFilesFlagMatchStem kFindFilesFlagReplaceEnvironmentVariables
using FindFilesFlag = uint32_t;

//! Default empty flag
constexpr FindFilesFlag kFindFilesFlagNone = 0x0;

//! Flag to recursively search directories.
constexpr FindFilesFlag kFindFilesFlagRecursive = (1 << 0);

//! When matching wildcards, only match the "stem".
//!
//! The "stem" is defined by carb::extras::Path.  In short, given the following files:
//!
//!   /a/b/c/d.txt       -> d
//!   /a/b/c/d           -> d
//!   /a/b/c/.d          -> .d
//!   /a/b/c/d.old.txt   -> d.old
constexpr FindFilesFlag kFindFilesFlagMatchStem = (1 << 1);

//! Before walking the filesystem, a text replace is performed on each given search path. The token `${MY_ENV_VAR}`
//! would be replaced with the environment variable `MY_ENV_VAR`.
//!
//! Example: ${USERPROFILE}/kit   -> C:/Users/ncournia/kit
constexpr FindFilesFlag kFindFilesFlagReplaceEnvironmentVariables = (1 << 2);

//! Callback for each encountered file invoked before canonicalization and pattern matching.
//! @param path the non-canonical path encountered
//! @param userData the value of \ref FindFilesArgs::onFilterNonCanonicalContext
//! @retval WalkAction::eContinue to continue with canonicalization and pattern matching.
//! @retval WalkAction::eSkip to stop processing the file and move to the next file.
//! @retval WalkAction::eStop to stop the search.
using FindFilesOnFilterNonCanonicalFn = WalkAction(const char* path, void* userData);

//! Callback invoked when a file matches a pattern matchWildcards and does not match a pattern in excludeWildcards.
//! @param canonical The canonical path to the file
//! @param userData the value of \ref FindFilesArgs::onMatchedContext
using FindFilesOnMatchedFn = void(const char* canonical, void* userData);

//! Callback invoked when a file matches a pattern matchWildcards and excludeWildcards.
//! @param canonical The canonical path to the file
//! @param userData the value of \ref FindFilesArgs::onExcludedContext
using FindFilesOnExcludedFn = void(const char* canonical, void* userData);

//! Callback invoked when a file matches does not match a pattern in matchWildcards.
//! @param canonical The canonical path to the file
//! @param userData the value of \ref FindFilesArgs::onSkippedContext
using FindFilesOnSkippedFn = void(const char* canonical, void* userData);

//! Callback invoked when starting a search in one of the given search paths.
//! @param canonical The canonical path to the file
//! @param userData the value of \ref FindFilesArgs::onSearchPathContext
using FindFilesOnSearchPathFn = void(const char* path, void* userData);

//! Search parameters passed to findFiles().
//!
//! Recommended usage:
//!
//!     FindFilesArgs args{}; // this zero initializes arguments
//!
//!     const char* const* paths = { "/myPath", "myRelativePath" };
//!     const char* const* patterns = { "*.dll" };
//!
//!     args.searchPaths = paths;
//!     args.searchPathsCount = CARB_COUNTOF(paths);
//!
//!     args.matchWildcards = patterns;
//!     args.matchWildcardsCount = CARB_COUNTOF(patterns);
//!
//!     args.onMatched = [](const char* canonical, void*) { printf("found: %s\n", canonical); };
//!
//!     if (!findFiles(args)) { CARB_LOG_ERROR(" :( "); }
//!
struct FindFilesArgs
{
    //! A list of paths (directories) to search.
    //!
    //! If \ref kFindFilesFlagReplaceEnvironmentVariables is specified in flags, tokens in the form `${MY_ENV_VAR}` will
    //! be replaced with the corresponding environment variable.
    //!
    //! If \ref kFindFilesFlagRecursive is specified in flags, each path is searched recursively.
    //!
    //! Paths may be absolute or relative.  If relative, the value of \ref IFileSystem::getAppDirectoryPath() is
    //! prepended to the path.
    //!
    //! @warning Must not be nullptr.
    const char* const* searchPaths;
    uint32_t searchPathsCount; //!< Number of paths in searchPaths.  Must be greater than 0.

    //! The wildcard pattern to match files in the given searchPaths.
    //!
    //! Special characters are:
    //!   * `*` matches all characters
    //!   * `?` matches a single character.
    //!
    //! For example, given the file `"libMyPlugin.plugin.dll"`:
    //!   * `*` -> Matches!
    //!   * `lib` -> Not a match.
    //!   * `*.so` -> Not a match.
    //!   * `lib*` -> Matches!
    //!   * `*.plugin.*` -> Matches!
    //!   * `libMyPlugin.plugin?dll` -> Matches!
    //!
    //! When matching, only the filename is considered.  So, given the path `/a/b/c/d.txt` only `d.txt` is considered
    //! when matching.
    //!
    //! The extension of the filename can be ignored by passing the \ref kFindFilesFlagMatchStem to flags.
    //!
    //! Matched files may be excluded (see \ref excludeWildcards below).
    //!
    //! @warning Must not be nullptr.
    const char* const* matchWildcards;
    uint32_t matchWildcardsCount; //!< Number of patterns in matchWildcards.  Must be greater than 0.

    //! The wildcard pattern to exclude files in the given search paths.
    //!
    //! Pattern matching follow the same rules as \ref matchWildcards.
    //!
    //! These patterns are checked only when the filename matches \ref matchWildcards.  If the filename matches a
    //! pattern in both \ref matchWildcards and \ref excludeWildcards, the file is excluded.
    //!
    //! May be nullptr.
    const char* const* excludeWildcards;
    uint32_t excludeWildcardsCount; //!< Number of patterns in excludeWildcards.  May be 0.

    //! A list of prefixes to ignore during pattern matching.
    //!
    //! If not pattern matches a filename, pattern matching will be attempted again with the prefixes in this list
    //! stripped.  For example, given:
    //!
    //!     ignorePrexies = { "lib" };
    //!     matchWildcard = { "MyPlugin.plugin.*" };
    //!
    //! We see the following output:
    //! * `MyPlugin.plugin.dll` -> Match!
    //! * `libMyPlugin.plugin.dll` -> Match! ("lib" prefix was ignored)
    //!
    //! May be nullptr
    const char* const* ignorePrefixes;
    uint32_t ignorePrefixesCount; //!< Number of prefixes in ignorePrefixes.  May be 0.

    //! IFileSystem to use to walk the given search paths.
    //!
    //! If nullptr, `tryAcquireInterface<IFileSystem>` is called.
    IFileSystem* fs;

    //! Callback for each encountered file invoked before canonicalization and pattern matching.
    //!
    //! This callback is invoked on each file in the given search paths.  The callback is invoked before many of the
    //! expensive parts of the search algorithm, such as file canonicalization and pattern matching.
    //! May be nullptr.
    FindFilesOnFilterNonCanonicalFn* onFilterNonCanonical;
    void* onFilterNonCanonicalContext; //!< Context passed to onFilterNonCanonical. May be nullptr.

    //! Callback invoked when a file matches a pattern matchWildcards and does not match a pattern in excludeWildcards.
    //!
    //! May be nullptr.
    FindFilesOnMatchedFn* onMatched;
    void* onMatchedContext; //!< Context passed to onMatched. May be nullptr.

    //! Callback invoked when a file matches a pattern matchWildcards and excludeWildcards.
    //!
    //! May be nullptr.
    FindFilesOnExcludedFn* onExcluded;
    void* onExcludedContext; //!< Context passed to onExcluded. May be nullptr.

    //! Callback invoked when a file matches does not match a pattern in matchWildcards.
    //!
    //! May be nullptr.
    FindFilesOnSkippedFn* onSkipped;
    void* onSkippedContext; //!< Context passed to onSkipped. May be nullptr.

    //! Callback invoked when starting a search in one of the given search paths.
    //!
    //! May be nullptr.
    FindFilesOnSearchPathFn* onSearchPath;
    void* onSearchPathContext; //!< Context passed to onSearchPath. May be nullptr.

    //! Bitmask of flags to change search behavior.  See FindFilesFlag.
    FindFilesFlag flags;
};

//! Finds files in a given list of search paths matching a given list of patterns.
//!
//! See \ref FindFilesArgs for argument documentation.
//!
//! The search can be incredibly expensive:
//! `O(searchPathsCount^(<files>^(ignorePrefixesCount^(matchWildcardsCount + excludeWildcardsCount))))`
//!
//! The following Python-ish pseudo-code gives you an idea of the algorithm used:
//! \rst
//! .. code-block:: python
//!
//!    foreach path in searchPaths:
//!     path = substituteEnvVars(path)
//!     foreach file in path:
//!      if eContinue == onFilterNonCanonical(file):
//!       foreach prefix in ["", ignorePrefixes]:
//!        stripped = prefix[len(prefix):]
//!        matched = False
//!        if isMatch(stripped, matchWildcards): # loops over each wildcard pattern
//!         if isMatch(stripped, excludeWildcards): # loops over each wildcard pattern
//!          return # any match of an exclusion pattern immediately excludes the file
//!         else:
//!          matched = True
//!       if matched:
//!         return onMatched(canonical)
//! \endrst
//! \param args The arguments as a structure
//! \returns `true` if the filesystem was searched; `false` otherwise.
inline bool findFiles(const FindFilesArgs& args);

#ifndef DOXYGEN_BUILD
namespace detail
{

struct FindFilesContext
{
    const FindFilesArgs* args;
    IFileSystem* fs;
};

inline WalkAction onFile(const DirectoryItemInfo* const info, void* userData)
{
    if (info->type == DirectoryItemType::eFile)
    {
        auto context = reinterpret_cast<const FindFilesContext*>(userData);

        // the canonicalization of a file is expensive.  here we give the user a chance to
        // tell us if we should canonicalize the file
        if (context->args->onFilterNonCanonical)
        {
            WalkAction action =
                context->args->onFilterNonCanonical(info->path, context->args->onFilterNonCanonicalContext);
            switch (action)
            {
                case WalkAction::eStop:
                    return WalkAction::eStop;

                case WalkAction::eSkip:
                    return WalkAction::eContinue;

                default: // eContinue
                    break; // fall-through
            }
        }

        std::string canonical = context->fs->makeCanonicalPath(info->path);

        extras::Path path(canonical);
        std::string target;
        if (context->args->flags & kFindFilesFlagMatchStem)
        {
            target = path.getStem();
        }
        else
        {
            target = path.getFilename();
        }

        const char* toMatch = target.c_str();

        // note, even if a pattern matches, we still have to loop through all of "ignorePrefixesCount" looking for an
        // exclusion since exclusions take precedent.
        bool matched = false;
        for (int32_t i = -1; i < int32_t(context->args->ignorePrefixesCount); ++i)
        {
            const char* prefix = "";
            if (-1 != i)
            {
                prefix = context->args->ignorePrefixes[i];
            }

            if (extras::startsWith(toMatch, prefix))
            {
                const char* strippedMatch = toMatch + std::strlen(prefix);
                if (omni::str::matchWildcards(
                        strippedMatch, context->args->matchWildcards, context->args->matchWildcardsCount))
                {
                    if (omni::str::matchWildcards(
                            strippedMatch, context->args->excludeWildcards, context->args->excludeWildcardsCount))
                    {
                        if (context->args->onExcluded)
                        {
                            context->args->onExcluded(canonical.c_str(), context->args->onExcludedContext);
                        }
                        return WalkAction::eContinue; // exclusion takes precedent. ignore the file and keep searching
                    }
                    else
                    {
                        matched = true;
                    }
                }
            }
        }

        if (matched)
        {
            if (context->args->onMatched)
            {
                context->args->onMatched(canonical.c_str(), context->args->onMatchedContext);
            }
        }
        else
        {
            if (context->args->onSkipped)
            {
                context->args->onSkipped(canonical.c_str(), context->args->onSkippedContext);
            }
        }
    }

    return WalkAction::eContinue;
}

} // namespace detail
#endif

inline bool findFiles(const FindFilesArgs& args)
{
    if (!args.searchPaths || !args.searchPathsCount)
    {
        CARB_LOG_ERROR("searchPath must be specified");
        return false;
    }

    if (!args.matchWildcards || !args.matchWildcardsCount)
    {
        CARB_LOG_ERROR("match wildcard must be specified");
        return false;
    }

    auto framework = getFramework();
    if (!framework)
    {
        CARB_LOG_ERROR("carb::Framework not active");
        return false;
    }

    IFileSystem* fs;
    if (args.fs)
    {
        fs = framework->verifyInterface<IFileSystem>(args.fs);
        if (!fs)
        {
            CARB_LOG_ERROR("incompatible carb::filesystem::IFileSystem");
            return false;
        }
    }
    else
    {
        fs = framework->tryAcquireInterface<IFileSystem>();
        if (!fs)
        {
            CARB_LOG_ERROR("unable to acquire carb::filesystem::IFileSystem");
            return false;
        }
    }

    detail::FindFilesContext userData = { &args, fs };
    for (uint32_t i = 0; i < args.searchPathsCount; ++i)
    {
        const char* dir = args.searchPaths[i];
        std::string dirWithEnv;
        if (args.flags & kFindFilesFlagReplaceEnvironmentVariables)
        {
            dirWithEnv = extras::replaceEnvironmentVariables(dir);
            dir = dirWithEnv.c_str();
        }

        extras::Path path{ dir };
        const char* fullPath = path.getStringBuffer();

        std::string tmp;
        if (!path.isAbsolute())
        {
            tmp = carb::fmt::format("{}/{}", fs->getAppDirectoryPath(), dir);
            fullPath = tmp.c_str();
        }

        if (args.onSearchPath)
        {
            args.onSearchPath(fullPath, args.onSearchPathContext);
        }

        if (args.flags & kFindFilesFlagRecursive)
        {
            fs->forEachDirectoryItemRecursive(fullPath, detail::onFile, const_cast<detail::FindFilesContext*>(&userData));
        }
        else
        {
            fs->forEachDirectoryItem(fullPath, detail::onFile, &userData);
        }
    }

    return true;
}

} // namespace filesystem
} // namespace carb
