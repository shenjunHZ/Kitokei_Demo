#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "usbVideo/CameraControl.hpp"
#include "usbVideo/CameraImage.hpp"

namespace usbVideo
{
    CameraControl::CameraControl(Logger& logger, const std::string& cameraDev)
        : m_logger{logger}
        , m_cameraDev{std::move(cameraDev)}
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
        if ( (m_cameraFd = open(m_cameraDev.c_str(), O_RDWR | O_NONBLOCK)) < 0)
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
        if (not (capability.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        {
            LOG_ERROR_MSG("{} is no video capture device.", m_cameraDev);
            return false;
        }
        // use for MMAP method
        if (not (capability.capabilities & V4L2_CAP_STREAMING))
        {
            LOG_ERROR_MSG("{} does not support streaming i/o .", m_cameraDev);
            return false;
        }
        return true;
    }

    bool CameraControl::getCameraFrameFormat(struct v4l2_format& format)
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

    bool CameraControl::setCameraFrameFormat(struct v4l2_format& format)
    {
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        if (-1 == ioctl(m_cameraFd, VIDIOC_S_FMT, &format))
        {
            LOG_ERROR_MSG("Cannot set YUYV video capture: {}", m_cameraDev);
            return false;
        }
        return true;
    }

    bool CameraControl::tryCameraFrameFormat(struct v4l2_format& format)
    {
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        if (-1 == ioctl(m_cameraFd, VIDIOC_TRY_FMT, &format))
        {
            LOG_ERROR_MSG("Cannot support YUYV video capture: {}", m_cameraDev);
            return false;
        }
        return true;
    }

    bool CameraControl::getBestCameraFrameFormat(configuration::bestFrameSize& frameSize)
    {
        int ret = -1;
        struct v4l2_fmtdesc fmtdesc;
        struct v4l2_format format;
        if ( (not tryCameraFrameFormat(format)) or (not setCameraFrameFormat(format)) )
        {
            return false;
        }

        //  All formats are enumerable by beginning at index zero and incrementing by one until EINVAL is returned.
        fmtdesc.index = 0;
        fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        while ((ret = ioctl(m_cameraFd, VIDIOC_ENUM_FMT, &fmtdesc)) == 0)
        {
            fmtdesc.index++;

            LOG_INFO_MSG(m_logger, "pixelformat = {} {} {} {}, description = {}",
                fmtdesc.pixelformat & 0xFF,
                (fmtdesc.pixelformat >> 8) & 0xFF,
                (fmtdesc.pixelformat >> 16) & 0xFF, 
                (fmtdesc.pixelformat >> 24) & 0xFF,
                fmtdesc.description);
            // pixel fromat is a four character code as computed by the v4l2_fourcc() macro
            if (V4L2_PIX_FMT_YUYV != fmtdesc.pixelformat)
            {
                continue;
            }

            return getPixelFormat(fmtdesc, frameSize);
        }
        if (0 != ret && 0 == fmtdesc.index)
        {
            LOG_ERROR_MSG("Enumerating frame formats failure: {}", std::strerror(errno));
            return false;
        }

        return false;
    }

    bool CameraControl::getPixelFormat(struct v4l2_fmtdesc& fmtdesc, configuration::bestFrameSize& frameSize)
    {
        int ret = -1;
        struct v4l2_frmsizeenum fsize;

        memset(&fsize, 0, sizeof(fsize));
        fsize.index = 0;
        fsize.pixel_format = fmtdesc.pixelformat;
        // Pointer to a struct v4l2_frmsizeenum that contains an index and pixel format and receives a frame width and height.
        while ((ret = ioctl(m_cameraFd, VIDIOC_ENUM_FRAMESIZES, &fsize)) == 0) 
        {
            if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) 
            {
                LOG_DEBUG_MSG("pixel format discrete: width = {}, height = {}",
                    fsize.discrete.width, fsize.discrete.height);

                ret = getFrameIntervals(fsize);
                if (fsize.discrete.width > frameSize.frameWidth && fsize.discrete.height > frameSize.frameHeight)
                {
                    frameSize.frameWidth = fsize.discrete.width;
                    frameSize.frameHeight = fsize.discrete.height;
                    frameSize.bBestFrame = true;
                }

                if (not ret)
                {
                    LOG_ERROR_MSG("Unable to get frame intervals.");
                }
            }
            else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) 
            {
                LOG_DEBUG_MSG("continuous: min width = {}, height = {}; max width = {}, height = {}",
                    fsize.stepwise.min_width, fsize.stepwise.min_height,
                    fsize.stepwise.max_width, fsize.stepwise.max_height);
                LOG_DEBUG_MSG("Refusing to enumerate frame intervals.");

                if (fsize.stepwise.max_width > frameSize.frameWidth && fsize.stepwise.max_height > frameSize.frameHeight)
                {
                    frameSize.frameWidth = fsize.stepwise.max_width;
                    frameSize.frameHeight = fsize.stepwise.max_height;
                    frameSize.bBestFrame = true;
                }
                break;
            }
            else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) 
            {
                LOG_DEBUG_MSG("stepwise: min width = {}, height = {}; max width = {}, height = {};"
                    "stepsize width = {}, height = {}",
                    fsize.stepwise.min_width, fsize.stepwise.min_height,
                    fsize.stepwise.max_width, fsize.stepwise.max_height,
                    fsize.stepwise.step_width, fsize.stepwise.step_height);
                LOG_DEBUG_MSG("Refusing to enumerate frame intervals.");

                if (fsize.stepwise.max_width > frameSize.frameWidth && fsize.stepwise.max_height > frameSize.frameHeight)
                {
                    frameSize.frameWidth = fsize.stepwise.max_width;
                    frameSize.frameHeight = fsize.stepwise.max_height;
                    frameSize.bBestFrame = true;
                }
                break;
            }
            fsize.index++;
        }
        if (ret != 0 && 0 == fsize.index)
        {
            LOG_ERROR_MSG("Enumerating frame sizes failure: {}", std::strerror(errno));
            return false;
        }

        return true;
    }

    bool CameraControl::getFrameIntervals(const struct v4l2_frmsizeenum& frmSizeEnum)
    {
        int ret = -1;
        struct v4l2_frmivalenum fival;
        memset(&fival, 0, sizeof(fival));

        fival.index = 0;
        fival.pixel_format = frmSizeEnum.pixel_format;
        fival.width = frmSizeEnum.discrete.width;
        fival.height = frmSizeEnum.discrete.height;

        while ((ret = ioctl(m_cameraFd, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0) 
        {
            if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE) 
            {
                LOG_DEBUG_MSG("Time interval frame: {}/{} .",
                    fival.discrete.numerator, fival.discrete.denominator);
            }
            else if (fival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) 
            {
                LOG_DEBUG_MSG("Time interval between frame: min {}/{}; max {}/{} .",
                    fival.stepwise.min.numerator, fival.stepwise.min.numerator,
                    fival.stepwise.max.denominator, fival.stepwise.max.denominator);
                break;
            }
            else if (fival.type == V4L2_FRMIVAL_TYPE_STEPWISE) 
            {
                LOG_DEBUG_MSG("Time interval between frame: min {}/{}; max {}/{} "
                    "stepsize {}/{} .",
                    fival.stepwise.min.numerator, fival.stepwise.min.denominator,
                    fival.stepwise.max.numerator, fival.stepwise.max.denominator,
                    fival.stepwise.step.numerator, fival.stepwise.step.denominator);
                break;
            }
            fival.index++;
        }
        if (ret != 0 && 0 == fival.index)
        {
            LOG_ERROR_MSG("Enumerating frame intervals failure: {}", std::strerror(errno));
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

    bool CameraControl::queryMapBuffer(const struct v4l2_buffer& buffer)
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
        LOG_DEBUG_MSG("Buffer address: {}, length: {}.", 
            m_imageBuffers[buffer.index].startBuffer, m_imageBuffers[buffer.index].bufferLength);
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
            //LOG_ERROR_MSG(" queue buffer failed: {}", std::strerror(errno));
            return false;
        }
        return true;
    }

    bool CameraControl::dequeueBuffer(struct v4l2_buffer& v4l2Buffer)
    {
        if (ioctl(m_cameraFd, VIDIOC_DQBUF, &v4l2Buffer) < 0)
        {
           // LOG_ERROR_MSG("depueue buffer failed: {}", std::strerror(errno));
            return false;
        }

        return true;
    }

    bool CameraControl::stopCameraStreaming(const int& videoType)
    {
        if (ioctl(m_cameraFd, VIDIOC_STREAMOFF, &videoType) < 0)
        {
            LOG_ERROR_MSG("Stop camera streaming (VIDIOC_STREAMOFF) failed: {}", std::strerror(errno));
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
