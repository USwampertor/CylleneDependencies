// Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include "../Interface.h"
#include "../Types.h"

namespace carb
{
namespace windowing
{

struct GLContext;

/**
 * Defines a GL context interface for off-screen rendering.
 */
struct IGLContext
{
    CARB_PLUGIN_INTERFACE("carb::windowing::IGLContext", 1, 0)

    /**
     * Creates a context for OpenGL.
     *
     * @param width The width of the off-screen surface for the context.
     * @param height The height of the off-screen surface for the context.
     * @return The GL context created.
     */
    GLContext*(CARB_ABI* createContextOpenGL)(int width, int height);

    /**
     * Creates a context for OpenGL(ES).
     *
     * @param width The width of the off-screen surface for the context.
     * @param height The height of the off-screen surface for the context.
     * @return The GL context created.
     */
    GLContext*(CARB_ABI* createContextOpenGLES)(int width, int height);

    /**
     * Destroys a GL context.
     *
     * @param ctx The GL context to be destroyed.
     */
    void(CARB_ABI* destroyContext)(GLContext* ctx);

    /**
     * Makes the GL context current.
     *
     * After calling this you can make any GL function calls.
     *
     * @param ctx The GL context to be made current.
     */
    void(CARB_ABI* makeContextCurrent)(GLContext* ctx);

    /**
     * Try and resolve an OpenGL or OpenGL(es) procedure address from name.
     *
     * @param procName The name of procedure to load.
     * @return The address of procedure.
     */
    void*(CARB_ABI* getProcAddress)(const char* procName);
};

} // namespace windowing
} // namespace carb
