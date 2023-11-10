// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once
#include "../BindingsPythonUtils.h"
#include "../Framework.h"
#include "TokensUtils.h"

#include <memory>
#include <string>
#include <vector>


namespace carb
{
namespace tokens
{

namespace
{


inline void definePythonModule(py::module& m)
{
    using namespace carb::tokens;

    m.attr("RESOLVE_FLAG_NONE") = py::int_(kResolveFlagNone);
    m.attr("RESOLVE_FLAG_LEAVE_TOKEN_IF_NOT_FOUND") = py::int_(kResolveFlagLeaveTokenIfNotFound);

    defineInterfaceClass<ITokens>(m, "ITokens", "acquire_tokens_interface")
        .def("set_value", wrapInterfaceFunction(&ITokens::setValue), py::call_guard<py::gil_scoped_release>())
        .def("set_initial_value", &ITokens::setInitialValue, py::call_guard<py::gil_scoped_release>())
        .def("remove_token", &ITokens::removeToken, py::call_guard<py::gil_scoped_release>())
        .def("exists", wrapInterfaceFunction(&ITokens::exists), py::call_guard<py::gil_scoped_release>())
        .def("resolve",
             [](ITokens* self, const std::string& str, ResolveFlags flags) -> py::str {
                 carb::tokens::ResolveResult result;
                 std::string resolvedString;
                 {
                     py::gil_scoped_release nogil;
                     resolvedString = carb::tokens::resolveString(self, str.c_str(), flags, &result);
                 }
                 if (result == ResolveResult::eSuccess)
                     return resolvedString;
                 else
                     return py::none();
             },
             py::arg("str"), py::arg("flags") = kResolveFlagNone)

        ;
}
} // namespace

} // namespace tokens
} // namespace carb
