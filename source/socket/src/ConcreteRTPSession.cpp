#include "socket/ConcreteRTPSession.hpp"
#include "common/CommonFunction.hpp"

namespace
{
    constexpr int AudioPayloadType = 97;
} // namespace
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
            LOG_ERROR_MSG("Send RTP packet size {} fail {}", len, RTPGetErrorString(errCode));
        }
        return errCode;
    }

    int ConcreteRTPSession::sendPacket(const std::string& data, const int& len, const unsigned long& timestampinc)
    {
        int errCode = m_rtpSession.SendPacket(data.c_str(), len, AudioPayloadType, false, timestampinc);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Send RTP packet size {} fail {}", len, RTPGetErrorString(errCode));
        }
        return errCode;
    }

    int ConcreteRTPSession::SendPacket(const std::string& data, const int& len, 
        const unsigned char& pt, const bool& mark, const unsigned long& timestampinc)
    {
        int errCode = m_rtpSession.SendPacket(data.c_str(), len, pt, mark, timestampinc);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Send RTP packet size {} fail {}", len, RTPGetErrorString(errCode));
        }
        return errCode;
    }

    bool ConcreteRTPSession::setSessionDefault()
    {
        // RTP payload type(encryption algorithm, sample frequency, bearer channel)
        // 8 means G.721 alaw, sample rate 8000HZ, single channel
        m_rtpSession.SetDefaultPayloadType(AudioPayloadType);
        m_rtpSession.SetDefaultMark(false);
        /*
        m=audio 9000 RTP/AVP 97
        a=rtpmap:97 pcma/16000/2
        a=framerate:25

        c=IN IP4 172.18.168.45

        1.m = audio: media type / 9000：port received / RTP/AVP：transmit protocol / 97：rtp header payload type

        2.a = rtpmap: optional / 97：rtp header payload type / pcma: audio encode type / 16000: sample rate / 2：channels

        注意：g711有两种编码类型，另一种是pcmu

        3.a=framerate:25    指1s播放几个rtp包，单位帧每秒，倒数为一个rtp包承载的数据播放的时间，单位s

                                   16000/25=320   表示每个时间戳增量值    每个rtp包的g711数据大小

        4.c=：媒体链接信息；IN：网络类型一般为IN；IP4：地址类型一般为IP4；后面是IP地址（注意是VLC所在的IP地址，不是发送方的IP）
        */
        m_rtpSession.SetDefaultTimestampIncrement(10);
    }

} // namespace endpoints