#pragma once

#include <stdint.h>
#include <string>

namespace Video
{
    int convertYuvToRgbPixel(int y, int u, int v);

    int convertYuvToRgbBuffer(uint8_t* yuv, uint8_t* rgb, const int& width, const int& height);

    int makeCaptureBMP(uint8_t *rgb_buffer,
        const std::string& file_name,
        const uint32_t& width,
        const uint32_t& height);

    int makeCaptureRGB(uint8_t *rgb_buffer,
        const std::string& file_name,
        const uint32_t& width,
        const uint32_t& height);
} // namespace Video