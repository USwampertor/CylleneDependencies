// Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once
#include <carb/IObject.h>
#include <carb/Interface.h>


namespace omni
{

namespace kit
{

class RunLoop;

/**
 * Interface to implement by custom run loop runners.
 *
 * `IApp` calls function on this interface it it was set, using `IApp::setRunLoopRunner`.
 */
class IRunLoopRunner : public carb::IObject
{
public:
    /**
     * Called once before starting application update.
     */
    virtual void startup() = 0;

    /**
     * Called each time new run loop was created (usually when someone requests new run loop). This function can both be
     * called before and after startup, so it is a runner responsibility to handle adding new run loops and spinning
     * them.
     *
     * Can be called from different threads.
     */
    virtual void onAddRunLoop(const char* name, RunLoop* loop) = 0;

    /**
     * Called when IApp wants to remove a run loop
     * Set block=true for onRemoveRunLoop to block until loop has completed
     */
    virtual void onRemoveRunLoop(const char* name, RunLoop* loop, bool block) = 0;

    /**
     * Called each application update until application is not quiting (`IApp::isRunning()` is true).
     */
    virtual void update() = 0;

    /**
     * Called once before shutting down an application, when `IApp::isRunning()` is false.
     */
    virtual void shutdown() = 0;
};

using IRunLoopRunnerPtr = carb::ObjectPtr<IRunLoopRunner>;


} // namespace kit
} // namespace omni
