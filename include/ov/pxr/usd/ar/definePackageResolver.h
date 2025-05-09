//
// Copyright 2018 Pixar
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
#ifndef PXR_USD_AR_DEFINE_PACKAGE_RESOLVER_H
#define PXR_USD_AR_DEFINE_PACKAGE_RESOLVER_H

/// \file ar/definePackageResolver.h
/// Macros for defining a package resolver implementation.

#include "pxr/pxr.h"
#include "pxr/usd/ar/api.h"
#include "pxr/usd/ar/packageResolver.h"

#include "pxr/base/tf/registryManager.h"
#include "pxr/base/tf/type.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \def AR_DEFINE_PACKAGE_RESOLVER
///
/// Performs registrations required for the specified package resolver
/// class to be discovered by Ar's plugin mechanism. This typically would be
/// invoked in the source file defining the resolver class. For example:
///
/// \code
/// // in .cpp file
/// AR_DEFINE_PACKAGE_RESOLVER(CustomPackageResolverClass, ArPackageResolver);
/// \endcode
#ifdef doxygen
#define AR_DEFINE_PACKAGE_RESOLVER(PackageResolverClass, BaseClass1, ...)
#else

#define AR_DEFINE_PACKAGE_RESOLVER(...)         \
TF_REGISTRY_FUNCTION(TfType) {                  \
    Ar_DefinePackageResolver<__VA_ARGS__>();    \
}

class Ar_PackageResolverFactoryBase 
    : public TfType::FactoryBase 
{
public:
    AR_API
    virtual ~Ar_PackageResolverFactoryBase();

    AR_API
    virtual ArPackageResolver* New() const = 0;
};

template <class T>
class Ar_PackageResolverFactory 
    : public Ar_PackageResolverFactoryBase 
{
public:
    virtual ArPackageResolver* New() const override
    {
        return new T;
    }
};

template <class PackageResolver, class ...Bases>
void Ar_DefinePackageResolver()
{
    TfType::Define<PackageResolver, TfType::Bases<Bases...>>()
        .template SetFactory<Ar_PackageResolverFactory<PackageResolver>>();
}

#endif // doxygen

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_AR_DEFINE_PACKAGE_RESOLVER_H
