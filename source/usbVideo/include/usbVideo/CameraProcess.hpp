#pragma once

#include <linux/videodev2.h>
#include <thread>
#include "Configurations/Configurations.hpp"
#include "Configurations/ParseConfigFile.hpp"
#include "logger/Logger.hpp"

namespace Video
{
    using v4l2Capability = struct v4l2_capability;
    using v4l2Format = struct v4l2_format;
    using v4l2Requestbuffers = struct v4l2_requestbuffers;
    class ICameraControl;

    class CameraProcess final
    {
    public:
        CameraProcess(Logger& logger, const configuration::AppConfiguration& config);
        ~CameraProcess();
        void runDevice();
        static void stopRun();

    private:
        bool initDevice();
        void outputDeviceInfo();
        int checkPixelFormat();

        bool captureCamera();
        void streamCamera();
        void exitProcess();

    private:
        std::unique_ptr<ICameraControl> m_cameraControl;
        v4l2Capability m_v4l2Capability;
        v4l2Format m_v4l2Format;

        std::thread m_captureThread;

        bool m_bPipe{true};
        bool m_bSharedMem{false};
        std::string m_pipeName{"videoCapturePipe"};
        std::string m_outputDir{"/tmp/videoCapture/"};
        int m_V4l2RequestBuffersCounter;
        configuration::captureFormat m_captureFormat{ configuration::captureFormat::CAPTURE_FORMAT_BMP };

        bool m_bSharedMemory{false};
        Logger& m_logger;
    };
} // namespace Video
