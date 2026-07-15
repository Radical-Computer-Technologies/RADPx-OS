#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static void event(const char *text) {
    write(1, text, strlen(text));
    write(1, "\n", 1);
}

int main(void) {
    event("RADIX_LIBRADIXC_RSO_START");

    struct timeval tv;
    if (gettimeofday(&tv, 0) != 0 || tv.tv_sec <= 0) {
        event("RADIX_LIBRADIXC_RSO_TIME_FAIL");
        return 1;
    }

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000L;
    if (nanosleep(&ts, 0) != 0) {
        event("RADIX_LIBRADIXC_RSO_SLEEP_FAIL");
        return 1;
    }

    struct stat st;
    if (stat("/bin/rash", &st) != 0 || st.st_size == 0) {
        event("RADIX_LIBRADIXC_RSO_STAT_FAIL");
        return 1;
    }

    int fd = open("/etc/hostname", O_RDONLY);
    if (fd < 0) {
        event("RADIX_LIBRADIXC_RSO_OPEN_FAIL");
        return 1;
    }
    char buffer[32];
    const ssize_t n = read(fd, buffer, sizeof(buffer));
    close(fd);
    if (n <= 0) {
        event("RADIX_LIBRADIXC_RSO_READ_FAIL");
        return 1;
    }

    void *mem = malloc(64);
    if (!mem) {
        event("RADIX_LIBRADIXC_RSO_MALLOC_FAIL");
        return 1;
    }
    memset(mem, 0x5a, 64);
    free(mem);

    event("RADIX_LIBRADIXC_RSO_OK");
    return 0;
}
