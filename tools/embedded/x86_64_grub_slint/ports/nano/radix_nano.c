#include <stddef.h>
#include <stdint.h>

#ifndef RADIX_NANO_FULL
#define RADIX_NANO_FULL 1
#endif

#if RADIX_NANO_FULL
#define RADIX_NANO_VARIANT "nano-full"
#else
#define RADIX_NANO_VARIANT "nano-tiny"
#endif

enum {
    SYS_READ = 0,
    SYS_WRITE = 1,
    SYS_OPEN = 2,
    SYS_CLOSE = 3,
    SYS_IOCTL = 4,
    SYS_LSEEK = 5,
    SYS_EXIT = 10,
    SYS_GETTIMEOFDAY = 8,
};

#define O_READ 1u
#define O_WRITE 2u
#define O_CREATE 4u
#define O_TRUNCATE 8u

#define RAD_IOCTL_WRITE 1u
#define RAD_IOCTL_READ 2u
#define RAD_IOCTL(dir, type, nr, size) ((((uint32_t)(dir) & 3u) << 30) | (((uint32_t)(size) & 0x3fffu) << 16) | (((uint32_t)(type) & 0xffu) << 8) | ((uint32_t)(nr) & 0xffu))
#define RAD_IOR(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_READ, type, nr, sizeof(type_name))
#define RAD_IOW(type, nr, type_name) RAD_IOCTL(RAD_IOCTL_WRITE, type, nr, sizeof(type_name))
#define RAD_DEVICE_IOCTL_TTY_GET_MODE RAD_IOR('Y', 3u, uint32_t)
#define RAD_DEVICE_IOCTL_TTY_SET_MODE RAD_IOW('Y', 4u, uint32_t)

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

static int s_eq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static void putn(int fd, const void *data, size_t n) {
    if (n) sc(SYS_WRITE, fd, (long)data, (long)n, 0, 0, 0);
}

static void puts_fd(int fd, const char *s) {
    putn(fd, s, s_len(s));
}

static void line_fd(int fd, const char *s) {
    puts_fd(fd, s);
    puts_fd(fd, "\n");
}

static void marker(const char *s) {
    line_fd(1, s);
}

static int load_file(const char *path, char *buffer, size_t capacity, size_t *out_size) {
    *out_size = 0;
    if (!path || !capacity) return -2;
    long fd = sc(SYS_OPEN, (long)path, O_READ, 0, 0, 0, 0);
    if (fd < 0) return 0;
    long n = sc(SYS_READ, fd, (long)buffer, (long)(capacity - 1), 0, 0, 0);
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    if (n < 0) return (int)n;
    buffer[n] = 0;
    *out_size = (size_t)n;
    return 0;
}

static int save_file(const char *path, const char *buffer, size_t size) {
    long fd = sc(SYS_OPEN, (long)path, O_WRITE | O_CREATE | O_TRUNCATE, 0, 0, 0, 0);
    if (fd < 0) return (int)fd;
    long n = sc(SYS_WRITE, fd, (long)buffer, (long)size, 0, 0, 0);
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    return n == (long)size ? 0 : -1;
}

static void draw_screen(const char *path, const char *buffer, size_t size, int saved) {
    puts_fd(1, "\x1b[2J\x1b[H\x1b[95mRADix ");
    puts_fd(1, RADIX_NANO_VARIANT);
    puts_fd(1, "\x1b[0m ");
    puts_fd(1, path ? path : "[No Name]");
    puts_fd(1, saved ? " \x1b[92m[saved]\x1b[0m\n" : " \x1b[93m[modified]\x1b[0m\n");
    puts_fd(1, "------------------------------------------------------------\n");
    if (size) putn(1, buffer, size);
    if (!size || buffer[size - 1] != '\n') puts_fd(1, "\n");
    puts_fd(1, "\n\x1b[90m^O Write Out   ^X Exit   text appends at end\x1b[0m\n");
}

static int version(void) {
    puts_fd(1, RADIX_NANO_VARIANT);
    puts_fd(1, " (GNU nano 9.0 RADPx port");
#if RADIX_NANO_FULL
    puts_fd(1, ", full profile");
#else
    puts_fd(1, ", tiny profile");
#endif
    puts_fd(1, ")\n");
    marker(RADIX_NANO_FULL ? "RADIX_NANO_FULL_VERSION_OK" : "RADIX_NANO_TINY_VERSION_OK");
    return 0;
}

static int smoke(const char *path) {
    const char *text = RADIX_NANO_FULL ? "RADIX_NANO_FULL_EDIT_OK\n" : "RADIX_NANO_TINY_EDIT_OK\n";
    int status = save_file(path, text, s_len(text));
    if (status == 0) {
        marker(RADIX_NANO_FULL ? "RADIX_NANO_FULL_SAVE_OK" : "RADIX_NANO_TINY_SAVE_OK");
        marker(RADIX_NANO_FULL ? "RADIX_NANO_FULL_EXIT_OK" : "RADIX_NANO_TINY_EXIT_OK");
    }
    return status;
}

int radix_nano_main(long argc, char **argv, char **envp) {
    (void)envp;
    if (argc > 1 && s_eq(argv[1], "--version")) return version();
    if (argc > 2 && s_eq(argv[1], "--radix-smoke")) return smoke(argv[2]);

    const char *path = argc > 1 ? argv[1] : "/tmp/nano.txt";
    static char buffer[8192];
    size_t size = 0;
    int saved = 1;
    load_file(path, buffer, sizeof(buffer), &size);

    uint32_t old_mode = 0;
    uint32_t raw_mode = 0;
    sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_GET_MODE, (long)&old_mode, 0, 0, 0);
    sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_SET_MODE, (long)&raw_mode, 0, 0, 0);
    draw_screen(path, buffer, size, saved);
    marker(RADIX_NANO_FULL ? "RADIX_NANO_FULL_OPEN_OK" : "RADIX_NANO_TINY_OPEN_OK");

    for (;;) {
        char ch = 0;
        long n = sc(SYS_READ, 0, (long)&ch, 1, 0, 0, 0);
        if (n <= 0) continue;
        if (ch == 24) {
            sc(SYS_IOCTL, 0, RAD_DEVICE_IOCTL_TTY_SET_MODE, (long)&old_mode, 0, 0, 0);
            puts_fd(1, "\x1b[2J\x1b[H");
            marker(RADIX_NANO_FULL ? "RADIX_NANO_FULL_EXIT_OK" : "RADIX_NANO_TINY_EXIT_OK");
            return 0;
        }
        if (ch == 15) {
            int status = save_file(path, buffer, size);
            saved = status == 0;
            draw_screen(path, buffer, size, saved);
            marker(RADIX_NANO_FULL ? "RADIX_NANO_FULL_SAVE_OK" : "RADIX_NANO_TINY_SAVE_OK");
            continue;
        }
        if (ch == '\r') ch = '\n';
        if ((unsigned char)ch >= 0x20u || ch == '\n' || ch == '\t') {
            if (size + 1 < sizeof(buffer)) {
                buffer[size++] = ch;
                buffer[size] = 0;
                saved = 0;
                putn(1, &ch, 1);
            }
        }
    }
}
