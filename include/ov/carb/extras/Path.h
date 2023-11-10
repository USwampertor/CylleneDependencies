// Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Path interpretation utilities.
#pragma once

#include "../Defines.h"

#include "../logging/Log.h"
#include "../../omni/String.h"

#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>


namespace carb
{
namespace extras
{

// Forward declarations
class Path;
inline Path operator/(const Path& left, const Path& right);
inline Path operator+(const Path& left, const Path& right);
inline Path operator+(const Path& left, const char* right);
inline Path operator+(const Path& left, const std::string& right);
inline Path operator+(const Path& left, const omni::string& right);
inline Path operator+(const char* left, const Path& right);
inline Path operator+(const std::string& left, const Path& right);
inline Path operator+(const omni::string& left, const Path& right);

/**
 * Path objects are used for filesystem path manipulations.
 *
 * All paths are UTF-8 encoded using forward slash as path separator.
 *
 * Note: the class supports implicit casting to a "std::string" and explicit cast to
 * a "const char *" pointer
 */
class Path
{
public:
    //! A special value depending on the context, such as "all remaining characters".
    static constexpr size_t npos = std::string::npos;
    static_assert(npos == size_t(-1), "Invalid assumption");

    //--------------------------------------------------------------------------------------
    // Constructors/destructor and assignment operations

    /**
     * Default constructor.
     */
    Path() = default;

    /**
     * Constructs a Path from a counted char array containing a UTF-8 encoded string.
     *
     * The Path object converts all backslashes to forward slashes.
     *
     * @param path a pointer to the UTF-8 string data
     * @param pathLen the size of \p path in bytes
     */
    Path(const char* path, size_t pathLen)
    {
        if (path && pathLen)
        {
            m_pathString.assign(path, pathLen);
            _sanitizePath();
        }
    }

    /**
     * Constructs a Path from a NUL-terminated char buffer containing a UTF-8 encoded string.
     *
     * The Path object converts all backslashes to forward slashes.
     *
     * @param path a pointer to the UTF-8 string data
     */
    Path(const char* path)
    {
        if (path)
        {
            m_pathString = path;
        }
        _sanitizePath();
    }

    /**
     * Constructs a Path from a `std::string` containing a UTF-8 encoded string.
     *
     * The Path object converts all backslashes to forward slashes.
     *
     * @param path The source string
     */
    Path(std::string path) : m_pathString(std::move(path))
    {
        _sanitizePath();
    }

    /**
     * Constructs a Path from an \ref omni::string containing a UTF-8 encoded string.
     *
     * The Path object converts all backslashes to forward slashes.
     *
     * @param path The source string
     */
    Path(const omni::string& path) : m_pathString(path.c_str(), path.length())
    {
        _sanitizePath();
    }

    /**
     * Copy constructor. Copies a Path from another Path.
     *
     * @param other The other path to copy from
     */
    Path(const Path& other) : m_pathString(other.m_pathString)
    {
    }

    /**
     * Move constructor. Moves a Path from another Path, which is left in a valid but unspecified state.
     *
     * @param other The other path to move from
     */
    Path(Path&& other) noexcept : m_pathString(std::move(other.m_pathString))
    {
    }

    /**
     * Copy-assign operator. Copies another Path into *this.
     *
     * @param other The other path to copy from
     * @returns `*this`
     */
    Path& operator=(const Path& other)
    {
        m_pathString = other.m_pathString;
        return *this;
    }

    /**
     * Move-assign operator. Moves another Path into *this and leaves the other path in a valid but unspecified state.
     *
     * @param other The other path to move from
     * @returns `*this`
     */
    Path& operator=(Path&& other) noexcept
    {
        m_pathString = std::move(other.m_pathString);
        return *this;
    }

    /**
     * Destructor
     */
    ~Path() = default;

    //--------------------------------------------------------------------------------------
    // Getting a string representation of the internal data

    /**
     * Gets the string representation of the path.
     *
     * @returns a copy of the UTF-8 string representation of the internal data
     */
    std::string getString() const
    {
        return m_pathString;
    }

    /**
     * Implicit conversion operator to a string representation.
     *
     * @returns a copy of the UTF-8 string representation of the internal data
     */
    operator std::string() const
    {
        return m_pathString;
    }

    /**
     * Gets the string data owned by this Path.
     *
     * @warning the return value from this function should not be retained, and may be dangling after `*this` is
     * modified in any way.
     *
     * @return pointer to the start of the path data
     */
    const char* c_str() const noexcept
    {
        return m_pathString.c_str();
    }

    /**
     * Gets the string data owned by this Path.
     *
     * @warning the return value from this function should not be retained, and may be dangling after `*this` is
     * modified in any way.
     *
     * @return pointer to the start of the path data
     */
    const char* getStringBuffer() const noexcept
    {
        return m_pathString.c_str();
    }

    /**
     * Enables outputting this Path to a stream object.
     *
     * @param os the stream to output to
     * @param path the Path to output
     */
    CARB_NO_DOC(friend) // Sphinx 3.5.4 complains that this is an invalid C++ declaration, so ignore `friend` for docs.
    std::ostream& operator<<(std::ostream& os, const Path& path)
    {
        os << path.m_pathString;
        return os;
    }

    /**
     * Explicit conversion operator to get the string data owned by this Path.
     *
     * @warning the return value from this function should not be retained, and may be dangling after `*this` is
     * modified in any way.
     *
     * @return pointer to the start of the path data
     */
    explicit operator const char*() const noexcept
    {
        return getStringBuffer();
    }

    //--------------------------------------------------------------------------------------
    // Path operations
    // Compare operations

    /**
     * Equality comparison
     * @param other a Path to compare against `*this` for equality
     * @returns `true` if `*this` and \p other are equal; `false` otherwise
     */
    bool operator==(const Path& other) const noexcept
    {
        return m_pathString == other.m_pathString;
    }

    /**
     * Equality comparison
     * @param other a `std::string` to compare against `*this` for equality
     * @returns `true` if `*this` and \p other are equal; `false` otherwise
     */
    bool operator==(const std::string& other) const noexcept
    {
        return m_pathString == other;
    }

    /**
     * Equality comparison
     * @param other a NUL-terminated char buffer to compare against `*this` for equality
     * @returns `true` if `*this` and \p other are equal; `false` otherwise (or if \p other is `nullptr`)
     */
    bool operator==(const char* other) const noexcept
    {
        if (other == nullptr)
        {
            return false;
        }
        return m_pathString == other;
    }

    /**
     * Inequality comparison
     * @param other a Path to compare against `*this` for inequality
     * @returns `true` if `*this` and \p other are not equal; `false` otherwise
     */
    bool operator!=(const Path& other) const noexcept
    {
        return !(*this == other);
    }

    /**
     * Inequality comparison
     * @param other a `std::string` to compare against `*this` for inequality
     * @returns `true` if `*this` and \p other are not equal; `false` otherwise
     */
    bool operator!=(const std::string& other) const noexcept
    {
        return !(*this == other);
    }

    /**
     * Inequality comparison
     * @param other a NUL-terminated char buffer to compare against `*this` for inequality
     * @returns `true` if `*this` and \p other are not equal; `false` otherwise (or if \p other is `nullptr`)
     */
    bool operator!=(const char* other) const noexcept
    {
        return !(*this == other);
    }

    /**
     * Gets the length of the path
     *
     * @returns the length of the path string data in bytes
     */
    size_t getLength() const noexcept
    {
        return m_pathString.size();
    }

    /**
     * Gets the length of the path
     *
     * @returns the length of the path string data in bytes
     */
    size_t length() const noexcept
    {
        return m_pathString.size();
    }

    /**
     * Gets the length of the path
     *
     * @returns the length of the path string data in bytes
     */
    size_t size() const noexcept
    {
        return m_pathString.size();
    }

    /**
     * Clears current path
     *
     * @returns `*this`
     */
    Path& clear()
    {
        m_pathString.clear();
        return *this;
    }

    /**
     * Checks if the path is an empty string
     *
     * @returns `true` if the path contains at least one character; `false` otherwise
     */
    bool isEmpty() const noexcept
    {
        return m_pathString.empty();
    }

    /**
     * Checks if the path is an empty string
     *
     * @returns `true` if the path contains at least one character; `false` otherwise
     */
    bool empty() const noexcept
    {
        return m_pathString.empty();
    }

    /**
     * Returns the filename component of the path, or an empty path object if there is no filename.
     *
     * Example: given `C:/foo/bar.txt`, this function would return `bar.txt`.
     *
     * @return The path object representing the filename
     */
    Path getFilename() const
    {
        size_t offset = _getFilenameOffset();
        if (offset == npos)
            return {};

        return Path(Sanitized, m_pathString.substr(offset));
    }

    /**
     * Returns the extension of the filename component of the path, including period (.), or an empty path object.
     *
     * Example: given `C:/foo/bar.txt`, this function would return `.txt`.
     *
     * @return The path object representing the extension
     */
    Path getExtension() const
    {
        size_t ext = _getExtensionOffset();
        if (ext == npos)
            return {};

        return Path(Sanitized, m_pathString.substr(ext));
    }

    /**
     * Returns the path to the parent directory, or an empty path object if there is no parent.
     *
     * Example: given `C:/foo/bar.txt`, this function would return `C:/foo`.
     *
     * @return The path object representing the parent directory
     */
    Path getParent() const;

    /**
     * Returns the filename component of the path stripped of the extension, or an empty path object if there is no
     * filename.
     *
     * Example: given `C:/foo/bar.txt`, this function would return `bar`.
     *
     * @return The path object representing the stem
     */
    Path getStem() const
    {
        size_t offset = _getFilenameOffset();
        if (offset == npos)
            return {};

        size_t ext = _getExtensionOffset();
        CARB_ASSERT(ext == npos || ext >= offset);
        // If ext is npos, `ext - offset` results in the full filename since there is no extension
        return Path(Sanitized, m_pathString.substr(offset, ext - offset));
    }

    /**
     * Returns the root name in the path.
     *
     * Example: given `C:/foo/bar.txt`, this function would return `C:`.
     * Example: given `//server/share`, this function would return `//server`.
     *
     * @return The path object representing the root name
     */
    Path getRootName() const
    {
        size_t rootNameEnd = _getRootNameEndOffset();
        if (rootNameEnd == npos)
        {
            return {};
        }

        return Path(Sanitized, m_pathString.substr(0, rootNameEnd));
    }

    /**
     * Returns the relative part of the path (the part after optional root name and root directory).
     *
     * Example: given `C:/foo/bar.txt`, this function would return `foo/bar.txt`.
     * Example: given `//server/share/foo.txt`, this function would return `share/foo.txt`.
     *
     * @return The path objects representing the relative part of the path
     */
    Path getRelativePart() const
    {
        size_t relativePart = _getRelativePartOffset();
        if (relativePart == npos)
        {
            return {};
        }

        return Path(Sanitized, m_pathString.substr(relativePart));
    }

    /**
     * Returns the root directory if it's present in the path.
     *
     * Example: given `C:/foo/bar.txt`, this function would return `/`.
     * Example: given `foo/bar.txt`, this function would return `` (empty Path).
     *
     * @note This function will only ever return an empty path or a forward slash.
     *
     * @return The path object representing the root directory
     */
    Path getRootDirectory() const
    {
        constexpr static auto kForwardSlashChar_ = kForwardSlashChar; // CC-1110
        return hasRootDirectory() ? Path(Sanitized, std::string(&kForwardSlashChar_, 1)) : Path();
    }

    /**
     * Checks if the path has a root directory
     *
     * @return `true` if \ref getRootDirectory() would return `/`; `false` if \ref getRootDirectory() would return an
     * empty string.
     */
    bool hasRootDirectory() const noexcept
    {
        size_t rootDirectoryEnd = _getRootDirectoryEndOffset();
        if (rootDirectoryEnd == npos)
        {
            return false;
        }

        size_t rootNameEnd = _getRootNameEndOffset();
        if (rootNameEnd == rootDirectoryEnd)
        {
            return false;
        }

        return true;
    }

    /**
     * Returns the root of the path: the root name and root directory if they are present.
     *
     * Example: given `C:/foo/bar.txt`, this function would return `C:/`.
     *
     * @return The path object representing the root of the path
     */
    Path getRoot() const
    {
        size_t rootDirectoryEnd = _getRootDirectoryEndOffset();
        if (rootDirectoryEnd == npos)
        {
            return {};
        }

        return Path(Sanitized, m_pathString.substr(0, rootDirectoryEnd));
    }

    /**
     * A helper function to append another path after *this and return as a new object.
     *
     * @note Unlike \ref join(), this function concatenates two Path objects without a path separator between them.
     *
     * @param toAppend the Path to append to `*this`
     * @return a new Path object that has `*this` appended with \p toAppend.
     */
    Path concat(const Path& toAppend) const
    {
        if (isEmpty())
        {
            return toAppend;
        }
        if (toAppend.isEmpty())
        {
            return *this;
        }

        std::string total;
        total.reserve(m_pathString.length() + toAppend.m_pathString.length());

        total.append(m_pathString);
        total.append(toAppend.m_pathString);

        return Path(Sanitized, std::move(total));
    }

    /**
     * A helper function to append another path after *this with a separator if necessary and return as a new object.
     *
     * @param toJoin the Path object to append to `*this`
     * @returns a new Path object that has `*this` followed by a separator (if one is not already present at the end of
     * `*this` or the beginning of \p toJoin), followed by \p toJoin
     */
    Path join(const Path& toJoin) const;

    /**
     * Appends a path to *this with a separator if necessary.
     *
     * @note Unlike \ref join(), `*this` is modified by this function.
     *
     * @param path the Path to append to `*this`
     * @returns `*this`
     */
    Path& operator/=(const Path& path)
    {
        return *this = *this / path;
    }

    /**
     * Appends a path to *this without adding a separator.
     *
     * @note Unlike \ref concat(), `*this` is modified by this function.
     *
     * @param path The Path to append to `*this`
     * @returns `*this`
     */
    Path& operator+=(const Path& path)
    {
        return *this = *this + path;
    }

    /**
     * Replaces the current file extension with the given extension.
     *
     * @param newExtension the new extension to use
     *
     * @returns `*this`
     */
    Path& replaceExtension(const Path& newExtension);

    /**
     * Normalizes *this as an absolute path and returns as a new Path object.
     *
     * @warning This function does NOT make an absolute path using the CWD as a root. You need to use
     * \ref carb::filesystem::IFileSystem to get the current working directory and pass it to this function if desired.
     *
     * @note If `*this` is already an absolute path as determined by \ref isAbsolute(), \p root is ignored.
     *
     * @param root The root Path to prepend to `*this`
     * @return A new Path object representing the constructed absolute path
     */
    Path getAbsolute(const Path& root = Path()) const
    {
        if (isAbsolute() || root.isEmpty())
        {
            return this->getNormalized();
        }
        return root.join(*this).getNormalized();
    }

    /**
     * Returns the result of the normalization of the current path as a new path
     *
     * Example: given `C:/foo/./bar/../bat.txt`, this function would return `C:/foo/bat.txt`.
     *
     * @return A new Path object representing the normalized current path
     */
    Path getNormalized() const;

    /**
     * Normalizes current path in place
     *
     * @note Unlike \ref getNormalized(), `*this` is modified by this function.
     *
     * Example: given `C:/foo/./bar/../bat.txt`, this function would change `*this` to contain `C:/foo/bat.txt`.
     *
     * @returns `*this`
     */
    Path& normalize()
    {
        return *this = getNormalized();
    }

    /**
     * Checks if the current path is an absolute path.
     *
     * @returns `true` if the current path is an absolute path; `false` otherwise.
     */
    bool isAbsolute() const noexcept;

    /**
     * Checks if the current path is a relative path.
     *
     * Effectively, `!isAbsolute()`.
     *
     * @return `true` if the current path is a relative path; `false` otherwise.
     */
    bool isRelative() const
    {
        return !isAbsolute();
    }

    /**
     * Returns current path made relative to a given base as a new Path object.
     *
     * @note The paths are not normalized prior to the operation.
     *
     * @param base the base path
     *
     * @return an empty path if it's impossible to match roots (different root names, different states of being
     * relative/absolute with a base path, not having a root directory while the base has it), otherwise a non-empty
     * relative path
     */
    Path getRelative(const Path& base) const noexcept;

private:
    static constexpr char kDotChar = '.';
    static constexpr char kForwardSlashChar = '/';
    static constexpr char kColonChar = ':';

    struct Sanitized_t
    {
    };
    constexpr static Sanitized_t Sanitized{};

    Path(Sanitized_t, std::string s) : m_pathString(std::move(s))
    {
        // Pre-sanitized, so no need to do it again
    }

    enum class PathTokenType
    {
        Slash,
        RootName,
        Dot,
        DotDot,
        Name
    };

    // Helper function to parse next path token from `[bufferStart, bufferEnd)` (bufferEnd is an off-the-end pointer).
    // On success returns pointer immediately after the token data and returns token type in the
    // resultType. On failure returns nullptr and `resultType` is unchanged.
    // Note: it doesn't determine if a Name is a RootName. (RootName is added to enum for convenience)
    static const char* _getTokenEnd(const char* bufferBegin, const char* bufferEnd, PathTokenType& resultType)
    {
        if (bufferBegin == nullptr || bufferEnd == nullptr || bufferEnd <= bufferBegin)
        {
            return nullptr;
        }

        // Trying to find the next slash
        constexpr static auto kForwardSlashChar_ = kForwardSlashChar; // CC-1110
        const char* tokenEnd = std::find(bufferBegin, bufferEnd, kForwardSlashChar_);
        // If found a slash as the first symbol then return pointer to the data after it
        if (tokenEnd == bufferBegin)
        {
            resultType = PathTokenType::Slash;
            return tokenEnd + 1;
        }

        // If no slash found we consider all passed data as a single token
        if (!tokenEnd)
        {
            tokenEnd = bufferEnd;
        }

        const size_t tokenSize = tokenEnd - bufferBegin;
        if (tokenSize == 1 && *bufferBegin == kDotChar)
        {
            resultType = PathTokenType::Dot;
        }
        else if (tokenSize == 2 && bufferBegin[0] == kDotChar && bufferBegin[1] == kDotChar)
        {
            resultType = PathTokenType::DotDot;
        }
        else
        {
            resultType = PathTokenType::Name;
        }
        return tokenEnd;
    }

    /**
     * Helper function to find the pointer to the start of the filename
     */
    size_t _getFilenameOffset() const noexcept
    {
        if (isEmpty())
        {
            return npos;
        }

        // Find the last slash
        size_t offset = m_pathString.rfind(kForwardSlashChar);
        if (offset == npos)
        {
            // Not empty, so no slash means only filename
            return 0;
        }

        // If the slash is the end, no filename
        if ((offset + 1) == m_pathString.length())
        {
            return npos;
        }

        return offset + 1;
    }

    /**
     * Helper function to find the pointer to the start of the extension
     */
    size_t _getExtensionOffset() const noexcept
    {
        size_t filename = _getFilenameOffset();
        if (filename == npos)
        {
            return npos;
        }

        size_t dot = m_pathString.rfind(kDotChar);

        // No extension if:
        // - No dot in filename portion (dot not found or last dot proceeds filename)
        // - filename ends with a dot
        if (dot == npos || dot < filename || dot == (m_pathString.length() - 1))
        {
            return npos;
        }

        // If the only dot found starts the filename, we don't consider that an extension
        return dot == filename ? npos : dot;
    }

    /**
     * Helper function to find the pointer to the end of the root name (the pointer just after the end of the root name)
     */
    size_t _getRootNameEndOffset() const noexcept
    {
        if (isEmpty())
        {
            return npos;
        }

        if (m_pathString.length() < 2)
        {
            return 0;
        }

#if CARB_PLATFORM_WINDOWS
        // Check if the path starts with a drive letter and colon (e.g. "C:/...")
        if (m_pathString.at(1) == kColonChar && std::isalpha(m_pathString.at(0)))
        {
            return 2;
        }
#endif

        // Check if the path is a UNC path (e.g. "//server/location/...")
        // This simple check is two slashes and not a slash
        if (m_pathString.length() >= 3 && m_pathString.at(0) == kForwardSlashChar &&
            m_pathString.at(1) == kForwardSlashChar && m_pathString.at(2) != kForwardSlashChar)
        {
            // Find the next slash
            size_t offset = m_pathString.find(kForwardSlashChar, 3);
            return offset == npos ? m_pathString.length() : offset;
        }

        return 0;
    }

    /**
     * Helper function to get the pointer to the start of the relative part of the path
     */
    size_t _getRelativePartOffset(size_t rootNameEnd = npos) const noexcept
    {
        size_t offset = rootNameEnd != npos ? rootNameEnd : _getRootNameEndOffset();
        if (offset == npos)
            return npos;

        // Find the first non-slash character
        constexpr static auto kForwardSlashChar_ = kForwardSlashChar; // CC-1110
        return m_pathString.find_first_not_of(&kForwardSlashChar_, offset, 1);
    }

    /**
     * Helper function to find the pointer to the end of the root directory (the pointer just after the end of the root
     * directory)
     */
    size_t _getRootDirectoryEndOffset() const noexcept
    {
        size_t rootNameEnd = _getRootNameEndOffset();
        size_t relativePart = _getRelativePartOffset(rootNameEnd);

        if (relativePart != rootNameEnd)
        {
            if (rootNameEnd >= m_pathString.length())
            {
                return rootNameEnd;
            }
            return rootNameEnd + 1;
        }
        return rootNameEnd;
    }

    // Patching paths in the constructors (using external string data) if needed
    void _sanitizePath()
    {
#if CARB_PLATFORM_WINDOWS
        constexpr char kBackwardSlashChar = '\\';

        // changing the backward slashes for Windows to forward ones
        for (char& c : m_pathString)
        {
            if (c == kBackwardSlashChar)
            {
                c = kForwardSlashChar;
            }
        }
#endif
    }

    std::string m_pathString;
};

/**
 * Concatenation operator
 * @see Path::concat()
 * @param left a Path
 * @param right a Path
 * @returns `left.concat(right)`
 */
inline Path operator+(const Path& left, const Path& right)
{
    return left.concat(right);
}

/**
 * Concatenation operator
 * @see Path::concat()
 * @param left a Path
 * @param right a NUL-terminated char buffer representing a path
 * @returns `left.concat(right)`
 */
inline Path operator+(const Path& left, const char* right)
{
    return left.concat(right);
}

/**
 * Concatenation operator
 * @see Path::concat()
 * @param left a Path
 * @param right a `std::string` representing a path
 * @returns `left.concat(right)`
 */
inline Path operator+(const Path& left, const std::string& right)
{
    return left.concat(right);
}

/**
 * Concatenation operator
 * @see Path::concat()
 * @param left a Path
 * @param right an \ref omni::string representing a path
 * @returns `left.concat(right)`
 */
inline Path operator+(const Path& left, const omni::string& right)
{
    return left.concat(right);
}

/**
 * Concatenation operator
 * @see Path::concat()
 * @param left a NUL-terminated char buffer representing a path
 * @param right a Path
 * @returns `Path(left).concat(right)`
 */
inline Path operator+(const char* left, const Path& right)
{
    return Path(left).concat(right);
}

/**
 * Concatenation operator
 * @see Path::concat()
 * @param left a `std::string` representing a path
 * @param right a Path
 * @returns `Path(left).concat(right)`
 */
inline Path operator+(const std::string& left, const Path& right)
{
    return Path(left).concat(right);
}

/**
 * Concatenation operator
 * @see Path::concat()
 * @param left an \ref omni::string representing a path
 * @param right a Path
 * @returns `Path(left).concat(right)`
 */
inline Path operator+(const omni::string& left, const Path& right)
{
    return Path(left).concat(right);
}

/**
 * Join operator
 *
 * Performs a Path::join() operation on two Path paths.
 * @param left a Path
 * @param right a Path
 * @returns `left.join(right)`
 */
inline Path operator/(const Path& left, const Path& right)
{
    return left.join(right);
}

/**
 * Helper function to get a Path object representing parent directory for the provided string representation of a path.
 * @see Path::getParent()
 * @param path a `std::string` representation of a path
 * @returns `Path(path).getParent()`
 */
inline Path getPathParent(std::string path)
{
    return Path(std::move(path)).getParent();
}

/**
 * Helper function to get a Path object representing the extension part of the provided string representation of a path.
 * @see Path::getExtension()
 * @param path a `std::string` representation of a path
 * @returns `Path(path).getExtension()`
 */
inline Path getPathExtension(std::string path)
{
    return Path(std::move(path)).getExtension();
}

/**
 * Helper function to get a Path object representing the stem part of the provided string representation of a path.
 * @see Path::getStem()
 * @param path a `std::string` representation of a path
 * @returns `Path(path).getStem()`
 */
inline Path getPathStem(std::string path)
{
    return Path(std::move(path)).getStem();
}

/**
 * Helper function to calculate a relative path from a provided path and a base path.
 * @see Path::getRelative()
 * @param path a `std::string` representation of a path
 * @param base a `std::string` representation of the base path
 * @returns `Path(path).getRelative(base)`
 */
inline Path getPathRelative(std::string path, std::string base)
{
    return Path(std::move(path)).getRelative(Path(std::move(base)));
}

/**
 * Equality operator.
 * @param left a `std::string` representation of a Path
 * @param right a Path
 * @returns `true` if the paths are equal; `false` otherwise
 */
inline bool operator==(const std::string& left, const Path& right)
{
    return right == left;
}

/**
 * Equality operator.
 * @param left a NUL-terminated char buffer representation of a Path
 * @param right a Path
 * @returns `true` if the paths are equal; `false` otherwise
 */
inline bool operator==(const char* left, const Path& right)
{
    return right == left;
}

/**
 * Inequality operator.
 * @param left a `std::string` representation of a Path
 * @param right a Path
 * @returns `true` if the paths are not equal; `false` otherwise
 */
inline bool operator!=(const std::string& left, const Path& right)
{
    return right != left;
}

/**
 * Inequality operator.
 * @param left a NUL-terminated char buffer representation of a Path
 * @param right a Path
 * @returns `true` if the paths are not equal; `false` otherwise
 */
inline bool operator!=(const char* left, const Path& right)
{
    return right != left;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Implementations of large public functions
////////////////////////////////////////////////////////////////////////////////////////////

inline Path Path::getParent() const
{
    size_t parentPathEnd = _getFilenameOffset();
    if (parentPathEnd == npos)
        parentPathEnd = m_pathString.length();

    size_t slashesDataStart = 0;
    if (hasRootDirectory())
    {
        slashesDataStart += _getRootDirectoryEndOffset();
    }

    while (parentPathEnd > slashesDataStart && m_pathString.at(parentPathEnd - 1) == kForwardSlashChar)
    {
        --parentPathEnd;
    }

    if (parentPathEnd == 0)
    {
        return {};
    }

    return Path(Sanitized, m_pathString.substr(0, parentPathEnd));
}

inline Path Path::join(const Path& joinedPart) const
{
    if (isEmpty())
    {
        return joinedPart;
    }
    if (joinedPart.isEmpty())
    {
        return *this;
    }

    const bool needSeparator =
        (m_pathString.back() != kForwardSlashChar) && (joinedPart.m_pathString.front() != kForwardSlashChar);

    std::string joined;
    joined.reserve(m_pathString.length() + joinedPart.m_pathString.length() + (needSeparator ? 1 : 0));

    joined.assign(m_pathString);

    if (needSeparator)
        joined.push_back(kForwardSlashChar);

    joined.append(joinedPart.m_pathString);

    return Path(Sanitized, std::move(joined));
}

inline Path& Path::replaceExtension(const Path& newExtension)
{
    CARB_ASSERT(std::addressof(newExtension) != this);

    // Erase the current extension
    size_t ext = _getExtensionOffset();
    if (ext != npos)
    {
        m_pathString.erase(ext);
    }

    // If the new extension is empty, we're done
    if (newExtension.isEmpty())
    {
        return *this;
    }

    // Append a dot if there isn't one in the new extension
    if (newExtension.m_pathString.front() != kDotChar)
    {
        m_pathString.push_back(kDotChar);
    }

    // Append new extension
    m_pathString.append(newExtension.m_pathString);
    return *this;
}

inline Path Path::getNormalized() const
{
    if (isEmpty())
    {
        return {};
    }

    enum class NormalizePartType
    {
        Slash,
        RootName,
        RootSlash,
        Dot,
        DotDot,
        Name,
        Error
    };

    struct ParsedPathPartDescription
    {
        NormalizePartType type;
        const char* data;
        size_t size;

        ParsedPathPartDescription(const char* partData, size_t partSize, PathTokenType partType)
            : data(partData), size(partSize)
        {
            switch (partType)
            {
                case PathTokenType::Slash:
                    type = NormalizePartType::Slash;
                    break;
                case PathTokenType::RootName:
                    type = NormalizePartType::RootName;
                    break;
                case PathTokenType::Dot:
                    type = NormalizePartType::Dot;
                    break;
                case PathTokenType::DotDot:
                    type = NormalizePartType::DotDot;
                    break;
                case PathTokenType::Name:
                    type = NormalizePartType::Name;
                    break;

                default:
                    type = NormalizePartType::Error;
                    CARB_LOG_ERROR("Invalid internal token state while normalizing a path");
                    CARB_ASSERT(false);
                    break;
            }
        }

        ParsedPathPartDescription(const char* partData, size_t partSize, NormalizePartType partType)
            : type(partType), data(partData), size(partSize)
        {
        }
    };

    std::vector<ParsedPathPartDescription> resultPathTokens;

    size_t prevTokenEndOffset = _getRootDirectoryEndOffset();
    const char* prevTokenEnd = prevTokenEndOffset == npos ? nullptr : m_pathString.data() + prevTokenEndOffset;
    const char* pathDataStart = m_pathString.data();
    const size_t pathDataLength = getLength();
    if (prevTokenEnd && prevTokenEnd > pathDataStart)
    {
        // Adding the root name and the root directory as different elements
        const char* possibleSlashPos = prevTokenEnd - 1;
        if (*possibleSlashPos == kForwardSlashChar)
        {
            if (possibleSlashPos > pathDataStart)
            {
                resultPathTokens.emplace_back(
                    pathDataStart, static_cast<size_t>(possibleSlashPos - pathDataStart), PathTokenType::RootName);
            }
            constexpr static auto kForwardSlashChar_ = kForwardSlashChar; // CC-1110
            resultPathTokens.emplace_back(&kForwardSlashChar_, 1, NormalizePartType::RootSlash);
        }
        else
        {
            resultPathTokens.emplace_back(
                pathDataStart, static_cast<size_t>(prevTokenEnd - pathDataStart), PathTokenType::RootName);
        }
    }
    else
    {
        prevTokenEnd = pathDataStart;
    }

    bool alreadyNormalized = true;
    const char* bufferEnd = pathDataStart + pathDataLength;
    PathTokenType curTokenType = PathTokenType::Name;
    for (const char* curTokenEnd = _getTokenEnd(prevTokenEnd, bufferEnd, curTokenType); curTokenEnd != nullptr;
         prevTokenEnd = curTokenEnd, curTokenEnd = _getTokenEnd(prevTokenEnd, bufferEnd, curTokenType))
    {
        switch (curTokenType)
        {
            case PathTokenType::Slash:
                if (resultPathTokens.empty() || resultPathTokens.back().type == NormalizePartType::Slash ||
                    resultPathTokens.back().type == NormalizePartType::RootSlash)
                {
                    // Skip if we already have a slash at the end
                    alreadyNormalized = false;
                    continue;
                }
                break;

            case PathTokenType::Dot:
                // Just skip it
                alreadyNormalized = false;
                continue;

            case PathTokenType::DotDot:
                if (resultPathTokens.empty())
                {
                    break;
                }
                // Check if the previous element is a part of the root name (even without a slash) and skip dot-dot in
                // such case
                if (resultPathTokens.back().type == NormalizePartType::RootName ||
                    resultPathTokens.back().type == NormalizePartType::RootSlash)
                {
                    alreadyNormalized = false;
                    continue;
                }

                if (resultPathTokens.size() > 1)
                {
                    CARB_ASSERT(resultPathTokens.back().type == NormalizePartType::Slash);

                    const NormalizePartType tokenTypeBeforeSlash = resultPathTokens[resultPathTokens.size() - 2].type;

                    // Remove <name>/<dot-dot> pattern
                    if (tokenTypeBeforeSlash == NormalizePartType::Name)
                    {
                        resultPathTokens.pop_back(); // remove the last slash
                        resultPathTokens.pop_back(); // remove the last named token
                        alreadyNormalized = false;
                        continue; // and we skip the addition of the dot-dot
                    }
                }

                break;

            case PathTokenType::Name:
                // No special processing needed
                break;

            default:
                CARB_LOG_ERROR("Invalid internal state while normalizing the path {%s}", getStringBuffer());
                CARB_ASSERT(false);
                alreadyNormalized = false;
                continue;
        }

        resultPathTokens.emplace_back(prevTokenEnd, static_cast<size_t>(curTokenEnd - prevTokenEnd), curTokenType);
    }

    if (resultPathTokens.empty())
    {
        constexpr static auto kDotChar_ = kDotChar; // CC-1110
        return Path(Sanitized, std::string(&kDotChar_, 1));
    }
    else if (resultPathTokens.back().type == NormalizePartType::Slash && resultPathTokens.size() > 1)
    {
        // Removing the trailing slash for special cases like "./" and "../"
        const size_t indexOfTokenBeforeSlash = resultPathTokens.size() - 2;
        const NormalizePartType typeOfTokenBeforeSlash = resultPathTokens[indexOfTokenBeforeSlash].type;

        if (typeOfTokenBeforeSlash == NormalizePartType::Dot || typeOfTokenBeforeSlash == NormalizePartType::DotDot)
        {
            resultPathTokens.pop_back();
            alreadyNormalized = false;
        }
    }

    if (alreadyNormalized)
    {
        return *this;
    }

    size_t totalSize = 0;
    for (auto& token : resultPathTokens)
        totalSize += token.size;

    std::string joined;
    joined.reserve(totalSize);

    for (auto& token : resultPathTokens)
        joined.append(token.data, token.size);

    return Path(Sanitized, std::move(joined));
}

inline bool Path::isAbsolute() const noexcept
{
#if CARB_POSIX
    return !isEmpty() && m_pathString[0] == kForwardSlashChar;
#elif CARB_PLATFORM_WINDOWS
    // Drive root (D:/abc) case. This is the only position where : is allowed on windows. Checking for separator is
    // important, because D:temp.txt is a relative path on windows.
    const char* pathDataStart = m_pathString.data();
    const size_t pathDataLength = getLength();
    if (pathDataLength > 2 && pathDataStart[1] == kColonChar && pathDataStart[2] == kForwardSlashChar)
        return true;
    // Drive letter (D:) case
    if (pathDataLength == 2 && pathDataStart[1] == kColonChar)
        return true;

    // extended drive letter path (ie: prefixed with "//./D:").
    if (pathDataLength > 4 && pathDataStart[0] == kForwardSlashChar && pathDataStart[1] == kForwardSlashChar &&
        pathDataStart[2] == kDotChar && pathDataStart[3] == kForwardSlashChar)
    {
        // at least a drive name was specified.
        if (pathDataLength > 6 && pathDataStart[5] == kColonChar)
        {
            // a drive plus an absolute path was specified (ie: "//./d:/abc") => succeed.
            if (pathDataStart[6] == kForwardSlashChar)
                return true;

            // a drive and relative path was specified (ie: "//./d:temp.txt") => fail.  We need to
            //   specifically fail here because this path would also get picked up by the generic
            //   special path check below and report success erroneously.
            else
                return false;
        }

        // requesting the full drive volume (ie: "//./d:") => report absolute to match behavior
        //   in the "d:" case above.
        if (pathDataLength == 6 && pathDataStart[5] == kColonChar)
            return true;
    }

    // check for special paths.  This includes all windows paths that begin with "\\" (converted
    // to Unix path separators for our purposes).  This class of paths includes extended path
    // names (ie: prefixed with "\\?\"), device names (ie: prefixed with "\\.\"), physical drive
    // paths (ie: prefixed with "\\.\PhysicalDrive<n>"), removable media access (ie: "\\.\X:")
    // COM ports (ie: "\\.\COM*"), and UNC paths (ie: prefixed with "\\servername\sharename\").
    //
    // Note that it is not necessarily sufficient to get absolute vs relative based solely on
    // the "//" prefix here without needing to dig further into the specific name used and what
    // it actually represents.  For now, we'll just assume that device, drive, volume, and
    // port names will not be used here and treat it as a UNC path.  Since all extended paths
    // and UNC paths must always be absolute, this should hold up at least for those.  If a
    // path for a drive, volume, or device is actually passed in here, it will still be treated
    // as though it were an absolute path.  The results of using such a path further may be
    // undefined however.
    if (pathDataLength > 2 && pathDataStart[0] == kForwardSlashChar && pathDataStart[1] == kForwardSlashChar &&
        pathDataStart[2] != kForwardSlashChar)
        return true;
    return false;
#else
    CARB_UNSUPPORTED_PLATFORM();
#endif
}

inline Path Path::getRelative(const Path& base) const noexcept
{
    // checking if the operation is possible
    if (isAbsolute() != base.isAbsolute() || (!hasRootDirectory() && base.hasRootDirectory()) ||
        getRootName() != base.getRootName())
    {
        return {};
    }

    PathTokenType curPathTokenType = PathTokenType::RootName;
    size_t curPathTokenEndOffset = _getRootDirectoryEndOffset();
    const char* curPathTokenEnd = curPathTokenEndOffset == npos ? nullptr : m_pathString.data() + curPathTokenEndOffset;
    const char* curPathTokenStart = curPathTokenEnd;
    const char* curPathEnd = m_pathString.data() + m_pathString.length();

    PathTokenType basePathTokenType = PathTokenType::RootName;
    size_t basePathTokenEndOffset = base._getRootDirectoryEndOffset();
    const char* basePathTokenEnd =
        basePathTokenEndOffset == npos ? nullptr : base.m_pathString.data() + basePathTokenEndOffset;
    const char* basePathEnd = base.m_pathString.data() + base.m_pathString.length();

    // finding the first mismatch
    for (;;)
    {
        curPathTokenStart = curPathTokenEnd;
        curPathTokenEnd = _getTokenEnd(curPathTokenEnd, curPathEnd, curPathTokenType);

        const char* baseTokenStart = basePathTokenEnd;
        basePathTokenEnd = _getTokenEnd(basePathTokenEnd, basePathEnd, basePathTokenType);

        if (!curPathTokenEnd || !basePathTokenEnd)
        {
            // Checking if both are null
            if (curPathTokenEnd == basePathTokenEnd)
            {
                constexpr static auto kDotChar_ = kDotChar; // CC-1110
                return Path(Sanitized, std::string(&kDotChar_, 1));
            }
            break;
        }

        if (curPathTokenType != basePathTokenType ||
            !std::equal(curPathTokenStart, curPathTokenEnd, baseTokenStart, basePathTokenEnd))
        {
            break;
        }
    }
    int requiredDotDotCount = 0;
    while (basePathTokenEnd)
    {
        if (basePathTokenType == PathTokenType::DotDot)
        {
            --requiredDotDotCount;
        }
        else if (basePathTokenType == PathTokenType::Name)
        {
            ++requiredDotDotCount;
        }

        basePathTokenEnd = _getTokenEnd(basePathTokenEnd, basePathEnd, basePathTokenType);
    }

    if (requiredDotDotCount < 0)
    {
        return {};
    }

    if (requiredDotDotCount == 0 && !curPathTokenEnd)
    {
        constexpr static auto kDotChar_ = kDotChar; // CC-1110
        return Path(Sanitized, std::string(&kDotChar_, 1));
    }

    const size_t leftoverCurPathSymbols = curPathTokenEnd != nullptr ? curPathEnd - curPathTokenStart : 0;
    constexpr static char kDotDotString[] = "..";
    constexpr static size_t kDotDotStrlen = CARB_COUNTOF(kDotDotString) - 1;
    const size_t requiredResultSize = ((1 /* '/' */) + (kDotDotStrlen)) * requiredDotDotCount + leftoverCurPathSymbols;

    Path result;
    result.m_pathString.reserve(requiredResultSize);

    if (requiredDotDotCount > 0)
    {
        result.m_pathString.append(kDotDotString, kDotDotStrlen);
        --requiredDotDotCount;

        for (int i = 0; i < requiredDotDotCount; ++i)
        {
            result.m_pathString.push_back(kForwardSlashChar);
            result.m_pathString.append(kDotDotString, kDotDotStrlen);
        }
    }

    bool needsSeparator = !result.m_pathString.empty();
    while (curPathTokenEnd)
    {
        if (curPathTokenType != PathTokenType::Slash)
        {
            if (CARB_LIKELY(needsSeparator))
            {
                result.m_pathString += kForwardSlashChar;
            }
            else
            {
                needsSeparator = true;
            }
            result.m_pathString.append(curPathTokenStart, curPathTokenEnd - curPathTokenStart);
        }

        curPathTokenStart = curPathTokenEnd;
        curPathTokenEnd = _getTokenEnd(curPathTokenEnd, curPathEnd, curPathTokenType);
    }

    return result;
}
} // namespace extras
} // namespace carb
