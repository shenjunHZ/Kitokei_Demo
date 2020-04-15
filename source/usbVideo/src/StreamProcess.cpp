#include "usbVideo/StreamProcess.hpp"
#include "usbVideo/EncodeCameraStream.hpp"
#include "common/CommonFunction.hpp"

namespace usbVideo
{
    StreamProcess::StreamProcess(Logger& logger, const configuration::AppConfiguration& config)
        : m_logger{logger}
        , isStreamBusy{false}
    {
        m_EncodeCameraStream = std::make_unique<EncodeCameraStream>(logger, config, [&]()
                                {
                                    isStreamBusy = false;
                                    cv.notify_one();
                                 });
        m_inputVideoFile = common::getCaptureOutputDir(config) + video::getPipeFileName(config);
        m_inputAudioFile = common::getCaptureOutputDir(config) + "audioPipe" ;
    }

    bool StreamProcess::initRegister(const configuration::bestFrameSize& frameSize)
    {
        if (not m_EncodeCameraStream)
        {
            return false;
        }

        return m_EncodeCameraStream->initRegister(m_inputVideoFile, frameSize, m_inputAudioFile);
    }

    void StreamProcess::startEncodeStream(const std::string& outputFile)
    {
        LOG_DEBUG_MSG("Start init video stream codec context.");
        std::unique_lock<std::mutex> locker(mutexStream);
        cv.wait(locker, [this]() { return !isStreamBusy; });

        //isStreamBusy = true;
        if (not m_EncodeCameraStream->initCodecContext(outputFile))
        {
            isStreamBusy = false;
            LOG_ERROR_MSG("Init codec context failed.");
            return;
        }
        LOG_DEBUG_MSG("Start write video stream to files.");
        m_EncodeCameraStream->runWriteFile();
    }

    void StreamProcess::stopEncodeStream()
    {
        m_EncodeCameraStream->stopWriteFile();
    }

} // namespace Video