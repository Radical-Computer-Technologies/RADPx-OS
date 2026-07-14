#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
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

static int stress_gettimeofday(struct timeval *tv) {
    return sc(8, (long)tv, 0, 0, 0, 0, 0) < 0 ? -1 : 0;
}

static int stress_nanosleep(const struct timespec *ts) {
    return sc(9, ts->tv_sec * 1000000000L + ts->tv_nsec, 0, 0, 0, 0, 0) < 0 ? -1 : 0;
}

static int stress_poll_empty(int timeout_ms) {
    return (int)sc(39, 0, 0, timeout_ms, 0, 0, 0);
}
#else
static size_t cstrlen(const char *text) {
    return strlen(text);
}

static int stress_gettimeofday(struct timeval *tv) {
    return gettimeofday(tv, 0);
}

static int stress_nanosleep(const struct timespec *ts) {
    return nanosleep(ts, 0);
}

static int stress_poll_empty(int timeout_ms) {
    return poll(0, 0, timeout_ms);
}
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

static long millis(void) {
    struct timeval tv;
    if (stress_gettimeofday(&tv) != 0) return 0;
    return tv.tv_sec * 1000L + tv.tv_usec / 1000L;
}

static int fail(const char *name) {
    event("RADIX_SCHED_WAKE_STRESS_FAIL");
#if defined(RADIX_GUEST_STRESS_DIRECT)
    sc(1, 1, (long)"sleep-stress-fail:", 18, 0, 0, 0);
    sc(1, 1, (long)name, (long)cstrlen(name), 0, 0, 0);
    sc(1, 1, (long)"\n", 1, 0, 0, 0);
#else
    write(1, "sleep-stress-fail:", 18);
    write(1, name, cstrlen(name));
    write(1, "\n", 1);
#endif
    return 1;
}

int main(void) {
    event("RADIX_SLEEP_STRESS_START");
    const long start = millis();
    event("RADIX_SLEEP_STRESS_TIME_START_OK");
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 5000000L;
    if (stress_nanosleep(&ts) != 0) return fail("nanosleep");
    event("RADIX_SLEEP_STRESS_NANOSLEEP_RETURN_OK");
    const long elapsed = millis() - start;
    if (elapsed < 0 || elapsed > 5000) return fail("elapsed");
    event("RADIX_SLEEP_STRESS_ELAPSED_OK");

    const long poll_start = millis();
    const int ready = stress_poll_empty(5);
    const long poll_elapsed = millis() - poll_start;
    if (ready != 0 || poll_elapsed < 0 || poll_elapsed > 5000) return fail("poll-timeout");
    event("RADIX_SLEEP_STRESS_POLL_RETURN_OK");

    event("RADIX_USER_NANOSLEEP_OK");
    event("RADIX_USER_POLL_TIMEOUT_OK");
    event("RADIX_SCHED_WAKE_STRESS_OK");
    event("sleep-stress-ok");
    return 0;
}
