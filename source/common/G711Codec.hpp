#pragma once

/*
* u-law, A-law and linear PCM conversions.
*/
namespace audio
{
    int PCM2G711a(unsigned char* outAudioData, const char* inAudioData, int dataLen);
    int PCM2G711u(unsigned char *outAudioData, const char *inAudioData, int dataLen);

    int G711a2PCM(char* outAudioData, const char* inAudioData, int dataLen);
    int G711u2PCM(char *OutAudioData, const char *InAudioData, int DataLen);
} // namespace common