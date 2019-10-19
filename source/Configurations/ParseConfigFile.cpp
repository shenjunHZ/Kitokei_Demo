#include <fstream>
#include <iostream>
#include "ParseConfigFile.hpp"

namespace
{
    boost::program_options::options_description createProgramOptionsDescription()
    {
        auto description = boost::program_options::options_description();
        using boost::program_options::value;

        description.add_options()
            (configuration::logFilePath, value<std::string>()->default_value("./log/logFile.log"), "use for recored logs")
            (configuration::cameraDevice,       value<std::string>()->default_value("/dev/video0"),         "camera device")
            (configuration::enableCameraStream, value<bool>()->default_value(true),                         "enable camera stream capture.")
            (configuration::pipeFileName,       value<std::string>()->default_value("cameraCapturePipe"),   "camera capture pipe file name.")
            (configuration::captureOutputDir,   value<std::string>()->default_value("/tmp/cameraCapture/"), "camera capture file path.")
            (configuration::videoName,          value<std::string>()->default_value("chessVideo"),          "camera video file name.")
            (configuration::videoFPS,           value<int>()->default_value(25),                            "frame rate.")
            (configuration::videoBitRate,       value<int>()->default_value(400000),                        "video bit rate.")
            (configuration::videoTimes,         value<int>()->default_value(30),                            "each file times.")
            (configuration::V4l2RequestBuffersCounter, value<int>()->default_value(2),             "request mmap buffer counters.")
            (configuration::V4L2CaptureFormat,         value<std::string>()->default_value("BMP"), "capture format set.")
            (configuration::captureWidth,              value<int>()->default_value(640),           "capture and video format width.")
            (configuration::captureHeight,             value<int>()->default_value(480),           "capture and video format height.")
            (configuration::chessBoardServerAddress, value<std::string>()->default_value("192.168.2.140"), "chess board server ip address.")
            (configuration::chessBoardServerPort,    value<unsigned int>()->default_value(8080),           "chess board server ip port.")
            (configuration::kitokeiLocalAddress,     value<std::string>()->default_value("127.0.0.1"),     "kitokei local ip address.")
            (configuration::kitokeiLocalPort,        value<unsigned int>()->default_value(8080),           "kitokei local ip port.")
            (configuration::audioName, value<std::string>()->default_value("chessName"), "audio file name.")
            (configuration::enableWriteAudioToFile, value<bool>()->default_value(true), "open write audio file.")
            (configuration::audioDevice,            value<std::string>()->default_value("default"), "open audio device.")
            (configuration::audioChannel,           value<int>()->default_value(1), "audio channels.")
            (configuration::sampleRate,             value<int>()->default_value(44100), "audio sample rate.")
            (configuration::playbackDevice,         value<std::string>()->default_value("default"), "playback audio device.")
            (configuration::readTestAudioFile,      value<std::string>()->default_value(""), "playback audio file");

        return description;
    }

    configuration::AppConfiguration parseProgramOptionsFile(std::ifstream& boostOptionsStream)
    {
        namespace po = boost::program_options;

        po::variables_map appConfig;
        po::store(po::parse_config_file(boostOptionsStream, createProgramOptionsDescription()), appConfig);
        try
        {
            po::notify(appConfig);
        }
        catch (const po::error& e)
        {
            std::cerr << "Parsing the config file failed: " << e.what() << std::endl;
            throw;
        }
        return appConfig;
    }
} // namespace
namespace configuration
{
    void parseProgramOptions(int argc, char** argv, AppConfiguration& cmdParams)
    {
        namespace po = boost::program_options;
        {
            po::options_description optDescr("General");
            try
            {
                auto defaultConfigFilePath = po::value<std::string>()->default_value("configuration.ini");
                optDescr.add_options()("config,c", defaultConfigFilePath, "Configuration file path")(
                    "help,h", "Prints this help message");
            }
            catch (const boost::bad_lexical_cast& e)
            {
                std::cerr << "parseProgramOptions:lexical_cast Failed in  default_value." << std::endl;
                std::cerr << e.what() << std::endl;
            }

            try
            {
                po::store(po::parse_command_line(argc, argv, optDescr), cmdParams);
                po::notify(cmdParams);
            }
            catch (const po::error& e)
            {
                std::cerr << e.what() << std::endl;
                std::exit(EXIT_FAILURE);
            }

            if (cmdParams.count("help") != 0u)
            {
                std::cout << optDescr << std::endl;
                std::exit(EXIT_SUCCESS);
            }
        } // namespce po = boost::program_options
    }

    AppConfiguration loadFromIniFile(const std::string& configFilename)
    {
        std::ifstream configFileStream{ configFilename };
        AppConfiguration configuration{ parseProgramOptionsFile(configFileStream) };

        return configuration;
    }

    configuration::AppAddresses getAppAddresses(const boost::program_options::variables_map& cmdParams)
    {
        return AppAddresses{ cmdParams[chessBoardServerAddress].as<std::string>(),
                             cmdParams[chessBoardServerPort].as<unsigned int>(),
                             cmdParams[kitokeiLocalAddress].as<std::string>(),
                             cmdParams[kitokeiLocalPort].as<unsigned int>() };
    }

}