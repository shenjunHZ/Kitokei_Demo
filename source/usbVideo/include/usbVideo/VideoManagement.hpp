#pragma once
#include "IVideoManagement.hpp"
#include "StreamProcess.hpp"
#include "logger/Logger.hpp"
#include "Configurations/ParseConfigFile.hpp"

namespace timerservice
{
    class TimerService;
    class Timer;
} // namespace timerservice

namespace usbVideo
{
    class VideoManagement final : public IVideoManagement
    {
    public:
        struct TimeStamp
        {
            virtual ~TimeStamp() = default;

            // Returns current timestamp string in extended iso format
            virtual std::string now() const;
        };

        VideoManagement(Logger& logger, const configuration::AppConfiguration& config,
            timerservice::TimerService& timerService);
        ~VideoManagement();

        void runVideoManagement() override;
        bool initVideoManagement(const configuration::bestFrameSize& frameSize) override;

    private:
        void onTimeout() override;

    private:
        Logger& m_logger;
        const configuration::AppConfiguration& m_config;
        std::unique_ptr<StreamProcess> m_streamProcess;
        std::unique_ptr<TimeStamp> m_timeStamp;
        timerservice::TimerService& m_timerService;

        std::thread streamThread;
        std::unique_ptr<timerservice::Timer> m_timer;
        configuration::bestFrameSize m_bestFrameSize;
    };
} // namespace usbVideo