#pragma once
#include <linux/videodev2.h>
#include <thread>
#include "Configurations/Configurations.hpp"
#include "Configurations/ParseConfigFile.hpp"
#include "logger/Logger.hpp"

namespace usbAudio
{
    class IAudioRecordService;
} // namespace usbAudio

namespace usbVideo
{
    using v4l2Capability = struct v4l2_capability;
    using v4l2Format = struct v4l2_format;
    using v4l2Requestbuffers = struct v4l2_requestbuffers;
    using v4l2Streamparm = struct v4l2_streamparm;
    class ICameraControl;

    class CameraService final
    {
    public:
        CameraService(Logger& logger, const configuration::AppConfiguration& config);
        ~CameraService();
        bool initDevice(configuration::bestFrameSize& frameSize);
        void runDevice();
        static void stopRun();

    private:
        void outputDeviceInfo();
        int checkPixelFormat();
        std::string convertPixeFormat();

        bool captureCamera();
        void streamCamera();
        void exitCameraService();

    private:
        Logger& m_logger;
        std::unique_ptr<ICameraControl> m_cameraControl;
        std::unique_ptr<usbAudio::IAudioRecordService> m_audioService;
        v4l2Capability m_v4l2Capability;
        v4l2Format m_v4l2Format;
        v4l2Streamparm m_v4l2Streamparm;

        std::thread m_captureThread;

        bool m_enableCameraStream{true};
        bool m_bSharedMem{false};
        std::string m_pipeName{"videoPipe"};
        std::string m_outputDir{"/tmp/videoCapture/"};
        int m_V4l2RequestBuffersCounter;
        configuration::captureFormat m_captureFormat{ configuration::captureFormat::CAPTURE_FORMAT_BMP };

        bool m_bSharedMemory{false};
        FILE* m_fd{ nullptr };
    };
} // namespace Video
