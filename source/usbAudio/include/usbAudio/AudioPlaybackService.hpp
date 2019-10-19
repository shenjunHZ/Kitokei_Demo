#pragma once
#include "Configurations/Configurations.hpp"
#include "Configurations/ParseConfigFile.hpp"
#include "logger/Logger.hpp"
#include "IAudioPlaybackService.hpp"
#include "LinuxAlsa.hpp"

namespace usbAudio
{
    class AudioPlaybackService final : public IAudioPlaybackService
    {
    public:
        AudioPlaybackService(Logger& logger, const configuration::AppConfiguration& config);
        ~AudioPlaybackService();

        bool initAudioPlayback() override;
        void exitAudioPlayback() override;

        int audioStartPlaying() override;
        int audioStopPlaying() override;

    private:
        bool startAnalysisSpeech();
        void uninitAudioPlayback();

        void speechBegin();
        void speechEnd(const configuration::SpeechEndReason& reason);

        void speechStartFromFile();
        int readAudioData();

        void endPlaybackWithReason(const int& errorCode);

        void destroyPlayback();
        void waitForPlaybackStop(configuration::ALSAAudioContext& playback, unsigned int timeout_ms = -1);

        void printWavHdr(size_t& size);

    private:
        Logger& m_logger;
        const configuration::AppConfiguration& m_config;
        std::unique_ptr<ISysAlsa> m_sysPlayback{};
        configuration::SpeechRecord m_speechRec;

        //std::function<void(const std::string&, const bool&)> registerNotify{};
        FILE* m_fp;
        configuration::wavePCMHeader m_waveHeader;
    };
} // namespace usbAudio