#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include "CommonFunction.hpp"

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

} // namespace common