#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include "CommonFunction.hpp"
#include "Configurations/Configurations.hpp"

namespace common
{
    bool isFileExistent(const std::string& fileName)
    {
        const boost::filesystem::path filePathName(fileName);
        boost::system::error_code error;
        auto file_status = boost::filesystem::status(filePathName, error);
        if (error) 
        {
            return false;
        }

        if (not boost::filesystem::exists(file_status)) 
        {
            return false;
        }

        if (boost::filesystem::is_directory(file_status)) 
        {
            return false;
        }

        return true;
    }

    std::string getCaptureOutputDir(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::captureOutputDir) != config.end())
        {
            return config[configuration::captureOutputDir].as<std::string>();
        }
        return "/tmp/cameraCapture/";
    }

    std::string getPipeFileName(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::pipeFileName) != config.end())
        {
            return config[configuration::pipeFileName].as<std::string>();
        }
        return "cameraCapturePipe";
    }

    int getCaptureWidth(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::captureWidth) != config.end())
        {
            return config[configuration::captureWidth].as<int>();
        }
        return 640;
    }

    int getCaptureHeight(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::captureHeight) != config.end())
        {
            return config[configuration::captureHeight].as<int>();
        }
        return 480;
    }

    int getVideoBitRate(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::videoBitRate) != config.end())
        {
            return config[configuration::videoBitRate].as<int>();
        }
        return 400 * 1000;
    }

    std::string getVideoName(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::videoName) != config.end())
        {
            return config[configuration::videoName].as<std::string>();
        }
        return "chessVideo";
    }

    bool enableAudioWriteToFile(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::enableWriteAudioToFile) != config.end())
        {
            return config[configuration::enableWriteAudioToFile].as<bool>();
        }
        return true;
    }

    std::string getAudioName(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::audioName) != config.end())
        {
            return config[configuration::audioName].as<std::string>();
        }
        return "chessAudio";
    }

} // namespace common

namespace audio
{
    std::string getAudioDevice(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::audioDevice) != config.end())
        {
            return config[configuration::audioDevice].as<std::string>();
        }
        return "default";
    }

    int getAudioChannel(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::audioChannel) != config.end())
        {
            return config[configuration::audioChannel].as<int>();
        }
        return 1;
    }

    int getAudioSampleRate(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::sampleRate) != config.end())
        {
            return config[configuration::sampleRate].as<int>();
        }
        return 44100;
    }

    std::string getPlaybackDevice(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::playbackDevice) != config.end())
        {
            return config[configuration::playbackDevice].as<std::string>();
        }
        return "default";
    }
} // namespace audio