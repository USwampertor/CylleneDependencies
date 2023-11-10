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
#include "../cpp17/Optional.h"
#include "DictionaryUtils.h"
#include "IDictionary.h"
#include "ISerializer.h"

#include <memory>
#include <vector>

namespace carb
{
namespace dictionary
{
struct Item
{
};
} // namespace dictionary
} // namespace carb

namespace carb
{
namespace dictionary
{

template <typename T>
inline py::tuple toTuple(const std::vector<T>& v)
{
    py::tuple tuple(v.size());
    for (size_t i = 0; i < v.size(); i++)
        tuple[i] = v[i];
    return tuple;
}

// Prerequisites: dictionary lock must be held followed by GIL
inline py::object getPyObjectLocked(dictionary::ScopedRead& lock,
                                    const dictionary::IDictionary* idictionary,
                                    const Item* baseItem,
                                    const char* path = "")
{
    CARB_ASSERT(baseItem);

    const Item* item = path && *path != '\0' ? idictionary->getItem(baseItem, path) : baseItem;
    ItemType itemType = idictionary->getItemType(item);
    switch (itemType)
    {
        case ItemType::eInt:
        {
            return py::int_(idictionary->getAsInt64(item));
        }
        case ItemType::eFloat:
        {
            return py::float_(idictionary->getAsFloat64(item));
        }
        case ItemType::eBool:
        {
            return py::bool_(idictionary->getAsBool(item));
        }
        case ItemType::eString:
        {
            return py::str(getStringFromItemValue(idictionary, item));
        }
        case ItemType::eDictionary:
        {
            size_t const arrayLength = idictionary->getArrayLength(item);
            if (arrayLength > 0)
            {
                py::tuple v(arrayLength);
                bool needsList = false;
                for (size_t idx = 0; idx != arrayLength; ++idx)
                {
                    v[idx] = getPyObjectLocked(lock, idictionary, idictionary->getItemChildByIndex(item, idx));
                    if (py::isinstance<py::dict>(v[idx]))
                    {
                        // The old code would return a list of dictionaries, but a tuple of everything else. *shrug*
                        needsList = true;
                    }
                }
                if (needsList)
                {
                    return py::list(std::move(v));
                }
                return v;
            }
            else
            {
                size_t childCount = idictionary->getItemChildCount(item);
                py::dict v;
                for (size_t idx = 0; idx < childCount; ++idx)
                {
                    const dictionary::Item* childItem = idictionary->getItemChildByIndex(item, idx);
                    if (childItem)
                    {
                        v[idictionary->getItemName(childItem)] = getPyObjectLocked(lock, idictionary, childItem);
                    }
                }
                return v;
            }
        }
        default:
            return py::none();
    }
}

inline py::object getPyObject(const dictionary::IDictionary* idictionary, const Item* baseItem, const char* path = "")
{
    if (!baseItem)
    {
        return py::none();
    }

    // We need both the dictionary lock and the GIL, but we should take the GIL last, so release the GIL temporarily,
    // grab the dictionary lock and then re-lock the GIL by resetting the optional<>.
    cpp17::optional<py::gil_scoped_release> nogil{ cpp17::in_place };
    dictionary::ScopedRead readLock(*idictionary, baseItem);
    nogil.reset();

    return getPyObjectLocked(readLock, idictionary, baseItem, path);
}

inline void setPyObject(dictionary::IDictionary* idictionary, Item* baseItem, const char* path, const py::handle& value)
{
    auto createDict = [](dictionary::IDictionary* idictionary, Item* baseItem, const char* path, const py::handle& value) {
        py::dict valueDict = value.cast<py::dict>();
        for (auto kv : valueDict)
        {
            std::string basePath = path ? path : "";
            if (!basePath.empty())
                basePath = basePath + "/";
            std::string subPath = basePath + kv.first.cast<std::string>().c_str();
            setPyObject(idictionary, baseItem, subPath.c_str(), kv.second);
        }
    };

    if (py::isinstance<py::bool_>(value))
    {
        auto val = value.cast<bool>();
        py::gil_scoped_release nogil;
        idictionary->makeBoolAtPath(baseItem, path, val);
    }
    else if (py::isinstance<py::int_>(value))
    {
        auto val = value.cast<int64_t>();
        py::gil_scoped_release nogil;
        idictionary->makeInt64AtPath(baseItem, path, val);
    }
    else if (py::isinstance<py::float_>(value))
    {
        auto val = value.cast<double>();
        py::gil_scoped_release nogil;
        idictionary->makeFloat64AtPath(baseItem, path, val);
    }
    else if (py::isinstance<py::str>(value))
    {
        auto val = value.cast<std::string>();
        py::gil_scoped_release nogil;
        idictionary->makeStringAtPath(baseItem, path, val.c_str());
    }
    else if (py::isinstance<py::tuple>(value) || py::isinstance<py::list>(value))
    {
        Item* item;

        py::sequence valueSeq = value.cast<py::sequence>();
        {
            py::gil_scoped_release nogil;
            item = idictionary->makeDictionaryAtPath(baseItem, path);
            idictionary->deleteChildren(item);
        }
        for (size_t idx = 0, valueSeqSize = valueSeq.size(); idx < valueSeqSize; ++idx)
        {
            py::object valueSeqElement = valueSeq[idx];
            if (py::isinstance<py::bool_>(valueSeqElement))
            {
                auto val = valueSeqElement.cast<bool>();
                py::gil_scoped_release nogil;
                idictionary->setBoolAt(item, idx, val);
            }
            else if (py::isinstance<py::int_>(valueSeqElement))
            {
                auto val = valueSeqElement.cast<int64_t>();
                py::gil_scoped_release nogil;
                idictionary->setInt64At(item, idx, val);
            }
            else if (py::isinstance<py::float_>(valueSeqElement))
            {
                auto val = valueSeqElement.cast<double>();
                py::gil_scoped_release nogil;
                idictionary->setFloat64At(item, idx, val);
            }
            else if (py::isinstance<py::str>(valueSeqElement))
            {
                auto val = valueSeqElement.cast<std::string>();
                py::gil_scoped_release nogil;
                idictionary->setStringAt(item, idx, val.c_str());
            }
            else if (py::isinstance<py::dict>(valueSeqElement))
            {
                std::string basePath = path ? path : "";
                std::string elemPath = basePath + "/" + std::to_string(idx);
                createDict(idictionary, baseItem, elemPath.c_str(), valueSeqElement);
            }
            else
            {
                CARB_LOG_WARN("Unknown type in sequence being written to item");
            }
        }
    }
    else if (py::isinstance<py::dict>(value))
    {
        createDict(idictionary, baseItem, path, value);
    }
}

inline carb::dictionary::IDictionary* getDictionary()
{
    return getCachedInterfaceForBindings<carb::dictionary::IDictionary>();
}

inline void definePythonModule(py::module& m)
{
    using namespace carb;
    using namespace carb::dictionary;

    m.doc() = "pybind11 carb.dictionary bindings";


    py::enum_<ItemType>(m, "ItemType")
        .value("BOOL", ItemType::eBool)
        .value("INT", ItemType::eInt)
        .value("FLOAT", ItemType::eFloat)
        .value("STRING", ItemType::eString)
        .value("DICTIONARY", ItemType::eDictionary)
        .value("COUNT", ItemType::eCount);

    py::enum_<UpdateAction>(m, "UpdateAction").value("OVERWRITE", UpdateAction::eOverwrite).value("KEEP", UpdateAction::eKeep);

    py::class_<Item>(m, "Item")
        .def("__getitem__", [](const Item& self, const char* path) { return getPyObject(getDictionary(), &self, path); })
        .def("__setitem__",
             [](Item& self, const char* path, py::object value) { setPyObject(getDictionary(), &self, path, value); })

        .def("__len__", [](Item& self) { return getDictionary()->getItemChildCount(&self); },
             py::call_guard<py::gil_scoped_release>())
        .def("get",
             [](const Item& self, const char* path, py::object defaultValue) {
                 py::object v = getPyObject(getDictionary(), &self, path);
                 return v.is_none() ? defaultValue : v;
             })
        .def("get_key_at",
             [](const Item& self, size_t index) -> py::object {
                 cpp17::optional<std::string> name;
                 {
                     py::gil_scoped_release nogil;
                     dictionary::ScopedRead readlock(*getDictionary(), &self);
                     auto child = getDictionary()->getItemChildByIndex(&self, index);
                     if (child)
                         name.emplace(getDictionary()->getItemName(child));
                 }
                 if (name)
                     return py::str(name.value());
                 return py::none();
             })
        .def("__contains__",
             [](const Item& self, py::object value) -> bool {
                 auto name = value.cast<std::string>();
                 py::gil_scoped_release nogil;
                 dictionary::ScopedRead readlock(*getDictionary(), &self);
                 ItemType type = getDictionary()->getItemType(&self);
                 if (type != ItemType::eDictionary)
                     return false;
                 return getDictionary()->getItem(&self, name.c_str()) != nullptr;
             })
        .def("get_keys",
             [](const Item& self) {
                 IDictionary* idictionary = getDictionary();
                 dictionary::ScopedRead readlock(*idictionary, &self);
                 std::vector<std::string> keys(idictionary->getItemChildCount(&self));
                 for (size_t i = 0; i < keys.size(); i++)
                 {
                     const Item* child = idictionary->getItemChildByIndex(&self, i);
                     if (child)
                         keys[i] = idictionary->getItemName(child);
                 }
                 return keys;
             },
             py::call_guard<py::gil_scoped_release>())
        .def("clear", [](Item& self) { getDictionary()->deleteChildren(&self); },
             py::call_guard<py::gil_scoped_release>())
        .def("get_dict", [](Item& self) { return getPyObject(getDictionary(), &self, nullptr); })
        .def("__str__", [](Item& self) { return py::str(getPyObject(getDictionary(), &self, nullptr)); })
        .def("__repr__", [](Item& self) {
            return py::str("carb.dictionary.Item({0})").format(getPyObject(getDictionary(), &self, nullptr));
        });


    using UpdateFunctionWrapper =
        ScriptCallbackRegistryPython<void*, dictionary::UpdateAction, const dictionary::Item*, dictionary::ItemType,
                                     const dictionary::Item*, dictionary::ItemType>;

    defineInterfaceClass<IDictionary>(m, "IDictionary", "acquire_dictionary_interface")
        .def("get_dict_copy", getPyObject,
             R"(
            Creates python object from the supplied dictionary at path (supplied item is unchanged). Item is calculated
            via the path relative to the base item.

            Args:
                base_item: The base item.
                path: Path, relative to the base item - to the item

            Returns:
                Python object with copies of the item data.)",
             py::arg("base_item"), py::arg("path") = "")
        .def("get_item", wrapInterfaceFunction(&IDictionary::getItem), py::arg("base_item"), py::arg("path") = "",
             py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>())
        .def("get_item_mutable", wrapInterfaceFunction(&IDictionary::getItemMutable), py::arg("base_item"),
             py::arg("path") = "", py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>())
        .def("get_item_child_count", wrapInterfaceFunction(&IDictionary::getItemChildCount),
             py::call_guard<py::gil_scoped_release>())
        .def("get_item_child_by_index", wrapInterfaceFunction(&IDictionary::getItemChildByIndex),
             py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>())
        .def("get_item_child_by_index_mutable", wrapInterfaceFunction(&IDictionary::getItemChildByIndexMutable),
             py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>())
        .def("get_item_parent", wrapInterfaceFunction(&IDictionary::getItemParent), py::return_value_policy::reference,
             py::call_guard<py::gil_scoped_release>())
        .def("get_item_parent_mutable", wrapInterfaceFunction(&IDictionary::getItemParentMutable),
             py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>())
        .def("get_item_type", wrapInterfaceFunction(&IDictionary::getItemType), py::call_guard<py::gil_scoped_release>())
        .def("get_item_name",
             [](const dictionary::IDictionary* idictionary, const Item* baseItem, const char* path) {
                 return getStringFromItemName(idictionary, baseItem, path);
             },
             py::arg("base_item"), py::arg("path") = "", py::call_guard<py::gil_scoped_release>())
        .def("create_item",
             [](const dictionary::IDictionary* idictionary, const py::object& item, const char* path,
                dictionary::ItemType itemType) {
                 Item* p = item.is_none() ? nullptr : item.cast<Item*>();
                 py::gil_scoped_release nogil;
                 return idictionary->createItem(p, path, itemType);
             },
             py::return_value_policy::reference)
        .def("is_accessible_as", wrapInterfaceFunction(&IDictionary::isAccessibleAs),
             py::call_guard<py::gil_scoped_release>())
        .def("is_accessible_as_array_of", wrapInterfaceFunction(&IDictionary::isAccessibleAsArrayOf),
             py::call_guard<py::gil_scoped_release>())
        .def("get_array_length", wrapInterfaceFunction(&IDictionary::getArrayLength),
             py::call_guard<py::gil_scoped_release>())
        .def("get_preferred_array_type", wrapInterfaceFunction(&IDictionary::getPreferredArrayType),
             py::call_guard<py::gil_scoped_release>())
        .def("get_as_int", wrapInterfaceFunction(&IDictionary::getAsInt64), py::call_guard<py::gil_scoped_release>())
        .def("set_int", wrapInterfaceFunction(&IDictionary::setInt64), py::call_guard<py::gil_scoped_release>())
        .def("get_as_float", wrapInterfaceFunction(&IDictionary::getAsFloat64), py::call_guard<py::gil_scoped_release>())
        .def("set_float", wrapInterfaceFunction(&IDictionary::setFloat64), py::call_guard<py::gil_scoped_release>())
        .def("get_as_bool", wrapInterfaceFunction(&IDictionary::getAsBool), py::call_guard<py::gil_scoped_release>())
        .def("set_bool", wrapInterfaceFunction(&IDictionary::setBool), py::call_guard<py::gil_scoped_release>())
        .def("get_as_string",
             [](const dictionary::IDictionary* idictionary, const Item* baseItem, const char* path) {
                 return getStringFromItemValue(idictionary, baseItem, path);
             },
             py::arg("base_item"), py::arg("path") = "", py::call_guard<py::gil_scoped_release>())
        .def("set_string",
             [](dictionary::IDictionary* idictionary, Item* item, const std::string& str) {
                 idictionary->setString(item, str.c_str());
             },
             py::call_guard<py::gil_scoped_release>())
        .def("get", &getPyObject, py::arg("base_item"), py::arg("path") = "")
        .def("set", &setPyObject, py::arg("item"), py::arg("path") = "", py::arg("value"))
        .def("set_int_array",
             [](const dictionary::IDictionary* idictionary, Item* item, const std::vector<int64_t>& v) {
                 idictionary->setInt64Array(item, v.data(), v.size());
             },
             py::call_guard<py::gil_scoped_release>())
        .def("set_float_array",
             [](const dictionary::IDictionary* idictionary, Item* item, const std::vector<double>& v) {
                 idictionary->setFloat64Array(item, v.data(), v.size());
             },
             py::call_guard<py::gil_scoped_release>())
        .def("set_bool_array",
             [](const dictionary::IDictionary* idictionary, Item* item, const std::vector<bool>& v) {
                 if (v.size() == 0)
                     return;

                 bool* pbool = CARB_STACK_ALLOC(bool, v.size());
                 for (size_t i = 0; i != v.size(); ++i)
                     pbool[i] = v[i];
                 idictionary->setBoolArray(item, pbool, v.size());
             },
             py::call_guard<py::gil_scoped_release>())
        .def("set_string_array",
             [](const dictionary::IDictionary* idictionary, Item* item, const std::vector<std::string>& v) {
                 if (v.size() == 0)
                     return;

                 const char** pstr = CARB_STACK_ALLOC(const char*, v.size());
                 for (size_t i = 0; i != v.size(); ++i)
                     pstr[i] = v[i].c_str();
                 idictionary->setStringArray(item, pstr, v.size());
             },
             py::call_guard<py::gil_scoped_release>())
        .def("destroy_item", wrapInterfaceFunction(&IDictionary::destroyItem), py::call_guard<py::gil_scoped_release>())
        .def("update",
             [](dictionary::IDictionary* idictionary, dictionary::Item* dstItem, const char* dstPath,
                const dictionary::Item* srcItem, const char* srcPath, const py::object& updatePolicy) {
                 if (py::isinstance<dictionary::UpdateAction>(updatePolicy))
                 {
                     dictionary::UpdateAction updatePolicyEnum = updatePolicy.cast<dictionary::UpdateAction>();
                     py::gil_scoped_release nogil;
                     if (updatePolicyEnum == dictionary::UpdateAction::eOverwrite)
                     {
                         idictionary->update(dstItem, dstPath, srcItem, srcPath, dictionary::overwriteOriginal, nullptr);
                     }
                     else if (updatePolicyEnum == dictionary::UpdateAction::eKeep)
                     {
                         idictionary->update(dstItem, dstPath, srcItem, srcPath, dictionary::keepOriginal, nullptr);
                     }
                     else
                     {
                         CARB_LOG_ERROR("Unknown update policy type");
                     }
                 }
                 else
                 {
                     const UpdateFunctionWrapper::FuncT updateFn =
                         updatePolicy.cast<const UpdateFunctionWrapper::FuncT>();
                     py::gil_scoped_release nogil;
                     idictionary->update(
                         dstItem, dstPath, srcItem, srcPath, UpdateFunctionWrapper::call, (void*)&updateFn);
                 }
             })
        .def("readLock", wrapInterfaceFunction(&IDictionary::readLock), py::call_guard<py::gil_scoped_release>())
        .def("writeLock", wrapInterfaceFunction(&IDictionary::writeLock), py::call_guard<py::gil_scoped_release>())
        .def("unlock", wrapInterfaceFunction(&IDictionary::unlock), py::call_guard<py::gil_scoped_release>());

    carb::defineInterfaceClass<ISerializer>(m, "ISerializer", "acquire_serializer_interface")
        .def("create_dictionary_from_file", &createDictionaryFromFile, py::arg("path"),
             py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>())
        .def("create_dictionary_from_string_buffer",
             [](ISerializer* self, std::string val) {
                 return self->createDictionaryFromStringBuffer(
                     val.data(), val.size(), carb::dictionary::fDeserializerOptionInSitu);
             },
             py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>())
        .def("create_string_buffer_from_dictionary",
             [](ISerializer* self, const carb::dictionary::Item* dictionary, SerializerOptions serializerOptions) {
                 const char* buf = self->createStringBufferFromDictionary(dictionary, serializerOptions);
                 std::string ret = buf; // Copy
                 self->destroyStringBuffer(buf);
                 return ret;
             },
             py::arg("item"), py::arg("ser_options") = 0, py::call_guard<py::gil_scoped_release>())

        .def("save_file_from_dictionary", &saveFileFromDictionary, py::arg("dict"), py::arg("path"),
             py::arg("options") = 0, py::call_guard<py::gil_scoped_release>());

    m.def("get_toml_serializer",
          []() {
              static ISerializer* s_serializer =
                  carb::getFramework()->acquireInterface<ISerializer>("carb.dictionary.serializer-toml.plugin");
              return s_serializer;
          },
          py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>());
    m.def("get_json_serializer",
          []() {
              static ISerializer* s_serializer =
                  carb::getFramework()->acquireInterface<ISerializer>("carb.dictionary.serializer-json.plugin");
              return s_serializer;
          },
          py::return_value_policy::reference, py::call_guard<py::gil_scoped_release>());
}
} // namespace dictionary
} // namespace carb
