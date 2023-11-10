// Copyright (c) 2019-2023, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
//! @file
//!
//! @brief ThreadPoolWrapper definition file.
#pragma once

#include "../cpp17/Tuple.h"
#include "../logging/Log.h"
#include "IThreadPool.h"

#include <future>

namespace carb
{
namespace tasking
{

#ifndef DOXYGEN_BUILD
namespace detail
{

template <class ReturnType>
struct ApplyWithPromise
{
    template <class Callable, class Tuple>
    void operator()(std::promise<ReturnType>& promise, Callable&& f, Tuple&& t)
    {
        promise.set_value(std::forward<ReturnType>(cpp17::apply(std::forward<Callable>(f), std::forward<Tuple>(t))));
    }
};

template <>
struct ApplyWithPromise<void>
{
    template <class Callable, class Tuple>
    void operator()(std::promise<void>& promise, Callable& f, Tuple&& t)
    {
        cpp17::apply(std::forward<Callable>(f), std::forward<Tuple>(t));
        promise.set_value();
    }
};

} // namespace detail
#endif

/**
 * Helper class for using IThreadPool API
 */
class ThreadPoolWrapper
{
public:
    /**
     * Constructor
     *
     * @param poolInterface The acquired IThreadPool interface.
     * @param workerCount (optional) The number of worker threads to create. If 0 (default) is specified, the value
     * returned from IThreadPool::getDefaultWorkerCount() is used.
     */
    ThreadPoolWrapper(IThreadPool* poolInterface, size_t workerCount = 0) : m_interface(poolInterface)
    {
        if (m_interface == nullptr)
        {
            CARB_LOG_ERROR("IThreadPool interface used to create a thread pool wrapper is null.");
            return;
        }

        if (workerCount == 0)
        {
            workerCount = m_interface->getDefaultWorkerCount();
        }

        m_pool = m_interface->createEx(workerCount);
        if (m_pool == nullptr)
        {
            CARB_LOG_ERROR("Couldn't create a new thread pool.");
        }
    }

    /**
     * Returns the number of worker threads in the thread pool.
     * @returns The number of worker threads.
     */
    size_t getWorkerCount() const
    {
        if (!isValid())
        {
            CARB_LOG_ERROR("Attempt to call the 'getWorkerCount' method of an invalid thread pool wrapper.");
            return 0;
        }

        return m_interface->getWorkerCount(m_pool);
    }

    /**
     * Enqueues a <a href="https://en.cppreference.com/w/cpp/named_req/Callable">Callable</a> to run on a worker thread.
     *
     * @param task The callable object. May be a lambda, [member] function, functor, etc.
     * @param args Optional <a href="https://en.cppreference.com/w/cpp/utility/functional/bind">std::bind</a>-style
     * arguments to pass to the callable object.
     * @returns A <a href="https://en.cppreference.com/w/cpp/thread/future">std::future</a> based on the return-type of
     * the callable object. If enqueuing failed, `valid()` on the returned future will be false.
     */
    template <class Callable, class... Args>
    auto enqueueJob(Callable&& task, Args&&... args)
    {
        using ReturnType = typename cpp17::invoke_result_t<Callable, Args...>;
        using Future = std::future<ReturnType>;
        using Tuple = std::tuple<std::decay_t<Args>...>;

        struct Data
        {
            std::promise<ReturnType> promise{};
            Callable f;
            Tuple args;
            Data(Callable&& f_, Args&&... args_) : f(std::forward<Callable>(f_)), args(std::forward<Args>(args_)...)
            {
            }
            void callAndDelete()
            {
                detail::ApplyWithPromise<ReturnType>{}(promise, f, args);
                delete this;
            }
        };

        if (!isValid())
        {
            CARB_LOG_ERROR("Attempt to call the 'enqueueJob' method of an invalid thread pool wrapper.");
            return Future{};
        }

        Data* pData = new (std::nothrow) Data{ std::forward<Callable>(task), std::forward<Args>(args)... };
        if (!pData)
        {
            CARB_LOG_ERROR("ThreadPoolWrapper: No memory for job");
            return Future{};
        }

        Future result = pData->promise.get_future();
        if (CARB_LIKELY(m_interface->enqueueJob(
                m_pool, [](void* userData) { static_cast<Data*>(userData)->callAndDelete(); }, pData)))
        {
            return result;
        }

        CARB_LOG_ERROR("ThreadPoolWrapper: failed to enqueue job");
        delete pData;
        return Future{};
    }

    /**
     * Returns the number of jobs currently enqueued or executing in the ThreadPool.
     *
     * enqueueJob() increments this value and the value is decremented as jobs finish.
     *
     * @note This value changes by other threads and cannot be read atomically.
     *
     * @returns The number of jobs currently executing in the ThreadPool.
     */
    size_t getCurrentlyRunningJobCount() const
    {
        if (!isValid())
        {
            CARB_LOG_ERROR("Attempt to call the 'getCurrentlyRunningJobCount' method of an invalid thread pool wrapper.");
            return 0;
        }

        return m_interface->getCurrentlyRunningJobCount(m_pool);
    }

    /**
     * Blocks the calling thread until all enqueued tasks have completed.
     */
    void waitUntilFinished() const
    {
        if (!isValid())
        {
            CARB_LOG_ERROR("Attempt to call the 'waitUntilFinished' method of an invalid thread pool wrapper.");
            return;
        }

        m_interface->waitUntilFinished(m_pool);
    }

    /**
     * Returns true if the underlying ThreadPool is valid.
     *
     * @returns `true` if the underlying ThreadPool is valid; `false` otherwise.
     */
    bool isValid() const
    {
        return m_pool != nullptr;
    }

    /**
     * Destructor
     */
    ~ThreadPoolWrapper()
    {
        if (isValid())
        {
            m_interface->destroy(m_pool);
        }
    }

    CARB_PREVENT_COPY_AND_MOVE(ThreadPoolWrapper);

private:
    // ThreadPoolWrapper private members and functions
    IThreadPool* m_interface = nullptr;
    ThreadPool* m_pool = nullptr;
};

} // namespace tasking
} // namespace carb
