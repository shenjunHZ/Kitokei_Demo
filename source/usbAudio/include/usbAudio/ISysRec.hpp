#pragma once
#include "Configurations/Configurations.hpp"

namespace usbAudio
{
    class ISysRec
    {
    public:
        virtual ~ISysRec() = default;

        /**
         * @fn
         * @brief	Get the default input device ID
         *
         * @return	return WAVE_MAPPER in windows.
         *          return "default" in linux.
        */
        virtual configuration::recordDevInfo getDefaultInputDev() = 0;
        virtual configuration::recordDevInfo setInputDev(const std::string& dev) = 0;

        /**
         * @fn
         * @brief	Get the total number of active input devices.
         * @return	the number. 0 means no active device.
         */
        virtual unsigned int getInputDevNum() = 0;

        /**
         * @fn
         * @brief	Create a recorder object.
         * @return	int			- Return 0 in success, otherwise return error code.
         * @param	out_rec		- [out] recorder object holder
         * @param	on_data_ind	- [in]	callback. called when data coming.
         * @param	user_cb_para	- [in] user params for the callback.
         * @see
         */
        virtual int createRecorder(configuration::AudioRecorder& recorder,
            std::function<void(const std::string& data)> on_data_ind, void* user_cb_para) = 0;

        /**
         * @fn
         * @brief	Destroy recorder object. free memory.
         * @param	rec	- [in]recorder object
         */
        virtual void destroyRecorder(configuration::AudioRecorder& recorder) = 0;

        /**
         * @fn
         * @brief	open the device.
         * @return	int			- Return 0 in success, otherwise return error code.
         * @param	rec			- [in] recorder object
         * @param	dev			- [in] device id, from 0.
         * @param	fmt			- [in] record format.
         * @see
         * 	get_default_input_dev()
         */
        virtual int openRecorder(configuration::AudioRecorder& recorder,
            const configuration::recordDevInfo& devInfo) = 0;

        /**
         * @fn
         * @brief	close the device.
         * @param	rec			- [in] recorder object
         */

        virtual void closeRecorder(configuration::AudioRecorder& recorder) = 0;

        /**
         * @fn
         * @brief	start record.
         * @return	int			- Return 0 in success, otherwise return error code.
         * @param	rec			- [in] recorder object
         */
        virtual int startRecord(configuration::AudioRecorder& recorder) = 0;

        /**
         * @fn
         * @brief	stop record.
         * @return	int			- Return 0 in success, otherwise return error code.
         * @param	rec			- [in] recorder object
         */
        virtual int stopRecord(configuration::AudioRecorder& recorder) = 0;

        /**
         * @fn
         * @brief	test if the recording has been stopped.
         * @return	int			- 1: stopped. 0 : recording.
         * @param	rec			- [in] recorder object
         */
        virtual int isRecordStopped(configuration::AudioRecorder& recorder) = 0;
    };
} // namespace usbAudio