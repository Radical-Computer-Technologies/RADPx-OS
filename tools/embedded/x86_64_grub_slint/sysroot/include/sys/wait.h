#ifndef RADIX_SYSROOT_SYS_WAIT_H
#define RADIX_SYSROOT_SYS_WAIT_H

typedef int pid_t;

#define WNOHANG 1
#define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#define WTERMSIG(status) ((status) & 0x7f)
#define WIFEXITED(status) (WTERMSIG(status) == 0)
#define WIFSIGNALED(status) (WTERMSIG(status) != 0)

pid_t waitpid(pid_t pid, int *wstatus, int options);
pid_t wait(int *wstatus);

#endif
