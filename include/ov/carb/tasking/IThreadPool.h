// Copyright (c) 2019-2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief IThreadPool definition file.
#pragma once

#include "../Interface.h"

namespace carb
{
namespace tasking
{

/**
 * Opaque handle for a thread pool.
 */
class ThreadPool DOXYGEN_EMPTY_CLASS;

/**
 * Defines the function for performing a user-provided job.
 *
 * @param jobData User provided data for the job, the memory must not be released until it no longer needed by the
 * task.
 */
typedef void (*JobFn)(void* jobData);


/**
 * Optional plugin providing helpful facilities for utilizing a pool of threads to perform basic small tasks.
 *
 * @warning It is not recommended to use IThreadPool in conjunction with ITasking; the latter is a much richer feature
 * set and generally preferred over IThreadPool. IThreadPool is a simple thread pool with the ability to run individual
 * tasks.
 *
 * @warning If multiple ThreadPool objects are used, caution must be taken to not overburden the system with too many
 * threads.
 *
 * @note Prefer using ThreadPoolWrapper.
 */
struct IThreadPool
{
    CARB_PLUGIN_INTERFACE("carb::tasking::IThreadPool", 1, 0)

    /**
     * Creates a new thread pool where the number of worker equals to the number specified by the user.
     *
     * @param workerCount Required number of worker threads.
     *
     * @return A newly created thread pool.
     */
    ThreadPool*(CARB_ABI* createEx)(size_t workerCount);

    /**
     * Creates a new thread pool where the number of worker equals to a value
     * returned by the "getDefaultWorkerCount" function.
     *
     * @return A newly created thread pool.
     */
    ThreadPool* create() const;

    /**
     * Destroys previously created thread pool.
     *
     * @param threadPool Previously created thread pool.
     */
    void(CARB_ABI* destroy)(ThreadPool* threadPool);

    /**
     * Returns default number of workers used for creation of a new thread pool.
     *
     * @return The default number of workers.
     */
    size_t(CARB_ABI* getDefaultWorkerCount)();

    /**
     * Returns the number of worker threads in the thread pool.
     *
     * @param threadPool ThreadPool previously created with create().
     * @returns The number of worker threads.
     */
    size_t(CARB_ABI* getWorkerCount)(ThreadPool* threadPool);

    /**
     * Adds a new task to be executed by the thread pool.
     *
     * @param threadPool Thread pool for execution of the job.
     * @param jobFunction User provided function to be executed by a worker.
     * @param jobData User provided data for the job, the memory must not be released until it no longer needed by the
     * task.
     *
     * @return Returns true if the task was successfully added into the thread pool.
     */
    bool(CARB_ABI* enqueueJob)(ThreadPool* threadPool, JobFn jobFunction, void* jobData);

    /**
     * Returns the number of currently executed tasks in the thread pool.
     *
     * @param threadPool Thread pool to be inspected.
     *
     * @return The number of currently executed tasks in the thread pool.
     */
    size_t(CARB_ABI* getCurrentlyRunningJobCount)(ThreadPool* threadPool);

    /**
     * Blocks execution of the current thread until the thread pool finishes all queued jobs.
     *
     * @param threadPool Thread pool to wait on.
     */
    void(CARB_ABI* waitUntilFinished)(ThreadPool* threadPool);
};

inline ThreadPool* IThreadPool::create() const
{
    return createEx(getDefaultWorkerCount());
}

} // namespace tasking
} // namespace carb
