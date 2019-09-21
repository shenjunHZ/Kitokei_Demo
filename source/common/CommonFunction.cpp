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

} // namespace common