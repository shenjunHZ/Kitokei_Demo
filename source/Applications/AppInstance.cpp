#include "AppInstance.hpp"
#include "logger/Logger.hpp"

namespace application
{
    AppInstance::AppInstance(spdlog::logger& logger, const configuration::AppConfiguration& config)
        : m_cameraProcess(std::make_unique<Video::CameraProcess>(logger, config))
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
            m_cameraProcessThread = std::thread(&CameraProcess::runDevice, m_cameraProcess);
        }
        
        return;
    }

} // namespace applications