#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utime.h>

static void event(const char *name) {
    write(1, name, strlen(name));
    write(1, "\n", 1);
}

static int fail(const char *name) {
    event("RADIX_POSIX_STRESS_FAIL");
    write(1, "posix-stress-fail:", 18);
    write(1, name, strlen(name));
    write(1, "\n", 1);
    return 1;
}

int main(void) {
    event("RADIX_POSIX_STRESS_START");

    int pipefd[2];
    if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) != 0) return fail("pipe2");
    char ch = 0;
    if (write(pipefd[1], "x", 1) != 1 || read(pipefd[0], &ch, 1) != 1 || ch != 'x') return fail("pipe2-rw");
    close(pipefd[0]);
    close(pipefd[1]);
    event("RADIX_POSIX_PIPE2_OK");

    if (mkdir("/tmp/posixstress", 0755) != 0) {
        struct stat existing;
        if (stat("/tmp/posixstress", &existing) != 0 || !S_ISDIR(existing.st_mode)) return fail("mkdir");
    }
    int dir = open("/tmp/posixstress", O_RDONLY | O_DIRECTORY);
    if (dir < 0) return fail("opendir");
    int fd = openat(dir, "file.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return fail("openat");
    if (write(fd, "abcdef", 6) != 6) return fail("write");
    if (ftruncate(fd, 3) != 0) return fail("ftruncate");
    close(fd);
    struct stat st;
    if (fstatat(dir, "file.txt", &st, 0) != 0 || st.st_size != 3) return fail("fstatat");
    if (utime("/tmp/posixstress/file.txt", 0) != 0) return fail("utime");
    if (fchdir(dir) != 0) return fail("fchdir");
    if (stat("file.txt", &st) != 0 || st.st_size != 3) return fail("relative-stat");
    close(dir);
    event("RADIX_POSIX_AT_FD_OK");
    event("RADIX_POSIX_FTRUNCATE_OK");
    event("RADIX_POSIX_UTIME_OK");

    pid_t sid_before = getsid(0);
    pid_t pgid_before = getpgrp();
    if (sid_before <= 0 || pgid_before <= 0 || getpgid(0) <= 0) return fail("ids");
    pid_t tty_pgrp = tcgetpgrp(0);
    if (tty_pgrp <= 0) return fail("tcgetpgrp");
    if (tcsetpgrp(0, tty_pgrp) != 0) return fail("tcsetpgrp");
    event("RADIX_POSIX_PROCESS_GROUP_OK");
    event("RADIX_POSIX_CONTROLLING_TTY_OK");

    pid_t session_child = fork();
    if (session_child < 0) return fail("fork-session");
    if (session_child == 0) {
        pid_t sid = setsid();
        _exit(sid > 0 ? 0 : 11);
    }
    int status = 0;
    pid_t waited = waitpid(session_child, &status, 0);
    if (waited != session_child || !WIFEXITED(status) || WEXITSTATUS(status) != 0) return fail("setsid-child");
    event("RADIX_POSIX_SESSION_OK");

    pid_t child = fork();
    if (child < 0) return fail("fork");
    if (child == 0) _exit(7);
    status = 0;
    waited = waitpid(child, &status, 0);
    if (waited != child || !WIFEXITED(status) || WEXITSTATUS(status) != 7) return fail("wait-exit");
    event("RADIX_POSIX_WAIT_STATUS_OK");

    pid_t grouped = fork();
    if (grouped < 0) return fail("fork-group");
    if (grouped == 0) {
        for (;;) sleep(1);
    }
    if (setpgid(grouped, grouped) != 0) return fail("setpgid-child");
    if (getpgid(grouped) != grouped) return fail("getpgid-child");
    if (kill(-grouped, SIGTERM) != 0) return fail("kill-group");
    status = 0;
    waited = waitpid(grouped, &status, 0);
    if (waited != grouped || !WIFSIGNALED(status) || WTERMSIG(status) != SIGTERM) return fail("wait-group-signal");
    event("RADIX_POSIX_PROCESS_GROUP_KILL_OK");

    pid_t killed = fork();
    if (killed < 0) return fail("fork-kill");
    if (killed == 0) {
        for (;;) sleep(1);
    }
    if (kill(killed, SIGTERM) != 0) return fail("kill");
    status = 0;
    waited = waitpid(killed, &status, 0);
    if (waited != killed || !WIFSIGNALED(status) || WTERMSIG(status) != SIGTERM) return fail("wait-signal");
    event("RADIX_POSIX_SIGNAL_JOBCTRL_OK");

    event("RADIX_POSIX_STRESS_OK");
    event("posix-stress-ok");
    return 0;
}
