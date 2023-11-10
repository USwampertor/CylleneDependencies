// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "PyBind.h"

#include <cstring>

namespace omni
{
namespace python
{
namespace detail
{
template <typename VT, typename T, size_t S>
T getVectorValue(const VT& vector, size_t i)
{
    if (i >= S)
    {
        throw py::index_error();
    }

    const T* components = reinterpret_cast<const T*>(&vector);
    return components[i];
}
template <typename VT, typename T, size_t S>
void setVectorValue(VT& vector, size_t i, T value)
{
    if (i >= S)
    {
        throw py::index_error();
    }

    T* components = reinterpret_cast<T*>(&vector);
    components[i] = value;
}

template <typename VT, typename T, size_t S>
py::list getVectorSlice(const VT& s, const py::slice& slice)
{
    size_t start, stop, step, slicelength;
    if (!slice.compute(S, &start, &stop, &step, &slicelength))
        throw py::error_already_set();
    py::list returnList;
    for (size_t i = 0; i < slicelength; ++i)
    {
        returnList.append(getVectorValue<VT, T, S>(s, start));
        start += step;
    }
    return returnList;
}

template <typename VT, typename T, size_t S>
void setVectorSlice(VT& s, const py::slice& slice, const py::sequence& value)
{
    size_t start, stop, step, slicelength;
    if (!slice.compute(S, &start, &stop, &step, &slicelength))
        throw py::error_already_set();
    if (slicelength != value.size())
        throw std::runtime_error("Left and right hand size of slice assignment have different sizes!");
    for (size_t i = 0; i < slicelength; ++i)
    {
        setVectorValue<VT, T, S>(s, start, value[i].cast<T>());
        start += step;
    }
}

template <typename TupleT, class T, size_t S>
py::class_<TupleT> bindVec(py::module& m, const char* name, const char* docstring = nullptr)
{
    py::class_<TupleT> c(m, name, docstring);

    c.def(py::init<>());

    // Python special methods for iterators, [], len():
    c.def("__len__", [](const TupleT&) { return S; });
    c.def("__getitem__", [](const TupleT& t, size_t i) { return getVectorValue<TupleT, T, S>(t, i); });
    c.def("__setitem__", [](TupleT& t, size_t i, T v) { setVectorValue<TupleT, T, S>(t, i, v); });
    c.def("__getitem__",
          [](const TupleT& t, py::slice slice) -> py::list { return getVectorSlice<TupleT, T, S>(t, slice); });
    c.def("__setitem__",
          [](TupleT& t, py::slice slice, const py::sequence& value) { setVectorSlice<TupleT, T, S>(t, slice, value); });
    c.def("__eq__", [](TupleT& self, TupleT& other) { return 0 == std::memcmp(&self, &other, sizeof(TupleT)); });

    // That allows passing python sequence into C++ function which accepts concrete TupleT:
    py::implicitly_convertible<py::sequence, TupleT>();

    return c;
}

} // namespace detail
} // namespace python
} // namespace omni
