#pragma once

namespace usbAudio
{
    class IAudioPlaybackService
    {
    public:
        virtual ~IAudioPlaybackService() = default;

        virtual bool initAudioPlayback() = 0;
        virtual void exitAudioPlayback() = 0;

        virtual int audioStartPlaying() = 0;
        virtual int audioStopPlaying() = 0;
        //virtual void setRegisterNotify(std::function<void(const std::string& result, const bool& isLast)>) = 0;
    };
} // namespace usbAudio