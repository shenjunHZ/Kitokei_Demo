#pragma once
/*
* use V4L2 framework get USB camera with YUYV
*/
#include <stdint.h>
#include <linux/videodev2.h>

namespace configuration
{
    struct bestFrameSize;
} // namespace configuration

namespace usbVideo
{
    class ICameraControl
    {
    public:
        virtual ~ICameraControl() = default;
        // open the camera device
        virtual int openDevice() = 0;
        // close the camera device
        virtual bool closeDevice() = 0;
        // get the camera video capability
        virtual bool getCameraCapability(struct v4l2_capability&) = 0;
        // get the camera video format
        virtual bool getCameraFrameFormat(struct v4l2_format&) = 0;
        // check format whether support or not
        virtual bool tryCameraFrameFormat(struct v4l2_format& format) = 0;
        // capture parameters
        virtual bool getCaptureParm(struct v4l2_streamparm& streamParm) = 0;
        virtual bool setCaptureParm(struct v4l2_streamparm& streamParm) = 0;

        // get best frame format
        virtual bool getBestCameraFrameFormat(configuration::bestFrameSize& frameSize) = 0;

        // set video capture pix format
        virtual bool setCameraPixFormat(uint32_t width, uint32_t height) = 0;
        // set video capture frame format
        virtual bool setCameraFrameFormat(struct v4l2_format& format) = 0;

        // request buffers and maps
        virtual int requestMmapBuffers(const struct v4l2_requestbuffers&) = 0;
        // start the streaming
        virtual bool startCameraStreaming(const struct v4l2_requestbuffers&) = 0;

        // v4l2 buffer queue
        virtual bool queueBuffer(const struct v4l2_buffer&) = 0;
        // v4l2 buffer dequeue
        virtual bool dequeueBuffer(struct v4l2_buffer&) = 0;
        // v4l2 query timestamp
        virtual bool queryBuffer(const struct v4l2_buffer&) = 0;

        // unmap buffers
        virtual bool unMapBuffers() = 0;
        // stop the streaming
        virtual bool stopCameraStreaming(const int& videoType) = 0;

        virtual bool getRGBBuffer(std::vector<uint8_t>& rgbBuffer, const struct v4l2_buffer& v4l2Buffer,
            const int& reqWidth, const int& reqHeight) = 0;
    };
}