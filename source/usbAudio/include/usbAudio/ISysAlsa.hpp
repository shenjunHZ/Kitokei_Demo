#pragma once
#include "Configurations/Configurations.hpp"

extern "C"
{
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>
}

namespace usbAudio
{
    class ISysAlsa
    {
    public:
        virtual ~ISysAlsa() = default;

        /**
         * @fn
         * @brief	Get the default input/output device ID
         *
         * @return	return WAVE_MAPPER in windows.
         *          return "default" in linux.
        */
        virtual configuration::audioDevInfo getDefaultDev() = 0;
        virtual configuration::audioDevInfo setAudioDev(const std::string& dev) = 0;

        /**
         * @fn
         * @brief	Get the total number of active input/output devices.
         * @return	the number. 0 means no active device.
         */
        virtual unsigned int getAudioDevNum(const snd_pcm_stream_t& stream) = 0;

        /**
         * @fn
         * @brief	Create a alsa object.
         * @return	int	 - Return 0 in success, otherwise return error code.
         */
        virtual int createALSAAudio(configuration::ALSAAudioContext& alsaAudioContext,
            std::function<void(const std::string& data)> on_data_ind, void* user_cb_para) = 0;

        /**
         * @fn
         * @brief	Destroy alsa object. free memory.
         */
        virtual void destroyALSAAudio(configuration::ALSAAudioContext& alsaAudioContext) = 0;

        /**
         * @fn
         * @brief	open the device.
         * @return	int			- Return 0 in success, otherwise return error code.
         */
        virtual int openALSAAudio(configuration::ALSAAudioContext& alsaAudioContext,
            const configuration::audioDevInfo& devInfo, const snd_pcm_stream_t& stream) = 0;

        /**
         * @fn
         * @brief	close the device.
         */
        virtual void closeALSAAudio(configuration::ALSAAudioContext& alsaAudioContext,
            const snd_pcm_stream_t& stream) = 0;

        /**
         * @fn
         * @brief	start alsa.
         * @return	int			- Return 0 in success, otherwise return error code.
         */
        virtual int startALSAAudio(configuration::ALSAAudioContext& alsaAudioContext, 
            const snd_pcm_stream_t& stream) = 0;

        /**
         * @fn
         * @brief	stop alsa.
         * @return	int			- Return 0 in success, otherwise return error code.
         */
        virtual int stopALSAAudio(configuration::ALSAAudioContext& alsaAudioContext,
            const snd_pcm_stream_t& stream) = 0;

        /**
         * @fn
         * @brief	test if the alsa starting has been stopped.
         * @return	int			- 1: stopped. 0 : starting.
         */
        virtual int isALSAAudioStopped(configuration::ALSAAudioContext& alsaAudioContext) = 0;

        /**
        * @fn
        * @brief	read audio data from file or network to pcm.
        * @return   int			- Return 0 in success, otherwise return error code.
        */
        virtual int readAudioDataToPCM(configuration::ALSAAudioContext& alsaAudioContext) = 0;
    };
} // namespace usbAudio