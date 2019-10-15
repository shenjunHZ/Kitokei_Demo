#include "AppInstance.hpp"
#include "logger/Logger.hpp"
#include "timer/IOService.hpp"
#include "timer/DefaultTimerService.hpp"
#include "usbVideo/CameraProcess.hpp"
#include "usbVideo/VideoManagement.hpp"
#include "usbAudio/AudioRecordService.hpp"
#include "usbAudio/AudioPlaybackService.hpp"
#include "common/CommonFunction.hpp"

namespace
{
    std::atomic_bool keep_running{ true };
    constexpr int PipeFileRight = 0666;
    constexpr int AudioSecondsDuration = 30;
} // namespace 
namespace application
{
    AppInstance::AppInstance(spdlog::logger& logger, const configuration::AppConfiguration& config, 
            const configuration::AppAddresses& appAddress)
        : m_config{config}
        , m_ioService{ std::make_unique<timerservice::IOService>() }
        , m_timerService{ std::make_unique<timerservice::DefaultTimerService>(*m_ioService) }
        , m_clientReceiver{ logger, config, appAddress, *m_timerService }
        , m_cameraProcess{ std::make_unique<usbVideo::CameraProcess>(logger, m_config) }
        , m_videoManagement{ std::make_unique<usbVideo::VideoManagement>(logger, m_config, *m_timerService) }
        , m_audioRecordService{std::make_unique<usbAudio::AudioRecordService>(logger, m_config)}
       // , m_audioPlayabckService{ std::make_unique<usbAudio::AudioPlaybackService>(logger, m_config) }
    {
        initService(logger);
    }

    AppInstance::~AppInstance()
    {
        usbVideo::CameraProcess::stopRun();
        if (m_audioRecordService)
        {
            m_audioRecordService->exitAudioRecord();
        }
        if (m_audioPlayabckService)
        {
            m_audioPlayabckService->exitAudioPlayback();
        }

        if (m_cameraProcessThread.joinable())
        {
            m_cameraProcessThread.join();
        }
    }

    void AppInstance::initService(spdlog::logger& logger)
    {
        std::string outputDir = common::getCaptureOutputDir(m_config);
        std::string pipeFileName = common::getPipeFileName(m_config);
        std::string pipeFile = outputDir + pipeFileName;

        if (not common::isFileExistent(pipeFile))
        {
            if (-1 == mkfifo(pipeFile.c_str(), PipeFileRight))
            {
                LOG_ERROR_MSG("Error creating the pipe: {}", pipeFile);
                return;
            }
        }
        else
        {
            LOG_DEBUG_MSG("Pipe file have exist {}.", pipeFile);
        }
        configuration::bestFrameSize frameSize;
        if (m_cameraProcess)
        {
            m_cameraProcess->initDevice(frameSize);
        }
        if (not frameSize.bBestFrame)
        {
            frameSize.frameWidth = common::getCaptureWidth(m_config);
            frameSize.frameHeight = common::getCaptureHeight(m_config);
        }
        if (m_videoManagement)
        {
            m_videoManagement->initVideoManagement(frameSize);
        }
        if (frameSize.bBestFrame)
        {
            LOG_INFO_MSG(logger, "Best video frame size width: {}, height: {}.", frameSize.frameWidth, frameSize.frameHeight);
        }
        else
        {
            LOG_INFO_MSG(logger, "Use configuration video frame size width: {}, height: {}.", frameSize.frameWidth, frameSize.frameHeight);
        }

        if (m_audioRecordService)
        {
            m_audioRecordService->initAudioRecord();
        }
//         if (m_audioPlayabckService)
//         {
//             m_audioPlayabckService->initAudioPlayback();
//         }
    }

    void AppInstance::clientDataReceived()
    {
        while (keep_running)
        {
            std::string dataMessage = m_clientReceiver.getDataMessage();

            if (0 < dataMessage.length())
            {
                //LOG_DEBUG_MSG("client received data : {} length: {}", dataMessage.c_str(), dataMessage.length());
            }
            else
            {
                usleep(1000 * 10);
            }
        }
    }

    void AppInstance::loopFuction()
    {
        if (m_cameraProcess)
        {
            m_cameraProcessThread = std::thread([&m_cameraProcess = this->m_cameraProcess]() 
            {
                m_cameraProcess->runDevice();
            });
        }
        if (m_videoManagement)
        {
            m_videoManagement->runVideoManagement(); // last call as open pipe with read mode
        }
        
        if (m_audioRecordService)
        {
            m_audioRecordService->audioStartListening(); // to do remove to client data received(start/stop audio)
        }
        int index = 0;
        while (index++ < AudioSecondsDuration)
        {
            sleep(1);
        }
        m_audioRecordService->audioStopListening();

//         if (m_audioPlayabckService)
//         {
//             m_audioPlayabckService->audioStartPlaying();
//         }

        m_dataReceivedThread = std::thread(&AppInstance::clientDataReceived, this);
        m_clientReceiver.receiveLoop();
    }

} // namespace applications
