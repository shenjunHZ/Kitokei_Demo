#pragma once
#include <thread>
#include <memory>
#include "ClientReceiver.hpp"
#include "Configurations/ParseConfigFile.hpp"
#include "timer/DefaultTimerService.hpp"
#include "usbVideo/CameraProcess.hpp"

namespace spdlog
{
    class logger; // NOLINT
} // namespace spdlog

namespace timerservice
{
    class IOService;
} // namespace timerservice

namespace application
{
    class AppInstance final
    {
    public:
        AppInstance(spdlog::logger& logger, const configuration::AppConfiguration& config,
            const configuration::AppAddresses& appAddress);
        ~AppInstance();

        void loopFuction();

    private:
        void initService();

    private:
        ClientReceiver m_clientReceiver;

        std::thread m_cameraProcessThread;
        std::unique_ptr<Video::CameraProcess> m_cameraProcess;
        std::unique_ptr<timerservice::IOService> m_ioService;
        std::unique_ptr<timerservice::TimerService> m_timerService;
    };
} // namespace application