#pragma once
#include "IRTPSession.hpp"
#include "logger/Logger.hpp"
#include "Configurations/ParseConfigFile.hpp"

namespace endpoints
{
    class ConcreteRTPSession : public IRTPSession
    {
    public:
        ConcreteRTPSession(Logger& logger, const configuration::AppConfiguration& config);
        ~ConcreteRTPSession();
        bool createRTPSession(RTPSessionParams& rtpSessionParams) override;
        bool startRTPPolling() override;
        int sendPacket(const std::string& data, const int& len) override;
        int sendPacket(const std::string& data, const int& len, const unsigned long& timestampinc) override;
        int SendPacket(const std::string& data, const int& len,
            const unsigned char& pt, const bool& mark, const unsigned long& timestampinc) override;
        int sendPacket(const std::uint8_t* data, const int& len, const unsigned long& timestampinc) override;
        int receivePacket(configuration::RTPSessionDatas& rtpSessionData) override;

    private:
        bool setSessionDefault();
        bool addRTPDestination(const RTPIPv4Address& rtpAddress);
        bool deleteRTPDestination(const RTPIPv4Address& rtpAddress);

    private:
        Logger& m_logger;
        const configuration::AppConfiguration& m_config;

        jrtplib::RTPSession m_rtpSendSession;
        jrtplib::RTPSession m_rtpReceiveSession;
        jrtplib::RTPUDPv4TransmissionParams m_rtpTransmissionParams;
    };
} // namespace endpoints