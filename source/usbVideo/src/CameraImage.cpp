#include "usbVideo/CameraImage.hpp"
#include "Configurations/Configurations.hpp"
#include "logger/Logger.hpp"

namespace
{
    constexpr auto BMP_MAGIC_0 = 0x42;
    constexpr auto BMP_MAGIC_1 = 0x4D;
    constexpr auto BMP_CREATOR1 = 0xADDE;
    constexpr auto BMP_CREATOR2 = 0xEFBE;
    constexpr auto BMP_DEFAULT_HRES = 2835;
    constexpr auto BMP_DEFAULT_VRES = 2835;

template<typename T>
void swap(T& a, T& b)   
{ 
    T tmp = a;
    a = b;
    b = tmp;
}
} // namespace

namespace usbVideo
{
    /* Copyright 2007 (c) Logitech. All Rights Reserved. (yuv -> rgb conversion) */
    int convertYuvToRgbPixel(int y, int u, int v)
    {
        unsigned int pixel32 = 0;
        unsigned char *pixel = (unsigned char *)&pixel32;
        int r, g, b;

        r = y + (1.370705 * (v - 128));
        g = y - (0.698001 * (v - 128)) - (0.337633 * (u - 128));
        b = y + (1.732446 * (u - 128));

        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        if (r < 0) r = 0;
        if (g < 0) g = 0;
        if (b < 0) b = 0;

        pixel[0] = r * 220 / 256;
        pixel[1] = g * 220 / 256;
        pixel[2] = b * 220 / 256;

        return pixel32;
    }

    int convertYuvToRgbBuffer(uint8_t* yuv, uint8_t* rgb, const int& width, const int& height)
    {
        unsigned int in, out = 0;
        unsigned int pixel_16;
        unsigned char pixel_24[3];
        unsigned int pixel32;
        int y0, u, y1, v;

        for (in = 0; in < width * height * 2; in += 4) 
        {
            pixel_16 =
                yuv[in + 3] << 24 |
                yuv[in + 2] << 16 |
                yuv[in + 1] << 8 |
                yuv[in + 0];

            y0 = (pixel_16 & 0x000000ff);
            u = (pixel_16 & 0x0000ff00) >> 8;
            y1 = (pixel_16 & 0x00ff0000) >> 16;
            v = (pixel_16 & 0xff000000) >> 24;

            pixel32 = convertYuvToRgbPixel(y0, u, v);
            pixel_24[0] = (pixel32 & 0x000000ff);
            pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
            pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;

            rgb[out++] = pixel_24[0];
            rgb[out++] = pixel_24[1];
            rgb[out++] = pixel_24[2];

            pixel32 = convertYuvToRgbPixel(y1, u, v);
            pixel_24[0] = (pixel32 & 0x000000ff);
            pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
            pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;

            rgb[out++] = pixel_24[0];
            rgb[out++] = pixel_24[1];
            rgb[out++] = pixel_24[2];
        }

        return 0;
    }

    int makeCaptureBMP(uint8_t *rgb_buffer,
        const std::string& file_name,
        const uint32_t& width,
        const uint32_t& height)
    {
        FILE *fd;
        size_t data_size;
        configuration::BMPfileMagic bmpfile_magic;
        configuration::BMPfileHeader bmpfile_header;
        configuration::BMPdibV3Heade bmpdib_header;
        uint32_t i, j;

        /* create the file first */
        fd = fopen(file_name.c_str(), "wb");

        if (fd == nullptr)
        {
            LOG_ERROR_MSG("make bmp capture failed.");
            return -1;
        }

        data_size = width * height * 3;

        /* fill in the magic */
        bmpfile_magic.magic[0] = BMP_MAGIC_0;
        bmpfile_magic.magic[1] = BMP_MAGIC_1;

        /* fill in the header */
        bmpfile_header.filesz = sizeof(configuration::BMPfileMagic) +
            sizeof(configuration::BMPfileHeader) +
            sizeof(configuration::BMPdibV3Heade) +
            width * height * 3;
        bmpfile_header.creator1 = BMP_CREATOR1;
        bmpfile_header.creator2 = BMP_CREATOR2;
        bmpfile_header.bmp_offset = sizeof(configuration::BMPfileMagic) +
            sizeof(configuration::BMPfileHeader) +
            sizeof(configuration::BMPdibV3Heade);

        /* fill in the dib V3 header */
        bmpdib_header.header_sz = sizeof(configuration::BMPdibV3Heade);
        bmpdib_header.width = width;
        bmpdib_header.height = height;
        bmpdib_header.nplanes = 1;
        bmpdib_header.bitspp = 24;
        bmpdib_header.compress_type = 0;
        bmpdib_header.bmp_bytesz = data_size;
        bmpdib_header.hres = BMP_DEFAULT_HRES;
        bmpdib_header.vres = BMP_DEFAULT_VRES;
        bmpdib_header.ncolors = 0;
        bmpdib_header.nimpcolors = 0;

        /* this is what is happening here:
         * i) lines are swapped - bottom with top, bottom-1 with top+1, etc...
         * ii) the RED and BLUE bytes are swapped for each pixes
         * iii) all this due to wierd bmp specs... */
        for (i = 0; i < width; i++)
        {
            uint32_t curpix = i * 3;
            for (j = 0; j < height / 2; j++)
            {
                /* swap R for B */
                swap(rgb_buffer[j*(width * 3) + curpix],
                    rgb_buffer[(height - j - 1)*(width * 3) + curpix + 2]);
                /* leave G */
                swap(rgb_buffer[j*(width * 3) + curpix + 1],
                    rgb_buffer[(height - j - 1)*(width * 3) + curpix + 1]);
                /* swap B for R */
                swap(rgb_buffer[j*(width * 3) + curpix + 2],
                    rgb_buffer[(height - j - 1)*(width * 3) + curpix]);
            }
        }

        if (fwrite(&bmpfile_magic, sizeof(configuration::BMPfileMagic), 1, fd) < 1)
        {
            LOG_ERROR_MSG("Error writing to the file.");
            fclose(fd);
            return -1;
        }

        if (fwrite(&bmpfile_header, sizeof(configuration::BMPfileHeader), 1, fd) < 1)
        {
            LOG_ERROR_MSG("Error writing to the file.");
            fclose(fd);
            return -1;
        }

        if (fwrite(&bmpdib_header, sizeof(configuration::BMPdibV3Heade), 1, fd) < 1)
        {
            LOG_ERROR_MSG("Error writing to the file.");
            fclose(fd);
            return -1;
        }

        if (fwrite(rgb_buffer, 1, data_size, fd) < data_size)
        {
            LOG_ERROR_MSG("Error writing to the file.");
            fclose(fd);
            return -1;
        }

        fclose(fd);
        return 0;
    }

    int makeCaptureRGB(uint8_t *rgb_buffer,
        const std::string& fileName,
        const uint32_t& width,
        const uint32_t& height)
    {
        FILE *fd;
        size_t data_size;

        /* create the file first */
        fd = fopen(fileName.c_str(), "wb");

        if (fd == nullptr)
        {
            LOG_ERROR_MSG("make capture rgb fail.");
            return -1;
        }

        data_size = width * height * 3;

        if (fwrite(rgb_buffer, 1, data_size, fd) < data_size)
        {
            LOG_ERROR_MSG("Error writing the {} file.", fileName.c_str());
            return -1;
        }

        fclose(fd);
        return 0;
    }
}