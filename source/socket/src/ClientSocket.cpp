//
// Created by junshen on 9/29/18.
//
#include <sys/socket.h>
#include <sys/epoll.h>
#include <cstring>
#include "socket/ClientSocket.hpp"
#include "logger/Logger.hpp"
#include "player/CodeConverter.hpp"
#include "configurations/AppConfiguration.hpp"
#include <boost/exception/diagnostic_information.hpp>

namespace endpoints
{
    namespace
    {
        constexpr uint32_t BUFFER_SIZE = 64 * 1024;
        std::atomic_bool keep_connect(true);
    } // namespace

    auto createServerAdd(const std::string &ipAddress, unsigned int portNumber)
    {
        Address address = Address::from_string(ipAddress);
        return IpEndpoint(address, portNumber);
    }
} // namespace endpoint

namespace endpoints
{
    ClientSocket::ClientSocket(Logger& logger,
                               ISocketSysCall& sysCall,
                               configuration::TcpConfiguration& tcpConfiguration,
                               applications::IDataListener& dataListener,
                               const configuration::AppConfiguration& config,
                               const configuration::AppAddresses& appAddress,
                               timerservice::TimerService& timerService)
    : m_socketfId{-1}
    , m_tcpConfiguration{tcpConfiguration}
    , m_socketSysCall{sysCall}
    , m_logger{logger}
    , m_endpointAddress{appAddress.serverAddress, appAddress.serverPort}
    , m_serverAddr{createServerAdd(appAddress.serverAddress, appAddress.serverPort)}
    , m_localAddr{createServerAdd(appAddress.clientAddress, appAddress.clientPort)}
    , m_dataListener{dataListener}
    , m_timerService{timerService}
    {
        LOG_INFO_MSG(m_logger, "Createing tcp endpoint to {} : {}",
                     appAddress.serverAddress , appAddress.serverPort);
        createTcpSocket(m_localAddr);
        setTcpSocketOptions(m_socketfId, m_tcpConfiguration);
        socklen_t socketLen = sizeof(*m_localAddr.data());
        auto const ret = m_socketSysCall.wrapper_tcp_bind(m_socketfId, m_localAddr.data(), socketLen);
        //auto const ret = 1;
        if(0 > ret)
        {
            LOG_INFO_MSG(m_logger, "tcp socket bind error code : {}", std::strerror(errno));
            rebindSocketToAnyPort();
        }
        else
        {
            LOG_INFO_MSG(m_logger, "binding tcp local endpoint success! {} : {}",
                         m_localAddr.address().to_string(), m_localAddr.port());
        }

    }

    ClientSocket::~ClientSocket()
    {
        LOG_INFO_MSG(m_logger, "Desorying tcp endpoint");
        m_continueReceiving = false;
        //if(m_dataReceiverThread.joinable())
        //{
        //    m_dataReceiverThread.join();
        //}

        if(not m_socketClosed)
        {
            m_socketSysCall.wrapper_close(m_socketfId);
        }
    }

    void ClientSocket::rebindSocketToAnyPort()
    {
        const Address ipAddress(
            m_localAddr.is_v4() ? Address(boost::asio::ip::address_v4::any()) : Address(boost::asio::ip::address_v6::any()));
        IpEndpoint clientAddr(ipAddress, 0);
        socklen_t socketLen = sizeof(*clientAddr.data());
        const auto ret = m_socketSysCall.wrapper_tcp_bind(m_socketfId, clientAddr.data(), socketLen);
        if (ret < 0)
        {
            const auto& errorMsg = std::string("tcp socket bind ANY. Error: ") + std::strerror(errno);
            LOG_ERROR_MSG(errorMsg.c_str());

            BOOST_THROW_EXCEPTION(std::runtime_error(errorMsg));
        }
        LOG_INFO_MSG(m_logger, "Binding SCTP local endpoint to any success! {} : {}",
                     clientAddr.address().to_string(), clientAddr.port());
    }

    void ClientSocket::setTcpInitialMessage()
    {

    }

    void ClientSocket::setTcpSocketOptions(int socketDescriptor, const configuration::TcpConfiguration& tcpConfiguration)
    {

    }

    void ClientSocket::getTcpSocketOptions(int socketDescriptor, tcp_info& tcpInfo, int& infoLen)
    {
        m_socketSysCall.wrapper_getsockopt(m_socketfId, IPPROTO_TCP, TCP_INFO,
                                           &tcpInfo, reinterpret_cast<socklen_t *>(&infoLen));

    }

    void ClientSocket::startSocketLink()
    {
        if(connectServer())
        {
            startDataReceiverThread();
        }
        else
        {
            bool bConnectStatus = false;
            do
            {
                LOG_ERROR_MSG("tcp connection failed. scocke id: {} Error code : {}", m_socketfId, std::strerror(errno));
                sleep(5);
                bConnectStatus = reConnectServer();
            }while(!bConnectStatus);

            startDataReceiverThread();
        }
    }

    void ClientSocket::stopConcreteTimer()
    {
        if(m_concreteTimer)
        {
            if(not m_concreteTimerStop)
            {
                m_concreteTimer->cancel();
                m_concreteTimer.reset();
            }
        }
    }

    bool ClientSocket::reConnectServer()
    {
        int connectStatus = -1;

        socklen_t socketLen = sizeof(*m_serverAddr.data());
        connectStatus = m_socketSysCall.wrapper_tcp_connect(m_socketfId, m_serverAddr.data(), socketLen);
        if(0 != connectStatus)
        {
            // timer
            //auto startConnectDuration = std::chrono::milliseconds{1000*10};
            //stopConcreteTimer();
            //m_concreteTimer = m_timerService.scheduleTimer(startConnectDuration, [this]()
            //{
                //reConnectServer();
            //});

            LOG_ERROR_MSG("tcp connection failed. scocke id: {} Error code : {} status: {}", m_socketfId, std::strerror(errno), connectStatus);
            return false;
        }
        m_socketClosed = false;
        LOG_INFO_MSG(m_logger, "tcp connection success.");
        return true;
    }

    bool ClientSocket::connectServer()
    {
        LOG_INFO_MSG(m_logger, "Initiating tcp connection with {} : {}",
                     m_endpointAddress.ipAddress, m_endpointAddress.portNumber);
        socklen_t socketLen = sizeof(*m_serverAddr.data());
        const int connectStatus = m_socketSysCall.wrapper_tcp_connect(m_socketfId, m_serverAddr.data(), socketLen);
        if(0 != connectStatus)
        {
            LOG_ERROR_MSG("tcp connection failed. scocke id: {} Error code : {}", m_socketfId, std::strerror(errno));
            return false;
        }
        m_socketClosed = false;
        LOG_INFO_MSG(m_logger, "tcp connection success.");
        return true;
    }

    void ClientSocket::createTcpSocket(IpEndpoint& ipEndpoint)
    {
        int domain = ipEndpoint.is_v4() ? AF_INET : AF_INET6;
        m_socketfId = m_socketSysCall.wrapper_socket(domain, SOCK_STREAM, IPPROTO_TCP);
        if(0 > m_socketfId)
        {
            LOG_ERROR_MSG("tcp socket creation failure. error code: {}", std::strerror(errno));
        }
        else
        {
            m_socketClosed = false;
        }
    }

    // start thread
    void ClientSocket::startDataReceiverThread()
    {
        m_continueReceiving = true;
        //m_dataReceiverThread = std::thread(&ClientSocket::receiveDataRoutine, this);
        receiveDataRoutine();
    }

    /*F_SETFL：设置文件状态标识
     * F_GETFD：读取文件描述标识*/
    bool ClientSocket::setNonblocking(int& fd)
    {
        return m_socketSysCall.wrapper_fcntl(fd, F_SETFL, m_socketSysCall.wrapper_fcntl(fd, F_GETFD, 0) | O_NONBLOCK) != -1;
    }

    void ClientSocket::receiveDataRoutine()
    {
        constexpr auto MAXEPOLLSIZE = 100;
        int nfds, kdpfd;
        struct epoll_event ev
        {
        }, events[MAXEPOLLSIZE];

        if (not setNonblocking(m_socketfId))
        {
            const auto& errorMsg = std::string("setNonblocking fail. Error: ") + std::strerror(errno);
            LOG_ERROR_MSG(errorMsg.c_str());

            BOOST_THROW_EXCEPTION(std::runtime_error(errorMsg));
        }

        kdpfd = m_socketSysCall.wrapper_epoll_create(MAXEPOLLSIZE);
        ev.events = EPOLLIN;
        ev.data.fd = m_socketfId;
        if (m_socketSysCall.wrapper_epoll_ctl(kdpfd, EPOLL_CTL_ADD, m_socketfId, &ev) < 0)
        {
            const auto& errorMsg = std::string("epoll_ctl call error. Error: ") + std::strerror(errno);
            BOOST_THROW_EXCEPTION(std::runtime_error(errorMsg));
        }

        while (m_continueReceiving)
        {
            nfds = m_socketSysCall.wrapper_epoll_wait(kdpfd, events, MAXEPOLLSIZE, 100);
            if (nfds == -1)
            {
                LOG_ERROR_MSG("epoll_wait failed. Error: {}", std::strerror(errno));
                continue;
            }
            //else if(m_socketClosed)
            {
                //sleep(5);
                //connectServer();
            }

            for (int n = 0; n < nfds; ++n)
            {
                if ((events[n].events & EPOLLIN) != 0u)
                {
                    receiveDataFromSocket();
                }
            }
        }
    }

    void ClientSocket::receiveDataFromSocket()
    {
        struct msghdr msg;
        msg.msg_name = nullptr;
        struct iovec io; // return data
        std::vector<uint8_t> receiveDataBuffer(BUFFER_SIZE);
        io.iov_base = receiveDataBuffer.data();
        io.iov_len = BUFFER_SIZE;
        msg.msg_iov = &io;
        msg.msg_iovlen = 1;
#ifdef RUN_IN_ARM
        const unsigned int size = m_socketSysCall.wrapper_tcp_recvmsg(m_socketfId, &msg, 0);
#else
        const uint64_t size = m_socketSysCall.wrapper_tcp_recvmsg(m_socketfId, &msg, 0);
#endif
        if(size >= static_cast<int>(BUFFER_SIZE))
        {
            LOG_ERROR_MSG("Exceed maximum buffer size.");
            return;
        }
        if(size > 0)
        {
            LOG_INFO_MSG(m_logger, "Received {} bytes from {} : {}", size,
                         m_endpointAddress.ipAddress, m_endpointAddress.portNumber);

            player::CodeConverter codeConverter("gb2312", "utf-8//TRANSLIT//IGNORE");
            std::shared_ptr<char*> dataBuffer = std::make_shared<char*>(new char[size * 2 + 1]);
            memset(*dataBuffer, 0 , size * 2 + 1);

            codeConverter.decodeCoverter(reinterpret_cast<char*>(receiveDataBuffer.data()), reinterpret_cast<size_t>(size),
                                         *dataBuffer, reinterpret_cast<size_t>(size * 2 + 1));
            m_dataListener.onDataMessage(*dataBuffer); // to do gsl
        }
        else
        {
            tcp_info info;
            int infoLen = sizeof(info);
            getTcpSocketOptions(m_socketfId, info, infoLen);
            LOG_INFO_MSG(m_logger, "socket status: {}", info.tcpi_state);

            if((info.tcpi_state == TCP_ESTABLISHED))
            {
               return;
            }
            processSocketState(info);
        }
    }

    void ClientSocket::processSocketState(const tcp_info& info)
    {
        if(TCP_CLOSE_WAIT == info.tcpi_state)
        {
            //m_socketSysCall.wrapper_close(m_socketfId);
            m_socketClosed = true;
            LOG_DEBUG_MSG("Socket {} close.", m_socketfId);
        }
        else
        {
            const auto& errorMsg = std::string("tcp message recieve failed code : ") + std::strerror(errno);
            LOG_ERROR_MSG(errorMsg);
        }
    }
} // namespace applications
