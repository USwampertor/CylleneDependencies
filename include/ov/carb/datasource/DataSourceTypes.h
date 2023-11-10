// Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Defines.h"

namespace carb
{
namespace datasource
{

typedef uint64_t RequestId;
typedef uint64_t SubscriptionId;
typedef uint64_t ConnectionId;

constexpr uint64_t kInvalidConnectionId = ~0ull;
constexpr uint64_t kSubscriptionFailed = 0;

struct Connection;

/**
 * Defines a descriptor for a connection.
 */
struct ConnectionDesc
{
    const char* url;
    const char* username;
    const char* password;
    bool disableCache;
};

/**
 * Defines a struct holding the list data item information
 */
struct ItemInfo
{
    const char* path;
    const char* version;
    time_t modifiedTimestamp;
    time_t createdTimestamp;
    size_t size;
    bool isDirectory;
    bool isWritable;
};


enum class ChangeAction
{
    eCreated,
    eDeleted,
    eModified,
    eConnectionLost
};

/**
 * Defines the connection event type.
 */
enum class ConnectionEventType
{
    eConnected,
    eFailed,
    eDisconnected,
    eInterrupted
};

/**
 * Response results for data requests.
 */
enum class Response
{
    eOk,
    eErrorInvalidPath,
    eErrorAlreadyExists,
    eErrorIncompatibleVersion,
    eErrorTimeout,
    eErrorAccess,
    eErrorUnknown
};

/**
 * Function callback on connection events.
 *
 * @param connection The connection used.
 * @param eventType The connection event type.
 * @param userData The user data passed back.
 */
typedef void (*OnConnectionEventFn)(Connection* connection, ConnectionEventType eventType, void* userData);

/**
 * Function callback on change events.
 *
 * @param path The path that has changed.
 * @param action The change action that has occurred.
 * @parm userData The user data passed back.
 */
typedef void (*OnChangeEventFn)(const char* path, ChangeAction action, void* userData);

/**
 * Function callback on listed data items.
 *
 * This is called for each item returned from IDataSource::listData
 *
 * @param response The response result.
 * @param path The path of the list item.
 * @param version The version of the list item.
 * @param userData The user data passed back.
 * @return true to continue iteration, false to stop it. This can be useful when searching for a specific file
 *  or when iteration needs to be user interruptable.
 */
typedef bool (*OnListDataItemFn)(Response response, const ItemInfo* const info, void* userData);

/**
 * Function callback on listed data items are done.
 *
 * @param response The response result.
 * @param path The path the listing is complete listing items for.
 * @param userData The user data passed back.
 */
typedef void (*OnListDataDoneFn)(Response response, const char* path, void* userData);

/**
 * Function callback on data created.
 *
 * @param response The response result.
 * @param path The path the data was created on.
 * @param version The version of the data created.
 * @param userData The user data passed back.
 */
typedef void (*OnCreateDataFn)(Response response, const char* path, const char* version, void* userData);

/**
 * Function callback on data deleted.
 *
 * @param response The response result.
 * @param path The path the data was created on.
 * @param userData The user data passed back.
 */
typedef void (*OnDeleteDataFn)(Response response, const char* path, void* userData);

/**
 * Function callback on data read.
 *
 * @param response The response result.
 * @param path The path the data was created on.
 * @param payload The payload data that was read. *** This must be freed when completed.
 * @param payloadSize The size of the payload data read.
 * @param userData The user data passed back.
 */
typedef void (*OnReadDataFn)(Response response, const char* path, uint8_t* payload, size_t payloadSize, void* userData);

/**
 * Function callback on data written.
 *
 * @param response The response result.
 * @param path The path the data was written at.
 * @param userData The user data passed back.
 */
typedef void (*OnWriteDataFn)(Response response, const char* path, void* userData);

/**
 * Function callback for allocation of data.
 *
 * @param size The size of data to allocate.
 * @return The pointer to the data allocated.
 */
typedef void* (*OnMallocFn)(size_t size);

/**
 * Function callback on data read.
 *
 * @param response The response result.
 * @param path The path the data was created on.
 * @param userData The user data passed back.
 */
typedef void (*OnIsWritableFn)(Response response, const char* path, bool writable, void* userData);
} // namespace datasource
} // namespace carb
