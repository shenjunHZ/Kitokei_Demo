#include "usbVideo/StreamProcess.hpp"
#include "usbVideo/EncodeCameraStream.hpp"
#include "common/CommonFunction.hpp"

namespace usbVideo
{

    StreamProcess::StreamProcess(Logger& logger, const configuration::AppConfiguration& config)
        : m_logger{logger}
        , m_EncodeCameraStream{std::make_unique<EncodeCameraStream>(logger, config)}
    {
        m_inputFile = common::getCaptureOutputDir(config) + common::getPipeFileName(config);
    }

    bool StreamProcess::initRegister(const configuration::bestFrameSize& frameSize)
    {
        if (not m_EncodeCameraStream)
        {
            return false;
        }
        return m_EncodeCameraStream->initRegister(m_inputFile, frameSize);
    }

    void StreamProcess::startEncodeStream(const std::string& outputFile)
    {
        if (not m_EncodeCameraStream->prepareOutputContext(outputFile))
        {
            return;
        }
        LOG_DEBUG_MSG("...Start write file....");
        m_EncodeCameraStream->runWriteFile();
    }

    void StreamProcess::stopEncodeStream()
    {
        m_EncodeCameraStream->stopWriteFile();
    }



} // namespace Video