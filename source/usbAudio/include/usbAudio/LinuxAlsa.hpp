#pragma once
#include "ISysAlsa.hpp"
#include "Configurations/Configurations.hpp"

namespace usbAudio
{
    class LinuxAlsa final : public ISysAlsa
    {
    public:
        LinuxAlsa(std::unique_ptr<configuration::WAVEFORMATEX> waveFormat);

        configuration::audioDevInfo getDefaultDev() override;

        configuration::audioDevInfo setAudioDev(const std::string& dev) override;

        unsigned int getAudioDevNum(const snd_pcm_stream_t& stream) override;

        int createALSAAudio(configuration::ALSAAudioContext& alsaAudioContext,
            std::function<void(const std::string& data)> on_data_ind, void* user_cb_para) override;
        void destroyALSAAudio(configuration::ALSAAudioContext& alsaAudioContext) override;

        int openALSAAudio(configuration::ALSAAudioContext& alsaAudioContext,
            const configuration::audioDevInfo& devInfo, const snd_pcm_stream_t& stream) override;
        void closeALSAAudio(configuration::ALSAAudioContext& alsaAudioContext,
            const snd_pcm_stream_t& stream)  override;

        int startALSAAudio(configuration::ALSAAudioContext& alsaAudioContext, 
            const snd_pcm_stream_t& stream) override;
        int stopALSAAudio(configuration::ALSAAudioContext& alsaAudioContext, const snd_pcm_stream_t& stream) override;

        int isALSAAudioStopped(configuration::ALSAAudioContext& alsaAudioContext) override;

        int readAudioDataToPCM(configuration::ALSAAudioContext& alsaAudioContext) override;

    private:
        int getPCMDeviceCnt(const snd_pcm_stream_t& stream);

        int openALSAAudioInternal(configuration::ALSAAudioContext& alsaAudioContext,
            const configuration::audioDevInfo& devInfo, const snd_pcm_stream_t& stream);
        void closeALSAAudioInternal(configuration::ALSAAudioContext& alsaAudioContext);

        int startALSAAudioInternal(snd_pcm_t* pcmHandle);
        int stopALSAAudioInternal(snd_pcm_t* pcmHandle);

        void cleanUpALSAAudioResource(configuration::ALSAAudioContext& alsaAudioContext);

        void closePCMDevice(snd_pcm_t* pcm);

        void freeAudioBuffer(configuration::ALSAAudioContext& alsaAudioContext);

        bool isStoppedInternal(configuration::ALSAAudioContext& alsaAudioContext);

        bool setALSAAudioParams(configuration::ALSAAudioContext& alsaAudioContext);
        bool setHWParams(configuration::ALSAAudioContext& alsaAudioContext);
        bool setSWParams(configuration::ALSAAudioContext& alsaAudioContext);

        void createCallbackThread(configuration::ALSAAudioContext& alsaAudioContext);
        int pcmRead(configuration::ALSAAudioContext & alsaAudioContext, const size_t& frameSize);
        int pcmWrite(configuration::ALSAAudioContext& alsaAudioContext, const size_t& frameSize);
        int xRunRecovery(configuration::ALSAAudioContext& alsaAudioContext, const snd_pcm_sframes_t& data);

    private:
        std::shared_ptr<configuration::WAVEFORMATEX> m_waveFormat{};
    };
} // namespace usbAudio