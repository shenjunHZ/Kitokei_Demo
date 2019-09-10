#pragma once

namespace usbVideo
{
    class IEncodeCameraStream
    {
    public:
        virtual ~IEncodeCameraStream() = default;
        // register
        virtual void avRegisterAll() = 0;
        // context
        virtual bool avFormatAllocContext(AVFormatContext&) = 0;
        // get format
        virtual bool avGuessFormat(std::string& filePath, AVOutputFormat&) = 0;
        // open file
        virtual bool avIoOpen() = 0;
        // create stream
        virtual bool avFormatNewStream(AVFormatContext&, AVStream&) = 0;
    };
} // namespace usbVideo