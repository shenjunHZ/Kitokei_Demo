#pragma once
#include <mutex>
#include <condition_variable>
#include "Configurations/Configurations.hpp"
#include "Configurations/ParseConfigFile.hpp"
#include "logger/Logger.hpp"
#include "IAudioRecordService.hpp"
#include "LinuxAlsa.hpp"

namespace endpoints
{
    class IRTPSession;
} // namespace endpoints

namespace usbAudio
{
    class AudioRecordService final : public IAudioRecordService
    {
    public:
        AudioRecordService(Logger& logger, const configuration::AppConfiguration& config, std::shared_ptr<endpoints::IRTPSession> rtpSession);
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

        void recordCallback(std::string& data);
        int writeAudioData(const std::string& data);
        int sendAudioData(const std::string& data);

        void endRecordOnError(const int& errorCode);

        void destroyRecorder();
        void waitForRecStop(configuration::ALSAAudioContext& recorder, unsigned int timeout_ms = -1);
        bool audioDataConversion(std::string& data);

    private:
        Logger& m_logger;
        const configuration::AppConfiguration& m_config;
        std::unique_ptr<ISysAlsa> m_sysRec{};
        configuration::SpeechRecord m_speechRec;
        std::string m_result{};

        std::function<void(const std::string&, const bool&)> registerNotify{};
        std::unique_ptr<TimeStamp> m_timeStamp;
        FILE* m_fp;
        configuration::wavePCMHeader m_waveHeader;
        std::shared_ptr<endpoints::IRTPSession> m_rtpSession;
        std::mutex dataMutex;
        std::vector<std::uint8_t> m_recordDatas;
        std::condition_variable cv;
        std::thread m_rtpSendThread;
    };
} // namespace usbAudio