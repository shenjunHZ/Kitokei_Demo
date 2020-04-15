#include "common/CommonFunction.hpp"
#include "usbAudio/LinuxAlsa.hpp"
#include "usbVideo/AudioService.hpp"

namespace
{
    constexpr unsigned short audioChannel = 1; // use mono
    constexpr unsigned int sampleRate = 8000;  // rate use 8000
    constexpr int bitsByte = 8;
    constexpr int sampleBit = 16;
    constexpr int MAX_WRITE_DATA = 2048;
    constexpr int waitForTimeout = 2;
} // namespace
namespace usbVideo
{
    AudioService::AudioService(Logger& logger, const configuration::AppConfiguration& config)
        : m_logger{logger}
        , m_config{config}
        , m_outputDir{ common::getCaptureOutputDir(config) }
    {

    }

    AudioService::~AudioService()
    {

    }

    bool AudioService::initAudioRecord()
    {
        LOG_INFO_MSG(m_logger, "Init audio record for video file.");

        int errcode = -1;
        configuration::WaveFormatTag waveFormatTag = configuration::WaveFormatTag::WAVE_FORMAT_PCM;
        configuration::WAVEFORMATEX wavfmt = {
            static_cast<unsigned short>(waveFormatTag), audioChannel, sampleRate,
            static_cast<unsigned int>(sampleRate * audioChannel * sampleBit / bitsByte),
            static_cast<unsigned short>(audioChannel* sampleBit / bitsByte),
            sampleBit,
            0 };
        m_sysRec = std::make_unique<usbAudio::LinuxAlsa>(m_logger, std::make_unique<configuration::WAVEFORMATEX>(wavfmt));

        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_INIT;
        m_speechRec.audioSource = configuration::SpeechAudioSource::SPEECH_MIC;
        std::string fileName = m_outputDir + m_pipeName;
        m_fd = fopen(fileName.c_str(), "wb");
        if (nullptr == m_fd)
        {
            LOG_ERROR_MSG("Open audio pipe file fail {}.", std::strerror(errno));
            return false;
        }

        if (m_sysRec)
        {
            errcode = m_sysRec->createALSAAudio(m_speechRec.alsaAudioContext, std::bind(&AudioService::recordCallback, this, std::placeholders::_1),
                static_cast<void*>(&m_speechRec));

            if (0 > errcode)
            {
                LOG_DEBUG_MSG("create recorder failed: {}", errcode);
                errcode = static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_RECORDFAIL);
                destroyRecorder();
                return errcode;
            }

            configuration::audioDevInfo devInfo = m_sysRec->getDefaultDev();
            devInfo = m_sysRec->setAudioDev(video::getDefaultAudioRecord(m_config));
            errcode = m_sysRec->openALSAAudio(m_speechRec.alsaAudioContext, devInfo, SND_PCM_STREAM_CAPTURE);
            if (0 != errcode)
            {
                LOG_DEBUG_MSG("recorder open failed: {}", errcode);
                errcode = static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_RECORDFAIL);
                destroyRecorder();
                return errcode;
            }
        }
        return false;
    }

    void AudioService::exitAudioRecord()
    {
        if (nullptr != m_sysRec)
        {
            if (!m_sysRec->isALSAAudioStopped(m_speechRec.alsaAudioContext))
            {
                m_sysRec->stopALSAAudio(m_speechRec.alsaAudioContext, SND_PCM_STREAM_CAPTURE);
            }
            m_sysRec->closeALSAAudio(m_speechRec.alsaAudioContext, SND_PCM_STREAM_CAPTURE);
            m_sysRec->destroyALSAAudio(m_speechRec.alsaAudioContext);
        }
    }

    int AudioService::audioStartListening()
    {
        if (m_speechRec.speechState >= configuration::SpeechState::SPEECH_STATE_STARTED)
        {
            LOG_WARNING_MSG("Camera audio already started.");
            return static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_ALREADY);
        }

        if (configuration::SpeechAudioSource::SPEECH_MIC == m_speechRec.audioSource && nullptr != m_sysRec)
        {
            int ret = m_sysRec->startALSAAudio(m_speechRec.alsaAudioContext, SND_PCM_STREAM_CAPTURE);
            if (ret != 0)
            {
                LOG_DEBUG_MSG("Start camera audio record failed: {}", ret);
                return static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_RECORDFAIL);
            }
        }

        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_STARTED;
        return static_cast<int>(configuration::ALSAErrorCode::ALSA_SUCCESS);
    }

    int AudioService::audioStopListening()
    {
        if (m_speechRec.speechState < configuration::SpeechState::SPEECH_STATE_STARTED)
        {
            LOG_WARNING_MSG("Not started or already stopped.");
            return 0;
        }

        if (m_speechRec.audioSource == configuration::SpeechAudioSource::SPEECH_MIC && nullptr != m_sysRec)
        {
            int ret = m_sysRec->stopALSAAudio(m_speechRec.alsaAudioContext, SND_PCM_STREAM_CAPTURE);

            if (ret != 0)
            {
                LOG_ERROR_MSG("Stop alsa audo failed!");
                return static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_RECORDFAIL);
            }
            waitForRecStop(m_speechRec.alsaAudioContext, waitForTimeout);
        }

        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_INIT;
        return 0;
    }

    void AudioService::destroyRecorder()
    {
        if (m_sysRec)
        {
            m_sysRec->destroyALSAAudio(m_speechRec.alsaAudioContext);
        }
    }

    void AudioService::recordCallback(std::string& data)
    {
        if (data.empty())
        {
            return;
        }
        if (m_speechRec.speechState < configuration::SpeechState::SPEECH_STATE_STARTED)
        {
            LOG_ERROR_MSG("Speech state issue.");
            return;
        }

        int errcode = writeAudioData(data);
        if (errcode < 0)
        {
            endRecordOnError(errcode);
            return;
        }
    }

    int AudioService::writeAudioData(const std::string& data)
    {
        if (data.empty())
        {
            LOG_ERROR_MSG("Audio data is empty.");
            return 0;
        }
        int dataSize = data.size();
        int dataIndex = 0;
        size_t writeData = 0;
        while (dataSize > 0)
        {
            if (dataSize > PIPE_BUF)
            {
                writeData = fwrite(&data[dataIndex], 1, PIPE_BUF, m_fd);
                if (writeData < PIPE_BUF)
                {
                    LOG_ERROR_MSG("Error writing audio data {}, should data {}.", writeData, PIPE_BUF);
                }
            }
            else
            {
                writeData = fwrite(&data[dataIndex], 1, dataSize, m_fd);
                if (writeData < dataSize)
                {
                    LOG_ERROR_MSG("Error writing audio data {}, should data {}.", writeData, dataSize);
                }
            }
            dataSize -= writeData;
            dataIndex += writeData;
        }

       // LOG_DEBUG_MSG("Success write the audio data {} to pipe.", dataIndex);
        return 0;
    }

    void AudioService::endRecordOnError(const int& errorCode)
    {
        if (m_speechRec.audioSource == configuration::SpeechAudioSource::SPEECH_MIC
            && m_sysRec)
        {
            m_sysRec->stopALSAAudio(m_speechRec.alsaAudioContext, SND_PCM_STREAM_CAPTURE);
        }

        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_INIT;
    }

    void AudioService::waitForRecStop(configuration::ALSAAudioContext& recorder, unsigned int timeout_ms /*= -1*/)
    {
        if (nullptr == m_sysRec)
        {
            return;
        }
        while (!m_sysRec->isALSAAudioStopped(recorder))
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

} //namespace usbVideo