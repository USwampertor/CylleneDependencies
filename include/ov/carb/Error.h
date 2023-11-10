// Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! \file
//!
//! \brief Core components for error eandling.
//! These functions are the foundation for language-specific bindings and should not be used directly. See
//! \c omni/Error.h for user-friendly C++ interfaces.
#pragma once

#include "Defines.h"
#include "Interface.h"
#include "Types.h"
#include "Version.h"
#include "detail/DeferredLoad.h"

#include "../omni/core/Result.h"

#include <cinttypes>

namespace carb
{

using omni::core::Result;

// bring all the kResult___ values into carb namespace
#define CARB_RESULT_USE_OMNI_RESULT_GEN(symbol_, ...) using omni::core::kResult##symbol_;
OMNI_RESULT_CODE_LIST(CARB_RESULT_USE_OMNI_RESULT_GEN)

//! Opaque type which stores an error. Use \c ErrorApi::getErrorInfo to extract information from it.
//!
//! \warning
//! Error pointers are intended to be owned by a single thread. They can be safely sent between threads, but it is not
//! safe to access their contents from multiple threads simultaneously.
class Error;

//! The low-level API for interacting with the Carbonite error handling system. At the core, this system maintains a
//! thread-specific error code and optional error message.
//!
//! The state of the error objects are maintained through `libcarb.so`/`carb.dll`, the API is accessed through
//! \c carbGetErrorApi or \c ErrorApi::instance. Different versions of the API interact with the same global state. A
//! call to \c ErrorApi::viewCurrentError with version 1.0 will still be able to read a \c ErrorApi::setError from a
//! version 4.0 API (version 4.0 does not exist at this time).
struct ErrorApi
{
    CARB_PLUGIN_INTERFACE("carb::ErrorApi", 1, 0);

    //! Get the singleton instance of this API.
    //!
    //! The implementation of this is backed by `libcarb.so`/`carb.dll`, so it is a singleton instance for all loaded
    //! modules. This function is backed by \c carbGetErrorApi with the \c version argument provided with the value of
    //! the API version the calling module was built against.
    static ErrorApi const& instance() noexcept;

    //! Get a view of this thread's current error, if it is set.
    //!
    //! \warning
    //! The caller does not own the returned error. Any information pointed to by the returned value can be altered or
    //! cleared by a function calling any of the \c setError calls on this thread. Errors accessed are meant to be
    //! acted upon immediately. If you wish to preserve information, copy the pieces you wish to save or store the whole
    //! thing with \c errorClone or \c takeCurrentError.
    //!
    //! \param[out] code If not \c nullptr and there is a current error, then \c *code will be set to the error code of
    //!             the current error. Use this to save an extra call to \c getErrorInfo when you are going to act on
    //!             the code value. If there is no current error, then this will be set to \c kResultSuccess.
    //!
    //! \returns The current error, if it was set. If there is no error, this returns \c nullptr.
    Error const*(CARB_ABI* viewCurrentError)(Result* code);

    //! Get this thread's current error as an owned object, if it is set. After this call, this thread's current error
    //! will be cleared.
    //!
    //! It is the caller's responsibility to clean up the returned value, unless it is \c nullptr.
    //!
    //! This function is equivalent to cloning the current error, then resetting it:
    //!
    //! \code
    //! ErrorApi const& api = ErrorApi::instance();
    //! Result code{}; // <- optimization only -- see viewCurrentError
    //! Error* my_err = api.errorClone(api.viewCurrentError(&code));
    //! api.setErrorTo(nullptr);
    //! \endcode
    //!
    //! \param[out] code If not \c nullptr and there is a current error, then \c *code will be set to the error code of
    //!             the current error. Use this to save an extra call to \c getErrorInfo when you are going to act on
    //!             the code value. If there is no current error, then this will be set to \c kResultSuccess.
    //! \returns An owned version of the current error, if it was set. If there is no error, this returns \c nullptr.
    Error*(CARB_ABI* takeCurrentError)(Result* code);

    //! Set this thread's current error to a pre-formatted string.
    //!
    //! In the case of any error of a call to this function, the current error will always be set with \a code. However,
    //! the associated message of the error will be the default for that code.
    //!
    //! \param code The code for the created error, which callers can potentially act on.
    //! \param message The associated message containing additional details about the error. If this is \c nullptr, the
    //!        default message for the \a code is used.
    //! \param message_size The number of bytes \a message is. If \a message is \c nullptr, this must be \c 0.
    //!
    //! \retval kResultSuccess when the error is set without issue.
    //! \retval kResultInvalidArgument if the \a message is \c nullptr and \a message_size is not 0 or if \a message is
    //!         not \c nullptr and the \a message_size is 0. The current error will still be set to \a code, but the
    //!         message will be the default for that code.
    //! \retval kResultOutOfMemory if the message is too large to fit in the string buffer, but the call to allocate
    //!         memory failed. The current error is still set to \a code, but the error message is saved as the default
    //!         for that code.
    //! \retval kResultTooMuchData If the provided message is too large to fit in any error message buffer (the current
    //!         maximum is 64 KiB). The current error will be set with a truncated version of the provided message.
    Result(CARB_ABI* setError)(Result code, char const* message, std::size_t message_size);

    //! Set this thread's current error to the exact \a error or clear the current error if \a error is \c nullptr.
    //! Responsibility for this instance is taken by the error system, so you should not call \c errorRelease on it.
    //!
    //! \param error The error to set. If this is \c nullptr, this clears the current error.
    //!
    //! \retval kResultSuccess when the error is set without issue.
    //! \retval kResultInvalidOperation if `error == viewCurrentError()`. Restoring a previously-saved error should
    //!         occur through a \c errorClone call, so doing this represents a bug in the code (note that you must have
    //!         used \c const_cast or equivalent to get this to compile). In this case, no action is performed, as the
    //!         current error is already set to the error.
    Result(CARB_ABI* setErrorTo)(Error* error);

    //! Set this thread's current error to a pre-allocated error indicating the system is out of memory. It will always
    //! succeed.
    void(CARB_ABI* setErrorOom)();

    //! Release the \a error which was previously `errorClone`d or `errorCreate`d.
    //!
    //! \retval kResultSuccess when the error was released.
    //! \retval kResultInvalidOperation when `error == viewCurrentError()`. The current error does not need to be
    //!         released.
    Result(CARB_ABI* errorRelease)(Error* error);

    //! Create a copy of the \a source error.
    //!
    //! \param source The error to clone from. If this is \c nullptr, \c nullptr will be returned without error.
    //!
    //! \returns The cloned error. In case of error, \c nullptr is returned and the current error is set with a message
    //!          containing additional details.
    Error*(CARB_ABI* errorClone)(Error const* source);

    //! Create an error message with the \a code and pre-formatted \a message string without setting the current error.
    //! The parameters operate similarly to \c setError, but are more strict. Where \c setError would return a non-OK
    //! code but still set the current error, this function would return \c nullptr and set the current error to the
    //! failure.
    //!
    //! \param code The code for the created error, which callers can potentially act on.
    //! \param message The associated message containing additional details about the error. If this is \c nullptr, the
    //!        default message for the \a code is used.
    //! \param message_size The number of bytes \a message is.
    //!
    //! \returns The created error on success. On failure, \c nullptr is returned and the current error is set with the
    //!          reason for the failure. The code values are the same as \c setError.
    Error*(CARB_ABI* errorCreate)(Result code, const char* message, std::size_t message_size);

    //! Extract the information associated with the \a error message. The output parameters are views of the properties,
    //! so they are only valid as long as \a error is valid.
    //!
    //! \param error The source to extract data from. This can not be \c nullptr.
    //! \param[out] code If not \c nullptr, \c *code will be set to the code of this error.
    //! \param[out] message If not \c nullptr, \c *message will point to the error's detail message.
    //! \param[out] message_size If not \c nullptr, \c *message_size will be set to the size of the error's detail
    //!             message. Note that \c *message is always null-terminated, but this is useful for optimization when
    //!             copying.
    //!
    //! \retval kResultSuccess when the operation was successfully performed.
    //! \retval kResultInvalidArgument if \a error is \c nullptr. The current error is not set in this case (since the
    //!         \a error can be \c current_error, we do not want to clear it for you.
    Result(CARB_ABI* getErrorInfo)(Error const* error, Result* code, char const** message, std::size_t* message_size);

    //! Get the name and default message for the given \a code. The \a name a symbol-like name in snake case like
    //! `"invalid_argument"`. The \a message is the default message for the \a code as a sentence fragment like
    //! `"invalid argument"`.
    //!
    //! Note that all of the output parameters are allowed to be \c nullptr. In this case, \c kResultSuccess is still
    //! returned. This can be useful for checking if a given \a code exists at all.
    //!
    //! \param code The error code to look up.
    //! \param[out] name A pointer to a place to put the name of the code. If this is \c nullptr, it will not be set.
    //! \param[out] name_size A pointer to place to put the size of \a name (in UTF-8 code units). Since \a name is
    //!             null-terminated, this is not strictly needed, but can save you a \c strlen call. If this is
    //!             \c nullptr, it will not be set.
    //! \param[out] message A pointer to the place to put the default message for this code. If this is \c nullptr, it
    //!             will not be set.
    //! \param[out] message_size A pointer to the place to put the size of \a message (in UTF-8 code units). Since
    //!             \a message is null-terminated, this is not strictly needed, but can save you a \c strlen call. If
    //!             this is \c nullptr, it will not be set.
    //!
    //! \retval kResultSuccess when the operation was successfully performed.
    //! \retval kResultNotFound when \a code is not in the known list of error codes. The current error is not set.
    Result(CARB_ABI* getCodeDescription)(
        Result code, char const** name, std::size_t* name_size, char const** message, std::size_t* message_size);
};

} // namespace carb

//! Get the instance of the error-handling API.
//!
//! \param version The requested version of the error-handling API to return; this value will be set to the maximum
//!        supported version.
//! \returns On success, this returns a pointer to the error API which is compatible with the provided \a version.
#if CARB_REQUIRE_LINKED
CARB_DYNAMICLINK carb::ErrorApi const* carbGetErrorApi(carb::Version* version);
#else
CARB_DYNAMICLINK carb::ErrorApi const* carbGetErrorApi(carb::Version* version) CARB_ATTRIBUTE(weak);
#endif

namespace carb
{
namespace detail
{

//! \fn getCarbErrorApiFunc
//! Loads the function which loads the \c ErrorApi.
CARB_DETAIL_DEFINE_DEFERRED_LOAD(getCarbErrorApiFunc, carbGetErrorApi, (carb::ErrorApi const* (*)(carb::Version*)));

} // namespace detail

inline ErrorApi const& ErrorApi::instance() noexcept
{
    static ErrorApi const* const papi = []() -> ErrorApi const* {
        const Version expected_version = ErrorApi::getInterfaceDesc().version;
        Version found_version = expected_version;

        auto p = detail::getCarbErrorApiFunc()(&found_version);
        CARB_FATAL_UNLESS(p != nullptr,
                          "Failed to load Error API for version this module was compiled against. This module was "
                          "compiled with Error API %" PRIu32 ".%" PRIu32
                          ", but the maximum-supported version of the "
                          "API in the linked %s is %" PRIu32 ".%" PRIu32,
                          expected_version.major, expected_version.minor,
                          CARB_PLATFORM_WINDOWS ? "carb.dll" : "libcarb.so", found_version.major, found_version.minor);
        return p;
    }();
    return *papi;
}

} // namespace carb
