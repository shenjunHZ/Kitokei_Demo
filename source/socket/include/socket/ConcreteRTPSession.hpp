#pragma once
#include <jrtplib3/rtpsession.h>
#include "IRTPSession.hpp"
#include "logger/Logger.hpp"
#include "Configurations/ParseConfigFile.hpp"

namespace endpoints
{
    class ConcreteRTPSession : public IRTPSession
    {
    public:
        ConcreteRTPSession(Logger& logger, const configuration::AppConfiguration& config);
        bool createRTPSession(const RTPSessionParams& rtpSessionParams) override;
        bool addRTPDestination(const RTPIPv4Address& rtpAddress) override;
        bool deleteRTPDestination(const RTPIPv4Address& rtpAddress) override;

        int sendPacket(const std::string& data, const int& len) override;
        int SendPacket(const std::string& data, const int& len,
            const unsigned char& pt, const bool& mark, const unsigned long& timestampinc) override;

    private:
        bool setSessionDefault();

    private:
        Logger& m_logger;
        const configuration::AppConfiguration& m_config;

        jrtplib::RTPSession m_rtpSession;
        jrtplib::RTPUDPv4TransmissionParams m_rtpTransmissionParams;
    };
} // namespace endpoints