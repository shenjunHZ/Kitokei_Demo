#include "usbVideo/VideoManagement.hpp"
#include "timer/TimerService.hpp"
#include "Configurations/ParseConfigFile.hpp"
#include "common/CommonFunction.hpp"

namespace
{
    int getVideoTimes(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::videoTimes) != config.end())
        {
            return config[configuration::videoTimes].as<int>();
        }
        return 30;
    }

    std::string getVideoName(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::videoName) != config.end())
        {
            return config[configuration::videoName].as<std::string>();
        }
        return "chessVideo";
    }
} // namespace

namespace usbVideo
{
    std::string VideoManagement::TimeStamp::now() const
    {
        auto time = std::time(nullptr);
        char string[30]{};
        std::strftime(string, sizeof(string), "%F_%T", std::localtime(&time));
        return string;
    }

    VideoManagement::VideoManagement(Logger& logger, const configuration::AppConfiguration& config,
        timerservice::TimerService& timerService)
        : m_logger{logger}
        , m_config{config}
        , m_streamProcess{ std::make_unique<StreamProcess>(logger, config) }
        , m_timeStamp{ std::make_unique<TimeStamp>() }
    {
        std::chrono::milliseconds period = std::chrono::milliseconds{ getVideoTimes(m_config) * 1000 * 60 };

        m_timer = timerService.schedulePeriodicTimer(period, [this]()
        {
            onTimeout();
        });

        std::string outputFile = common::getCaptureOutputDir(config) + getVideoName(config) + m_timeStamp->now();
        streamThread = std::thread([&m_streamProcess = this->m_streamProcess, &outputFile]() 
        {
            m_streamProcess->startEncodeStream(outputFile);
        });
    }

    void VideoManagement::onTimeout()
    {
        m_streamProcess->stopEncodeStream();

        if (streamThread.joinable())
        {
            streamThread.join();
        }

        std::string outputFile = common::getCaptureOutputDir(m_config) + getVideoName(m_config) + m_timeStamp->now();
        streamThread = std::thread([&m_streamProcess = this->m_streamProcess, &outputFile]()
        {
            m_streamProcess->startEncodeStream(outputFile);
        });
    }

} // namespace usbVideo