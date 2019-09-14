#include "AppInstance.hpp"
#include "logger/Logger.hpp"
#include "timer/IOService.hpp"
#include "timer/DefaultTimerService.hpp"
#include "usbVideo/CameraProcess.hpp"
#include "usbVideo/VideoManagement.hpp"

namespace
{
    std::atomic_bool keep_running{ true };
} // namespace 
namespace application
{
    AppInstance::AppInstance(spdlog::logger& logger, const configuration::AppConfiguration& config, 
            const configuration::AppAddresses& appAddress)
        : m_ioService{ std::make_unique<timerservice::IOService>() }
        , m_timerService{ std::make_unique<timerservice::DefaultTimerService>(*m_ioService) }
        , m_clientReceiver{ logger, config, appAddress, *m_timerService }
        , m_cameraProcess{ std::make_unique<usbVideo::CameraProcess>(logger, config) }
        , m_videoManagement{ std::make_unique<usbVideo::VideoManagement>(logger, config, *m_timerService) }
    {
        initService();
    }

    AppInstance::~AppInstance()
    {
        if (m_cameraProcessThread.joinable())
        {
            m_cameraProcessThread.join();
        }
    }

    void AppInstance::initService()
    {

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
/*******************************************/
       // for test catch video
        while (keep_running)
        {
            sleep(1000);
        }
/******************************************/
        
        usbVideo::CameraProcess::stopRun();
        return;
    }

} // namespace applications
