#pragma once
#include "ISysRec.hpp"
#include "Configurations/Configurations.hpp"

extern "C" 
{
#include <alsa/asoundlib.h>
}

namespace usbAudio
{
    class LinuxRec final : public ISysRec
    {
    public:
        LinuxRec(std::unique_ptr<configuration::WAVEFORMATEX> waveFormat);

        configuration::recordDevInfo getDefaultInputDev() override;
        configuration::recordDevInfo setInputDev(const std::string& dev) override;
        unsigned int getInputDevNum() override;

        int createRecorder(configuration::AudioRecorder& recorder,
            std::function<void(const std::string& data)> on_data_ind, void* user_cb_para) override;
        void destroyRecorder(configuration::AudioRecorder& recorder) override;

        int openRecorder(configuration::AudioRecorder& recorder,
            const configuration::recordDevInfo& recordDev) override;
        void closeRecorder(configuration::AudioRecorder& recorder)  override;

        int startRecord(configuration::AudioRecorder& recorder) override;
        int stopRecord(configuration::AudioRecorder& recorder) override;
        int isRecordStopped(configuration::AudioRecorder& recorder) override;

    private:
        int getPCMDeviceCnt(const snd_pcm_stream_t& stream);
        int openRecorderInternal(configuration::AudioRecorder& recorder,
            const configuration::recordDevInfo& recordDev);
        int startRecordInternal(snd_pcm_t* pcm);

        void cleanUpRecorderResource(configuration::AudioRecorder& recorder);
        void closeRecDevice(snd_pcm_t* pcm);

        int stopRecordInternal(snd_pcm_t* pcm);
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