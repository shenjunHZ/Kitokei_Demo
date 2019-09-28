#include "AppInstance.hpp"
#include "logger/Logger.hpp"
#include "timer/IOService.hpp"
#include "timer/DefaultTimerService.hpp"
#include "usbVideo/CameraProcess.hpp"
#include "usbVideo/VideoManagement.hpp"
#include "common/CommonFunction.hpp"

namespace
{
    std::atomic_bool keep_running{ true };
    constexpr int PipeFileRight = 0666;
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
    {
        initService(logger);
    }

    AppInstance::~AppInstance()
    {
        usbVideo::CameraProcess::stopRun();

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

        while (keep_running)
        {
            sleep(1);
            //LOG_DEBUG_MSG("this is main thread.");
        }
 
        return;
    }

} // namespace applications
