#ifndef RADIX_SYSROOT_TIME_H
#define RADIX_SYSROOT_TIME_H

#include <stddef.h>
#include <sys/types.h>

struct timespec {
    long tv_sec;
    long tv_nsec;
};

typedef long clock_t;
#define CLOCKS_PER_SEC 1000000l
#define UTIME_NOW 1073741823l
#define UTIME_OMIT 1073741822l

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

int nanosleep(const struct timespec *req, struct timespec *rem);
time_t time(time_t *tloc);
clock_t clock(void);
struct tm *localtime(const time_t *timep);
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);

#endif
