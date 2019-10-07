#pragma once
#include "Configurations/Configurations.hpp"
#include "Configurations/ParseConfigFile.hpp"
#include "logger/Logger.hpp"
#include "IAudioRecordService.hpp"
#include "LinuxRec.hpp"

extern "C" 
{

}

namespace usbAudio
{
    class AudioRecordService final : public IAudioRecordService
    {
    public:
        AudioRecordService(Logger& logger, const configuration::AppConfiguration& config);
        ~AudioRecordService();
        struct TimeStamp
        {
            virtual ~TimeStamp() = default;

            // Returns current timestamp string in extended iso format
            virtual std::string now() const;
        };

        bool initAudioRecord() override;
        void exitAudioRecord() override;

        int audioStartListening() override;
        int audioStopListening() override;

        void setRegisterNotify(std::function<void(const std::string&, const bool&)> registerNotify);

    private:
        bool startAnalysisMic();
        void UninitAudioRecord();
        bool openOutputAudioFile(const std::string& fileName);
        bool createFilePath(const std::string& fileName);

        void speechBegin();
        void speechEnd(const configuration::SpeechEndReason& reason);

        void recordCallback(const std::string& data);
        int writeAudioData(const std::string& data);

        void endRecordOnError(const int& errorCode);

        void destroyRecorder();
        void waitForRecStop(configuration::AudioRecorder& recorder, unsigned int timeout_ms = -1);

    private:
        Logger& m_logger;
        const configuration::AppConfiguration& m_config;
        std::unique_ptr<ISysRec> m_sysRec{};
        configuration::SpeechRecord m_speechRec;
        std::string m_result{};

        std::function<void(const std::string&, const bool&)> registerNotify{};
        std::unique_ptr<TimeStamp> m_timeStamp;
        FILE* m_fp;
        configuration::wavePCMHeader m_waveHeader;
    };
} // namespace usbAudio