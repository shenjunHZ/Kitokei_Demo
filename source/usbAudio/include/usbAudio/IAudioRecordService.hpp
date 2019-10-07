#pragma once

namespace usbAudio
{
    class IAudioRecordService
    {
    public:
        virtual ~IAudioRecordService() = default;

        virtual bool initAudioRecord() = 0;
        virtual void exitAudioRecord() = 0;

        virtual int audioStartListening() = 0;
        virtual int audioStopListening() = 0;
        virtual void setRegisterNotify(std::function<void(const std::string& result, const bool& isLast)>) = 0;
    };
} // namespace usbAudio