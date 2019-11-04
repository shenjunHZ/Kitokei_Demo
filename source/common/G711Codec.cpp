#include "G711Codec.hpp"
#include "logger/Logger.hpp"

namespace
{
    constexpr int SEG_SHIFT = 4;        /* Left shift for segment number. */
    constexpr int QUANT_MASK = 0xf;	    /* Quantization field mask. */
    constexpr int BIAS = 0x84;          /* Bias for linear code. */
    constexpr int SEG_MASK = 0x70;      /* Segment field mask. */
    constexpr int SIGN_BIT = 0x80;      /* Sign bit for a A-law byte. */
    constexpr int NSEGS = 8;            /* Number of A-law segments. */

    short seg_end[8] = { 0xFF, 0x1FF, 0x3FF, 0x7FF,
            0xFFF, 0x1FFF, 0x3FFF, 0x7FFF };

    int search(short val, short* table, short size)
    {
        for (int i = 0; i < size; i++)
        {
            if (val <= *table++)
                return (i);
        }
        return (size);
    }

    /* Convert a 16-bit linear PCM value to 8-bit A-law */
    unsigned char linear2alaw(short pcm_val)
    {
        short		mask;
        short		seg;
        unsigned char	aval;

        if (pcm_val >= 0)
        {
            mask = 0xD5;		/* sign (7th) bit = 1 */
        }
        else
        {
            mask = 0x55;		/* sign bit = 0 */
            pcm_val = -pcm_val - 8;
        }

        /* Convert the scaled magnitude to segment number. */
        seg = search(pcm_val, seg_end, 8);

        /* Combine the sign, segment, and quantization bits. */

        if (seg >= 8)		/* out of range, return maximum value. */
        {
            return (0x7F ^ mask);
        }
        else
        {
            aval = seg << SEG_SHIFT;
            if (seg < 2)
            {
                aval |= (pcm_val >> 4) & QUANT_MASK;
            }
            else
            {
                aval |= (pcm_val >> (seg + 3)) & QUANT_MASK;
            }
            return (aval ^ mask);
        }
    }

    int g711aEncode(unsigned char* g711Data, const short* pcmData, int len)
    {
        for (int i = 0; i < len; i++)
        {
            g711Data[i] = linear2alaw(pcmData[i]);
        }

        return len;
    }

    /* Convert a 16-bit linear PCM value to 8-bit u-law */
    unsigned char linear2ulaw(short pcm_val)
    {
        int		mask;
        int		seg;
        unsigned char	uval;

        /* Get the sign and the magnitude of the value. */
        if (pcm_val < 0)
        {
            pcm_val = BIAS - pcm_val;
            mask = 0x7F;
        }
        else
        {
            pcm_val += BIAS;
            mask = 0xFF;
        }

        /* Convert the scaled magnitude to segment number. */
        seg = search(pcm_val, seg_end, 8);

        /*
         * Combine the sign, segment, quantization bits;
         * and complement the code word.
         */
        if (seg >= 8)		/* out of range, return maximum value. */
            return (0x7F ^ mask);
        else
        {
            uval = (seg << 4) | ((pcm_val >> (seg + 3)) & 0xF);
            return (uval ^ mask);
        }
    }

    int g711uEncode(unsigned char* g711Data, const short* pcmData, int len)
    {
        for (int i = 0; i < len; i++)
        {
            g711Data[i] = linear2ulaw(pcmData[i]);
        }

        return len;
    }

    /* Convert an A-law value to 16-bit linear PCM */
    int alaw2linear(unsigned char a_val)
    {
        int	t;
        int	seg;

        a_val ^= 0x55;

        t = (a_val & QUANT_MASK) << 4;
        seg = ((unsigned)a_val & SEG_MASK) >> SEG_SHIFT;
        switch (seg)
        {
        case 0:
            t += 8;
            break;
        case 1:
            t += 0x108;
            break;
        default:
            t += 0x108;
            t <<= seg - 1;
        }
        return ((a_val & SIGN_BIT) ? t : -t);
    }

    int g711aDecode(short* amp, const unsigned char* g711a_data, int g711a_bytes)
    {
        int i;
        int samples;
        unsigned char code;
        int sl;

        for (samples = i = 0; ; )
        {
            if (i >= g711a_bytes)
                break;
            code = g711a_data[i++];

            sl = alaw2linear(code);

            amp[samples++] = (short)sl;
        }
        return samples * 2;
    }

    /*
    * ulaw2linear() - Convert a u-law value to 16-bit linear PCM
    *
    * First, a biased linear code is derived from the code word. An unbiased
    * output can then be obtained by subtracting 33 from the biased code.
    *
    * Note that this function expects to be passed the complement of the
    * original code word. This is in keeping with ISDN conventions.
    */
    int ulaw2linear(unsigned char u_val)
    {
        int	t;

        /* Complement to obtain normal u-law value. */
        u_val = ~u_val;

        /*
        * Extract and bias the quantization bits. Then
        * shift up by the segment number and subtract out the bias.
        */
        t = ((u_val & QUANT_MASK) << 3) + BIAS;
        t <<= ((unsigned)u_val & SEG_MASK) >> SEG_SHIFT;

        return ((u_val & SIGN_BIT) ? (BIAS - t) : (t - BIAS));
    }

    int g711uDecode(short* amp, const unsigned char* g711u_data, int g711u_bytes)
    {
        int i;
        int samples;
        unsigned char code;
        int sl;

        for (samples = i = 0;;)
        {
            if (i >= g711u_bytes)
                break;
            code = g711u_data[i++];

            sl = ulaw2linear(code);

            amp[samples++] = (short)sl;
        }
        return samples * 2;
    }
} // namespace
namespace audio
{
    /*
     * function: convert PCM audio format to g711 alaw/ulaw
     *	 InAudioData:	PCM data prepared to encode
     *   OutAudioData:	encoded g711 alaw/ulaw.
     *   DataLen:		PCM data size.
     */

     /*alaw*/
    int PCM2G711a(unsigned char* outAudioData, const char* inAudioData, int dataLen)
    {
        if ((nullptr == outAudioData) or (nullptr == inAudioData) or (0 == dataLen))
        {
            LOG_ERROR_MSG("Error, empty data or transmit failed, exit !");
            return -1;
        }

        return g711aEncode((unsigned char*)outAudioData, (short*)inAudioData, dataLen / 2);
    }

    /*ulaw*/
    int PCM2G711u(unsigned char *outAudioData, const char *inAudioData, int dataLen)
    {
        if ((nullptr == outAudioData) or (nullptr == inAudioData) or (0 == dataLen))
        {
            LOG_ERROR_MSG("Error, empty data or transmit failed, exit !");
            return -1;
        }

        return g711uEncode((unsigned char *)outAudioData, (short*)inAudioData, dataLen / 2);
    }

    /*
     * function: convert g711 alaw audio format to PCM.
     *	 InAudioData:	g711 alaw data prepared to decode
     *   OutAudioData:	decode PCM audio data.
     *   DataLen:		g711a data size.
     */

     /*alaw*/
    int G711a2PCM(char* outAudioData, const char* inAudioData, int dataLen)
    {
        if ((nullptr == inAudioData) or (nullptr == outAudioData) or (0 == dataLen))
        {
            LOG_ERROR_MSG("Error, empty data or transmit failed, exit !");
            return -1;
        }
        
        return g711aDecode((short*)outAudioData, (unsigned char*)inAudioData, dataLen);
    }

    /*ulaw*/
    int G711u2PCM(char *OutAudioData, const char *InAudioData, int DataLen)
    {
        if ((nullptr == InAudioData) or (nullptr == OutAudioData) or (0 == DataLen))
        {
            LOG_ERROR_MSG("Error, empty data or transmit failed, exit !");
            return -1;
        }

        return g711uDecode((short*)OutAudioData, (unsigned char *)InAudioData, DataLen);
    }

} // namespace common