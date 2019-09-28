#include <atomic>
#include <sys/types.h>
#include <memory>
#include <sstream>
#include "common/CommonFunction.hpp"
#include "usbVideo/CameraProcess.hpp"
#include "usbVideo/CameraControl.hpp"
#include "usbVideo/CameraImage.hpp"

namespace
{
    constexpr int V4l2RequestBuffersCounter = 4;
    constexpr int RGBCountSize = 3;

    std::string getDefaultCameraDevice(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::cameraDevice) != config.end())
        {
            return config[configuration::cameraDevice].as<std::string>();
        }
        return "/dev/video0";
    }

    bool getEnableCameraStream(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::enableCameraStream) != config.end())
        {
            return config[configuration::enableCameraStream].as<bool>();
        }
        return true;
    }

    int getV4l2RequestBuffersCounter(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::V4l2RequestBuffersCounter) != config.end())
        {
            return config[configuration::V4l2RequestBuffersCounter].as<int>();
        }
        return V4l2RequestBuffersCounter;
    }

    std::string getV4L2CaptureFormat(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::V4L2CaptureFormat) != config.end())
        {
            return config[configuration::V4L2CaptureFormat].as<std::string>();
        }
        return "BMP";
    }

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

    CameraProcess::CameraProcess(Logger& logger, const configuration::AppConfiguration& config)
        : m_logger{ logger }
        , m_enableCameraStream{ getEnableCameraStream(config) }
        , m_pipeName{ common::getPipeFileName(config) }
        , m_outputDir{ common::getCaptureOutputDir(config) }
        , m_V4l2RequestBuffersCounter{ getV4l2RequestBuffersCounter(config) }
        , m_cameraControl(std::make_unique<CameraControl>(logger, getDefaultCameraDevice(config)))
    {
        std::string format = getV4L2CaptureFormat(config);
        m_captureFormat = covertV4L2CaptureFormat(format);
    }

    CameraProcess::~CameraProcess()
    {
        if (m_captureThread.joinable())
        {
            m_captureThread.join();
        }
    }

    bool CameraProcess::initDevice(configuration::bestFrameSize& frameSize)
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

            m_cameraControl->getBestCameraFrameFormat(frameSize);
        }
        return true;
    }

    void CameraProcess::outputDeviceInfo()
    {
        LOG_DEBUG_MSG("***********Device Infomation**********");
        LOG_DEBUG_MSG("Capability Driver: {}", m_v4l2Capability.driver);
        LOG_DEBUG_MSG("Capability Card: {}", m_v4l2Capability.card);
        LOG_DEBUG_MSG("Capability BusInfo: {}", m_v4l2Capability.bus_info);

        LOG_DEBUG_MSG("Format Width: {}", m_v4l2Format.fmt.pix.width);
        LOG_DEBUG_MSG("Format Height: {}", m_v4l2Format.fmt.pix.height);
        LOG_DEBUG_MSG("Pixel format: {}", checkPixelFormat() );
        LOG_DEBUG_MSG("Bytes per line: {}", m_v4l2Format.fmt.pix.bytesperline);
        LOG_DEBUG_MSG("***********Device Infomation**********");
    }

    int CameraProcess::checkPixelFormat()
    {
        switch (m_v4l2Format.fmt.pix.pixelformat)
        {
        case V4L2_PIX_FMT_YUYV:
            return V4L2_PIX_FMT_YUYV;
        default:
            return -1;
        }

        return -1;
    }

    void CameraProcess::runDevice()
    {
        outputDeviceInfo();

        if(m_outputDir.empty())
        {
            LOG_ERROR_MSG("output directory is empty.");
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
            exitProcess();
        }
        LOG_DEBUG_MSG("Request mmap buffers {} success.", successBuffers);

        /* start the capture */
        m_cameraControl->startCameraStreaming(requestBuffers);

        // start capture
        //m_captureThread = std::thread(&CameraProcess::captureCamera, this);
        if (m_enableCameraStream)
        {
            // stream
            LOG_DEBUG_MSG("....Start catch camera stream ....");
            streamCamera();
        }
        else if(not captureCamera())
        {
            LOG_DEBUG_MSG("Capture camera file failure.");
        }

        fclose(m_fd);
        exitProcess();
    }

    bool CameraProcess::captureCamera()
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

    void CameraProcess::streamCamera()
    {
        std::vector<uint8_t> rgbBuffer;
        rgbBuffer.resize(m_v4l2Format.fmt.pix.width * m_v4l2Format.fmt.pix.height * RGBCountSize);

        struct v4l2_buffer v4l2Buffer;
        v4l2Buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2Buffer.memory = V4L2_MEMORY_MMAP;
        for (int index = 0; index < m_V4l2RequestBuffersCounter; ++index)
        {
            v4l2Buffer.index = index;
            m_cameraControl->queueBuffer(v4l2Buffer);
        }

        while (keep_running)
        {
            /* get the idx of ready buffer */
            for (int i = 0; i < m_V4l2RequestBuffersCounter; i++)
            {
                /* Check if the thread should stop. */
                //if (stream_finish) return;

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
                int64_t data_size = m_v4l2Format.fmt.pix.width * m_v4l2Format.fmt.pix.height * RGBCountSize;
                size_t writeDataSize = 0;
                int index = 0;

                while (data_size)
                {
                    if (index >= rgbBuffer.size())
                    {
                        break;
                    }

                    if (data_size > PIPE_BUF)
                    {
                        writeDataSize = fwrite(&rgbBuffer[index], 1, PIPE_BUF, m_fd);
                        if (writeDataSize < PIPE_BUF)
                        {
                            LOG_ERROR_MSG("Error writing the data {}, should data {}.", writeDataSize, PIPE_BUF);
                        }
                    }
                    else
                    {
                        writeDataSize = fwrite(&rgbBuffer[index], 1, data_size, m_fd);
                        if (writeDataSize < data_size)
                        {
                            LOG_ERROR_MSG("Error writing the data {}, should data {}.", writeDataSize, data_size);
                        }
                    }
                    index += writeDataSize;
                    data_size -= writeDataSize;
                    fflush(m_fd);
                } 

                //LOG_DEBUG_MSG("Success writing the data {}", index);
                m_cameraControl->queueBuffer(v4l2Buffer);
            }
        }
    }

    void CameraProcess::exitProcess()
    {
        LOG_DEBUG_MSG("camshot ending.");

        /* Clean up */
        if (m_captureThread.joinable())
        {
            m_captureThread.join();
        }
        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        m_cameraControl->endCameraStreaming(type);

        m_cameraControl->unMapBuffers();
        m_cameraControl->closeDevice();
    }

    void CameraProcess::stopRun()
    {
        keep_running = false;
    }

} // namespace Video
