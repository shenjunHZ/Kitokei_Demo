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
} // namespace common