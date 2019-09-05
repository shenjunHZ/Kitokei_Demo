#include <string>
#include <boost/program_options.hpp>
#include "Applications/AppInstance.hpp"
#include "Configurations/Configurations.hpp"
#include "Configurations/ParseConfigFile.hpp"
#include "logger/Logger.hpp"

std::string getLogFilePath(const configuration::AppConfiguration& config)
{
    if (config.find(configuration::logFilePath) != config.end())
    {
        return config[configuration::logFilePath].as<std::string>();
    }
    return "";
}

void runApp(const configuration::AppConfiguration& configParams)
{
    std::string filePath = getLogFilePath(configParams);
    auto& logger = logger::getLogger(filePath);
    LOG_INFO_MSG(logger, "Start to run app");

    application::AppInstance appInstance(logger, configParams);

    appInstance.loopFuction();
}

int main(int argc, char* argv[])
{
    boost::program_options::variables_map cmdParams;
    std::string config = "";
    if (argc > 2)
    {
        configuration::parseProgramOptions(argc, argv, cmdParams);
        config = cmdParams["config"].as<std::string>();
    }
    else
    {
        config = "./configuration.ini";
    }

    try
    {
        configuration::AppConfiguration configParams = configuration::loadFromIniFile(config);
        runApp(configParams);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR_MSG(boost::diagnostic_information(e));
        std::string t = e.what();
        std::cerr << "parse config file failed: " << e.what();

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}