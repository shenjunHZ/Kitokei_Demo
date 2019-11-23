#include <fstream>
#include "usbAudio/AudioPlaybackService.hpp"
#include "common/CommonFunction.hpp"
#include "socket/ConcreteRTPSession.hpp"

namespace
{
    const int waitForTimeout = 2;
    const int MAX_WRITE_DATA = 2048;
    const unsigned short SAMPLE_BIT_SIZE = 16;
    constexpr unsigned short BitsByte = 8;
    std::atomic_bool keep_running{true};
} // namespace

namespace usbAudio
{


    AudioPlaybackService::AudioPlaybackService(Logger& logger, const configuration::AppConfiguration& config, std::shared_ptr<endpoints::IRTPSession> rtpSession)
        : m_logger{ logger }
        , m_config{ config }
        , m_fp{ nullptr }
        , m_rtpSession{std::move(rtpSession)}
    {

    }

    AudioPlaybackService::~AudioPlaybackService()
    {
        if (m_fp)
        {
            fclose(m_fp);
            m_fp = nullptr;
        }
    }

    bool AudioPlaybackService::initAudioPlayback()
    {
        LOG_INFO_MSG(m_logger, "Start to playback audio data.");

        return startAnalysisSpeech();
    }

    bool AudioPlaybackService::startAnalysisSpeech()
    {
        configuration::SpeechRecNotifier recNotifier =
        {
            //std::bind(&ISRService::analysisResult, this, std::placeholders::_1, std::placeholders::_2),
            std::bind(&AudioPlaybackService::speechBegin, this),
            std::bind(&AudioPlaybackService::speechEnd, this, std::placeholders::_1)
        };

        int errcode = -1;
        unsigned short audioChannel = 1; // default use mono
        if (2 == audio::getAudioChannel(m_config))
        {
            audioChannel = 2;
        }
        unsigned int sampleRate = 8000; // rate must more than 8000
        if (audio::getAudioSampleRate(m_config) > sampleRate)
        {
            sampleRate = audio::getAudioSampleRate(m_config);
        }
        configuration::WaveFormatTag waveFormatTag = configuration::WaveFormatTag::WAVE_FORMAT_PCM;
        std::string audioFormat = audio::getAudioFormat(m_config);
        if ("G711a" == audioFormat)
        {
            waveFormatTag = configuration::WaveFormatTag::WAVE_FORMAT_G711a;
        }

        configuration::WAVEFORMATEX wavfmt = {
            static_cast<unsigned short>(waveFormatTag), audioChannel, sampleRate,
            static_cast<unsigned int>(sampleRate * audioChannel * SAMPLE_BIT_SIZE / BitsByte),
            static_cast<unsigned short>(audioChannel* SAMPLE_BIT_SIZE / BitsByte),
            SAMPLE_BIT_SIZE,
            0 };
            //static_cast<unsigned short>(sizeof(configuration::WAVEFORMATEX)) };
        m_sysPlayback = std::make_unique<LinuxAlsa>(m_logger, std::make_unique<configuration::WAVEFORMATEX>(wavfmt));
        // wav fmt chuck head
        m_waveHeader.bits_per_sample = wavfmt.wBitsPerSample;
        m_waveHeader.block_align = wavfmt.nBlockAlign;
        m_waveHeader.avg_bytes_per_sec = wavfmt.nAvgBytesPerSec;
        m_waveHeader.samples_per_sec = wavfmt.nSamplesPerSec;
        m_waveHeader.channels = wavfmt.nChannels;
        m_waveHeader.format_tag = wavfmt.wFormatTag;

        if (0 == m_sysPlayback->getAudioDevNum(SND_PCM_STREAM_PLAYBACK))
        {
            LOG_ERROR_MSG("Have no playback PCM device can be used.");
            return false;
        }

        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_INIT;
        m_speechRec.audioSource = configuration::SpeechAudioSource::SPEECH_DATA;
        m_speechRec.notif = std::move(recNotifier);

        if (m_sysPlayback)
        {
            errcode = m_sysPlayback->createALSAAudio(m_speechRec.alsaAudioContext, nullptr, static_cast<void*>(&m_speechRec));

            if (0 > errcode)
            {
                LOG_DEBUG_MSG("create playback failed: {}", errcode);
                errcode = static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_GENERAL);
                destroyPlayback();
                return false;
            }

            configuration::audioDevInfo devInfo = m_sysPlayback->getDefaultDev();
            devInfo = m_sysPlayback->setAudioDev(audio::getPlaybackDevice(m_config));

            errcode = m_sysPlayback->openALSAAudio(m_speechRec.alsaAudioContext, devInfo, SND_PCM_STREAM_PLAYBACK);
            if (0 != errcode)
            {
                LOG_DEBUG_MSG("playback open failed: {}", errcode);
                errcode = static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_GENERAL);
                destroyPlayback();
                return false;
            }
        }
        return true;
    }

    void AudioPlaybackService::exitAudioPlayback()
    {
        uninitAudioPlayback();
    }

    void AudioPlaybackService::uninitAudioPlayback()
    {
        if (nullptr != m_sysPlayback)
        {
            if (!m_sysPlayback->isALSAAudioStopped(m_speechRec.alsaAudioContext))
            {
                m_sysPlayback->stopALSAAudio(m_speechRec.alsaAudioContext, SND_PCM_STREAM_PLAYBACK);
            }
            m_sysPlayback->closeALSAAudio(m_speechRec.alsaAudioContext, SND_PCM_STREAM_PLAYBACK);
            m_sysPlayback->destroyALSAAudio(m_speechRec.alsaAudioContext);
        }
    }

    void AudioPlaybackService::destroyPlayback()
    {
        if (m_sysPlayback)
        {
            m_sysPlayback->destroyALSAAudio(m_speechRec.alsaAudioContext);
        }
    }

    int AudioPlaybackService::audioStartPlaying()
    {
        if (m_speechRec.speechState >= configuration::SpeechState::SPEECH_STATE_STARTED)
        {
            LOG_DEBUG_MSG("already STARTED.");
            return static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_ALREADY);
        }
        speechBegin();

        if (nullptr != m_sysPlayback)
        {
            int ret = m_sysPlayback->startALSAAudio(m_speechRec.alsaAudioContext, SND_PCM_STREAM_PLAYBACK);
            if (ret != 0)
            {
                LOG_DEBUG_MSG("start playback failed: {}", ret);
                return static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_PLAYBACKFAIL);
            }
        }
        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_STARTED;

        m_speechRec.alsaAudioContext.audioThread = std::thread([this, &alsaAudioContext = m_speechRec.alsaAudioContext]()
        {
            //this->speechStartFromFile();
            this->speechStartFromRTP();
        });

        return 0;
    }

    int AudioPlaybackService::audioStopPlaying()
    {
        if (m_speechRec.speechState < configuration::SpeechState::SPEECH_STATE_STARTED)
        {
            LOG_DEBUG_MSG("Not started or already stopped.");
            return 0;
        }
        keep_running = false;
        if (m_speechRec.alsaAudioContext.audioThread.joinable())
        {
            m_speechRec.alsaAudioContext.audioThread.join();
        }

        if ( nullptr != m_sysPlayback)
        {
            int ret = m_sysPlayback->stopALSAAudio(m_speechRec.alsaAudioContext, SND_PCM_STREAM_PLAYBACK);

            if (ret != 0)
            {
                LOG_ERROR_MSG("Stop failed!");
                return static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_PLAYBACKFAIL);
            }
            waitForPlaybackStop(m_speechRec.alsaAudioContext, waitForTimeout);
        }

        speechEnd(configuration::SpeechEndReason::END_REASON_VAD_DETECT);
        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_INIT;
        return 0;
    }

    /* after stop playback, there are still some data callbacks */
    void AudioPlaybackService::waitForPlaybackStop(configuration::ALSAAudioContext& playback, unsigned int timeout_ms /*= -1*/)
    {
        if (nullptr == m_sysPlayback)
        {
            return;
        }
        while (!m_sysPlayback->isALSAAudioStopped(playback))
        {
            sleep(1);
            if (timeout_ms != (unsigned int)-1)
            {
                if (0 == timeout_ms--)
                {
                    break;
                }
            }
        }
    }

    void AudioPlaybackService::speechBegin()
    {
        std::string audioFile = audio::getPlaybackAudioFile(m_config);
        if (not audioFile.empty())
        {
            m_fp = fopen(audioFile.c_str(), "rb");
            if (nullptr == m_fp)
            {
                LOG_ERROR_MSG("open audio file {} error.", audioFile.c_str());
                return;
            }
        }

        LOG_INFO_MSG(m_logger, "....Start pcm playing....");
        size_t size = fread(&m_waveHeader, 1, sizeof(m_waveHeader), m_fp);
        printWavHdr(size);
        m_rtpSession->startRTPPolling();
    }

    void AudioPlaybackService::speechEnd(const configuration::SpeechEndReason& reason)
    {
        if (configuration::SpeechEndReason::END_REASON_VAD_DETECT == reason)
        {
            LOG_INFO_MSG(m_logger, "Playback speaking done.");
        }
        else
        {
            LOG_INFO_MSG(m_logger, "Recognizer error: {}", static_cast<int>(reason));
        }
        if (m_fp)
        {
            fclose(m_fp);
            LOG_INFO_MSG(m_logger, "....Stop pcm playing....");
            m_fp = nullptr;
        }
    }

    void AudioPlaybackService::speechStartFromFile()
    {
        if (m_speechRec.speechState < configuration::SpeechState::SPEECH_STATE_STARTED)
        {
            LOG_ERROR_MSG("Speech state issue.");
            return;
        }

        readFileAudioData();
    }

    int AudioPlaybackService::readFileAudioData()
    {
        size_t frames = m_speechRec.alsaAudioContext.periodFrames;
        size_t size = (m_speechRec.alsaAudioContext.periodFrames * m_speechRec.alsaAudioContext.bitsPerFrame / BitsByte);

        while (keep_running)
        {
            int ret = fread(m_speechRec.alsaAudioContext.audioBuffer, 1, size, m_fp);
            if (1 > ret)
            {
                LOG_DEBUG_MSG("reading the end.");
                break;
            }

            // write to PCM device
            if (m_sysPlayback)
            {
                int errCode = m_sysPlayback->readAudioDataToPCM(m_speechRec.alsaAudioContext);
                if (0 > errCode)
                {
                    endPlaybackWithReason(errCode);
                    return errCode;
                }
            }
        }
        endPlaybackWithReason(static_cast<int>(configuration::SpeechEndReason::END_REASON_VAD_DETECT));
        return 0;
    }

    void AudioPlaybackService::endPlaybackWithReason(const int& errorCode)
    {
        if (m_sysPlayback)
        {
            m_sysPlayback->stopALSAAudio(m_speechRec.alsaAudioContext, SND_PCM_STREAM_PLAYBACK);
        }

        speechEnd(static_cast<configuration::SpeechEndReason>(errorCode));
        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_INIT;
    }

    void AudioPlaybackService::printWavHdr(size_t& size)
    {
        LOG_DEBUG_MSG("read size: {}", size);
        LOG_DEBUG_MSG("RiffID: {}{}{}{}", m_waveHeader.riff[0], m_waveHeader.riff[1], m_waveHeader.riff[2], m_waveHeader.riff[3]);
        LOG_DEBUG_MSG("RiffSize: {}", m_waveHeader.chunk_size);
        LOG_DEBUG_MSG("WaveID: {}{}{}{}", m_waveHeader.wave[0], m_waveHeader.wave[1], m_waveHeader.wave[2], m_waveHeader.wave[3]);
        LOG_DEBUG_MSG("FmtID: {}{}{}{}", m_waveHeader.fmt[0], m_waveHeader.fmt[1], m_waveHeader.fmt[2], m_waveHeader.fmt[3]);
        LOG_DEBUG_MSG("FmtSize: {}", m_waveHeader.fmt_chunk_size);
        LOG_DEBUG_MSG("wFormatTag: {}", m_waveHeader.format_tag);
        LOG_DEBUG_MSG("nChannels: {}", m_waveHeader.channels);
        LOG_DEBUG_MSG("nSamplesPerSec: {}", m_waveHeader.samples_per_sec);
        LOG_DEBUG_MSG("nAvgBytesPerSec: {}", m_waveHeader.avg_bytes_per_sec);
        LOG_DEBUG_MSG("nBlockAlign: {}", m_waveHeader.block_align);
        LOG_DEBUG_MSG("wBitsPerSample: {}", m_waveHeader.bits_per_sample);
        LOG_DEBUG_MSG("DataID: {}{}{}{}", m_waveHeader.data[0], m_waveHeader.data[1], m_waveHeader.data[2], m_waveHeader.data[3]);
        LOG_DEBUG_MSG("nDataBytes: {}", m_waveHeader.data_chunk_size);
    }

    void AudioPlaybackService::speechStartFromRTP()
    {
        if (m_speechRec.speechState < configuration::SpeechState::SPEECH_STATE_STARTED)
        {
            LOG_ERROR_MSG("Speech state issue.");
            return;
        }
        while (keep_running)
        {
            configuration::RTPSessionDatas rtpSessionDatas;
            m_rtpSession->receivePacket(rtpSessionDatas);

            m_rtpSession->startRTPPolling();
        }
    }

} // namespace usbAudio

int usbAudio::AudioPlaybackService::readRTPAudioData(configuration::RTPSessionDatas& rtpSessionDatas)
{
    size_t frames = m_speechRec.alsaAudioContext.periodFrames;
    size_t size = (frames * m_speechRec.alsaAudioContext.bitsPerFrame / BitsByte);

    return 0;
}
