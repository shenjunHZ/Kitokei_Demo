#include <atomic>
#include <sys/types.h>
#include <memory>
#include <sstream>
#include "common/CommonFunction.hpp"
#include "usbVideo/CameraProcess.hpp"
#include "usbVideo/CameraControl.hpp"
#include "usbVideo/CameraImage.hpp"
#include "logger/Logger.hpp"

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

    bool getPipeStoreFlag(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::pipeStore) != config.end())
        {
            return config[configuration::pipeStore].as<bool>();
        }
        return true;
    }

    std::string getPipeFileName(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::pipeFileName) != config.end())
        {
            return config[configuration::pipeFileName].as<std::string>();
        }
        return "cameraCapturePipe";
    }

    std::string getCaptureOutputDir(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::captureOutputDir) != config.end())
        {
            return config[configuration::captureOutputDir].as<std::string>();
        }
        return "/tmp/cameraCapture/";
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

namespace Video
{
    constexpr int PipeFileRight = 0666;
    std::atomic_bool keep_running{ true };

    CameraProcess::CameraProcess(Logger& logger, const configuration::AppConfiguration& config)
        : m_bPipe{ getPipeStoreFlag(config) }
        , m_pipeName{ getPipeFileName(config) }
        , m_outputDir{ getCaptureOutputDir(config) }
        , m_V4l2RequestBuffersCounter{ getV4l2RequestBuffersCounter(config) }
        , m_captureFormat{ covertV4L2CaptureFormat(getV4L2CaptureFormat(config)) }
        , m_logger{logger}
        , m_cameraControl(std::make_unique<CameraControl>(getDefaultCameraDevice(config)))
    {
 
    }

    CameraProcess::~CameraProcess()
    {
        if (m_captureThread.joinable())
        {
            m_captureThread.join();
        }
    }

    bool CameraProcess::initDevice()
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
            if (m_cameraControl->getCameraFormat(m_v4l2Format) < 0)
            {
                m_cameraControl->closeDevice();
                return false;
            }
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
        if (not initDevice())
        {
            return;
        }
        outputDeviceInfo();

        if (m_bPipe && !m_pipeName.empty() && !m_outputDir.empty())
        {
            std::string pipeFile = m_outputDir + m_pipeName;
            if (not common::isFileExistent(pipeFile))
            {
                if (-1 == mkfifo(pipeFile.c_str(), PipeFileRight))
                {
                    LOG_ERROR_MSG("Error creating the pipe: {}", pipeFile);
                    m_cameraControl->closeDevice();
                    return;
                }
            }
            else
            {
                LOG_DEBUG_MSG("Pipe file have exist {}.", pipeFile);
            }
        }
        else
        {
            LOG_ERROR_MSG("pipe config false or name is empty.");
            m_cameraControl->closeDevice();
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

        /* start the capture */
        m_cameraControl->startCameraStreaming(requestBuffers);

        // start capture thread
        m_captureThread = std::thread(&CameraProcess::captureCamera, this);

        // stream
        streamCamera();
        exitProcess();
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

        if (m_bSharedMem)
        {

        }

    }

    void CameraProcess::captureCamera()
    {
        std::vector<uint8_t> rgbBuffer;

        int ready_buf;
        char cur_name[64];
        struct timeval timestamp;

        if (m_bSharedMem)
        {
           // rgb_buffer = p_shm;
        }
        else
        {
            rgbBuffer.resize(m_v4l2Format.fmt.pix.width * m_v4l2Format.fmt.pix.height * RGBCountSize);
        }

        while (keep_running)
        {
            /* Wait for the start condition */

            /* queue one buffer and 'refresh it' */
            struct v4l2_buffer v4l2Buffer;
            for (int i = 0; i < m_V4l2RequestBuffersCounter; i++)
            {
                v4l2Buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                v4l2Buffer.memory = V4L2_MEMORY_MMAP;
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
                return;
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
                return;
            }

            m_cameraControl->queryBuffer(v4l2Buffer);

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
                break;
            }

            return;
        }
    }

    void CameraProcess::streamCamera()
    {
        std::vector<uint8_t> rgbBuffer;
        if (m_bSharedMem)
        {
            
        }
        else
        {
            rgbBuffer.resize(m_v4l2Format.fmt.pix.width * m_v4l2Format.fmt.pix.height * RGBCountSize);
        }

        struct v4l2_buffer v4l2Buffer;
        for (int index = 0; index < m_V4l2RequestBuffersCounter; ++index)
        {
            v4l2Buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            v4l2Buffer.memory = V4L2_MEMORY_MMAP;
            v4l2Buffer.index = index;
            m_cameraControl->queueBuffer(v4l2Buffer);
        }

        while (keep_running)
        {
            /* get the idx of ready buffer */
            for (int i = 0; i < m_V4l2RequestBuffersCounter; i++)
            {
                /* Check if the thread should stop. */
                if (stream_finish) return;

                if (not m_cameraControl->dequeueBuffer(v4l2Buffer))
                {
                    continue;
                }
                //LOG_DEBUG_MSG("Request dequeue buffer {} ready.", v4l2Buffer.index);

                switch (checkPixelFormat())
                {
                case V4L2_PIX_FMT_YUYV:
                    /* convert data to rgb */
                    if (m_cameraControl->getRGBBuffer(rgbBuffer, v4l2Buffer, m_v4l2Format.fmt.pix.width, m_v4l2Format.fmt.pix.height))
                    {
                        LOG_DEBUG_MSG("Converted to rgb.");
                    }
                    break;
                default:
                    LOG_ERROR_MSG("Unsupported pixelformat!");
                    return;
                }

                /* make the image */

                /* create the file name */
                std::string fileName = "";
                if (m_bPipe)
                {
                    fileName = m_outputDir + m_pipeName;
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
                    break;
                }

                m_cameraControl->queueBuffer(v4l2Buffer);
            }
        }

        return NULL;
    }

} // namespace Video