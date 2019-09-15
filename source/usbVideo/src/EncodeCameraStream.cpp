#include "usbVideo/EncodeCameraStream.hpp"
#include "Configurations/ParseConfigFile.hpp"
#include "common/CommonFunction.hpp"

namespace
{
    int getVideoFPS(const configuration::AppConfiguration& config)
    {
        if (config.find(configuration::videoFPS) != config.end())
        {
            return config[configuration::videoFPS].as<int>();
        }
        return 25;
    }

    constexpr int threadCount = 8;
    constexpr int bitDepth = 256;
    constexpr int RGBCountSize = 4;
    constexpr int alignment = 0;
    std::atomic_bool keep_running{ true };
}// namespace 
namespace usbVideo
{
    EncodeCameraStream::EncodeCameraStream(Logger& logger, const configuration::AppConfiguration& config)
        : m_logger{logger}
        , m_config{config}
    {
        videoWidth = common::getCaptureWidth(config);
        videoHeight = common::getCaptureHeight(config);
    }

    bool EncodeCameraStream::initRegister(const std::string& inputFile)
    {
        m_fd = fopen(inputFile.c_str(), "rb");
        if (nullptr == m_fd)
        {
            LOG_ERROR_MSG("open input file failed {}", inputFile);
            return false;
        }
        // register all, have not been used ?
        av_register_all();
        // register all encode, can be removed as av_register_all
        avcodec_register_all();
        return true;
    }

    bool EncodeCameraStream::createEncoder()
    {
        // Find a registered encoder with a matching codec ID.
        m_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (nullptr == m_codec)
        {
            LOG_ERROR_MSG("find encoder H264 failed.");
            return false;
        }
        // Allocate an AVCodecContext and set its fields to default values.
        m_codecContext = avcodec_alloc_context3(m_codec);
        if (nullptr == m_codecContext)
        {
            LOG_ERROR_MSG("alloc context failed.");
            return false;
        }
        m_codecContext->width = videoWidth;
        m_codecContext->height = videoHeight;
        m_codecContext->time_base = {1, getVideoFPS(m_config)};
        m_codecContext->framerate = {getVideoFPS(m_config), 1};
        m_codecContext->bit_rate = m_codecContext->width * m_codecContext->height * getVideoFPS(m_config) * bitDepth;
        m_codecContext->gop_size = getVideoFPS(m_config);
        m_codecContext->max_b_frames = 0;
        m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
        m_codecContext->codec_id = AV_CODEC_ID_H264;
        m_codecContext->thread_count = threadCount;
        m_codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        return true;
    }

    bool EncodeCameraStream::openEncoder()
    {
        // Initialize the AVCodecContext to use the given AVCodec.
        int ret = avcodec_open2(m_codecContext, m_codec, NULL);
        if (  ret < 0 )
        {
            LOG_ERROR_MSG("encoder open failed {}.", ret);
            return false;
        }
        return true;
    }

    bool EncodeCameraStream::prepareOutputContext(const std::string& outputFile)
    {
        if (not createEncoder())
        {
            return false;
        }
        if (not openEncoder())
        {
            return false;
        }
        // Allocate an AVFormatContext for an output format.
        int ret = avformat_alloc_output_context2(&m_formatContext, NULL, NULL, outputFile.c_str());
        if (ret < 0)
        {
            LOG_ERROR_MSG("alloc output context failed {}", ret);
            closeEncoder();
            return false;
        }
        // Add a new stream to a media file.
        m_stream = avformat_new_stream(m_formatContext, NULL);
        if (nullptr == m_stream)
        {
            LOG_ERROR_MSG("create format stream failed.");
            closeEncoder();
            return false;
        }
        m_stream->id = 0;
        m_stream->codecpar->codec_tag = 0;
        //  Copy the contents of src to dst.
        ret = avcodec_parameters_from_context(m_stream->codecpar, m_codecContext);
        if (ret < 0)
        {
            LOG_ERROR_MSG("set parameters from context failed {}", ret);
            closeEncoder();
            return false;
        }
        // Print detailed information about the input or output format
        av_dump_format(m_formatContext, 0, outputFile.c_str(), 1);

        // Allocate an AVFrame and set its fields to default values.
        m_yuv = av_frame_alloc();
        if (nullptr == m_yuv)
        {
            LOG_ERROR_MSG("Alloc yuv frame failed.");
            closeEncoder();
            return false;
        }
        m_yuv->format = AV_PIX_FMT_YUV420P;
        m_yuv->width = videoWidth;
        m_yuv->height = videoHeight;
        // Allocate new buffer(s) for audio or video data.
        ret = av_frame_get_buffer(m_yuv, alignment);
        if (ret < 0)
        {
            LOG_ERROR_MSG("get yuv buffer failed {}", ret);
            closeEncoder();
            return false;
        }
        // Create and initialize a AVIOContext for accessing the resource indicated by url.
        ret = avio_open(&m_formatContext->pb, outputFile.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            LOG_ERROR_MSG("avio open failed {}", ret);
            closeEncoder();
            return false;
        }
        // Allocate the stream private data and write the stream header to an output media file.
        ret = avformat_write_header(m_formatContext, NULL);
        if (ret < 0)
        {
            LOG_ERROR_MSG("write header failed {}", ret);
            closeEncoder();
            return false;
        }

        return true;
    }

    void EncodeCameraStream::runWriteFile()
    {
        int p = 0;
        rgbBuffer.clear();
        rgbBuffer.resize(videoWidth * videoHeight * RGBCountSize);

        SwsContext* swsContext = sws_getCachedContext(swsContext,
            videoWidth, videoHeight, AV_PIX_FMT_BGRA,
            videoWidth, videoHeight, AV_PIX_FMT_YUV420P, SWS_BICUBIC,
            NULL, NULL, NULL);

        while (keep_running)
        {
            int len = fread(&rgbBuffer[0], 1, videoWidth * videoHeight * RGBCountSize, m_fd);
            if (len <= 0)
            {
                continue;
            }

            uint8_t *indata[AV_NUM_DATA_POINTERS] = { 0 };
            indata[0] = &rgbBuffer[0];
            int inlinesize[AV_NUM_DATA_POINTERS] = { 0 };
            inlinesize[0] = videoWidth * RGBCountSize;

            int h = sws_scale(swsContext, indata, inlinesize, 0, videoHeight, m_yuv->data, m_yuv->linesize);
            if (h <= 0)
            {
                continue;
            }
            //6 encode frame
            m_yuv->pts = p;
            p = p + 3600;
            //  Supply a raw video or audio frame to the encoder. Use avcodec_receive_packet() to retrieve buffered output packets.
            int ret = avcodec_send_frame(m_codecContext, m_yuv);
            if (ret != 0)
            {
                continue;
            }
            AVPacket pkt;
            // Initialize optional fields of a packet with default values.
            av_init_packet(&pkt);
            // Read encoded data from the encoder.
            ret = avcodec_receive_packet(m_codecContext, &pkt);
            if (ret != 0)
            {
                continue;
            }
            // Write a packet to an output media file ensuring correct interleaving.
            av_interleaved_write_frame(m_formatContext, &pkt);
        }

        // Write the stream trailer to an output media file and free the file private data.
        av_write_trailer(m_formatContext);

        closeEncoder();

        // 清理视频重采样上下文
        sws_freeContext(swsContext);
    }

    void EncodeCameraStream::closeEncoder()
    {
        // Close the resource accessed by the AVIOContext s and free it.
        avio_close(m_formatContext->pb);

        // Free an AVFormatContext and all its streams.
        avformat_free_context(m_formatContext);

        // Close a given AVCodecContext and free all the data associated with it (but not the AVCodecContext itself).
        avcodec_close(m_codecContext);

        //  Free the codec context and everything associated with it and write NULL to the provided pointer.
        avcodec_free_context(&m_codecContext);
    }

    void EncodeCameraStream::stopWriteFile()
    {
        keep_running = false;
    }

} // 