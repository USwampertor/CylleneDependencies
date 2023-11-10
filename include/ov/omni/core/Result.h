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
//! \brief Result codes form the basics of error handling.
#pragma once

#include "../../carb/Defines.h"
#include "OmniAttr.h"

#include <cstdint>

namespace omni
{
namespace core
{

//! \{
//! Error code for the result of an operation.
//!
//! The numeric encoding for values follows Microsoft's
//! <a
//! href="https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-erref/0642cb2f-2075-4469-918c-4441e69c548a">
//! HRESULT</a> scheme. Many values are direct copies of those from the Windows API, such as \c kResultNotImplemented.
//! Codes which are NVIDIA-provided, will have the mask \c 0xa4310000. This comes from setting the "customer bit" (bit
//! at most-significant index 2) and having a "facility" (bits from index 5-15) of \c 0b10000110001 aka \c 0x431 (which
//! is \c "NVDA" in Morse Code).
using Result OMNI_ATTR("constant, prefix=kResult") = std::int32_t;

//! Returns \c true if the given \ref omni::core::Result is not a failure code.
//!
//! `true` will be returned not only if the given result is \ref omni::core::kResultSuccess, but any other \ref
//! omni::core::Result that is not a failure code (such as warning \ref omni::core::Result codes).
#define OMNI_SUCCEEDED(x_) ((x_) >= 0)

//! Returns `true` if the given @ref omni::core::Result is a failure code.
#define OMNI_FAILED(x_) ((x_) < 0)

//! If the given \ref omni::core::Result is a failure code, calls `return result` to exit the current function.
#define OMNI_RETURN_IF_FAILED(x_)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        auto result = (x_);                                                                                            \
        if (OMNI_FAILED(result))                                                                                       \
        {                                                                                                              \
            return result;                                                                                             \
        }                                                                                                              \
    } while (0)

//! Operation successful. No error occurred.
constexpr Result kResultSuccess = 0;

//! The feature or method was not implemented. It might be at some point in the future.
//!
//! * POSIX: \c ENOSYS
//! * Windows: \c E_NOTIMPL
constexpr Result kResultNotImplemented = 0x80004001;

//! The operation was aborted.
//!
//! * Windows: \c E_ABORT
constexpr Result kResultOperationAborted = 0x80004004;

//! The operation failed.
constexpr Result kResultFail = 0x80004005;

//! The item was not found.
constexpr Result kResultNotFound = 0x80070002;

//! Access has been denied for this operation.
//!
//! * POSIX: \c EACCES
//! * Windows: \c E_ACCESSDENIED
constexpr Result kResultAccessDenied = 0x80070005;

//! A system is out of memory. This does not necesarily mean resident memory has been exhausted (although it can),
//! as this code can be used to special conditions such as exhausting graphics memory or running out of a specific
//! memory pool. It can also indicate that an allocation would have been too big and failed ahead of time.
//!
//! * POSIX: \c ENOMEM
//! * Windows: \c E_OUTOFMEMORY
constexpr Result kResultOutOfMemory = 0x8007000E;

//! The operation is not supported.
constexpr Result kResultNotSupported = 0x80070032;

//! One or more of the arguments passed to a given function was invalid.
//!
//! * POSIX: \c EINVAL
//! * Windows: \c E_INVALIDARG
constexpr Result kResultInvalidArgument = 0x80070057;

//! The system is in an invalid state to perform the operation. This is distinct from \c kResultInvalidOperation in that
//! it covers situations like "system is not yet started" or "file is closed."
constexpr Result kResultInvalidState = 0x80070004;

//! Version check failure.
constexpr Result kResultVersionCheckFailure = 0x80070283;

//! Failed to parse the version.
constexpr Result kResultVersionParseError = 0x80070309;

//! Insufficient buffer.
constexpr Result kResultInsufficientBuffer = 0x8007007A;

//! Try the operation again. This is typically emitted in situations where an operation would require blocking, but the
//! system is configured to be non-blocking. For example, attempting to read from a TCP socket when no data has been
//! received would return \c kResultTryAgain.
//!
//! * POSIX: \c EAGAIN, \c EWOULDBLOCK
//! * Windows: \c WMI_TRY_AGAIN
constexpr Result kResultTryAgain = 0x8007106B; //!< Try the operation again.

//! An operation was interrupted. An "interruption" happens in cases where the operation did not complete successfully
//! due to an outside system (such as a timer) interrupting it. For example, a function `Result wait_for(duration d)`
//! might give \c kResultSuccess when function returns because the duration expired and \c kResultInterrupted if the
//! system is shutting down.
//!
//! * POSIX: \c EINTR
//! * Windows: \c WSAEINTR
constexpr Result kResultInterrupted = 0xa4310001;

//! Interface not implemented.
constexpr Result kResultNoInterface = 0x80004002;

//! Pointer is null.
//!
//! * POSIX: covered by \c EINVAL
constexpr Result kResultNullPointer = 0x80004003;

//! Object already exists.
//!
//! * POSIX: \c EEXIST or \c EBUSY
constexpr Result kResultAlreadyExists = 0x80030050;

//! The operation was not valid for the target. For example, attempting to perform a write operation on a read-only file
//! would result in this error.
//!
//! * POSIX: \c EPERM
constexpr Result kResultInvalidOperation = 0x800710DD;

//! No more items to return. This is meant for things like reader queues when they have run out of data and will never
//! have more data. For cases where something like an async queue being temporarily empty, use \c kResultTryAgain.
constexpr Result kResultNoMoreItems = 0x8009002A;

//! Invalid index.
//!
//! * POSIX: covered by \c EINVAL or \c ENOENT, depending on the situation
constexpr Result kResultInvalidIndex = 0x80091008;

//! Not enough data.
constexpr Result kResultNotEnoughData = 0x80290101;

//! Too much data.
constexpr Result kResultTooMuchData = 0x80290102;

//! Invalid data type. This is used in cases where a specific type of data is requested, but that is not the data which
//! the receiver has.
constexpr Result kResultInvalidDataType = 0x8031000B;

//! Invalid data size. This arises when the correct type of data is requested, but the requester believes the data size
//! is different from the receiver. The cause of this is typically a version mismatch.
constexpr Result kResultInvalidDataSize = 0x8031000C;

//! \}

//! \cond DEV
// clang-format off

//! The list of all result codes as a higher-order macro. The provided \c item_ should accept three parameters:
//!
//! * \c symbol -- the PascalCase version of the symbol; e.g.: \c AlreadyExists. Note that this includes neither the
//!   \c kResult prefix nor the \c omni::core namespace qualifier, so you need to paste those yourself if desired.
//! * \c snake_symbol -- the snake_case version of the symbol; e.g.: \c try_again
//! * \c message -- a string literal of the associated message for the error; e.g.: `"access denied"`
#define OMNI_RESULT_CODE_LIST(item_)                                                \
    /*   (symbol,               snek_symbol,            message)             */     \
    item_(Success,              success,                "operation succeeded")      \
    item_(NotImplemented,       not_implemented,        "not implemented")          \
    item_(OperationAborted,     operation_aborted,      "aborted")                  \
    item_(Fail,                 fail,                   "failure")                  \
    item_(NotFound,             not_found,              "not found")                \
    item_(AccessDenied,         access_denied,          "access denied")            \
    item_(OutOfMemory,          out_of_memory,          "out of memory")            \
    item_(NotSupported,         not_supported,          "not supported")            \
    item_(InvalidArgument,      invalid_argument,       "invalid argument")         \
    item_(InvalidState,         invalid_state,          "invalid state")            \
    item_(VersionCheckFailure,  version_check_failure,  "version check failure")    \
    item_(VersionParseError,    version_parse_error,    "version parse error")      \
    item_(InsufficientBuffer,   insufficient_buffer,    "insufficient buffer")      \
    item_(TryAgain,             try_again,              "try again")                \
    item_(Interrupted,          interrupted,            "interrupted")              \
    item_(NoInterface,          no_interface,           "no interface")             \
    item_(NullPointer,          null_pointer,           "null pointer")             \
    item_(AlreadyExists,        already_exists,         "already exists")           \
    item_(InvalidOperation,     invalid_operation,      "invalid operation")        \
    item_(NoMoreItems,          no_more_items,          "no more items")            \
    item_(InvalidIndex,         invalid_index,          "invalid index")            \
    item_(NotEnoughData,        not_enough_data,        "not enough data")          \
    item_(TooMuchData,          too_much_data,          "too much data")            \
    item_(InvalidDataType,      invalid_data_type,      "invalid data type")        \
    item_(InvalidDataSize,      invalid_data_size,      "invalid data size")

// clang-format on
//! \endcond

} // namespace core
} // namespace omni
