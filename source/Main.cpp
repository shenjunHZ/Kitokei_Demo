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

void runApp(const configuration::AppConfiguration& configParams, const configuration::AppAddresses& address)
{
    std::string filePath = getLogFilePath(configParams);
    auto& logger = logger::getLogger(filePath);
    LOG_INFO_MSG(logger, "Start to run Kitokei app.");

    try 
    {
        application::AppInstance appInstance(logger, configParams, address);

        appInstance.loopFuction();
    }
    catch (const std::exception& e)
    {
        LOG_ERROR_MSG("Kitokei app run failed {}", e.what());
    }
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
        const auto addresses = configuration::getAppAddresses(configParams);
        runApp(configParams, addresses);
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