// Copyright (c) 2019-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Framework.h"
#include "../Types.h"
#include "../logging/ILogging.h"
#include "IDataSource.h"

#include <future>

namespace carb
{
namespace datasource
{

inline Connection* connectAndWait(const ConnectionDesc& desc, const IDataSource* dataSource)
{
    std::promise<Connection*> promise;
    auto future = promise.get_future();

    dataSource->connect(desc,
                        [](Connection* connection, ConnectionEventType eventType, void* userData) {
                            std::promise<Connection*>* promise = reinterpret_cast<std::promise<Connection*>*>(userData);
                            switch (eventType)
                            {
                                case ConnectionEventType::eConnected:
                                    promise->set_value(connection);
                                    break;
                                case ConnectionEventType::eFailed:
                                case ConnectionEventType::eInterrupted:
                                    promise->set_value(nullptr);
                                    break;
                                case ConnectionEventType::eDisconnected:
                                    break;
                            }
                        },
                        &promise);

    return future.get();
}

inline Connection* connectAndWait(const ConnectionDesc& desc, const char* pluginName = nullptr)
{
    carb::Framework* framework = carb::getFramework();
    IDataSource* dataSource = framework->acquireInterface<IDataSource>(pluginName);
    return connectAndWait(desc, dataSource);
}
} // namespace datasource
} // namespace carb
