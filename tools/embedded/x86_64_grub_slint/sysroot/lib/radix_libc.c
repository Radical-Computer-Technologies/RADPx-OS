#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include <ctype.h>
#include <getopt.h>
#include <glob.h>
#include <inttypes.h>
#include <langinfo.h>
#include <libgen.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <regex.h>
#include <search.h>
#include <strings.h>
#include <wchar.h>
#include <wctype.h>

#include <radix/syscalls.h>

int errno;
static mode_t g_process_umask = 022;

typedef struct {
    uint64_t size;
    int is_directory;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t mtime_millis;
} radix_stat_t;

typedef struct {
    unsigned char type;
    char name[256];
} radix_dirent_t;

typedef struct {
    unsigned char bytes[4];
} radix_ipv4_t;

typedef struct {
    uint16_t family;
    uint16_t port;
    radix_ipv4_t address;
    unsigned char zero[8];
} radix_sockaddr_in_t;

struct DIR {
    int fd;
    struct dirent current;
};

static int map_error(long status) {
    switch (status) {
    case -2: errno = EINVAL; break;
    case -3: errno = ENOENT; break;
    case -4: errno = ENOMEM; break;
    case -5: errno = EAGAIN; break;
    case -6: errno = ENOSYS; break;
    case -7: errno = EEXIST; break;
    default: errno = EIO; break;
    }
    return -1;
}

static long ok_or_errno(long status) {
    return status < 0 ? map_error(status) : status;
}

uint16_t htons(uint16_t value) { return (uint16_t)((value << 8u) | (value >> 8u)); }
uint16_t ntohs(uint16_t value) { return htons(value); }
uint32_t htonl(uint32_t value) {
    return ((value & 0x000000ffu) << 24u)
        | ((value & 0x0000ff00u) << 8u)
        | ((value & 0x00ff0000u) >> 8u)
        | ((value & 0xff000000u) >> 24u);
}
uint32_t ntohl(uint32_t value) { return htonl(value); }

static int sockaddr_to_radix(const struct sockaddr *src, socklen_t len, radix_sockaddr_in_t *dst) {
    if (!src || !dst || len < sizeof(struct sockaddr_in) || src->sa_family != AF_INET) {
        errno = EINVAL;
        return -1;
    }
    const struct sockaddr_in *in = (const struct sockaddr_in*)src;
    memset(dst, 0, sizeof(*dst));
    dst->family = AF_INET;
    dst->port = ntohs(in->sin_port);
    uint32_t host = ntohl(in->sin_addr.s_addr);
    dst->address.bytes[0] = (unsigned char)(host >> 24u);
    dst->address.bytes[1] = (unsigned char)(host >> 16u);
    dst->address.bytes[2] = (unsigned char)(host >> 8u);
    dst->address.bytes[3] = (unsigned char)(host);
    return 0;
}

static void sockaddr_from_radix(const radix_sockaddr_in_t *src, struct sockaddr *dst, socklen_t *len) {
    if (!src || !dst || !len || *len < sizeof(struct sockaddr_in)) return;
    struct sockaddr_in *in = (struct sockaddr_in*)dst;
    memset(in, 0, sizeof(*in));
    in->sin_family = AF_INET;
    in->sin_port = htons(src->port);
    uint32_t host = ((uint32_t)src->address.bytes[0] << 24u)
        | ((uint32_t)src->address.bytes[1] << 16u)
        | ((uint32_t)src->address.bytes[2] << 8u)
        | (uint32_t)src->address.bytes[3];
    in->sin_addr.s_addr = htonl(host);
    *len = sizeof(*in);
}

ssize_t read(int fd, void *buf, size_t count) {
    return ok_or_errno(radix_syscall6(RADIX_SYS_READ, fd, (long)buf, count, 0, 0, 0));
}

ssize_t write(int fd, const void *buf, size_t count) {
    return ok_or_errno(radix_syscall6(RADIX_SYS_WRITE, fd, (long)buf, count, 0, 0, 0));
}

int open(const char *pathname, int flags, ...) {
    return (int)ok_or_errno(radix_syscall6(RADIX_SYS_OPEN, (long)pathname, flags, 0, 0, 0, 0));
}

int openat(int dirfd, const char *pathname, int flags, ...) {
    if (dirfd == AT_FDCWD || (pathname && pathname[0] == '/')) return open(pathname, flags);
    errno = ENOSYS;
    return -1;
}

int close(int fd) {
    return (int)ok_or_errno(radix_syscall6(RADIX_SYS_CLOSE, fd, 0, 0, 0, 0, 0));
}

off_t lseek(int fd, off_t offset, int whence) {
    return ok_or_errno(radix_syscall6(RADIX_SYS_LSEEK, fd, offset, whence, 0, 0, 0));
}

static void copy_stat(struct stat *dst, const radix_stat_t *src) {
    dst->st_dev = 0;
    dst->st_ino = 0;
    dst->st_size = src->size;
    dst->st_is_directory = src->is_directory;
    dst->st_mode = src->mode;
    dst->st_nlink = 1;
    dst->st_uid = src->uid;
    dst->st_gid = src->gid;
    dst->st_blksize = 512;
    dst->st_blocks = (src->size + 511) / 512;
    dst->st_atime = (time_t)(src->mtime_millis / 1000);
    dst->st_mtime = (time_t)(src->mtime_millis / 1000);
    dst->st_ctime = (time_t)(src->mtime_millis / 1000);
    dst->st_atim.tv_sec = dst->st_atime;
    dst->st_atim.tv_nsec = (long)((src->mtime_millis % 1000u) * 1000000u);
    dst->st_mtim.tv_sec = dst->st_mtime;
    dst->st_mtim.tv_nsec = (long)((src->mtime_millis % 1000u) * 1000000u);
    dst->st_ctim.tv_sec = dst->st_ctime;
    dst->st_ctim.tv_nsec = (long)((src->mtime_millis % 1000u) * 1000000u);
    dst->st_mtime_ms = src->mtime_millis;
}

int stat(const char *pathname, struct stat *statbuf) {
    radix_stat_t st;
    long r = radix_syscall6(RADIX_SYS_STAT, (long)pathname, (long)&st, 0, 0, 0, 0);
    if (r < 0) return map_error(r);
    copy_stat(statbuf, &st);
    return 0;
}

int fstat(int fd, struct stat *statbuf) {
    radix_stat_t st;
    long r = radix_syscall6(RADIX_SYS_FSTAT, fd, (long)&st, 0, 0, 0, 0);
    if (r < 0) return map_error(r);
    copy_stat(statbuf, &st);
    return 0;
}

int fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags) {
    if (dirfd == AT_FDCWD || (pathname && pathname[0] == '/')) {
        return (flags & AT_SYMLINK_NOFOLLOW) ? lstat(pathname, statbuf) : stat(pathname, statbuf);
    }
    errno = ENOSYS;
    return -1;
}

int lstat(const char *pathname, struct stat *statbuf) {
    return stat(pathname, statbuf);
}

int ioctl(int fd, unsigned long request, ...) {
    va_list ap;
    va_start(ap, request);
    void *arg = va_arg(ap, void*);
    va_end(ap);
    return (int)ok_or_errno(radix_syscall6(RADIX_SYS_IOCTL, fd, request, (long)arg, 0, 0, 0));
}

int fcntl(int fd, int cmd, ...) {
    va_list ap;
    va_start(ap, cmd);
    long arg = va_arg(ap, long);
    va_end(ap);
    return (int)ok_or_errno(radix_syscall6(RADIX_SYS_FCNTL, fd, cmd, arg, 0, 0, 0));
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    return (int)ok_or_errno(radix_syscall6(RADIX_SYS_POLL, (long)fds, nfds, timeout, 0, 0, 0));
}

int socket(int domain, int type, int protocol) {
    return (int)ok_or_errno(radix_syscall6(RADIX_SYS_SOCKET, domain, type, protocol, 0, 0, 0));
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    radix_sockaddr_in_t radix_addr;
    if (sockaddr_to_radix(addr, addrlen, &radix_addr) < 0) return -1;
    return (int)ok_or_errno(radix_syscall6(RADIX_SYS_BIND, sockfd, (long)&radix_addr, sizeof(radix_addr), 0, 0, 0));
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    radix_sockaddr_in_t radix_addr;
    if (sockaddr_to_radix(addr, addrlen, &radix_addr) < 0) return -1;
    return (int)ok_or_errno(radix_syscall6(RADIX_SYS_CONNECT, sockfd, (long)&radix_addr, sizeof(radix_addr), 0, 0, 0));
}

int listen(int sockfd, int backlog) {
    return (int)ok_or_errno(radix_syscall6(RADIX_SYS_LISTEN, sockfd, backlog, 0, 0, 0, 0));
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    radix_sockaddr_in_t radix_addr;
    size_t radix_len = sizeof(radix_addr);
    long r = radix_syscall6(RADIX_SYS_ACCEPT, sockfd, (long)(addr ? &radix_addr : 0), (long)(addrlen ? &radix_len : 0), 0, 0, 0);
    if (r < 0) return map_error(r);
    if (addr && addrlen) sockaddr_from_radix(&radix_addr, addr, addrlen);
    return (int)r;
}

int shutdown(int sockfd, int how) {
    return (int)ok_or_errno(radix_syscall6(RADIX_SYS_SHUTDOWN, sockfd, how, 0, 0, 0, 0));
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    radix_sockaddr_in_t radix_addr;
    if (sockaddr_to_radix(dest_addr, addrlen, &radix_addr) < 0) return -1;
    return ok_or_errno(radix_syscall6(RADIX_SYS_SENDTO, sockfd, (long)buf, len, flags, (long)&radix_addr, sizeof(radix_addr)));
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    radix_sockaddr_in_t radix_addr;
    size_t radix_len = sizeof(radix_addr);
    long r = radix_syscall6(RADIX_SYS_RECVFROM, sockfd, (long)buf, len, flags, (long)(src_addr ? &radix_addr : 0), (long)(addrlen ? &radix_len : 0));
    if (r < 0) return map_error(r);
    if (src_addr && addrlen) sockaddr_from_radix(&radix_addr, src_addr, addrlen);
    return r;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    return ok_or_errno(radix_syscall6(RADIX_SYS_SENDTO, sockfd, (long)buf, len, flags, 0, 0));
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return ok_or_errno(radix_syscall6(RADIX_SYS_RECVFROM, sockfd, (long)buf, len, flags, 0, 0));
}

static int parse_ipv4_host_order(const char *src, uint32_t *out) {
    if (!src || !out) return 0;
    uint32_t octets[4] = {0, 0, 0, 0};
    const char *p = src;
    for (int i = 0; i < 4; ++i) {
        if (!*p || *p < '0' || *p > '9') return 0;
        uint32_t value = 0;
        int digits = 0;
        while (*p >= '0' && *p <= '9') {
            value = value * 10u + (uint32_t)(*p - '0');
            if (value > 255u) return 0;
            ++p;
            ++digits;
        }
        if (digits == 0) return 0;
        octets[i] = value;
        if (i < 3) {
            if (*p != '.') return 0;
            ++p;
        }
    }
    if (*p) return 0;
    *out = (octets[0] << 24u) | (octets[1] << 16u) | (octets[2] << 8u) | octets[3];
    return 1;
}

int inet_aton(const char *cp, struct in_addr *inp) {
    uint32_t host = 0;
    if (!parse_ipv4_host_order(cp, &host)) return 0;
    if (inp) inp->s_addr = htonl(host);
    return 1;
}

int inet_pton(int af, const char *src, void *dst) {
    if (af != AF_INET || !dst) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    struct in_addr addr;
    if (!inet_aton(src, &addr)) return 0;
    memcpy(dst, &addr, sizeof(addr));
    return 1;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    if (af != AF_INET || !src || !dst) {
        errno = EAFNOSUPPORT;
        return 0;
    }
    if (size < 16) {
        errno = ENOSPC;
        return 0;
    }
    uint32_t host = ntohl(((const struct in_addr*)src)->s_addr);
    snprintf(dst, size, "%u.%u.%u.%u",
        (unsigned)((host >> 24u) & 0xffu),
        (unsigned)((host >> 16u) & 0xffu),
        (unsigned)((host >> 8u) & 0xffu),
        (unsigned)(host & 0xffu));
    return dst;
}

static uint64_t resolver_now_millis(void) {
    struct timeval tv;
    long status = radix_syscall6(RADIX_SYS_GETTIMEOFDAY, (long)&tv, 0, 0, 0, 0, 0);
    if (status < 0) return 0;
    return (uint64_t)tv.tv_sec * 1000u + (uint64_t)(tv.tv_usec / 1000u);
}

static void resolver_sleep_ms(uint32_t ms) {
    radix_syscall6(RADIX_SYS_NANOSLEEP, (long)ms * 1000000L, 0, 0, 0, 0, 0);
}

static int resolver_read_file(const char *path, char *buffer, size_t size) {
    if (!path || !buffer || size == 0) return -1;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buffer, size - 1);
    close(fd);
    if (n < 0) return -1;
    buffer[n] = 0;
    return (int)n;
}

static int resolver_hosts_lookup(const char *name, struct in_addr *addr) {
    char text[1024];
    if (!name || !addr || resolver_read_file("/etc/hosts", text, sizeof(text)) < 0) return 0;
    char *line = text;
    while (*line) {
        char *next = line;
        while (*next && *next != '\n') ++next;
        if (*next) *next++ = 0;
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;
        while (*line == ' ' || *line == '\t') ++line;
        if (!*line) {
            line = next;
            continue;
        }
        char *ip = line;
        while (*line && *line != ' ' && *line != '\t') ++line;
        if (*line) *line++ = 0;
        struct in_addr parsed;
        if (!inet_aton(ip, &parsed)) {
            line = next;
            continue;
        }
        while (*line) {
            while (*line == ' ' || *line == '\t') ++line;
            char *host = line;
            while (*line && *line != ' ' && *line != '\t') ++line;
            if (*line) *line++ = 0;
            if (*host && strcasecmp(host, name) == 0) {
                *addr = parsed;
                return 1;
            }
        }
        line = next;
    }
    return 0;
}

static struct in_addr resolver_nameserver(void) {
    struct in_addr fallback;
    inet_aton("10.0.2.3", &fallback);
    char text[512];
    if (resolver_read_file("/etc/resolv.conf", text, sizeof(text)) < 0) return fallback;
    char *line = text;
    while (*line) {
        char *next = line;
        while (*next && *next != '\n') ++next;
        if (*next) *next++ = 0;
        while (*line == ' ' || *line == '\t') ++line;
        if (strncmp(line, "nameserver", 10) == 0 && (line[10] == ' ' || line[10] == '\t')) {
            line += 10;
            while (*line == ' ' || *line == '\t') ++line;
            char ip[32];
            size_t pos = 0;
            while (line[pos] && line[pos] != ' ' && line[pos] != '\t' && pos + 1 < sizeof(ip)) {
                ip[pos] = line[pos];
                ++pos;
            }
            ip[pos] = 0;
            struct in_addr parsed;
            if (inet_aton(ip, &parsed)) return parsed;
        }
        line = next;
    }
    return fallback;
}

static int dns_put_name(uint8_t *packet, size_t size, size_t *pos, const char *name) {
    const char *label = name;
    while (*label) {
        const char *dot = strchr(label, '.');
        size_t len = dot ? (size_t)(dot - label) : strlen(label);
        if (len == 0 || len > 63 || *pos + len + 1 >= size) return 0;
        packet[(*pos)++] = (uint8_t)len;
        memcpy(packet + *pos, label, len);
        *pos += len;
        if (!dot) break;
        label = dot + 1;
    }
    if (*pos >= size) return 0;
    packet[(*pos)++] = 0;
    return 1;
}

static int dns_skip_name(const uint8_t *packet, size_t size, size_t *pos) {
    size_t p = *pos;
    for (unsigned hops = 0; hops < 64; ++hops) {
        if (p >= size) return 0;
        uint8_t len = packet[p++];
        if (len == 0) {
            *pos = p;
            return 1;
        }
        if ((len & 0xc0u) == 0xc0u) {
            if (p >= size) return 0;
            *pos = p + 1;
            return 1;
        }
        if ((len & 0xc0u) != 0 || p + len > size) return 0;
        p += len;
    }
    return 0;
}

static uint16_t read_be16_libc(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8u) | p[1]);
}

static uint32_t read_be32_libc(const uint8_t *p) {
    return ((uint32_t)p[0] << 24u) | ((uint32_t)p[1] << 16u) | ((uint32_t)p[2] << 8u) | (uint32_t)p[3];
}

static void write_be16_libc(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8u);
    p[1] = (uint8_t)value;
}

static int resolver_dns_lookup_a(const char *name, struct in_addr *addr) {
    if (!name || !addr) return EAI_NONAME;
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) return EAI_AGAIN;
    struct sockaddr_in dns;
    memset(&dns, 0, sizeof(dns));
    dns.sin_family = AF_INET;
    dns.sin_port = htons(53);
    dns.sin_addr = resolver_nameserver();
    uint8_t query[512];
    memset(query, 0, sizeof(query));
    uint16_t txid = (uint16_t)(resolver_now_millis() ^ 0x5244u);
    write_be16_libc(query + 0, txid);
    write_be16_libc(query + 2, 0x0100u);
    write_be16_libc(query + 4, 1u);
    size_t qlen = 12;
    if (!dns_put_name(query, sizeof(query), &qlen, name) || qlen + 4 > sizeof(query)) {
        close(fd);
        return EAI_NONAME;
    }
    write_be16_libc(query + qlen, 1u);
    qlen += 2;
    write_be16_libc(query + qlen, 1u);
    qlen += 2;
    if (sendto(fd, query, qlen, 0, (struct sockaddr*)&dns, sizeof(dns)) != (ssize_t)qlen) {
        close(fd);
        return EAI_AGAIN;
    }
    uint8_t response[512];
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    ssize_t received = 0;
    uint64_t start = resolver_now_millis();
    for (unsigned attempt = 0; attempt < 2000; ++attempt) {
        from_len = sizeof(from);
        received = recvfrom(fd, response, sizeof(response), 0, (struct sockaddr*)&from, &from_len);
        if (received > 0) break;
        if (received < 0 && errno != EAGAIN) {
            close(fd);
            return EAI_AGAIN;
        }
        if (attempt > 16 && start && resolver_now_millis() - start > 1000u) break;
        resolver_sleep_ms(1);
    }
    close(fd);
    if (received < 12) return EAI_AGAIN;
    size_t rlen = (size_t)received;
    if (read_be16_libc(response) != txid) return EAI_FAIL;
    uint16_t flags = read_be16_libc(response + 2);
    if ((flags & 0x000fu) != 0) return EAI_NONAME;
    uint16_t qd = read_be16_libc(response + 4);
    uint16_t an = read_be16_libc(response + 6);
    size_t pos = 12;
    for (uint16_t i = 0; i < qd; ++i) {
        if (!dns_skip_name(response, rlen, &pos) || pos + 4 > rlen) return EAI_FAIL;
        pos += 4;
    }
    for (uint16_t i = 0; i < an; ++i) {
        if (!dns_skip_name(response, rlen, &pos) || pos + 10 > rlen) return EAI_FAIL;
        uint16_t type = read_be16_libc(response + pos);
        uint16_t klass = read_be16_libc(response + pos + 2);
        (void)read_be32_libc(response + pos + 4);
        uint16_t rdlen = read_be16_libc(response + pos + 8);
        pos += 10;
        if (pos + rdlen > rlen) return EAI_FAIL;
        if (type == 1u && klass == 1u && rdlen == 4u) {
            uint32_t host = ((uint32_t)response[pos] << 24u)
                | ((uint32_t)response[pos + 1] << 16u)
                | ((uint32_t)response[pos + 2] << 8u)
                | (uint32_t)response[pos + 3];
            addr->s_addr = htonl(host);
            return 0;
        }
        pos += rdlen;
    }
    return EAI_NONAME;
}

static int resolver_lookup_ipv4(const char *name, struct in_addr *addr) {
    if (inet_aton(name, addr)) return 0;
    if (resolver_hosts_lookup(name, addr)) return 0;
    return resolver_dns_lookup_a(name, addr);
}

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
    if (!res) return EAI_FAIL;
    *res = 0;
    if (!node && !(hints && (hints->ai_flags & AI_PASSIVE))) return EAI_NONAME;
    if (hints && hints->ai_family != 0 && hints->ai_family != AF_INET) return EAI_FAMILY;
    if (hints && hints->ai_socktype != 0 && hints->ai_socktype != SOCK_DGRAM && hints->ai_socktype != SOCK_STREAM) return EAI_FAIL;
    int socktype = hints && hints->ai_socktype ? hints->ai_socktype : SOCK_STREAM;
    int protocol = hints && hints->ai_protocol ? hints->ai_protocol : (socktype == SOCK_DGRAM ? IPPROTO_UDP : IPPROTO_TCP);
    int port = 0;
    if (service && *service) {
        if (strcmp(service, "ntp") == 0) port = 123;
        else {
            char *end = 0;
            long parsed = strtol(service, &end, 10);
            if (!end || *end || parsed < 0 || parsed > 65535) return EAI_SERVICE;
            port = (int)parsed;
        }
    }
    struct in_addr address;
    if (!node) address.s_addr = htonl(INADDR_ANY);
    else {
        int status = resolver_lookup_ipv4(node, &address);
        if (status != 0) return status;
    }
    struct addrinfo *ai = (struct addrinfo*)calloc(1, sizeof(*ai));
    struct sockaddr_in *sa = (struct sockaddr_in*)calloc(1, sizeof(*sa));
    if (!ai || !sa) {
        free(ai);
        free(sa);
        return EAI_MEMORY;
    }
    sa->sin_family = AF_INET;
    sa->sin_port = htons((uint16_t)port);
    sa->sin_addr = address;
    ai->ai_family = AF_INET;
    ai->ai_socktype = socktype;
    ai->ai_protocol = protocol;
    ai->ai_addrlen = sizeof(*sa);
    ai->ai_addr = (struct sockaddr*)sa;
    if (hints && (hints->ai_flags & AI_CANONNAME) && node) ai->ai_canonname = strdup(node);
    *res = ai;
    return 0;
}

void freeaddrinfo(struct addrinfo *res) {
    while (res) {
        struct addrinfo *next = res->ai_next;
        free(res->ai_addr);
        free(res->ai_canonname);
        free(res);
        res = next;
    }
}

const char *gai_strerror(int ecode) {
    switch (ecode) {
    case 0: return "success";
    case EAI_BADFLAGS: return "bad flags";
    case EAI_NONAME: return "name not known";
    case EAI_AGAIN: return "temporary failure";
    case EAI_FAIL: return "resolver failure";
    case EAI_FAMILY: return "address family not supported";
    case EAI_MEMORY: return "out of memory";
    case EAI_SERVICE: return "service not supported";
    default: return "resolver error";
    }
}

struct hostent *gethostbyname(const char *name) {
    static struct hostent host;
    static char *aliases[1];
    static char *addr_list[2];
    static struct in_addr address;
    static char host_name[256];
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    struct addrinfo *result = 0;
    int status = getaddrinfo(name, 0, &hints, &result);
    if (status != 0 || !result || !result->ai_addr) {
        freeaddrinfo(result);
        errno = EAGAIN;
        return 0;
    }
    const struct sockaddr_in *sa = (const struct sockaddr_in*)result->ai_addr;
    address = sa->sin_addr;
    strncpy(host_name, name ? name : "", sizeof(host_name) - 1);
    host_name[sizeof(host_name) - 1] = 0;
    aliases[0] = 0;
    addr_list[0] = (char*)&address;
    addr_list[1] = 0;
    host.h_name = host_name;
    host.h_aliases = aliases;
    host.h_addrtype = AF_INET;
    host.h_length = sizeof(address);
    host.h_addr_list = addr_list;
    freeaddrinfo(result);
    return &host;
}

int tcgetattr(int fd, struct termios *termios_p) {
    if (!termios_p) { errno = EINVAL; return -1; }
    long r = radix_syscall6(RADIX_SYS_IOCTL, fd, RADIX_IOR('Y', 5u, struct termios), (long)termios_p, 0, 0, 0);
    if (r < 0) return map_error(r);
    return 0;
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    if (!termios_p) { errno = EINVAL; return -1; }
    if (optional_actions != TCSANOW && optional_actions != TCSADRAIN && optional_actions != TCSAFLUSH) {
        errno = EINVAL;
        return -1;
    }
    int status = (int)ok_or_errno(radix_syscall6(RADIX_SYS_IOCTL, fd, RADIX_IOW('Y', 6u, struct termios), (long)termios_p, 0, 0, 0));
    if (status < 0) return status;
    if (optional_actions == TCSAFLUSH) return tcflush(fd, TCIFLUSH);
    return 0;
}

int tcflush(int fd, int queue_selector) {
    uint32_t queues = 0;
    if (queue_selector == TCIFLUSH) queues = 1u;
    else if (queue_selector == TCOFLUSH) queues = 2u;
    else if (queue_selector == TCIOFLUSH) queues = 3u;
    else { errno = EINVAL; return -1; }
    return (int)ok_or_errno(radix_syscall6(RADIX_SYS_IOCTL, fd, RADIX_IOW('Y', 7u, uint32_t), (long)&queues, 0, 0, 0));
}

void cfmakeraw(struct termios *termios_p) {
    speed_t ispeed = termios_p->c_ispeed;
    speed_t ospeed = termios_p->c_ospeed;
    termios_p->c_iflag = 0;
    termios_p->c_oflag = 0;
    termios_p->c_lflag = 0;
    termios_p->c_ispeed = ispeed ? ispeed : B9600;
    termios_p->c_ospeed = ospeed ? ospeed : B9600;
    termios_p->c_cc[VMIN] = 1;
    termios_p->c_cc[VTIME] = 0;
}

speed_t cfgetispeed(const struct termios *termios_p) { return termios_p ? termios_p->c_ispeed : B9600; }
speed_t cfgetospeed(const struct termios *termios_p) { return termios_p ? termios_p->c_ospeed : B9600; }
int cfsetispeed(struct termios *termios_p, speed_t speed) { if (!termios_p) { errno = EINVAL; return -1; } termios_p->c_ispeed = speed; return 0; }
int cfsetospeed(struct termios *termios_p, speed_t speed) { if (!termios_p) { errno = EINVAL; return -1; } termios_p->c_ospeed = speed; return 0; }

int chdir(const char *path) { return (int)ok_or_errno(radix_syscall6(RADIX_SYS_CHDIR, (long)path, 0, 0, 0, 0, 0)); }
int fchdir(int fd) { (void)fd; errno = ENOSYS; return -1; }
char *getcwd(char *buf, size_t size) { return radix_syscall6(RADIX_SYS_GETCWD, (long)buf, size, 0, 0, 0, 0) < 0 ? 0 : buf; }
char *getlogin(void) { return "root"; }
int getlogin_r(char *buf, size_t buflen) {
    const char *name = "root";
    size_t n = strlen(name) + 1u;
    if (!buf || buflen < n) { errno = ERANGE; return ERANGE; }
    memcpy(buf, name, n);
    return 0;
}
int isatty(int fd) { return (int)ok_or_errno(radix_syscall6(RADIX_SYS_ISATTY, fd, 0, 0, 0, 0, 0)); }
int pipe(int pipefd[2]) { return (int)ok_or_errno(radix_syscall6(RADIX_SYS_PIPE, (long)pipefd, 0, 0, 0, 0, 0)); }
int dup(int oldfd) { return (int)ok_or_errno(radix_syscall6(RADIX_SYS_DUP, oldfd, 0, 0, 0, 0, 0)); }
int dup2(int oldfd, int newfd) { return (int)ok_or_errno(radix_syscall6(RADIX_SYS_DUP2, oldfd, newfd, 0, 0, 0, 0)); }
int getdtablesize(void) { return 256; }
pid_t fork(void) { return (pid_t)ok_or_errno(radix_syscall6(RADIX_SYS_FORK, 0, 0, 0, 0, 0, 0)); }
int execve(const char *pathname, char *const argv[], char *const envp[]) { (void)envp; return (int)ok_or_errno(radix_syscall6(RADIX_SYS_EXECVE, (long)pathname, (long)argv, 0, 0, 0, 0)); }
static int exec_path_candidate(const char *prefix, const char *file, char *const argv[]) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", prefix, file);
    return execve(path, argv, 0);
}
int execvp(const char *file, char *const argv[]) {
    if (!file) { errno = EINVAL; return -1; }
    if (strchr(file, '/')) return execve(file, argv, 0);
    if (exec_path_candidate("/bin", file, argv) == 0) return 0;
    return exec_path_candidate("/usr/bin", file, argv);
}
int execl(const char *pathname, const char *arg, ...) {
    char *argv[32];
    int argc = 0;
    argv[argc++] = (char*)arg;
    va_list ap;
    va_start(ap, arg);
    while (argc < 31) {
        char *next = va_arg(ap, char*);
        argv[argc++] = next;
        if (!next) break;
    }
    va_end(ap);
    if (argc == 31) argv[31] = 0;
    return execve(pathname, argv, 0);
}
int execlp(const char *file, const char *arg, ...) {
    char *argv[32];
    int argc = 0;
    argv[argc++] = (char*)arg;
    va_list ap;
    va_start(ap, arg);
    while (argc < 31) {
        char *next = va_arg(ap, char*);
        argv[argc++] = next;
        if (!next) break;
    }
    va_end(ap);
    if (argc == 31) argv[31] = 0;
    return execvp(file, argv);
}
long fpathconf(int fd, int name) { (void)fd; return name == _PC_PIPE_BUF ? PIPE_BUF : -1; }
pid_t waitpid(pid_t pid, int *wstatus, int options) { return (pid_t)ok_or_errno(radix_syscall6(RADIX_SYS_WAITPID, pid, (long)wstatus, options, 0, 0, 0)); }
pid_t wait(int *wstatus) { return waitpid(-1, wstatus, 0); }
pid_t getpid(void) { return (pid_t)radix_syscall6(RADIX_SYS_GETPID, 0, 0, 0, 0, 0, 0); }
pid_t getppid(void) { return (pid_t)radix_syscall6(RADIX_SYS_GETPPID, 0, 0, 0, 0, 0, 0); }
uid_t getuid(void) { return (uid_t)radix_syscall6(RADIX_SYS_GETUID, 0, 0, 0, 0, 0, 0); }
uid_t geteuid(void) { return (uid_t)radix_syscall6(RADIX_SYS_GETEUID, 0, 0, 0, 0, 0, 0); }
gid_t getgid(void) { return (gid_t)radix_syscall6(RADIX_SYS_GETGID, 0, 0, 0, 0, 0, 0); }
gid_t getegid(void) { return (gid_t)radix_syscall6(RADIX_SYS_GETEGID, 0, 0, 0, 0, 0, 0); }
int gethostname(char *name, size_t len) {
    const char *host = "radix";
    size_t n = strlen(host) + 1u;
    if (!name || len == 0) { errno = EINVAL; return -1; }
    if (n > len) { errno = ENAMETOOLONG; return -1; }
    memcpy(name, host, n);
    return 0;
}
int access(const char *pathname, int mode) {
    if (mode & ~(R_OK | W_OK | X_OK)) { errno = EINVAL; return -1; }
    struct stat st;
    if (stat(pathname, &st) < 0) return -1;
    if (mode == F_OK) return 0;
    uid_t uid = geteuid();
    gid_t gid = getegid();
    mode_t readable = S_IROTH;
    mode_t writable = S_IWOTH;
    mode_t executable = S_IXOTH;
    if (uid == st.st_uid) {
        readable = S_IRUSR;
        writable = S_IWUSR;
        executable = S_IXUSR;
    } else if (gid == st.st_gid) {
        readable = S_IRGRP;
        writable = S_IWGRP;
        executable = S_IXGRP;
    }
    if ((mode & R_OK) && !(st.st_mode & readable)) { errno = EACCES; return -1; }
    if ((mode & W_OK) && !(st.st_mode & writable)) { errno = EACCES; return -1; }
    if ((mode & X_OK) && !(st.st_mode & executable)) { errno = EACCES; return -1; }
    return 0;
}
int unlink(const char *pathname) { return (int)ok_or_errno(radix_syscall6(RADIX_SYS_REMOVE, (long)pathname, 0, 0, 0, 0, 0)); }
int rmdir(const char *pathname) { return (int)ok_or_errno(radix_syscall6(RADIX_SYS_RMDIR, (long)pathname, 0, 0, 0, 0, 0)); }
int mkdir(const char *pathname, mode_t mode) {
    int r = (int)ok_or_errno(radix_syscall6(RADIX_SYS_MKDIR, (long)pathname, 0, 0, 0, 0, 0));
    if (r == 0 && mode) (void)chmod(pathname, mode & ~g_process_umask);
    return r;
}
int chmod(const char *pathname, mode_t mode) { return (int)ok_or_errno(radix_syscall6(RADIX_SYS_CHMOD, (long)pathname, mode, 0, 0, 0, 0)); }
mode_t umask(mode_t mask) {
    mode_t previous = g_process_umask;
    g_process_umask = mask & 0777;
    return previous;
}
int symlink(const char *target, const char *linkpath) { return (int)ok_or_errno(radix_syscall6(RADIX_SYS_SYMLINK, (long)target, (long)linkpath, 0, 0, 0, 0)); }
int link(const char *oldpath, const char *newpath) { return (int)ok_or_errno(radix_syscall6(RADIX_SYS_LINK, (long)oldpath, (long)newpath, 0, 0, 0, 0)); }
ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) { return ok_or_errno(radix_syscall6(RADIX_SYS_READLINK, (long)pathname, (long)buf, bufsiz, 0, 0, 0)); }
int utime(const char *filename, const struct utimbuf *times) { (void)filename; (void)times; errno = ENOSYS; return -1; }
int fsync(int fd) { return (int)ok_or_errno(radix_syscall6(RADIX_SYS_FSYNC, fd, 0, 0, 0, 0, 0)); }
int nanosleep(const struct timespec *req, struct timespec *rem) { (void)rem; if (!req || req->tv_sec < 0 || req->tv_nsec < 0 || req->tv_nsec >= 1000000000L) { errno = EINVAL; return -1; } return (int)ok_or_errno(radix_syscall6(RADIX_SYS_NANOSLEEP, req->tv_sec * 1000000000L + req->tv_nsec, 0, 0, 0, 0, 0)); }
unsigned int sleep(unsigned int seconds) { struct timespec ts = {(long)seconds, 0}; return nanosleep(&ts, 0) == 0 ? 0 : seconds; }
int gettimeofday(struct timeval *tv, void *tz) { (void)tz; return (int)ok_or_errno(radix_syscall6(RADIX_SYS_GETTIMEOFDAY, (long)tv, 0, 0, 0, 0, 0)); }
int settimeofday(const struct timeval *tv, const void *tz) { (void)tz; if (!tv) { errno = EINVAL; return -1; } return (int)ok_or_errno(radix_syscall6(RADIX_SYS_SETTIMEOFDAY, (long)tv, 0, 0, 0, 0, 0)); }
time_t time(time_t *tloc) { struct timeval tv; if (gettimeofday(&tv, 0) < 0) return (time_t)-1; if (tloc) *tloc = tv.tv_sec; return tv.tv_sec; }
clock_t clock(void) { struct timeval tv; if (gettimeofday(&tv, 0) < 0) return (clock_t)-1; return tv.tv_sec * CLOCKS_PER_SEC + tv.tv_usec; }
struct tm *localtime(const time_t *timep) {
    static struct tm tm;
    time_t t = timep ? *timep : 0;
    if (t < 0) t = 0;
    tm.tm_sec = (int)(t % 60);
    tm.tm_min = (int)((t / 60) % 60);
    tm.tm_hour = (int)((t / 3600) % 24);
    tm.tm_mday = 1 + (int)((t / 86400) % 31);
    tm.tm_mon = 0;
    tm.tm_year = 70;
    tm.tm_wday = (int)((4 + t / 86400) % 7);
    tm.tm_yday = (int)((t / 86400) % 365);
    tm.tm_isdst = 0;
    return &tm;
}

static struct sigaction g_signal_actions[NSIG];
static int g_signal_actions_initialized;
static sigset_t g_signal_mask;

static void init_signal_actions(void) {
    if (g_signal_actions_initialized) return;
    for (int i = 0; i < NSIG; ++i) {
        g_signal_actions[i].sa_handler = SIG_DFL;
        g_signal_actions[i].sa_mask = 0;
        g_signal_actions[i].sa_flags = 0;
    }
    g_signal_actions_initialized = 1;
}

sighandler_t signal(int signum, sighandler_t handler) {
    if (signum <= 0 || signum >= NSIG || handler == SIG_ERR) { errno = EINVAL; return SIG_ERR; }
    init_signal_actions();
    sighandler_t old = g_signal_actions[signum].sa_handler;
    g_signal_actions[signum].sa_handler = handler;
    g_signal_actions[signum].sa_mask = 0;
    g_signal_actions[signum].sa_flags = 0;
    return old;
}

int kill(int pid, int sig) {
    if (pid <= 0 || sig <= 0 || sig >= NSIG) { errno = EINVAL; return -1; }
    return (int)ok_or_errno(radix_syscall6(RADIX_SYS_KILL, pid, sig, 0, 0, 0, 0));
}
int raise(int sig) {
    if (sig <= 0 || sig >= NSIG) { errno = EINVAL; return -1; }
    init_signal_actions();
    sighandler_t handler = g_signal_actions[sig].sa_handler;
    if (handler == SIG_IGN) return 0;
    if (handler && handler != SIG_DFL && handler != SIG_ERR) {
        handler(sig);
        return 0;
    }
    return kill(getpid(), sig);
}
int sigemptyset(sigset_t *set) { if (!set) { errno = EINVAL; return -1; } *set = 0; return 0; }
int sigfillset(sigset_t *set) { if (!set) { errno = EINVAL; return -1; } *set = ~0ul; return 0; }
int sigaddset(sigset_t *set, int signum) {
    if (!set || signum <= 0 || signum >= NSIG) { errno = EINVAL; return -1; }
    *set |= 1ul << signum;
    return 0;
}
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    if (oldset) *oldset = g_signal_mask;
    if (!set) return 0;
    if (how == SIG_BLOCK) g_signal_mask |= *set;
    else if (how == SIG_UNBLOCK) g_signal_mask &= ~(*set);
    else if (how == SIG_SETMASK) g_signal_mask = *set;
    else { errno = EINVAL; return -1; }
    return 0;
}
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    if (signum <= 0 || signum >= NSIG) { errno = EINVAL; return -1; }
    init_signal_actions();
    if (oldact) *oldact = g_signal_actions[signum];
    if (act) g_signal_actions[signum] = *act;
    return 0;
}

DIR *opendir(const char *name) {
    int fd = open(name, O_RDONLY | O_DIRECTORY);
    if (fd < 0) return 0;
    static DIR dirs[4];
    for (int i = 0; i < 4; ++i) {
        if (dirs[i].fd == 0) {
            dirs[i].fd = fd;
            return &dirs[i];
        }
    }
    close(fd);
    errno = EBUSY;
    return 0;
}

struct dirent *readdir(DIR *dirp) {
    if (!dirp || dirp->fd <= 0) {
        errno = EBADF;
        return 0;
    }
    radix_dirent_t ent;
    long n = radix_syscall6(RADIX_SYS_GETDENTS, dirp->fd, (long)&ent, 1, 0, 0, 0);
    if (n <= 0) return 0;
    dirp->current.d_type = ent.type;
    for (size_t i = 0; i < sizeof(dirp->current.d_name); ++i) {
        dirp->current.d_name[i] = ent.name[i];
        if (!ent.name[i]) break;
    }
    return &dirp->current;
}

void rewinddir(DIR *dirp) {
    if (dirp && dirp->fd > 0) (void)lseek(dirp->fd, 0, SEEK_SET);
}

int closedir(DIR *dirp) {
    if (!dirp || dirp->fd <= 0) {
        errno = EBADF;
        return -1;
    }
    int fd = dirp->fd;
    dirp->fd = 0;
    return close(fd);
}

int dirfd(DIR *dirp) {
    return dirp ? dirp->fd : -1;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    if (d < s) {
        for (size_t i = 0; i < n; ++i) d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; --i) d[i - 1] = s[i - 1];
    }
    return dest;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char*)s;
    for (size_t i = 0; i < n; ++i) p[i] = (unsigned char)c;
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *a = (const unsigned char*)s1;
    const unsigned char *b = (const unsigned char*)s2;
    for (size_t i = 0; i < n; ++i) if (a[i] != b[i]) return (int)a[i] - (int)b[i];
    return 0;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char*)s;
    for (size_t i = 0; i < n; ++i) {
        if (p[i] == (unsigned char)c) return (void*)(p + i);
    }
    return 0;
}

void *memrchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char*)s;
    for (size_t i = n; i > 0; --i) {
        if (p[i - 1] == (unsigned char)c) return (void*)(p + i - 1);
    }
    return 0;
}

void *mempcpy(void *dest, const void *src, size_t n) {
    memcpy(dest, src, n);
    return (unsigned char*)dest + n;
}

void *rawmemchr(const void *s, int c) {
    const unsigned char *p = (const unsigned char*)s;
    while (*p != (unsigned char)c) ++p;
    return (void*)p;
}

size_t strlen(const char *s) { size_t n = 0; if (s) while (s[n]) ++n; return n; }
size_t strnlen(const char *s, size_t maxlen) { size_t n = 0; if (s) while (n < maxlen && s[n]) ++n; return n; }
char *strcpy(char *dest, const char *src) { char *d = dest; while ((*d++ = *src++)) {} return dest; }
char *strncpy(char *dest, const char *src, size_t n) { size_t i = 0; for (; i < n && src[i]; ++i) dest[i] = src[i]; for (; i < n; ++i) dest[i] = 0; return dest; }
char *strcat(char *dest, const char *src) { strcpy(dest + strlen(dest), src); return dest; }
char *strncat(char *dest, const char *src, size_t n) { size_t d = strlen(dest); size_t i = 0; for (; i < n && src[i]; ++i) dest[d + i] = src[i]; dest[d + i] = 0; return dest; }
int strcmp(const char *s1, const char *s2) { while (*s1 && *s1 == *s2) { ++s1; ++s2; } return (unsigned char)*s1 - (unsigned char)*s2; }
int strncmp(const char *s1, const char *s2, size_t n) { for (size_t i = 0; i < n; ++i) { if (s1[i] != s2[i] || !s1[i]) return (unsigned char)s1[i] - (unsigned char)s2[i]; } return 0; }
int strcoll(const char *s1, const char *s2) { return strcmp(s1, s2); }
size_t strspn(const char *s, const char *accept) {
    size_t n = 0;
    for (; s[n]; ++n) {
        if (!strchr(accept, s[n])) break;
    }
    return n;
}
size_t strcspn(const char *s, const char *reject) {
    size_t n = 0;
    for (; s[n]; ++n) {
        if (strchr(reject, s[n])) break;
    }
    return n;
}
char *strchr(const char *s, int c) { while (*s) { if (*s == (char)c) return (char*)s; ++s; } return c == 0 ? (char*)s : 0; }
char *strrchr(const char *s, int c) { const char *last = 0; do { if (*s == (char)c) last = s; } while (*s++); return (char*)last; }
char *strpbrk(const char *s, const char *accept) {
    for (; s && *s; ++s) {
        if (strchr(accept, *s)) return (char*)s;
    }
    return 0;
}
char *strstr(const char *haystack, const char *needle) {
    if (!needle || !*needle) return (char*)haystack;
    size_t n = strlen(needle);
    for (const char *p = haystack; p && *p; ++p) {
        if (strncmp(p, needle, n) == 0) return (char*)p;
    }
    return 0;
}
char *strcasestr(const char *haystack, const char *needle) {
    if (!needle || !*needle) return (char*)haystack;
    size_t n = strlen(needle);
    for (const char *p = haystack; p && *p; ++p) {
        if (strncasecmp(p, needle, n) == 0) return (char*)p;
    }
    return 0;
}
char *strtok(char *str, const char *delim) {
    static char *saved;
    char *p = str ? str : saved;
    if (!p || !delim) return 0;
    p += strspn(p, delim);
    if (!*p) { saved = 0; return 0; }
    char *end = p + strcspn(p, delim);
    if (*end) {
        *end = 0;
        saved = end + 1;
    } else {
        saved = 0;
    }
    return p;
}

int isdigit(int c) { return c >= '0' && c <= '9'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int isalpha(int c) { return islower(c) || isupper(c); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isblank(int c) { return c == ' ' || c == '\t'; }
int iscntrl(int c) { unsigned char ch = (unsigned char)c; return ch < 0x20 || ch == 0x7f; }
int isgraph(int c) { unsigned char ch = (unsigned char)c; return ch > 0x20 && ch < 0x7f; }
int isprint(int c) { unsigned char ch = (unsigned char)c; return ch >= 0x20 && ch < 0x7f; }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int ispunct(int c) { return isgraph(c) && !isalnum(c); }
int tolower(int c) { return isupper(c) ? c + ('a' - 'A') : c; }
int toupper(int c) { return islower(c) ? c - ('a' - 'A') : c; }

typedef struct heap_header {
    size_t size;
    int free;
    struct heap_header *next;
} heap_header_t;

static unsigned char g_heap[2 * 1024 * 1024];
static heap_header_t *g_heap_head;

static void heap_init(void) {
    if (g_heap_head) return;
    g_heap_head = (heap_header_t*)g_heap;
    g_heap_head->size = sizeof(g_heap) - sizeof(heap_header_t);
    g_heap_head->free = 1;
    g_heap_head->next = 0;
}

void *malloc(size_t size) {
    if (!size) return 0;
    heap_init();
    size = (size + 15u) & ~15u;
    for (heap_header_t *h = g_heap_head; h; h = h->next) {
        if (!h->free || h->size < size) continue;
        if (h->size > size + sizeof(heap_header_t) + 16u) {
            heap_header_t *next = (heap_header_t*)((unsigned char*)(h + 1) + size);
            next->size = h->size - size - sizeof(heap_header_t);
            next->free = 1;
            next->next = h->next;
            h->next = next;
            h->size = size;
        }
        h->free = 0;
        return h + 1;
    }
    errno = ENOMEM;
    return 0;
}

void free(void *ptr) {
    if (!ptr) return;
    heap_header_t *h = ((heap_header_t*)ptr) - 1;
    h->free = 1;
    for (heap_header_t *p = g_heap_head; p; p = p->next) {
        while (p->next && p->free && p->next->free) {
            p->size += sizeof(heap_header_t) + p->next->size;
            p->next = p->next->next;
        }
    }
}

void *calloc(size_t nmemb, size_t size) {
    if (size && nmemb > ((size_t)-1) / size) { errno = ENOMEM; return 0; }
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (!size) { free(ptr); return 0; }
    heap_header_t *h = ((heap_header_t*)ptr) - 1;
    if (h->size >= size) return ptr;
    void *next = malloc(size);
    if (!next) return 0;
    memcpy(next, ptr, h->size);
    free(ptr);
    return next;
}

char *strdup(const char *s) {
    size_t n = strlen(s) + 1u;
    char *out = (char*)malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

char *strerror(int errnum) {
    switch (errnum) {
    case 0: return "OK";
    case EINVAL: return "Invalid argument";
    case ENOENT: return "No such file";
    case ENOMEM: return "Out of memory";
    case ENOSYS: return "Not implemented";
    default: return "RADix error";
    }
}

long strtol(const char *nptr, char **endptr, int base) {
    const char *p = nptr;
    while (isspace((unsigned char)*p)) ++p;
    int neg = 0;
    if (*p == '-' || *p == '+') neg = *p++ == '-';
    if (base == 0) base = (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) ? 16 : (p[0] == '0' ? 8 : 10);
    if (base == 16 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
    unsigned long value = 0;
    while (*p) {
        int digit = isdigit((unsigned char)*p) ? *p - '0' : (isalpha((unsigned char)*p) ? tolower((unsigned char)*p) - 'a' + 10 : -1);
        if (digit < 0 || digit >= base) break;
        value = value * (unsigned long)base + (unsigned long)digit;
        ++p;
    }
    if (endptr) *endptr = (char*)p;
    return neg ? -(long)value : (long)value;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) { return (unsigned long)strtol(nptr, endptr, base); }
double strtod(const char *nptr, char **endptr) {
    if (!nptr) {
        if (endptr) *endptr = 0;
        return 0.0;
    }
    const char *p = nptr;
    while (isspace((unsigned char)*p)) ++p;
    int sign = 1;
    if (*p == '-' || *p == '+') sign = *p++ == '-' ? -1 : 1;
    double value = 0.0;
    int any = 0;
    while (isdigit((unsigned char)*p)) {
        value = value * 10.0 + (double)(*p - '0');
        ++p;
        any = 1;
    }
    if (*p == '.') {
        double place = 0.1;
        ++p;
        while (isdigit((unsigned char)*p)) {
            value += place * (double)(*p - '0');
            place *= 0.1;
            ++p;
            any = 1;
        }
    }
    if ((*p == 'e' || *p == 'E') && any) {
        const char *exp_start = p;
        ++p;
        int exp_sign = 1;
        if (*p == '-' || *p == '+') exp_sign = *p++ == '-' ? -1 : 1;
        int exp = 0;
        int exp_any = 0;
        while (isdigit((unsigned char)*p)) {
            exp = exp * 10 + (*p - '0');
            ++p;
            exp_any = 1;
        }
        if (exp_any) {
            while (exp-- > 0) value = exp_sign > 0 ? value * 10.0 : value / 10.0;
        } else {
            p = exp_start;
        }
    }
    if (endptr) *endptr = (char*)(any ? p : nptr);
    return sign < 0 ? -value : value;
}
int atoi(const char *nptr) { return (int)strtol(nptr, 0, 10); }
long atol(const char *nptr) { return strtol(nptr, 0, 10); }
double atof(const char *nptr) { return strtod(nptr, 0); }
int abs(int n) { return n < 0 ? -n : n; }
long labs(long n) { return n < 0 ? -n : n; }
intmax_t strtoimax(const char *nptr, char **endptr, int base) { return (intmax_t)strtol(nptr, endptr, base); }
uintmax_t strtoumax(const char *nptr, char **endptr, int base) { return (uintmax_t)strtoul(nptr, endptr, base); }
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    const unsigned char *items = (const unsigned char*)base;
    size_t low = 0;
    size_t high = nmemb;
    if (!key || !items || !compar || !size) return 0;
    while (low < high) {
        const size_t mid = low + (high - low) / 2u;
        const unsigned char *candidate = items + mid * size;
        const int cmp = compar(key, candidate);
        if (cmp == 0) return (void*)candidate;
        if (cmp < 0) high = mid;
        else low = mid + 1u;
    }
    return 0;
}
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    unsigned char *items = (unsigned char*)base;
    if (!items || !compar || size == 0) return;
    for (size_t i = 0; i < nmemb; ++i) {
        for (size_t j = i + 1; j < nmemb; ++j) {
            unsigned char *a = items + i * size;
            unsigned char *b = items + j * size;
            if (compar(a, b) <= 0) continue;
            for (size_t k = 0; k < size; ++k) {
                unsigned char tmp = a[k];
                a[k] = b[k];
                b[k] = tmp;
            }
        }
    }
}

typedef struct radix_tnode {
    const void *key;
    struct radix_tnode *left;
    struct radix_tnode *right;
} radix_tnode_t;

void *tfind(const void *key, void *const *rootp, int (*compar)(const void *, const void *)) {
    if (!rootp || !compar) return 0;
    radix_tnode_t *node = (radix_tnode_t*)*rootp;
    while (node) {
        int cmp = compar(key, node->key);
        if (cmp == 0) return &node->key;
        node = cmp < 0 ? node->left : node->right;
    }
    return 0;
}

void *tsearch(const void *key, void **rootp, int (*compar)(const void *, const void *)) {
    if (!rootp || !compar) return 0;
    radix_tnode_t **slot = (radix_tnode_t**)rootp;
    while (*slot) {
        int cmp = compar(key, (*slot)->key);
        if (cmp == 0) return &(*slot)->key;
        slot = cmp < 0 ? &(*slot)->left : &(*slot)->right;
    }
    radix_tnode_t *node = (radix_tnode_t*)calloc(1, sizeof(*node));
    if (!node) return 0;
    node->key = key;
    *slot = node;
    return &node->key;
}

static radix_tnode_t *tdelete_min(radix_tnode_t **rootp) {
    radix_tnode_t **slot = rootp;
    while ((*slot)->left) slot = &(*slot)->left;
    radix_tnode_t *node = *slot;
    *slot = node->right;
    return node;
}

void *tdelete(const void *key, void **rootp, int (*compar)(const void *, const void *)) {
    if (!rootp || !compar) return 0;
    radix_tnode_t **slot = (radix_tnode_t**)rootp;
    while (*slot) {
        int cmp = compar(key, (*slot)->key);
        if (cmp < 0) {
            slot = &(*slot)->left;
        } else if (cmp > 0) {
            slot = &(*slot)->right;
        } else {
            radix_tnode_t *victim = *slot;
            if (!victim->left) {
                *slot = victim->right;
            } else if (!victim->right) {
                *slot = victim->left;
            } else {
                radix_tnode_t *replacement = tdelete_min(&victim->right);
                replacement->left = victim->left;
                replacement->right = victim->right;
                *slot = replacement;
            }
            free(victim);
            return *slot ? (void*)&(*slot)->key : (void*)rootp;
        }
    }
    return 0;
}

static void twalk_node(const radix_tnode_t *node, void (*action)(const void *, VISIT, int), int depth) {
    if (!node || !action) return;
    if (!node->left && !node->right) {
        action(&node->key, leaf, depth);
        return;
    }
    action(&node->key, preorder, depth);
    twalk_node(node->left, action, depth + 1);
    action(&node->key, postorder, depth);
    twalk_node(node->right, action, depth + 1);
    action(&node->key, endorder, depth);
}

void twalk(const void *root, void (*action)(const void *, VISIT, int)) {
    twalk_node((const radix_tnode_t*)root, action, 0);
}

void tdestroy(void *root, void (*free_node)(void *nodep)) {
    radix_tnode_t *node = (radix_tnode_t*)root;
    if (!node) return;
    tdestroy(node->left, free_node);
    tdestroy(node->right, free_node);
    if (free_node) free_node((void*)node->key);
    free(node);
}

static char g_env_storage[64][128] = {
    "TERM=radix",
    "HOME=/home/root",
    "USER=root",
    "LOGNAME=root",
    "SHELL=/bin/rash",
    "PATH=/bin:/usr/bin",
    "TERMINFO=/usr/share/terminfo"
};
static char *g_environ[65] = {
    g_env_storage[0],
    g_env_storage[1],
    g_env_storage[2],
    g_env_storage[3],
    g_env_storage[4],
    g_env_storage[5],
    g_env_storage[6],
    0
};
char **environ = g_environ;

static int env_name_match(const char *entry, const char *name, size_t name_len) {
    return entry && strncmp(entry, name, name_len) == 0 && entry[name_len] == '=';
}

static int env_find_slot(const char *name, size_t name_len) {
    for (int i = 0; i < 64 && g_environ[i]; ++i) {
        if (env_name_match(g_environ[i], name, name_len)) return i;
    }
    return -1;
}

char *getenv(const char *name) {
    if (!name || !*name || strchr(name, '=')) return 0;
    const size_t name_len = strlen(name);
    const int slot = env_find_slot(name, name_len);
    return slot < 0 ? 0 : g_environ[slot] + name_len + 1;
}

int putenv(char *string) {
    if (!string) { errno = EINVAL; return -1; }
    char *eq = strchr(string, '=');
    if (!eq || eq == string) { errno = EINVAL; return -1; }
    const size_t name_len = (size_t)(eq - string);
    int slot = env_find_slot(string, name_len);
    if (slot < 0) {
        for (slot = 0; slot < 64 && g_environ[slot]; ++slot) {}
        if (slot >= 64) { errno = ENOMEM; return -1; }
    }
    g_environ[slot] = string;
    if (slot + 1 < 65) g_environ[slot + 1] = 0;
    return 0;
}

int setenv(const char *name, const char *value, int overwrite) {
    if (!name || !*name || strchr(name, '=') || !value) { errno = EINVAL; return -1; }
    const size_t name_len = strlen(name);
    int slot = env_find_slot(name, name_len);
    if (slot >= 0 && !overwrite) return 0;
    if (slot < 0) {
        for (slot = 0; slot < 64 && g_environ[slot]; ++slot) {}
        if (slot >= 64) { errno = ENOMEM; return -1; }
    }
    if (snprintf(g_env_storage[slot], sizeof(g_env_storage[slot]), "%s=%s", name, value) >= (int)sizeof(g_env_storage[slot])) {
        errno = ENOMEM;
        return -1;
    }
    g_environ[slot] = g_env_storage[slot];
    if (slot + 1 < 65) g_environ[slot + 1] = 0;
    return 0;
}

int unsetenv(const char *name) {
    if (!name || !*name || strchr(name, '=')) { errno = EINVAL; return -1; }
    const size_t name_len = strlen(name);
    int slot = env_find_slot(name, name_len);
    if (slot < 0) return 0;
    for (; slot < 64 && g_environ[slot]; ++slot) g_environ[slot] = g_environ[slot + 1];
    return 0;
}
char *realpath(const char *path, char *resolved_path) {
    if (!path) { errno = EINVAL; return 0; }
    char *out = resolved_path ? resolved_path : (char*)malloc(4096);
    if (!out) return 0;
    if (path[0] == '/') {
        strncpy(out, path, 4095);
        out[4095] = 0;
        return out;
    }
    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) {
        if (!resolved_path) free(out);
        return 0;
    }
    snprintf(out, 4096, "%s/%s", cwd, path);
    return out;
}
static const char *g_progname = "radix";
const char *getprogname(void) { return g_progname; }
void setprogname(const char *name) { if (name && *name) g_progname = name; }
void abort(void) { radix_syscall6(RADIX_SYS_EXIT, 127, 0, 0, 0, 0, 0); for (;;) {} }
int mkstemp(char *template) {
    if (!template) { errno = EINVAL; return -1; }
    size_t len = strlen(template);
    if (len < 6 || strcmp(template + len - 6, "XXXXXX") != 0) { errno = EINVAL; return -1; }
    for (int attempt = 0; attempt < 1000; ++attempt) {
        unsigned value = (unsigned)(getpid() * 37 + attempt);
        static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
        for (int i = 0; i < 6; ++i) {
            template[len - 6 + i] = alphabet[(value + (unsigned)i * 13u) % (sizeof(alphabet) - 1u)];
            value /= (unsigned)(sizeof(alphabet) - 1u);
        }
        int fd = open(template, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) return fd;
    }
    errno = EEXIST;
    return -1;
}
int mkstemps(char *template, int suffixlen) {
    if (suffixlen != 0) { errno = ENOSYS; return -1; }
    return mkstemp(template);
}
void _exit(int status) { radix_syscall6(RADIX_SYS_EXIT, status, 0, 0, 0, 0, 0); for (;;) {} }
void exit(int status) { _exit(status); }
void __radix_assert_fail(const char *expr, const char *file, int line) {
    dprintf(2, "assertion failed: %s at %s:%d\n", expr ? expr : "?", file ? file : "?", line);
    abort();
}

static struct passwd g_root_passwd = {
    (char*)"root",
    (char*)"x",
    0,
    0,
    (char*)"RADix root",
    (char*)"/root",
    (char*)"/bin/rash",
};
static int g_passwd_iter_emitted;

static int copy_passwd(const struct passwd *src, struct passwd *dst, char *buf, size_t buflen, struct passwd **result) {
    const char *strings[] = {src->pw_name, src->pw_passwd, src->pw_gecos, src->pw_dir, src->pw_shell};
    size_t need = 0;
    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) need += strlen(strings[i]) + 1u;
    if (!dst || !buf || buflen < need) {
        if (result) *result = 0;
        return ERANGE;
    }
    char *cursor = buf;
    char **targets[] = {&dst->pw_name, &dst->pw_passwd, &dst->pw_gecos, &dst->pw_dir, &dst->pw_shell};
    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) {
        size_t n = strlen(strings[i]) + 1u;
        memcpy(cursor, strings[i], n);
        *targets[i] = cursor;
        cursor += n;
    }
    dst->pw_uid = src->pw_uid;
    dst->pw_gid = src->pw_gid;
    if (result) *result = dst;
    return 0;
}

struct passwd *getpwnam(const char *name) {
    return name && strcmp(name, "root") == 0 ? &g_root_passwd : 0;
}

struct passwd *getpwuid(uid_t uid) {
    return uid == 0 ? &g_root_passwd : 0;
}

struct passwd *getpwent(void) {
    if (g_passwd_iter_emitted) return 0;
    g_passwd_iter_emitted = 1;
    return &g_root_passwd;
}

void setpwent(void) {
    g_passwd_iter_emitted = 0;
}

void endpwent(void) {
    g_passwd_iter_emitted = 0;
}

int getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result) {
    struct passwd *hit = getpwnam(name);
    if (!hit) { if (result) *result = 0; return 0; }
    return copy_passwd(hit, pwd, buf, buflen, result);
}

int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t buflen, struct passwd **result) {
    struct passwd *hit = getpwuid(uid);
    if (!hit) { if (result) *result = 0; return 0; }
    return copy_passwd(hit, pwd, buf, buflen, result);
}

int glob_pattern_p(const char *pattern, int quote) {
    (void)quote;
    return pattern && (strchr(pattern, '*') || strchr(pattern, '?') || strchr(pattern, '['));
}

int glob(const char *pattern, int flags, int (*errfunc)(const char *epath, int eerrno), glob_t *pglob) {
    (void)errfunc;
    if (!pattern || !pglob) return GLOB_ABORTED;
    pglob->gl_pathc = 0;
    pglob->gl_pathv = 0;
    if (glob_pattern_p(pattern, 0) && !(flags & GLOB_NOCHECK)) return GLOB_NOMATCH;
    char **paths = (char**)calloc(2u + ((flags & GLOB_DOOFFS) ? pglob->gl_offs : 0u), sizeof(char*));
    if (!paths) return GLOB_NOSPACE;
    size_t offset = (flags & GLOB_DOOFFS) ? pglob->gl_offs : 0u;
    paths[offset] = strdup(pattern);
    if (!paths[offset]) { free(paths); return GLOB_NOSPACE; }
    pglob->gl_pathc = 1;
    pglob->gl_pathv = paths;
    return 0;
}

void globfree(glob_t *pglob) {
    if (!pglob || !pglob->gl_pathv) return;
    size_t offset = pglob->gl_offs;
    for (size_t i = 0; i < pglob->gl_pathc; ++i) free(pglob->gl_pathv[offset + i]);
    free(pglob->gl_pathv);
    pglob->gl_pathv = 0;
    pglob->gl_pathc = 0;
}

struct FILE {
    int fd;
    int error;
};

static FILE g_stdio_files[3] = {{0, 0}, {1, 0}, {2, 0}};
FILE *stdin = &g_stdio_files[0];
FILE *stdout = &g_stdio_files[1];
FILE *stderr = &g_stdio_files[2];

static void out_ch(char **cursor, size_t *remaining, int fd, char ch, int *count) {
    if (cursor && remaining && *remaining > 1) {
        **cursor = ch;
        ++*cursor;
        --*remaining;
    } else if (fd >= 0) {
        write(fd, &ch, 1);
    }
    ++*count;
}

static void out_str(char **cursor, size_t *remaining, int fd, const char *s, int *count) {
    if (!s) s = "(null)";
    while (*s) out_ch(cursor, remaining, fd, *s++, count);
}

static void out_ulong(char **cursor, size_t *remaining, int fd, unsigned long value, int base, int upper, int *count) {
    char tmp[32];
    int pos = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    do {
        tmp[pos++] = digits[value % (unsigned long)base];
        value /= (unsigned long)base;
    } while (value && pos < (int)sizeof(tmp));
    while (pos > 0) out_ch(cursor, remaining, fd, tmp[--pos], count);
}

static int vformat(char *str, size_t size, int fd, const char *format, va_list ap) {
    char *cursor = str;
    size_t remaining = size;
    int count = 0;
    for (const char *p = format; p && *p; ++p) {
        if (*p != '%') {
            out_ch(str ? &cursor : 0, str ? &remaining : 0, fd, *p, &count);
            continue;
        }
        ++p;
        while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') ++p;
        while (isdigit((unsigned char)*p)) ++p;
        if (*p == '.') { ++p; while (isdigit((unsigned char)*p)) ++p; }
        int long_count = 0;
        while (*p == 'l') { ++long_count; ++p; }
        switch (*p) {
        case '%': out_ch(str ? &cursor : 0, str ? &remaining : 0, fd, '%', &count); break;
        case 'c': out_ch(str ? &cursor : 0, str ? &remaining : 0, fd, (char)va_arg(ap, int), &count); break;
        case 's': out_str(str ? &cursor : 0, str ? &remaining : 0, fd, va_arg(ap, const char*), &count); break;
        case 'd':
        case 'i': {
            long v = long_count ? va_arg(ap, long) : va_arg(ap, int);
            if (v < 0) { out_ch(str ? &cursor : 0, str ? &remaining : 0, fd, '-', &count); v = -v; }
            out_ulong(str ? &cursor : 0, str ? &remaining : 0, fd, (unsigned long)v, 10, 0, &count);
            break;
        }
        case 'u': out_ulong(str ? &cursor : 0, str ? &remaining : 0, fd, long_count ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int), 10, 0, &count); break;
        case 'x': out_ulong(str ? &cursor : 0, str ? &remaining : 0, fd, long_count ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int), 16, 0, &count); break;
        case 'X': out_ulong(str ? &cursor : 0, str ? &remaining : 0, fd, long_count ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int), 16, 1, &count); break;
        case 'p':
            out_str(str ? &cursor : 0, str ? &remaining : 0, fd, "0x", &count);
            out_ulong(str ? &cursor : 0, str ? &remaining : 0, fd, (unsigned long)va_arg(ap, void*), 16, 0, &count);
            break;
        default:
            if (*p) out_ch(str ? &cursor : 0, str ? &remaining : 0, fd, *p, &count);
            break;
        }
    }
    if (str && size) *cursor = 0;
    return count;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap) { return vformat(str, size, -1, format, ap); }
int snprintf(char *str, size_t size, const char *format, ...) { va_list ap; va_start(ap, format); int r = vsnprintf(str, size, format, ap); va_end(ap); return r; }
int vdprintf(int fd, const char *format, va_list ap) { return vformat(0, 0, fd, format, ap); }
int dprintf(int fd, const char *format, ...) { va_list ap; va_start(ap, format); int r = vdprintf(fd, format, ap); va_end(ap); return r; }
int vfprintf(FILE *stream, const char *format, va_list ap) { return vdprintf(stream ? stream->fd : 1, format, ap); }
int fprintf(FILE *stream, const char *format, ...) { va_list ap; va_start(ap, format); int r = vfprintf(stream, format, ap); va_end(ap); return r; }
int vprintf(const char *format, va_list ap) { return vdprintf(1, format, ap); }
int printf(const char *format, ...) { va_list ap; va_start(ap, format); int r = vprintf(format, ap); va_end(ap); return r; }
int sprintf(char *str, const char *format, ...) { va_list ap; va_start(ap, format); int r = vsnprintf(str, (size_t)-1, format, ap); va_end(ap); return r; }

static int hex_nibble(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int sscanf(const char *str, const char *format, ...) {
    if (!str || !format) return 0;
    va_list ap;
    va_start(ap, format);
    int matched = 0;
    if (strcmp(format, "#%1hX%1hX%1hX") == 0 && str[0] == '#') {
        for (int i = 0; i < 3; ++i) {
            unsigned short *out = va_arg(ap, unsigned short*);
            int value = hex_nibble((unsigned char)str[i + 1]);
            if (!out || value < 0) break;
            *out = (unsigned short)value;
            ++matched;
        }
    }
    va_end(ap);
    return matched;
}

int puts(const char *s) { int r = write(1, s, strlen(s)); write(1, "\n", 1); return r < 0 ? EOF : r + 1; }
int fputs(const char *s, FILE *stream) { return write(stream ? stream->fd : 1, s, strlen(s)) < 0 ? EOF : 0; }
int putchar(int c) { char ch = (char)c; return write(1, &ch, 1) == 1 ? c : EOF; }
int fputc(int c, FILE *stream) { char ch = (char)c; return write(stream ? stream->fd : 1, &ch, 1) == 1 ? c : EOF; }
int putc(int c, FILE *stream) { return fputc(c, stream); }
int fgetc(FILE *stream) { char ch; return read(stream ? stream->fd : 0, &ch, 1) == 1 ? (unsigned char)ch : EOF; }
int getc(FILE *stream) { return fgetc(stream); }
int getchar(void) { return fgetc(stdin); }

static FILE *alloc_file(int fd) {
    FILE *f = (FILE*)malloc(sizeof(FILE));
    if (!f) return 0;
    f->fd = fd;
    f->error = 0;
    return f;
}

FILE *fdopen(int fd, const char *mode) { (void)mode; return fd >= 0 ? alloc_file(fd) : 0; }
FILE *fopen(const char *path, const char *mode) {
    int flags = O_RDONLY;
    if (mode && mode[0] == 'w') flags = O_WRONLY | O_CREAT | O_TRUNC;
    else if (mode && mode[0] == 'a') flags = O_WRONLY | O_CREAT | O_APPEND;
    else if (mode && strchr(mode, '+')) flags = O_RDWR | O_CREAT;
    int fd = open(path, flags, 0666);
    return fd < 0 ? 0 : alloc_file(fd);
}
int fclose(FILE *stream) { if (!stream) return EOF; int r = close(stream->fd); free(stream); return r; }
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) { if (!stream || !size) return 0; ssize_t n = read(stream->fd, ptr, size * nmemb); if (n < 0) { stream->error = 1; return 0; } return (size_t)n / size; }
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) { if (!stream || !size) return 0; ssize_t n = write(stream->fd, ptr, size * nmemb); if (n < 0) { stream->error = 1; return 0; } return (size_t)n / size; }
int fflush(FILE *stream) { (void)stream; return 0; }
int fseek(FILE *stream, long offset, int whence) {
    if (!stream) { errno = EINVAL; return -1; }
    off_t result = lseek(stream->fd, (off_t)offset, whence);
    if (result < 0) { stream->error = 1; return -1; }
    return 0;
}
long ftell(FILE *stream) {
    if (!stream) { errno = EINVAL; return -1; }
    off_t result = lseek(stream->fd, 0, SEEK_CUR);
    if (result < 0) { stream->error = 1; return -1; }
    return (long)result;
}
void rewind(FILE *stream) {
    if (!stream) return;
    (void)fseek(stream, 0, SEEK_SET);
    clearerr(stream);
}
int fileno(FILE *stream) { return stream ? stream->fd : -1; }
int ferror(FILE *stream) { return stream ? stream->error : 1; }
void clearerr(FILE *stream) { if (stream) stream->error = 0; }
void __fseterr(FILE *stream) { if (stream) stream->error = 1; }
char *fgets(char *s, int size, FILE *stream) {
    if (!s || size <= 0 || !stream) return 0;
    int i = 0;
    while (i + 1 < size) {
        char ch;
        ssize_t n = read(stream->fd, &ch, 1);
        if (n <= 0) break;
        s[i++] = ch;
        if (ch == '\n') break;
    }
    if (!i) return 0;
    s[i] = 0;
    return s;
}
int remove(const char *pathname) { return unlink(pathname); }
int rename(const char *oldpath, const char *newpath) { return (int)ok_or_errno(radix_syscall6(RADIX_SYS_RENAME, (long)oldpath, (long)newpath, 0, 0, 0, 0)); }
void perror(const char *s) { if (s && *s) { dprintf(2, "%s: ", s); } dprintf(2, "%s\n", strerror(errno)); }

int truncate(const char *path, off_t length) {
    return (int)ok_or_errno(radix_syscall6(RADIX_SYS_TRUNCATE, (long)path, length, 0, 0, 0, 0));
}

int ftruncate(int fd, off_t length) {
    (void)fd;
    (void)length;
    errno = ENOSYS;
    return -1;
}

char *setlocale(int category, const char *locale) {
    (void)category;
    (void)locale;
    return "C.UTF-8";
}

struct lconv *localeconv(void) {
    static struct lconv lc = {
        ".", "", "", "", "", ".", "", "", "", "",
        127, 127, 127, 127, 127, 127, 127, 127,
        127, 127, 127, 127, 127, 127
    };
    return &lc;
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
    (void)tm;
    if (!s || !max || !format) return 0;
    size_t n = strlen(format);
    if (n >= max) return 0;
    memcpy(s, format, n + 1);
    return n;
}

char *nl_langinfo(nl_item item) {
    (void)item;
    return "UTF-8";
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && tolower((unsigned char)*s1) == tolower((unsigned char)*s2)) { ++s1; ++s2; }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        int a = tolower((unsigned char)s1[i]);
        int b = tolower((unsigned char)s2[i]);
        if (a != b || !a) return a - b;
    }
    return 0;
}

double floor(double x) { long i = (long)x; return (double)(i > x ? i - 1 : i); }
double ceil(double x) { long i = (long)x; return (double)(i < x ? i + 1 : i); }
double fabs(double x) { return x < 0 ? -x : x; }
double pow(double x, double y) {
    long exponent = (long)y;
    if ((double)exponent != y) return 0.0;
    int negative = exponent < 0;
    if (negative) exponent = -exponent;
    double result = 1.0;
    while (exponent-- > 0) result *= x;
    return negative && result != 0.0 ? 1.0 / result : result;
}
double log10(double x) {
    if (x <= 0.0) return 0.0 / 0.0;
    double value = 0.0;
    while (x >= 10.0) {
        x /= 10.0;
        value += 1.0;
    }
    while (x > 0.0 && x < 1.0) {
        x *= 10.0;
        value -= 1.0;
    }
    return value;
}
int isinf(double x) { return x == __builtin_huge_val() || x == -__builtin_huge_val(); }
int isnan(double x) { return x != x; }
int signbit(double x) { return x < 0.0; }

size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps) {
    (void)ps;
    if (!s) return 0;
    if (n == 0) return (size_t)-2;
    unsigned char c = (unsigned char)*s;
    if (!c) { if (pwc) *pwc = 0; return 0; }
    if (pwc) *pwc = c;
    return 1;
}

size_t mbrlen(const char *s, size_t n, mbstate_t *ps) {
    return mbrtowc(0, s, n, ps);
}

int mbsinit(const mbstate_t *ps) { return !ps || ps->state == 0; }

wint_t btowc(int c) {
    return c == EOF ? WEOF : (unsigned char)c;
}

size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps) {
    (void)ps;
    if (!s) return 1;
    if (wc < 0 || wc > 0x7f) { errno = EINVAL; return (size_t)-1; }
    *s = (char)wc;
    return 1;
}

int wctomb(char *s, wchar_t wc) {
    size_t n = wcrtomb(s, wc, 0);
    return n == (size_t)-1 ? -1 : (int)n;
}

int wcwidth(wchar_t wc) { return wc == 0 ? 0 : (wc < 32 ? 0 : 1); }
size_t wcslen(const wchar_t *s) { size_t n = 0; if (s) while (s[n]) ++n; return n; }
size_t wcsnlen(const wchar_t *s, size_t maxlen) { size_t n = 0; if (s) while (n < maxlen && s[n]) ++n; return n; }
wchar_t *wcscat(wchar_t *dest, const wchar_t *src) {
    wchar_t *out = dest;
    dest += wcslen(dest);
    while ((*dest++ = *src++)) {}
    return out;
}
wchar_t *wmemcpy(wchar_t *dest, const wchar_t *src, size_t n) {
    for (size_t i = 0; i < n; ++i) dest[i] = src[i];
    return dest;
}
wchar_t *wmempcpy(wchar_t *dest, const wchar_t *src, size_t n) {
    wmemcpy(dest, src, n);
    return dest + n;
}
wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (s[i] == c) return (wchar_t*)(s + i);
    }
    return 0;
}
void mbszero(mbstate_t *ps) { if (ps) memset(ps, 0, sizeof(*ps)); }
size_t mbstowcs(wchar_t *dest, const char *src, size_t n) { size_t i = 0; for (; i < n && src[i]; ++i) dest[i] = (unsigned char)src[i]; if (i < n) dest[i] = 0; return i; }
size_t mbsrtowcs(wchar_t *dest, const char **src, size_t len, mbstate_t *ps) {
    (void)ps;
    if (!src || !*src) { errno = EINVAL; return (size_t)-1; }
    const char *s = *src;
    size_t i = 0;
    while (s[i]) {
        if (dest) {
            if (i >= len) break;
            dest[i] = (unsigned char)s[i];
        }
        ++i;
    }
    if (dest && i < len) {
        dest[i] = 0;
        *src = 0;
    } else if (dest) {
        *src = s + i;
    }
    return i;
}
size_t wcsrtombs(char *dest, const wchar_t **src, size_t len, mbstate_t *ps) {
    (void)ps;
    if (!src || !*src) { errno = EINVAL; return (size_t)-1; }
    const wchar_t *s = *src;
    size_t i = 0;
    while (s[i]) {
        if (s[i] < 0 || s[i] > 0x7f) { errno = EILSEQ; return (size_t)-1; }
        if (dest) {
            if (i >= len) break;
            dest[i] = (char)s[i];
        }
        ++i;
    }
    if (dest && i < len) {
        dest[i] = 0;
        *src = 0;
    } else if (dest) {
        *src = s + i;
    }
    return i;
}
size_t wcstombs(char *dest, const wchar_t *src, size_t n) { size_t i = 0; for (; i < n && src[i]; ++i) dest[i] = (char)(src[i] & 0x7f); if (i < n) dest[i] = 0; return i; }
int iswspace(wchar_t wc) { return isspace((int)wc); }
int iswalpha(wchar_t wc) { return isalpha((int)wc); }
int iswalnum(wchar_t wc) { return isalnum((int)wc); }
int iswblank(wchar_t wc) { return wc == ' ' || wc == '\t'; }
int iswdigit(wchar_t wc) { return isdigit((int)wc); }
int iswgraph(wchar_t wc) { return wc > 32 && wc < 127; }
int iswlower(wchar_t wc) { return islower((int)wc); }
int iswprint(wchar_t wc) { return wc >= 32 && wc < 127; }
int iswpunct(wchar_t wc) { return wc > 32 && wc < 127 && !isalnum((int)wc); }
int iswcntrl(wchar_t wc) { return wc >= 0 && wc < 32; }
int iswupper(wchar_t wc) { return isupper((int)wc); }
int iswxdigit(wchar_t wc) { return isxdigit((int)wc); }
int towlower(wchar_t wc) { return tolower((int)wc); }
int towupper(wchar_t wc) { return toupper((int)wc); }
wctype_t wctype(const char *name) {
    if (!name) return 0;
    if (!strcmp(name, "alnum")) return iswalnum;
    if (!strcmp(name, "alpha")) return iswalpha;
    if (!strcmp(name, "blank")) return iswblank;
    if (!strcmp(name, "cntrl")) return iswcntrl;
    if (!strcmp(name, "digit")) return iswdigit;
    if (!strcmp(name, "graph")) return iswgraph;
    if (!strcmp(name, "lower")) return iswlower;
    if (!strcmp(name, "print")) return iswprint;
    if (!strcmp(name, "punct")) return iswpunct;
    if (!strcmp(name, "space")) return iswspace;
    if (!strcmp(name, "upper")) return iswupper;
    if (!strcmp(name, "xdigit")) return iswxdigit;
    return 0;
}
int iswctype(wint_t wc, wctype_t desc) { return desc ? desc(wc) : 0; }
wctrans_t wctrans(const char *name) {
    if (!name) return 0;
    if (!strcmp(name, "tolower")) return towlower;
    if (!strcmp(name, "toupper")) return towupper;
    return 0;
}
wint_t towctrans(wint_t wc, wctrans_t desc) { return desc ? desc(wc) : wc; }

int regcomp(regex_t *preg, const char *regex, int cflags) {
    if (!preg || !regex) return EINVAL;
    preg->pattern = regex;
    preg->cflags = cflags;
    preg->re_nsub = 0;
    return 0;
}

int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags) {
    (void)eflags;
    if (!preg || !preg->pattern || !string) return REG_NOMATCH;
    const char *hit = 0;
    const char *pat = preg->pattern;
    if ((preg->cflags & REG_ICASE) == 0) {
        for (const char *p = string; *p; ++p) {
            size_t i = 0;
            while (pat[i] && p[i] && pat[i] == p[i]) ++i;
            if (!pat[i]) { hit = p; break; }
        }
    } else {
        for (const char *p = string; *p; ++p) {
            size_t i = 0;
            while (pat[i] && p[i] && tolower((unsigned char)pat[i]) == tolower((unsigned char)p[i])) ++i;
            if (!pat[i]) { hit = p; break; }
        }
    }
    if (!hit) return REG_NOMATCH;
    if (nmatch && pmatch) {
        pmatch[0].rm_so = hit - string;
        pmatch[0].rm_eo = pmatch[0].rm_so + (regoff_t)strlen(pat);
    }
    return 0;
}

size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size) {
    (void)preg;
    const char *msg = errcode == REG_NOMATCH ? "No match" : "Regex error";
    size_t n = strlen(msg) + 1u;
    if (errbuf && errbuf_size) {
        strncpy(errbuf, msg, errbuf_size - 1u);
        errbuf[errbuf_size - 1u] = 0;
    }
    return n;
}

void regfree(regex_t *preg) { (void)preg; }

char *optarg;
int optind = 1;
int opterr = 1;
int optopt;

int getopt(int argc, char *const argv[], const char *optstring) {
    if (optind >= argc || !argv[optind] || argv[optind][0] != '-' || argv[optind][1] == 0) return -1;
    char option = argv[optind][1];
    const char *found = strchr(optstring, option);
    optopt = option;
    if (!found) { ++optind; return '?'; }
    if (found[1] == ':') {
        if (argv[optind][2]) optarg = &argv[optind][2];
        else if (optind + 1 < argc) optarg = argv[++optind];
        else { ++optind; return ':'; }
    } else {
        optarg = 0;
    }
    ++optind;
    return option;
}

int getopt_long(int argc, char *const argv[], const char *optstring, const struct option *longopts, int *longindex) {
    if (optind < argc && argv[optind] && argv[optind][0] == '-' && argv[optind][1] == '-') {
        const char *name = argv[optind] + 2;
        for (int i = 0; longopts && longopts[i].name; ++i) {
            if (strcmp(name, longopts[i].name) == 0) {
                if (longindex) *longindex = i;
                ++optind;
                if (longopts[i].flag) { *longopts[i].flag = longopts[i].val; return 0; }
                return longopts[i].val;
            }
        }
        ++optind;
        return '?';
    }
    return getopt(argc, argv, optstring);
}

int getopt_long_only(int argc, char *const argv[], const char *optstring, const struct option *longopts, int *longindex) {
    return getopt_long(argc, argv, optstring, longopts, longindex);
}

char *basename(char *path) {
    if (!path || !*path) return ".";
    char *last = strrchr(path, '/');
    return last && last[1] ? last + 1 : path;
}

char *dirname(char *path) {
    if (!path || !*path) return ".";
    char *last = strrchr(path, '/');
    if (!last) return ".";
    if (last == path) { path[1] = 0; return path; }
    *last = 0;
    return path;
}
