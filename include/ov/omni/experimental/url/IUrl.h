// Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

#pragma once

#include "../../core/IObject.h"
#include "../../String.h"

namespace omni
{
namespace experimental
{
OMNI_DECLARE_INTERFACE(IUrl);

class IUrl_abi : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.IUrl")>
{
protected:
    /**
     * Clears this URL
     */
    virtual void clear_abi() noexcept = 0;

    /**
     * Return the string representation of this URL
     */
    virtual omni::string to_string_abi() noexcept = 0;

    /**
     * Return the string representation of this URL, but with valid UTF-8 characters
     * decoded. This will leave invalid UTF-8 byte sequences and certain ASCII characters
     * encoded; including control codes, and characters that are reserved by the URL
     * specification as sub-delimiters.
     */
    virtual omni::string to_string_utf8_abi() noexcept = 0;

    /**
     * Sets this URL from a string
     */
    virtual void from_string_abi(omni::string const& url_string) noexcept = 0;

    /**
     * Sets this URL from a posix file path
     * The scheme will be "file" and the path will be the normalized and encoded file path
     * Normalization includes removing redundant path segments such as "//", "/./" and
     * collapsing ".." segments if possible. For example, it will convert "a/b/../" to "a"
     */
    virtual void from_filepath_posix_abi(omni::string const& filepath) noexcept = 0;

    /**
     * Sets this URL from a windows file path
     * The scheme will be "file" and the path will be the normalized and encoded file path
     * Path normalization includes everything from "from_filepath_posix_abi" plus:
     * - The drive letter is made uppercase
     * - Path separators are converted from \\ to /
     * - UNC paths such as "\\\\server\\share\path" or "\\\\?\\C:\\path" are handled correctly
     */
    virtual void from_filepath_windows_abi(omni::string const& filepath) noexcept = 0;

    /**
     * Sets this URL from a file path based on the native OS.
     * This calls either from_filepath_posix_abi or from_filepath_windows_abi
     */
    virtual void from_filepath_native_abi(omni::string const& filepath) noexcept = 0;

    /**
     * Returns true if the URL has a scheme component.
     * "scheme" is the part before the first colon, for example "http" or "omniverse".
     * A URL without a scheme component can only be a relative reference.
     *
     * @see get_scheme()
     * @see set_scheme()
     */
    virtual bool has_scheme_abi() noexcept = 0;

    /**
     * Returns true if the URL has an authority component.
     * "authority" is the part between the // and /
     * For example "user@server:port"
     *
     * @see get_authority_encoded()
     * @see set_authority_encoded()
     * @see has_userinfo()
     * @see has_host()
     * @see has_port()
     */
    virtual bool has_authority_abi() noexcept = 0;

    /**
     * Returns true if the URL has a userinfo sub-component.
     * "userinfo" is the part of the authority before @
     *
     * @see get_userinfo()
     * @see set_userinfo()
     * @see has_authority()
     */
    virtual bool has_userinfo_abi() noexcept = 0;

    /**
     * Returns true if the URL has a host sub-component.
     * "host" is the part of the authority between @ and :
     *
     * @see get_host()
     * @see set_host()
     * @see has_authority()
     */
    virtual bool has_host_abi() noexcept = 0;

    /**
     * Returns true if the URL has a port sub-component.
     * "port" is the part of the authority after :
     *
     * @see get_port()
     * @see set_port()
     * @see has_authority()
     */
    virtual bool has_port_abi() noexcept = 0;

    /**
     * Returns true if the URL has a path component.
     * "path" is the part after _abi(and including) /
     * For example "/path/to/my/file.txt"
     *
     * @see get_path_encoded()
     * @see set_path_encoded()
     * @see set_path_decoded()
     */
    virtual bool has_path_abi() noexcept = 0;

    /**
     * Returns true if the URL has a query component.
     * "query" is the part after ? but before #
     *
     * @see get_query_encoded()
     * @see set_query_encoded()
     * @see set_query_decoded()
     */
    virtual bool has_query_abi() noexcept = 0;

    /**
     * Returns true if the URL has a fragment component.
     * "fragment" is the part after #
     *
     * @see get_fragment_encoded()
     * @see set_fragment_encoded()
     * @see set_fragment_decoded()
     */
    virtual bool has_fragment_abi() noexcept = 0;

    /**
     * Returns the scheme.
     * The scheme will always be fully decoded and in lower case.
     *
     * @see has_scheme()
     * @see set_scheme()
     */
    virtual omni::string get_scheme_abi() noexcept = 0;

    /**
     * Returns the authority, which may contain percent-encoded data
     * For example if the 'userinfo' contains : or @ it must be percent-encoded.
     *
     * @see set_authority_encoded()
     * @see get_userinfo()
     * @see get_host()
     * @see get_port()
     */
    virtual omni::string get_authority_encoded_abi() noexcept = 0;

    /**
     * Returns the userinfo, fully decoded.
     *
     * @see get_authority_encoded()
     * @see set_userinfo()
     * @see has_userinfo()
     */
    virtual omni::string get_userinfo_abi() noexcept = 0;

    /**
     * Returns the host, fully decoded.
     *
     * @see get_authority_encoded()
     * @see set_host()
     * @see has_host()
     */
    virtual omni::string get_host_abi() noexcept = 0;

    /**
     * Returns the port number
     *
     * @see get_authority_encoded()
     * @see set_port()
     * @see has_port()
     */
    virtual uint16_t get_port_abi() noexcept = 0;

    /**
     * Returns the percent-encoded path component.
     *
     * @see get_path_utf8()
     * @see set_path_encoded()
     * @see set_path_decoded()
     * @see has_path()
     */
    virtual omni::string get_path_encoded_abi() noexcept = 0;

    /**
     * Returns the path component with all printable ascii and valid UTF-8 characters decoded
     * Invalid UTF-8 and ASCII control codes will still be percent-encoded.
     * It's generally safe to print the result of this function on screen and in log files.
     *
     * @see get_path_encoded()
     * @see set_path_encoded()
     * @see set_path_decoded()
     * @see has_path()
     */
    virtual omni::string get_path_utf8_abi() noexcept = 0;

    /**
     * Returns the percent-encoded query component.
     *
     * @see get_query_encoded()
     * @see set_query_encoded()
     * @see set_query_decoded()
     * @see has_query()
     */
    virtual omni::string get_query_encoded_abi() noexcept = 0;

    /**
     * Returns the percent-encoded fragment component.
     *
     * @see get_fragment_encoded()
     * @see set_fragment_encoded()
     * @see set_fragment_decoded()
     * @see has_fragment()
     */
    virtual omni::string get_fragment_encoded_abi() noexcept = 0;

    /**
     * Sets the scheme.
     *
     * @see has_scheme()
     * @see get_scheme()
     */
    virtual void set_scheme_abi(omni::string const& scheme) noexcept = 0;

    /**
     * Sets the authority, which is expected to have all the sub-components percent-encoded.
     * If characters that _MUST_ be encoded are detected, they will be percent-encoded, however the percent sign itself
     * will _NOT_ be encoded.
     *
     * @see get_authority_encoded()
     * @see set_userinfo()
     * @see set_host()
     * @see set_port()
     */
    virtual void set_authority_encoded_abi(omni::string const& authority) noexcept = 0;

    /**
     * Sets the userinfo. This function expects the userinfo is not already percent-encoded.
     *
     * @see set_authority_encoded()
     * @see get_userinfo()
     * @see has_userinfo()
     */
    virtual void set_userinfo_abi(omni::string const& userinfo) noexcept = 0;

    /**
     * Sets the host. This function expects the host is not already percent-encoded.
     *
     * @see set_authority_encoded()
     * @see get_host()
     * @see has_host()
     */
    virtual void set_host_abi(omni::string const& host) noexcept = 0;

    /**
     * Sets the port number
     *
     * @see set_authority_encoded()
     * @see get_port()
     * @see has_port()
     */
    virtual void set_port_abi(uint16_t port) noexcept = 0;

    /**
     * Sets the path, which is already percent-encoded.
     * If characters that _MUST_ be encoded are detected, they will be percent-encoded, however the percent sign itself
     * will _NOT_ be encoded.
     *
     * @see get_path_encoded()
     * @see set_path_decoded()
     * @see has_path()
     */
    virtual void set_path_encoded_abi(omni::string const& path_encoded) noexcept = 0;

    /**
     * Sets the path, which is NOT already percent-encoded.
     * If characters that _MUST_ be encoded are detected, they will be percent-encoded, including the percent sign
     * itself
     *
     * @see get_path_encoded()
     * @see set_path_encoded()
     * @see has_path()
     */
    virtual void set_path_decoded_abi(omni::string const& path_decoded) noexcept = 0;

    /**
     * Sets the query, which is already percent-encoded.
     * If characters that _MUST_ be encoded are detected, they will be percent-encoded, however the percent sign itself
     * will _NOT_ be encoded.
     *
     * @see get_query_encoded()
     * @see set_query_decoded()
     * @see has_query()
     */
    virtual void set_query_encoded_abi(omni::string const& query_encoded) noexcept = 0;

    /**
     * Sets the query, which is NOT already percent-encoded.
     * If characters that _MUST_ be encoded are detected, they will be percent-encoded, including the percent sign
     * itself
     *
     * @see get_query_encoded()
     * @see set_query_encoded()
     * @see has_query()
     */
    virtual void set_query_decoded_abi(omni::string const& query_decoded) noexcept = 0;

    /**
     * Sets the fragment, which is already percent-encoded.
     * If characters that _MUST_ be encoded are detected, they will be percent-encoded, however the percent sign itself
     * will _NOT_ be encoded.
     *
     * @see get_fragment_encoded()
     * @see set_fragment_decoded()
     * @see has_fragment()
     */
    virtual void set_fragment_encoded_abi(omni::string const& fragment_encoded) noexcept = 0;

    /**
     * Sets the fragment, which is NOT already percent-encoded.
     * If characters that _MUST_ be encoded are detected, they will be percent-encoded, including the percent sign
     * itself
     *
     * @see get_fragment_encoded()
     * @see set_fragment_encoded()
     * @see has_fragment()
     */
    virtual void set_fragment_decoded_abi(omni::string const& fragment_decoded) noexcept = 0;

    /**
     * Create a new IUrl object that represents the shortest possible URL that makes @p other_url relative to this URL.
     *
     * Relative URLs are described in section 5.2 "Relative Resolution" of RFC-3986
     *
     * @param other_url URL to make a relative URL to.
     *
     * @return A new IUrl object that is the relative URL between this URL and @p other_url.
     */
    virtual IUrl* make_relative_abi(IUrl* other_url) noexcept = 0;

    /**
     * Creates a new IUrl object that is the result of resolving the provided @p relative_url with this URL as the base
     * URL.
     *
     * The algorithm for doing the combination is described in section 5.2 "Relative Resolution" of RFC-3986.
     *
     * @param relative_url URL to resolve with this URL as the base URL.
     *
     * @return A new IUrl object that is the result of resolving @p relative_url with this URL.
     */
    virtual IUrl* resolve_relative_abi(IUrl* relative_url) noexcept = 0;
};

} // namespace experimental
} // namespace omni

#include "IUrl.gen.h"
