// Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Interface.h"

namespace carb
{
namespace scripting
{

/**
 * Defines a scripting return code.
 */
enum class ExecutionErrorCode
{
    eOk,
    eError
};

using OutputFlags = uint32_t;
const OutputFlags kOutputFlagCaptureStdout = 1;
const OutputFlags kOutputFlagCaptureStderr = (1 << 1);

struct ExecutionError
{
    ExecutionErrorCode code;
    const char* message;
};

/**
 * Scripting plugin description
 */
struct ScriptingDesc
{
    const char* languageName;
    const char* const* fileExtensions; ///! File extensions list, each prefixed with period. E.g. ".py"
    size_t fileExtensionCount;
};

/**
 * Context of execution.
 *
 * Context keeps all shared data between executions. For example global state (variables, functions etc.) in python.
 */
struct Context;

/**
 * Script is an execution unit.
 *
 * By making a Script from a code you give an opportunity to plugin to preload and compile the code once.
 */
struct Script;


/**
 * Opaque container to pass and retrieve data with Scripting interface.
 */
struct Object;

/**
 * Defines a generic scripting interface.
 *
 * Specific implementations such as Python realize this simple interface
 * and allow access to the Carbonite framework through run-time scripts and not
 * just compiled code.
 */
struct IScripting
{
    CARB_PLUGIN_INTERFACE("carb::scripting::IScripting", 1, 0)

    /**
     * Add raw module search path, this path will be added to the list unmodified, potentially requiring
     * language-specific search patterns.
     */
    void(CARB_ABI* addSearchPath)(const char* path);
    /**
     * Remove raw module search path.
     */
    void(CARB_ABI* removeSearchPath)(const char* path);

    /**
     * Create an execution context.
     *
     * Context:
     * 1. Keeps execution result: errors, stdout, stderr.
     * 2. Stores globals between execution calls.
     */
    Context*(CARB_ABI* createContext)();

    /**
     * Destroy an execution context.
     */
    void(CARB_ABI* destroyContext)(Context* context);

    /**
     * Get a global execution context.
     * This context uses interpreter global state for globals.
     */
    Context*(CARB_ABI* getGlobalContext)();

    /**
     * Execute code from a file on a Context.
     *
     * @return false iff execution produced an error. Use getLastExecutionError(context) to get more details.
     */
    bool(CARB_ABI* executeFile)(Context* context, const char* path, OutputFlags outputCaptureFlags);

    /**
     * Execute a string of code on a Context.
     *
     * @param sourceFile Set as __file__ in python. Can be nullptr, will default to executable name then.
     *
     * @return false iff execution produced an error. Use getLastExecutionError(context) to get more details.
     */
    bool(CARB_ABI* executeString)(Context* context, const char* str, OutputFlags outputCaptureFlags, const char* sourceFile);

    /**
     * Execute a Script on a Context.
     *
     * @return false iff execution produced an error. Use getLastExecutionError(context) to get more details.
     */
    bool(CARB_ABI* executeScript)(Context* context, Script* script, OutputFlags outputCaptureFlags);

    /**
     * Execute a Script with arguments on a Context.
     *
     * @return false iff execution produced an error. Use getLastExecutionError(context) to get more details.
     */
    bool(CARB_ABI* executeScriptWithArgs)(
        Context* context, Script* script, const char* const* argv, size_t argc, OutputFlags outputCaptureFlags);

    /**
     * Check if context has a function.
     */
    bool(CARB_ABI* hasFunction)(Context* context, const char* functionName);

    /**
     * Execute a function on a Context.
     *
     * @param returnObject Pass an object to store returned data, if any. Can be nullptr.
     * @return false iff execution produced an error. Use getLastExecutionError(context) to get more details.
     */
    bool(CARB_ABI* executeFunction)(Context* context,
                                    const char* functionName,
                                    Object* returnObject,
                                    OutputFlags outputCaptureFlags);

    /**
     * Check if object has a method
     */
    bool(CARB_ABI* hasMethod)(Context* context, Object* self, const char* methodName);

    /**
     * Execute Object method on a Context.
     *
     * @param returnObject Pass an object to store returned data, if any. Can be nullptr.
     * @return false iff execution produced an error. Use getLastExecutionError(context) to get more details.
     */
    bool(CARB_ABI* executeMethod)(
        Context* context, Object* self, const char* methodName, Object* returnObject, OutputFlags outputCaptureFlags);

    /**
     * Get last stdout, if stdout from the execute within the given context.
     */
    const char*(CARB_ABI* getLastStdout)(Context* context);

    /**
     * Get last stderr, if stdout from the execute within the given context.
     */
    const char*(CARB_ABI* getLastStderr)(Context* context);

    /**
     * Get last execution error from last execute within the given context call if any.
     */
    const ExecutionError&(CARB_ABI* getLastExecutionError)(Context* context);

    /**
     * Create a script instance from an explicit string.
     *
     * @param str The full script as a string.
     * @return The script.
     */
    Script*(CARB_ABI* createScriptFromString)(const char* str);

    /**
     * Create a script instance from a file path.
     *
     * @param path The file path such as "assets/scripts/hello.py"
     * @return The script.
     */
    Script*(CARB_ABI* createScriptFromFile)(const char* path);

    /**
     * Destroys the script and releases all resources from a previously created script.
     *
     * @param script The previously created script.
     */
    void(CARB_ABI* destroyScript)(Script* script);

    /**
     * Create an object to hold scripting data.
     */
    Object*(CARB_ABI* createObject)();

    /**
     * Destroy an object.
     */
    void(CARB_ABI* destroyObject)(Object* object);

    /**
     * Is object empty?
     */
    bool(CARB_ABI* isObjectNone)(Object* object);

    /**
     * Get a object data as string.
     *
     * Returned string is internally buffered and valid until next call.
     * If an object is not of a string type, nullptr is returned.
     */
    const char*(CARB_ABI* getObjectAsString)(Object* object);

    /**
     * Get a object data as integer.
     *
     * If an object is not of an int type, 0 is returned.
     */
    int(CARB_ABI* getObjectAsInt)(Object* object);

    /**
     * Gets the Scripting plugin desc.
     *
     * @return The desc.
     */
    const ScriptingDesc&(CARB_ABI* getDesc)();

    /**
     * Collects all plugin folders (by asking the Framework), appends language specific subfolder and adds them to
     * search path.
     */
    void(CARB_ABI* addPluginBindingFoldersToSearchPath)();

    /**
     * Temp helper to scope control GIL
     */
    void(CARB_ABI* releaseGIL)();
    void(CARB_ABI* acquireGIL)();
};
} // namespace scripting
} // namespace carb
