#pragma once

#include <string>

#define LOG_CONFIG_PREFIX "log"
#define VIDEO_CONFIG_PREFIX "video"
#define V4L2_CONFIG_PREFIX "V4L2"
#define SOCKET_CONFIG_PREFIX "socket"

namespace configuration
{
    constexpr auto logFilePath = LOG_CONFIG_PREFIX ".logFilePath";
    constexpr auto cameraDevice       = VIDEO_CONFIG_PREFIX ".cameraDevice";
    constexpr auto enableCameraStream = VIDEO_CONFIG_PREFIX ".enableCameraStream";
    constexpr auto pipeFileName       = VIDEO_CONFIG_PREFIX ".pipeFileName";
    constexpr auto captureOutputDir   = VIDEO_CONFIG_PREFIX ".captureOutputDir";
    constexpr auto videoName          = VIDEO_CONFIG_PREFIX ".videoName";
    constexpr auto videoFPS           = VIDEO_CONFIG_PREFIX ".videoFPS";
    constexpr auto videoBitRate       = VIDEO_CONFIG_PREFIX ".videoBitRate";
    constexpr auto videoTimes         = VIDEO_CONFIG_PREFIX ".videoTimes";
    constexpr auto V4l2RequestBuffersCounter = V4L2_CONFIG_PREFIX ".V4l2RequestBuffersCounter";
    constexpr auto V4L2CaptureFormat         = V4L2_CONFIG_PREFIX ".V4L2CaptureFormat";
    constexpr auto captureWidth              = V4L2_CONFIG_PREFIX ".captureWidth";
    constexpr auto captureHeight             = V4L2_CONFIG_PREFIX ".captureHeight";
    constexpr auto chessBoardServerAddress = SOCKET_CONFIG_PREFIX ".chessBoardServerAddress";
    constexpr auto chessBoardServerPort    = SOCKET_CONFIG_PREFIX ".chessBoardServerPort";
    constexpr auto kitokeiLocalAddress     = SOCKET_CONFIG_PREFIX ".kitokeiLocalAddress";
    constexpr auto kitokeiLocalPort        = SOCKET_CONFIG_PREFIX ".kitokeiLocalPort";

    struct AppAddresses
    {
        std::string chessBoardServerAddress;
        unsigned int chessBoardServerPort;

        std::string kitokeiLocalAddress;
        unsigned int kitokeiLocalPort;
    };

    struct TcpConfiguration
    {

    };

    struct imageBuffer
    {
        imageBuffer()
        {
            startBuffer = nullptr;
            bufferLength = 0;
        }

        void *startBuffer;
        uint32_t bufferLength;
    };

    enum class captureFormat
    {
        CAPTURE_FORMAT_JPG,
        CAPTURE_FORMAT_PNG,
        CAPTURE_FORMAT_BMP,
        CAPTURE_FORMAT_RGB,
        CAPTURE_FORMAT_RAW,
        CAPTURE_FORMAT_LAST
    };

    struct bestFrameSize
    {
        uint32_t frameWidth;
        uint32_t frameHeight;
        bool bBestFrame;

        bestFrameSize()
        {
            frameWidth = 0;
            frameHeight = 0;
            bBestFrame = false;
        }
    };

    struct BMPfileMagic 
    {
        unsigned char magic[2];
    };

    struct BMPfileHeader
    {
        uint32_t filesz;
        uint16_t creator1;
        uint16_t creator2;
        uint32_t bmp_offset;
    };

    struct BMPdibV3Heade
    {
        uint32_t header_sz;
        uint32_t width;
        uint32_t height;
        uint16_t nplanes;
        uint16_t bitspp;
        uint32_t compress_type;
        uint32_t bmp_bytesz;
        uint32_t hres;
        uint32_t vres;
        uint32_t ncolors;
        uint32_t nimpcolors;
    };
} // namespace configuration