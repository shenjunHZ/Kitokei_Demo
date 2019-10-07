#include <fstream>
#include "usbAudio/AudioRecordService.hpp"
#include "common/CommonFunction.hpp"

namespace
{
    const int waitForTimeout = 2;
    const int MAX_WRITE_DATA = 2048;
    const unsigned short SAMPLE_BIT_SIZE = 16;
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

    AudioRecordService::AudioRecordService(Logger& logger, const configuration::AppConfiguration& config)
        : m_logger{ logger }
        , m_config { config }
        , m_timeStamp{ std::make_unique<TimeStamp>() }
        , m_fp{nullptr}
    {

    }

    AudioRecordService::~AudioRecordService()
    {
 
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
        int sampleRate = 8000; // rate must more than 8000
        if (audio::getAudioSampleRate(m_config) > sampleRate)
        {
            sampleRate = audio::getAudioSampleRate(m_config);
        }
        configuration::WAVEFORMATEX wavfmt = { 
            WAVE_FORMAT_PCM, audioChannel, sampleRate, 
            sampleRate * audioChannel * SAMPLE_BIT_SIZE / 8,
            audioChannel* SAMPLE_BIT_SIZE / 8,
            SAMPLE_BIT_SIZE,
            sizeof(configuration::WAVEFORMATEX) };
        m_sysRec = std::make_unique<LinuxRec>(std::make_unique<configuration::WAVEFORMATEX>(wavfmt));
        // wav fmt chuck head
        m_waveHeader.bits_per_sample   = wavfmt.wBitsPerSample;
        m_waveHeader.block_align       = wavfmt.nBlockAlign;
        m_waveHeader.avg_bytes_per_sec = wavfmt.nAvgBytesPerSec;
        m_waveHeader.samples_per_sec   = wavfmt.nSamplesPerSec;
        m_waveHeader.channels          = wavfmt.nChannels;
        m_waveHeader.format_tag        = wavfmt.wFormatTag;

        if (0 == m_sysRec->getInputDevNum())
        {
            LOG_ERROR_MSG("Have no PCM device can be used.");
            return false;
        }

        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_INIT;
        m_speechRec.audioSource = configuration::SpeechAudioSource::SPEECH_MIC;
        m_speechRec.notif = std::move(recNotifier);

        if (m_sysRec)
        {
            errcode = m_sysRec->createRecorder(m_speechRec.audioRecorder, std::bind(&AudioRecordService::recordCallback, this, std::placeholders::_1),
                static_cast<void*>(&m_speechRec));

            if (0 > errcode)
            {
                LOG_DEBUG_MSG("create recorder failed: {}", errcode);
                errcode = static_cast<int>(configuration::RecordErrorCode::RECORD_ERR_RECORDFAIL);
                destroyRecorder();
                return errcode;
            }

            configuration::recordDevInfo devInfo = m_sysRec->getDefaultInputDev(); // to do config
            devInfo = m_sysRec->setInputDev(common::getAudioDevice(m_config));
            errcode = m_sysRec->openRecorder(m_speechRec.audioRecorder, devInfo);
            if (0 != errcode)
            {
                LOG_DEBUG_MSG("recorder open failed: {}", errcode);
                errcode = static_cast<int>(configuration::RecordErrorCode::RECORD_ERR_RECORDFAIL);
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
            if (!m_sysRec->isRecordStopped(m_speechRec.audioRecorder))
            {
                m_sysRec->stopRecord(m_speechRec.audioRecorder);
            }
            m_sysRec->closeRecorder(m_speechRec.audioRecorder);
            m_sysRec->destroyRecorder(m_speechRec.audioRecorder);
        }
    }

    void AudioRecordService::destroyRecorder()
    {
        if (m_sysRec)
        {
            m_sysRec->destroyRecorder(m_speechRec.audioRecorder);
        }
    }

    int AudioRecordService::audioStartListening()
    {
        int ret = -1;
        if (m_speechRec.speechState >= configuration::SpeechState::SPEECH_STATE_STARTED)
        {
            LOG_DEBUG_MSG("already STARTED.");
            return static_cast<int>(configuration::RecordErrorCode::RECORD_ERR_ALREADY);
        }
        if (nullptr != m_speechRec.notif.onSpeechBegin)
        {
            m_speechRec.notif.onSpeechBegin();
        }

        if (configuration::SpeechAudioSource::SPEECH_MIC == m_speechRec.audioSource && nullptr != m_sysRec)
        {
            ret = m_sysRec->startRecord(m_speechRec.audioRecorder);
            if (ret != 0)
            {
                LOG_DEBUG_MSG("start record failed: {}", ret);
                return static_cast<int>(configuration::RecordErrorCode::RECORD_ERR_RECORDFAIL);
            }
        }

        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_STARTED;
        return 0;
    }

    int AudioRecordService::audioStopListening()
    {
        int ret = 0;
        const char* rslt = nullptr;

        if (m_speechRec.speechState < configuration::SpeechState::SPEECH_STATE_STARTED)
        {
            LOG_DEBUG_MSG("Not started or already stopped.");
            return 0;
        }

        if (m_speechRec.audioSource == configuration::SpeechAudioSource::SPEECH_MIC && nullptr != m_sysRec)
        {
            ret = m_sysRec->stopRecord(m_speechRec.audioRecorder);

            if (ret != 0)
            {
                LOG_ERROR_MSG("Stop failed!");
                return static_cast<int>(configuration::RecordErrorCode::RECORD_ERR_RECORDFAIL);
            }
            waitForRecStop(m_speechRec.audioRecorder, waitForTimeout);
        }
        if (nullptr != m_speechRec.notif.onSpeechEnd)
        {
            m_speechRec.notif.onSpeechEnd(configuration::SpeechEndReason::END_REASON_VAD_DETECT);
        }
        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_INIT;
        return 0;
    }

    void AudioRecordService::speechBegin()
    {
        m_result = "";
        if (common::enableAudioWriteToFile(m_config))
        {
            std::string outputFile = common::getCaptureOutputDir(m_config) + common::getAudioName(m_config) + "_" + m_timeStamp->now() + ".wav";
            if (openOutputAudioFile(outputFile))
            {
                m_waveHeader.clear();
                fwrite(&m_waveHeader, sizeof(m_waveHeader), 1, m_fp); //添加wav音频头，使用采样率为16000
            }
        }

        LOG_INFO_MSG(m_logger, "Start Record Listening...");
    }

    void AudioRecordService::speechEnd(const configuration::SpeechEndReason& reason)
    {
        if (configuration::SpeechEndReason::END_REASON_VAD_DETECT == reason)
        {
            LOG_INFO_MSG(m_logger, "Record Speaking done.");
        }
        else
        {
            LOG_INFO_MSG(m_logger, "Recognizer error: {}", static_cast<int>(reason));
        }
        if (m_fp)
        {
            /* fixed size of wav file header data */
            m_waveHeader.size_8 += m_waveHeader.data_size + (sizeof(m_waveHeader) - 8);
            /* write the corrected data back to the file header.
            The audio file is in wav format.*/
            fseek(m_fp, 4, 0);
            fwrite(&m_waveHeader.size_8, sizeof(m_waveHeader.size_8), 1, m_fp); //write size_8 data
            fseek(m_fp, 40, 0); //将文件指针偏移到存储data_size值的位置
            fwrite(&m_waveHeader.data_size, sizeof(m_waveHeader.data_size), 1, m_fp); //write data_size data
            fseek(m_fp, 0, SEEK_END);

            fclose(m_fp);
            m_fp = nullptr;
        }
    }

    void AudioRecordService::recordCallback(const std::string& data)
    {
        int errcode = -1;
        if (data.empty()/* || m_speechRec.ep_stat >= MSP_EP_AFTER_SPEECH*/)
        {
            return;
        }

        errcode = writeAudioData(data);
        if (errcode < 0)
        {
            endRecordOnError(errcode);
            return;
        }
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
        m_waveHeader.data_size += data.size();

        return 0;
    }

    void AudioRecordService::endRecordOnError(const int& errorCode)
    {
        if (m_speechRec.audioSource == configuration::SpeechAudioSource::SPEECH_MIC
            && m_sysRec)
        {
            m_sysRec->stopRecord(m_speechRec.audioRecorder);
        }
        if (m_speechRec.notif.onSpeechEnd)
        {
            m_speechRec.notif.onSpeechEnd(static_cast<configuration::SpeechEndReason>(errorCode));
        }

        m_speechRec.speechState = configuration::SpeechState::SPEECH_STATE_INIT;
    }

    void AudioRecordService::setRegisterNotify(std::function<void(const std::string&, const bool&)> registerNotify)
    {
        this->registerNotify = registerNotify;
    }

    /* after stop_record, there are still some data callbacks */
    void AudioRecordService::waitForRecStop(configuration::AudioRecorder& recorder, unsigned int timeout_ms /*= -1*/)
    {
        if (nullptr == m_sysRec)
        {
            return;
        }
        while (!m_sysRec->isRecordStopped(recorder))
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

} // namespace usbAudio