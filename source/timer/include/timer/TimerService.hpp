//
// Created by junshen on 10/4/18.
//
#include <memory>
#include <chrono>
#include <functional>
#include "timer/include/Timer.hpp"

#pragma once

namespace timerservice
{
    class TimerService
    {
    public:
        using TimeoutCallback = std::function<void()>;

        virtual std::unique_ptr<Timer> scheduleTimer(const std::chrono::milliseconds&,
        const TimeoutCallback& callback) = 0;

        virtual std::unique_ptr<Timer> schedulePeriodicTimer(const std::chrono::milliseconds&,
        const TimeoutCallback& callback) = 0;

        virtual std::chrono::milliseconds getTimestamp() const = 0;

        virtual ~TimerService() = default;
    };
} // namespace timerservice
