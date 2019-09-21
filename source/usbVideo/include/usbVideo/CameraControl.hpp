#pragma once

#include <vector>
#include "ICameraControl.hpp"
#include "Configurations/Configurations.hpp"
#include "logger/Logger.hpp"

namespace usbVideo
{
    class CameraControl final : public ICameraControl
    {
    public:
        CameraControl(Logger& logger, const std::string& cameraDev);
        ~CameraControl();
        int openDevice() override;
        bool closeDevice() override;
        bool getCameraCapability(struct v4l2_capability& capability) override;
        bool getCameraFrameFormat(struct v4l2_format& format) override;
        bool getBestCameraFrameFormat(struct v4l2_fmtdesc& fmtdesc) override;

        bool setCameraPixFormat(uint32_t width, uint32_t height) override { return false; };

        int requestMmapBuffers(const struct v4l2_requestbuffers& requestBuffers) override;
        bool startCameraStreaming(const struct v4l2_requestbuffers& requestBuffers) override;

        bool queueBuffer(const struct v4l2_buffer& v4l2Buffer) override;
        bool dequeueBuffer(struct v4l2_buffer& v4l2Buffer) override;
        bool queryBuffer(const struct v4l2_buffer& v4l2Buffer) override;

        bool endCameraStreaming(const int& videoType) override;
        bool unMapBuffers() override;

        bool getRGBBuffer(std::vector<uint8_t>& rgbBuffer, const struct v4l2_buffer& v4l2Buffer,
            const int& reqWidth, const int& reqHeight);

    private:
        bool queryMapBuffer(const struct v4l2_buffer& buffer);
        bool getPixelFormat(struct v4l2_fmtdesc& fmtdesc);
        bool getFrameIntervals(const struct v4l2_frmsizeenum& frmSizeEnum);

    private:
        Logger& m_logger;
        int m_cameraFd{-1};
        std::string m_cameraDev{"/dev/video0"};
        std::vector<configuration::imageBuffer> m_imageBuffers;
    };
} // namespace Video