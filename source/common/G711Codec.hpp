#pragma once

/*
* u-law, A-law and linear PCM conversions.
*/
namespace audio
{
    int PCM2G711a(unsigned char* outAudioData, const char* inAudioData, const int dataLen);
    int PCM2G711u(unsigned char *outAudioData, const char *inAudioData, const int dataLen);

    int G711a2PCM(char* outAudioData, const char* inAudioData, const int dataLen);
    int G711u2PCM(char *outAudioData, const char *inAudioData, const int dataLen);
} // namespace common