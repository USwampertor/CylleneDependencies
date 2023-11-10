// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../BindingsPythonUtils.h"
#include "../dictionary/DictionaryBindingsPython.h"
#include "ISettings.h"
#include "SettingsUtils.h"

#include <memory>
#include <string>
#include <vector>

namespace carb
{
namespace dictionary
{
struct SubscriptionId
{
};
} // namespace dictionary

namespace settings
{

namespace
{

template <typename T>
py::list toList(const std::vector<T>& v)
{
    py::list list;
    for (const T& e : v)
        list.append(e);
    return list;
}

template <typename T>
std::vector<T> toAllocatedArray(const py::sequence& s)
{
    std::vector<T> v(s.size());
    for (size_t i = 0, size = s.size(); i < size; ++i)
        v[i] = s[i].cast<T>();
    return v;
}

// std::vector<bool> is typically specialized, so avoid it
std::unique_ptr<bool[]> toBoolArray(const py::sequence& s, size_t& size)
{
    size = s.size();
    std::unique_ptr<bool[]> p(new bool[size]);
    for (size_t i = 0; i < size; ++i)
        p[i] = s[i].cast<bool>();
    return p;
}

void setValueFromPyObject(ISettings* isregistry, const char* path, const py::object& value)
{
    if (py::isinstance<py::bool_>(value))
    {
        auto val = value.cast<bool>();
        py::gil_scoped_release nogil;
        isregistry->setBool(path, val);
    }
    else if (py::isinstance<py::int_>(value))
    {
        auto val = value.cast<int64_t>();
        py::gil_scoped_release nogil;
        isregistry->setInt64(path, val);
    }
    else if (py::isinstance<py::float_>(value))
    {
        auto val = value.cast<double>();
        py::gil_scoped_release nogil;
        isregistry->setFloat64(path, val);
    }
    else if (py::isinstance<py::str>(value))
    {
        auto val = value.cast<std::string>();
        py::gil_scoped_release nogil;
        isregistry->setString(path, val.c_str());
    }
    else if (py::isinstance<py::tuple>(value) || py::isinstance<py::list>(value))
    {
        py::sequence valueSeq = value.cast<py::sequence>();
        {
            py::gil_scoped_release nogil;
            isregistry->destroyItem(path);
        }
        for (size_t idx = 0, valueSeqSize = valueSeq.size(); idx < valueSeqSize; ++idx)
        {
            py::object valueSeqElement = valueSeq[idx];
            if (py::isinstance<py::bool_>(valueSeqElement))
            {
                auto val = valueSeqElement.cast<bool>();
                py::gil_scoped_release nogil;
                isregistry->setBoolAt(path, idx, val);
            }
            else if (py::isinstance<py::int_>(valueSeqElement))
            {
                auto val = valueSeqElement.cast<int64_t>();
                py::gil_scoped_release nogil;
                isregistry->setInt64At(path, idx, val);
            }
            else if (py::isinstance<py::float_>(valueSeqElement))
            {
                auto val = valueSeqElement.cast<double>();
                py::gil_scoped_release nogil;
                isregistry->setFloat64At(path, idx, val);
            }
            else if (py::isinstance<py::str>(valueSeqElement))
            {
                auto val = valueSeqElement.cast<std::string>();
                py::gil_scoped_release nogil;
                isregistry->setStringAt(path, idx, val.c_str());
            }
            else if (py::isinstance<py::dict>(valueSeqElement))
            {
                std::string basePath = path ? path : "";
                std::string elemPath = basePath + "/" + std::to_string(idx);
                setValueFromPyObject(isregistry, elemPath.c_str(), valueSeqElement);
            }
            else
            {
                CARB_LOG_WARN("Unknown type in sequence being written to %s", path);
            }
        }
    }
    else if (py::isinstance<py::dict>(value))
    {
        {
            py::gil_scoped_release nogil;
            isregistry->destroyItem(path);
        }
        py::dict valueDict = value.cast<py::dict>();
        for (auto kv : valueDict)
        {
            std::string basePath = path ? path : "";
            if (!basePath.empty())
                basePath = basePath + "/";
            std::string subPath = basePath + kv.first.cast<std::string>().c_str();
            setValueFromPyObject(isregistry, subPath.c_str(), kv.second.cast<py::object>());
        }
    }
}

void setDefaultValueFromPyObject(ISettings* isregistry, const char* path, const py::object& value)
{
    if (py::isinstance<py::bool_>(value))
    {
        auto val = value.cast<bool>();
        py::gil_scoped_release nogil;
        isregistry->setDefaultBool(path, val);
    }
    else if (py::isinstance<py::int_>(value))
    {
        auto val = value.cast<int64_t>();
        py::gil_scoped_release nogil;
        isregistry->setDefaultInt64(path, val);
    }
    else if (py::isinstance<py::float_>(value))
    {
        auto val = value.cast<double>();
        py::gil_scoped_release nogil;
        isregistry->setDefaultFloat64(path, val);
    }
    else if (py::isinstance<py::str>(value))
    {
        auto val = value.cast<std::string>();
        py::gil_scoped_release nogil;
        isregistry->setDefaultString(path, val.c_str());
    }
    else if (py::isinstance<py::tuple>(value) || py::isinstance<py::list>(value))
    {
        py::sequence valueSeq = value.cast<py::sequence>();

        if (valueSeq.size() == 0)
        {
            py::gil_scoped_release nogil;
            isregistry->setDefaultArray<int>(path, nullptr, 0);
        }
        else
        {
            const py::object& firstElement = valueSeq[0];
            if (py::isinstance<py::bool_>(firstElement))
            {
                size_t size;
                auto array = toBoolArray(valueSeq, size);
                py::gil_scoped_release nogil;
                isregistry->setDefaultArray<bool>(path, array.get(), size);
            }
            else if (py::isinstance<py::int_>(firstElement))
            {
                auto array = toAllocatedArray<int64_t>(valueSeq);
                py::gil_scoped_release nogil;
                isregistry->setDefaultArray<int64_t>(path, &array.front(), array.size());
            }
            else if (py::isinstance<py::float_>(firstElement))
            {
                auto array = toAllocatedArray<double>(valueSeq);
                py::gil_scoped_release nogil;
                isregistry->setDefaultArray<double>(path, &array.front(), array.size());
            }
            else if (py::isinstance<py::str>(firstElement))
            {
                std::vector<std::string> strs(valueSeq.size());
                std::vector<const char*> strPtrs(valueSeq.size());
                for (size_t i = 0, size = valueSeq.size(); i < size; ++i)
                {
                    strs[i] = valueSeq[i].cast<std::string>();
                    strPtrs[i] = strs[i].c_str();
                }
                py::gil_scoped_release nogil;
                isregistry->setDefaultArray<const char*>(path, strPtrs.data(), strPtrs.size());
            }
            else if (py::isinstance<py::dict>(firstElement))
            {
                std::string basePath = path ? path : "";
                for (size_t i = 0, size = valueSeq.size(); i < size; ++i)
                {
                    std::string elemPath = basePath + "/" + std::to_string(i);
                    setDefaultValueFromPyObject(isregistry, elemPath.c_str(), valueSeq[i]);
                }
            }
            else
            {
                CARB_LOG_WARN("Unknown type in sequence being set as default in '%s'", path);
            }
        }
    }
    else if (py::isinstance<py::dict>(value))
    {
        py::dict valueDict = value.cast<py::dict>();
        for (auto kv : valueDict)
        {
            std::string basePath = path ? path : "";
            if (!basePath.empty())
                basePath = basePath + "/";
            std::string subPath = basePath + kv.first.cast<std::string>().c_str();
            setDefaultValueFromPyObject(isregistry, subPath.c_str(), kv.second.cast<py::object>());
        }
    }
}

} // namespace

inline void definePythonModule(py::module& m)
{
    using namespace carb;
    using namespace carb::settings;

    m.doc() = "pybind11 carb.settings bindings";

    py::class_<dictionary::SubscriptionId>(m, "SubscriptionId");

    py::enum_<dictionary::ChangeEventType>(m, "ChangeEventType")
        .value("CREATED", dictionary::ChangeEventType::eCreated)
        .value("CHANGED", dictionary::ChangeEventType::eChanged)
        .value("DESTROYED", dictionary::ChangeEventType::eDestroyed);

    static ScriptCallbackRegistryPython<dictionary::SubscriptionId*, void, const dictionary::Item*, dictionary::ChangeEventType>
        s_nodeChangeEventCBs;

    static ScriptCallbackRegistryPython<dictionary::SubscriptionId*, void, const dictionary::Item*,
                                        const dictionary::Item*, dictionary::ChangeEventType>
        s_treeChangeEventCBs;

    using UpdateFunctionWrapper =
        ScriptCallbackRegistryPython<void*, dictionary::UpdateAction, const dictionary::Item*, dictionary::ItemType,
                                     const dictionary::Item*, dictionary::ItemType>;

    defineInterfaceClass<ISettings>(m, "ISettings", "acquire_settings_interface")
        .def("is_accessible_as", wrapInterfaceFunction(&ISettings::isAccessibleAs),
             py::call_guard<py::gil_scoped_release>())
        .def("get_as_int", wrapInterfaceFunction(&ISettings::getAsInt64), py::call_guard<py::gil_scoped_release>())
        .def("set_int", wrapInterfaceFunction(&ISettings::setInt64), py::call_guard<py::gil_scoped_release>())
        .def("get_as_float", wrapInterfaceFunction(&ISettings::getAsFloat64), py::call_guard<py::gil_scoped_release>())
        .def("set_float", wrapInterfaceFunction(&ISettings::setFloat64), py::call_guard<py::gil_scoped_release>())
        .def("get_as_bool", wrapInterfaceFunction(&ISettings::getAsBool), py::call_guard<py::gil_scoped_release>())
        .def("set_bool", wrapInterfaceFunction(&ISettings::setBool), py::call_guard<py::gil_scoped_release>())
        .def("get_as_string",
             [](const ISettings* isregistry, const char* path) { return getStringFromItemValue(isregistry, path); },
             py::call_guard<py::gil_scoped_release>())
        .def("set_string",
             [](ISettings* isregistry, const char* path, const std::string& str) {
                 isregistry->setString(path, str.c_str());
             },
             py::call_guard<py::gil_scoped_release>())
        .def("get",
             // The defaultValue here is DEPRECATED, some of the scripts out there still use it like that. TODO: remove
             // it after some time.
             [](const ISettings* isregistry, const char* path) -> py::object {
                 const dictionary::Item* item = isregistry->getSettingsDictionary(path);
                 auto obj = dictionary::getPyObject(getCachedInterfaceForBindings<dictionary::IDictionary>(), item);
                 if (py::isinstance<py::tuple>(obj))
                 {
                     // Settings wants a list instead of a tuple
                     return py::list(std::move(obj));
                 }
                 return obj;
             },
             py::arg("path"))
        .def("set", &setValueFromPyObject, py::arg("path"), py::arg("value"))
        .def("set_default", &setDefaultValueFromPyObject, py::arg("path"), py::arg("value"))
        .def("set_int_array",
             [](ISettings* isregistry, const char* path, const std::vector<int32_t>& array) {
                 settings::setIntArray(isregistry, path, array);
             },
             py::call_guard<py::gil_scoped_release>())
        .def("set_float_array",
             [](ISettings* isregistry, const char* path, const std::vector<double>& array) {
                 settings::setFloatArray(isregistry, path, array);
             },
             py::call_guard<py::gil_scoped_release>())
        .def("set_bool_array",
             [](ISettings* isregistry, const char* path, const std::vector<bool>& array) {
                 settings::setBoolArray(isregistry, path, array);
             },
             py::call_guard<py::gil_scoped_release>())
        .def("set_string_array",
             [](ISettings* isregistry, const char* path, const std::vector<std::string>& array) {
                 settings::setStringArray(isregistry, path, array);
             },
             py::call_guard<py::gil_scoped_release>())
        .def("destroy_item", wrapInterfaceFunction(&ISettings::destroyItem), py::call_guard<py::gil_scoped_release>())
        .def("get_settings_dictionary", wrapInterfaceFunction(&ISettings::getSettingsDictionary),
             py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>())
        .def("create_dictionary_from_settings", wrapInterfaceFunction(&ISettings::createDictionaryFromSettings),
             py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>())
        .def("initialize_from_dictionary", wrapInterfaceFunction(&ISettings::initializeFromDictionary),
             py::call_guard<py::gil_scoped_release>())
        .def("subscribe_to_node_change_events",
             [](ISettings* isregistry, const char* path, const decltype(s_nodeChangeEventCBs)::FuncT& eventFn) {
                 auto eventFnCopy = s_nodeChangeEventCBs.create(eventFn);
                 dictionary::SubscriptionId* id =
                     isregistry->subscribeToNodeChangeEvents(path, s_nodeChangeEventCBs.call, eventFnCopy);
                 s_nodeChangeEventCBs.add(id, eventFnCopy);
                 return id;
             },
             py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>())
        .def("subscribe_to_tree_change_events",
             [](ISettings* isregistry, const char* path, const decltype(s_treeChangeEventCBs)::FuncT& eventFn) {
                 auto eventFnCopy = s_treeChangeEventCBs.create(eventFn);
                 dictionary::SubscriptionId* id =
                     isregistry->subscribeToTreeChangeEvents(path, s_treeChangeEventCBs.call, eventFnCopy);
                 s_treeChangeEventCBs.add(id, eventFnCopy);
                 return id;
             },
             py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>())
        .def("unsubscribe_to_change_events",
             [](ISettings* isregistry, dictionary::SubscriptionId* id) {
                 isregistry->unsubscribeToChangeEvents(id);
                 s_nodeChangeEventCBs.tryRemoveAndDestroy(id);
                 s_treeChangeEventCBs.tryRemoveAndDestroy(id);
             },
             py::call_guard<py::gil_scoped_release>())
        .def("set_default_int",
             [](ISettings* isregistry, const char* path, int value) { isregistry->setDefaultInt(path, value); },
             py::call_guard<py::gil_scoped_release>())
        .def("set_default_float",
             [](ISettings* isregistry, const char* path, float value) { isregistry->setDefaultFloat(path, value); },
             py::call_guard<py::gil_scoped_release>())
        .def("set_default_bool",
             [](ISettings* isregistry, const char* path, bool value) { isregistry->setDefaultBool(path, value); },
             py::call_guard<py::gil_scoped_release>())
        .def("set_default_string",
             [](ISettings* isregistry, const char* path, const std::string& str) {
                 isregistry->setDefaultString(path, str.c_str());
             },
             py::call_guard<py::gil_scoped_release>())
        .def("update", [](ISettings* isregistry, const char* path, const dictionary::Item* dictionary,
                          const char* dictionaryPath, const py::object& updatePolicy) {
            if (py::isinstance<dictionary::UpdateAction>(updatePolicy))
            {
                dictionary::UpdateAction updatePolicyEnum = updatePolicy.cast<dictionary::UpdateAction>();
                py::gil_scoped_release nogil;
                if (updatePolicyEnum == dictionary::UpdateAction::eOverwrite)
                {
                    isregistry->update(path, dictionary, dictionaryPath, dictionary::overwriteOriginal, nullptr);
                }
                else if (updatePolicyEnum == dictionary::UpdateAction::eKeep)
                {
                    isregistry->update(path, dictionary, dictionaryPath, dictionary::keepOriginal, nullptr);
                }
                else
                {
                    CARB_LOG_ERROR("Unknown update policy type");
                }
            }
            else
            {
                const UpdateFunctionWrapper::FuncT updateFn = updatePolicy.cast<const UpdateFunctionWrapper::FuncT>();
                py::gil_scoped_release nogil;
                isregistry->update(path, dictionary, dictionaryPath, UpdateFunctionWrapper::call, (void*)&updateFn);
            }
        });
}
} // namespace settings
} // namespace carb
