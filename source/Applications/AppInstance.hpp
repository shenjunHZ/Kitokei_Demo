#pragma once
#include <thread>
#include <memory>
#include "ClientReceiver.hpp"
#include "Configurations/ParseConfigFile.hpp"

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
    class CameraProcess;
    class IVideoManagement;
} // namespace usbVideo

namespace usbAudio
{
    class IAudioRecordService;
} // namespace usbAudio

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
        void initService(spdlog::logger& logger);
        void clientDataReceived();

    private:
        const configuration::AppConfiguration& m_config;
        std::unique_ptr<timerservice::IOService> m_ioService;
        std::unique_ptr<timerservice::TimerService> m_timerService{};

        ClientReceiver m_clientReceiver;
        std::thread m_dataReceivedThread;

        std::unique_ptr<usbVideo::CameraProcess> m_cameraProcess;
        std::unique_ptr<usbVideo::IVideoManagement> m_videoManagement;
        std::thread m_cameraProcessThread;
        std::thread m_videoManagementThread;

        std::unique_ptr<usbAudio::IAudioRecordService> m_audioRecordService;
        std::thread m_audioRecordThread;
    };
} // namespace application