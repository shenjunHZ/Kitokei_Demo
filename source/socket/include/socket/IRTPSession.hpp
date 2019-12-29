#pragma once
#include <jrtplib3/rtpsession.h>
#include <jrtplib3/rtppacket.h>
#include <jrtplib3/rtpsessionparams.h>
#include <jrtplib3/rtpudpv4transmitter.h>
#include "Configurations/Configurations.hpp"

namespace endpoints
{
    using namespace jrtplib;

    class IRTPSession
    {
    public:
        virtual bool createRTPSession(RTPSessionParams& rtpSessionParams) = 0;
        virtual bool startRTPPolling() = 0;
        virtual int sendPacket(const std::string& data, const int& len) = 0;
        virtual int sendPacket(const std::string& data, const int& len, 
            const unsigned long& timestampinc) = 0;
        virtual int SendPacket(const std::string& data, const int& len,
            const unsigned char& pt, const bool& mark, const unsigned long& timestampinc) = 0;
        virtual int sendPacket(const std::uint8_t* data, const int& len, const unsigned long& timestampinc) = 0;
        virtual int receivePacket(configuration::RTPSessionDatas& rtpSessionData) = 0;

        virtual ~IRTPSession() = default;
    };
} // namespace endpoints