#include "usbAudio/LinuxRec.hpp"
#include "logger/Logger.hpp"
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
    LinuxRec::LinuxRec(std::unique_ptr<configuration::WAVEFORMATEX> waveFormat)
        : m_waveFormat{ std::move(waveFormat) }
    {

    }

 /***********************API for create recorder and playback****************************************/
    int LinuxRec::openPlayback(configuration::AudioPlayback& playback,
        const configuration::audioDevInfo& devInfo)
    {
        if (playback.playbackState >= PlaybackState::PLAYBACK_STATE_READY)
        {
            return 0;
        }

        int ret = snd_pcm_open(reinterpret_cast<snd_pcm_t**>(&playback.pcmHandle), devInfo.devName.c_str(),
            SND_PCM_STREAM_PLAYBACK, 0);
        if (ret < 0)
        {
            const char* err = snd_strerror(ret);
            LOG_ERROR_MSG("Open playback pcm device: {}, failed {}", devInfo.devName, err);
            if (playback.pcmHandle)
            {
                closeRecDevice(static_cast<snd_pcm_t*>(playback.pcmHandle));
                playback.pcmHandle = nullptr;
            }
            return static_cast<int>(RecordErrorCode::RECORD_ERR_INVAL);
        }
        if (ret == 0)
        {
            playback.playbackState = PlaybackState::PLAYBACK_STATE_READY;
        }
        return ret;
    }

    // create recorder use capture
    int LinuxRec::createRecorder(configuration::AudioRecorder& recorder,
        std::function<void(const std::string& data)> onDataInd, void* userCallbackPara)
    {
        recorder.onDataInd = onDataInd;
        recorder.userCallbackPara = userCallbackPara;
        recorder.recordState = RecordState::RECORD_STATE_CREATED;

        return 0;
    }

    int LinuxRec::openRecorder(configuration::AudioRecorder& recorder,
        const configuration::audioDevInfo& recordDev)
    {
        if (recorder.recordState >= RecordState::RECORD_STATE_READY)
        {
            return 0;
        }

        int ret = openRecorderInternal(recorder, recordDev, SND_PCM_STREAM_CAPTURE);
        if (ret == 0)
        {
            recorder.recordState = RecordState::RECORD_STATE_READY;
        }
        return ret;
    }

    int LinuxRec::openRecorderInternal(configuration::AudioRecorder& recorder,
        const configuration::audioDevInfo& recordDev, const snd_pcm_stream_t& stream)
    {
        unsigned int buf_size;
        /*open PCM device, in ALSA each PCM device have the match name to be used.
        * for example: PCM name define " char *pcm_name = "plughw:0,0" "
        * the important PCM device interface“plughw”and“hw”. 
        * use "plughw" interface，do not care hardware, and if set the config params not match with the real hardware params,
        * ALSA will auto change the config. if use "hw" interface，we shall to check hardware params whether it support or not.
        * plughw device id and sub-device id.
        * stream: SND_PCM_STREAM_CAPTURE / SND_PCM_STREAM_PLAYBACK
        * mode: SND_PCM_NONBLOCK
        */
        int ret = snd_pcm_open(reinterpret_cast<snd_pcm_t**>(&recorder.pcmHandle), recordDev.devName.c_str(),
            stream, 0);
        if (ret < 0)
        {
            const char* err = snd_strerror(ret);
            LOG_ERROR_MSG("Open pcm device: {}, failed {}", recordDev.devName, err);
            cleanUpRecorderResource(recorder);
            return static_cast<int>(RecordErrorCode::RECORD_ERR_INVAL);
        }

        if (not setRecorderParams(recorder))
        {
            cleanUpRecorderResource(recorder);
            return static_cast<int>(RecordErrorCode::RECORD_ERR_INVAL);
        }

        size_t size = (recorder.periodFrames * recorder.bitsPerFrame / BitsByte);
        recorder.audioBuffer = new char[size];
        if (not recorder.audioBuffer)
        {
            cleanUpRecorderResource(recorder);
            LOG_ERROR_MSG("Create audio buffer failed.");
            return static_cast<int>(RecordErrorCode::RECORD_ERR_MEMFAIL);
        }

        return ret;
    }

    int LinuxRec::startRecord(configuration::AudioRecorder& recorder)
    {
        if (recorder.recordState < configuration::RecordState::RECORD_STATE_READY)
        {
            return static_cast<int>(configuration::RecordErrorCode::RECORD_ERR_NOT_READY);
        }
        if (recorder.recordState == configuration::RecordState::RECORD_STATE_RECORDING)
        {
            return 0;
        }
//         size_t audioTotalSize = audioSecondsDuration * m_waveFormat->nSamplesPerSec *
//             m_waveFormat->wBitsPerSample * m_waveFormat->nChannels / 8;

        int ret = startRecordInternal(static_cast<snd_pcm_t*>(recorder.pcmHandle));
        if (ret == 0)
        {
            recorder.recordState = configuration::RecordState::RECORD_STATE_RECORDING;
            recorder.recThread = std::thread([this, &recorder]()
            {
                this->createCallbackThread(recorder);
            });
        }

        return ret;
    }

    int LinuxRec::startRecordInternal(snd_pcm_t* pcmHandle)
    {
        if (nullptr == pcmHandle)
        {
            return -1;
        }
        return snd_pcm_start(pcmHandle);
    }

    void LinuxRec::createCallbackThread(configuration::AudioRecorder& recorder)
    {
        while (keep_running)
        {
            size_t frames = recorder.periodFrames;
            size_t bytes = frames * recorder.bitsPerFrame / BitsByte;

            /* closing, exit the thread */
            if (RecordState::RECORD_STATE_STOPPING == recorder.recordState
                || RecordState::RECORD_STATE_CLOSING == recorder.recordState)
            {
                return;
            }

            if (RecordState::RECORD_STATE_RECORDING > recorder.recordState)
            {
                usleep(100 * 1000);
            }

            //memset(recorder.audioBuffer, 0, bytes);
            if (pcmRead(recorder, frames) != frames)
            {
                return;
            }

            if (recorder.onDataInd)
            {
                std::string recordData(recorder.audioBuffer, bytes);
                recorder.onDataInd(recordData);
            }
        }
        return;
    }

    int LinuxRec::pcmRead(configuration::AudioRecorder& recorder, const size_t& frameSize)
    {
        snd_pcm_t* handle = static_cast<snd_pcm_t*>(recorder.pcmHandle);
        char* audioData = recorder.audioBuffer;
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
                if (xRunRecovery(recorder, rData) < 0)
                {
                    return -1;
                }
            }

            if (rData > 0)
            {
                count -= rData;
                //LOG_DEBUG_MSG("====Write audio data frame: {}", rData);
                audioData += rData * recorder.bitsPerFrame / BitsByte;
            }
        }
        return frameSize;
    }

    int LinuxRec::xRunRecovery(configuration::AudioRecorder& recorder, const snd_pcm_sframes_t& data)
    {
        snd_pcm_t* handle = static_cast<snd_pcm_t*>(recorder.pcmHandle);
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

 /************************API for clear resource*************************************/

    void LinuxRec::destroyRecorder(configuration::AudioRecorder& recorder)
    {
        LOG_DEBUG_MSG("Recorder destroy.");
    }

    void LinuxRec::closeRecorder(configuration::AudioRecorder& recorder)
    {
        if (recorder.recordState < configuration::RecordState::RECORD_STATE_READY)
        {
            return;
        }
        if (recorder.recordState == RecordState::RECORD_STATE_RECORDING)
        {
            stopRecord(recorder);
        }
        recorder.recordState = RecordState::RECORD_STATE_CLOSING;
        closeRecorderInternal(recorder);

        recorder.recordState = RecordState::RECORD_STATE_CREATED;
    }

    // stop record
    int LinuxRec::stopRecord(configuration::AudioRecorder& recorder)
    {
        if (recorder.recordState < RecordState::RECORD_STATE_RECORDING)
        {
            return 0;
        }

        recorder.recordState = RecordState::RECORD_STATE_STOPPING;
        keep_running = false;
        int ret = stopRecordInternal(static_cast<snd_pcm_t*>(recorder.pcmHandle));
//         if (ret == 0)
//         {
//             recorder.recordState = configuration::RecordState::RECORD_STATE_READY;
//         }
//         if (recorder.recThread.joinable())
//         {
//             recorder.recThread.join();
//         }

        return ret;
    }

    int LinuxRec::stopRecordInternal(snd_pcm_t* pcmHandle)
    {
        return snd_pcm_drop(pcmHandle);
    }

    void LinuxRec::closeRecorderInternal(configuration::AudioRecorder& recorder)
    {
        snd_pcm_t * handle = static_cast<snd_pcm_t*>(recorder.pcmHandle);

        /* wait for the pcm thread quit first */
        keep_running = false;
        if (recorder.recThread.joinable())
        {
            recorder.recThread.join();
        }

        if (handle)
        {
            snd_pcm_close(handle);
            recorder.pcmHandle = nullptr;
        }
        freeRecBuffer(recorder);
    }

    // free record buffer
    void LinuxRec::freeRecBuffer(configuration::AudioRecorder& recorder)
    {
        if (recorder.audioBuffer)
        {
            delete[](recorder.audioBuffer);
            recorder.audioBuffer = nullptr;
        }
    }

    // clean up resource
    void LinuxRec::cleanUpRecorderResource(configuration::AudioRecorder& recorder)
    {
        if (recorder.pcmHandle)
        {
            closeRecDevice(static_cast<snd_pcm_t*>(recorder.pcmHandle));
            recorder.pcmHandle = nullptr;
        }
        freeRecBuffer(recorder);
    }

    // close record device
    void LinuxRec::closeRecDevice(snd_pcm_t* pcm)
    {
        if (nullptr != pcm)
        {
            snd_pcm_close(pcm);
        }
    }

    // is stop record
    int LinuxRec::isRecordStopped(configuration::AudioRecorder& recorder)
    {
        if (recorder.recordState == configuration::RecordState::RECORD_STATE_RECORDING)
        {
            return 0;
        }

        return isStoppedInternal(recorder);
    }

    bool LinuxRec::isStoppedInternal(configuration::AudioRecorder& recorder)
    {
        snd_pcm_state_t state;

        state = snd_pcm_state(static_cast<snd_pcm_t*>(recorder.pcmHandle));
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
    configuration::audioDevInfo LinuxRec::getDefaultInputDev()
    {
        configuration::audioDevInfo recordDev;
        recordDev.devName = "default";
        return recordDev;
    }

    configuration::audioDevInfo LinuxRec::setInputDev(const std::string& dev)
    {
        configuration::audioDevInfo recordDev;
        recordDev.devName = dev;
        return recordDev;
    }

    unsigned int LinuxRec::getInputDevNum(const snd_pcm_stream_t& stream)
    {
        // stream SND_PCM_STREAM_PLAYBACK or SND_PCM_STREAM_CAPTURE，respectively play or record for PCM 
        return getPCMDeviceCnt(stream);
    }

    // return the count of pcm device , list all cards
    int LinuxRec::getPCMDeviceCnt(const snd_pcm_stream_t& stream)
    {
        void **hints, **n;
        char *io = nullptr;
        char* name = nullptr;
        char* descr = nullptr;
        int cnt = 0;
        std::string filter = stream == SND_PCM_STREAM_CAPTURE ? "Input" : "Output";;
        LOG_DEBUG_MSG("***********PCM {} Device Infomation**********", filter);

        if (snd_device_name_hint(-1, "pcm", &hints) < 0)
            return 0;
        n = hints;
        while (*n != nullptr)
        {
            io = snd_device_name_get_hint(*n, "IOID");
            name = snd_device_name_get_hint(*n, "NAME");
            descr = snd_device_name_get_hint(*n, "DESC");
            if (name && (io == nullptr || strcmp(io, filter.c_str()) == 0))
            {
                LOG_DEBUG_MSG("PCM Device Name: {}, describe: {}",name, descr);
                cnt++;
            }
            if (io != nullptr)
            {
                free(io);
            }
            if (name != nullptr)
                free(name);
            n++;
        }

        snd_device_name_free_hint(hints);
        LOG_DEBUG_MSG("***********PCM {} Device Infomation**********", filter);
        return cnt;
    }

    bool LinuxRec::setRecorderParams(configuration::AudioRecorder& recorder)
    {
        if (not m_waveFormat)
        {
            return false;
        }

        bool ret = setHWParams(recorder);
        if (not ret)
        {
            return ret;
        }
        return setSWParams(recorder);
    }

    bool LinuxRec::setHWParams(configuration::AudioRecorder& recorder)
    {
        snd_pcm_hw_params_t* hwParams;
        snd_pcm_t* handle = static_cast<snd_pcm_t*>(recorder.pcmHandle);
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

        //recorder.bufferTime = BufferTime;
        //recorder.periodTime = PeriodTime;
        if (0 == recorder.bufferTime || 0 == recorder.periodTime)
        {
            ret = snd_pcm_hw_params_get_buffer_time_max(hwParams,
                &recorder.bufferTime, 0);
            if (ret < 0)
            {
                LOG_ERROR_MSG("Get buffer time failed use default value.");
                recorder.bufferTime = BufferTime;
                recorder.periodTime = PeriodTime;
            }
            else if (recorder.bufferTime > 500000)
            {
                recorder.bufferTime = 500000;
                recorder.periodTime = recorder.bufferTime / 4;
            }
        }
        ret = snd_pcm_hw_params_set_period_time_near(handle, hwParams,
            &recorder.periodTime, 0);
        if (ret < 0)
        {
            LOG_ERROR_MSG("set period time fail {}.", snd_strerror(ret));
            return false;
        }
        ret = snd_pcm_hw_params_set_buffer_time_near(handle, hwParams,
            &recorder.bufferTime, 0);
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
        recorder.periodFrames = size;
        if (ret < 0)
        {
            LOG_ERROR_MSG("get period size fail {}.", snd_strerror(ret));
            return false;
        }
        ret = snd_pcm_hw_params_get_buffer_size(hwParams, &size);
        if (ret < 0)
        {
            LOG_ERROR_MSG("get buffer size fail {}.", snd_strerror(ret));
            return false;
        }
        if (size == recorder.periodFrames)
        {
            LOG_ERROR_MSG("Can't use period equal to buffer size {} == {})",
                size, recorder.periodFrames);
            return false;
        }
        recorder.bufferFrames = size;

        if (snd_pcm_format_physical_width(pcmFormat) != m_waveFormat->wBitsPerSample)
        {
            LOG_WARNING_MSG("Bits per sample mismatch with wave fromat.");
        }
        recorder.bitsPerFrame = m_waveFormat->wBitsPerSample * m_waveFormat->nChannels;

        /* set to driver */
        ret = snd_pcm_hw_params(handle, hwParams);
        if (ret < 0)
        {
            LOG_ERROR_MSG("Unable to install hw params {}.", snd_strerror(ret));
            return false;
        }

        return true;
    }

    bool LinuxRec::setSWParams(configuration::AudioRecorder& recorder)
    {
        snd_pcm_sw_params_t *swParams;
        snd_pcm_t * handle = static_cast<snd_pcm_t*>(recorder.pcmHandle);

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
             (recorder.bufferFrames/recorder.periodFrames) * recorder.periodFrames);
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
        ret = snd_pcm_sw_params_set_avail_min(handle, swParams, recorder.periodFrames);
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
} // namespace usbAudio