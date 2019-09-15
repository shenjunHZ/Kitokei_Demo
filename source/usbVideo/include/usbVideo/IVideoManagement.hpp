#pragma once
#include <string>

namespace usbVideo
{
    class IVideoManagement
    {
    public:
        virtual ~IVideoManagement() = default;
        // timeout each file write
        virtual void onTimeout() = 0;
        virtual void runVideoManagement() = 0;
    };
} // namespace Video