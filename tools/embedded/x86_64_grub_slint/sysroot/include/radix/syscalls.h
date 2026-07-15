#ifndef RADIX_SYSROOT_SYSCALLS_H
#define RADIX_SYSROOT_SYSCALLS_H

#include <stddef.h>
#include <stdint.h>

#define RADIX_SYS_READ 0
#define RADIX_SYS_WRITE 1
#define RADIX_SYS_OPEN 2
#define RADIX_SYS_CLOSE 3
#define RADIX_SYS_IOCTL 4
#define RADIX_SYS_LSEEK 5
#define RADIX_SYS_STAT 6
#define RADIX_SYS_FSTAT 7
#define RADIX_SYS_GETTIMEOFDAY 8
#define RADIX_SYS_NANOSLEEP 9
#define RADIX_SYS_EXIT 10
#define RADIX_SYS_FORK 11
#define RADIX_SYS_EXECVE 12
#define RADIX_SYS_WAITPID 13
#define RADIX_SYS_GETPID 14
#define RADIX_SYS_GETPPID 15
#define RADIX_SYS_DUP 16
#define RADIX_SYS_DUP2 17
#define RADIX_SYS_CHDIR 18
#define RADIX_SYS_GETCWD 19
#define RADIX_SYS_BRK 20
#define RADIX_SYS_PIPE 21
#define RADIX_SYS_FCNTL 22
#define RADIX_SYS_ACCESS 23
#define RADIX_SYS_ISATTY 24
#define RADIX_SYS_SOCKET 25
#define RADIX_SYS_BIND 26
#define RADIX_SYS_CONNECT 27
#define RADIX_SYS_LISTEN 28
#define RADIX_SYS_ACCEPT 29
#define RADIX_SYS_SENDTO 30
#define RADIX_SYS_RECVFROM 31
#define RADIX_SYS_SHUTDOWN 32
#define RADIX_SYS_SETSOCKOPT 33
#define RADIX_SYS_GETSOCKOPT 34
#define RADIX_SYS_MMAP 35
#define RADIX_SYS_MUNMAP 36
#define RADIX_SYS_POLL 39
#define RADIX_SYS_SETTIMEOFDAY 40
#define RADIX_SYS_GETDENTS 1000
#define RADIX_SYS_REMOVE 1001
#define RADIX_SYS_MKDIR 1002
#define RADIX_SYS_RMDIR 1003
#define RADIX_SYS_RENAME 1004
#define RADIX_SYS_TRUNCATE 1005
#define RADIX_SYS_GETUID 1008
#define RADIX_SYS_GETEUID 1009
#define RADIX_SYS_GETGID 1010
#define RADIX_SYS_GETEGID 1011
#define RADIX_SYS_CHMOD 1014
#define RADIX_SYS_LINK 1015
#define RADIX_SYS_SYMLINK 1016
#define RADIX_SYS_READLINK 1017
#define RADIX_SYS_FSYNC 1018
#define RADIX_SYS_KILL 1019
#define RADIX_SYS_FCHDIR 1020
#define RADIX_SYS_FTRUNCATE 1021
#define RADIX_SYS_UTIME 1022
#define RADIX_SYS_GETPGID 1023
#define RADIX_SYS_SETPGID 1024
#define RADIX_SYS_GETSID 1025
#define RADIX_SYS_SETSID 1026
#define RADIX_SYS_TCGETPGRP 1027
#define RADIX_SYS_TCSETPGRP 1028
#define RADIX_SYS_PIPE2 1029

static inline long radix_syscall6(long n, long a, long b, long c, long d, long e, long f) {
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

#endif
