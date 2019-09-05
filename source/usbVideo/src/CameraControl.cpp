#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "usbVideo/CameraControl.hpp"
#include "usbVideo/CameraImage.hpp"
#include "logger/Logger.hpp"

namespace Video
{
    CameraControl::CameraControl(const std::string& cameraDev)
        : m_cameraDev{std::move(cameraDev)}
    {

    }

    CameraControl::~CameraControl()
    {
        if (-1 != m_cameraFd)
        {
            close(m_cameraFd);
        }
    }

    int CameraControl::openDevice()
    {
        if ( (m_cameraFd = open(m_cameraDev.c_str(), O_RDWR)) < 0)
        {
            LOG_ERROR_MSG("Open camera {} failed: {}", m_cameraDev, std::strerror(errno));
        }
        return m_cameraFd;
    }

    bool CameraControl::closeDevice()
    {
        if (-1 != m_cameraFd)
        {
            if ( close(m_cameraFd) < 0)
            {
                LOG_ERROR_MSG("Close camera fd failed: {}", m_cameraFd);
                return false;
            }
        }

        return true;
    }

    bool CameraControl::getCameraCapability(struct v4l2_capability& capability)
    {
        if ( -1 == ioctl(m_cameraFd, VIDIOC_QUERYCAP, &capability) )
        {
            LOG_ERROR_MSG("get VIDIOC_QUERYCAP failed: {}", m_cameraDev);
            return false;
        }
        return true;
    }

    bool CameraControl::getCameraFormat(struct v4l2_format& format)
    {
        // set the type - needed so we can get the structure
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if ( -1 == ioctl(m_cameraFd, VIDIOC_G_FMT, &format) )
        {
            LOG_ERROR_MSG("get VIDIOC_G_FMT failed: {}", m_cameraDev);
            return false;
        }
        return true;
    }

    int CameraControl::requestMmapBuffers(const struct v4l2_requestbuffers& requestBuffers)
    {
        if ( ioctl(m_cameraFd, VIDIOC_REQBUFS, &requestBuffers) < 0 )
        {
            LOG_ERROR_MSG("request VIDIOC_REQBUFS failed: {}", std::strerror(errno));
            return -1;
        }

        /* exit if there is not enough memory for 2 buffers */
        if (requestBuffers.count < 2)
        {
            LOG_ERROR_MSG("Unable to allocate memory for at least 2 image buffers.");
            return -1;
        }

        /* allocate the memory needed for our structure */
        m_imageBuffers.resize(requestBuffers.count);
        int totalBuffers = 0;

        for (int i = 0; i < requestBuffers.count; i++)
        {
            struct v4l2_buffer buffer;
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = i;
            if (queryMapBuffer(buffer))
            {
                ++totalBuffers;
            }
        }

        return totalBuffers;
    }

    bool CameraControl::queryMapBuffer(struct v4l2_buffer& buffer)
    {
        if ( ioctl(m_cameraFd, VIDIOC_QUERYBUF, &buffer) < 0)
        {
            LOG_ERROR_MSG("query map buffer VIDIOC_QUERYBUF failed {}.", std::strerror(errno));
            return false;
        }

        m_imageBuffers[buffer.index].bufferLength = buffer.length;
        m_imageBuffers[buffer.index].startBuffer = mmap(NULL, buffer.length, 
                                                        PROT_READ | PROT_WRITE, MAP_SHARED,
                                                        m_cameraFd, buffer.m.offset);

        if (m_imageBuffers[buffer.index].startBuffer == MAP_FAILED)
        {
            LOG_ERROR_MSG("mmap failed: {}", std::strerror(errno));
            return false;
        }
        return true;
    }

    bool CameraControl::startCameraStreaming(const struct v4l2_requestbuffers& requestBuffers)
    {
        for (int i = 0; i < requestBuffers.count; i++)
        {
            struct v4l2_buffer v4l2Buffer;
            v4l2Buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            v4l2Buffer.memory = V4L2_MEMORY_MMAP;
            v4l2Buffer.index = i;

            queueBuffer(v4l2Buffer);
        }

        if ( ioctl(m_cameraFd, VIDIOC_STREAMON, &requestBuffers.type) < 0)
        {
            LOG_ERROR_MSG("start camera stream (VIDIOC_STREAMON) failed {}.", std::strerror(errno));
            return false;
        }
        return true;
    }

    bool CameraControl::queueBuffer(const struct v4l2_buffer& v4l2Buffer)
    {
        if ( ioctl(m_cameraFd, VIDIOC_QBUF, &v4l2Buffer) < 0)
        {

            LOG_ERROR_MSG("Type not supported, index out of bounds or no buffers allocate failed: {}", std::strerror(errno));
            return false;
        }
        return true;
    }

    bool CameraControl::dequeueBuffer(const struct v4l2_buffer& v4l2Buffer)
    {
        if (ioctl(m_cameraFd, VIDIOC_DQBUF, &v4l2Buffer) < 0)
        {
            LOG_ERROR_MSG("depueue buffer failed: {}", std::strerror(errno));
            return false;
        }

        return true;
    }

    bool CameraControl::endCameraStreaming(const int& videoType)
    {
        if (ioctl(m_cameraFd, VIDIOC_STREAMOFF, &videoType) < 0)
        {
            LOG_ERROR_MSG("en camera streaming (VIDIOC_STREAMOFF) failed: {}", std::strerror(errno));
            return false;
        }
        return true;
    }

    bool CameraControl::unMapBuffers()
    {
        for (int i = 0; i < m_imageBuffers.size(); i++)
        {
            munmap(m_imageBuffers[i].startBuffer, m_imageBuffers[i].bufferLength);
        }
        return true;
    }

    bool CameraControl::getRGBBuffer(std::vector<uint8_t>& rgbBuffer, const struct v4l2_buffer& v4l2Buffer, 
        const int& reqWidth, const int& reqHeight)
    {
        if ( convertYuvToRgbBuffer(static_cast<uint8_t*>(m_imageBuffers[v4l2Buffer.index].startBuffer), 
            &rgbBuffer[0], reqWidth, reqHeight) == 0 )
        {
            return true;
        }
        return false;
    }

    bool CameraControl::queryBuffer(const struct v4l2_buffer& v4l2Buffer)
    {
        if (ioctl(m_cameraFd, VIDIOC_QUERYBUF, &v4l2Buffer) < 0)
        {
            LOG_ERROR_MSG("query buffer (VIDIOC_QUERYBUF) fail: {}", std::strerror(errno));
            return false;
        }

        return true;
    }

} // namespace Video
