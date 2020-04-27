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

    ConcreteRTPSession::~ConcreteRTPSession()
    {
        m_rtpSendSession.Destroy();
        m_rtpReceiveSession.Destroy();
    }

    bool ConcreteRTPSession::createRTPSession(RTPSessionParams& rtpSessionParams)
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
        m_rtpTransmissionParams.SetPortbase(rtp::getRTPLocalSendPort(m_config));
        rtpSessionParams.SetCNAME("sender@host");

        int errCode = m_rtpSendSession.Create(rtpSessionParams, &m_rtpTransmissionParams);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("RTP send session create fail {}", RTPGetErrorString(errCode));
            return false;
        }

        m_rtpTransmissionParams.SetPortbase(rtp::getRTPLocalReceivePort(m_config));
        rtpSessionParams.SetCNAME("receiver@host");

        errCode = m_rtpReceiveSession.Create(rtpSessionParams, &m_rtpTransmissionParams);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("RTP receive session create fail {}", RTPGetErrorString(errCode));
            return false;
        }
        
        LOG_INFO_MSG(m_logger, "Create RTP session success, sender SSRC is: {}, receiver SSRC is: {}.", 
            m_rtpSendSession.GetLocalSSRC(), m_rtpReceiveSession.GetLocalSSRC());

        RTPIPv4Address rtpAddr(destIp, rtp::getRTPRemotePort(m_config));
        addRTPDestination(rtpAddr);
        return true;
    }

    bool ConcreteRTPSession::addRTPDestination(const RTPIPv4Address& rtpAddress)
    {
        int errCode = m_rtpSendSession.AddDestination(rtpAddress);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Add RTP Destination to sender fail {}", RTPGetErrorString(errCode));
            return false;
        }
        errCode = m_rtpReceiveSession.AddDestination(rtpAddress);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Add RTP Destination to receiver fail {}", RTPGetErrorString(errCode));
            return false;
        }
        return true;
    }

    bool ConcreteRTPSession::deleteRTPDestination(const RTPIPv4Address& rtpAddress)
    {
        int errCode = m_rtpSendSession.DeleteDestination(rtpAddress);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Delete RTP Destination to sender fail {}", RTPGetErrorString(errCode));
            return false;
        }
        errCode = m_rtpReceiveSession.DeleteDestination(rtpAddress);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Delete RTP Destination to receiver fail {}", RTPGetErrorString(errCode));
            return false;
        }
        return true;
    }

    int ConcreteRTPSession::sendPacket(const std::string& data, const int& len)
    {
        setSessionDefault();
        int errCode = m_rtpSendSession.SendPacket(data.c_str(), len);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Send RTP packet size {} fail {}", len, RTPGetErrorString(errCode));
        }
        return errCode;
    }

    int ConcreteRTPSession::sendPacket(const std::string& data, const int& len, const unsigned long& timestampinc)
    {
        int errCode = m_rtpSendSession.SendPacket(data.c_str(), len, AudioPayloadType, false, timestampinc);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Send RTP packet size {} fail {}", len, RTPGetErrorString(errCode));
        }
        return errCode;
    }

    int ConcreteRTPSession::sendPacket(const std::uint8_t* data, const int& len, const unsigned long& timestampinc)
    {
        int errCode = m_rtpSendSession.SendPacket(data, len, AudioPayloadType, false, timestampinc);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Send RTP packet size {} fail {}", len, RTPGetErrorString(errCode));
        }
        return errCode;
    }

    int ConcreteRTPSession::SendPacket(const std::string& data, const int& len, 
        const unsigned char& pt, const bool& mark, const unsigned long& timestampinc)
    {
        int errCode = m_rtpSendSession.SendPacket(data.c_str(), len, pt, mark, timestampinc);
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Send RTP packet size {} fail {}", len, RTPGetErrorString(errCode));
        }
        return errCode;
    }

    int ConcreteRTPSession::receivePacket(configuration::RTPSessionDatas& rtpSessionDatas)
    {
        int packetReceived = 0;
        m_rtpReceiveSession.BeginDataAccess();

        if (m_rtpReceiveSession.GotoFirstSource())
        {
            do
            {
                RTPPacket* packet;
                while ((packet = m_rtpReceiveSession.GetNextPacket()) != 0)
                {
                    LOG_DEBUG_MSG("Got packet {}, payload length {} with extended sequence number {} from SSRC {}.",
                        packet->GetPacketLength(), packet->GetPayloadLength(), packet->GetExtendedSequenceNumber(), packet->GetSSRC());
                    configuration::RTPSessionData rtpSessionData;
                    rtpSessionData.timestamp = packet->GetTimestamp();
                    rtpSessionData.sequenceNumber = packet->GetSequenceNumber();
                    rtpSessionData.payloadDatas.resize(packet->GetPayloadLength());
                    memcpy(&rtpSessionData.payloadDatas[0], packet->GetPayloadData(), packet->GetPayloadLength());
                    rtpSessionDatas.push(rtpSessionData);

                    m_rtpReceiveSession.DeletePacket(packet);
                    ++packetReceived;
                }
            } while (m_rtpReceiveSession.GotoNextSource());
        }

        m_rtpReceiveSession.EndDataAccess();
        if (packetReceived <= 0)
        {
            RTPTime::Wait(RTPTime(0, 10));
        }
        return packetReceived;
    }

    bool ConcreteRTPSession::setSessionDefault()
    {
        // RTP payload type(encryption algorithm, sample frequency, bearer channel)
        // 8 means G.721 alaw, sample rate 8000HZ, single channel
        m_rtpSendSession.SetDefaultPayloadType(AudioPayloadType);
        m_rtpSendSession.SetDefaultMark(false);
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
        m_rtpSendSession.SetDefaultTimestampIncrement(10);
        return true;
    }

    bool ConcreteRTPSession::startRTPPolling()
    {
#ifndef RTP_SUPPORT_THREAD
        int errCode = m_rtpSendSession.Poll();
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Polling sender RTP fail {}", RTPGetErrorString(errCode));
            return false;
        }
        errCode = m_rtpReceiveSession.Poll();
        if (errCode < 0)
        {
            LOG_ERROR_MSG("Polling receiver RTP fail {}", RTPGetErrorString(errCode));
            return false;
        }
#endif // RTP_SUPPORT_THREAD
        return true;
    }

} // namespace endpoints