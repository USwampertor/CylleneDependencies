// Copyright (c) 2022-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "PyBind.h"
#include "../String.h"

namespace pybind11
{
namespace detail
{

//! This converts between @c omni::string and native Python @c str types.
template <>
struct type_caster<omni::string>
{
    // NOTE: The _ is needed to convert the character literal into a `pybind::descr` or functions using this type will
    // fail to initialize inside of `def`.
    PYBIND11_TYPE_CASTER(omni::string, _("omni::string"));

    //! Convert @a src Python string into @c omni::string instance.
    bool load(handle src, bool)
    {
        PyObject* source = src.ptr();
        if (PyObject* source_bytes_raw = PyUnicode_AsEncodedString(source, "UTF-8", "strict"))
        {
            auto source_bytes = reinterpret_steal<bytes>(handle{ source_bytes_raw });
            char* str;
            Py_ssize_t str_len;
            int rc = PyBytes_AsStringAndSize(source_bytes.ptr(), &str, &str_len);
            // Getting the string should always work here -- we've already ensured it encoded to UTF-8 bytes
            if (rc == -1)
            {
                return false;
            }

            this->value.assign(str, omni::string::size_type(str_len));
            return true;
        }
        else
        {
            return false;
        }
    }

    //! Convert @a src string into a Python Unicode string.
    //!
    //! @note
    //! The return value policy and parent object arguments are ignored. The return policy is irrelevant, as the source
    //! string is always copied into the native form. The parent object is not supported by the Python string type.
    static handle cast(const omni::string& src, return_value_policy, handle) noexcept
    {
        PyObject* native = PyUnicode_FromStringAndSize(src.data(), Py_ssize_t(src.size()));
        return handle{ native };
    }
};

} // namespace detail
} // namespace pybind11
