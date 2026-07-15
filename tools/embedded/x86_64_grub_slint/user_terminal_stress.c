#include <stddef.h>
#include <stdint.h>

enum {
    SYS_WRITE = 1,
    SYS_EXIT = 10,
    SYS_NANOSLEEP = 9,
    SYS_FORK = 11,
    SYS_EXECVE = 12,
    SYS_WAITPID = 13,
    WAIT_NOHANG = 1,
};

#define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#define WTERMSIG(status) ((status) & 0x7f)
#define WIFEXITED(status) (WTERMSIG(status) == 0)

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

static size_t s_len(const char *text) {
    size_t n = 0;
    if (text) while (text[n]) ++n;
    return n;
}

static void line(const char *text) {
    sc(SYS_WRITE, 1, (long)text, (long)s_len(text), 0, 0, 0);
    sc(SYS_WRITE, 1, (long)"\n", 1, 0, 0, 0);
}

static void line_num(const char *prefix, long value) {
    char buf[96];
    size_t pos = 0;
    for (size_t i = 0; prefix && prefix[i] && pos + 1 < sizeof(buf); ++i) buf[pos++] = prefix[i];
    if (value < 0 && pos + 1 < sizeof(buf)) {
        buf[pos++] = '-';
        value = -value;
    }
    char digits[24];
    size_t d = 0;
    do {
        digits[d++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value && d < sizeof(digits));
    while (d && pos + 1 < sizeof(buf)) buf[pos++] = digits[--d];
    buf[pos] = '\0';
    line(buf);
}

static int run_one(const char *path, const char *ok, const char *fail) {
    long child = sc(SYS_FORK, 0, 0, 0, 0, 0, 0);
    if (child == 0) {
        line("RADIX_TERMINAL_STRESS_CHILD_EXEC");
        char *argv[2];
        argv[0] = (char*)path;
        argv[1] = 0;
        long exec_status = sc(SYS_EXECVE, (long)path, (long)argv, 0, 0, 0, 0);
        line_num("RADIX_TERMINAL_STRESS_CHILD_EXEC_FAIL status=", exec_status);
        sc(SYS_EXIT, 127, 0, 0, 0, 0, 0);
    }
    if (child < 0) {
        line_num("RADIX_TERMINAL_STRESS_FORK_FAIL status=", child);
        line(fail);
        return 1;
    }
    int status = 0;
    long waited = 0;
    for (int tries = 0; tries < 200000; ++tries) {
        waited = sc(SYS_WAITPID, child, (long)&status, WAIT_NOHANG, 0, 0, 0);
        if (waited == child || waited < 0) break;
        sc(SYS_NANOSLEEP, 0, 0, 0, 0, 0, 0);
    }
    if (waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        line(ok);
        return 0;
    }
    line_num("RADIX_TERMINAL_STRESS_WAIT_RESULT waited=", waited);
    line_num("RADIX_TERMINAL_STRESS_WAIT_STATUS status=", status);
    line(fail);
    return 1;
}

int main(void) {
    int failed = 0;
    failed |= run_one("/usr/bin/libc-rso-stress", "RADIX_LIBRADIXC_RSO_BOOT_OK", "RADIX_LIBRADIXC_RSO_BOOT_FAIL");
    failed |= run_one("/usr/bin/tinfo-rso-stress", "RADIX_LIBTINFO_RSO_BOOT_OK", "RADIX_LIBTINFO_RSO_BOOT_FAIL");
    failed |= run_one("/usr/bin/tui-stress-dyn", "RADIX_TUI_STRESS_DYN_BOOT_OK", "RADIX_TUI_STRESS_DYN_BOOT_FAIL");
    failed |= run_one("/usr/bin/sleep-stress", "RADIX_SLEEP_STRESS_BOOT_OK", "RADIX_SLEEP_STRESS_BOOT_FAIL");
    failed |= run_one("/usr/bin/signal-stress", "RADIX_SIGNAL_STRESS_BOOT_OK", "RADIX_SIGNAL_STRESS_BOOT_FAIL");
    failed |= run_one("/usr/bin/posix-stress", "RADIX_POSIX_STRESS_BOOT_OK", "RADIX_POSIX_STRESS_BOOT_FAIL");
    failed |= run_one("/usr/bin/tty-stress", "RADIX_TTY_STRESS_BOOT_OK", "RADIX_TTY_STRESS_BOOT_FAIL");
    failed |= run_one("/usr/bin/tui-stress", "RADIX_TUI_STRESS_BOOT_OK", "RADIX_TUI_STRESS_BOOT_FAIL");
    line(failed ? "RADIX_STRESS_AUTOTEST_FAIL" : "RADIX_STRESS_AUTOTEST_OK");
    return failed ? 1 : 0;
}
