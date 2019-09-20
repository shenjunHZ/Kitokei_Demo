#pragma once
#include "IEncodeCameraStream.hpp"
#include "logger/Logger.hpp"
#include "Configurations/ParseConfigFile.hpp"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

namespace usbVideo
{
    class EncodeCameraStream final : public IEncodeCameraStream
    {
    public:
        EncodeCameraStream(Logger& logger, const configuration::AppConfiguration& config);
        ~EncodeCameraStream();
        bool initRegister(const std::string& inputFile) override;
        bool prepareOutputContext(const std::string& outputFile) override;

        void runWriteFile() override;
        void stopWriteFile() override;

    private:
        // create encoder
        bool createEncoder() override;
        // destroy encoder
        void destroyEncoder() override;

    private:
        Logger& m_logger;
        const configuration::AppConfiguration& m_config;
        int videoWidth;
        int videoHeight;
        // Format I/O context.
        AVCodecContext*  m_codecContext{ nullptr };
        AVCodec*         m_codec{ nullptr };
        AVDictionary*    m_dictionary{nullptr};
        AVFormatContext* m_formatContext{ nullptr };
        AVStream* m_stream{ nullptr };
        AVFrame* m_yuv{ nullptr };

        FILE* m_fd{nullptr};
        std::vector<uint8_t> rgbBuffer;
    };

} // namespace Video