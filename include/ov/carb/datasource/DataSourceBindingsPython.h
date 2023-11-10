// Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../BindingsPythonUtils.h"
#include "IDataSource.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace carb
{
namespace datasource
{

struct Connection
{
};

struct ConnectionDescPy
{
    std::string url;
    std::string username;
    std::string password;
    bool disableCache;
};

struct ItemInfoPy
{
    std::string path;
    std::string version;
    std::chrono::system_clock::time_point modifiedTimestamp;
    std::chrono::system_clock::time_point createdTimestamp;
    size_t size;
    bool isDirectory;
    bool isWritable;
};

inline void definePythonModule(py::module& m)
{
    using namespace carb;
    using namespace carb::datasource;

    m.doc() = "pybind11 carb.datasource bindings";

    py::class_<Connection>(m, "Connection");

    m.attr("INVALID_CONNECTION_ID") = py::int_(kInvalidConnectionId);
    m.attr("SUBSCRIPTION_FAILED") = py::int_(kSubscriptionFailed);

    py::enum_<ChangeAction>(m, "ChangeAction", R"(
        ChangeAction.
        )")
        .value("CREATED", ChangeAction::eCreated)
        .value("DELETED", ChangeAction::eDeleted)
        .value("MODIFIED", ChangeAction::eModified)
        .value("CONNECTION_LOST", ChangeAction::eConnectionLost);

    py::enum_<ConnectionEventType>(m, "ConnectionEventType", R"(
        Connection event results.
        )")
        .value("CONNECTED", ConnectionEventType::eConnected)
        .value("DISCONNECTED", ConnectionEventType::eDisconnected)
        .value("FAILED", ConnectionEventType::eFailed)
        .value("INTERUPTED", ConnectionEventType::eInterrupted);

    py::enum_<Response>(m, "Response", R"(
        Response results for data requests.
        )")
        .value("OK", Response::eOk)
        .value("ERROR_INVALID_PATH", Response::eErrorInvalidPath)
        .value("ERROR_ALREADY_EXISTS", Response::eErrorAlreadyExists)
        .value("ERROR_INCOMPATIBLE_VERSION", Response::eErrorIncompatibleVersion)
        .value("ERROR_TIMEOUT", Response::eErrorTimeout)
        .value("ERROR_ACCESS", Response::eErrorAccess)
        .value("ERROR_UNKNOWN", Response::eErrorUnknown);

    py::class_<ConnectionDescPy>(m, "ConnectionDesc", R"(
        Descriptor for a connection.
        )")
        .def(py::init<>())
        .def_readwrite("url", &ConnectionDescPy::url)
        .def_readwrite("username", &ConnectionDescPy::username)
        .def_readwrite("password", &ConnectionDescPy::password)
        .def_readwrite("disable_cache", &ConnectionDescPy::disableCache);

    py::class_<ItemInfoPy>(m, "ItemInfo", R"(
        Class holding the list data item information
        )")
        .def(py::init<>())
        .def_readonly("path", &ItemInfoPy::path)
        .def_readonly("version", &ItemInfoPy::version)
        .def_readonly("modified_timestamp", &ItemInfoPy::modifiedTimestamp)
        .def_readonly("created_timestamp", &ItemInfoPy::createdTimestamp)
        .def_readonly("size", &ItemInfoPy::size)
        .def_readonly("is_directory", &ItemInfoPy::isDirectory)
        .def_readonly("is_writable", &ItemInfoPy::isWritable);

    defineInterfaceClass<IDataSource>(m, "IDataSource", "acquire_datasource_interface")
        .def("get_supported_protocols", wrapInterfaceFunction(&IDataSource::getSupportedProtocols))
        .def("connect",
             [](IDataSource* iface, const ConnectionDescPy& descPy,
                std::function<void(Connection * connection, ConnectionEventType eventType)> fn) {
                 auto callable = createPyAdapter(std::move(fn));
                 using Callable = decltype(callable)::element_type;
                 ConnectionDesc desc = { descPy.url.c_str(), descPy.username.c_str(), descPy.password.c_str(),
                                         descPy.disableCache };

                 iface->connect(desc,
                                [](Connection* connection, ConnectionEventType eventType, void* userData) {
                                    Callable::callAndKeep(userData, connection, eventType);
                                    if (eventType != ConnectionEventType::eConnected)
                                    {
                                        Callable::destroy(userData);
                                    }
                                },
                                callable.release());
             })
        .def("disconnect", wrapInterfaceFunctionReleaseGIL(&IDataSource::disconnect))
        .def("stop_request", wrapInterfaceFunction(&IDataSource::stopRequest))
        .def("list_data",
             [](IDataSource* iface, Connection* connection, const char* path, bool recursize,
                std::function<bool(Response, const ItemInfoPy&)> onListDataItemFn,
                std::function<void(Response, const std::string&)> onListDataDoneFn) {
                 auto pair = std::make_pair(
                     createPyAdapter(std::move(onListDataItemFn)), createPyAdapter(std::move(onListDataDoneFn)));
                 using Pair = decltype(pair);

                 auto pairHeap = new Pair(std::move(pair));

                 auto onListDataItemCppFn = [](Response response, const ItemInfo* const info, void* userData) -> bool {
                     auto pyCallbacks = static_cast<Pair*>(userData);
                     ItemInfoPy infoPy;
                     infoPy.path = info->path;
                     infoPy.version = info->version ? info->version : "";
                     infoPy.modifiedTimestamp = std::chrono::system_clock::from_time_t(info->modifiedTimestamp);
                     infoPy.createdTimestamp = std::chrono::system_clock::from_time_t(info->createdTimestamp);
                     infoPy.size = info->size;
                     infoPy.isDirectory = info->isDirectory;
                     infoPy.isWritable = info->isWritable;

                     return pyCallbacks->first->call(response, infoPy);
                 };

                 auto onListDataDoneCppFn = [](Response response, const char* path, void* userData) {
                     auto pyCallbacks = reinterpret_cast<Pair*>(userData);
                     pyCallbacks->second->call(response, path);
                     delete pyCallbacks;
                 };

                 return iface->listData(connection, path, recursize, onListDataItemCppFn, onListDataDoneCppFn, pairHeap);
             })
        .def("create_data",
             [](IDataSource* iface, Connection* connection, const char* path, const py::bytes& payload,
                std::function<void(Response response, const char* path, const char* version)> onCreateDataFn) {
                 auto callable = createPyAdapter(std::move(onCreateDataFn));
                 using Callable = decltype(callable)::element_type;

                 std::string payloadContent(payload);
                 static_assert(sizeof(std::string::value_type) == sizeof(uint8_t), "payload data size mismatch");

                 return iface->createData(connection, path, reinterpret_cast<uint8_t*>(&payloadContent[0]),
                                          payloadContent.size(), Callable::adaptCallAndDestroy, callable.release());
             })
        .def("delete_data",
             [](IDataSource* iface, Connection* connection, const char* path,
                std::function<void(Response response, const char* path)> onDeleteDataFn) {
                 auto callable = createPyAdapter(std::move(onDeleteDataFn));
                 using Callable = decltype(callable)::element_type;

                 return iface->deleteData(connection, path, Callable::adaptCallAndDestroy, callable.release());
             })
        .def("read_data",
             [](IDataSource* iface, Connection* connection, const char* path,
                std::function<void(Response response, const char* path, const py::bytes& payload)> onReadDataFn) {
                 auto callable = createPyAdapter(std::move(onReadDataFn));
                 using Callable = decltype(callable)::element_type;

                 return iface->readData(
                     connection, path, std::malloc,
                     [](Response response, const char* path, uint8_t* data, size_t dataSize, void* userData) {
                         static_assert(sizeof(char) == sizeof(uint8_t), "payload data size mismatch");
                         py::gil_scoped_acquire gil; // make sure we own the GIL for creating py::bytes
                         const py::bytes payload(reinterpret_cast<const char*>(data), dataSize);
                         Callable::callAndDestroy(userData, response, path, payload);
                         // Data needs to be freed manually.
                         if (data)
                         {
                             std::free(data);
                         }
                     },
                     callable.release());
             })
        .def("read_data_sync",
             [](IDataSource* iface, Connection* connection, const char* path) -> py::bytes {
                 void* data{ nullptr };
                 size_t size{ 0 };
                 Response response = iface->readDataSync(connection, path, std::malloc, &data, &size);
                 py::gil_scoped_acquire gil; // make sure we own the GIL for creating py::bytes
                 py::bytes bytes =
                     response == Response::eOk ? py::bytes(reinterpret_cast<const char*>(data), size) : py::bytes();
                 if (data)
                 {
                     std::free(data);
                 }
                 return bytes;
             })
        .def("write_data",
             [](IDataSource* iface, Connection* connection, const char* path, const py::bytes& payload,
                const char* version, std::function<void(Response response, const char* path)> onWriteDataFn) {
                 auto callable = createPyAdapter(std::move(onWriteDataFn));
                 using Callable = decltype(callable)::element_type;

                 std::string payloadContent(payload);
                 static_assert(sizeof(std::string::value_type) == sizeof(uint8_t), "payload data size mismatch");

                 return iface->writeData(connection, path, reinterpret_cast<const uint8_t*>(payloadContent.data()),
                                         payloadContent.size(), version, Callable::adaptCallAndDestroy,
                                         callable.release());
             })
        .def("subscribe_to_change_events",
             [](IDataSource* iface, Connection* connection, const char* path,
                std::function<void(const char* path, ChangeAction action)> fn) {
                 using namespace std::placeholders;
                 return createPySubscription(std::move(fn),
                                             std::bind(iface->subscribeToChangeEvents, connection, path, _1, _2),
                                             [iface, connection](SubscriptionId id) {
                                                 // Release the GIL since unsubscribe can block on a mutex and deadlock
                                                 py::gil_scoped_release gsr;
                                                 iface->unsubscribeToChangeEvents(connection, id);
                                             });
             })
        .def("get_connection_native_handle", wrapInterfaceFunction(&IDataSource::getConnectionNativeHandle))
        .def("get_connection_url", wrapInterfaceFunction(&IDataSource::getConnectionUrl))
        .def("get_connection_username", wrapInterfaceFunction(&IDataSource::getConnectionUsername))
        .def("get_connection_id", wrapInterfaceFunction(&IDataSource::getConnectionId))
        .def("is_writable", [](IDataSource* iface, Connection* connection, const char* path,
                               std::function<void(Response response, const char* path, bool writable)> fn) {
            auto callable = createPyAdapter(std::move(fn));
            using Callable = decltype(callable)::element_type;

            return iface->isWritable(connection, path, Callable::adaptCallAndDestroy, callable.release());
        });
}
} // namespace datasource
} // namespace carb
