//
// Copyright 2016 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//

#include "pxr/pxr.h"
#include "pxr/base/tf/api.h"
#include <boost/python/handle.hpp>

PXR_NAMESPACE_OPEN_SCOPE

struct TfPyExceptionState {
    TfPyExceptionState(boost::python::handle<> const &type,
                       boost::python::handle<> const &value,
                       boost::python::handle<> const &trace) :
            _type(type), _value(value), _trace(trace) {}

    TF_API
    ~TfPyExceptionState();

    boost::python::handle<> const &GetType() const { return _type; }
    boost::python::handle<> const &GetValue() const { return _value; }
    boost::python::handle<> const &GetTrace() const { return _trace; }

    TF_API
    void Release();

    TF_API
    std::string GetExceptionString() const;

private:
    boost::python::handle<> _type, _value, _trace;
};

PXR_NAMESPACE_CLOSE_SCOPE
