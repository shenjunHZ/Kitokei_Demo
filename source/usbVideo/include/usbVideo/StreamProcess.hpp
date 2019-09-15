#pragma once
#include "Configurations/Configurations.hpp"
#include "Configurations/ParseConfigFile.hpp"
#include "logger/Logger.hpp"
#include "IEncodeCameraStream.hpp"

namespace usbVideo
{
    class StreamProcess final
    {
    public:
        StreamProcess(Logger& logger, const configuration::AppConfiguration& config);
        void startEncodeStream(const std::string& outputFile);
        void stopEncodeStream();

        ~StreamProcess() = default;

    private:
        bool initRegister();

    private:
        Logger& m_logger;
        std::unique_ptr<IEncodeCameraStream> m_EncodeCameraStream;

        std::string m_inputFile{};
    };
} // namespace Video