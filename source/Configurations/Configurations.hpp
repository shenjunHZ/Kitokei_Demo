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
#define RTP_CONFIG_PREFIX "rtp"

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
    constexpr auto playbackDevice         = AUDIO_CONFIG_PREFIX ".playbackDevice";
    constexpr auto readTestAudioFile      = AUDIO_CONFIG_PREFIX ".readTestAudioFile";
    constexpr auto audioFormat            = AUDIO_CONFIG_PREFIX ".audioFormat";
    constexpr auto remoteRTPPort      = RTP_CONFIG_PREFIX ".remoteRTPPort";
    constexpr auto localRTPPort       = RTP_CONFIG_PREFIX ".localRTPPort";
    constexpr auto remoteRTPIpAddress = RTP_CONFIG_PREFIX ".remoteRTPIpAddress";

 /*****************socket struct**************************/
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
    struct audioDevInfo
    {
        std::string devName;
        int devIndex;

        audioDevInfo()
        {
            devName = "";
            devIndex = -1;
        }
    };

    /* Do not change the sequence */
    enum class ALSAState
    {
        ALSA_STATE_CREATED,	/* Init		*/
        ALSA_STATE_CLOSING,
        ALSA_STATE_READY,		/* Opened	*/
        ALSA_STATE_STOPPING,	/* During Stop	*/
        ALSA_STATE_STARTING,	/* Started	*/
    };

    /* error code */
    enum class ALSAErrorCode
    {
        ALSA_ERR_BASE = -1,
        ALSA_ERR_GENERAL = -2,
        ALSA_ERR_MEMFAIL = -3,
        ALSA_ERR_INVAL = -4,
        ALSA_ERR_NOT_READY = -5,
        ALSA_ERR_RECORDFAIL = -6,
        ALSA_ERR_PLAYBACKFAIL = -7,
        ALSA_ERR_ALREADY = -8,
        ALSA_ERR_WRITEDATA = -9,
        ALSA_ERR_File_Descriptor_Bad_State = -77
    };

    /* recorder object. */
    struct ALSAAudioContext
    {
        std::function<void(std::string& data)> onDataInd; // record callback
        void* userCallbackPara{nullptr};   // save SpeechRecord point
        ALSAState alsaState;           // internal record state
        void* pcmHandle{nullptr};      // snd_pcm_t point

        void*        bufheader{nullptr}; // recorder buffer
        unsigned int bufcount{0};

        // for linux
        std::thread audioThread;
        unsigned int bufferTime{0};
        unsigned int periodTime{0};
        size_t       periodFrames{0}; // The number of frames generated by a channel 1 second
        size_t       bufferFrames{0};
        int          bitsPerFrame{0}; // bit data per frame = bit per sample * channel
        char*        audioBuffer{nullptr};
    };

    enum class SpeechAudioSource
    {
        SPEECH_MIC,	 /* write data from mic */
        SPEECH_USER, /* write data from user by calling API */
        SPEECH_DATA  /* read data */
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
        SpeechState       speechState;  // record and playback
        ALSAAudioContext  alsaAudioContext;
        SpeechRecNotifier notif;
    };

#ifndef WAVE_FORMAT_PCM  

    enum class WaveFormatTag
    {
        WAVE_FORMAT_PCM = 1,
        WAVE_FORMAT_G711a = 6
    };

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

#pragma pack(push)
#pragma pack(2)
    /* wav audio header format */
    typedef struct _wave_pcm_hdr
    {
        char            riff[4];                // = "RIFF"
        int             chunk_size;             // = File total size - 8
        char            wave[4];                // = "WAVE"
        // fmt sub-chunk
        char            fmt[4];                 // = "fmt"
        int             fmt_chunk_size;         // = the size of the next structure : 16 for PCM, or 18 or 40
        short int       format_tag;             // = PCM : 1
        short int       channels;               // = channel number: Mono = 1, Stereo = 2, etc.
        int             samples_per_sec;        // = sample rate : 8000, 16000, 44100, etc.
        int             avg_bytes_per_sec;      // = bytes per second : samples_per_sec * bits_per_sample / 8 * channels
        short int       block_align;            // = bytes per sampling point : bits_per_sample / 8 * channels
        short int       bits_per_sample;        // = quantized bit number: 8bits, 16bits, etc.
        short int       cbSize;
        // fact sub-chunk
        char            fact[4];
        int             fact_chunk_size;
        int             dwSampleLength;
        // data sub-chunk
        char            data[4];                // = "data";
        int             data_chunk_size;        // = pure data length : FileSize - 44 for PCM

        _wave_pcm_hdr()
            : riff{ 'R', 'I', 'F', 'F' }
            , wave{ 'W', 'A', 'V', 'E' }
            , fmt{ 'f', 'm', 't', ' ' }
            , fact{'f', 'a', 'c', 't'}
            , data{ 'd', 'a', 't', 'a' }
        {
            chunk_size = 0;
            fmt_chunk_size = 18;
            format_tag = 1;
            channels = 1;
            samples_per_sec = 16000;
            avg_bytes_per_sec = 32000;
            block_align = 2;
            bits_per_sample = 16;
            cbSize = 0;

            fact_chunk_size = 4;
            data_chunk_size = 0;
        }

        void clear()
        {
            chunk_size = 0;
            data_chunk_size = 0;
            dwSampleLength = 0;
        }
    } wavePCMHeader;

#pragma pack(pop)
} // namespace configuration