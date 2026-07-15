#ifndef RADIX_SYSROOT_ARPA_INET_H
#define RADIX_SYSROOT_ARPA_INET_H

#include <netinet/in.h>

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int inet_pton(int af, const char *src, void *dst);
int inet_aton(const char *cp, struct in_addr *inp);

#endif
