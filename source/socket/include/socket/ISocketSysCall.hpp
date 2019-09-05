//
// Created by junshen on 9/29/18.
//

#pragma once

namespace endpoints
{
    class ISocketSysCall
    {
    public:
        virtual int wrapper_socket(int, int, int) = 0;
        virtual int wrapper_setsockopt(int, int, int, const void*, socklen_t) = 0;
        virtual int wrapper_getsockopt(int, int, int, void*, socklen_t*) = 0;
        virtual int wrapper_close(int) = 0;
        virtual int wrapper_tcp_bind(int, struct sockaddr*, socklen_t) = 0;
        virtual int wrapper_tcp_connect(int, struct sockaddr*, socklen_t) = 0;
        virtual int wrapper_tcp_sendmsg(int sockfd, struct msghdr* msg, int flags) = 0;
        virtual int wrapper_tcp_recvmsg(int sockfd, struct msghdr* msg, int flags) = 0;
        virtual int wrapper_fcntl(int, int, long) = 0;
        virtual int wrapper_epoll_create(int) = 0;
        virtual int wrapper_epoll_ctl(int, int, int, struct epoll_event*) = 0;
        virtual int wrapper_epoll_wait(int, struct epoll_event*, int, int) = 0;

        virtual ~ISocketSysCall() = default;
    };
} // namespace endpoints
