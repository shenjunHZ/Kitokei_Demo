#pragma once
/*
* use V4L2 framework get USB camera with YUV
*/
#include <stdint.h>
#include <linux/videodev2.h>

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
        // get frame format
        virtual bool getCameraFrameType(struct v4l2_fmtdesc&) = 0;

        // set video capture format
        virtual bool setCameraPixFormat(uint32_t width, uint32_t height) = 0;

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
        // end the streaming
        virtual bool endCameraStreaming(const int& videoType) = 0;

        virtual bool getRGBBuffer(std::vector<uint8_t>& rgbBuffer, const struct v4l2_buffer& v4l2Buffer,
            const int& reqWidth, const int& reqHeight) = 0;
    };
}