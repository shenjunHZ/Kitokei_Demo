#pragma once
#include <functional>
#include <string>
#include <thread>
#include <memory>

#define LOG_CONFIG_PREFIX "log"
#define VIDEO_CONFIG_PREFIX "video"
#define V4L2_CONFIG_PREFIX "V4L2"
#define SOCKET_CONFIG_PREFIX "socket"
#define AUDIO_CONFIG_PREFIX "audio"

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
    constexpr auto audioName              = AUDIO_CONFIG_PREFIX ".audioName";
    constexpr auto enableWriteAudioToFile = AUDIO_CONFIG_PREFIX ".enableWriteAudioToFile";
    constexpr auto audioDevice            = AUDIO_CONFIG_PREFIX ".audioDevice";
    constexpr auto audioChannel           = AUDIO_CONFIG_PREFIX ".audioChannel";
    constexpr auto sampleRate             = AUDIO_CONFIG_PREFIX ".sampleRate";

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

 /*****************video struct**************************/
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
/*****************audio struct**************************/
    struct recordDevInfo
    {
        std::string devName;
        int devIndex;

        recordDevInfo()
        {
            devName = "";
            devIndex = -1;
        }
    };

    /* Do not change the sequence */
    enum class RecordState
    {
        RECORD_STATE_CREATED,	/* Init		*/
        RECORD_STATE_CLOSING,
        RECORD_STATE_READY,		/* Opened	*/
        RECORD_STATE_STOPPING,	/* During Stop	*/
        RECORD_STATE_RECORDING,	/* Started	*/
    };

    /* error code */
    enum class RecordErrorCode
    {
        RECORD_ERR_BASE = -1,
        RECORD_ERR_GENERAL = -2,
        RECORD_ERR_MEMFAIL = -3,
        RECORD_ERR_INVAL = -4,
        RECORD_ERR_NOT_READY = -5,
        RECORD_ERR_RECORDFAIL = -6,
        RECORD_ERR_ALREADY = -7
    };

    /* recorder object. */
    struct AudioRecorder
    {
        std::function<void(const std::string& data)> onDataInd; // record callback
        RecordState recordState;    // internal record state
        void* userCallbackPara{};   // save SpeechRecord point
        void* pcmHandle{};       // snd_pcm_t point

        // for windows
        void*        bufheader;            // recorder buffer
        unsigned int bufcount;
        void*        recordHandleThread;   // thread call back handle for windows

        // for linux
        std::thread recThread;
        unsigned int bufferTime;
        unsigned int periodTime;
        size_t       periodFrames; // The number of frames generated by a channel 1 second
        size_t       bufferFrames;
        int          bitsPerFrame; // bit data per frame = bit per sample * channel
        char*        audioBuffer{};
    };

    enum class SpeechAudioSource
    {
        SPEECH_MIC,	/* write data from mic */
        SPEECH_USER	/* write data from user by calling API */
    };

    /* internal state */
    enum class SpeechState
    {
        SPEECH_STATE_INIT,
        SPEECH_STATE_STARTED
    };

    enum class SpeechEndReason
    {
        END_REASON_VAD_DETECT = 0  /* detected speech done  */
    };

    struct SpeechRecNotifier
    {
        std::function<void()> onSpeechBegin{};
        std::function<void(const SpeechEndReason& reason)> onSpeechEnd{};
    };

    struct SpeechRecord
    {
        SpeechAudioSource audioSource;  /* from mic or manual  stream write */
        SpeechState       speechState;
        AudioRecorder     audioRecorder;
        SpeechRecNotifier notif;
    };

#ifndef WAVE_FORMAT_PCM  
#define WAVE_FORMAT_PCM  1
    struct WAVEFORMATEX
    {
        unsigned short	  wFormatTag;      // format type
        unsigned short    nChannels;       // number of channels (i.e. mono, stereo...)
        unsigned int      nSamplesPerSec;  // sample rate
        unsigned int      nAvgBytesPerSec; // for buffer estimation
        unsigned short	  nBlockAlign;     // block size of data
        unsigned short    wBitsPerSample;  // number of bits per sample of mono data
        unsigned short    cbSize;          // the count in bytes of the size of
                                           // extra information (after cbSize)
    };
#endif

    /* wav audio header format */
    typedef struct _wave_pcm_hdr
    {
        char            riff[4];                // = "RIFF"
        int             size_8;                 // = FileSize - 8
        char            wave[4];                // = "WAVE"
        // fmt chunk
        char            fmt[4];                 // = "fmt"
        int             fmt_size;               // = the size of the next structure : 16
        short int       format_tag;             // = PCM : 1
        short int       channels;               // = channel number: 1
        int             samples_per_sec;        // = sample rate : 8000 | 6000 | 11025 | 16000
        int             avg_bytes_per_sec;      // = bytes per second : samples_per_sec * bits_per_sample / 8 * channels
        short int       block_align;            // = bytes per sampling point : bits_per_sample / 8 * channels
        short int       bits_per_sample;        // = quantized bit number: 8 | 16, rounds up to 8 * M
        // data chunk
        char            data[4];                // = "data";
        int             data_size;              // = pure data length : FileSize - 44

        _wave_pcm_hdr()
            : riff{ 'R', 'I', 'F', 'F' }
            , wave{ 'W', 'A', 'V', 'E' }
            , fmt{ 'f', 'm', 't', ' ' }
            , data{ 'd', 'a', 't', 'a' }
        {
            size_8 = 0;
            fmt_size = 16;
            format_tag = 1;
            channels = 1;
            samples_per_sec = 16000;
            avg_bytes_per_sec = 32000;
            block_align = 2;
            bits_per_sample = 16;
            data_size = 0;
        }

        void clear()
        {
            size_8 = 0;
            data_size = 0;
        }
    } wavePCMHeader;
} // namespace configuration