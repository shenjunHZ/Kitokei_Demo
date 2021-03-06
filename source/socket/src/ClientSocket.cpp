//
// Created by junshen on 9/29/18.
//
#include <cstring>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <stdexcept>
#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include "socket/ClientSocket.hpp"
#include "logger/Logger.hpp"
#include "common/CodeConverter.hpp"
#include "Configurations/Configurations.hpp"

namespace endpoints
{
    namespace
    {
        constexpr uint32_t BUFFER_SIZE = 64 * 1024;
        std::atomic_bool keep_connect(true);
    } // namespace

    std::string getIpAddressByHostname(const std::string& hostName)
    try
    {
        using namespace boost::asio::ip;
        tcp::resolver::query query(hostName, "");

        boost::asio::io_service ioService;
		boost::asio::ip::tcp::resolver::iterator resolvedIterator = tcp::resolver(ioService).resolve(query);
        while (resolvedIterator != tcp::resolver::iterator())
        {
            tcp::endpoint endpoint = *resolvedIterator;
			boost::asio::ip::address address;
			address = address.from_string(endpoint.address().to_string());
			if (address.is_v4())
			{
				return address.to_string();
			}
			resolvedIterator++;
        }
        BOOST_THROW_EXCEPTION(std::runtime_error("Unable to resolve " + hostName + " with boost tcp resolver"));
    }
    catch (const boost::system::system_error& e)
    {
        BOOST_THROW_EXCEPTION(
            std::runtime_error("Error while trying to resolve " + hostName + " with boost tcp resolver: " + e.what()));
    }

    auto createServerAdd(const std::string &ipAddress, unsigned int portNumber)
    {
        std::string ipAdd = ipAddress;
        if (ipAdd.empty())
        {
			ipAdd = getIpAddressByHostname(boost::asio::ip::host_name());
			LOG_DEBUG_MSG("To solve host ip address: {} link to tcp service.", ipAdd);
        }
		
        Address address = Address::from_string(ipAdd);
        return IpEndpoint(address, portNumber);
    }
} // namespace endpoint

namespace endpoints
{
    ClientSocket::ClientSocket(Logger& logger,
                               ISocketSysCall& sysCall,
                               configuration::TcpConfiguration& tcpConfiguration,
                               endpoints::IDataListener& dataListener,
                               const configuration::AppConfiguration& config,
                               const configuration::AppAddresses& appAddress,
                               timerservice::TimerService& timerService)
    : m_socketfId{-1}
    , m_tcpConfiguration{tcpConfiguration}
    , m_socketSysCall{sysCall}
    , m_logger{logger}
    , m_endpointAddress{appAddress.chessBoardServerAddress, appAddress.chessBoardServerPort }
    , m_serverAddr{createServerAdd(appAddress.chessBoardServerAddress, appAddress.chessBoardServerPort)}
    , m_localAddr{createServerAdd(appAddress.kitokeiLocalAddress, appAddress.kitokeiLocalPort)}
    , m_dataListener{dataListener}
    , m_timerService{timerService}
	, linkStatus(eLinkStatus::LINK_STATUS_INIT)
    {
        LOG_INFO_MSG(m_logger, "Createing tcp endpoint to {} : {}",
                     appAddress.chessBoardServerAddress, appAddress.chessBoardServerPort);
        createTcpSocket(m_localAddr);
        setTcpSocketOptions(m_socketfId, m_tcpConfiguration);
        socklen_t socketLen = sizeof(*m_localAddr.data());
		auto const ret = 0; // m_socketSysCall.wrapper_tcp_bind(m_socketfId, m_localAddr.data(), socketLen);
        if(0 > ret)
        {
            LOG_INFO_MSG(m_logger, "tcp socket bind {}:{} error code : {}", 
				m_localAddr.address().to_string(), m_localAddr.port(), std::strerror(errno));
            rebindSocketToAnyPort();
        }
        else
        {
            LOG_INFO_MSG(m_logger, "binding tcp local endpoint success! {} : {}",
                         m_localAddr.address().to_string(), m_localAddr.port());
        }
		linkStatus = eLinkStatus::LINK_STATUS_BIND;
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
            LOG_ERROR_MSG("tcp socket bind ANY. Error: {}", std::strerror(errno));
			return;
        }
        LOG_INFO_MSG(m_logger, "Binding SCTP local endpoint to any success! {} : {}",
                     clientAddr.address().to_string(), clientAddr.port());
		linkStatus = eLinkStatus::LINK_STATUS_BIND;
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

	bool ClientSocket::prepareConnect()
	{
		if (connectServer())
		{
			return true;
		}
		else
		{
			bool bConnectStatus = false;
			do
			{
				sleep(5);
				bConnectStatus = reConnectServer();
			} while (!bConnectStatus);
		}
		return true;
	}

    void ClientSocket::startSocketLink()
    {
		if (linkStatus < eLinkStatus::LINK_STATUS_BIND)
		{
			return;
		}

		prepareConnect();
        startDataReceiverThread();
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
        socklen_t socketLen = sizeof(*m_serverAddr.data());
        int connectStatus = m_socketSysCall.wrapper_tcp_connect(m_socketfId, m_serverAddr.data(), socketLen);
        if(0 != connectStatus)
        {
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
        receiveDataRoutine();
    }

    /*F_SETFL：set file state flag
     * F_GETFD：read file description*/
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

            common::CodeConverter codeConverter("gb2312", "utf-8//TRANSLIT//IGNORE");
            std::shared_ptr<char*> dataBuffer = std::make_shared<char*>(new char[size * 2 + 1]);
            memset(*dataBuffer, 0 , size * 2 + 1);

            codeConverter.decodeCoverter(reinterpret_cast<char*>(receiveDataBuffer.data()), size, *dataBuffer, (size * 2 + 1));
            m_dataListener.onDataMessage(*dataBuffer);
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
            m_socketSysCall.wrapper_close(m_socketfId);
            m_socketClosed = true;
            LOG_DEBUG_MSG("Socket {} close.", m_socketfId);

            createTcpSocket(m_localAddr);
            setTcpSocketOptions(m_socketfId, m_tcpConfiguration);
            startSocketLink();
        }
        else
        {
            const auto& errorMsg = std::string("tcp message recieve failed code : ") + std::strerror(errno);
            LOG_ERROR_MSG(errorMsg);
        }
    }
} // namespace applications
