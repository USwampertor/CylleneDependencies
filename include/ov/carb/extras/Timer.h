// Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
//
// NVIDIA CORPORATION and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto. Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA CORPORATION is strictly prohibited.
//
#pragma once

#include <chrono>

namespace carb
{
namespace extras
{

/**
 * Timer class.
 */
class Timer
{
protected:
    std::chrono::high_resolution_clock::time_point m_startTimePoint, m_stopTimePoint;
    bool m_isRunning = false;

public:
    enum class Scale
    {
        eSeconds,
        eMilliseconds,
        eMicroseconds,
        eNanoseconds
    };

    /**
     * Returns precision of the timer (minimal tick duration).
     */
    double getPrecision()
    {
        return std::chrono::high_resolution_clock::period::num / (double)std::chrono::high_resolution_clock::period::den;
    }

    /**
     * Starts timer.
     */
    void start()
    {
        m_startTimePoint = std::chrono::high_resolution_clock::now();
        m_isRunning = true;
    }
    /**
     * Stops timer.
     */
    void stop()
    {
        m_stopTimePoint = std::chrono::high_resolution_clock::now();
        m_isRunning = false;
    }

    /**
     * Gets elapsed time in a specified form, using specified time scale.
     *
     * @param timeScale Time scale that you want to get result in.
     *
     * @return Elapsed time between timer start, and timer stop events. If timer wasn't stopped before, returns elapsed
     * time between timer start and this function call, timer will continue to tick. Template parameter allows to select
     * between integral returned elapsed time (default), and floating point elapsed time, double precision recommended.
     */
    template <typename ReturnType = int64_t>
    ReturnType getElapsedTime(Scale timeScale = Scale::eMilliseconds)
    {
        std::chrono::high_resolution_clock::time_point stopTimePoint;
        if (m_isRunning)
        {
            stopTimePoint = std::chrono::high_resolution_clock::now();
        }
        else
        {
            stopTimePoint = m_stopTimePoint;
        }

        auto elapsedTime = stopTimePoint - m_startTimePoint;

        using dblSeconds = std::chrono::duration<ReturnType, std::ratio<1>>;
        using dblMilliseconds = std::chrono::duration<ReturnType, std::milli>;
        using dblMicroseconds = std::chrono::duration<ReturnType, std::micro>;
        using dblNanoseconds = std::chrono::duration<ReturnType, std::nano>;

        switch (timeScale)
        {
            case Scale::eSeconds:
                return std::chrono::duration_cast<dblSeconds>(elapsedTime).count();
            case Scale::eMilliseconds:
                return std::chrono::duration_cast<dblMilliseconds>(elapsedTime).count();
            case Scale::eMicroseconds:
                return std::chrono::duration_cast<dblMicroseconds>(elapsedTime).count();
            case Scale::eNanoseconds:
                return std::chrono::duration_cast<dblNanoseconds>(elapsedTime).count();

            default:
                return std::chrono::duration_cast<dblMilliseconds>(elapsedTime).count();
        }
    }
};
} // namespace extras
} // namespace carb
