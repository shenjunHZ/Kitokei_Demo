#include "usbAudio/LinuxAlsa.hpp"
extern "C"
{
#include <alsa/error.h>
}

namespace
{
    const int BUF_COUNT = 4;
    const int FRAME_CNT = 10;
    const int SAMPLE_RATE = 16000;

    constexpr int BufferTime = 500 * 1000;
    constexpr int PeriodTime = 100 * 1000;
    constexpr int PCMTimeWait = 100;
    constexpr int BitsByte = 8;
    std::atomic_bool keep_running{true};
} // namespace

using namespace configuration;
namespace usbAudio
{
    LinuxAlsa::LinuxAlsa(Logger& logger, std::unique_ptr<configuration::WAVEFORMATEX> waveFormat)
        : m_waveFormat{ std::move(waveFormat) }
        , m_logger{logger}
    {

    }

 /***********************API for create recorder and playback****************************************/
    // create recorder use capture
    int LinuxAlsa::createALSAAudio(configuration::ALSAAudioContext& alsaAudioContext,
        std::function<void(std::string& data)> onDataInd, void* userCallbackPara)
    {
        alsaAudioContext.onDataInd = onDataInd;
        alsaAudioContext.userCallbackPara = userCallbackPara;
        alsaAudioContext.alsaState = ALSAState::ALSA_STATE_CREATED;

        return 0;
    }

    int LinuxAlsa::openALSAAudio(configuration::ALSAAudioContext& alsaAudioContext,
        const configuration::audioDevInfo& devInfo, const snd_pcm_stream_t& stream)
    {
        if (alsaAudioContext.alsaState >= ALSAState::ALSA_STATE_READY)
        {
            return 0;
        }

        int ret = openALSAAudioInternal(alsaAudioContext, devInfo, stream);
        if (ret == 0)
        {
            alsaAudioContext.alsaState = ALSAState::ALSA_STATE_READY;
        }
        return ret;
    }

    int LinuxAlsa::openALSAAudioInternal(configuration::ALSAAudioContext& alsaAudioContext,
        const configuration::audioDevInfo& devInfo, const snd_pcm_stream_t& stream)
    {
        /*open PCM device, in ALSA each PCM device have the match name to be used.
        * for example: PCM name define " char *pcm_name = "plughw:0,0" "
        * the important PCM device interface“plughw”and“hw”. 
        * use "plughw" interface，do not care hardware, and if set the config params not match with the real hardware params,
        * ALSA will auto change the config. if use "hw" interface，we shall to check hardware params whether it support or not.
        * plughw device id and sub-device id.
        * stream: SND_PCM_STREAM_CAPTURE / SND_PCM_STREAM_PLAYBACK
        * mode: SND_PCM_NONBLOCK
        */
        int ret = snd_pcm_open(reinterpret_cast<snd_pcm_t**>(&alsaAudioContext.pcmHandle), devInfo.devName.c_str(),
            stream, 0);
        if (ret < 0)
        {
            const char* err = snd_strerror(ret);
            LOG_ERROR_MSG("Open pcm device: {}, failed {}", devInfo.devName, err);
            cleanUpALSAAudioResource(alsaAudioContext);
            return static_cast<int>(ALSAErrorCode::ALSA_ERR_INVAL);
        }

        if (not setALSAAudioParams(alsaAudioContext))
        {
            cleanUpALSAAudioResource(alsaAudioContext);
            return static_cast<int>(ALSAErrorCode::ALSA_ERR_INVAL);
        }

        size_t size = (alsaAudioContext.periodFrames * alsaAudioContext.bitsPerFrame / BitsByte);
        alsaAudioContext.audioBuffer.resize(size);
        if (alsaAudioContext.audioBuffer.empty())
        {
            cleanUpALSAAudioResource(alsaAudioContext);
            LOG_ERROR_MSG("Create audio buffer failed.");
            return static_cast<int>(ALSAErrorCode::ALSA_ERR_MEMFAIL);
        }

        return ret;
    }

    int LinuxAlsa::startALSAAudio(configuration::ALSAAudioContext& alsaAudioContext, const snd_pcm_stream_t& stream)
    {
        if (alsaAudioContext.alsaState < configuration::ALSAState::ALSA_STATE_READY)
        {
            return static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_NOT_READY);
        }
        if (alsaAudioContext.alsaState == configuration::ALSAState::ALSA_STATE_STARTING)
        {
            return static_cast<int>(configuration::ALSAErrorCode::ALSA_ERR_ALREADY);
        }
//         size_t audioTotalSize = audioSecondsDuration * m_waveFormat->nSamplesPerSec *
//             m_waveFormat->wBitsPerSample * m_waveFormat->nChannels / 8;

        int ret = 0;
        if (SND_PCM_STREAM_CAPTURE == stream)
        {
            ret = startALSAAudioInternal(static_cast<snd_pcm_t*>(alsaAudioContext.pcmHandle));
            if (ret == 0)
            {
                alsaAudioContext.alsaState = configuration::ALSAState::ALSA_STATE_STARTING;
                keep_running = true;

                alsaAudioContext.audioThread = std::thread([this, &alsaAudioContext]()
                {
                    this->createCallbackThread(alsaAudioContext);
                });
            }
            if (ALSAErrorCode::ALSA_ERR_File_Descriptor_Bad_State == static_cast<ALSAErrorCode>(ret))
            {
                snd_pcm_state_t state = snd_pcm_state(static_cast<snd_pcm_t*>(alsaAudioContext.pcmHandle));
                LOG_WARNING_MSG("PCM state is {}", coverToDescription(state));
            }
        }
        else if (SND_PCM_STREAM_PLAYBACK == stream)
        {
            alsaAudioContext.alsaState = configuration::ALSAState::ALSA_STATE_STARTING;
        }

        return ret;
    }

    int LinuxAlsa::startALSAAudioInternal(snd_pcm_t* pcmHandle)
    {
        if (nullptr == pcmHandle)
        {
            return -1;
        }
        /* if the time between this read/write with the last maybe too long,
        * then the device has already been xrun by the time read/write.
        * knowing the ALSA driver's default policy for xrun, it is best to call
        * snd_pcm_prepare to re-prepare the device and then turn it on read/write.
        */
        int errCode = snd_pcm_prepare(pcmHandle);
        if (0 == errCode)
        {
            errCode = snd_pcm_start(pcmHandle);
        }
        if (0 > errCode)
        {
            const char* err = snd_strerror(errCode);
            LOG_ERROR_MSG("PCM start failed {}", err);
        }
        return errCode;
    }

    void LinuxAlsa::createCallbackThread(configuration::ALSAAudioContext& alsaAudioContext)
    {
        while (keep_running)
        {
            size_t frames = alsaAudioContext.periodFrames;
            size_t bytes = frames * alsaAudioContext.bitsPerFrame / BitsByte;

            /* closing, exit the thread */
            if (ALSAState::ALSA_STATE_STOPPING == alsaAudioContext.alsaState
                || ALSAState::ALSA_STATE_CLOSING == alsaAudioContext.alsaState)
            {
                return;
            }

            if (ALSAState::ALSA_STATE_STARTING > alsaAudioContext.alsaState)
            {
                usleep(100 * 1000);
            }

			const size_t readFrames = pcmRead(alsaAudioContext, frames);
            if (readFrames != frames)
            {
				LOG_WARNING_MSG("PCM read audio frames {} not match {}.", readFrames, frames);
                return;
            }

            if (alsaAudioContext.onDataInd)
            {
                std::string recordData(alsaAudioContext.audioBuffer.begin(), alsaAudioContext.audioBuffer.end());
                alsaAudioContext.onDataInd(recordData);
            }
        }
        return;
    }

    int LinuxAlsa::pcmRead(configuration::ALSAAudioContext& alsaAudioContext, const size_t& frameSize)
    {
        snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsaAudioContext.pcmHandle);
        char* audioData = reinterpret_cast<char*>(&alsaAudioContext.audioBuffer[0]);
        snd_pcm_uframes_t count = frameSize;

        while (count > 0)
        {
            /* The third parameter represents the size of the read and write data(frames), 
            * in frames instead of bytes, with a frame size of 4 bytes in a dual-channel 16-bit format 
            * i means interleaved data*/
            snd_pcm_sframes_t rData = snd_pcm_readi(handle, audioData, count);
            if (rData == -EAGAIN || (rData >= 0 && static_cast<size_t>(rData) < count))
            {
                snd_pcm_wait(handle, PCMTimeWait);
            }
            else if (rData < 0)
            {
                if (xRunRecovery(alsaAudioContext, rData) < 0)
                {
                    return -1;
                }
            }

            if (rData > 0)
            {
                count -= rData;
                audioData += rData * alsaAudioContext.bitsPerFrame / BitsByte;
            }
        }
        return frameSize;
    }

    int LinuxAlsa::xRunRecovery(configuration::ALSAAudioContext& alsaAudioContext, const snd_pcm_sframes_t& data)
    {
        snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsaAudioContext.pcmHandle);
        int ret = -1;
        if (data == -EPIPE)
        {	/* over-run */
            LOG_WARNING_MSG("overrun happend!");

            ret = snd_pcm_prepare(handle);
            if (ret < 0)
            {
                LOG_ERROR_MSG("Can't recovery from overrun,"
                    "prepare failed: {}.", snd_strerror(ret));
                return ret;
            }
            return 0;
        }
        else if (data == -ESTRPIPE)
        {
            while ((ret = snd_pcm_resume(handle)) == -EAGAIN)
            {
                LOG_WARNING_MSG("pcm resume wait until the suspend flag is released");
                usleep(200 * 1000);	/* wait until the suspend flag is released */
            }
            if (ret < 0)
            {
                ret = snd_pcm_prepare(handle);
                if (ret < 0)
                {
                    LOG_ERROR_MSG("Can't recovery from suspend,"
                        "prepare failed: %s\n", snd_strerror(ret));
                    return ret;
                }
            }
            return 0;
        }
        return ret;
    }

    int LinuxAlsa::readAudioDataToPCM(configuration::ALSAAudioContext& alsaAudioContext)
    {
            size_t frames = alsaAudioContext.periodFrames;
            size_t bytes = frames * alsaAudioContext.bitsPerFrame / BitsByte;

            /* closing, exit the thread */
            if (ALSAState::ALSA_STATE_STOPPING == alsaAudioContext.alsaState
                || ALSAState::ALSA_STATE_CLOSING == alsaAudioContext.alsaState)
            {
                return 0;
            }

            if (ALSAState::ALSA_STATE_STARTING > alsaAudioContext.alsaState)
            {
                usleep(100 * 1000);
            }

            if (pcmWrite(alsaAudioContext, frames) != frames)
            {
                return static_cast<int>(ALSAErrorCode::ALSA_ERR_WRITEDATA);
            }

        return 0;
    }

    int LinuxAlsa::pcmWrite(configuration::ALSAAudioContext& alsaAudioContext, const size_t& frameSize)
    {
        snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsaAudioContext.pcmHandle);
        char* audioData = reinterpret_cast<char*>(alsaAudioContext.audioBuffer[0]);
        snd_pcm_uframes_t count = frameSize;

        while (count > 0)
        {
            /* The third parameter represents the size of the read and write data(frames),
            * in frames instead of bytes, with a frame size of 4 bytes in a dual-channel 16-bit format
            * i means interleaved data*/
            snd_pcm_sframes_t rData = snd_pcm_writei(handle, audioData, count);
            if (rData == -EAGAIN || (rData >= 0 && static_cast<size_t>(rData) < count))
            {
                snd_pcm_wait(handle, PCMTimeWait);
            }
            else if (rData < 0)
            {
                if (xRunRecovery(alsaAudioContext, rData) < 0)
                {
                    return -1;
                }
            }

            if (rData > 0)
            {
                count -= rData;
                audioData += rData * alsaAudioContext.bitsPerFrame / BitsByte;
            }
        }
        return frameSize;
    }

 /************************API for clear resource*************************************/
    void LinuxAlsa::destroyALSAAudio(configuration::ALSAAudioContext& alsaAudioContext)
    {
        LOG_DEBUG_MSG("ALSA Audio destroy.");
    }

    void LinuxAlsa::closeALSAAudio(configuration::ALSAAudioContext& alsaAudioContext, const snd_pcm_stream_t& stream)
    {
        if (alsaAudioContext.alsaState < configuration::ALSAState::ALSA_STATE_READY)
        {
            LOG_DEBUG_MSG("close failed, ALSA Audio status not ready.");
            return;
        }
        if (alsaAudioContext.alsaState == ALSAState::ALSA_STATE_STARTING)
        {
            stopALSAAudio(alsaAudioContext, stream);
        }
        
        closeALSAAudioInternal(alsaAudioContext);
        alsaAudioContext.alsaState = ALSAState::ALSA_STATE_CLOSING;
    }

    int LinuxAlsa::stopALSAAudio(configuration::ALSAAudioContext& alsaAudioContext, const snd_pcm_stream_t& stream)
    {
        if (alsaAudioContext.alsaState < ALSAState::ALSA_STATE_STARTING)
        {
            LOG_DEBUG_MSG("stop failed, ALSA Audio status not starting.");
            return -1;
        }

        int ret = stopALSAAudioInternal(static_cast<snd_pcm_t*>(alsaAudioContext.pcmHandle));
        alsaAudioContext.alsaState = ALSAState::ALSA_STATE_STOPPING;
        keep_running = false;

        if (SND_PCM_STREAM_CAPTURE == stream)
        {
            /* wait for the pcm thread quit first */
            if (alsaAudioContext.audioThread.joinable())
            {
                alsaAudioContext.audioThread.join();
            }
        }
        return ret;
    }

    int LinuxAlsa::stopALSAAudioInternal(snd_pcm_t* pcmHandle)
    {
        return snd_pcm_drop(pcmHandle);
    }

    void LinuxAlsa::closeALSAAudioInternal(configuration::ALSAAudioContext& alsaAudioContext)
    {
        snd_pcm_t * handle = static_cast<snd_pcm_t*>(alsaAudioContext.pcmHandle);
        if (handle)
        {
            snd_pcm_close(handle);
            alsaAudioContext.pcmHandle = nullptr;
        }
        freeAudioBuffer(alsaAudioContext);
    }

    // free audio buffer
    void LinuxAlsa::freeAudioBuffer(configuration::ALSAAudioContext& alsaAudioContext)
    {
        if (not alsaAudioContext.audioBuffer.empty())
        {
			alsaAudioContext.audioBuffer.clear();
        }
    }

    // clean up resource
    void LinuxAlsa::cleanUpALSAAudioResource(configuration::ALSAAudioContext& alsaAudioContext)
    {
        if (alsaAudioContext.pcmHandle)
        {
            closePCMDevice(static_cast<snd_pcm_t*>(alsaAudioContext.pcmHandle));
            alsaAudioContext.pcmHandle = nullptr;
        }
        freeAudioBuffer(alsaAudioContext);
    }

    // close pcm device
    void LinuxAlsa::closePCMDevice(snd_pcm_t* pcm)
    {
        if (nullptr != pcm)
        {
            snd_pcm_close(pcm);
        }
    }

    // is stop record
    int LinuxAlsa::isALSAAudioStopped(configuration::ALSAAudioContext& alsaAudioContext)
    {
        if (alsaAudioContext.alsaState == configuration::ALSAState::ALSA_STATE_STARTING)
        {
            LOG_DEBUG_MSG("stop failed, ALSA Audio status is starting.");
            return 0;
        }

        return isStoppedInternal(alsaAudioContext);
    }

    bool LinuxAlsa::isStoppedInternal(configuration::ALSAAudioContext& alsaAudioContext)
    {
        snd_pcm_state_t state = snd_pcm_state(static_cast<snd_pcm_t*>(alsaAudioContext.pcmHandle));
        switch (state) 
        {
        case SND_PCM_STATE_RUNNING:
        case SND_PCM_STATE_DRAINING:
            return false;
        default: break;
        }
        return true;
    }

 /****************API for recorder configurations********************************/
    configuration::audioDevInfo LinuxAlsa::getDefaultDev()
    {
        configuration::audioDevInfo recordDev;
        recordDev.devName = "default";
        return recordDev;
    }

    configuration::audioDevInfo LinuxAlsa::setAudioDev(const std::string& dev)
    {
        configuration::audioDevInfo recordDev;
        recordDev.devName = dev;
        return recordDev;
    }

    unsigned int LinuxAlsa::getAudioDevNum(const snd_pcm_stream_t& stream)
    {
        // stream SND_PCM_STREAM_PLAYBACK or SND_PCM_STREAM_CAPTURE，respectively play or record for PCM 
        return getPCMDeviceCnt(stream);
    }

    // return the count of pcm device , list all cards
    int LinuxAlsa::getPCMDeviceCnt(const snd_pcm_stream_t& stream)
    {
        void **hints, **n;
        char *io = nullptr;
        char* name = nullptr;
        char* descr = nullptr;
        int cnt = 0;
        std::string filter = stream == SND_PCM_STREAM_CAPTURE ? "Input" : "Output";;
        LOG_DEBUG_MSG("*****************************PCM {} Device Infomation***********************************", filter);

        if (snd_device_name_hint(-1, "pcm", &hints) < 0)
            return 0;
        n = hints;
        while (*n != nullptr)
        {
            io = snd_device_name_get_hint(*n, "IOID");
            name = snd_device_name_get_hint(*n, "NAME");
            descr = snd_device_name_get_hint(*n, "DESC");
            if (name && descr && (io != nullptr && strcmp(io, filter.c_str()) == 0))
            {
                LOG_DEBUG_MSG("PCM Device Name: {}, describe: {}",name, descr);
                cnt++;
            }
            if (io != nullptr)
            {
                free(io);
            }
            if (name != nullptr)
            {
                free(name);
            }
			if (nullptr != descr)
			{
				free(descr);
			}
            n++;
        }

        snd_device_name_free_hint(hints);
        LOG_DEBUG_MSG("**************************PCM {} Device Infomation****************************************", filter);
        return cnt;
    }

    bool LinuxAlsa::setALSAAudioParams(configuration::ALSAAudioContext& alsaAudioContext)
    {
        if (not m_waveFormat)
        {
            return false;
        }

        bool ret = setHWParams(alsaAudioContext);
        if (not ret)
        {
            return ret;
        }
        return setSWParams(alsaAudioContext);
    }

    bool LinuxAlsa::setHWParams(configuration::ALSAAudioContext& alsaAudioContext)
    {
        snd_pcm_hw_params_t* hwParams;
        snd_pcm_t* handle = static_cast<snd_pcm_t*>(alsaAudioContext.pcmHandle);
        /* this function initializes the allocated snd_pcm_hw_params_t struct
        with the sound card's full configuration space parameter */
        snd_pcm_hw_params_alloca(&hwParams);
        int ret = snd_pcm_hw_params_any(handle, hwParams);
        if (ret < 0)
        {
            LOG_ERROR_MSG("Broken configuration hwParms for this PCM {}.", snd_strerror(ret));
            return false;
        }
        /* set access type
        SND_PCM_ACCESS_RW_INTERLEAVED
        Staggered access. Each PCM frame in the buffer contains a continuous sampling of all
        the channel Setting. For example, the sound card wants to play 16-bit PCM stereo data,
        which means that there is 16-bit left channel data in each PCM frame, and then 16-bit right channel data.
        */
        ret = snd_pcm_hw_params_set_access(handle, hwParams,
            SND_PCM_ACCESS_RW_INTERLEAVED);
        if (ret < 0)
        {
            LOG_ERROR_MSG("Access type not available {}.", ret);
            return false;
        }

        snd_pcm_format_t pcmFormat = snd_pcm_build_linear_format(m_waveFormat->wBitsPerSample,
            m_waveFormat->wBitsPerSample, m_waveFormat->wBitsPerSample == 8 ? 1 : 0, 0);
        if (pcmFormat == SND_PCM_FORMAT_UNKNOWN)
        {
            LOG_ERROR_MSG("Invalid format SND_PCM_FORMAT_UNKNOWN.");
            return false;
        }
        ret = snd_pcm_hw_params_set_format(handle, hwParams, pcmFormat);
        if (ret < 0)
        {
            LOG_ERROR_MSG("Sample format non available {}.", snd_strerror(ret));
            return false;
        }

        ret = snd_pcm_hw_params_set_channels(handle, hwParams, m_waveFormat->nChannels);
        if (ret < 0)
        {
            const char* err = snd_strerror(ret);
            LOG_ERROR_MSG("Channels count non available {}.", err);
            return false;
        }
        unsigned int rate = m_waveFormat->nSamplesPerSec;
        ret = snd_pcm_hw_params_set_rate_near(handle, hwParams, &rate, 0);
        if (ret < 0)
        {
            LOG_ERROR_MSG("Set sample rate failed {}.", snd_strerror(ret));
            return false;
        }
        if (rate != m_waveFormat->nSamplesPerSec)
        {
            LOG_ERROR_MSG("Sample rate mismatch.");
            return false;
        }
        const char *pcmName = snd_pcm_name(handle);
        LOG_DEBUG_MSG("Using PCM name: {}", pcmName);

        //alsaAudioContext.bufferTime = BufferTime;
        //alsaAudioContext.periodTime = PeriodTime;
        if (0 == alsaAudioContext.bufferTime || 0 == alsaAudioContext.periodTime)
        {
            ret = snd_pcm_hw_params_get_buffer_time_max(hwParams,
                &alsaAudioContext.bufferTime, 0);
            if (ret < 0)
            {
                LOG_ERROR_MSG("Get buffer time failed use default value.");
                alsaAudioContext.bufferTime = BufferTime;
                alsaAudioContext.periodTime = PeriodTime;
            }
            else if (alsaAudioContext.bufferTime > 500000)
            {
                alsaAudioContext.bufferTime = 500000;
                alsaAudioContext.periodTime = alsaAudioContext.bufferTime / 4;
            }
        }
        ret = snd_pcm_hw_params_set_period_time_near(handle, hwParams,
            &alsaAudioContext.periodTime, 0);
        if (ret < 0)
        {
            LOG_ERROR_MSG("set period time fail {}.", snd_strerror(ret));
            return false;
        }
        ret = snd_pcm_hw_params_set_buffer_time_near(handle, hwParams,
            &alsaAudioContext.bufferTime, 0);
        if (ret < 0)
        {
            LOG_ERROR_MSG("set buffer time failed {}.", snd_strerror(ret));
            return false;
        }

        /*注意在ALSA中peroid_size是以frame为单位的。
        * The configured buffer and period sizes are stored in "frames" in the runtime. 
        * 1 frame = channels * sample_size. 
        * 所以要对peroid_size进行转换：chunk_bytes = peroid_size * sample_length / 8。
        * chunk_bytes就是我们单次从WAV读PCM数据的大小。
        */
        snd_pcm_uframes_t size = 0;
        ret = snd_pcm_hw_params_get_period_size(hwParams, &size, 0);
        if (ret < 0)
        {
            LOG_ERROR_MSG("get period size fail {}.", snd_strerror(ret));
            return false;
        }
		alsaAudioContext.periodFrames = size;

        ret = snd_pcm_hw_params_get_buffer_size(hwParams, &size);
        if (ret < 0)
        {
            LOG_ERROR_MSG("get buffer size fail {}.", snd_strerror(ret));
            return false;
        }
        if (size == alsaAudioContext.periodFrames)
        {
            LOG_ERROR_MSG("Can't use period equal to buffer size {} == {})",
                size, alsaAudioContext.periodFrames);
            return false;
        }
        alsaAudioContext.bufferFrames = size;

        if (snd_pcm_format_physical_width(pcmFormat) != m_waveFormat->wBitsPerSample)
        {
            LOG_WARNING_MSG("Bits per sample mismatch with wave fromat.");
        }
        alsaAudioContext.bitsPerFrame = m_waveFormat->wBitsPerSample * m_waveFormat->nChannels;

        /* set to driver */
        ret = snd_pcm_hw_params(handle, hwParams);
        if (ret < 0)
        {
            LOG_ERROR_MSG("Unable to install hw params {}.", snd_strerror(ret));
            return false;
        }
        LOG_INFO_MSG(m_logger, "ALSA audio period time: {}, period frames: {}; buffer time: {}, buffer frames: {}",
            alsaAudioContext.periodTime, alsaAudioContext.periodFrames, 
            alsaAudioContext.bufferTime, alsaAudioContext.bufferFrames);

        return true;
    }

    bool LinuxAlsa::setSWParams(configuration::ALSAAudioContext& alsaAudioContext)
    {
        snd_pcm_sw_params_t *swParams;
        snd_pcm_t * handle = static_cast<snd_pcm_t*>(alsaAudioContext.pcmHandle);

        snd_pcm_sw_params_alloca(&swParams);
        int ret = snd_pcm_sw_params_current(handle, swParams);
        if (ret < 0)
        {
            LOG_ERROR_MSG("get current sw para fail {}.", snd_strerror(ret));
            return false;
        }
        /* set a value bigger than the buffer frames to prevent the auto start.
         * we use the snd_pcm_start to explicit start the pcm
         * Suppose the third parameter is set to 320.
         * For record, when the user calls readi and the data volume reaches 320 frames,
         * the alsa driver will start AD conversion and capture data. */
//         ret = snd_pcm_sw_params_set_start_threshold(handle, swParams,
//             recorder.bufferFrames * 2);
         ret = snd_pcm_sw_params_set_start_threshold(handle, swParams,
             (alsaAudioContext.bufferFrames/ alsaAudioContext.periodFrames) * alsaAudioContext.periodFrames);
        if (ret < 0)
        {
            LOG_ERROR_MSG("set start threshold fail {}.", snd_strerror(ret));
            return false;
        }
        /* When the user USES snd_pcm_wait(), 
        * what this actually encapsulates is the poll call of the system, 
        * indicating that the user is waiting. 
        * For record, it is waiting for the newly collected data of the sound card below to reach a certain amount.
        * This number is set with snd_pcm_sw_params_set_avail_min.The unit is frame. */
        ret = snd_pcm_sw_params_set_avail_min(handle, swParams, alsaAudioContext.periodFrames);
        if (ret < 0)
        {
            LOG_ERROR_MSG("set avail min failed {}.", snd_strerror(ret));
            return false;
        }

        if ((ret = snd_pcm_sw_params(handle, swParams)) < 0)
        {
            LOG_ERROR_MSG("unable to install sw params {}.", snd_strerror(ret));
            return false;
        }
        return true;
    }

    std::string LinuxAlsa::coverToDescription(const snd_pcm_state_t& state)
    {
        switch (state)
        {
        case SND_PCM_STATE_OPEN:
            return "SND_PCM_STATE_OPEN";
        case SND_PCM_STATE_SETUP:
            return "SND_PCM_STATE_SETUP";
        case SND_PCM_STATE_PREPARED:
            return "SND_PCM_STATE_PREPARED";
        case SND_PCM_STATE_RUNNING:
            return "SND_PCM_STATE_RUNNING";
        case SND_PCM_STATE_XRUN:
            return "SND_PCM_STATE_XRUN";
        case SND_PCM_STATE_DRAINING:
            return "SND_PCM_STATE_DRAINING";
        case SND_PCM_STATE_PAUSED:
            return "SND_PCM_STATE_PAUSED";
        case SND_PCM_STATE_SUSPENDED:
            return "SND_PCM_STATE_SUSPENDED";
        case SND_PCM_STATE_DISCONNECTED:
            return "SND_PCM_STATE_DISCONNECTED";
        }
        return "PCM_UNKNOW_STATE";
    }

} // namespace usbAudio