#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void event(const char *name) {
    write(1, name, strlen(name));
    write(1, "\n", 1);
}

static int fail(const char *name) {
    event("RADIX_SIGNAL_STRESS_FAIL");
    write(1, "signal-stress-fail:", 19);
    write(1, name, strlen(name));
    write(1, "\n", 1);
    return 1;
}

static void handler(int signum) {
    (void)signum;
}

int main(void) {
    event("RADIX_SIGNAL_STRESS_START");
    if (signal(SIGWINCH, handler) == SIG_ERR) return fail("signal-install");
    if (signal(SIGWINCH, SIG_DFL) != handler) return fail("signal-old");
    event("RADIX_SIGNAL_STRESS_SIGNAL_API_OK");

    struct sigaction act;
    struct sigaction oldact;
    if (sigemptyset(&act.sa_mask) != 0) return fail("emptyset");
    act.sa_handler = SIG_IGN;
    act.sa_flags = 0;
    if (sigaction(SIGINT, &act, &oldact) != 0) return fail("sigaction-install");
    if (sigaction(SIGINT, 0, &oldact) != 0 || oldact.sa_handler != SIG_IGN) return fail("sigaction-query");
    if (sigaction(-1, 0, 0) == 0) return fail("invalid-signal");
    event("RADIX_SIGNAL_STRESS_SIGACTION_OK");

    event("RADIX_SIGNAL_TABLE_OK");
    event("RADIX_SIGNAL_IGNORE_OK");
    event("RADIX_SIGNAL_STRESS_OK");
    event("signal-stress-ok");
    return 0;
}
