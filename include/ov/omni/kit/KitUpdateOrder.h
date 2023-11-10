// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include <carb/events/IEvents.h>

// Kit's event subscribers order numbers, which control
// the execution order flow of the application were previously
// magic values scattered across dozens of extensions
// across multiple repros. This centralizes it for now.

// This file defines a ordering that can be used to
// by extensions to have a deterministic execution flow

namespace omni
{
namespace kit
{
namespace update
{

enum PreUpdateOrdering : int32_t
{
    eTelmetryInitialize = -50, // Typically the first item in a frame to begin profile capture
    eFabricBeginFrame = -20,
    eUnspecifiedPreUpdateOrder = carb::events::kDefaultOrder
};

enum UpdateOrdering : int32_t
{
    eCheckForHydraRenderComplete = -100, // Checks for HydraEngine::render completion on GPU
                                         //      1. Pushes StageRenderingEvent::NewFrame
                                         //      2. triggers renderingEventStream::pump
    eUsdTimelineStateRefresh = -90, // Applies pending Timeline state changes
    ePythonAsyncFutureBeginUpdate = -50, // asyncio.Future blocked awaiting
                                         //      1. IApp.next_pre_update_async
                                         //      2. UsdContext.next_frame_async / next_usd_async
    ePythonExecBeginUpdate = -45, // Enables execution of all python blocked by ePythonAsyncFutureBeginUpdate
                                  // Enable python execution blocked awaiting UsdContext::RenderingEventStream::Pump()
    eOmniClientUpdate = -30, // Run OmniClient after python but before main simulation
    eUsdTimelineUpdate = -20, // ITimeline wants to execute before eUsdContextUpdate
    eUsdContextUpdate = -10, // Core UsdUpdate execution
                             //      1. Update liveModeUpdate listeners
                             //      2. triggers stageEventStream::pump
                             //      3. MaterialWatcher::update
                             //      4. IHydraEngine::setTime
                             //      5. triggers IUsdStageUpdate::pump (see IUsdStageEventOrdering below)
                             //      6. AudioManager::update

    // extras::SettingWrapper is hardcoded to kDefaultOrder
    // which means this is when during the main update cycle,
    // event listeners for settings changes events will fire
    // There are a mininum of 60+ unique setting subscription listeners
    // in default kit session
    eUnspecifiedUpdateOrder = carb::events::kDefaultOrder,
    eUIRendering = 15, // Trigger UI/ImGui Drawing
    eFabricFlush = 20, // Fabric Flush after eUsdContextUpdate
    eHydraRendering = 30, // Triggers HydraEngine::render
    ePythonAsyncFutureEndUpdate = 50, // asyncio.Future blocked awaiting
                                      //      1. IApp.next_update_async (legacy)
    ePythonExecEndUpdate = 100, // Enables execution of all python blocked by ePythonAsyncFutureEndUpdate
                                // Enable python execution blocked awaiting UsdContext::StageEventStream::Pump()
};

/*
 * IApp.next_update_async() is the original model of python scripting kit, a preferrred
 * approach is either App.pre_update_async() and/or App.post_update_async(). Both can be used inside
 * a single app tick, pseduo python code:
 *       While true:
 *           await omni.kit.app.get_app().pre_update_async()
 *           # Do a bunch of python scripting things before USD Update or hydra rendering is scheduled
 *           await omni.kit.app.get_app().post_update_async()
 *           # Do a bunch of python scripting things after USD Update & hydra rendering has been scheduled
 *
 * Alternatively use either just pre_update_async() or post_update_async() in python depending on whether you want your
 * script to execute before USDUpdate or after.
 */

enum PostUpdateOrdering : int32_t
{
    eReleasePrevFrameGpuResources = -50,
    ePythonAsyncFuturePostUpdate = -25, // asyncio.Future blocked awaiting
                                        //      1. IApp.post_update_async (deprecates IApp.next_update_async)
    ePythonExecPostUpdate = -10, // Enables execution of all python blocked by ePythonAsyncFuturePostUpdate
    eUnspecifiedPostUpdateOrder = carb::events::kDefaultOrder,
    eKitAppFactoryUpdate = eUnspecifiedPostUpdateOrder,
    eKitAppOSUpdate = eUnspecifiedPostUpdateOrder,
    eKitInternalUpdate = 100
};

#pragma push_macro("min")
#undef min

enum KitUsdStageEventOrdering : int32_t
{
    eKitUsdFileStageEvent = std::numeric_limits<carb::events::Order>::min() + 1,
    eKitUnspecifiedUsdStageEventOrder = carb::events::kDefaultOrder,
};

#pragma pop_macro("min")

enum IUsdStageEventOrdering : int32_t
{
    eIUsdStageUpdateAnimationGraph = 0, // Hard-coded in separate non kit-sdk repro (gitlab)
    eIUsdStageUpdatePinocchioPrePhysics = 8, // Hard-coded in separate non kit-sdk repro (gitlab)
    eIUsdStageUpdateTensorPrePhysics = 9, // Hard-coded in separate non kit-sdk repro (perforce)
    eIUsdStageUpdateForceFieldsPrePhysics = 9, // Hard-coded in separate non kit-sdk repro (perforce)
    eIUsdStageUpdatePhysicsVehicle = 9, // Hard-coded in separate non kit-sdk repro (perforce)
    eIUsdStageUpdatePhysicsCCT = 9, // Hard-coded in separate non kit-sdk repro (perforce)
    eIUsdStageUpdatePhysicsCameraPrePhysics = 9, // Hard-coded in separate non kit-sdk repro (perforce)
    eIUsdStageUpdatePhysics = 10, // Hard-coded in separate non kit-sdk repro (perforce)
    eIUsdStageUpdateFabricPostPhysics = 11, // Hard-coded in separate non kit-sdk repro (perforce)
    eIUsdStageUpdateVehiclePostPhysics = 12, // Hard-coded in separate non kit-sdk repro (perforce)
    eIUsdStageUpdateForceFieldsPostPhysics = 12, // Hard-coded in separate non kit-sdk repro (perforce)
    eIUsdStageUpdatePhysicsCameraPostPhysics = 12, // Hard-coded in separate non kit-sdk repro (perforce)
    eIUsdStageUpdatePhysicsUI = 12, // Hard-coded in separate non kit-sdk repro (perforce)
    eIUsdStageUpdatePinocchioPostPhysics = 13, // Hard-coded in separate non kit-sdk repro (gitlab)
    eIUsdStageUpdateOmnigraph = 100, // Defined inside kit-sdk
    eIUsdStageUpdatePhysxFC = 102, // Hard-coded in separate non kit-sdk repro (perforce)
    eIUsdStageUpdateDebugDraw = 1000, // Defined inside kit-sdk
    eIUsdStageUpdateLast = 2000 // Must always be last when all prior StageUpdate events have been handled
};

} // namespace update
} // namespace kit
} // namespace omni
