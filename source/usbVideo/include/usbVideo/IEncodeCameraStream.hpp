#pragma once
#include <string>

namespace usbVideo
{
    class IEncodeCameraStream
    {
    public:
        virtual ~IEncodeCameraStream() = default;
        // init av register
        virtual bool initRegister(const std::string& inputFile) = 0;
        // prepare context
        virtual bool prepareOutputContext(const std::string& outputFile) = 0;
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