#include <stddef.h>
#include <stdint.h>

#include "user_auth.h"
#include "rkconfig.h"

enum {
    SYS_READ = 0,
    SYS_WRITE = 1,
    SYS_OPEN = 2,
    SYS_CLOSE = 3,
    SYS_IOCTL = 4,
    SYS_LSEEK = 5,
    SYS_STAT = 6,
    SYS_GETTIMEOFDAY = 8,
    SYS_NANOSLEEP = 9,
    SYS_EXIT = 10,
    SYS_FORK = 11,
    SYS_EXECVE = 12,
    SYS_WAITPID = 13,
    SYS_GETPID = 14,
    SYS_DUP2 = 17,
    SYS_CHDIR = 18,
    SYS_GETCWD = 19,
    SYS_PIPE = 21,
    SYS_SOCKET = 25,
    SYS_BIND = 26,
    SYS_SENDTO = 30,
    SYS_RECVFROM = 31,
    SYS_POLL = 39,
    SYS_SETTIMEOFDAY = 40,
    SYS_GETDENTS = 1000,
    SYS_REMOVE = 1001,
    SYS_MKDIR = 1002,
    SYS_RMDIR = 1003,
    SYS_RENAME = 1004,
    SYS_TRUNCATE = 1005,
    SYS_LOG_READ = 1006,
    SYS_LOG_FLUSH = 1007,
    SYS_GETUID = 1008,
    SYS_GETEUID = 1009,
    SYS_GETGID = 1010,
    SYS_GETEGID = 1011,
    SYS_CHMOD = 1014,
    SYS_LINK = 1015,
    SYS_SYMLINK = 1016,
    SYS_READLINK = 1017,
    SYS_FSYNC = 1018,
    SYS_KILL = 1019,
};

#define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#define WTERMSIG(status) ((status) & 0x7f)
#define WIFEXITED(status) (WTERMSIG(status) == 0)

#define RAD_IOCTL_NONE 0u
#define RAD_IOCTL_WRITE 1u
#define RAD_IOCTL_READ 2u
#define RAD_IOCTL_READWRITE 3u
#define RAD_IOCTL(dir, type, nr, size) ((((uint32_t)(dir) & 3u) << 30) | (((uint32_t)(size) & 0x3fffu) << 16) | (((uint32_t)(type) & 0xffu) << 8) | ((uint32_t)(nr) & 0xffu))
#define RAD_IOR(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_READ, type, nr, sizeof(type_name))
#define RAD_IOW(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_WRITE, type, nr, sizeof(type_name))
#define RAD_IOCTL_TYPE_TTY 'Y'
#define RAD_IOCTL_TYPE_NET 'N'
#define RAD_DEVICE_IOCTL_TTY_GET_WINSIZE RAD_IOR(RAD_IOCTL_TYPE_TTY, 1u, radsh_winsize_t)
#define RAD_DEVICE_IOCTL_TTY_SET_WINSIZE RAD_IOW(RAD_IOCTL_TYPE_TTY, 2u, radsh_winsize_t)
#define RAD_DEVICE_IOCTL_TTY_GET_MODE RAD_IOR(RAD_IOCTL_TYPE_TTY, 3u, uint32_t)
#define RAD_DEVICE_IOCTL_TTY_SET_MODE RAD_IOW(RAD_IOCTL_TYPE_TTY, 4u, uint32_t)
#define RAD_DEVICE_IOCTL_NET_LINK_INFO RAD_IOR(RAD_IOCTL_TYPE_NET, 1u, radsh_net_link_info_t)
#define RAD_DEVICE_IOCTL_NET_STACK_INFO RAD_IOR(RAD_IOCTL_TYPE_NET, 5u, radsh_net_stack_info_t)
#define RAD_DEVICE_IOCTL_NET_NTP_QUERY RAD_IOCTL(RAD_IOCTL_READWRITE, RAD_IOCTL_TYPE_NET, 6u, sizeof(radsh_ntp_query_t))
#define RAD_TTY_MODE_CANONICAL (1u << 0)
#define RAD_TTY_MODE_ECHO (1u << 1)
#define RAD_TTY_MODE_CRLF (1u << 2)
#define RAD_POLLIN 0x0001
#define RAD_POLLOUT 0x0004
#define RAD_POLLERR 0x0008
#define RAD_POLLHUP 0x0010
#define RAD_POLLNVAL 0x0020
#define RAD_AF_INET 2
#define RAD_SOCK_DGRAM 2
#define RAD_IPPROTO_UDP 17

#ifndef RADIX_RKCONFIG_NET_IPV4_A
#define RADIX_RKCONFIG_NET_IPV4_A 10
#define RADIX_RKCONFIG_NET_IPV4_B 0
#define RADIX_RKCONFIG_NET_IPV4_C 2
#define RADIX_RKCONFIG_NET_IPV4_D 15
#endif

#ifndef RADIX_RKCONFIG_NET_NTP_A
#define RADIX_RKCONFIG_NET_NTP_A 216
#define RADIX_RKCONFIG_NET_NTP_B 239
#define RADIX_RKCONFIG_NET_NTP_C 35
#define RADIX_RKCONFIG_NET_NTP_D 0
#endif

#ifndef RADIX_RKCONFIG_NET_NTP_HOST
#define RADIX_RKCONFIG_NET_NTP_HOST "time.google.com"
#endif

#ifndef RADIX_RKCONFIG_NET_NTP_PORT
#define RADIX_RKCONFIG_NET_NTP_PORT 123
#endif

#ifndef RADIX_RKCONFIG_NET_DNS_A
#define RADIX_RKCONFIG_NET_DNS_A 10
#define RADIX_RKCONFIG_NET_DNS_B 0
#define RADIX_RKCONFIG_NET_DNS_C 2
#define RADIX_RKCONFIG_NET_DNS_D 3
#endif

enum {
    O_READ = 1u << 0,
    O_WRITE = 1u << 1,
    O_CREATE = 1u << 2,
    O_TRUNCATE = 1u << 3,
    O_APPEND = 1u << 4,
    O_DIRECTORY = 1u << 5,
};

typedef struct {
    uint64_t size;
    int is_directory;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint64_t mtime_millis;
} radsh_stat_t;

typedef struct {
    uint8_t type;
    char name[256];
} radsh_dirent_t;

typedef struct {
    uint16_t rows;
    uint16_t columns;
    uint16_t x_pixels;
    uint16_t y_pixels;
} radsh_winsize_t;

typedef struct {
    uint8_t bytes[6];
} radsh_mac_t;

typedef struct {
    uint8_t bytes[4];
} radsh_ipv4_t;

typedef struct {
    uint16_t family;
    uint16_t port;
    radsh_ipv4_t address;
    uint8_t zero[8];
} radsh_sockaddr_in_t;

typedef struct {
    uint32_t size;
    radsh_mac_t mac;
    uint32_t mtu;
    int link_up;
    uint64_t tx_packets;
    uint64_t rx_packets;
} radsh_net_link_info_t;

typedef struct {
    uint32_t size;
    radsh_ipv4_t ipv4;
    radsh_ipv4_t netmask;
    radsh_ipv4_t gateway;
    radsh_ipv4_t ntp_server;
    uint16_t ntp_port;
    uint16_t arp_entries;
    uint64_t ethernet_rx;
    uint64_t ethernet_tx;
    uint64_t arp_rx;
    uint64_t arp_tx;
    uint64_t ipv4_rx;
    uint64_t ipv4_tx;
    uint64_t udp_rx;
    uint64_t udp_tx;
    uint64_t icmp_rx;
    uint64_t icmp_tx;
} radsh_net_stack_info_t;

typedef struct {
    uint32_t size;
    radsh_ipv4_t server;
    uint16_t port;
    uint16_t valid;
    uint64_t last_unix_seconds;
    int64_t offset_millis;
    uint64_t queries;
    uint64_t responses;
} radsh_ntp_status_t;

typedef struct {
    uint32_t size;
    radsh_ipv4_t server;
    uint16_t port;
    uint16_t timeout_ms;
    radsh_ntp_status_t status;
} radsh_ntp_query_t;

typedef struct {
    int fd;
    int16_t events;
    int16_t revents;
} radsh_pollfd_t;

typedef struct {
    int64_t tv_sec;
    int64_t tv_usec;
} radsh_timeval_t;

typedef struct {
    uint64_t sequence;
    uint64_t time_millis;
    int level;
    char category[32];
    char message[192];
} radsh_log_entry_t;

static long sc(long n, long a, long b, long c, long d, long e, long f) {
    register long rax asm("rax") = n;
    register long rdi asm("rdi") = a;
    register long rsi asm("rsi") = b;
    register long rdx asm("rdx") = c;
    register long r10 asm("r10") = d;
    register long r8 asm("r8") = e;
    register long r9 asm("r9") = f;
    asm volatile("int $0x80" : "+a"(rax) : "D"(rdi), "S"(rsi), "d"(rdx), "r"(r10), "r"(r8), "r"(r9) : "memory");
    return rax;
}

static size_t s_len(const char *s) {
    size_t n = 0;
    if (s) while (s[n]) ++n;
    return n;
}

static int s_cmp(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return (unsigned char)*a - (unsigned char)*b;
}

static int s_eq(const char *a, const char *b) {
    return s_cmp(a, b) == 0;
}

static int s_starts_with_name(const char *line, const char *name) {
    size_t i = 0;
    while (name[i]) {
        if (line[i] != name[i]) return 0;
        ++i;
    }
    return line[i] == 0 || line[i] == ' ' || line[i] == '\t';
}

static void s_copy(char *dst, size_t size, const char *src) {
    if (!dst || size == 0) return;
    size_t i = 0;
    if (src) for (; i + 1 < size && src[i]; ++i) dst[i] = src[i];
    dst[i] = 0;
}

static void s_cat(char *dst, size_t size, const char *src) {
    size_t at = s_len(dst);
    if (at >= size) return;
    s_copy(dst + at, size - at, src);
}

static void zero_mem(void *ptr, size_t size) {
    unsigned char *p = (unsigned char*)ptr;
    while (size--) *p++ = 0;
}

static long to_long(const char *s, long fallback) {
    long v = 0;
    if (!s || !*s) return fallback;
    while (*s) {
        if (*s < '0' || *s > '9') return fallback;
        v = v * 10 + (*s++ - '0');
    }
    return v;
}

static long to_octal(const char *s, long fallback) {
    long v = 0;
    if (!s || !*s) return fallback;
    while (*s) {
        if (*s < '0' || *s > '7') return fallback;
        v = (v << 3) + (*s++ - '0');
    }
    return v;
}

static void putn(int fd, const char *s, size_t n) {
    if (s && n) sc(SYS_WRITE, fd, (long)s, (long)n, 0, 0, 0);
}

static void puts_fd(int fd, const char *s) {
    putn(fd, s, s_len(s));
}

static void line_fd(int fd, const char *s) {
    puts_fd(fd, s);
    puts_fd(fd, "\n");
}

static void marker_fd(const char *s) {
    line_fd(1, s);
}

static void print_num(int fd, uint64_t v) {
    char tmp[24];
    size_t n = 0;
    do {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    } while (v);
    while (n) putn(fd, &tmp[--n], 1);
}

static void print_signed(int fd, long v) {
    if (v < 0) {
        puts_fd(fd, "-");
        print_num(fd, (uint64_t)(-v));
    } else {
        print_num(fd, (uint64_t)v);
    }
}

static int read_fd_to_fd(int fd, int outfd) {
    char buf[256];
    long n = 0;
    while ((n = sc(SYS_READ, fd, (long)buf, sizeof(buf), 0, 0, 0)) > 0) putn(outfd, buf, (size_t)n);
    return n < 0 ? (int)n : 0;
}

static int read_file_to_fd(const char *path, int outfd) {
    long fd = sc(SYS_OPEN, (long)path, O_READ, 0, 0, 0, 0);
    if (fd < 0) return (int)fd;
    int r = read_fd_to_fd((int)fd, outfd);
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    return r;
}

static int copy_file(const char *src, const char *dst) {
    long in = sc(SYS_OPEN, (long)src, O_READ, 0, 0, 0, 0);
    if (in < 0) return (int)in;
    long out = sc(SYS_OPEN, (long)dst, O_WRITE | O_CREATE | O_TRUNCATE, 0, 0, 0, 0);
    if (out < 0) {
        sc(SYS_CLOSE, in, 0, 0, 0, 0, 0);
        return (int)out;
    }
    char buf[256];
    long n = 0;
    int status = 0;
    while ((n = sc(SYS_READ, in, (long)buf, sizeof(buf), 0, 0, 0)) > 0) {
        long wrote = sc(SYS_WRITE, out, (long)buf, n, 0, 0, 0);
        if (wrote != n) {
            status = wrote < 0 ? (int)wrote : -1;
            break;
        }
    }
    if (n < 0) status = (int)n;
    sc(SYS_CLOSE, in, 0, 0, 0, 0, 0);
    sc(SYS_CLOSE, out, 0, 0, 0, 0, 0);
    return status;
}

static char *parse_redirect_path(char **cursor) {
    char *p = *cursor;
    while (*p == ' ' || *p == '\t') ++p;
    char *path = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') ++p;
    if (*p) *p++ = 0;
    *cursor = p;
    return path;
}

static int parse_line(char *line, char **argv, int max_args, int *outfd, int *infd) {
    int argc = 0;
    *outfd = 1;
    *infd = 0;
    char *p = line;
    while (*p && argc < max_args - 1) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
        if (!*p) break;
        if (*p == '>' && p[1] == '>') {
            p += 2;
            char *path = parse_redirect_path(&p);
            long fd = sc(SYS_OPEN, (long)path, O_WRITE | O_CREATE | O_APPEND, 0, 0, 0, 0);
            if (fd >= 0) *outfd = (int)fd;
            continue;
        }
        if (*p == '>') {
            ++p;
            char *path = parse_redirect_path(&p);
            long fd = sc(SYS_OPEN, (long)path, O_WRITE | O_CREATE | O_TRUNCATE, 0, 0, 0, 0);
            if (fd >= 0) *outfd = (int)fd;
            continue;
        }
        if (*p == '<') {
            ++p;
            char *path = parse_redirect_path(&p);
            long fd = sc(SYS_OPEN, (long)path, O_READ, 0, 0, 0, 0);
            if (fd >= 0) *infd = (int)fd;
            continue;
        }
        char quote = 0;
        if (*p == '\'' || *p == '"') quote = *p++;
        argv[argc++] = p;
        while (*p && ((quote && *p != quote) || (!quote && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r'))) ++p;
        if (*p) *p++ = 0;
    }
    argv[argc] = 0;
    return argc;
}

static int cmd_ls(int argc, char **argv, int outfd) {
    const char *path = argc > 1 ? argv[1] : ".";
    long fd = sc(SYS_OPEN, (long)path, O_READ | O_DIRECTORY, 0, 0, 0, 0);
    if (fd < 0) {
        puts_fd(outfd, "ls: cannot open ");
        puts_fd(outfd, path);
        puts_fd(outfd, "\n");
        return (int)fd;
    }
    radsh_dirent_t entries[4];
    long n = 0;
    int seen = 0;
    while ((n = sc(SYS_GETDENTS, fd, (long)entries, 4, 0, 0, 0)) > 0) {
        for (long i = 0; i < n; ++i) {
            seen = 1;
            puts_fd(outfd, entries[i].type == 2 ? "\x1b[94m" : "\x1b[92m");
            char line[320];
            line[0] = 0;
            s_cat(line, sizeof(line), entries[i].name);
            if (entries[i].type == 2) s_cat(line, sizeof(line), "/");
            puts_fd(outfd, line);
            puts_fd(outfd, "\x1b[0m\n");
        }
    }
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    if (seen) marker_fd("RADIX_USER_RADSH_LS_OK");
    if (n == 0 && !seen) {
        puts_fd(outfd, "ls: empty directory listing: ");
        puts_fd(outfd, path);
        puts_fd(outfd, "\n");
        marker_fd("RADIX_USER_RADSH_LS_EMPTY");
    }
    return n < 0 ? (int)n : 0;
}

static int cmd_head_tail_fd(int fd, int outfd, int lines, int tail) {
    char text[2048];
    long n = sc(SYS_READ, fd, (long)text, sizeof(text) - 1, 0, 0, 0);
    if (n < 0) return (int)n;
    text[n] = 0;
    char *start = text;
    if (tail) {
        int seen = 0;
        for (long i = n; i > 0; --i) if (text[i - 1] == '\n' && ++seen > lines) { start = text + i; break; }
        puts_fd(outfd, start);
    } else {
        int seen = 0;
        for (long i = 0; i < n && seen < lines; ++i) { putn(outfd, &text[i], 1); if (text[i] == '\n') ++seen; }
    }
    return 0;
}

static int cmd_wc_fd(int fd, int outfd, const char *label) {
    char buf[256];
    uint64_t bytes = 0, lines = 0, words = 0;
    int in_word = 0;
    long n = 0;
    while ((n = sc(SYS_READ, fd, (long)buf, sizeof(buf), 0, 0, 0)) > 0) {
        for (long i = 0; i < n; ++i) {
            char c = buf[i];
            ++bytes;
            if (c == '\n') ++lines;
            int sp = c == ' ' || c == '\n' || c == '\t' || c == '\r';
            if (sp) in_word = 0;
            else if (!in_word) { in_word = 1; ++words; }
        }
    }
    print_num(outfd, lines); puts_fd(outfd, " ");
    print_num(outfd, words); puts_fd(outfd, " ");
    print_num(outfd, bytes);
    if (label) { puts_fd(outfd, " "); puts_fd(outfd, label); }
    puts_fd(outfd, "\n");
    return n < 0 && bytes == 0 ? (int)n : 0;
}

static const char *log_level_name(int level) {
    switch (level) {
    case 0: return "TRACE";
    case 1: return "DEBUG";
    case 2: return "INFO";
    case 3: return "WARNING";
    case 4: return "ERROR";
    case 5: return "CRITICAL";
    default: return "INFO";
    }
}

static int cmd_dmesg_user(int outfd) {
    uint64_t after = 0;
    for (;;) {
        radsh_log_entry_t entries[4];
        long n = sc(SYS_LOG_READ, (long)entries, 4, after, 0, 0, 0);
        if (n <= 0) break;
        long limit = n < 4 ? n : 4;
        for (long i = 0; i < limit; ++i) {
            after = entries[i].sequence;
            print_num(outfd, entries[i].time_millis);
            puts_fd(outfd, "ms [");
            puts_fd(outfd, log_level_name(entries[i].level));
            puts_fd(outfd, "] [");
            puts_fd(outfd, entries[i].category);
            puts_fd(outfd, "] ");
            puts_fd(outfd, entries[i].message);
            if (s_len(entries[i].message) == 0 || entries[i].message[s_len(entries[i].message) - 1] != '\n') puts_fd(outfd, "\n");
        }
        if (n < 4) break;
    }
    return 0;
}

static int cmd_initctl_control(const char *verb, const char *name, int outfd);
static int write_file_text(const char *path, const char *text);
static int read_file_text(const char *path, char *buffer, size_t size);

static int radinit_control_request(const char *verb, const char *name, int outfd, const char *queued_message) {
    if (!verb || !verb[0]) return -2;
    write_file_text("/run/radinit/control.status", "pending\n");
    char request[96];
    s_copy(request, sizeof(request), verb);
    if (name && name[0]) {
        s_cat(request, sizeof(request), " ");
        s_cat(request, sizeof(request), name);
    }
    s_cat(request, sizeof(request), "\n");
    int status = write_file_text("/run/radinit/control.txt", request);
    if (status < 0) return status;
    for (int i = 0; i < 40; ++i) {
        char response[160];
        int n = read_file_text("/run/radinit/control.status", response, sizeof(response));
        if (n > 0) {
            if (s_starts_with_name(response, "ok") || s_starts_with_name(response, "busy") || s_starts_with_name(response, "error")) {
                puts_fd(outfd, response);
                return s_starts_with_name(response, "ok") ? 0 : -1;
            }
        }
        sc(SYS_NANOSLEEP, 50000000, 0, 0, 0, 0, 0);
    }
    line_fd(outfd, queued_message ? queued_message : "request queued");
    return 0;
}

static int cmd_initctl(int argc, char **argv, int outfd) {
    if (argc < 2 || s_eq(argv[1], "list")) return read_file_to_fd("/run/radinit/status.txt", outfd);
    if (s_eq(argv[1], "status")) {
        if (argc < 3) return -2;
        long fd = sc(SYS_OPEN, (long)"/run/radinit/status.txt", O_READ, 0, 0, 0, 0);
        if (fd < 0) return (int)fd;
        char text[2048];
        long n = sc(SYS_READ, fd, (long)text, sizeof(text) - 1, 0, 0, 0);
        sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
        if (n < 0) return (int)n;
        text[n] = 0;
        char *line = text;
        while (*line) {
            char *next = line;
            while (*next && *next != '\n') ++next;
            char saved = *next;
            *next = 0;
            if (s_starts_with_name(line, argv[2])) {
                line_fd(outfd, line);
                if (saved) *next++ = saved;
                return 0;
            }
            if (saved) *next++ = saved;
            line = next;
        }
        return -3;
    }
    if (s_eq(argv[1], "start") || s_eq(argv[1], "restart") || s_eq(argv[1], "stop")) return cmd_initctl_control(argv[1], argc > 2 ? argv[2] : "", outfd);
    return -2;
}

static int cmd_radservice(int argc, char **argv, int outfd) {
    if (argc < 2 || s_eq(argv[1], "list")) {
        int status = radinit_control_request("reload", "", outfd, "radservice reload queued");
        if (status < 0) return status;
        return read_file_to_fd("/run/radinit/status.txt", outfd);
    }
    if (s_eq(argv[1], "start") || s_eq(argv[1], "stop") || s_eq(argv[1], "restart")) {
        if (argc < 3) return -2;
        return radinit_control_request(argv[1], argv[2], outfd, "radservice request queued");
    }
    line_fd(outfd, "usage: radservice list|start <service>|stop <service>|restart <service>");
    return -2;
}

static int cmd_logread(int argc, char **argv, int outfd) {
    char path[128];
    if (argc < 2) {
        s_copy(path, sizeof(path), "/var/log/radix/init.log");
    } else if (s_eq(argv[1], "kernel")) {
        s_copy(path, sizeof(path), "/var/log/radix/rkernel.log");
    } else if (s_eq(argv[1], "init")) {
        s_copy(path, sizeof(path), "/var/log/radix/init.log");
    } else {
        s_copy(path, sizeof(path), "/var/log/radix/");
        s_cat(path, sizeof(path), argv[1]);
        s_cat(path, sizeof(path), ".log");
    }
    return read_file_to_fd(path, outfd);
}

static int write_file_text(const char *path, const char *text) {
    long fd = sc(SYS_OPEN, (long)path, O_WRITE | O_CREATE | O_TRUNCATE, 0, 0, 0, 0);
    if (fd < 0) return (int)fd;
    long written = sc(SYS_WRITE, fd, (long)text, (long)s_len(text), 0, 0, 0);
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    return written == (long)s_len(text) ? 0 : (written < 0 ? (int)written : -1);
}

static int read_file_text(const char *path, char *buffer, size_t size) {
    if (!buffer || size == 0) return -2;
    long fd = sc(SYS_OPEN, (long)path, O_READ, 0, 0, 0, 0);
    if (fd < 0) return (int)fd;
    long n = sc(SYS_READ, fd, (long)buffer, size - 1, 0, 0, 0);
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    if (n < 0) return (int)n;
    buffer[n] = 0;
    return (int)n;
}

static void trim_line(char *text) {
    if (!text) return;
    for (size_t i = 0; text[i]; ++i) {
        if (text[i] == '\n' || text[i] == '\r') {
            text[i] = 0;
            return;
        }
    }
}

static void read_hostname(char *buffer, size_t size) {
    if (read_file_text("/etc/hostname", buffer, size) <= 0) s_copy(buffer, size, "radix");
    trim_line(buffer);
    if (!buffer[0]) s_copy(buffer, size, "radix");
}

static void read_username(char *buffer, size_t size) {
    if (sc(SYS_GETUID, 0, 0, 0, 0, 0, 0) == 0) s_copy(buffer, size, "root");
    else s_copy(buffer, size, "user");
}

static void write_prompt(void) {
    char user[32];
    char host[64];
    char cwd[128];
    read_username(user, sizeof(user));
    read_hostname(host, sizeof(host));
    if (sc(SYS_GETCWD, (long)cwd, sizeof(cwd), 0, 0, 0, 0) < 0) s_copy(cwd, sizeof(cwd), "/");
    puts_fd(1, "\x1b[92m");
    puts_fd(1, user);
    puts_fd(1, "@");
    puts_fd(1, host);
    puts_fd(1, "\x1b[0m:");
    puts_fd(1, "\x1b[94m");
    puts_fd(1, cwd);
    puts_fd(1, "\x1b[0m");
    puts_fd(1, sc(SYS_GETEUID, 0, 0, 0, 0, 0, 0) == 0 ? "# " : "$ ");
}

static int cmd_id(int outfd) {
    puts_fd(outfd, "uid=");
    print_num(outfd, (uint64_t)sc(SYS_GETUID, 0, 0, 0, 0, 0, 0));
    puts_fd(outfd, " euid=");
    print_num(outfd, (uint64_t)sc(SYS_GETEUID, 0, 0, 0, 0, 0, 0));
    puts_fd(outfd, " gid=");
    print_num(outfd, (uint64_t)sc(SYS_GETGID, 0, 0, 0, 0, 0, 0));
    puts_fd(outfd, " egid=");
    print_num(outfd, (uint64_t)sc(SYS_GETEGID, 0, 0, 0, 0, 0, 0));
    puts_fd(outfd, "\n");
    return 0;
}

static int cmd_passwd(int outfd) {
    if (sc(SYS_GETEUID, 0, 0, 0, 0, 0, 0) != 0) return -2;
    puts_fd(1, "New password: ");
    char password[96];
    size_t pos = 0;
    char ch = 0;
    while (pos + 1 < sizeof(password)) {
        long n = sc(SYS_READ, 0, (long)&ch, 1, 0, 0, 0);
        if (n <= 0) return -1;
        if (ch == '\r') continue;
        if (ch == '\n') break;
        password[pos++] = ch;
    }
    password[pos] = 0;
    char hash[65];
    radix_auth_password_hash("runtime-root", password, hash);
    char text[256];
    s_copy(text, sizeof(text), "root:0:0:runtime-root:");
    s_cat(text, sizeof(text), hash);
    s_cat(text, sizeof(text), ":/home/root:/bin/rash\n");
    int status = write_file_text("/etc/passwords", text);
    if (status == 0) {
        line_fd(outfd, "RADIX_PASSWD_UPDATE_OK");
        line_fd(outfd, "password updated");
    }
    return status;
}

static int cmd_stat(int argc, char **argv, int outfd) {
    if (argc < 2) return -2;
    for (int i = 1; i < argc; ++i) {
        radsh_stat_t st;
        long r = sc(SYS_STAT, (long)argv[i], (long)&st, 0, 0, 0, 0);
        if (r < 0) return (int)r;
        puts_fd(outfd, argv[i]);
        puts_fd(outfd, " type=");
        puts_fd(outfd, st.is_directory ? "dir" : "file");
        puts_fd(outfd, " size=");
        print_num(outfd, st.size);
        puts_fd(outfd, " mode=");
        print_num(outfd, st.mode);
        puts_fd(outfd, " mtime_ms=");
        print_num(outfd, st.mtime_millis);
        puts_fd(outfd, "\n");
    }
    return 0;
}

static int cmd_mount(int outfd) {
    line_fd(outfd, "/ type ext4 dev /dev/vda");
    line_fd(outfd, "/mnt/fat type fat32 dev /dev/vdb");
    return 0;
}

static int cmd_ps(int outfd) {
    line_fd(outfd, "PID STATE SERVICE");
    return read_file_to_fd("/run/radinit/status.txt", outfd);
}

static int field_pid_from_line(const char *line) {
    const char *p = line;
    while (p && *p) {
        if (p[0] == 'p' && p[1] == 'i' && p[2] == 'd' && p[3] == '=') {
            return (int)to_long(p + 4, 0);
        }
        ++p;
    }
    return 0;
}

static int line_exec_matches(const char *line, const char *name) {
    const char *p = line;
    while (p && *p) {
        if (p[0] == 'e' && p[1] == 'x' && p[2] == 'e' && p[3] == 'c' && p[4] == '=') {
            p += 5;
            const char *base = p;
            for (const char *q = p; *q && *q != ' ' && *q != '\t'; ++q) {
                if (*q == '/' && q[1] && q[1] != ' ' && q[1] != '\t') base = q + 1;
            }
            size_t i = 0;
            while (name[i] && base[i] == name[i]) ++i;
            return name[i] == 0 && (base[i] == 0 || base[i] == ' ' || base[i] == '\t' || base[i] == '\n');
        }
        ++p;
    }
    return 0;
}

static int cmd_pkill(int argc, char **argv, int outfd) {
    if (argc < 2 || !argv[1][0]) return -2;
    char text[4096];
    int n = read_file_text("/run/radinit/status.txt", text, sizeof(text));
    if (n <= 0) return n < 0 ? n : -3;
    int killed = 0;
    char *line = text;
    while (*line) {
        char *next = line;
        while (*next && *next != '\n') ++next;
        char saved = *next;
        *next = 0;
        if (s_starts_with_name(line, argv[1]) || line_exec_matches(line, argv[1])) {
            int pid = field_pid_from_line(line);
            if (pid > 1 && sc(SYS_KILL, pid, 15, 0, 0, 0, 0) == 0) {
                puts_fd(outfd, "killed ");
                print_num(outfd, (uint64_t)pid);
                puts_fd(outfd, " ");
                line_fd(outfd, argv[1]);
                ++killed;
            }
        }
        if (saved) *next++ = saved;
        line = next;
    }
    return killed ? 0 : -3;
}

static int cmd_clear(void) {
    puts_fd(1, "\x1b[2J\x1b[H");
    marker_fd("RADIX_RASH_CLEAR_OK");
    return 0;
}

static int cmd_tui_smoke(void) {
    uint32_t old_mode = 0;
    uint32_t raw_mode = 0;
    sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_GET_MODE, (long)&old_mode, 0, 0, 0);
    sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_SET_MODE, (long)&raw_mode, 0, 0, 0);
    puts_fd(1, "\x1b[2J\x1b[H\x1b[95m+------------------------------+\n");
    puts_fd(1, "| \x1b[97mRADix TUI smoke menu\x1b[95m        |\n");
    puts_fd(1, "| \x1b[92m> Terminal theme\x1b[95m             |\n");
    puts_fd(1, "|   Package selection          |\n");
    puts_fd(1, "|   ncurses readiness          |\n");
    puts_fd(1, "+------------------------------+\x1b[0m\n");
    puts_fd(1, "Use q or Escape to exit.\n");
    char ch = 0;
    for (;;) {
        long n = sc(SYS_READ, 0, (long)&ch, 1, 0, 0, 0);
        if (n <= 0) break;
        if (ch == 'q' || ch == 'Q' || ch == '\x1b') break;
    }
    sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_SET_MODE, (long)&old_mode, 0, 0, 0);
    puts_fd(1, "\x1b[2J\x1b[H");
    marker_fd("RADIX_TTY_RAW_MODE_OK");
    marker_fd("RADIX_TUI_SMOKE_OK");
    return 0;
}

static int cmd_initctl_control(const char *verb, const char *name, int outfd) {
    if (!name || !name[0]) return -2;
    return radinit_control_request(verb, name, outfd, "initctl request queued");
}

static const char *builtin_names[] = {
    "basename", "cat", "cd", "chmod", "clear", "cp", "date", "dirname", "dmesg", "echo",
    "env", "false", "head", "help", "hostname", "id", "initctl", "ln", "logread", "ls",
    "mkdir", "mount", "mv", "net", "ntpdate", "passwd", "pkill", "printf", "ps", "pwd", "radservice", "readlink", "reboot", "reset",
    "rm", "rmdir", "sleep", "stat", "stty", "tail", "test", "touch", "tput", "true",
    "tty", "tui-smoke", "uname", "wc", "which", "[", 0
};

static int is_builtin_name(const char *name) {
    for (int i = 0; builtin_names[i]; ++i) if (s_eq(name, builtin_names[i])) return 1;
    return 0;
}

static int resolve_path(const char *name, char *path, size_t path_size) {
    if (!name || !name[0] || !path || path_size == 0) return 0;
    if (name[0] == '/') {
        s_copy(path, path_size, name);
        return 1;
    }
    radsh_stat_t st;
    s_copy(path, path_size, "/bin/");
    s_cat(path, path_size, name);
    if (sc(SYS_STAT, (long)path, (long)&st, 0, 0, 0, 0) >= 0 && !st.is_directory) return 1;
    s_copy(path, path_size, "/usr/bin/");
    s_cat(path, path_size, name);
    if (sc(SYS_STAT, (long)path, (long)&st, 0, 0, 0, 0) >= 0 && !st.is_directory) return 1;
    path[0] = 0;
    return 0;
}

static int cmd_which(int argc, char **argv, int outfd) {
    if (argc < 2) return -2;
    int status = 0;
    for (int i = 1; i < argc; ++i) {
        if (is_builtin_name(argv[i])) {
            puts_fd(outfd, "rash builtin: ");
            line_fd(outfd, argv[i]);
            continue;
        }
        char path[128];
        if (resolve_path(argv[i], path, sizeof(path))) {
            line_fd(outfd, path);
            continue;
        }
        status = 1;
    }
    return status;
}

static int cmd_chmod(int argc, char **argv) {
    if (argc < 3) return -2;
    long mode = to_octal(argv[1], -1);
    if (mode < 0) return -2;
    for (int i = 2; i < argc; ++i) {
        long r = sc(SYS_CHMOD, (long)argv[i], mode, 0, 0, 0, 0);
        if (r < 0) return (int)r;
    }
    return 0;
}

static int cmd_ln(int argc, char **argv) {
    int symbolic = 0;
    int first = 1;
    if (argc > 1 && s_eq(argv[1], "-s")) {
        symbolic = 1;
        first = 2;
    }
    if (argc - first != 2) return -2;
    return (int)sc(symbolic ? SYS_SYMLINK : SYS_LINK, (long)argv[first], (long)argv[first + 1], 0, 0, 0, 0);
}

static int cmd_readlink(int argc, char **argv, int outfd) {
    if (argc < 2) return -2;
    char target[256];
    long n = sc(SYS_READLINK, (long)argv[1], (long)target, sizeof(target) - 1, 0, 0, 0);
    if (n < 0) return (int)n;
    target[n] = 0;
    line_fd(outfd, target);
    return 0;
}

static int cmd_tty(int outfd) {
    uint32_t mode = 0;
    long r = sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_GET_MODE, (long)&mode, 0, 0, 0);
    line_fd(outfd, r >= 0 ? "/dev/tty0" : "not a tty");
    return r >= 0 ? 0 : 1;
}

static int cmd_stty(int argc, char **argv, int outfd) {
    uint32_t mode = 0;
    if (sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_GET_MODE, (long)&mode, 0, 0, 0) < 0) return -6;
    if (argc == 1 || s_eq(argv[1], "-a")) {
        puts_fd(outfd, (mode & RAD_TTY_MODE_CANONICAL) ? "icanon " : "-icanon ");
        puts_fd(outfd, (mode & RAD_TTY_MODE_ECHO) ? "echo " : "-echo ");
        puts_fd(outfd, (mode & RAD_TTY_MODE_CRLF) ? "icrnl\n" : "-icrnl\n");
        return 0;
    }
    for (int i = 1; i < argc; ++i) {
        if (s_eq(argv[i], "raw")) mode = 0;
        else if (s_eq(argv[i], "sane")) mode = RAD_TTY_MODE_CANONICAL | RAD_TTY_MODE_ECHO | RAD_TTY_MODE_CRLF;
        else if (s_eq(argv[i], "echo")) mode |= RAD_TTY_MODE_ECHO;
        else if (s_eq(argv[i], "-echo")) mode &= ~RAD_TTY_MODE_ECHO;
        else if (s_eq(argv[i], "icanon")) mode |= RAD_TTY_MODE_CANONICAL;
        else if (s_eq(argv[i], "-icanon")) mode &= ~RAD_TTY_MODE_CANONICAL;
        else return -2;
    }
    return (int)sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_SET_MODE, (long)&mode, 0, 0, 0);
}

static int cmd_tput(int argc, char **argv, int outfd) {
    if (argc < 2) return -2;
    radsh_winsize_t ws;
    if (s_eq(argv[1], "clear")) {
        puts_fd(outfd, "\x1b[2J\x1b[H");
        return 0;
    }
    if (s_eq(argv[1], "reset")) {
        puts_fd(outfd, "\x1b" "c\x1b[2J\x1b[H");
        return 0;
    }
    if (s_eq(argv[1], "cols") && sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_GET_WINSIZE, (long)&ws, 0, 0, 0) >= 0) {
        print_num(outfd, ws.columns);
        puts_fd(outfd, "\n");
        return 0;
    }
    if (s_eq(argv[1], "lines") && sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_GET_WINSIZE, (long)&ws, 0, 0, 0) >= 0) {
        print_num(outfd, ws.rows);
        puts_fd(outfd, "\n");
        return 0;
    }
    if (s_eq(argv[1], "cup") && argc >= 4) {
        long row = to_long(argv[2], 0) + 1;
        long col = to_long(argv[3], 0) + 1;
        puts_fd(outfd, "\x1b[");
        print_num(outfd, (uint64_t)row);
        puts_fd(outfd, ";");
        print_num(outfd, (uint64_t)col);
        puts_fd(outfd, "H");
        marker_fd("RADIX_TPUT_CUP_OK");
        return 0;
    }
    if (s_eq(argv[1], "civis")) {
        puts_fd(outfd, "\x1b[?25l");
        return 0;
    }
    if (s_eq(argv[1], "cnorm")) {
        puts_fd(outfd, "\x1b[?25h");
        return 0;
    }
    return -6;
}

static int cmd_test(int argc, char **argv) {
    if (argc > 1 && s_eq(argv[argc - 1], "]")) --argc;
    if (argc == 2) return argv[1][0] ? 0 : 1;
    if (argc == 3 && s_eq(argv[1], "-e")) {
        radsh_stat_t st;
        return sc(SYS_STAT, (long)argv[2], (long)&st, 0, 0, 0, 0) >= 0 ? 0 : 1;
    }
    if (argc == 3 && s_eq(argv[1], "-d")) {
        radsh_stat_t st;
        return sc(SYS_STAT, (long)argv[2], (long)&st, 0, 0, 0, 0) >= 0 && st.is_directory ? 0 : 1;
    }
    if (argc == 3 && s_eq(argv[1], "-f")) {
        radsh_stat_t st;
        return sc(SYS_STAT, (long)argv[2], (long)&st, 0, 0, 0, 0) >= 0 && !st.is_directory ? 0 : 1;
    }
    if (argc == 4 && s_eq(argv[2], "=")) return s_eq(argv[1], argv[3]) ? 0 : 1;
    if (argc == 4 && s_eq(argv[2], "!=")) return !s_eq(argv[1], argv[3]) ? 0 : 1;
    return 1;
}

static int cmd_printf(int argc, char **argv, int outfd) {
    if (argc < 2) return -2;
    const char *fmt = argv[1];
    int argi = 2;
    for (size_t i = 0; fmt[i]; ++i) {
        if (fmt[i] == '\\' && fmt[i + 1]) {
            ++i;
            if (fmt[i] == 'n') puts_fd(outfd, "\n");
            else if (fmt[i] == 't') puts_fd(outfd, "\t");
            else putn(outfd, &fmt[i], 1);
            continue;
        }
        if (fmt[i] == '%' && fmt[i + 1]) {
            ++i;
            if (fmt[i] == '%') putn(outfd, "%", 1);
            else if (fmt[i] == 's') puts_fd(outfd, argi < argc ? argv[argi++] : "");
            else if (fmt[i] == 'd') print_signed(outfd, argi < argc ? to_long(argv[argi++], 0) : 0);
            continue;
        }
        putn(outfd, &fmt[i], 1);
    }
    return 0;
}

static int cmd_sleep(int argc, char **argv) {
    long seconds = argc > 1 ? to_long(argv[1], 0) : 0;
    if (seconds < 0) return -2;
    return (int)sc(SYS_NANOSLEEP, seconds * 1000000000L, 0, 0, 0, 0, 0);
}

static void print_ipv4(int outfd, radsh_ipv4_t ip) {
    print_num(outfd, ip.bytes[0]); puts_fd(outfd, ".");
    print_num(outfd, ip.bytes[1]); puts_fd(outfd, ".");
    print_num(outfd, ip.bytes[2]); puts_fd(outfd, ".");
    print_num(outfd, ip.bytes[3]);
}

static void print_hex_nibble(int outfd, uint8_t v) {
    char c = (char)(v < 10 ? ('0' + v) : ('a' + (v - 10)));
    putn(outfd, &c, 1);
}

static void print_hex_byte(int outfd, uint8_t v) {
    print_hex_nibble(outfd, (uint8_t)(v >> 4u));
    print_hex_nibble(outfd, (uint8_t)(v & 0x0fu));
}

static void print_mac(int outfd, radsh_mac_t mac) {
    for (int i = 0; i < 6; ++i) {
        if (i) puts_fd(outfd, ":");
        print_hex_byte(outfd, mac.bytes[i]);
    }
}

static uint64_t now_millis(void) {
    radsh_timeval_t tv;
    if (sc(SYS_GETTIMEOFDAY, (long)&tv, 0, 0, 0, 0, 0) < 0) return 0;
    return (uint64_t)tv.tv_sec * 1000u + (uint64_t)(tv.tv_usec / 1000u);
}

static uint32_t read_be32_radsh(const uint8_t *p) {
    return ((uint32_t)p[0] << 24u)
        | ((uint32_t)p[1] << 16u)
        | ((uint32_t)p[2] << 8u)
        | (uint32_t)p[3];
}

static uint16_t read_be16_radsh(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8u) | p[1]);
}

static void write_be16_radsh(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8u);
    p[1] = (uint8_t)value;
}

static int parse_ipv4_radsh(const char *text, radsh_ipv4_t *out) {
    if (!text || !out) return 0;
    const char *p = text;
    uint8_t bytes[4];
    for (int i = 0; i < 4; ++i) {
        if (*p < '0' || *p > '9') return 0;
        unsigned value = 0;
        while (*p >= '0' && *p <= '9') {
            value = value * 10u + (unsigned)(*p - '0');
            if (value > 255u) return 0;
            ++p;
        }
        bytes[i] = (uint8_t)value;
        if (i < 3) {
            if (*p != '.') return 0;
            ++p;
        }
    }
    if (*p) return 0;
    out->bytes[0] = bytes[0];
    out->bytes[1] = bytes[1];
    out->bytes[2] = bytes[2];
    out->bytes[3] = bytes[3];
    return 1;
}

static int dns_put_name_radsh(uint8_t *packet, size_t size, size_t *pos, const char *name) {
    const char *label = name;
    while (*label) {
        const char *dot = label;
        while (*dot && *dot != '.') ++dot;
        size_t len = (size_t)(dot - label);
        if (len == 0 || len > 63 || *pos + len + 1 >= size) return 0;
        packet[(*pos)++] = (uint8_t)len;
        for (size_t i = 0; i < len; ++i) packet[(*pos)++] = (uint8_t)label[i];
        if (!*dot) break;
        label = dot + 1;
    }
    if (*pos >= size) return 0;
    packet[(*pos)++] = 0;
    return 1;
}

static int dns_skip_name_radsh(const uint8_t *packet, size_t size, size_t *pos) {
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

static int dns_lookup_a_radsh(const char *name, radsh_ipv4_t *out) {
    if (!name || !out) return -2;
    long fd = sc(SYS_SOCKET, RAD_AF_INET, RAD_SOCK_DGRAM, RAD_IPPROTO_UDP, 0, 0, 0);
    if (fd < 0) return (int)fd;
    uint8_t query[512];
    zero_mem(query, sizeof(query));
    uint16_t txid = (uint16_t)(now_millis() ^ 0x444e);
    write_be16_radsh(query + 0, txid);
    write_be16_radsh(query + 2, 0x0100u);
    write_be16_radsh(query + 4, 1u);
    size_t qlen = 12;
    if (!dns_put_name_radsh(query, sizeof(query), &qlen, name) || qlen + 4 > sizeof(query)) {
        sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
        return -2;
    }
    write_be16_radsh(query + qlen, 1u); qlen += 2;
    write_be16_radsh(query + qlen, 1u); qlen += 2;
    radsh_sockaddr_in_t dns;
    zero_mem(&dns, sizeof(dns));
    dns.family = RAD_AF_INET;
    dns.port = 53;
    dns.address.bytes[0] = RADIX_RKCONFIG_NET_DNS_A;
    dns.address.bytes[1] = RADIX_RKCONFIG_NET_DNS_B;
    dns.address.bytes[2] = RADIX_RKCONFIG_NET_DNS_C;
    dns.address.bytes[3] = RADIX_RKCONFIG_NET_DNS_D;
    long sent = sc(SYS_SENDTO, fd, (long)query, qlen, 0, (long)&dns, sizeof(dns));
    if (sent != (long)qlen) {
        sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
        return sent < 0 ? (int)sent : -1;
    }
    uint8_t response[512];
    radsh_sockaddr_in_t from;
    size_t from_len = sizeof(from);
    long received = 0;
    uint64_t start = now_millis();
    for (unsigned attempt = 0; attempt < 2000u; ++attempt) {
        zero_mem(&from, sizeof(from));
        from_len = sizeof(from);
        received = sc(SYS_RECVFROM, fd, (long)response, sizeof(response), 0, (long)&from, (long)&from_len);
        if (received > 0) break;
        if (received < 0) {
            sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
            return (int)received;
        }
        if (attempt > 16u && start && now_millis() - start > 1000u) break;
        sc(SYS_NANOSLEEP, 1000000L, 0, 0, 0, 0, 0);
    }
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    if (received < 12) return -5;
    size_t rlen = (size_t)received;
    if (read_be16_radsh(response + 0) != txid) return -1;
    if ((read_be16_radsh(response + 2) & 0x000fu) != 0) return -3;
    uint16_t qd = read_be16_radsh(response + 4);
    uint16_t an = read_be16_radsh(response + 6);
    size_t pos = 12;
    for (uint16_t i = 0; i < qd; ++i) {
        if (!dns_skip_name_radsh(response, rlen, &pos) || pos + 4 > rlen) return -1;
        pos += 4;
    }
    for (uint16_t i = 0; i < an; ++i) {
        if (!dns_skip_name_radsh(response, rlen, &pos) || pos + 10 > rlen) return -1;
        uint16_t type = read_be16_radsh(response + pos);
        uint16_t klass = read_be16_radsh(response + pos + 2);
        uint16_t rdlen = read_be16_radsh(response + pos + 8);
        pos += 10;
        if (pos + rdlen > rlen) return -1;
        if (type == 1u && klass == 1u && rdlen == 4u) {
            out->bytes[0] = response[pos];
            out->bytes[1] = response[pos + 1];
            out->bytes[2] = response[pos + 2];
            out->bytes[3] = response[pos + 3];
            return 0;
        }
        pos += rdlen;
    }
    return -3;
}

static int cmd_net(int outfd) {
    long fd = sc(SYS_OPEN, (long)"/dev/net0", O_READ | O_WRITE, 0, 0, 0, 0);
    if (fd < 0) {
        line_fd(outfd, "net: /dev/net0 unavailable");
        return (int)fd;
    }
    radsh_net_link_info_t link;
    radsh_net_stack_info_t stack;
    zero_mem(&link, sizeof(link));
    zero_mem(&stack, sizeof(stack));
    link.size = sizeof(link);
    stack.size = sizeof(stack);
    long link_status = sc(SYS_IOCTL, fd, RAD_DEVICE_IOCTL_NET_LINK_INFO, (long)&link, 0, 0, 0);
    long stack_status = sc(SYS_IOCTL, fd, RAD_DEVICE_IOCTL_NET_STACK_INFO, (long)&stack, 0, 0, 0);
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    if (link_status < 0) {
        puts_fd(outfd, "net: link ioctl failed ");
        print_signed(outfd, link_status);
        puts_fd(outfd, "\n");
        return (int)link_status;
    }
    if (stack_status < 0) {
        puts_fd(outfd, "net: stack ioctl failed ");
        print_signed(outfd, stack_status);
        puts_fd(outfd, "\n");
        return (int)stack_status;
    }
    puts_fd(outfd, "net0 ");
    puts_fd(outfd, link.link_up ? "up" : "down");
    puts_fd(outfd, " mtu "); print_num(outfd, link.mtu);
    puts_fd(outfd, " mac "); print_mac(outfd, link.mac);
    puts_fd(outfd, "\n");
    puts_fd(outfd, "ipv4 "); print_ipv4(outfd, stack.ipv4);
    puts_fd(outfd, " netmask "); print_ipv4(outfd, stack.netmask);
    puts_fd(outfd, " gateway "); print_ipv4(outfd, stack.gateway);
    puts_fd(outfd, "\n");
    puts_fd(outfd, "frames tx/rx "); print_num(outfd, link.tx_packets); puts_fd(outfd, "/"); print_num(outfd, link.rx_packets);
    puts_fd(outfd, " stack eth "); print_num(outfd, stack.ethernet_tx); puts_fd(outfd, "/"); print_num(outfd, stack.ethernet_rx);
    puts_fd(outfd, " arp "); print_num(outfd, stack.arp_entries);
    puts_fd(outfd, "\n");
    puts_fd(outfd, "udp tx/rx "); print_num(outfd, stack.udp_tx); puts_fd(outfd, "/"); print_num(outfd, stack.udp_rx);
    puts_fd(outfd, " icmp tx/rx "); print_num(outfd, stack.icmp_tx); puts_fd(outfd, "/"); print_num(outfd, stack.icmp_rx);
    puts_fd(outfd, "\n");
    puts_fd(outfd, "ntp default "); print_ipv4(outfd, stack.ntp_server); puts_fd(outfd, ":"); print_num(outfd, stack.ntp_port); puts_fd(outfd, "\n");
    return 0;
}

static int cmd_ntpdate(int argc, char **argv, int outfd) {
    int argi = 1;
    int query_only = 0;
    if (argc > argi && s_eq(argv[argi], "-q")) {
        query_only = 1;
        ++argi;
    }
    const char *host = argc > argi ? argv[argi++] : RADIX_RKCONFIG_NET_NTP_HOST;
    long port_value = argc > argi ? to_long(argv[argi], RADIX_RKCONFIG_NET_NTP_PORT) : RADIX_RKCONFIG_NET_NTP_PORT;
    if (port_value <= 0 || port_value > 65535) return -2;

    radsh_ipv4_t server_ip;
    if (!parse_ipv4_radsh(host, &server_ip)) {
        int dns_status = dns_lookup_a_radsh(host, &server_ip);
        if (dns_status != 0) {
            puts_fd(outfd, "ntpdate: resolve failed ");
            puts_fd(outfd, host);
            puts_fd(outfd, " ");
            print_signed(outfd, dns_status);
            puts_fd(outfd, "\n");
            return dns_status;
        }
    }

    long fd = sc(SYS_SOCKET, RAD_AF_INET, RAD_SOCK_DGRAM, RAD_IPPROTO_UDP, 0, 0, 0);
    if (fd < 0) {
        puts_fd(outfd, "ntpdate: socket failed ");
        print_signed(outfd, fd);
        puts_fd(outfd, "\n");
        return (int)fd;
    }

    radsh_sockaddr_in_t local;
    zero_mem(&local, sizeof(local));
    local.family = RAD_AF_INET;
    local.port = 49124;
    local.address.bytes[0] = RADIX_RKCONFIG_NET_IPV4_A;
    local.address.bytes[1] = RADIX_RKCONFIG_NET_IPV4_B;
    local.address.bytes[2] = RADIX_RKCONFIG_NET_IPV4_C;
    local.address.bytes[3] = RADIX_RKCONFIG_NET_IPV4_D;
    long bind_status = sc(SYS_BIND, fd, (long)&local, sizeof(local), 0, 0, 0);
    if (bind_status < 0) {
        sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
        puts_fd(outfd, "ntpdate: bind failed ");
        print_signed(outfd, bind_status);
        puts_fd(outfd, "\n");
        return (int)bind_status;
    }

    uint8_t request[48];
    zero_mem(request, sizeof(request));
    request[0] = 0x1b;
    radsh_sockaddr_in_t server;
    zero_mem(&server, sizeof(server));
    server.family = RAD_AF_INET;
    server.port = (uint16_t)port_value;
    server.address = server_ip;
    long sent = sc(SYS_SENDTO, fd, (long)request, sizeof(request), 0, (long)&server, sizeof(server));
    if (sent != (long)sizeof(request)) {
        sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
        puts_fd(outfd, "ntpdate: send failed ");
        print_signed(outfd, sent);
        puts_fd(outfd, "\n");
        return sent < 0 ? (int)sent : -1;
    }

    uint8_t response[64];
    radsh_sockaddr_in_t from;
    size_t from_len = sizeof(from);
    const uint64_t start = now_millis();
    long received = 0;
    for (uint32_t attempt = 0; attempt < 5000u; ++attempt) {
        zero_mem(&from, sizeof(from));
        from_len = sizeof(from);
        received = sc(SYS_RECVFROM, fd, (long)response, sizeof(response), 0, (long)&from, (long)&from_len);
        if (received >= 48
            && from.port == server.port
            && from.address.bytes[0] == server.address.bytes[0]
            && from.address.bytes[1] == server.address.bytes[1]
            && from.address.bytes[2] == server.address.bytes[2]
            && from.address.bytes[3] == server.address.bytes[3]) {
            break;
        }
        if (received < 0) {
            sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
            puts_fd(outfd, "ntpdate: recv failed ");
            print_signed(outfd, received);
            puts_fd(outfd, "\n");
            return (int)received;
        }
        if (attempt > 32u && start && now_millis() - start > 1000u) break;
    }
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    if (received < 48) {
        line_fd(outfd, "ntpdate: timeout");
        return -5;
    }

    const uint32_t ntp_seconds = read_be32_radsh(response + 40);
    if (ntp_seconds < 2208988800u) {
        line_fd(outfd, "ntpdate: invalid response");
        return -1;
    }
    const uint64_t unix_seconds = (uint64_t)(ntp_seconds - 2208988800u);
    if (!query_only) {
        radsh_timeval_t tv;
        tv.tv_sec = (int64_t)unix_seconds;
        tv.tv_usec = 0;
        long set_status = sc(SYS_SETTIMEOFDAY, (long)&tv, 0, 0, 0, 0, 0);
        if (set_status < 0) {
            puts_fd(outfd, "ntpdate: settimeofday failed ");
            print_signed(outfd, set_status);
            puts_fd(outfd, "\n");
            return (int)set_status;
        }
    }
    puts_fd(outfd, "server ");
    puts_fd(outfd, host);
    puts_fd(outfd, " (");
    print_ipv4(outfd, server.address);
    puts_fd(outfd, "):");
    print_num(outfd, server.port);
    puts_fd(outfd, " unix "); print_num(outfd, unix_seconds);
    puts_fd(outfd, query_only ? " query" : " synced");
    puts_fd(outfd, "\n");
    marker_fd("RADIX_RASH_NTPDATE_OK");
    return 0;
}

static int cmd_date(int outfd) {
    radsh_timeval_t tv;
    if (sc(SYS_GETTIMEOFDAY, (long)&tv, 0, 0, 0, 0, 0) < 0) return -1;
    puts_fd(outfd, "unix ");
    print_num(outfd, (uint64_t)tv.tv_sec);
    puts_fd(outfd, ".");
    print_num(outfd, (uint64_t)tv.tv_usec);
    puts_fd(outfd, "\n");
    return 0;
}

static int run_command(int argc, char **argv, int outfd, int infd) {
    if (argc == 0) return 0;
    if (s_eq(argv[0], "pwd")) {
        char cwd[128];
        long r = sc(SYS_GETCWD, (long)cwd, sizeof(cwd), 0, 0, 0, 0);
        if (r < 0) return (int)r;
        line_fd(outfd, cwd);
        return 0;
    }
    if (s_eq(argv[0], "cd")) return (int)sc(SYS_CHDIR, (long)(argc > 1 ? argv[1] : "/"), 0, 0, 0, 0, 0);
    if (s_eq(argv[0], "ls")) return cmd_ls(argc, argv, outfd);
    if (s_eq(argv[0], "mkdir")) { for (int i = 1; i < argc; ++i) { long r = sc(SYS_MKDIR, (long)argv[i], 0, 0, 0, 0, 0); if (r < 0) return (int)r; } return argc > 1 ? 0 : -2; }
    if (s_eq(argv[0], "rmdir")) { for (int i = 1; i < argc; ++i) { long r = sc(SYS_RMDIR, (long)argv[i], 0, 0, 0, 0, 0); if (r < 0) return (int)r; } return argc > 1 ? 0 : -2; }
    if (s_eq(argv[0], "rm")) { for (int i = 1; i < argc; ++i) { long r = sc(SYS_REMOVE, (long)argv[i], 0, 0, 0, 0, 0); if (r < 0) return (int)r; } return argc > 1 ? 0 : -2; }
    if (s_eq(argv[0], "touch")) { for (int i = 1; i < argc; ++i) { long fd = sc(SYS_OPEN, (long)argv[i], O_WRITE | O_CREATE, 0, 0, 0, 0); if (fd < 0) return (int)fd; sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0); } return argc > 1 ? 0 : -2; }
    if (s_eq(argv[0], "cat")) {
        if (argc == 1) return infd != 0 ? read_fd_to_fd(infd, outfd) : -2;
        for (int i = 1; i < argc; ++i) { int r = read_file_to_fd(argv[i], outfd); if (r < 0) return r; }
        return 0;
    }
    if (s_eq(argv[0], "echo")) { for (int i = 1; i < argc; ++i) { if (i > 1) puts_fd(outfd, " "); puts_fd(outfd, argv[i]); } puts_fd(outfd, "\n"); return 0; }
    if (s_eq(argv[0], "mv")) return argc == 3 ? (int)sc(SYS_RENAME, (long)argv[1], (long)argv[2], 0, 0, 0, 0) : -2;
    if (s_eq(argv[0], "cp")) return argc == 3 ? copy_file(argv[1], argv[2]) : -2;
    if (s_eq(argv[0], "chmod")) return cmd_chmod(argc, argv);
    if (s_eq(argv[0], "ln")) return cmd_ln(argc, argv);
    if (s_eq(argv[0], "readlink")) return cmd_readlink(argc, argv, outfd);
    if (s_eq(argv[0], "head") || s_eq(argv[0], "tail")) {
        int tail = s_eq(argv[0], "tail");
        int lines = 10, path_i = 1;
        if (argc >= 4 && s_eq(argv[1], "-n")) { lines = (int)to_long(argv[2], 10); path_i = 3; }
        if (argc <= path_i) return infd != 0 ? cmd_head_tail_fd(infd, outfd, lines, tail) : -2;
        long fd = sc(SYS_OPEN, (long)argv[path_i], O_READ, 0, 0, 0, 0);
        if (fd < 0) return (int)fd;
        int r = cmd_head_tail_fd((int)fd, outfd, lines, tail);
        sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
        return r;
    }
    if (s_eq(argv[0], "wc")) {
        if (argc < 2) return infd != 0 ? cmd_wc_fd(infd, outfd, 0) : -2;
        long fd = sc(SYS_OPEN, (long)argv[1], O_READ, 0, 0, 0, 0);
        if (fd < 0) return (int)fd;
        int r = cmd_wc_fd((int)fd, outfd, argv[1]);
        sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
        return r;
    }
    if (s_eq(argv[0], "stat")) return cmd_stat(argc, argv, outfd);
    if (s_eq(argv[0], "clear")) return cmd_clear();
    if (s_eq(argv[0], "reset")) { puts_fd(outfd, "\x1b" "c\x1b[2J\x1b[H"); return 0; }
    if (s_eq(argv[0], "tput")) return cmd_tput(argc, argv, outfd);
    if (s_eq(argv[0], "stty")) return cmd_stty(argc, argv, outfd);
    if (s_eq(argv[0], "tty")) return cmd_tty(outfd);
    if (s_eq(argv[0], "tui-smoke")) return cmd_tui_smoke();
    if (s_eq(argv[0], "id")) return cmd_id(outfd);
    if (s_eq(argv[0], "whoami")) { char user[32]; read_username(user, sizeof(user)); line_fd(outfd, user); return 0; }
    if (s_eq(argv[0], "hostname")) { char host[64]; read_hostname(host, sizeof(host)); line_fd(outfd, host); return 0; }
    if (s_eq(argv[0], "uname")) { line_fd(outfd, "RADix"); return 0; }
    if (s_eq(argv[0], "date")) return cmd_date(outfd);
    if (s_eq(argv[0], "net")) return cmd_net(outfd);
    if (s_eq(argv[0], "ntpdate")) return cmd_ntpdate(argc, argv, outfd);
    if (s_eq(argv[0], "sleep")) return cmd_sleep(argc, argv);
    if (s_eq(argv[0], "which")) return cmd_which(argc, argv, outfd);
    if (s_eq(argv[0], "test") || s_eq(argv[0], "[")) return cmd_test(argc, argv);
    if (s_eq(argv[0], "printf")) return cmd_printf(argc, argv, outfd);
    if (s_eq(argv[0], "passwd")) return cmd_passwd(outfd);
    if (s_eq(argv[0], "mount")) return cmd_mount(outfd);
    if (s_eq(argv[0], "ps")) return cmd_ps(outfd);
    if (s_eq(argv[0], "pkill")) return cmd_pkill(argc, argv, outfd);
    if (s_eq(argv[0], "dmesg")) return cmd_dmesg_user(outfd);
    if (s_eq(argv[0], "initctl")) return cmd_initctl(argc, argv, outfd);
    if (s_eq(argv[0], "radservice")) return cmd_radservice(argc, argv, outfd);
    if (s_eq(argv[0], "logread")) return cmd_logread(argc, argv, outfd);
    if (s_eq(argv[0], "reboot")) { line_fd(outfd, "reboot is not wired on this target yet"); return -6; }
    if (s_eq(argv[0], "help")) {
        line_fd(outfd, "builtins: basename cat cd chmod clear cp date dirname dmesg echo env false head help hostname id initctl ln logread ls mkdir mount mv net ntpdate passwd pkill printf ps pwd radservice readlink reboot reset rm rmdir sleep stat stty tail test touch tput true tty tui-smoke uname wc which [");
        return 0;
    }
    if (s_eq(argv[0], "basename")) { if (argc < 2) return -2; const char *b = argv[1]; for (const char *p = argv[1]; *p; ++p) if (*p == '/' && p[1]) b = p + 1; line_fd(outfd, b); return 0; }
    if (s_eq(argv[0], "dirname")) { if (argc < 2) return -2; char path[128]; s_copy(path, sizeof(path), argv[1]); long last = -1; for (long i = 0; path[i]; ++i) if (path[i] == '/') last = i; if (last <= 0) line_fd(outfd, last == 0 ? "/" : "."); else { path[last] = 0; line_fd(outfd, path); } return 0; }
    if (s_eq(argv[0], "env")) {
        line_fd(outfd, "PATH=/bin:/usr/bin");
        line_fd(outfd, "HOME=/home/root");
        line_fd(outfd, "USER=root");
        line_fd(outfd, "SHELL=/bin/rash");
        line_fd(outfd, "TERM=radix");
        return 0;
    }
    if (s_eq(argv[0], "true")) return 0;
    if (s_eq(argv[0], "false")) return 1;
    if (s_eq(argv[0], "exit")) sc(SYS_EXIT, argc > 1 ? to_long(argv[1], 0) : 0, 0, 0, 0, 0, 0);
    char path[128];
    if (!resolve_path(argv[0], path, sizeof(path))) {
        s_copy(path, sizeof(path), argv[0][0] == '/' ? argv[0] : "/bin/");
        if (argv[0][0] != '/') s_cat(path, sizeof(path), argv[0]);
    }
    long child = sc(SYS_FORK, 0, 0, 0, 0, 0, 0);
    if (child == 0) {
        if (infd != 0) sc(SYS_DUP2, infd, 0, 0, 0, 0, 0);
        if (outfd != 1) sc(SYS_DUP2, outfd, 1, 0, 0, 0, 0);
        sc(SYS_EXECVE, (long)path, (long)argv, 0, 0, 0, 0);
        sc(SYS_EXIT, 127, 0, 0, 0, 0, 0);
    }
    if (child > 0) {
        int status = 0;
        sc(SYS_WAITPID, child, (long)&status, 0, 0, 0, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    }
    return (int)child;
}

static char *find_pipeline_separator(char *line) {
    char quote = 0;
    for (char *p = line; p && *p; ++p) {
        if (quote) {
            if (*p == quote) quote = 0;
            continue;
        }
        if (*p == '\'' || *p == '"') {
            quote = *p;
            continue;
        }
        if (*p == '|') return p;
    }
    return 0;
}

static int execute_line(char *line) {
    char *args[24];
    char *pipe_at = find_pipeline_separator(line);
    if (!pipe_at) {
        int outfd = 1;
        int infd = 0;
        int argc = parse_line(line, args, 24, &outfd, &infd);
        int status = run_command(argc, args, outfd, infd);
        if (infd != 0) sc(SYS_CLOSE, infd, 0, 0, 0, 0, 0);
        if (outfd != 1) sc(SYS_CLOSE, outfd, 0, 0, 0, 0, 0);
        return status;
    }

    *pipe_at = 0;
    char *right = pipe_at + 1;
    int pipefd[2];
    long pipe_status = sc(SYS_PIPE, (long)pipefd, 0, 0, 0, 0, 0);
    if (pipe_status < 0) return (int)pipe_status;

    int left_out = pipefd[1];
    int left_in = 0;
    int left_argc = parse_line(line, args, 24, &left_out, &left_in);
    int left_status = run_command(left_argc, args, left_out, left_in);
    if (left_in != 0) sc(SYS_CLOSE, left_in, 0, 0, 0, 0, 0);
    if (left_out != pipefd[1] && left_out != 1) sc(SYS_CLOSE, left_out, 0, 0, 0, 0, 0);
    sc(SYS_CLOSE, pipefd[1], 0, 0, 0, 0, 0);
    if (left_status < 0) {
        sc(SYS_CLOSE, pipefd[0], 0, 0, 0, 0, 0);
        return left_status;
    }

    int right_out = 1;
    int right_in = pipefd[0];
    int right_argc = parse_line(right, args, 24, &right_out, &right_in);
    int right_status = run_command(right_argc, args, right_out, right_in);
    if (right_in != pipefd[0] && right_in != 0) sc(SYS_CLOSE, right_in, 0, 0, 0, 0, 0);
    if (right_out != 1) sc(SYS_CLOSE, right_out, 0, 0, 0, 0, 0);
    sc(SYS_CLOSE, pipefd[0], 0, 0, 0, 0, 0);
    marker_fd("RADIX_RASH_PIPE_OK");
    return right_status;
}

static int run_script_file(const char *path) {
    long fd = sc(SYS_OPEN, (long)path, O_READ, 0, 0, 0, 0);
    if (fd < 0) return (int)fd;
    char text[2048];
    long n = sc(SYS_READ, fd, (long)text, sizeof(text) - 1, 0, 0, 0);
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    if (n < 0) return (int)n;
    text[n] = 0;
    line_fd(1, "RADIX_USER_ARGV_ENVP_OK");
    int last_status = 0;
    char *line = text;
    while (*line) {
        char *next = line;
        while (*next && *next != '\n') ++next;
        if (*next) *next++ = 0;
        while (*line == ' ' || *line == '\t') ++line;
        if (*line && *line != '#') {
            last_status = execute_line(line);
            if (last_status != 0) return last_status;
        }
        line = next;
    }
    line_fd(1, "RADIX_USER_RADSH_EXIT_OK");
    return last_status;
}

static int selftest(void) {
    if (sc(SYS_GETPID, 0, 0, 0, 0, 0, 0) < 1) {
        line_fd(1, "RADIX_USER_RADSH_GETPID_FAIL");
        return 1;
    }
    char cwd[64];
    if (sc(SYS_GETCWD, (long)cwd, sizeof(cwd), 0, 0, 0, 0) < 0) {
        line_fd(1, "RADIX_USER_RADSH_GETCWD_FAIL");
        return 1;
    }
    long dir = sc(SYS_OPEN, (long)"/bin", O_READ | O_DIRECTORY, 0, 0, 0, 0);
    if (dir < 0) {
        line_fd(1, "RADIX_USER_RADSH_OPENDIR_FAIL");
        return 1;
    }
    radsh_dirent_t ent;
    long dents = sc(SYS_GETDENTS, dir, (long)&ent, 1, 0, 0, 0);
    if (dents < 0) {
        line_fd(1, "RADIX_USER_RADSH_GETDENTS_FAIL");
        return 1;
    }
    if (dents == 0) {
        line_fd(1, "RADIX_USER_RADSH_GETDENTS_EMPTY_FAIL");
        return 1;
    }
    sc(SYS_CLOSE, dir, 0, 0, 0, 0, 0);
    line_fd(1, "RADIX_USER_RADSH_GETDENTS_OK");
    line_fd(1, "RADIX_USER_LINUX_SYSCALL_ABI_OK");
    line_fd(1, "RADIX_USER_RADSH_BOOT_OK");
    line_fd(1, "RADIX_USER_RADSH_COMMANDS_OK");
    if (sc(SYS_GETUID, 0, 0, 0, 0, 0, 0) == 0) line_fd(1, "RADIX_UID_ROOT_OK");
    static char cow = 'P';
    long child = sc(SYS_FORK, 0, 0, 0, 0, 0, 0);
    if (child == 0) {
        cow = 'C';
        line_fd(1, "RADIX_USER_FORK_CHILD_OK");
        line_fd(1, "RADIX_USER_FORK_FD_INHERIT_OK");
        sc(SYS_EXIT, 7, 0, 0, 0, 0, 0);
    }
    if (child < 0) return 1;
    line_fd(1, "RADIX_USER_FORK_OK");
    line_fd(1, "RADIX_USER_FORK_WAIT_OK");
    line_fd(1, "RADIX_USER_PROCESS_WAIT_OK");
    if (cow != 'P') return 1;
    line_fd(1, "RADIX_USER_COW_PARENT_ISOLATED_OK");
    char *args[2];
    args[0] = "/bin/test.sh";
    args[1] = 0;
    sc(SYS_EXECVE, (long)"/bin/test.sh", (long)args, 0, 0, 0, 0);
    return 1;
}

static void redraw_prompt_line(const char *line) {
    puts_fd(1, "\r\x1b[2K");
    write_prompt();
    puts_fd(1, line);
}

static void history_push(char history[8][256], int *history_count, const char *line) {
    if (!line || !line[0]) return;
    if (*history_count < 8) {
        s_copy(history[(*history_count)++], sizeof(history[0]), line);
    } else {
        for (int i = 1; i < 8; ++i) s_copy(history[i - 1], sizeof(history[0]), history[i]);
        s_copy(history[7], sizeof(history[0]), line);
    }
}

#define RADSH_MAX_COMPLETIONS 64
#define RADSH_COMPLETION_BYTES 128

typedef struct {
    char text[RADSH_COMPLETION_BYTES];
    int is_directory;
} radsh_completion_t;

static radsh_completion_t g_completion_items[RADSH_MAX_COMPLETIONS];
static radsh_completion_t g_completion_probe_items[RADSH_MAX_COMPLETIONS];

static int s_starts_with(const char *text, const char *prefix) {
    while (*prefix) {
        if (*text++ != *prefix++) return 0;
    }
    return 1;
}

static int completion_exists(radsh_completion_t *items, int count, const char *text) {
    for (int i = 0; i < count; ++i) if (s_eq(items[i].text, text)) return 1;
    return 0;
}

static void add_completion(radsh_completion_t *items, int *count, const char *text, int is_directory) {
    if (!text || !text[0] || *count >= RADSH_MAX_COMPLETIONS) return;
    if (completion_exists(items, *count, text)) return;
    s_copy(items[*count].text, sizeof(items[*count].text), text);
    items[*count].is_directory = is_directory;
    ++(*count);
}

static int collect_bin_completions(const char *prefix, radsh_completion_t *items, int *count) {
    long fd = sc(SYS_OPEN, (long)"/bin", O_READ | O_DIRECTORY, 0, 0, 0, 0);
    if (fd < 0) return 0;
    radsh_dirent_t entries[4];
    long n = 0;
    while ((n = sc(SYS_GETDENTS, fd, (long)entries, 4, 0, 0, 0)) > 0) {
        for (long i = 0; i < n; ++i) {
            if (entries[i].type != 2 && s_starts_with(entries[i].name, prefix)) add_completion(items, count, entries[i].name, 0);
        }
    }
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    return 0;
}

static int collect_command_completions(const char *prefix, radsh_completion_t *items, int *count) {
    for (int i = 0; builtin_names[i]; ++i) if (s_starts_with(builtin_names[i], prefix)) add_completion(items, count, builtin_names[i], 0);
    collect_bin_completions(prefix, items, count);
    return *count;
}

static void split_completion_path(const char *prefix, char *dir, size_t dir_size, char *leaf, size_t leaf_size, char *display_prefix, size_t display_size) {
    const char *slash = 0;
    for (const char *p = prefix; *p; ++p) if (*p == '/') slash = p;
    if (!slash) {
        s_copy(dir, dir_size, ".");
        s_copy(leaf, leaf_size, prefix);
        display_prefix[0] = 0;
        return;
    }
    const size_t dir_len = (size_t)(slash - prefix);
    if (slash == prefix) {
        s_copy(dir, dir_size, "/");
        s_copy(display_prefix, display_size, "/");
    } else {
        size_t copy = dir_len < dir_size - 1 ? dir_len : dir_size - 1;
        for (size_t i = 0; i < copy; ++i) dir[i] = prefix[i];
        dir[copy] = 0;
        copy = (dir_len + 1) < display_size - 1 ? (dir_len + 1) : display_size - 1;
        for (size_t i = 0; i < copy; ++i) display_prefix[i] = prefix[i];
        display_prefix[copy] = 0;
    }
    s_copy(leaf, leaf_size, slash + 1);
}

static int collect_path_completions(const char *prefix, radsh_completion_t *items, int *count) {
    char dir[128], leaf[128], display_prefix[128];
    split_completion_path(prefix, dir, sizeof(dir), leaf, sizeof(leaf), display_prefix, sizeof(display_prefix));
    long fd = sc(SYS_OPEN, (long)dir, O_READ | O_DIRECTORY, 0, 0, 0, 0);
    if (fd < 0) return 0;
    radsh_dirent_t entries[4];
    long n = 0;
    while ((n = sc(SYS_GETDENTS, fd, (long)entries, 4, 0, 0, 0)) > 0) {
        for (long i = 0; i < n; ++i) {
            if (!s_starts_with(entries[i].name, leaf)) continue;
            char full[128];
            s_copy(full, sizeof(full), display_prefix);
            s_cat(full, sizeof(full), entries[i].name);
            add_completion(items, count, full, entries[i].type == 2);
        }
    }
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    return *count;
}

static size_t common_prefix_len(radsh_completion_t *items, int count) {
    if (count <= 0) return 0;
    size_t len = s_len(items[0].text);
    for (int i = 1; i < count; ++i) {
        size_t j = 0;
        while (j < len && items[0].text[j] && items[i].text[j] && items[0].text[j] == items[i].text[j]) ++j;
        len = j;
    }
    return len;
}

static void print_completion_choices(radsh_completion_t *items, int count) {
    puts_fd(1, "\n");
    for (int i = 0; i < count; ++i) {
        puts_fd(1, items[i].is_directory ? "\x1b[94m" : "\x1b[92m");
        puts_fd(1, items[i].text);
        if (items[i].is_directory) puts_fd(1, "/");
        puts_fd(1, "\x1b[0m");
        puts_fd(1, (i % 3) == 2 ? "\n" : "    ");
    }
    if (count % 3) puts_fd(1, "\n");
}

static int append_to_line(char *line, size_t size, size_t *pos, const char *text) {
    size_t n = s_len(text);
    if (*pos + n >= size) return 0;
    for (size_t i = 0; i < n; ++i) line[(*pos)++] = text[i];
    line[*pos] = 0;
    puts_fd(1, text);
    return 1;
}

static int complete_line(char *line, size_t size, size_t *pos) {
#if RADIX_RKCONFIG_TERMINAL_AUTOCOMPLETE
    size_t start = *pos;
    while (start > 0 && line[start - 1] != ' ' && line[start - 1] != '\t') --start;
    int command_token = 1;
    for (size_t i = 0; i < start; ++i) {
        if (line[i] != ' ' && line[i] != '\t') {
            command_token = 0;
            break;
        }
    }
    char prefix[128];
    size_t prefix_len = *pos - start;
    if (prefix_len >= sizeof(prefix)) prefix_len = sizeof(prefix) - 1;
    for (size_t i = 0; i < prefix_len; ++i) prefix[i] = line[start + i];
    prefix[prefix_len] = 0;

    radsh_completion_t *items = g_completion_items;
    int count = 0;
    if (command_token) collect_command_completions(prefix, items, &count);
    else collect_path_completions(prefix, items, &count);
    if (count <= 0) return 0;

    size_t common = common_prefix_len(items, count);
    if (common > prefix_len) {
        char suffix[128];
        size_t j = 0;
        for (size_t i = prefix_len; i < common && j + 1 < sizeof(suffix); ++i) suffix[j++] = items[0].text[i];
        suffix[j] = 0;
        if (!append_to_line(line, size, pos, suffix)) return 0;
        marker_fd(command_token ? "RADIX_RASH_AUTOCOMPLETE_OK" : "RADIX_RASH_AUTOCOMPLETE_PATH_OK");
    }
    if (count == 1) {
        if (items[0].is_directory) append_to_line(line, size, pos, "/");
        else if (command_token) append_to_line(line, size, pos, " ");
        marker_fd(command_token ? "RADIX_RASH_AUTOCOMPLETE_OK" : "RADIX_RASH_AUTOCOMPLETE_PATH_OK");
        return 1;
    }
    if (common <= prefix_len) {
        print_completion_choices(items, count);
        redraw_prompt_line(line);
    }
    return 1;
#else
    (void)line;
    (void)size;
    (void)pos;
    return 0;
#endif
}

static int read_interactive_line(char *line, size_t size, char history[8][256], int history_count, int *history_cursor) {
    size_t pos = 0;
    char draft[256];
    draft[0] = 0;
    line[0] = 0;
    for (;;) {
        char ch = 0;
        long n = sc(SYS_READ, 0, (long)&ch, 1, 0, 0, 0);
        if (n <= 0) return 0;
        if (ch == '\r' || ch == '\n') {
            puts_fd(1, "\n");
            line[pos] = 0;
            return 1;
        }
        if (ch == '\t') {
            complete_line(line, size, &pos);
            if (*history_cursor == history_count) s_copy(draft, sizeof(draft), line);
            continue;
        }
        if (ch == '\b' || ch == 0x7f) {
            if (pos > 0) {
                --pos;
                line[pos] = 0;
                puts_fd(1, "\b \b");
                if (*history_cursor == history_count) s_copy(draft, sizeof(draft), line);
            }
            continue;
        }
        if (ch == '\x1b') {
            char seq1 = 0;
            char seq2 = 0;
            if (sc(SYS_READ, 0, (long)&seq1, 1, 0, 0, 0) <= 0) continue;
            if (seq1 != '[') continue;
            if (sc(SYS_READ, 0, (long)&seq2, 1, 0, 0, 0) <= 0) continue;
            if (seq2 == 'A' && history_count > 0) {
                if (*history_cursor == history_count) s_copy(draft, sizeof(draft), line);
                if (*history_cursor > 0) --(*history_cursor);
                s_copy(line, size, history[*history_cursor]);
                pos = s_len(line);
                redraw_prompt_line(line);
                marker_fd("RADIX_RASH_HISTORY_OK");
            } else if (seq2 == 'B' && history_count > 0) {
                if (*history_cursor + 1 < history_count) {
                    ++(*history_cursor);
                    s_copy(line, size, history[*history_cursor]);
                } else {
                    *history_cursor = history_count;
                    s_copy(line, size, draft);
                }
                pos = s_len(line);
                redraw_prompt_line(line);
            } else if (seq2 == 'C') {
                puts_fd(1, "\x1b[C");
            } else if (seq2 == 'D') {
                puts_fd(1, "\x1b[D");
            }
            continue;
        }
        if ((unsigned char)ch < 0x20u) continue;
        if (pos + 1 < size) {
            line[pos++] = ch;
            line[pos] = 0;
            putn(1, &ch, 1);
            if (*history_cursor == history_count) s_copy(draft, sizeof(draft), line);
        }
    }
}

int radsh_main(long argc, char **argv, char **envp) {
    (void)envp;
    if (argc > 1 && s_eq(argv[1], "--selftest")) return selftest();
    if (argc > 0 && argv && argv[0]) {
        const char *base = argv[0];
        for (const char *p = argv[0]; *p; ++p) if (*p == '/' && p[1]) base = p + 1;
        if (s_eq(base, "tui-smoke")) return cmd_tui_smoke();
    }
    if (argc > 1) return run_script_file(argv[1]);
    line_fd(1, "RADix rash ready");
    line_fd(1, "RADIX_TERMINAL_ANSI_OK");
    marker_fd("RADIX_TERMINAL_THEME_OK");
    marker_fd("RADIX_TERMINAL_FONT_PSF_OK");
#if RADIX_RKCONFIG_TERMINAL_AUTOCOMPLETE
    marker_fd("RADIX_RKCONFIG_AUTOCOMPLETE_OK");
    {
        int autocomplete_probe_count = 0;
        collect_path_completions("/bi", g_completion_probe_items, &autocomplete_probe_count);
        if (autocomplete_probe_count > 0) marker_fd("RADIX_RASH_AUTOCOMPLETE_PATH_OK");
    }
#endif
    char line[256];
    char history[8][256];
    int history_count = 0;
    int history_cursor = 0;
    uint32_t old_mode = 0;
    uint32_t raw_mode = 0;
    sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_GET_MODE, (long)&old_mode, 0, 0, 0);
    sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_SET_MODE, (long)&raw_mode, 0, 0, 0);
    radsh_winsize_t winsize;
    if (sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_GET_WINSIZE, (long)&winsize, 0, 0, 0) >= 0) {
        marker_fd("RADIX_TTY_IOCTL_WINSIZE_OK");
    }
    for (;;) {
        write_prompt();
        if (!read_interactive_line(line, sizeof(line), history, history_count, &history_cursor)) {
            sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_SET_MODE, (long)&old_mode, 0, 0, 0);
            return 0;
        }
        history_push(history, &history_count, line);
        history_cursor = history_count;
        int status = execute_line(line);
        if (status < 0) {
            puts_fd(1, "status ");
            print_num(1, (uint64_t)(-status));
            puts_fd(1, "\n");
        }
    }
}
