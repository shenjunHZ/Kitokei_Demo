#include "AppInstance.hpp"
#include "logger/Logger.hpp"
#include "timer/IOService.hpp"

namespace
{
    int testCatchTimes = 10;
} // namespace 
namespace application
{
    AppInstance::AppInstance(spdlog::logger& logger, const configuration::AppConfiguration& config, 
            const configuration::AppAddresses& appAddress)
        : m_cameraProcess(std::make_unique<Video::CameraProcess>(logger, config))
        , m_ioService{ std::make_unique<timerservice::IOService>() }
        , m_timerService{ std::make_unique<timerservice::DefaultTimerService>(*m_ioService) }
        , m_clientReceiver{ logger, config, appAddress, *m_timerService }
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
        while (testCatchTimes)
        {
            sleep(1);
            --testCatchTimes;
        }
/******************************************/
        
        Video::CameraProcess::stopRun();
        return;
    }

} // namespace applications
