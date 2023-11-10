// Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Interface.h"
#include "../Types.h"
#include "DataSourceTypes.h"

namespace carb
{
namespace datasource
{

/**
 * Defines a data source interface.
 */
struct IDataSource
{
    CARB_PLUGIN_INTERFACE("carb::datasource::IDataSource", 1, 0)

    /**
     * Gets a list of supported protocols for this interface.
     *
     * @return The comma-separated list of supported protocols.
     */
    const char*(CARB_ABI* getSupportedProtocols)();

    /**
     * Connects to a datasource.
     *
     * @param desc The connection descriptor.
     * @param onConnectionEvent The callback for handling connection events.
     * @param userData The userData to be passed back to callback.
     */
    void(CARB_ABI* connect)(const ConnectionDesc& desc, OnConnectionEventFn onConnectionEvent, void* userData);

    /**
     * Disconnects from a datasource.
     *
     * @param connection The connection to use.
     */
    void(CARB_ABI* disconnect)(Connection* connection);

    /**
     * Attempts to stop processing a specified request on a connection.
     *
     * @param connection The connection to use.
     * @param id The request id to stop processing.
     */
    void(CARB_ABI* stopRequest)(Connection* connection, RequestId id);

    /**
     * Lists all the child relative data path entries from the specified path in the data source.
     *
     * You must delete the path returned.
     *
     * @param connection The connect to use.
     * @param path The path to start the listing from.
     * @param recursive true to recursively list items in directory and subdirectories.
     * @param onListDataItem The callback for each item listed.
     * @param onListDataDone The callback for when there are no more items listed.
     * @param userData The userData to be pass to callback.
     * @return The data request id or 0 if failed.
     */
    RequestId(CARB_ABI* listData)(Connection* connection,
                                  const char* path,
                                  bool recursize,
                                  OnListDataItemFn onListDataItem,
                                  OnListDataDoneFn onListDataDone,
                                  void* userData);

    /**
     * Creates a data block associated to the specified path to the data source.
     *
     * @param connection The connect to use.
     * @param path The path to create the data. Must not exist.
     * @param payload The payload data to be initialize to.
     * @param payloadSize The size of the payload data to initialize.
     * @param onCreateData Callback function use.
     * @param userData The userData to be pass to callback.
     * @return The data request id or 0 if failed.
     */
    RequestId(CARB_ABI* createData)(Connection* connection,
                                    const char* path,
                                    uint8_t* payload,
                                    size_t payloadSize,
                                    OnCreateDataFn onCreateData,
                                    void* userData);

    /**
     * Deletes a data block based on the specified path from the data source.
     *
     * @param connection The connect to use.
     * @param path The path of the data to be destroyed(deleted).
     * @param onFree The callback function to be used to free the data.
     * @param onDeleteData The callback function to be called when the data is deleted.
     * @param userData The userData to be pass to callback.
     * @return The data request id or 0 if failed.
     */
    RequestId(CARB_ABI* deleteData)(Connection* connection, const char* path, OnDeleteDataFn onDeleteData, void* userData);

    /**
     * Initiates an asynchronous read of data from the datasource. A callback is called when the read completes.
     *
     * @param connection The connection to use.
     * @param path The path for the data.
     * @param onMalloc The callback function to allocate the memory that will be returned in data
     * @param onReadData The callback function called once the data is read.
     * @param userData The userData to be pass to callback.
     * @return The data request id or 0 if failed.
     */
    RequestId(CARB_ABI* readData)(
        Connection* connection, const char* path, OnMallocFn onMalloc, OnReadDataFn onReadData, void* userData);

    /**
     * Synchronously reads data from the data source.
     *
     * @param connection The connection to use.
     * @param path The path for the data.
     * @param onMalloc the callback function to allocate memory that will be returned
     * @param block The allocated memory holding the data will be returned here
     * @param size The size of the allocated block will be returned here
     * @return One of the response codes to indicate the success of the call
     */
    Response(CARB_ABI* readDataSync)(
        Connection* connection, const char* path, OnMallocFn onMalloc, void** block, size_t* size);

    /**
     * Writes data to the data source.
     *
     * @param connection The connection to use.
     * @param path The path for the data.
     * @param payload The data that was written. *** This memory must be freed by the caller.
     * @param payloadSize The size of the data written.
     * @param version The version of the data written.
     * @param onWriteData The callback function to call when payload data is written.
     * @param userData The userData to be pass to callback.
     * @return The data request id or 0 if failed.
     */
    RequestId(CARB_ABI* writeData)(Connection* connection,
                                   const char* path,
                                   const uint8_t* payload,
                                   size_t payloadSize,
                                   const char* version,
                                   OnWriteDataFn onWriteData,
                                   void* userData);

    /**
     * Creates a subscription for modifications to data.
     *
     * @param connection The connection to use.
     * @param path The path for the data.
     * @param onModifyData The function to call when the data is modified.
     * @param userData The user data ptr to be associated with the callback.
     * @return The subscription id or 0 if failed.
     */
    SubscriptionId(CARB_ABI* subscribeToChangeEvents)(Connection* connection,
                                                      const char* path,
                                                      OnChangeEventFn onChangeEvent,
                                                      void* userData);

    /**
     * Removes a subscription for modifications to data.
     *
     * @param connection The connection from which to remove the subscription.
     * @param id The subscription id to unsubscribe from.
     */
    void(CARB_ABI* unsubscribeToChangeEvents)(Connection* connection, SubscriptionId subscriptionId);

    /**
     * Gets the native handle from a datasource connection.
     *
     * @param The connection from which to native connection handle from.
     * @return The native connection handle.
     */
    void*(CARB_ABI* getConnectionNativeHandle)(Connection* connection);

    /**
     * Gets the url from a datasource connection.
     *
     * @param The connection from which to get url from.
     * @return The connection url.
     */
    const char*(CARB_ABI* getConnectionUrl)(Connection* connection);

    /**
     * Gets the username from a datasource connection.
     *
     * @param The connection from which to get username from.
     * @return The connection username. nullptr if username is not applicable for the connection.
     */
    const char*(CARB_ABI* getConnectionUsername)(Connection* connection);

    /**
     * Gets the unique connection id from a datasource connection.
     * @param The connection from which to get id from.
     * @return The connection id. kInvalidConnectionId if the datasource has no id implementation
     *         or the connection is invalid.
     */
    ConnectionId(CARB_ABI* getConnectionId)(Connection* connection);

    /**
     * Tests whether it's possible to write data with the provided path.
     *
     * @param path The path to write the data.
     * @return true if it's possible to write to this data.
     */
    RequestId(CARB_ABI* isWritable)(Connection* connection, const char* path, OnIsWritableFn onIsWritable, void* userData);

    /**
     * Returns authentication token, which encapsulates the security identity of the connection.
     * The token can be used to connect to other omniverse services.
     *
     * @param connection from which to get authentication token from.
     * @return authentication token as a string.
     */
    const char*(CARB_ABI* getConnectionAuthToken)(Connection* connection);
};
} // namespace datasource
} // namespace carb
