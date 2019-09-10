#pragma once
/*
* use ffmpeg framework get camera stream files
*/
#include <libavformat/avformat.h>

namespace usbVideo
{
    class IDecodeCameraStream
    {
    public:
        virtual ~IDecodeCameraStream() = default;
        // Register all formats.Including unpacking format and packaging format
        virtual bool registerAllHandler() = 0;
        // Open a file and parse it.Contents that can be parsed include: 
        // video stream, audio stream, video stream parameters, audio stream parameters, video frame index.
        virtual bool openInputFile(AVFormatContext&, std::string& fileName) = 0;
        // Find formats and indexes.
        virtual bool findStreamInfo(AVFormatContext&) = 0;
        /*
         * Print detailed information about the input or output format, such as
?        * duration, bitrate, streams, container, programs, metadata, side data,
?        * codec and time base.
?        *
?        * @param ic ? ? ? ?the context to analyze
?        * @param index ? ? index of the stream to dump information about
?        * @param url ? ? ? the URL to print, such as source or destination file
?        * @param is_output Select whether the specified context is an input(0) or output(1)
         *
         * What this function does: av_dump_format() is a hand-debuting function that lets you see 
         * what's in pformatctx-> streams.Generally next we use the av_find_stream_info() function, 
         * which fills in the correct information for pformatctx-> streams.
         */
        virtual bool avDumpFormat() = 0;
        // get decoder
        virtual bool avCodecFindDecoder(enum AVCodecID&) = 0;
        virtual bool avCodecOpen(AVCodecContext&, AVCodec&) = 0;
    };
} // namespace usbVideo