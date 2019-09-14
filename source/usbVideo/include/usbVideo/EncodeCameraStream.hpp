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
        bool initRegister(const std::string& inputFile) override;
        bool prepareOutputContext(const std::string& outputFile) override;

        void runWriteFile() override;
        void stopWriteFile() override;

    private:
        // create encoder
        bool createEncoder() override;
        // open encoder
        bool openEncoder() override;
        // close encoder
        void closeEncoder() override;

    private:
        FILE* m_fd{nullptr};
        AVCodec* m_codec{ nullptr };
        AVCodecContext* m_codecContext{nullptr};
        AVFormatContext* m_formatContext{nullptr};
        AVStream* m_stream{nullptr};
        AVFrame* m_yuv{nullptr};
        std::vector<uint8_t> rgbBuffer;
        int videoWidth;
        int videoHeight;

        Logger& m_logger;
        const configuration::AppConfiguration& m_config;
    };

} // namespace Video