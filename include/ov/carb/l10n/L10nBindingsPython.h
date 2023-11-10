// Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../BindingsPythonUtils.h"
#include "IL10n.h"
#include "L10nUtils.h"

namespace carb
{
namespace l10n
{

inline void definePythonModule(py::module& m)
{
    m.doc() = "pybind11 carb.l10n bindings";

    py::class_<LanguageTable>(m, "LanguageTable");
    py::class_<LanguageIdentifier>(m, "LanguageIdentifier");

    m.def("register_for_client", carb::l10n::registerLocalizationForClient,
          R"(Register the l10n plugin for the current client.

This must be called before using any of the localization plugins, if
carb::startupFramework() has not been called.

This use case is only encountered in the tests. Standard Carbonite applications
call carb::startupFramework() so they should never have to call this.

If this is not called, the localization system will be non-functional.)",
          py::call_guard<py::gil_scoped_release>());

    m.def("deregister_localization_for_client", carb::l10n::deregisterLocalizationForClient,
          R"(Deregister the localization plugin for the current client.

This can be called to deregister the localization plugin for the current client,
if carb::shutdownFramework will not be called.)",
          py::call_guard<py::gil_scoped_release>());

    m.def("get_localized_string",
          [](const char* string) {
              return carb::l10n::getLocalizedString(
                  (g_carbLocalization == nullptr) ? 0 : g_carbLocalization->getHashFromKeyString(string), string);
          },
          R"(Retrieve a string from the localization database, given its hash.


This function retrieves a localized string based on the hash of the keystring.
This should be used on all strings found in the UI, so that they can
automatically be shown in the correct language. Strings returned from this
function should never be cached, so that changing the language at runtime will
not result in stale strings being shown in the UI.

Args:
    string: The keystring that identifies the set of localized strings to
            return.  This will typically correspond to the US English string
            for this UI text.  This string will be returned if there is no
            localization table entry for this key.

Returns:
    The localized string for the input hash in the currently set language, if
    a string exists for that language.

    If no localized string from the currently set language exists for the hash,
    the US English string will be returned.

    If the hash is not found in the localization database, the string parameter
    will be returned. Alternatively, if a config setting is enabled, error
    messages will be returned in this case.)",
          py::call_guard<py::gil_scoped_release>());

    m.def("get_hash_from_key_string",
          [](const char* string) {
              return (g_carbLocalization == nullptr) ? 0 : g_carbLocalization->getHashFromKeyString(string);
          },
          R"(Hash a keystring for localization.

This hashes a keystring that can be looked up with carb_localize_hashed().
Strings must be hashed before passing them into carb_localize_hashed(); this is done
largely so that automated tools can easily find all of the localized strings in
scripts by searching for this function name.

In cases where a string will be looked up many times, it is ideal to cache the
hash returned, so that it is not recalculated excessively.

Args:
    string: The keystring to hash.
            This must be a string.
            This must not be None.

Returns:
    The hash for the string argument.
    This hash can be used in carb_localize_hashed().)",
          py::call_guard<py::gil_scoped_release>());

    m.def("get_localized_string_from_hash",
          [](StringIdentifier id, const char* string) { return carb::l10n::getLocalizedString(id, string); },
          R"(Retrieve a string from the localization database, given its hash.


This function retrieves a localized string based on the hash of the keystring.
This should be used on all strings found in the UI, so that they can
automatically be shown in the correct language. Strings returned from this
function should never be cached, so that changing the language at runtime will
not result in stale strings being shown in the UI.

Args:
    id:     A hash that was previously returned by hash_localization_string().
    string: The keystring that was hashed with hash_localization_string().
            This is passed to ensure that a readable string is returned if
            the hash is not found in the localization table.

Returns:
    The localized string for the input hash in the currently set language, if
    a string exists for that language.

    If no localized string from the currently set language exists for the hash,
    the US English string will be returned.

    If the hash is not found in the localization database, the string parameter
    will be returned. Alternatively, if a config setting is enabled, error
    messages will be returned in this case.)",
          py::arg("id") = 0, py::arg("string") = "{TRANSLATION NOT FOUND}", py::call_guard<py::gil_scoped_release>());
}

} // namespace l10n
} // namespace carb
