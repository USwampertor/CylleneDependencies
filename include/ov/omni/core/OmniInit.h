// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief Core header for starting the Omniverse core.
#pragma once

#include "Omni.h"

#include "../../carb/ClientUtils.h"
#include "../../carb/StartupUtils.h"

//! Initializes the omni library along with Carbonite. Ensures that both libraries will be cleaned up upon exit.
//!
//! This macro should be used in `main()`. It creates some objects which will release the framework when they go out of
//! scope.
//!
//! Use this macro in conjunction with @ref OMNI_APP_GLOBALS().
//!
//! For startup, this function calls \ref carb::acquireFrameworkAndRegisterBuiltins() and \ref carb::startupFramework().
//! At a high level, these functions:
//!
//!  - Determines application path from CLI args and env vars (see @ref carb::extras::getAppPathAndName()).
//!  - Sets application path as filesystem root.
//!  - Loads plugins for settings: *carb.settings.plugin*, *carb.dictionary.plugin*, *carb.tokens.plugins* and any
//!    serializer plugin.
//!  - Searches for config file, loads it and applies CLI args overrides.
//!  - Configures logging with config file.
//!  - Loads plugins according to config file.
//!  - Configures default plugins according to config file.
//!  - Starts the default profiler (if loaded).
//!
//! @param ... May be either \a empty (default initialization), `argc, argv` (command-line arguments), or a const-
//! reference to a \ref carb::StartupFrameworkDesc.
#define OMNI_CORE_INIT(...)                                                                                            \
    omni::core::ScopedOmniCore scopedOmniverse_;                                                                       \
    omni::core::ScopedFrameworkStartup scopedFrameworkStartup_{ __VA_ARGS__ };

namespace omni
{
namespace core
{

//! Scoped object which calls @ref OMNI_CORE_START() and @ref OMNI_CORE_STOP().
//!
//! Rather than directly using this object, use @ref OMNI_CORE_INIT().
struct ScopedOmniCore
{
    //! Starts the Carbonite @ref carb::Framework and calls @ref omniCoreStart.
    ScopedOmniCore(const OmniCoreStartArgs* args = nullptr)
    {
        if (!carb::getFramework())
        {
            carb::acquireFrameworkAndRegisterBuiltins(args);
        }
    }

    //! Calls @ref omniCoreStop and tears down the Carbonite @ref carb::Framework.
    ~ScopedOmniCore()
    {
        carb::releaseFrameworkAndDeregisterBuiltins();
    }

private:
    CARB_PREVENT_COPY_AND_MOVE(ScopedOmniCore);
};

//! Scoped object which calls @ref carb::startupFramework() and @ref carb::shutdownFramework().
//!
//! Rather than directly using this object, use @ref OMNI_CORE_INIT().
struct ScopedFrameworkStartup
{
    //! Default constructor which does not startup the framework due to a lack of arguments.
    //!
    //! This constructor is present to make @ref OMNI_CORE_INIT() useful when the application wishes to call @ref
    //! carb::startupFramework() explicitly.
    ScopedFrameworkStartup() : m_startedFramework{ false }
    {
    }

    //! Constructor which passes @p argc and @p argv to @ref carb::startupFramework().
    //!
    //! All other parameters passed to @ref carb::startupFramework() are default values.
    ScopedFrameworkStartup(int argc, char** argv) : m_startedFramework{ true }
    {
        carb::StartupFrameworkDesc startupParams = carb::StartupFrameworkDesc::getDefault();
        startupParams.argv = argv;
        startupParams.argc = argc;
        carb::startupFramework(startupParams);
    }

    //! Constructor which allows specify all parameters to @ref carb::startupFramework().
    ScopedFrameworkStartup(const carb::StartupFrameworkDesc& startupParams) : m_startedFramework{ true }
    {
        carb::startupFramework(startupParams);
    }

    //! Calls @ref carb::shutdownFramework() if a non-default constructor was called.
    ~ScopedFrameworkStartup()
    {
        if (m_startedFramework)
        {
            carb::shutdownFramework();
        }
    }

private:
    CARB_PREVENT_COPY_AND_MOVE(ScopedFrameworkStartup);

private:
    bool m_startedFramework;
};

} // namespace core
} // namespace omni
