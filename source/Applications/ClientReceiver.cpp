//
// Created by junshen on 10/1/18.
//
#include "ClientReceiver.hpp"
#include "socket/SocketSysCall.hpp"
#include "logger/Logger.hpp"

namespace application
{
    ClientReceiver::ClientReceiver(Logger& logger, const configuration::AppConfiguration& config,
                                   const configuration::AppAddresses& appAddress, timerservice::TimerService& timerService)
        :m_logger{logger}
    {
        configuration::TcpConfiguration tcpConfiguration;
        m_socketSysCall = std::make_unique<endpoints::SocketSysCall>();
        m_clientSocket = std::make_unique<endpoints::ClientSocket>(logger, *m_socketSysCall, tcpConfiguration,
                                                                   *this, config, appAddress, timerService);
    }

    void ClientReceiver::receiveLoop()
    {
        m_clientSocket->startSocketLink();
    }


    void ClientReceiver::onDataMessage(const std::string& dataMessage)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_receiveDatas.push(dataMessage);
        LOG_DEBUG_MSG("data message : {}", dataMessage.c_str());

        m_condition.notify_one();
    }

    std::string ClientReceiver::getDataMessage()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait(lock, [this]()
                        {
                            return !m_receiveDatas.empty();
                        });
        std::string temp = m_receiveDatas.front();
        m_receiveDatas.pop();
        return temp;
    }
}
