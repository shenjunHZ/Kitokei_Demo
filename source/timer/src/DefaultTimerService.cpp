//
// Created by junshen on 10/4/18.
//

#include "DefaultTimerService.hpp"
#include <boost/exception/diagnostic_information.hpp>
#include <functional>
#include "logger/Logger.hpp"
#include "TimerService.hpp"

namespace timerservice
{
    DefaultTimerService::DefaultTimerService(boost::asio::io_service& ioService) : ioService{ioService}
    {

    }

    std::unique_ptr<Timer> DefaultTimerService::scheduleTimer(const std::chrono::milliseconds& duration,
                                        const TimerService::TimeoutCallback& callback)
    {
        return createConcreteTimer(duration, callback, false);
    }

    std::unique_ptr<Timer> DefaultTimerService::schedulePeriodicTimer(const std::chrono::milliseconds& duration,
        const TimerService::TimeoutCallback& callback)
    {
        return createConcreteTimer(duration, callback, true);
    }

    std::unique_ptr<Timer> DefaultTimerService::createConcreteTimer(
        const std::chrono::milliseconds& duration,
        const TimerService::TimeoutCallback& callback,
        bool isPeriodicTimer)
    {
        auto timer = std::make_shared<boost::asio::steady_timer>(ioService, duration);
        TimerId id = emplaceTimer(timer);
        timer->async_wait([id, duration, callback = callback, this, isPeriodicTimer](auto&& error) {
            if (isPeriodicTimer)
            {
                this->onPeriodicTimerTimeout(id, duration, callback, error);
                return;
            }
            this->onTimeout(id, callback, error);
        });
        return std::make_unique<ConcreteTimer>(*this, id);
    }

    std::chrono::milliseconds DefaultTimerService::getTimestamp() const
    {
        auto tp = std::chrono::system_clock::now();
        std::chrono::milliseconds timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
        return timestamp;
    }

    void DefaultTimerService::cancelTimeout(TimerId id)
    {
        std::lock_guard<std::mutex> lg(mtx);
        try
        {
            timers.erase(id);
        }
        catch (const boost::system::system_error& e)
        {
            LOG_ERROR_MSG(boost::diagnostic_information(e));
        }
    }

    TimerId DefaultTimerService::emplaceTimer(const std::shared_ptr<boost::asio::steady_timer>& timer)
    {
        std::lock_guard<std::mutex> lg(mtx);
        const auto id = idGenerator();
        timers.emplace(id, timer);
        return id;
    }

    void DefaultTimerService::onTimeout(
        TimerId id,
        const TimerService::TimeoutCallback& callback,
        const boost::system::error_code& error)
    {
        {
            std::lock_guard<std::mutex> lg(mtx);
            timers.erase(id);
        }

        if (not error or error != boost::asio::error::operation_aborted)
        {
            // Call callback only when the timer has not been canceled
            callback();
        }
    }

    void DefaultTimerService::onPeriodicTimerTimeout(
        TimerId id,
        const std::chrono::milliseconds& duration,
        const TimerService::TimeoutCallback& callback,
        const boost::system::error_code& error)
    {
        if (not error or error != boost::asio::error::operation_aborted)
        {
            // Call callback only when the timer has not been canceled
            callback();
            auto& timer = timers.at(id);
            timer->expires_from_now(duration);
            timer->async_wait([id, duration, callback = callback, this](auto&& error) {
                this->onPeriodicTimerTimeout(id, duration, callback, error);
            });
            return;
        }
        LOG_DEBUG_MSG("DefaultTimerService: timeout with exception (id={})", id);
    }

    void DefaultTimerService::ConcreteTimer::cancel()
    {
        timerService.cancelTimeout(id);
    }

    DefaultTimerService::ConcreteTimer::~ConcreteTimer()
    {
        LOG_DEBUG_MSG("DefaultTimerService::ConcreteTimer::~ConcreteTimer()");
        cancel();
    }

    DefaultTimerService::ConcreteTimer::ConcreteTimer(DefaultTimerService& timerService_, TimerId id_)
        : timerService{timerService_}, id{id_}
    {
    }

} // namespace timerservice
