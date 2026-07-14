#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#if defined(RADIX_GUEST_STRESS_DIRECT)
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

static size_t cstrlen(const char *text) {
    size_t n = 0;
    while (text && text[n]) ++n;
    return n;
}

static int stress_isatty(int fd) { return (int)sc(24, fd, 0, 0, 0, 0, 0); }
static int stress_ioctl(int fd, unsigned long request, void *arg) { return sc(4, fd, (long)request, (long)arg, 0, 0, 0) < 0 ? -1 : 0; }
static int stress_poll(struct pollfd *fds, nfds_t count, int timeout_ms) { return (int)sc(39, (long)fds, (long)count, timeout_ms, 0, 0, 0); }
static int stress_tcgetattr(int fd, struct termios *termios_p) { return stress_ioctl(fd, RADIX_TTY_GET_TERMIOS, termios_p); }
static int stress_tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    (void)optional_actions;
    return stress_ioctl(fd, RADIX_TTY_SET_TERMIOS, (void*)termios_p);
}
static int stress_tcflush(int fd, int queue_selector) {
    uint32_t queues = queue_selector == TCIFLUSH ? 1u : (queue_selector == TCOFLUSH ? 2u : 3u);
    return stress_ioctl(fd, RADIX_TTY_FLUSH, &queues);
}
static void stress_cfmakeraw(struct termios *termios_p) {
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
#else
static size_t cstrlen(const char *text) { return strlen(text); }
static int stress_isatty(int fd) { return isatty(fd); }
static int stress_ioctl(int fd, unsigned long request, void *arg) { return ioctl(fd, request, arg); }
static int stress_poll(struct pollfd *fds, nfds_t count, int timeout_ms) { return poll(fds, count, timeout_ms); }
static int stress_tcgetattr(int fd, struct termios *termios_p) { return tcgetattr(fd, termios_p); }
static int stress_tcsetattr(int fd, int optional_actions, const struct termios *termios_p) { return tcsetattr(fd, optional_actions, termios_p); }
static int stress_tcflush(int fd, int queue_selector) { return tcflush(fd, queue_selector); }
static void stress_cfmakeraw(struct termios *termios_p) { cfmakeraw(termios_p); }
#endif

static void event(const char *name) {
#if defined(RADIX_GUEST_STRESS_DIRECT)
    sc(1, 1, (long)name, (long)cstrlen(name), 0, 0, 0);
    sc(1, 1, (long)"\n", 1, 0, 0, 0);
#else
    write(1, name, cstrlen(name));
    write(1, "\n", 1);
#endif
}

static int fail(const char *name) {
    event("RADIX_TTY_STRESS_FAIL");
#if defined(RADIX_GUEST_STRESS_DIRECT)
    sc(1, 1, (long)"tty-stress-fail:", 16, 0, 0, 0);
    sc(1, 1, (long)name, (long)cstrlen(name), 0, 0, 0);
    sc(1, 1, (long)"\n", 1, 0, 0, 0);
#else
    write(1, "tty-stress-fail:", 16);
    write(1, name, cstrlen(name));
    write(1, "\n", 1);
#endif
    return 1;
}

int main(void) {
    event("RADIX_TTY_STRESS_START");
    if (!stress_isatty(0) || !stress_isatty(1)) return fail("isatty");
    event("RADIX_TTY_STRESS_ISATTY_OK");

    struct termios old_term;
    if (stress_tcgetattr(0, &old_term) != 0) return fail("tcgetattr");
    event("RADIX_TTY_STRESS_TCGETATTR_OK");

    struct winsize ws;
    if (stress_ioctl(0, TIOCGWINSZ, &ws) != 0 || ws.ws_row == 0 || ws.ws_col == 0) return fail("winsize");
    event("RADIX_TTY_STRESS_WINSIZE_OK");

    struct termios raw = old_term;
    stress_cfmakeraw(&raw);
    if (stress_tcsetattr(0, TCSANOW, &raw) != 0) return fail("raw-set");
    event("RADIX_TTY_STRESS_RAW_SET_OK");

    struct termios roundtrip;
    if (stress_tcgetattr(0, &roundtrip) != 0) return fail("raw-get");
    if ((roundtrip.c_lflag & (ICANON | ECHO)) != 0) return fail("raw-flags");
    event("RADIX_TTY_STRESS_RAW_GET_OK");

    struct pollfd pfd;
    pfd.fd = 0;
    pfd.events = POLLIN;
    pfd.revents = 0;
    if (stress_poll(&pfd, 1, 1) < 0) return fail("poll-empty");
    event("RADIX_TTY_STRESS_POLL_OK");

    if (stress_tcflush(0, TCIFLUSH) != 0) return fail("flush");
    event("RADIX_TTY_STRESS_FLUSH_OK");
    if (stress_tcsetattr(0, TCSAFLUSH, &old_term) != 0) return fail("restore");
    event("RADIX_TTY_STRESS_RESTORE_OK");

    event("RADIX_TTY_RAW_STRESS_OK");
    event("RADIX_TTY_CBREAK_STRESS_OK");
    event("RADIX_PTY_POLL_STRESS_OK");
    event("tty-stress-ok");
    return 0;
}
