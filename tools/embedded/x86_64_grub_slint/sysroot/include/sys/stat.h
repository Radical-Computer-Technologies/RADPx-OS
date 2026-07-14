#ifndef RADIX_SYSROOT_SYS_STAT_H
#define RADIX_SYSROOT_SYS_STAT_H

#include <stdint.h>
#include <sys/types.h>
#include <time.h>

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    uint64_t st_size;
    int st_is_directory;
    uint32_t st_mode;
    nlink_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    uint64_t st_mtime_ms;
};

#define S_IFMT  0170000
#define S_IFDIR 0040000
#define S_IFREG 0100000
#define S_IFCHR 0020000
#define S_IFBLK 0060000
#define S_IFIFO 0010000
#define S_IFLNK 0120000
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IRWXG (S_IRGRP | S_IWGRP | S_IXGRP)
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
#define S_IRWXO (S_IROTH | S_IWOTH | S_IXOTH)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)

int stat(const char *pathname, struct stat *statbuf);
int fstat(int fd, struct stat *statbuf);
int fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags);
int lstat(const char *pathname, struct stat *statbuf);
int mkdir(const char *pathname, mode_t mode);
int chmod(const char *pathname, mode_t mode);
int futimens(int fd, const struct timespec times[2]);
mode_t umask(mode_t mask);

#endif
