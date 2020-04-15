#include <atomic>
#include <sys/types.h>
#include <memory>
#include <sstream>
#include "common/CommonFunction.hpp"
#include "usbVideo/CameraService.hpp"
#include "usbVideo/CameraControl.hpp"
#include "usbVideo/CameraImage.hpp"
#include "usbVideo/AudioService.hpp"

namespace
{
    constexpr int RGBCountSize = 3;

    configuration::captureFormat covertV4L2CaptureFormat(const std::string& format)
    {
        if ("BMP" == format)
        {
            return configuration::captureFormat::CAPTURE_FORMAT_BMP;
        }

        return configuration::captureFormat::CAPTURE_FORMAT_RGB;
    }
} // namespace

namespace usbVideo
{
    std::atomic_bool keep_running{ true };

    CameraService::CameraService(Logger& logger, const configuration::AppConfiguration& config)
        : m_logger{ logger }
        , m_enableCameraStream{ video::getEnableCameraStream(config) }
        , m_pipeName{ video::getPipeFileName(config) }
        , m_outputDir{ common::getCaptureOutputDir(config) }
        , m_V4l2RequestBuffersCounter{ video::getV4l2RequestBuffersCounter(config) }
        , m_cameraControl(std::make_unique<CameraControl>(logger, video::getDefaultCameraDevice(config)))
        //, m_audioService(std::make_unique<AudioService>(logger, config))
    {
        std::string format = video::getV4L2CaptureFormat(config);
        m_captureFormat = covertV4L2CaptureFormat(format);
    }

    CameraService::~CameraService()
    {
        if (m_audioService)
        {
            m_audioService->exitAudioRecord();
        }
        if (m_captureThread.joinable())
        {
            m_captureThread.join();
        }
    }

    bool CameraService::initDevice(configuration::bestFrameSize& frameSize)
    {
        if (m_cameraControl)
        {
            if (m_cameraControl->openDevice() < 0)
            {
                return false;
            }
            if (m_cameraControl->getCameraCapability(m_v4l2Capability) < 0)
            {
                m_cameraControl->closeDevice();
                return false;
            }
            if (m_cameraControl->getCameraFrameFormat(m_v4l2Format) < 0)
            {
                m_cameraControl->closeDevice();
                return false;
            }
            if (m_cameraControl->getCaptureParm(m_v4l2Streamparm) < 0)
            {
                m_cameraControl->closeDevice();
                return false;
            }

            if (not m_cameraControl->getBestCameraFrameFormat(frameSize))
            {
                frameSize.frameWidth = m_v4l2Format.fmt.pix.width;
                frameSize.frameHeight = m_v4l2Format.fmt.pix.height;
                frameSize.bBestFrame = true;
            }
            return true;
        }

        return false;
    }

    void CameraService::outputDeviceInfo()
    {
        LOG_INFO_MSG(m_logger, "***********Video Device Infomation**********");
        LOG_INFO_MSG(m_logger, "Capability driver: {}", m_v4l2Capability.driver);
        LOG_INFO_MSG(m_logger, "Capability card: {}", m_v4l2Capability.card);
        LOG_INFO_MSG(m_logger, "Capability bus_info: {}", m_v4l2Capability.bus_info);
        LOG_INFO_MSG(m_logger, "Capability version: {}", m_v4l2Capability.version);
        LOG_INFO_MSG(m_logger, "Capability capabilities: {}", m_v4l2Capability.capabilities);
        LOG_INFO_MSG(m_logger, "Capability device_caps: {}", m_v4l2Capability.device_caps);

        // V4L2_CAP_TIMEPERFRAME	0x1000	 timeperframe field is supported 
        LOG_INFO_MSG(m_logger, "Capture capability: {}", m_v4l2Streamparm.parm.capture.capability);
        // V4L2_MODE_HIGHQUALITY	0x0001	  High quality imaging mode 
        LOG_INFO_MSG(m_logger, "Capture capturemode: {}", m_v4l2Streamparm.parm.capture.capturemode);
        LOG_INFO_MSG(m_logger, "Capture timeperframe: {} / {}",
            m_v4l2Streamparm.parm.capture.timeperframe.numerator, m_v4l2Streamparm.parm.capture.timeperframe.denominator);

        LOG_INFO_MSG(m_logger, "Format Width: {}", m_v4l2Format.fmt.pix.width);
        LOG_INFO_MSG(m_logger, "Format Height: {}", m_v4l2Format.fmt.pix.height);
        LOG_INFO_MSG(m_logger, "Pixel format: {}", convertPixeFormat() );
        LOG_INFO_MSG(m_logger, "Bytes per line: {}", m_v4l2Format.fmt.pix.bytesperline);
        LOG_INFO_MSG(m_logger, "***********Video Device Infomation**********");
    }

    std::string CameraService::convertPixeFormat()
    {
        switch (m_v4l2Format.fmt.pix.pixelformat)
        {
        case V4L2_PIX_FMT_YUYV:
            return "(Y, U, Y, V)  YUV 4:2:2";
        case V4L2_PIX_FMT_MJPEG:
            return "(M, J, P, G)  Motion-JPEG";
        default:
            return "";
        }
    }

    int CameraService::checkPixelFormat()
    {
        switch (m_v4l2Format.fmt.pix.pixelformat)
        {
        case V4L2_PIX_FMT_YUYV:
            return V4L2_PIX_FMT_YUYV;
        case V4L2_PIX_FMT_MJPEG:
            return V4L2_PIX_FMT_MJPEG;
        default:
            return -1;
        }

        return -1;
    }

    void CameraService::runDevice()
    {
        outputDeviceInfo();

        if(m_outputDir.empty())
        {
            LOG_ERROR_MSG("Output directory is empty.");
            m_cameraControl->closeDevice();
            return;
        }
        std::string fileName = m_outputDir + m_pipeName;
        m_fd = fopen(fileName.c_str(), "wb");
        if (nullptr == m_fd)
        {
            LOG_ERROR_MSG("open write file fail {}.", std::strerror(errno));
            return;
        }

        v4l2Requestbuffers requestBuffers;
        requestBuffers.count = m_V4l2RequestBuffersCounter;
        requestBuffers.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        requestBuffers.memory = V4L2_MEMORY_MMAP;

        int successBuffers = m_cameraControl->requestMmapBuffers(requestBuffers);
        if (successBuffers != m_V4l2RequestBuffersCounter)
        {
            LOG_ERROR_MSG("mmap buffers failed.");
            exitCameraService();
        }
        LOG_DEBUG_MSG("Request mmap {} buffers success.", successBuffers);

        /* start the capture */
        m_cameraControl->startCameraStreaming(requestBuffers);
        if (m_audioService)
        {
            m_audioService->initAudioRecord();
            m_audioService->audioStartListening();
        }

        // start capture
        if (m_enableCameraStream)
        {
            // stream
            LOG_DEBUG_MSG("Start catch camera stream.");
            streamCamera();
        }
        else if(not captureCamera())
        {
            LOG_DEBUG_MSG("Capture camera file failure.");
        }

        fclose(m_fd);
        exitCameraService();
    }

    bool CameraService::captureCamera()
    {
        std::vector<uint8_t> rgbBuffer;

        int ready_buf;
        char cur_name[64];
        struct timeval timestamp;

        rgbBuffer.resize(m_v4l2Format.fmt.pix.width * m_v4l2Format.fmt.pix.height * RGBCountSize);
        /* Wait for the start condition */

        /* queue one buffer and 'refresh it' */
        struct v4l2_buffer v4l2Buffer;
        v4l2Buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2Buffer.memory = V4L2_MEMORY_MMAP;
        for (int i = 0; i < m_V4l2RequestBuffersCounter; i++)
        {
            v4l2Buffer.index = i;
            m_cameraControl->queueBuffer(v4l2Buffer);
        }
        for (int i = 0; i < m_V4l2RequestBuffersCounter - 1; i++)
        {
            m_cameraControl->dequeueBuffer(v4l2Buffer);
        }

        /* get the idx of ready buffer */
        if (!m_cameraControl->dequeueBuffer(v4l2Buffer))
        {
            return false;
        }

        switch (checkPixelFormat())
        {
        case V4L2_PIX_FMT_YUYV:
            /* convert data to rgb */
            if ( m_cameraControl->getRGBBuffer(rgbBuffer, v4l2Buffer, m_v4l2Format.fmt.pix.width, m_v4l2Format.fmt.pix.height) )
            {
                LOG_DEBUG_MSG("Converted to rgb.");
            }
            break;
        default:
            LOG_ERROR_MSG("Unsupported pixelformat!");
            return false;
        }

        //m_cameraControl->queryBuffer(v4l2Buffer);

        /* make the image */

        /* create the file name */
        std::string fileName = m_outputDir;
        {
            std::stringstream strStream;
            strStream << v4l2Buffer.timestamp.tv_sec;
            fileName += "camshot_" + strStream.str() + ".bmp";
        }

        switch (m_captureFormat)
        {
        case configuration::captureFormat::CAPTURE_FORMAT_BMP:
            makeCaptureBMP(&rgbBuffer[0],
                fileName,
                m_v4l2Format.fmt.pix.width,
                m_v4l2Format.fmt.pix.height);
            break;
        case configuration::captureFormat::CAPTURE_FORMAT_RGB:
            makeCaptureRGB(&rgbBuffer[0],
                fileName,
                m_v4l2Format.fmt.pix.width,
                m_v4l2Format.fmt.pix.height);
            break;
        default:
            LOG_ERROR_MSG("Not supported format requested!");
            return false;
        }

        return true;
    }

    void CameraService::streamCamera()
    {
        std::vector<uint8_t> rgbBuffer;
        rgbBuffer.resize(m_v4l2Format.fmt.pix.width * m_v4l2Format.fmt.pix.height * RGBCountSize);

        struct v4l2_buffer v4l2Buffer;
        v4l2Buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2Buffer.memory = V4L2_MEMORY_MMAP;
        for (int iCount = 0; iCount < m_V4l2RequestBuffersCounter; ++iCount)
        {
            v4l2Buffer.index = iCount;
            m_cameraControl->queueBuffer(v4l2Buffer);
        }

        while (keep_running)
        {
            /* get the idx of ready buffer */
            for (int iCount = 0; iCount < m_V4l2RequestBuffersCounter; ++iCount)
            {
                if (not m_cameraControl->dequeueBuffer(v4l2Buffer))
                {
                    m_cameraControl->queueBuffer(v4l2Buffer);
                    continue;
                }
                //LOG_DEBUG_MSG("Request dequeue buffer {} ready.", v4l2Buffer.index);

                switch (checkPixelFormat())
                {
                case V4L2_PIX_FMT_YUYV:
                    /* convert data to rgb */
                    m_cameraControl->getRGBBuffer(rgbBuffer, v4l2Buffer, m_v4l2Format.fmt.pix.width, m_v4l2Format.fmt.pix.height);
                    break;
                default:
                    LOG_ERROR_MSG("Unsupported pixelformat!");
                    return;
                }

                /* write data */
                int64_t dataSize = rgbBuffer.size();
                size_t writeDataSize = 0;
                int dataIndex = 0;

                while (dataSize)
                {
                    if (dataIndex >= rgbBuffer.size())
                    {
                        break;
                    }

                    if (dataSize > PIPE_BUF)
                    {
                        writeDataSize = fwrite(&rgbBuffer[dataIndex], 1, PIPE_BUF, m_fd);
                        if (writeDataSize < PIPE_BUF)
                        {
                            LOG_ERROR_MSG("Error writing the data {}, should data {}.", writeDataSize, PIPE_BUF);
                        }
                    }
                    else
                    {
                        writeDataSize = fwrite(&rgbBuffer[dataIndex], 1, dataSize, m_fd);
                        if (writeDataSize < dataSize)
                        {
                            LOG_ERROR_MSG("Error writing the data {}, should data {}.", writeDataSize, dataSize);
                        }
                    }
                    dataIndex += writeDataSize;
                    dataSize -= writeDataSize;
                    //fflush(m_fd);
                }
               // LOG_DEBUG_MSG("Success write the data {} to video pipe.", dataIndex);

                m_cameraControl->queueBuffer(v4l2Buffer);
            }
        }
    }

    void CameraService::exitCameraService()
    {
        LOG_DEBUG_MSG("Start exit camera service.");

        /* Clean up */
        if (m_captureThread.joinable())
        {
            m_captureThread.join();
        }
        const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        m_cameraControl->stopCameraStreaming(type);
        if (m_audioService)
        {
            m_audioService->audioStopListening();
        }

        m_cameraControl->unMapBuffers();
        m_cameraControl->closeDevice();

        LOG_DEBUG_MSG("Exit camera service success.");
    }

    void CameraService::stopRun()
    {
        keep_running = false;
    }

} // namespace Video
