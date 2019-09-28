#pragma once
#include "IEncodeCameraStream.hpp"
#include "logger/Logger.hpp"
#include "Configurations/ParseConfigFile.hpp"

extern "C"
{
#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}

namespace usbVideo
{
    class EncodeCameraStream final : public IEncodeCameraStream
    {
    public:
        EncodeCameraStream(Logger& logger, const configuration::AppConfiguration& config);
        ~EncodeCameraStream();
        bool initRegister(const std::string& inputFile, const configuration::bestFrameSize& frameSize) override;
        bool prepareOutputContext(const std::string& outputFile) override;

        void runWriteFile() override;
        void stopWriteFile() override;

    private:
        void flushEncoder();
        // create encoder
        bool createEncoder() override;
        // destroy encoder
        void destroyEncoder() override;
        bool initFilter();
        void initWatemake();
        void addWatermarke(std::vector<uint8_t>& dataY);

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
        AVStream*        m_stream{ nullptr };
        AVFrame*         m_yuv{ nullptr };

        AVFilterGraph*   m_filterGraph{ nullptr };
        AVFilterContext* m_filterSrcContext{ nullptr };
        AVFilterContext* m_filterSinkContext{ nullptr };

        FILE* m_fd{nullptr};
        std::vector<uint8_t> rgbBuffer;
        std::vector<uint8_t> m_numArray;
    };

} // namespace Video