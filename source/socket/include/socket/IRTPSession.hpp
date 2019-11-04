#pragma once
#include <jrtplib3/rtpsessionparams.h>
#include <jrtplib3/rtpudpv4transmitter.h>

namespace endpoints
{
    using namespace jrtplib;

    class IRTPSession
    {
    public:
        virtual bool createRTPSession(const RTPSessionParams& rtpSessionParams) = 0;
        virtual bool addRTPDestination(const RTPIPv4Address& rtpAddress) = 0;
        virtual bool deleteRTPDestination(const RTPIPv4Address& rtpAddress) = 0;

        virtual int sendPacket(const std::string& data, const int& len) = 0;
        virtual int sendPacket(const std::string& data, const int& len, 
            const unsigned long& timestampinc) = 0;
        virtual int SendPacket(const std::string& data, const int& len,
            const unsigned char& pt, const bool& mark, const unsigned long& timestampinc) = 0;

        virtual ~IRTPSession() = default;
    };
} // namespace endpoints