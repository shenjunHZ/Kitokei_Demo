#include "socket/ConcreteRTPSession.hpp"
#include "common/CommonFunction.hpp"

namespace endpoints
{
    using namespace jrtplib;

    ConcreteRTPSession::ConcreteRTPSession(Logger& logger, const configuration::AppConfiguration& config)
        : m_logger{logger}
        , m_config{config}
    {

    }

    bool ConcreteRTPSession::createRTPSession(const RTPSessionParams& rtpSessionParams)
    {
        std::string remoteIpAddress = rtp::getRTPRemoteIpAddress(m_config);
        if (remoteIpAddress.empty())
        {
            LOG_ERROR_MSG("Empty IP address for rtp.");
            return false;
        }
        uint32_t destIp = inet_addr(remoteIpAddress.c_str());
        if (INADDR_NONE == destIp)
        {
            LOG_ERROR_MSG("Bad IP address specified.");
            return false;
        }
        /* The inet_addr function returns a value in network byte order, but
        * we need the IP address in host byte order, so we use a call to
        * ntohl */
        destIp = ntohl(destIp);

        /* Now, we'll create a RTP session, set the destination, send some
        * packets and poll for incoming data. */
        m_rtpTransmissionParams.SetPortbase(rtp::getRTPLocalPort(m_config));
        RTPIPv4Address rtpAddr(destIp, rtp::getRTPRemotePort(m_config));

        int errCode = m_rtpSession.Create(rtpSessionParams, &m_rtpTransmissionParams);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("RTP Session create fail {}", RTPGetErrorString(errCode));
            return false;
        }
        
        LOG_INFO_MSG(m_logger, "Create RTP session success.");
        addRTPDestination(rtpAddr);
        return true;
    }

    bool ConcreteRTPSession::addRTPDestination(const RTPIPv4Address& rtpAddress)
    {
        int errCode = m_rtpSession.AddDestination(rtpAddress);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Add RTP Destination fail {}", RTPGetErrorString(errCode));
            return false;
        }
        return true;
    }

    bool ConcreteRTPSession::deleteRTPDestination(const RTPIPv4Address& rtpAddress)
    {
        int errCode = m_rtpSession.DeleteDestination(rtpAddress);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Delete RTP Destination fail {}", RTPGetErrorString(errCode));
            return false;
        }
        return true;
    }

    int ConcreteRTPSession::sendPacket(const std::string& data, const int& len)
    {
        setSessionDefault();
        int errCode = m_rtpSession.SendPacket(data.c_str(), len);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Send RTP packet fail {}", RTPGetErrorString(errCode));
        }
        return errCode;
    }

    int ConcreteRTPSession::SendPacket(const std::string& data, const int& len, 
        const unsigned char& pt, const bool& mark, const unsigned long& timestampinc)
    {
        int errCode = m_rtpSession.SendPacket(data.c_str(), len, pt, mark, timestampinc);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Send RTP packet fail {}", RTPGetErrorString(errCode));
        }
        return errCode;
    }

    bool ConcreteRTPSession::setSessionDefault()
    {
        m_rtpSession.SetDefaultPayloadType(0);
        m_rtpSession.SetDefaultMark(false);
        m_rtpSession.SetDefaultTimestampIncrement(10);
    }

} // namespace endpoints