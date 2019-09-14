#pragma once
#include <boost/program_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include "Configurations/Configurations.hpp"

namespace configuration
{
    using AppConfiguration = boost::program_options::variables_map;

    void parseProgramOptions(int argc, char** argv, AppConfiguration& cmdParams);

    AppConfiguration loadFromIniFile(const std::string& configFilename);
    AppAddresses getAppAddresses(const boost::program_options::variables_map& cmdParams);
}