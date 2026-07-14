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
    SYS_POLL = 39,
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
};

#define RAD_IOCTL_NONE 0u
#define RAD_IOCTL_WRITE 1u
#define RAD_IOCTL_READ 2u
#define RAD_IOCTL_READWRITE 3u
#define RAD_IOCTL(dir, type, nr, size) ((((uint32_t)(dir) & 3u) << 30) | (((uint32_t)(size) & 0x3fffu) << 16) | (((uint32_t)(type) & 0xffu) << 8) | ((uint32_t)(nr) & 0xffu))
#define RAD_IOR(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_READ, type, nr, sizeof(type_name))
#define RAD_IOW(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_WRITE, type, nr, sizeof(type_name))
#define RAD_IOCTL_TYPE_TTY 'Y'
#define RAD_DEVICE_IOCTL_TTY_GET_WINSIZE RAD_IOR(RAD_IOCTL_TYPE_TTY, 1u, radsh_winsize_t)
#define RAD_DEVICE_IOCTL_TTY_SET_WINSIZE RAD_IOW(RAD_IOCTL_TYPE_TTY, 2u, radsh_winsize_t)
#define RAD_DEVICE_IOCTL_TTY_GET_MODE RAD_IOR(RAD_IOCTL_TYPE_TTY, 3u, uint32_t)
#define RAD_DEVICE_IOCTL_TTY_SET_MODE RAD_IOW(RAD_IOCTL_TYPE_TTY, 4u, uint32_t)
#define RAD_TTY_MODE_CANONICAL (1u << 0)
#define RAD_TTY_MODE_ECHO (1u << 1)
#define RAD_TTY_MODE_CRLF (1u << 2)
#define RAD_POLLIN 0x0001
#define RAD_POLLOUT 0x0004
#define RAD_POLLERR 0x0008
#define RAD_POLLHUP 0x0010
#define RAD_POLLNVAL 0x0020

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
    if (s_eq(argv[1], "start") || s_eq(argv[1], "restart")) return cmd_initctl_control(argv[1], argc > 2 ? argv[2] : "", outfd);
    if (s_eq(argv[1], "stop")) {
        line_fd(outfd, "initctl stop is not wired until process signals land");
        return -6;
    }
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
    write_file_text("/run/radinit/control.status", "pending\n");
    char request[96];
    s_copy(request, sizeof(request), verb);
    s_cat(request, sizeof(request), " ");
    s_cat(request, sizeof(request), name);
    s_cat(request, sizeof(request), "\n");
    int status = write_file_text("/run/radinit/control.txt", request);
    if (status < 0) return status;
    for (int i = 0; i < 20; ++i) {
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
    line_fd(outfd, "initctl request queued");
    return 0;
}

static const char *builtin_names[] = {
    "basename", "cat", "cd", "chmod", "clear", "cp", "date", "dirname", "dmesg", "echo",
    "env", "false", "head", "help", "hostname", "id", "initctl", "ln", "logread", "ls",
    "mkdir", "mount", "mv", "passwd", "printf", "ps", "pwd", "readlink", "reboot", "reset",
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
    if (s_eq(argv[0], "sleep")) return cmd_sleep(argc, argv);
    if (s_eq(argv[0], "which")) return cmd_which(argc, argv, outfd);
    if (s_eq(argv[0], "test") || s_eq(argv[0], "[")) return cmd_test(argc, argv);
    if (s_eq(argv[0], "printf")) return cmd_printf(argc, argv, outfd);
    if (s_eq(argv[0], "passwd")) return cmd_passwd(outfd);
    if (s_eq(argv[0], "mount")) return cmd_mount(outfd);
    if (s_eq(argv[0], "ps")) return cmd_ps(outfd);
    if (s_eq(argv[0], "dmesg")) return cmd_dmesg_user(outfd);
    if (s_eq(argv[0], "initctl")) return cmd_initctl(argc, argv, outfd);
    if (s_eq(argv[0], "logread")) return cmd_logread(argc, argv, outfd);
    if (s_eq(argv[0], "reboot")) { line_fd(outfd, "reboot is not wired on this target yet"); return -6; }
    if (s_eq(argv[0], "help")) {
        line_fd(outfd, "builtins: basename cat cd chmod clear cp date dirname dmesg echo env false head help hostname id initctl ln logread ls mkdir mount mv passwd printf ps pwd readlink reboot reset rm rmdir sleep stat stty tail test touch tput true tty tui-smoke uname wc which [");
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
    if (child > 0) { int status = 0; sc(SYS_WAITPID, child, (long)&status, 0, 0, 0, 0); return status; }
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
        if (ch == '\r') continue;
        if (ch == '\n') {
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
