#pragma once
#include <string>
#include "Configurations/ParseConfigFile.hpp"

namespace common
{
    bool isFileExistent(const std::string& fileName);

    std::string getCaptureOutputDir(const configuration::AppConfiguration& config);

    std::string getPipeFileName(const configuration::AppConfiguration& config);

    int getCaptureWidth(const configuration::AppConfiguration& config);

    int getCaptureHeight(const configuration::AppConfiguration& config);

    int getVideoBitRate(const configuration::AppConfiguration& config);

    std::string getVideoName(const configuration::AppConfiguration& config);

    bool enableAudioWriteToFile(const configuration::AppConfiguration& config);

    std::string getAudioName(const configuration::AppConfiguration& config);

} // namespace common

namespace audio
{
    std::string getAudioDevice(const configuration::AppConfiguration& config);

    int getAudioChannel(const configuration::AppConfiguration& config);

    int getAudioSampleRate(const configuration::AppConfiguration& config);

    std::string getPlaybackDevice(const configuration::AppConfiguration& config);

    std::string getPlaybackAudioFile(const configuration::AppConfiguration& config);
} // namespace audio