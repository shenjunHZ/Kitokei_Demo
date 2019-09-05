#pragma once

#include <boost/program_options.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/exception/diagnostic_information.hpp>

namespace configuration
{
    using AppConfiguration = boost::program_options::variables_map;

    void parseProgramOptions(int argc, char** argv, AppConfiguration& cmdParams);

    AppConfiguration loadFromIniFile(const std::string& configFilename);
}