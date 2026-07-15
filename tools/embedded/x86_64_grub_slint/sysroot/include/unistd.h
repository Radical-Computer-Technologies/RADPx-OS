#ifndef RADIX_SYSROOT_UNISTD_H
#define RADIX_SYSROOT_UNISTD_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int chdir(const char *path);
int fchdir(int fd);
char *getcwd(char *buf, size_t size);
char *getlogin(void);
int getlogin_r(char *buf, size_t buflen);
int isatty(int fd);
int pipe(int pipefd[2]);
int pipe2(int pipefd[2], int flags);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int getdtablesize(void);
pid_t fork(void);
int execve(const char *pathname, char *const argv[], char *const envp[]);
int execl(const char *pathname, const char *arg, ...);
int execvp(const char *file, char *const argv[]);
int execlp(const char *file, const char *arg, ...);
long fpathconf(int fd, int name);
pid_t getpid(void);
pid_t getppid(void);
pid_t getpgrp(void);
pid_t getpgid(pid_t pid);
int setpgid(pid_t pid, pid_t pgid);
pid_t getsid(pid_t pid);
pid_t setsid(void);
pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
int gethostname(char *name, size_t len);
int access(const char *pathname, int mode);
int unlink(const char *pathname);
int rmdir(const char *pathname);
int symlink(const char *target, const char *linkpath);
int link(const char *oldpath, const char *newpath);
ssize_t readlink(const char *pathname, char *buf, size_t bufsiz);
int fsync(int fd);
int truncate(const char *path, off_t length);
int ftruncate(int fd, off_t length);
unsigned int sleep(unsigned int seconds);
void _exit(int status) __attribute__((noreturn));

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#define _PC_PIPE_BUF 1

#endif
