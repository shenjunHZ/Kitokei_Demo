//
// Created by junshen on 9/29/18.
//

#pragma once
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <atomic>
#include <cstdlib>
#include <fcntl.h>
#include <functional>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>

#include "ISocketSysCall.hpp"

namespace endpoints
{
    class SocketSysCall : public ISocketSysCall
    {
    public:
        int wrapper_socket(int, int, int) override;
        int wrapper_setsockopt(int, int, int, const void*, socklen_t) override;
        int wrapper_getsockopt(int, int, int, void*, socklen_t*) override;
        int wrapper_close(int) override;
        int wrapper_tcp_bind(int, struct sockaddr*, socklen_t) override;
        int wrapper_tcp_connect(int, struct sockaddr*, socklen_t) override;
        int wrapper_tcp_sendmsg(int sockfd, struct msghdr* msg, int flags) override;
        int wrapper_tcp_recvmsg(int sockfd, struct msghdr* msg, int flags) override;
        int wrapper_fcntl(int, int, long) override;
        int wrapper_epoll_create(int) override;
        int wrapper_epoll_ctl(int, int, int, struct epoll_event*) override;
        int wrapper_epoll_wait(int, struct epoll_event*, int, int) override;
    };
}
