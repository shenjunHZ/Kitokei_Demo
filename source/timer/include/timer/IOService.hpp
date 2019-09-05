//
// Created by junshen on 10/4/18.
//

#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <thread>
#include "logger/Logger.hpp"

namespace timerservice
{
    class IOService
    {
    public:
        IOService() = default;
        IOService(const IOService&) = delete;
        void operator=(const IOService&) = delete;
        IOService(IOService&&) = delete;
        void operator=(IOService&&) = delete;
        ~IOService();

        void stop();
        operator boost::asio::io_service&(); // NOLINT

    private:
        boost::asio::io_service ioService;
        boost::asio::io_service::work dummyWork{ioService};
        std::thread ioServiceWorker{[this]() {
            try
            {
                ioService.run();
            }
            catch (...)
            {
                LOG_ERROR_MSG("IOService worker error: {}", boost::current_exception_diagnostic_information());
                throw;
            }
        }};
    };

} // namespace timerservice

