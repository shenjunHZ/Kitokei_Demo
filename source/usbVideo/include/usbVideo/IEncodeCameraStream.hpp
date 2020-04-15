#pragma once
#include <string>
#include <functional>

namespace configuration
{
    struct bestFrameSize;
} // namespace configuration

namespace usbVideo
{
    using CloseVideoNotify = std::function<void(void)>;

    class IEncodeCameraStream
    {
    public:
        virtual ~IEncodeCameraStream() = default;
        // init av register
        virtual bool initRegister(const std::string& inputVideoFile, const configuration::bestFrameSize& frameSize, 
            const std::string& inputAudioFile) = 0;
        // prepare context
        virtual bool initCodecContext(const std::string& outputFile) = 0;
        // run write output file
        virtual void runWriteFile() = 0;
        // stop write output file
        virtual void stopWriteFile() = 0;
        // create encoder
        virtual bool createEncoder() = 0;
        // destroy encoder
        virtual void destroyEncoder() = 0;
    };
} // namespace usbVideo