#pragma once
#include <thread>
#include <memory>
#include "ClientReceiver.hpp"
#include "Configurations/ParseConfigFile.hpp"
#include "usbVideo/CameraProcess.hpp"

namespace spdlog
{
    class logger;
} // namespace spdlog

namespace timerservice
{
    class IOService;
    class TimerService;
} // namespace timerservice

namespace usbVideo
{
    class IVideoManagement;
}

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
        std::unique_ptr<timerservice::IOService> m_ioService;
        std::unique_ptr<timerservice::TimerService> m_timerService{};
        ClientReceiver m_clientReceiver;

        std::unique_ptr<usbVideo::CameraProcess> m_cameraProcess{};
        std::unique_ptr<usbVideo::IVideoManagement> m_videoManagement;
        std::thread m_cameraProcessThread;
        std::thread m_videoManagementThread;

    };
} // namespace application