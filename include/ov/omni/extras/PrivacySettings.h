// Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
/** @file
 *  @brief Helper class to retrieve the current privacy settings state.
 */
#pragma once

#include "../../carb/InterfaceUtils.h"
#include "../../carb/settings/ISettings.h"
#include "../../carb/extras/StringSafe.h"


namespace omni
{
/** common namespace for extra helper functions and classes. */
namespace extras
{

/** Consent level names.  These consent levels control which types of structured log events
 *  produced by an app will be sent to telemetry servers for analysis.  Each consent
 *  level will default to `false` before the privacy settings have been loaded.
 */
enum class ConsentLevel
{
    /** Privacy consent level that corresponds to the @ref PrivacySettings::kPerformanceKey
     *  setting name.  This consent level controls whether events such as hardware information,
     *  app performance statistics, resource usage levels, or crash reports will be sent to
     *  telemetry servers for analysis.
     */
    ePerformance,

    /** Privacy consent level that corresponds to the @ref PrivacySettings::kPersonalizationKey
     *  setting name.  This consent level controls whether events such as user app settings,
     *  window layouts, search keywords, etc will be sent to telemetry servers for analysis.
     */
    ePersonalization,

    /** Privacy consent level that corresponds to the @ref PrivacySettings::kUsageKey setting
     *  name.  This consent level controls whether events such as user activity, app feature
     *  usage, extension usage, etc will be sent to the telemetry servers for analysis.
     */
    eUsage,

    /** The total number of available consent levels.  This is not a valid consent level to
     *  query and will always return `false`.
     */
    eCount,
};

/** Static helper class to provide standardized access to the telemetry privacy setting values.
 *  These settings provide information such as the active user ID and the consent permissions
 *  for the various event types.  These settings are expected to already have been loaded into
 *  the settings registry (accessible through ISettings).  This just provides simple access
 *  to the settings values but does not provide functionality for loading the settings in the
 *  first place or providing a wrapper for change subscriptions on those settings.
 *
 *  Loading the settings is left up to the `omni.structuredlog.plugin` module.  This module will
 *  manage loading the settings when it initializes.  It will also refresh the settings any
 *  time the original settings file for them changes.  This file is located at
 *  `$HOME_PATH/.nvidia-omniverse/config/privacy.toml` (where `$HOME_PATH` is the user's home
 *  directory on the local system).
 *
 *  This helper class does not directly handle change subscriptions to the various privacy
 *  settings.  This is done to avoid complication and potential issues during shutdown
 *  of global objects.  It does however expose the paths of the various privacy settings
 *  so that external callers can subscribe for and manage their own change notifications.
 *
 *  @note The methods in this helper class are just as thread safe as the ISettings interface
 *        itself is when retrieving a value.  All methods are static so there is no state
 *        data that could be clobbered by multiple threads in this object itself.
 *
 *  @note The Carbonite framework must be initialized before using this helper class.  A
 *        crash will occur if the framework is not initialized first.
 */
class PrivacySettings
{
public:
    /** @addtogroup keySettings Privacy Settings Keys
     *
     *  Paths for the various expected privacy settings.  These can be used to manually
     *  access the settings values or to subscribe for change notifications on either
     *  each individual setting or the entire privacy settings tree.
     *  @{
     */
    /** The settings key path for the version of the privacy settings file. */
    static constexpr const char* kVersionKey = "/privacy/version";

    /** The settings key path for the 'performance' consent level. */
    static constexpr const char* kPerformanceKey = "/privacy/performance";

    /** The settings key path for the 'personalization' consent level. */
    static constexpr const char* kPersonalizationKey = "/privacy/personalization";

    /** The settings key path for the 'usage' consent level. */
    static constexpr const char* kUsageKey = "/privacy/usage";

    /** The settings key path for the current user ID name. */
    static constexpr const char* kUserIdKey = "/privacy/userId";

    /** The settings key path for the current user's email address.  This is not
     *  expected to be present for all users.
     */
    static constexpr const char* kEmailKey = "/privacy/email";

    /** The settings key path for the 'external build' flag. */
    static constexpr const char* kExternalBuildKey = "/privacy/externalBuild";

    /** The settings key path for the 'send extra diagnostic data' flag. */
    static constexpr const char* kExtraDiagnosticDataOptInKey = "/privacy/extraDiagnosticDataOptIn";

    /** The settings key path for all of the privacy settings tree. */
    static constexpr const char* kSettingTree = "/privacy";
    /** @} */

    /** Retrieves the version setting found in the privacy config.
     *
     *  @returns A string containing the version information value for the privacy settings
     *           file.  This version gives an indication of what other values might be
     *           expected in the file.
     *
     *  @thread_safety This call is thread safe.
     */
    static const char* getVersion()
    {
        return _getSettingStringValue(kVersionKey, "1.0");
    }

    /** Retrieves the user ID found in the privacy config.
     *
     *  @returns A string containing the user ID that is currently logged into omniverse.
     *           This is the user ID that should be used when writing out any structured
     *           log events or sending crash reports.  This does not necessarily reflect the
     *           user ID that will be used to login to a Nucleus server instance however.
     *  @returns An empty string if no user ID setting is currently present.
     *
     *  @thread_safety This call is thread safe.
     */
    static const char* getUserId()
    {
        return _getSettingStringValue(kUserIdKey, "");
    }

    /** Retrieves the user email address if specified in the privacy config.
     *
     *  @returns A string containing the currently logged in user's email address.  This
     *           is only expected to be present for some users.  This email address will
     *           not be emitted with any structured log events.
     *  @returns An empty string if no user email address is currently present.
     *
     *  @thread_safety This call is thread safe.
     */
    static const char* getEmail()
    {
        return _getSettingStringValue(kEmailKey, "");
    }

    /** Retrieves the consent state for a requested consent level.
     *
     *  @param[in] level    The consent level to query.
     *  @returns The current state of the requested consent level if present.
     *  @returns `false` if the state of the requested consent level could not be successfully
     *           queried.
     *
     *  @thread_safety This call is thread safe.
     */
    static bool getConsentLevel(ConsentLevel level)
    {
        static const char* map[] = { kPerformanceKey, kPersonalizationKey, kUsageKey };


        if (((size_t)level) >= ((size_t)ConsentLevel::eCount))
            return false;

        return _getSettingBoolValue(map[(size_t)level], false);
    }

    /** Checks whether the user has opted into sending diagnostic data.
     *
     *  @returns `true` if the user has opted into sending diagnostic data.
     *  @returns `false` if the user has opted out of sending diagnostic data.
     */
    static bool canSendExtraDiagnosticData()
    {
        const char* optIn = _getSettingStringValue(kExtraDiagnosticDataOptInKey, "");

        return carb::extras::compareStringsNoCase(optIn, "externalBuilds") == 0;
    }

private:
    /** Attempts to retrieve the ISettings interface.
     *
     *  @returns The ISettings interface object if loaded and successfully acquired.
     *  @returns `nullptr` if the ISettings interface object could not be acquired or has not
     *           already been loaded.
     *
     *  @note This does not attempt to load the ISettings plugin itself.  It is assumed that
     *        an external caller will have already loaded the appropriate module before calling
     *        into here.  However, if acquiring the interface object does fail, all calls that
     *        use it will still gracefully fail.
     *
     *  @thread_safety This call is thread safe.
     */
    static carb::settings::ISettings* _getSettingsInterface()
    {
        return carb::getCachedInterface<carb::settings::ISettings>();
    }

    /** Attempts to retrieve a setting as a string value.
     *
     *  @param[in] name         The name of the setting to retrieve the value for.  This may
     *                          not be `nullptr`.
     *  @param[in] defaultValue The default value to return in case the setting cannot be
     *                          returned for any reason.  This value is not interpreted or
     *                          accessed in any way except to return it to the caller in
     *                          failure cases.
     *  @returns The value of the requested setting if it is present and accessible as a string.
     *  @returns The @p defaultValue parameter if the ISettings interface object could not be
     *           acquired or is not loaded.
     *  @returns The @p defaultValue parameter if the requested setting is not accessible as a
     *           string value.
     *
     *  @thread_safety This call is thread safe.
     */
    static const char* _getSettingStringValue(const char* name, const char* defaultValue)
    {
        carb::settings::ISettings* settings = _getSettingsInterface();
        const char* value;

        if (settings == nullptr || name == nullptr)
            return defaultValue;

        value = settings->getStringBuffer(name);

        if (value == nullptr)
            return defaultValue;

        return value;
    }

    /** Attempts to retrieve a setting as a boolean value.
     *
     *  @param[in] name         The name of the setting to retrieve the value for.  This may
     *                          not be `nullptr`.
     *  @param[in] defaultValue The default value to return in case the setting cannot be
     *                          returned for any reason.  This value is not interpreted or
     *                          accessed in any way except to return it to the caller in
     *                          failure cases.
     *  @returns The value of the requested setting if it is present and accessible as a boolean.
     *  @returns The @p defaultValue parameter if the ISettings interface object could not be
     *           acquired or is not loaded.
     *  @returns The @p defaultValue parameter if the requested setting is not accessible as a
     *           string value.
     *
     *  @thread_safety This call is thread safe.
     */
    static bool _getSettingBoolValue(const char* name, bool defaultValue)
    {
        carb::settings::ISettings* settings = _getSettingsInterface();


        if (settings == nullptr || !settings->isAccessibleAs(carb::dictionary::ItemType::eBool, name))
            return defaultValue;

        return settings->getAsBool(name);
    }
};

} // namespace extras
} // namespace omni
