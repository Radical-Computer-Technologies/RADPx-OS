#ifndef RADIX_SYSROOT_NETINET_IN_H
#define RADIX_SYSROOT_NETINET_IN_H

#include <stdint.h>

typedef uint16_t sa_family_t;
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;
typedef uint32_t socklen_t;

#define AF_INET 2
#define PF_INET AF_INET
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define INADDR_ANY ((in_addr_t)0x00000000u)
#define INADDR_BROADCAST ((in_addr_t)0xffffffffu)

struct in_addr {
    in_addr_t s_addr;
};

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    unsigned char sin_zero[8];
};

uint16_t htons(uint16_t value);
uint16_t ntohs(uint16_t value);
uint32_t htonl(uint32_t value);
uint32_t ntohl(uint32_t value);

#endif
