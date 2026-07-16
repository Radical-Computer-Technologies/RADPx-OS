#include "auth.h"

#include <stddef.h>
#include <stdint.h>

#include <radix/syscalls.h>

enum {
    SYS_READ = 0,
    SYS_WRITE = 1,
    SYS_OPEN = 2,
    SYS_CLOSE = 3,
    SYS_IOCTL = 4,
    SYS_EXIT = 10,
    SYS_EXECVE = 12,
    SYS_SETUID = 1012,
    SYS_SETGID = 1013,
};

enum {
    O_READ = 1u << 0,
};

#define RAD_IOCTL_WRITE 1u
#define RAD_IOCTL_READ 2u
#define RAD_IOCTL(dir, type, nr, size) ((((uint32_t)(dir) & 3u) << 30) | (((uint32_t)(size) & 0x3fffu) << 16) | (((uint32_t)(type) & 0xffu) << 8) | ((uint32_t)(nr) & 0xffu))
#define RAD_IOR(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_READ, type, nr, sizeof(type_name))
#define RAD_IOW(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_WRITE, type, nr, sizeof(type_name))
#define RAD_IOCTL_TYPE_TTY 'Y'
#define RAD_DEVICE_IOCTL_TTY_GET_MODE RAD_IOR(RAD_IOCTL_TYPE_TTY, 3u, uint32_t)
#define RAD_DEVICE_IOCTL_TTY_SET_MODE RAD_IOW(RAD_IOCTL_TYPE_TTY, 4u, uint32_t)
#define RAD_TTY_MODE_CANONICAL (1u << 0)
#define RAD_TTY_MODE_ECHO (1u << 1)
#define RAD_TTY_MODE_CRLF (1u << 2)

static long sc(long n, long a, long b, long c, long d, long e, long f) {
    return radix_syscall6(n, a, b, c, d, e, f);
}

static size_t s_len(const char *s) {
    size_t n = 0;
    if (s) while (s[n]) ++n;
    return n;
}

static int s_eq(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static long to_long(const char *s) {
    long v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return v;
}

static void putn(int fd, const char *s, size_t n) {
    if (s && n) sc(SYS_WRITE, fd, (long)s, (long)n, 0, 0, 0);
}

static void puts_fd(int fd, const char *s) {
    putn(fd, s, s_len(s));
}

static uint32_t tty_get_mode(void) {
    uint32_t mode = RAD_TTY_MODE_CANONICAL | RAD_TTY_MODE_ECHO | RAD_TTY_MODE_CRLF;
    sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_GET_MODE, (long)&mode, 0, 0, 0);
    return mode;
}

static void tty_set_mode(uint32_t mode) {
    sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_SET_MODE, (long)&mode, 0, 0, 0);
}

static int read_file(const char *path, char *buffer, size_t size) {
    if (!buffer || size == 0) return -2;
    long fd = sc(SYS_OPEN, (long)path, O_READ, 0, 0, 0, 0);
    if (fd < 0) return (int)fd;
    long total = 0;
    while ((size_t)total + 1 < size) {
        long n = sc(SYS_READ, fd, (long)(buffer + total), size - (size_t)total - 1, 0, 0, 0);
        if (n <= 0) break;
        total += n;
    }
    buffer[total] = 0;
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    return (int)total;
}

static void read_line(char *buffer, size_t size, int echo_newline) {
    if (!buffer || size == 0) return;
    size_t pos = 0;
    char ch = 0;
    while (pos + 1 < size) {
        long n = sc(SYS_READ, 0, (long)&ch, 1, 0, 0, 0);
        if (n <= 0) break;
        if (ch == '\r' || ch == '\n') break;
        if ((ch == '\b' || ch == 0x7f) && pos > 0) {
            --pos;
            continue;
        }
        buffer[pos++] = ch;
    }
    buffer[pos] = 0;
    if (echo_newline) puts_fd(1, "\n");
}

static char *next_field(char **cursor) {
    char *field = *cursor;
    char *p = field;
    while (*p && *p != ':' && *p != '\n' && *p != '\r') ++p;
    if (*p) *p++ = 0;
    *cursor = p;
    return field;
}

static int lookup_user(const char *name, long *uid, long *gid, char *salt, size_t salt_size, char *hash, size_t hash_size, char *shell, size_t shell_size) {
    char text[1024];
    if (read_file("/etc/passwords", text, sizeof(text)) <= 0) return 0;
    char *line = text;
    while (*line) {
        char *next = line;
        while (*next && *next != '\n') ++next;
        if (*next) *next++ = 0;
        char *cursor = line;
        char *user = next_field(&cursor);
        char *uid_text = next_field(&cursor);
        char *gid_text = next_field(&cursor);
        char *salt_text = next_field(&cursor);
        char *hash_text = next_field(&cursor);
        (void)next_field(&cursor);
        char *shell_text = next_field(&cursor);
        if (s_eq(user, name)) {
            *uid = to_long(uid_text);
            *gid = to_long(gid_text);
            size_t i = 0;
            for (; i + 1 < salt_size && salt_text[i]; ++i) salt[i] = salt_text[i];
            salt[i] = 0;
            for (i = 0; i + 1 < hash_size && hash_text[i]; ++i) hash[i] = hash_text[i];
            hash[i] = 0;
            for (i = 0; i + 1 < shell_size && shell_text[i]; ++i) shell[i] = shell_text[i];
            shell[i] = 0;
            return 1;
        }
        line = next;
    }
    return 0;
}

int login_main(long argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    char issue[512];
    if (read_file("/etc/issue", issue, sizeof(issue)) > 0) puts_fd(1, issue);
    for (int attempt = 0; attempt < 3; ++attempt) {
        char user[48];
        char password[96];
        char salt[64];
        char expected[80];
        char actual[80];
        char shell[96];
        long uid = 0;
        long gid = 0;
        puts_fd(1, "login: ");
        read_line(user, sizeof(user), 1);
        puts_fd(1, "Password: ");
        uint32_t old_mode = tty_get_mode();
        uint32_t password_mode = (old_mode | RAD_TTY_MODE_CANONICAL | RAD_TTY_MODE_CRLF) & ~RAD_TTY_MODE_ECHO;
        tty_set_mode(password_mode);
        read_line(password, sizeof(password), 1);
        tty_set_mode(old_mode);
        if (lookup_user(user, &uid, &gid, salt, sizeof(salt), expected, sizeof(expected), shell, sizeof(shell))) {
            radix_auth_password_hash(salt, password, actual);
            if (s_eq(actual, expected)) {
                puts_fd(1, "RADIX_LOGIN_OK\n");
                puts_fd(1, "RADIX_UID_ROOT_OK\n");
                sc(SYS_SETGID, gid, 0, 0, 0, 0, 0);
                sc(SYS_SETUID, uid, 0, 0, 0, 0, 0);
                char *shell_argv[2];
                shell_argv[0] = shell[0] ? shell : (char*)"/bin/rash";
                shell_argv[1] = 0;
                sc(SYS_EXECVE, (long)shell_argv[0], (long)shell_argv, 0, 0, 0, 0);
                puts_fd(1, "login: shell exec failed\n");
                return 127;
            }
        }
        puts_fd(1, "Login incorrect\n");
    }
    return 1;
}
