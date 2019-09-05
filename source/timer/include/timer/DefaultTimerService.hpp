//
// Created by junshen on 10/4/18.
//

#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "TimerService.hpp"
#include "Timer.hpp"
#include "TimerIdGenerator.hpp"

namespace timerservice
{
    class DefaultTimerService : public TimerService
    {
    public:
        explicit DefaultTimerService(boost::asio::io_service&);

        std::unique_ptr<Timer> scheduleTimer(const std::chrono::milliseconds&,
                                            const TimerService::TimeoutCallback& callback) override;

        std::unique_ptr<Timer> schedulePeriodicTimer(const std::chrono::milliseconds&,
                                                     const TimerService::TimeoutCallback&) override;

        std::chrono::milliseconds getTimestamp() const override;

    private:
        void cancelTimeout(TimerId);
        TimerId emplaceTimer(const std::shared_ptr<boost::asio::steady_timer>&);

        std::unique_ptr<Timer> createConcreteTimer(
            const std::chrono::milliseconds&,
            const TimerService::TimeoutCallback&,
            bool);

        void onTimeout(TimerId, const TimeoutCallback&, const boost::system::error_code&);

        void onPeriodicTimerTimeout(
            TimerId,
            const std::chrono::milliseconds&,
            const TimerService::TimeoutCallback&,
            const boost::system::error_code&);

        class ConcreteTimer final : public Timer
        {
        public:
            ConcreteTimer(DefaultTimerService&, TimerId);

            ConcreteTimer(const ConcreteTimer&) = delete;
            void operator=(const ConcreteTimer&) = delete;
            ConcreteTimer(ConcreteTimer&&) = delete;
            void operator=(ConcreteTimer&&) = delete;

            void cancel() override;
            ~ConcreteTimer() override;

        private:
            DefaultTimerService& timerService;
            TimerId id;
        };

        TimerIdGenerator idGenerator;

        boost::asio::io_service& ioService;
        std::mutex mtx;

        std::unordered_map<TimerId, std::shared_ptr<boost::asio::steady_timer>> timers;
    };

} // namespace timerservice

