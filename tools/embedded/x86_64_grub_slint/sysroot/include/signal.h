#ifndef RADIX_SYSROOT_SIGNAL_H
#define RADIX_SYSROOT_SIGNAL_H

#include <stddef.h>

#define NSIG 32

#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGABRT 6
#define SIGKILL 9
#define SIGSEGV 11
#define SIGTERM 15
#define SIGCONT 18
#define SIGTSTP 20
#define SIGWINCH 28

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SA_RESETHAND 0x1
#define SA_NODEFER 0x2

typedef void (*sighandler_t)(int);
typedef int sig_atomic_t;

#define SIG_ERR ((sighandler_t)-1)
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)

typedef unsigned long sigset_t;

struct sigaction {
    union {
        sighandler_t sa_handler;
        void (*sa_sigaction)(int, void *, void *);
    };
    sigset_t sa_mask;
    int sa_flags;
};

sighandler_t signal(int signum, sighandler_t handler);
int kill(int pid, int sig);
int raise(int sig);
int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int signum);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

#endif
