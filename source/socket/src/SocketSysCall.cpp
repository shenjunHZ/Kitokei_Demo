//
// Created by junshen on 9/29/18.
//
#include "SocketSysCall.hpp"

namespace endpoints
{
    // create socket
    int SocketSysCall::wrapper_socket(int domain, int type, int protocol)
    {
        return socket(domain, type, protocol);
    }

    // set socket
    int SocketSysCall::wrapper_setsockopt(int sockfd, int level, int optname, const void* optval, socklen_t optlen)
    {
        return setsockopt(sockfd, level, optname, optval, optlen);
    };

    int SocketSysCall::wrapper_getsockopt(int sockfd, int level, int optname, void* optval, socklen_t* optlen)
    {
        return getsockopt(sockfd, level, optname, optval, optlen);
    }

    // close socket
    int SocketSysCall::wrapper_close(int fd)
    {
        return close(fd);
    };

    // bind address
    int SocketSysCall::wrapper_tcp_bind(int sd, struct sockaddr* addrs, socklen_t addlen)
    {
        return bind(sd, addrs, addlen);
    };

    // tcp connect
    int SocketSysCall::wrapper_tcp_connect(int sd, struct sockaddr* addrs, socklen_t addlen)
    {
        return connect(sd, addrs, addlen);
    };

    int SocketSysCall::wrapper_fcntl(int fd, int cmd, long arg)
    {
        return fcntl(fd, cmd, arg);
    };

    int SocketSysCall::wrapper_epoll_create(int size)
    {
        return epoll_create(size);
    };

    int SocketSysCall::wrapper_epoll_ctl(int epfd, int op, int fd, struct epoll_event* event)
    {
        return epoll_ctl(epfd, op, fd, event);
    };

    int SocketSysCall::wrapper_epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout)
    {
        return epoll_wait(epfd, events, maxevents, timeout);
    };

    // receive msg
    int SocketSysCall::wrapper_tcp_recvmsg(int sockfd, struct msghdr* msg, int flags)
    {
        return recvmsg(sockfd, msg, flags);
    }

    // send msg
    int SocketSysCall::wrapper_tcp_sendmsg(int sockfd, struct msghdr* msg, int flags)
    {
        return sendmsg(sockfd, msg, flags);
    }
}
