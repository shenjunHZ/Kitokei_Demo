#pragma once
#include "usbAudio/IAudioRecordService.hpp"
#include "Configurations/Configurations.hpp"
#include "Configurations/ParseConfigFile.hpp"
#include "logger/Logger.hpp"

namespace usbAudio
{
    class ISysAlsa;
} //namespace usbAudio

namespace usbVideo
{
    class AudioService : public usbAudio::IAudioRecordService
    {
    public:
        AudioService(Logger& logger, const configuration::AppConfiguration& config);
        ~AudioService();

        bool initAudioRecord() override;
        void exitAudioRecord() override;

        int audioStartListening() override;
        int audioStopListening() override;

        void setRegisterNotify(std::function<void(const std::string& result, const bool& isLast)>) override {};

    private:
        void recordCallback(std::string& data);
        int writeAudioData(const std::string& data);
        void destroyRecorder();
        void waitForRecStop(configuration::ALSAAudioContext& recorder, unsigned int timeout_ms = -1);
        void endRecordOnError(const int& errorCode);

    private:
        Logger& m_logger;
        const configuration::AppConfiguration& m_config;
        std::string m_pipeName{ "audioPipe" };
        std::string m_outputDir{ "/tmp/videoCapture/" };
        std::unique_ptr<usbAudio::ISysAlsa> m_sysRec;
        configuration::SpeechRecord m_speechRec;
        FILE* m_fd{nullptr};
    };
} // namespace usbVideo