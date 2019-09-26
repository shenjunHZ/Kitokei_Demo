#include "usbVideo/EncodeCameraStream.hpp"
#include "Configurations/ParseConfigFile.hpp"
#include "common/CommonFunction.hpp"

namespace
{
    int getVideoFPS(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::videoFPS) != config.end())
        {
            return config[configuration::videoFPS].as<int>();
        }
        return 25;
    }

    int getIndex(char c)
    {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        else if (c == '-')
        {
            return 10;
        }
        else if (c == ' ')
        {
            return 11;
        }
        else if (c == ':')
        {
            return 12;
        }
        return 11;
    }

    constexpr int threadCounts = 8;
    constexpr int minQuantizer = 10;
    constexpr int maxQuantizer = 51;
    constexpr int maxBframe = 3;
    constexpr int RGBCountSize = 3;
    // If equal to 0, alignment will be chosen automatically for the current CPU.
    constexpr int alignment = 0;

    const std::string timePattern = "yyyy-MM-dd HH:mm:ss";
    constexpr int wateMarkWidth = 17;
    constexpr int wateMarkHeight = 23;
    constexpr int off_x = 18;
    constexpr int off_y = 18;

    std::atomic_bool keep_running{ true };
}// namespace 
namespace usbVideo
{
    EncodeCameraStream::EncodeCameraStream(Logger& logger, const configuration::AppConfiguration& config)
        : m_logger{logger}
        , m_config{config}
    {
        videoWidth = common::getCaptureWidth(config);
        videoHeight = common::getCaptureHeight(config);
    }

    EncodeCameraStream::~EncodeCameraStream()
    {
        if (m_fd)
        {
            fclose(m_fd);
            m_fd = nullptr;
        }
    }

    bool EncodeCameraStream::initRegister(const std::string& inputFile)
    {
        m_fd = fopen(inputFile.c_str(), "rb");
        if (nullptr == m_fd)
        {
            LOG_ERROR_MSG("open input file failed {}", inputFile);
            return false;
        }
        // register all, have not been used
        //av_register_all();
        // register all encode, could be removed as av_register_all
        //avcodec_register_all();
        // Initialize the filter system. Register all builtin filters
        //avfilter_register_all();

        initWatemake();
        return true;
    }

    void EncodeCameraStream::initWatemake()
    {
        int patternLen = timePattern.size();
        m_numArray.resize(wateMarkWidth * wateMarkHeight * patternLen);

        char NUM_0[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,0,0,1,1,1,1,
            1,0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,0,0,0,0,1,1,1,1,0,
            0,0,0,0,0,1,1,1,0,0,0,0,1,1,1,1,0,0,1,1,0,0,1,1,1,0,0,0,0,1,1,1,1,0,0,1,1,0,0,1,1,1,0,0,0,0,1,1,1,1,0,0,1,1,0,0,1,1,1,0,0,
            0,0,1,1,1,1,0,0,1,1,0,0,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,
            0,0,1,1,1,0,0,0,0,1,1,1,1,1,1,0,0,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,
            0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

        char NUM_1[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,
            0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,
            0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,
            0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
            1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,
            0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

        char NUM_2[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,
            1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,
            0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,
            0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,
            0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,
            1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

        char NUM_3[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,
            1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,
            1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,
            0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,
            0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
            0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

        char NUM_4[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,
            0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,1,1,1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,1,1,1,
            1,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
            0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
            1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,
            0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

        char NUM_5[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
            1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,
            0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,
            1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,
            0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,
            0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
            0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

        char NUM_6[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,
            0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,
            0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,
            0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,
            0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,
            0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

        char NUM_7[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
            1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,
            1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,
            0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,
            0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

        char NUM_8[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,
            1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,
            1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,
            0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,
            0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,
            0,0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

        char NUM_9[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,
            1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1,
            1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,
            0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,
            0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
            0,0,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

        char NUM_SPACE[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

        char NUM_COLON[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,
            0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,
            1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

        char NUM_LINE[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

        int size = wateMarkWidth * wateMarkHeight;
        memcpy(&m_numArray[0], NUM_0, size);
        memcpy(&m_numArray[size * 1], NUM_1, size);
        memcpy(&m_numArray[size * 2], NUM_2, size);
        memcpy(&m_numArray[size * 3], NUM_3, size);
        memcpy(&m_numArray[size * 4], NUM_4, size);
        memcpy(&m_numArray[size * 5], NUM_5, size);
        memcpy(&m_numArray[size * 6], NUM_6, size);
        memcpy(&m_numArray[size * 7], NUM_7, size);
        memcpy(&m_numArray[size * 8], NUM_8, size);
        memcpy(&m_numArray[size * 9], NUM_9, size);
        memcpy(&m_numArray[size * 10], NUM_LINE, size);
        memcpy(&m_numArray[size * 11], NUM_SPACE, size);
        memcpy(&m_numArray[size * 12], NUM_COLON, size);
    }

    bool EncodeCameraStream::initFilter()
    {
        if (nullptr == m_codecContext)
        {
            LOG_ERROR_MSG("init filter failed as codec context is empty.");
            return false;
        }
 
        const AVFilter* buffersrc = avfilter_get_by_name("buffer");
        const AVFilter* buffersink = avfilter_get_by_name("buffersink");
        AVFilterInOut *outputs = avfilter_inout_alloc();
        AVFilterInOut *inputs = avfilter_inout_alloc();
        if (not buffersrc or not buffersink or
            not outputs or not outputs)
        {
            LOG_ERROR_MSG("init filter failed as create filter failure.");
            return false;
        }
        //std::string filtersDescr = "scale=640:480";
        //std::string filtersDescr = "movie=my_logo.png[wm];[in][wm]overlay=5:5[out]";
        //std::string filtersDescr = "drawtext=fontfile=arial.ttf:fontcolor=white:fontsize=36:text='ABCD':x=20:y=20";
        //std::string filtersDescr = "drawbox=x=100:y=100:w=100:h=100:color=pink@0.5";
        //std::string filtersDescr = "crop=1/2*in_w:1/2*in_h:0:0";
        std::string filtersDescr = "drawtext=fontfile=/usr/share/fonts/truetype/ubuntu-font-family/lato/Lato-Black.ttf:fontcolor=white:fontsize=36:text='Chess Kitokei':x=18:y=200";

        enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };

        m_filterGraph = avfilter_graph_alloc();
        if (not m_filterGraph)
        {
            LOG_ERROR_MSG("init filter failed, create filter graph failure.");
            avfilter_inout_free(&inputs);
            avfilter_inout_free(&outputs);
            return false;
        }

        /* buffer video source: the decoded frames from the decoder will be inserted here. */
        char args[100];
        snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            m_codecContext->width, m_codecContext->height, m_codecContext->pix_fmt,
            m_codecContext->time_base.num, m_codecContext->time_base.den,
            m_codecContext->sample_aspect_ratio.num, m_codecContext->sample_aspect_ratio.den);
        LOG_DEBUG_MSG("filter args: {}", args);

        int ret = avfilter_graph_create_filter(&m_filterSrcContext, buffersrc, "in",
            args, NULL, m_filterGraph);
        if (ret < 0)
        {
            LOG_ERROR_MSG("Cannot create buffer source.");
            avfilter_inout_free(&inputs);
            avfilter_inout_free(&outputs);
            return false;
        }

        /* buffer video sink: to terminate the filter chain. */
        ret = avfilter_graph_create_filter(&m_filterSinkContext, buffersink, "out",
            NULL, NULL, m_filterGraph);
        if (ret < 0)
        {
            LOG_ERROR_MSG("Cannot create buffer sink.");
            avfilter_inout_free(&inputs);
            avfilter_inout_free(&outputs);
            return false;
        }

        ret = av_opt_set_int_list(m_filterSinkContext, "pix_fmts", pix_fmts,
            AV_PIX_FMT_YUV420P, AV_OPT_SEARCH_CHILDREN);
        if (ret < 0)
        {
            LOG_ERROR_MSG("Cannot set output filter sink context.");
            avfilter_inout_free(&inputs);
            avfilter_inout_free(&outputs);
            return false;
        }

        /* Endpoints for the filter graph. */
        outputs->name = av_strdup("in");
        outputs->filter_ctx = m_filterSrcContext;
        outputs->pad_idx = 0;
        outputs->next = NULL;

        inputs->name = av_strdup("out");
        inputs->filter_ctx = m_filterSinkContext;
        inputs->pad_idx = 0;
        inputs->next = NULL;
        if ( (ret = avfilter_graph_parse_ptr(m_filterGraph, filtersDescr.c_str(),
            &inputs, &outputs, NULL)) < 0 )
        {
            LOG_ERROR_MSG("avfilter_graph_parse_ptr failed {}.", ret);
            //avfilter_inout_free(&inputs);
            //avfilter_inout_free(&outputs);
            //return false;
        }

        if ( (ret = avfilter_graph_config(m_filterGraph, NULL)) < 0 )
        {
            LOG_ERROR_MSG("avfilter_graph_config failed {}.", ret);
            avfilter_inout_free(&inputs);
            avfilter_inout_free(&outputs);
            return false;
        }

        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        return true;
    }

    void EncodeCameraStream::addWatermarke(std::vector<uint8_t>& dataY)
    {
        //struct tm *tm_now = localtime(&time);
        auto timeT = std::time(nullptr);
        std::string tmp(32, 0);
        auto tmpSize = std::strftime(&tmp.front(), tmp.size(), "%Y-%m-%d %X", std::localtime(&timeT)); // 2019-07-01 10:17:23
        tmp.resize(tmpSize);
        int bigWidth = m_codecContext->width;

        int size = wateMarkWidth * wateMarkHeight;
        uint8_t* start = &dataY[0] + off_y * bigWidth + off_x;
        for (int i = 0; i < timePattern.size(); i++)
        {
            uint8_t* timeNum = nullptr;
            try
            {
                int index = getIndex(tmp.at(i));
                timeNum = &m_numArray[0] + size * index;
            }
            catch (const std::out_of_range& e)
            {
                LOG_ERROR_MSG("Get time out of range {}", e.what());
                break;
            }
            if (not timeNum)
            {
                return;
            }

            for (int j = 0; j < wateMarkHeight; j++)
            {
                uint8_t* destIndex = start + i * wateMarkWidth;

                for (int k = 0; k < wateMarkWidth; k++)
                {
                    if (*(timeNum + j * wateMarkWidth + k) == 1)
                    {
                        *(destIndex + j * bigWidth + k) = 235; // watermark, 235 white, 16 black
                    }
                }
            }
        }
    }

    bool EncodeCameraStream::createEncoder()
    {
        // Find a registered encoder with a matching codec ID.
        m_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (nullptr == m_codec)
        {
            LOG_ERROR_MSG("find encoder H264 failed.");
            return false;
        }
        // Allocate an AVCodecContext and set its fields to default values.
        m_codecContext = avcodec_alloc_context3(m_codec);
        if (nullptr == m_codecContext)
        {
            LOG_ERROR_MSG("alloc codec context failed.");
            return false;
        }
         m_codecContext->width = videoWidth;
         m_codecContext->height = videoHeight;
         m_codecContext->time_base = {1, getVideoFPS(m_config)};
         m_codecContext->framerate = {getVideoFPS(m_config), 1};
         m_codecContext->bit_rate = common::getVideoBitRate(m_config);
         m_codecContext->gop_size = getVideoFPS(m_config) * 2;
         m_codecContext->max_b_frames = maxBframe;
         m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
         m_codecContext->codec_type = AVMEDIA_TYPE_VIDEO;
         m_codecContext->codec_id = AV_CODEC_ID_H264;
         m_codecContext->qmin = minQuantizer;
         m_codecContext->qmax = maxQuantizer;
         m_codecContext->thread_count = threadCounts;
         m_codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
         av_dict_set(&m_dictionary, "preset", "slow", 0);
         av_dict_set(&m_dictionary, "tune", "zerolatency", 0);
         LOG_DEBUG_MSG("Video bit rate {} bps.", m_codecContext->bit_rate);

        // Initialize the AVCodecContext to use the given AVCodec.
        // need libx264
        int ret = avcodec_open2(m_codecContext, m_codec, NULL);
        if (ret < 0)
        {
            LOG_ERROR_MSG("encoder open failed {}.", ret);
            return false;
        }
        return true;
    }

    bool EncodeCameraStream::prepareOutputContext(const std::string& outputFile)
    {
        if (not createEncoder())
        {
            return false;
        }
        if (not initFilter())
        {
            destroyEncoder();
            return false;
        }
        // Allocate an AVFormatContext for an output format.
        // outputFile should use .mp4 .flv etc.
        int ret = avformat_alloc_output_context2(&m_formatContext, NULL, NULL, outputFile.c_str());
        if (ret < 0)
        {
            LOG_ERROR_MSG("alloc output context failed {}", ret);
            destroyEncoder();
            return false;
        }
        // Add a new stream to a media file.
        m_stream = avformat_new_stream(m_formatContext, NULL);
        if (nullptr == m_stream)
        {
            LOG_ERROR_MSG("create format stream failed.");
            destroyEncoder();
            return false;
        }
//        m_stream->id = 0;
//        m_stream->codecpar->codec_tag = 0;
        //  Copy the contents of src to dst.
        ret = avcodec_parameters_from_context(m_stream->codecpar, m_codecContext);
        if (ret < 0)
        {
            LOG_ERROR_MSG("set parameters from context failed {}", ret);
            destroyEncoder();
            return false;
        }
        // Print detailed information about the input or output format
        av_dump_format(m_formatContext, 0, outputFile.c_str(), 1);

        // Allocate an AVFrame and set its fields to default values.
        m_yuv = av_frame_alloc();
        if (nullptr == m_yuv)
        {
            LOG_ERROR_MSG("Alloc yuv frame failed.");
            destroyEncoder();
            return false;
        }

        m_yuv->format = AV_PIX_FMT_YUV420P;
        m_yuv->width = videoWidth;
        m_yuv->height = videoHeight;
        // Allocate new buffer(s) for audio or video data.
        ret = av_frame_get_buffer(m_yuv, alignment);
        if (ret < 0)
        {
            LOG_ERROR_MSG("get yuv buffer failed {}", ret);
            destroyEncoder();
            return false;
        }
        // Create and initialize a AVIOContext for accessing the resource indicated by url.
        ret = avio_open(&m_formatContext->pb, outputFile.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            LOG_ERROR_MSG("avio open failed {}", ret);
            destroyEncoder();
            return false;
        }
        // Allocate the stream private data and write the stream header to an output media file.
        ret = avformat_write_header(m_formatContext, NULL);
        if (ret < 0)
        {
            LOG_ERROR_MSG("write header failed {}", ret);
            destroyEncoder();
            return false;
        }

        return true;
    }

    void EncodeCameraStream::runWriteFile()
    {
        keep_running = true;
        int pts = 0;
        rgbBuffer.clear();
        rgbBuffer.resize(videoWidth * videoHeight * RGBCountSize);

        SwsContext* swsContext{ nullptr };
        swsContext  = sws_getCachedContext(swsContext,
            videoWidth, videoHeight, AV_PIX_FMT_RGB24,
            videoWidth, videoHeight, AV_PIX_FMT_YUV420P,
            SWS_BICUBIC, NULL, NULL, NULL);
        if (not swsContext)
        {
            LOG_ERROR_MSG("create sws context failed.");
            keep_running = false;
        }

        AVFrame* wateMarkFrame = av_frame_alloc();
        AVFrame* filterFrame = av_frame_alloc();
        while (keep_running)
        {
            /* read data */
            int64_t data_size = videoWidth * videoHeight * RGBCountSize;
            size_t readDataSize = 0;
            int index = 0;

            while (data_size)
            {
                if (index >= rgbBuffer.size())
                {
                    break;
                }

                if (data_size > PIPE_BUF)
                {
                    readDataSize = fread(&rgbBuffer[index], 1, PIPE_BUF, m_fd);
                    if (readDataSize < PIPE_BUF)
                    {
                        LOG_ERROR_MSG("Error read the data {}, should data {}.", readDataSize, PIPE_BUF);
                    }
                }
                else
                {
                    readDataSize = fread(&rgbBuffer[index], 1, data_size, m_fd);
                    if (readDataSize < data_size)
                    {
                        LOG_ERROR_MSG("Error writing the data {}, should data {}.", readDataSize, data_size);
                    }
                }
                index += readDataSize;
                data_size -= readDataSize;
            }
            //LOG_DEBUG_MSG("Success reading the data {}", index);

            uint8_t* indata[AV_NUM_DATA_POINTERS] = { 0 };
            indata[0] = &rgbBuffer[0];

            int inlinesize[AV_NUM_DATA_POINTERS] = { 0 };
            inlinesize[0] = videoWidth * RGBCountSize;

            int outputHeight = sws_scale(swsContext,
                indata, inlinesize,
                0, videoHeight,
                m_yuv->data, m_yuv->linesize);
            if (outputHeight <= 0)
            {
                LOG_ERROR_MSG("Scale change failed.");
                continue;
            }
            /* 
            PTS (Presentation Timestamps)
            This displays a timestamp that tells the player when to display the frame's data.
            */
            m_yuv->pts = pts;
            //pts = pts + 3600;

            // encode frame
            std::vector<uint8_t> copyData;
            copyData.resize(videoWidth * videoHeight);
            memcpy(&copyData[0], m_yuv->data[0], videoWidth * videoHeight);
            addWatermarke(copyData);

            av_frame_ref(wateMarkFrame, m_yuv);
            memcpy(wateMarkFrame->data[0], &copyData[0], videoWidth * videoHeight);
            wateMarkFrame->pts = pts;
            pts = pts + 3600; // assessed value to do get actual value
            //wateMarkFrame->pts = av_frame_get_best_effort_timestamp(m_yuv);

            /* push the decoded frame into the filter graph */
            if (av_buffersrc_add_frame_flags(m_filterSrcContext, wateMarkFrame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
            {
                LOG_ERROR_MSG("Error while feeding the filter graph.");
                av_frame_free(&filterFrame);
                av_frame_free(&wateMarkFrame);
                break;
            }

            AVPacket pkt;
            // Initialize optional fields of a packet with default values.
            av_init_packet(&pkt);

            // pull filtered frames from the filter graph
            while (true)
            {
                int ret = av_buffersink_get_frame(m_filterSinkContext, filterFrame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF )
                {
                    LOG_DEBUG_MSG("Get sink frame the filter graph EOF.");
                    break;
                }
                else if (ret < 0)
                {
                    LOG_ERROR_MSG("Get sink frame the filter graph failed {}.", ret);
                    break;
                }

                //  Supply a raw video or audio frame to the encoder. Use avcodec_receive_packet() to retrieve buffered output packets.
                ret = avcodec_send_frame(m_codecContext, filterFrame);
                if (ret != 0)
                {
                    continue;
                }
                // Read encoded data from the encoder.
                ret = avcodec_receive_packet(m_codecContext, &pkt);
                if (ret != 0)
                {
                    continue;
                }
                //pts += pkt.size;
                // Write a packet to an output media file ensuring correct interleaving.
                av_interleaved_write_frame(m_formatContext, &pkt);
            }
        }

        flushEncoder();
        // Write the stream trailer to an output media file and free the file private data.
        if (m_formatContext)
        {
            av_write_trailer(m_formatContext);
        }

        av_frame_free(&filterFrame);
        av_frame_free(&wateMarkFrame);
        destroyEncoder();

        // clear sws context
        if (swsContext)
        {
            sws_freeContext(swsContext);
        }
    }

    void EncodeCameraStream::flushEncoder()
    {
        /*    Encoder or decoder requires flushing with NULL input at the end in order to
        *     give the complete and correct output.
        */
        if (not (m_codecContext->codec->capabilities & AV_CODEC_CAP_DELAY) )
        {
            return;
        }
        while (true) 
        {
            LOG_DEBUG_MSG("Flushing stream index {}, id {} encoder.", m_stream->index, m_stream->id);
            /*  It can be NULL, in which case it is considered a flush packet.
            *   This signals the end of the stream.
            *   If the encoder still has packets buffered, it will return them after this call.
            *   Once flushing mode has been entered, additional flush
            *   packets are ignored, and sending frames will return AVERROR_EOF.
            */
            int ret = avcodec_send_frame(m_codecContext, nullptr);

            AVPacket pkt;
            // Initialize optional fields of a packet with default values.
            av_init_packet(&pkt);

            ret = avcodec_receive_packet(m_codecContext, &pkt);
            if (ret < 0)
            {
                break;
            }

            ret = av_interleaved_write_frame(m_formatContext, &pkt);
            if (ret < 0)
            {
                break;
            }
        }
        return;
    }

    void EncodeCameraStream::destroyEncoder()
    {
        // Close the resource accessed by the AVIOContext and free it.
        if (m_formatContext && m_formatContext->pb)
        {
            avio_close(m_formatContext->pb);
        }

        // Free an AVFormatContext and all its streams.
        if (m_formatContext)
        {
            avformat_free_context(m_formatContext);
            m_formatContext = nullptr;
        }

        if (m_codecContext)
        {
            // Close a given AVCodecContext and free all the data associated with it (but not the AVCodecContext itself).
            avcodec_close(m_codecContext);

            //  Free the codec context and everything associated with it and write NULL to the provided pointer.
            avcodec_free_context(&m_codecContext);
            m_codecContext = nullptr;
        }
        
        if (m_yuv)
        {
            av_frame_free(&m_yuv);
            m_yuv = nullptr;
        }

        if (m_filterSrcContext)
        {
            avfilter_free(m_filterSrcContext);
            m_filterSrcContext = nullptr;
        }

        if (m_filterSinkContext)
        {
            avfilter_free(m_filterSinkContext);
            m_filterSinkContext = nullptr;
        }

        if (m_filterGraph)
        {
            avfilter_graph_free(&m_filterGraph);
            m_filterGraph = nullptr;
        }
    }

    void EncodeCameraStream::stopWriteFile()
    {
        keep_running = false;
    }

} // 