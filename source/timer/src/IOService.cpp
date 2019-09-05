//
// Created by junshen on 10/4/18.
//

#include "IOService.hpp"

namespace timerservice
{
    IOService::~IOService()
    {
        if (not ioService.stopped())
        {
            LOG_ERROR_MSG("IOService thread was not stopped before destruction - this can lead to race conditions with "
                              "lifetime of its users");
            stop();
        }
    }

    void IOService::stop()
    {
        ioService.stop();
        ioServiceWorker.join();
    }

    IOService::operator boost::asio::io_service&()
    {
        return ioService;
    }

} // namespace timerservice
