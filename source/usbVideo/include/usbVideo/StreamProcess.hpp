#pragma once
#include <mutex>
#include <condition_variable>
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
        bool initRegister(const configuration::bestFrameSize& frameSize);
        void startEncodeStream(const std::string& outputFile);
        void stopEncodeStream();

        ~StreamProcess() = default;

    private:
        Logger& m_logger;
        bool isStreamBusy;
        std::unique_ptr<IEncodeCameraStream> m_EncodeCameraStream;

        std::string m_inputVideoFile{};
        std::string m_inputAudioFile{};

        std::mutex mutexStream;
        std::condition_variable cv;
    };
} // namespace Video