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
        EncodeCameraStream(Logger& logger, const configuration::AppConfiguration& config, const CloseVideoNotify& notify);
        ~EncodeCameraStream();
        bool initRegister(const std::string& inputVideoFile, const configuration::bestFrameSize& frameSize, 
            const std::string& inputAudioFile) override;
        bool initCodecContext(const std::string& outputFile) override;

        void runWriteFile() override;
        void stopWriteFile() override;

    private:
        void flushEncoder();
        // create encoder
        bool createEncoder() override;
        bool createVideoEncoder();
        bool createAudioEncoder();
        // init encoder
        bool initVideoCodecContext(const std::string& outputFile);
        bool initAudioCodecContext(const std::string& outputFile);
        void prepareFrame();
        void writeVideoFrame();
        void writeAudioFrame();
        // destroy encoder
        void destroyEncoder() override;
        bool initFilter();
        void initWatemake();
        void addWatermarke(std::vector<uint8_t>& dataY);

        void closeFile();

    private:
        Logger& m_logger;
        const configuration::AppConfiguration& m_config;
        int videoWidth;
        int videoHeight;
        // Format I/O context.
        AVCodecContext*  m_codecContext{ nullptr };
        AVCodecContext*  m_codecAudioContext{ nullptr };
        AVDictionary*    m_dictionary{nullptr};
        AVFormatContext* m_formatContext{ nullptr };
        AVStream*        m_stream{ nullptr };

        AVFrame*         m_yuv{ nullptr };
        AVFrame*         m_acc{ nullptr };

        AVFilterGraph*   m_filterGraph{ nullptr };
        AVFilterContext* m_filterSrcContext{ nullptr };
        AVFilterContext* m_filterSinkContext{ nullptr };

        SwsContext* swsContext_{ nullptr };
        AVFrame* wateMarkFrame{ nullptr };
        AVFrame* filterFrame{nullptr};

        int pts{0};
        int audioPts{ 0 };

        FILE* m_fd{nullptr};
        FILE* m_audioFd{nullptr};
        int fdAudio{-1};
        std::vector<uint8_t> rgbBuffer;
        std::vector<uint8_t> m_numArray;
        std::vector<std::uint8_t> audioBuffer;
        const CloseVideoNotify& closeVideoNotify;
    };

} // namespace Video