#pragma once

#include <thread>
#include <memory>
#include "Configurations/ParseConfigFile.hpp"
#include "usbVideo/CameraProcess.hpp"

namespace spdlog
{
    class logger; // NOLINT
}

namespace application
{
    class AppInstance final
    {
    public:
        AppInstance(spdlog::logger& logger, const configuration::AppConfiguration& config);
        ~AppInstance();

        void loopFuction();

    private:
        void initService();

    private:
        std::thread m_cameraProcessThread;
        std::unique_ptr<Video::CameraProcess> m_cameraProcess;
    };
} // namespace application