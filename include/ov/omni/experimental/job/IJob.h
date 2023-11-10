// Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//

//! @file
//!
//! @brief IJob definition file.
#pragma once

#include "../../../carb/Interface.h"
#include "../../core/IObject.h"
#include "../../../carb/IObject.h"

namespace omni
{
/** Namespace for experimental Interfaces and functionality. */
namespace experimental
{
namespace job
{

/**
 * Defines the function for performing a user-provided job.
 *
 * @param job_data User provided data for the job, the memory must not be released until it no longer needed by the
 * task.
 */
using JobFunction = void (*)(void* job_data);

/** Forward declaration of the IAffinityMask interface. */
OMNI_DECLARE_INTERFACE(IAffinityMask);

/**
 * Alias for an affinity mask.
 */
using MaskType = uint64_t;

/**
 * Interface for providing a CPU affinity mask to the plugin. Instances of this interface can be thought of as an array
 * of \c MaskType values, which allows for setting affinities on machines with more than 64 processors. Each affinity
 * mask this object contains is a bitmask that represents the associated CPUs.
 *
 * On Linux, this object is treated as one large bitset analogous to cpu_set_t. So \c get_affinity_mask(0) represents
 * CPUs 0-63, \c get_affinity_mask(1) represents CPUs 64-127, etc.
 *
 * On Windows, each affinity mask in this object applies to its own Processor Group, so \c get_affinity_mask(0) is for
 * Processor Group 0, \c get_affinity_mask(1) for Processor Group 1, etc.
 */
class IAffinityMask_abi
    : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.experimental.job.IAffinityMask")>
{
protected:
    /**
     * Gets the affinity mask at \c index.
     *
     * @note \c index must be less than \ref get_mask_count_abi()
     *
     * @param index Index to get affinity mask for.
     *
     * @return The affinity mask at the provided index.
     */
    virtual MaskType get_affinity_mask_abi(size_t index) noexcept = 0;

    /**
     * Gets the affinity \c mask at \c index.
     *
     * @note \c index must be less than \ref get_mask_count_abi()
     *
     * @param index Index to set affinity mask for.
     * @param mask Mask to set.
     */
    virtual void set_affinity_mask_abi(size_t index, MaskType mask) noexcept = 0;

    /**
     * Gets the current number of affinity masks stored by this object.
     *
     * @return The current number of affinity masks stored by this object.
     */
    virtual size_t get_mask_count_abi() noexcept = 0;

    /**
     * Gets the default number of affinity masks stored by this object.
     *
     * @return The default number of affinity masks stored by this object.
     */
    virtual size_t get_default_mask_count_abi() noexcept = 0;

    /**
     * Sets the number of affinity masks stored by this object to \c count.
     *
     * If \c count is greater than the current size, the appended affinity masks will bet set to \c 0. If \c count
     * is less than the current size, then this object will only contain the first \c count elements after this call.
     *
     * @param count Number of affinity masks to set the size to.
     */
    virtual void set_mask_count_abi(size_t count) noexcept = 0;
};


/** Forward declaration of the IJob interface. */
OMNI_DECLARE_INTERFACE(IJob);
/** Forward declaration of the IJobWorker interface. */
OMNI_DECLARE_INTERFACE(IJobWorker);
/** Forward declaration of the IJobAffinity interface. */
OMNI_DECLARE_INTERFACE(IJobAffinity);

/**
 * Basic interface for launching jobs on a foreign job system.
 */
class IJob_abi : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.experimental.job.IJob")>
{
protected:
    /**
     * Adds a new job to be executed.
     *
     * @param job_fn User provided function to be executed by a worker.
     * @param job_data User provided data for the job, the memory must not be released until it no longer needed by the
     * task.
     */
    virtual void enqueue_job_abi(JobFunction job_fn, OMNI_ATTR("in, out") void* job_data) noexcept = 0;
};

/**
 * Interface for managing the number of workers in the job system.
 */
class IJobWorker_abi : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.experimental.job.IJobWorker")>
{
protected:
    /**
     * Returns default number of workers used for creation of a new Job system.
     *
     * @return The default number of workers.
     */
    virtual size_t get_default_worker_count_abi() noexcept = 0;

    /**
     * Returns the number of worker threads in the job system.
     *
     * @returns The number of worker threads.
     */
    virtual size_t get_worker_count_abi() noexcept = 0;

    /**
     * Sets the number of workers in the job system.
     *
     * This function may stop all current threads and reset any previously set thread affinity.
     *
     * @param count The new number of workers to set in the system. A value of 0 means to use the default value returned
     * by getDefaultWorkerCount()
     */
    virtual void set_worker_count_abi(size_t count) noexcept = 0;
};

/**
 * Interface for setting CPU affinity for the job system.
 */
class IJobAffinity_abi
    : public omni::core::Inherits<omni::core::IObject, OMNI_TYPE_ID("omni.experimental.job.IJobAffinity")>
{
protected:
    /**
     * Gets the current affinity of a worker.
     *
     * @param worker_id The worker to set the affinity of. If this id is larger than the current number of workers,
     * a \c nullptr will be returned.
     *
     * @return The current affinity being used by the worker. The returned value may be \c nullptr if the worker's
     * affinity could not be determined.
     */
    virtual IAffinityMask* get_affinity_abi(size_t worker_id) noexcept = 0;

    /**
     * Attempts to set the affinity for the specified worker.
     *
     * @note  On Windows each thread can only belong to a single Processor Group, so the CPU Affinity will only be set
     * to the first non-zero entry. That is to say, if both \c mask->get_affinity_mask(0) and
     * \c mask->get_affinity_mask(1) both have bits sets, only the CPUs in \c mask->get_affinity_mask(0) will be set for
     * the affinity.
     *
     * @param worker_id The worker to set the affinity of. If this id is larger than the current number of workers,
     * false will be returned.
     * @param mask The affinity values to set.
     *
     * @return true if the affinity was successfully set, false otherwise.
     */
    virtual bool set_affinity_abi(size_t worker_id, OMNI_ATTR("not_null") IAffinityMask* mask) noexcept = 0;
};

} // namespace job
} // namespace experimental
} // namespace omni

#include "IJob.gen.h"
