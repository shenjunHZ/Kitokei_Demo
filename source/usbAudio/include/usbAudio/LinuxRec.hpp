#pragma once
#include "ISysRec.hpp"
#include "Configurations/Configurations.hpp"

namespace usbAudio
{
    class LinuxRec final : public ISysRec
    {
    public:
        LinuxRec(std::unique_ptr<configuration::WAVEFORMATEX> waveFormat);

        configuration::audioDevInfo getDefaultInputDev() override;
        configuration::audioDevInfo setInputDev(const std::string& dev) override;
        unsigned int getInputDevNum(const snd_pcm_stream_t& stream) override;

        int createRecorder(configuration::AudioRecorder& recorder,
            std::function<void(const std::string& data)> on_data_ind, void* user_cb_para) override;
        void destroyRecorder(configuration::AudioRecorder& recorder) override;

        int openRecorder(configuration::AudioRecorder& recorder,
            const configuration::audioDevInfo& recordDev) override;
        void closeRecorder(configuration::AudioRecorder& recorder)  override;

        int openPlayback(configuration::AudioPlayback& playback,
            const configuration::audioDevInfo& devInfo) override;

        int startRecord(configuration::AudioRecorder& recorder) override;
        int stopRecord(configuration::AudioRecorder& recorder) override;
        int isRecordStopped(configuration::AudioRecorder& recorder) override;

    private:
        int getPCMDeviceCnt(const snd_pcm_stream_t& stream);
        int openRecorderInternal(configuration::AudioRecorder& recorder,
            const configuration::audioDevInfo& recordDev, const snd_pcm_stream_t& stream);
        int startRecordInternal(snd_pcm_t* pcmHandle);

        void cleanUpRecorderResource(configuration::AudioRecorder& recorder);
        void closeRecDevice(snd_pcm_t* pcm);

        int stopRecordInternal(snd_pcm_t* pcmHandle);
        void closeRecorderInternal(configuration::AudioRecorder& recorder);
        bool isStoppedInternal(configuration::AudioRecorder& recorder);
        void freeRecBuffer(configuration::AudioRecorder& recorder);

        bool setRecorderParams(configuration::AudioRecorder& recorder);
        bool setHWParams(configuration::AudioRecorder& recorder);
        bool setSWParams(configuration::AudioRecorder& recorder);
        void createCallbackThread(configuration::AudioRecorder& recorder);
        int pcmRead(configuration::AudioRecorder& recorder, const size_t& frameSize);
        int xRunRecovery(configuration::AudioRecorder& recorder, const snd_pcm_sframes_t& data);

    private:
        std::shared_ptr<configuration::WAVEFORMATEX> m_waveFormat{};
    };
} // namespace usbAudio