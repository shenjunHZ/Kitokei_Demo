//
// Created by shenjun on 18-9-28.
//

#pragma once
#include <string>
#include <thread>
#include <vector>
#include <stdexcept>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/detail/endpoint.hpp>
#include "logger/LoggerFwd.hpp"
#include "socket/ISocketSysCall.hpp"
#include "Configurations/Configurations.hpp"
#include "Configurations/ParseConfigFile.hpp"
#include "IDataListener.hpp"
#include "timer/TimerService.hpp"
#include "timer/Timer.hpp"

namespace endpoints
{
    using IpEndpoint = boost::asio::ip::detail::endpoint;
    using Address = boost::asio::ip::address;
	typedef enum class eLinkStatus
	{
		LINK_STATUS_INIT = 0,
		LINK_STATUS_BIND = 1,
		LINK_STATUS_CONNECTED = 2
	}LinkStatus;

    class ClientSocket final
    {
    public:
        ClientSocket(
                Logger& logger,
                ISocketSysCall& sysCall,
                configuration::TcpConfiguration& tcpConfiguration,
                endpoints::IDataListener& dataListener,
                const configuration::AppConfiguration& config,
                const configuration::AppAddresses& appAddress,
                timerservice::TimerService& timerService);

        ~ClientSocket();

        void startSocketLink();

    private:
        void createTcpSocket(IpEndpoint&);
        void setTcpSocketOptions(int, const configuration::TcpConfiguration&);
        void getTcpSocketOptions(int socketDescriptor, tcp_info& tcpInfo, int& infoLen);
        void startDataReceiverThread();
        void receiveDataRoutine();
        void receiveDataFromSocket();
        bool connectServer();
        void stopConcreteTimer();
        bool reConnectServer();
        void processSocketState(const tcp_info& info);

        bool setNonblocking(int&);
        void rebindSocketToAnyPort();
        void setTcpInitialMessage();

    private:
        int m_socketfId;
        configuration::TcpConfiguration m_tcpConfiguration;
        std::thread m_connectThread;
        endpoints::ISocketSysCall& m_socketSysCall;
        Logger& m_logger;

        std::atomic<bool> m_continueReceiving{false};
        std::atomic<bool> m_socketClosed{true};
        std::atomic<bool> m_concreteTimerStop{true};

        struct tcpEndpointAddress
        {
            std::string ipAddress;
            unsigned int portNumber;
        };
        tcpEndpointAddress m_endpointAddress;
        IpEndpoint m_serverAddr;
        IpEndpoint m_localAddr;
        endpoints::IDataListener& m_dataListener;
        timerservice::TimerService& m_timerService;
        std::unique_ptr<timerservice::Timer> m_concreteTimer;
		LinkStatus linkStatus;
    };
} // namespace cp_nb
