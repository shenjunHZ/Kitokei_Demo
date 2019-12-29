#include <fstream>
#include "usbAudio/AudioRecordService.hpp"
#include "common/CommonFunction.hpp"
#include "common/G711Codec.hpp"
#include "socket/ConcreteRTPSession.hpp"

namespace
{
    const int waitForTimeout = 2;
    const int MAX_WRITE_DATA = 2048;
    const int MAX_SOCKET_DATA = 1046;
    constexpr unsigned short BitsByte = 8;
    std::atomic_bool keep_running{ true };
} // namespace

namespace usbAudio
{
    std::string AudioRecordService::TimeStamp::now() const
    {
        auto time = std::time(nullptr);
        char string[30]{};
        std::strftime(string, sizeof(string), "%F_%T", std::localtime(&time));
        return string;
    }

    AudioRecordService::AudioRecordService(Logger& logger, const configuration::AppConfiguration& config, std::shared_ptr<endpoints::IRTPSession> rtpSession)
        : m_logger{ logger }
        , m_config { config }
        , m_timeStamp{ std::make_unique<TimeStamp>() }
        , m_fp{nullptr}
        , m_rtpSession{ std::move(rtpSession) }
        , m_rtpSendThread{ std::thread([this]() {this->sendAudioData(""); }) }
    {
        if (m_rtpSession)
        {
            jrtplib::RTPSessionParams rtpSessionParams;
            rtpSessionParams.SetOwnTimestampUnit(1.0 / audio::getAudioSampleRate(m_config));
            rtpSessionParams.SetAcceptOwnPackets(true);
            // IMPORTANT: The local timestamp unit MUST be set, otherwise
            // RTCP Sender Report info will be calculated wrong
            // In this case, we'll be sending 10 samples each second, so we'll
            // put the timestamp unit to (1.0/10.0)

            m_rtpSession->createRTPSession(rtpSessionParams);
        }
    }

    AudioRecordService::~AudioRecordService()
    {
        if (m_fp)
        {
            fclose(m_fp);
            m_fp = nullptr;
        }
    }

    bool AudioRecordService::initAudioRecord()
    {
        LOG_INFO_MSG(m_logger, "Start to record audio data.");

        return startAnalysisMic();
    }

    bool AudioRecordService::startAnalysisMic()
    {
        configuration::SpeechRecNotifier recNotifier =
        {
            //std::bind(&ISRService::analysisResult, this, std::placeholders::_1, std::placeholders::_2),
            std::bind(&AudioRecordService::speechBegin, this),
            std::bind(&AudioRecordService::speechEnd, this, std::placeholders::_1)
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
			m_waveHeader.format_tag = static_cast<unsigned short>(waveFormatTag);
        }
		else if ("G711u" == audioFormat)
		{
			waveFormatTag = configuration::WaveFormatTag::WAVE_FORMAT_G711u;
			m_waveHeader.format_tag = static_cast<unsigned short>(waveFormatTag);
		}

        configuration::WAVEFORMATEX wavfmt = {
            static_cast<unsigned short>(waveFormatTag), audioChannel, sampleRate,
            static_cast<unsigned int>(sampleRate * audioChannel * audio::getSampleBit(m_config) / BitsByte),
            static_cast<unsigned short>(audioChannel* audio::getSampleBit(m_config) / BitsByte),
			audio::getSampleBit(m_config),
            0 };
            //static_cast<unsigned short>(sizeof(configuration::WAVEFORMATEX)) };
        m_sysRec = std::make_unique<LinuxAlsa>(m_logger, std::make_unique<configuration::WAVEFORMATEX>(wavfmt));
        // wav fmt chuck head
        m_waveHeader.bits_per_sample   = wavfmt.wBitsPerSample;
        m_waveHeader.block_align       = wavfmt.nBlockAlign;
        m_waveHeader.avg_bytes_per_sec = wavfmt.nAvgBytesPerSec;
		if ("PCM" != audioFormat)
		{
			m_waveHeader.bits_per_sample /= 2;
			m_waveHeader.block_align /= 2;
			m_waveHeader.avg_bytes_per_sec /= 2;
		}

        m_waveHeader.samples_per_sec   = wavfmt.nSamplesPerSec;
        m_waveHeader.channels          = wavfmt.nChannels;

        if (0 == m_sysRec->getAudioDevNum(SND_PCM_STREAM_CAPTURE))
        {
            LOG_ERROR_MSG("Have no record PCM device can be used.");
            return false;
        }

        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_INIT;
        m_speechRec.audioSource = configuration::SpeechAudioSource::SPEECH_MIC;
        m_speechRec.notif = std::move(recNotifier);

        if (m_sysRec)
        {
            errcode = m_sysRec->createALSAAudio(m_speechRec.alsaAudioContext, std::bind(&AudioRecordService::recordCallback, this, std::placeholders::_1),
                static_cast<void*>(&m_speechRec));

            if (0 > errcode)
            {
                LOG_DEBUG_MSG("create recorder failed: {}", errcode);
                errcode = static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_RECORDFAIL);
                destroyRecorder();
                return errcode;
            }

            configuration::audioDevInfo devInfo = m_sysRec->getDefaultDev();
            devInfo = m_sysRec->setAudioDev(audio::getAudioDevice(m_config));
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

    void AudioRecordService::exitAudioRecord()
    {
        UninitAudioRecord();
    }

    void AudioRecordService::UninitAudioRecord()
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

    void AudioRecordService::destroyRecorder()
    {
        if (m_sysRec)
        {
            m_sysRec->destroyALSAAudio(m_speechRec.alsaAudioContext);
        }
    }

    int AudioRecordService::audioStartListening()
    {
        if (m_speechRec.speechState >= configuration::SpeechState::SPEECH_STATE_STARTED)
        {
            LOG_WARNING_MSG("already STARTED.");
            return static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_ALREADY);
        }
        speechBegin();

        if (configuration::SpeechAudioSource::SPEECH_MIC == m_speechRec.audioSource && nullptr != m_sysRec)
        {
            int ret = m_sysRec->startALSAAudio(m_speechRec.alsaAudioContext, SND_PCM_STREAM_CAPTURE);
            if (ret != 0)
            {
                LOG_DEBUG_MSG("start record failed: {}", ret);
                return static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_RECORDFAIL);
            }
        }

        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_STARTED;
        return 0;
    }

    int AudioRecordService::audioStopListening()
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
                LOG_ERROR_MSG("Stop failed!");
                return static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_RECORDFAIL);
            }
            waitForRecStop(m_speechRec.alsaAudioContext, waitForTimeout);
        }
        speechEnd(configuration::SpeechEndReason::END_REASON_VAD_DETECT);

        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_INIT;
        return 0;
    }

    void AudioRecordService::speechBegin()
    {
        m_result = "";
        if (audio::enableAudioWriteToFile(m_config))
        {
            std::string outputFile = common::getCaptureOutputDir(m_config) + audio::getAudioName(m_config) + "_" + m_timeStamp->now() + ".wav";
            if (openOutputAudioFile(outputFile))
            {
                m_waveHeader.chunkClear();
                fwrite(&m_waveHeader, sizeof(m_waveHeader), 1, m_fp);
            }
        }
        m_rtpSession->startRTPPolling();
        LOG_INFO_MSG(m_logger, "....Start Record Listening....");
    }

    void AudioRecordService::speechEnd(const configuration::SpeechEndReason& reason)
    {
        if (configuration::SpeechEndReason::END_REASON_VAD_DETECT == reason)
        {
            LOG_INFO_MSG(m_logger, "Record speaking done.");
        }
        else
        {
            LOG_INFO_MSG(m_logger, "Recognizer error: {}", static_cast<int>(reason));
        }
        if (m_fp)
        {
            m_waveHeader.dwSampleLength = m_waveHeader.data_chunk_size / (audio::getSampleBit(m_config) / BitsByte);
            /* fixed size of wav file header data */
            m_waveHeader.chunk_size += m_waveHeader.data_chunk_size + (sizeof(m_waveHeader) - 8);
            /* write the corrected data back to the file header.
            The audio file is in wav format.*/
            fseek(m_fp, sizeof(m_waveHeader.riff), SEEK_SET);
            fwrite(&m_waveHeader.chunk_size, sizeof(m_waveHeader.chunk_size), 1, m_fp);
            // point move to dwSampleLength position
            int offsetPosition = sizeof(m_waveHeader.data_chunk_size) + sizeof(m_waveHeader.data) + sizeof(m_waveHeader.dwSampleLength);
            fseek(m_fp, (sizeof(m_waveHeader) - offsetPosition), SEEK_SET);
            fwrite(&m_waveHeader.dwSampleLength, sizeof(m_waveHeader.dwSampleLength), 1, m_fp);
            // point move to data_size position
            fseek(m_fp, (sizeof(m_waveHeader) - sizeof(m_waveHeader.data_chunk_size)), SEEK_SET); 
            fwrite(&m_waveHeader.data_chunk_size, sizeof(m_waveHeader.data_chunk_size), 1, m_fp);
            fseek(m_fp, 0, SEEK_END);

            fclose(m_fp);
            LOG_DEBUG_MSG("........................Wirte {} pure data to wav file.....................", m_waveHeader.data_chunk_size);
            m_fp = nullptr;
        }
    }

    void AudioRecordService::recordCallback(std::string& data)
    {
        if (data.empty()/* || m_speechRec.ep_stat >= MSP_EP_AFTER_SPEECH*/)
        {
            return;
        }
        if (m_speechRec.speechState < configuration::SpeechState::SPEECH_STATE_STARTED)
        {
            LOG_ERROR_MSG("Speech state issue.");
            return;
        }

        if (not audioDataConversion(data))
        {
            LOG_ERROR_MSG("Audio data convert failure, will use original data to save.");
        }
        int errcode = writeAudioData(data);
        if (errcode < 0)
        {
            endRecordOnError(errcode);
            return;
        }

        dataMutex.lock();
        std::copy(data.begin(), data.end(), std::back_inserter(m_recordDatas));
        dataMutex.unlock();
        cv.notify_one();
        //sendAudioData(data);
    }

    int AudioRecordService::writeAudioData(const std::string& data)
    {
        if (data.empty())
        {
            LOG_ERROR_MSG("audio data is empty.");
            return 0;
        }
        int dataSize = data.size();
        int index = 0;
        size_t writeData = 0;
        while (dataSize > 0)
        {
            if (dataSize > MAX_WRITE_DATA)
            {
                writeData = fwrite(&data[0 + index * MAX_WRITE_DATA], MAX_WRITE_DATA, 1, m_fp);
            }
            else
            {
                writeData = fwrite(&data[0 + index * MAX_WRITE_DATA], dataSize, 1, m_fp);
            }
            dataSize -= MAX_WRITE_DATA;
            ++index;
        }

        //size_t writeData = fwrite(data.c_str(), data.size(), 1, m_fp);
        m_waveHeader.data_chunk_size += data.size();

        return 0;
    }

    void AudioRecordService::endRecordOnError(const int& errorCode)
    {
        if (m_speechRec.audioSource == configuration::SpeechAudioSource::SPEECH_MIC
            && m_sysRec)
        {
            m_sysRec->stopALSAAudio(m_speechRec.alsaAudioContext, SND_PCM_STREAM_CAPTURE);
        }
        speechEnd(static_cast<configuration::SpeechEndReason>(errorCode));

        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_INIT;
    }

    void AudioRecordService::setRegisterNotify(std::function<void(const std::string&, const bool&)> registerNotify)
    {
        this->registerNotify = registerNotify;
    }

    /* after stop_record, there are still some data callbacks */
    void AudioRecordService::waitForRecStop(configuration::ALSAAudioContext& recorder, unsigned int timeout_ms /*= -1*/)
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

    bool AudioRecordService::openOutputAudioFile(const std::string& fileName)
    {
        m_fp = fopen(fileName.c_str(), "wb");
        if (nullptr == m_fp)
        {
            if (errno != ENOENT)
            {
                return false;
            }
            if (createFilePath(fileName))
            {
                m_fp = fopen(fileName.c_str(), "wb");
            }
        }
        if (m_fp)
        {
            return true;
        }
        return false;
    }

    bool AudioRecordService::createFilePath(const std::string& fileName)
    {
        const char* start;
        mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
        const char* path = fileName.c_str();

        if (path[0] == '/')
            start = strchr(path + 1, '/');
        else
            start = strchr(path, '/');

        while (start) 
        {
            char* buffer = strdup(path);
            buffer[start - path] = 0x00;

            if (mkdir(buffer, mode) == -1 && errno != EEXIST) 
            {
                LOG_ERROR_MSG("creating directory {} fail.", buffer);
                free(buffer);
                return -1;
            }
            free(buffer);
            start = strchr(start + 1, '/');
        }
        return true;
    }

    bool AudioRecordService::audioDataConversion(std::string& data)
    {
        std::string audioFormat = audio::getAudioFormat(m_config);
        std::vector<unsigned char> converData;
        int dataSize = data.size();
        converData.resize(dataSize);
        
        if ("PCM" == audioFormat)
        {
            return true;
        }
        else if ("G711a" == audioFormat)
        {
            dataSize = audio::PCM2G711a(&converData[0], data.c_str(), data.size());
            if (dataSize > 0)
            {
                converData.resize(dataSize);
                data.clear();
                std::copy(converData.begin(), converData.end(), std::back_inserter(data));
                return true;
            }
        }
        else if ("G711u" == audioFormat)
        {
            dataSize = audio::PCM2G711u(&converData[0], data.c_str(), data.size());
            if (dataSize > 0)
            {
                converData.resize(dataSize);
                data.clear();
                std::copy(converData.begin(), converData.end(), std::back_inserter(data));
                return true;
            }
        }

        return false;
    }

    int AudioRecordService::sendAudioData(const std::string&)
    {
        const unsigned long timestampinc = 320;
        const unsigned int sendDataSize = 320;

        while (keep_running)
        {
            std::unique_lock<std::mutex> guard(dataMutex);
            cv.wait(guard, [&recordDatas = this->m_recordDatas]()
            {
                return recordDatas.size() > sendDataSize;  // 8000/25=320
            });

            m_rtpSession->sendPacket(&m_recordDatas[0], sendDataSize, timestampinc);
            auto it = m_recordDatas.begin();
            for (int index = 0; index < sendDataSize; index++)
            {
                it = m_recordDatas.erase(it);
            }
        }
        return 0;
    }

} // namespace usbAudio