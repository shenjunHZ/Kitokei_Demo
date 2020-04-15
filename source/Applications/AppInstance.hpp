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
    class CameraService;
    class IVideoManagement;
} // namespace usbVideo

namespace usbAudio
{
    class IAudioRecordService;
    class IAudioPlaybackService;
} // namespace usbAudio

namespace endpoints
{
    class IRTPSession;
} // namespace endpoints

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
        bool createPipeFile();

    private:
        const configuration::AppConfiguration& m_config;
        std::unique_ptr<timerservice::IOService> m_ioService;
        std::unique_ptr<timerservice::TimerService> m_timerService{};

        ClientReceiver m_clientReceiver;
        std::thread m_dataReceivedThread;

        std::unique_ptr<usbVideo::CameraService> m_cameraProcess;
        std::unique_ptr<usbVideo::IVideoManagement> m_videoManagement;
        std::thread m_cameraProcessThread;
        std::thread m_videoManagementThread;

        std::shared_ptr<endpoints::IRTPSession> m_rtpSession;
        std::unique_ptr<usbAudio::IAudioRecordService> m_audioRecordService;
        std::unique_ptr<usbAudio::IAudioPlaybackService> m_audioPlayabckService;
        std::thread m_audioRecordThread;
    };
} // namespace application