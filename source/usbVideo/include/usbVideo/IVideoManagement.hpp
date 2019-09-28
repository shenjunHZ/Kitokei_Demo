#pragma once
#include <string>

namespace configuration
{
    struct bestFrameSize;
} // namespace configuration

namespace usbVideo
{
    class IVideoManagement
    {
    public:
        virtual ~IVideoManagement() = default;
        // timeout each file write
        virtual void onTimeout() = 0;
        virtual void runVideoManagement() = 0;
        virtual bool initVideoManagement(const configuration::bestFrameSize& frameSize) = 0;
    };
} // namespace Video