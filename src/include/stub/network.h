/*
    futils - utility library
    Copyright (c) 2021-2024 on-keyday (https://github.com/on-keyday)
    Released under the MIT license
    https://opensource.org/licenses/mit-license.php
*/

// stub/network.h - Stub for network functions
#pragma once

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif
struct sockaddr {
    unsigned short sa_family;
    char sa_data[14];
};
struct in_addr {
    unsigned long s_addr;
};
struct sockaddr_in {
    unsigned short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};
#define AF_INET 2
struct in6_addr {
    unsigned char s6_addr[16];
};
struct sockaddr_in6 {
    unsigned short sin6_family;
    unsigned short sin6_port;
    unsigned long sin6_flowinfo;
    struct in6_addr sin6_addr;
    unsigned long sin6_scope_id;
};
#define AF_INET6 10

struct sockaddr_un {
    unsigned short sun_family;
    char sun_path[108];
};
#define AF_UNIX 1

struct sockaddr_storage {
    unsigned short ss_family;
    char __ss_padding[126];
    unsigned long long __ss_align;
};

#define AF_UNSPEC 0

struct fd_set {
    unsigned long fds_bits[1];
};

#define FD_SET(fd, set) ((set)->fds_bits[0] |= (1UL << (fd)))
#define FD_ZERO(set) ((set)->fds_bits[0] = 0)
#define FD_ISSET(fd, set) ((set)->fds_bits[0] & (1UL << (fd)))

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

#define SOL_SOCKET 0xffff
#define SO_REUSEADDR 0x0004
#define IPV6_V6ONLY 27
#define IPPROTO_TCP 6
#define IPPROTO_IPV6 41
#define IPPROTO_UDP 17
#define TCP_NODELAY 0x0001
#define IPPROTO_IP 0
#define IP_TTL 2
#define IP_MTU 14
#define IP_PMTUDISC_WANT 1
#define IP_PMTUDISC_DO 2
#define IP_PMTUDISC_DONT 3
#define IP_PMTUDISC_PROBE 4
#define IP_MTU_DISCOVER 10
#define IPV6_MTU_DISCOVER 25
#define IPV6_DONTFRAG 62
#define IP_HDRINCL 2
#define IPV6_HDRINCL 36

#define SO_TYPE 0x1008
#define SO_PROTOCOL 0x1028
#define SO_DOMAIN 0x1029

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3

#define AI_PASSIVE 0x0001

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    unsigned int ai_addrlen;
    char* ai_canonname;
    struct sockaddr* ai_addr;
    struct addrinfo* ai_next;
};

typedef unsigned int socklen_t;
struct timeval;
struct fd_set;
struct addrinfo;
struct epoll_event;
typedef unsigned char sigset_t;
EXTERN_C int send(int socket, const void* buffer, size_t length, int flags);
EXTERN_C int recv(int socket, void* buffer, size_t length, int flags);
EXTERN_C int sendto(int socket, const void* message, size_t length, int flags, const struct sockaddr* dest_addr,
                    socklen_t dest_len);
EXTERN_C int recvfrom(int socket, void* buffer, size_t length, int flags, struct sockaddr* address, socklen_t* address_len);
EXTERN_C int setsockopt(int socket, int level, int option_name, const void* option_value, socklen_t option_len);
EXTERN_C int getsockopt(int socket, int level, int option_name, void* option_value, socklen_t* option_len);
EXTERN_C int shutdown(int socket, int how);
EXTERN_C int connect(int socket, const struct sockaddr* address, socklen_t address_len);
EXTERN_C int bind(int socket, const struct sockaddr* address, socklen_t address_len);
EXTERN_C int listen(int socket, int backlog);
EXTERN_C int accept(int socket, struct sockaddr* address, socklen_t* address_len);
EXTERN_C int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout);
EXTERN_C int gethostname(char* name, size_t len);
EXTERN_C int getsockname(int socket, struct sockaddr* address, socklen_t* address_len);
EXTERN_C int getpeername(int socket, struct sockaddr* address, socklen_t* address_len);
EXTERN_C int socket(int domain, int type, int protocol);
EXTERN_C int getaddrinfo(const char* node, const char* service, const struct addrinfo* hints, struct addrinfo** res);
EXTERN_C int freeaddrinfo(struct addrinfo* res);
EXTERN_C int close(int fd);
EXTERN_C int ioctl(int fd, unsigned long request, ...);
EXTERN_C int epoll_create1(int flags);
EXTERN_C int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event);
EXTERN_C int epoll_pwait(int epfd, struct epoll_event* events, int maxevents, int timeout, const sigset_t* sigmask);
#undef EXTERN_C
