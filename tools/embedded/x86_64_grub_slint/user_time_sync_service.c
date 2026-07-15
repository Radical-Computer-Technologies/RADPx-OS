#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static size_t s_len(const char *s) {
    size_t n = 0;
    if (s) while (s[n]) ++n;
    return n;
}

static void put_fd(int fd, const char *s) {
    if (s) (void)write(fd, s, s_len(s));
}

static uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24u) | ((uint32_t)p[1] << 16u) | ((uint32_t)p[2] << 8u) | (uint32_t)p[3];
}

static int read_file(const char *path, char *buffer, size_t size) {
    if (!buffer || size == 0) return -1;
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buffer, size - 1u);
    close(fd);
    if (n < 0) return -1;
    buffer[n] = 0;
    return (int)n;
}

static const char *find_key(const char *json, const char *key) {
    const size_t key_len = s_len(key);
    for (const char *p = json; p && *p; ++p) {
        if (*p != '"') continue;
        const char *q = p + 1;
        size_t i = 0;
        while (i < key_len && q[i] == key[i]) ++i;
        if (i != key_len || q[i] != '"') continue;
        q += i + 1;
        while (*q == ' ' || *q == '\n' || *q == '\r' || *q == '\t') ++q;
        if (*q != ':') continue;
        ++q;
        while (*q == ' ' || *q == '\n' || *q == '\r' || *q == '\t') ++q;
        return q;
    }
    return 0;
}

static int json_string(const char *json, const char *key, char *dst, size_t size) {
    const char *p = find_key(json, key);
    if (!p) return 0;
    if (*p == '[') {
        while (*p && *p != '"') ++p;
    }
    if (*p != '"') return 0;
    ++p;
    size_t n = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) ++p;
        if (n + 1 < size) dst[n++] = *p;
        ++p;
    }
    if (size) dst[n] = 0;
    return *p == '"';
}

static int json_int(const char *json, const char *key, int fallback) {
    const char *p = find_key(json, key);
    if (!p) return fallback;
    int v = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        ++p;
        ++digits;
    }
    return digits ? v : fallback;
}

static void install_timezone(const char *timezone) {
    int fd = open("/etc/timezone", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0) {
        put_fd(fd, timezone);
        put_fd(fd, "\n");
        close(fd);
    }
    fd = open("/etc/localtime", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd >= 0) {
        put_fd(fd, "TZ=");
        put_fd(fd, timezone);
        put_fd(fd, "\n");
        close(fd);
    }
}

static void service_sleep_seconds(long seconds) {
    struct timespec ts;
    ts.tv_sec = seconds;
    ts.tv_nsec = 0;
    (void)nanosleep(&ts, 0);
}

static int sync_time_once(const char *server, int port, int set_clock) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo *result = 0;
    if (getaddrinfo(server, 0, &hints, &result) != 0 || !result) {
        put_fd(1, "RADIX_SERVICE_TIME_SYNC_RESOLVE_FAIL\n");
        return 2;
    }

    struct sockaddr_in remote = *(struct sockaddr_in*)result->ai_addr;
    remote.sin_port = htons((uint16_t)port);
    freeaddrinfo(result);

    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        put_fd(1, "RADIX_SERVICE_TIME_SYNC_SOCKET_FAIL\n");
        return 3;
    }
    uint8_t request[48];
    memset(request, 0, sizeof(request));
    request[0] = 0x1b;
    if (sendto(fd, request, sizeof(request), 0, (struct sockaddr*)&remote, sizeof(remote)) != (ssize_t)sizeof(request)) {
        close(fd);
        put_fd(1, "RADIX_SERVICE_TIME_SYNC_SEND_FAIL\n");
        return 4;
    }

    uint8_t response[64];
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    ssize_t received = -1;
    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = 1000000;
    for (int attempt = 0; attempt < 1200; ++attempt) {
        memset(&from, 0, sizeof(from));
        from_len = sizeof(from);
        received = recvfrom(fd, response, sizeof(response), 0, (struct sockaddr*)&from, &from_len);
        if (received >= 48) break;
        nanosleep(&delay, 0);
    }
    close(fd);
    if (received < 48) {
        put_fd(1, "RADIX_SERVICE_TIME_SYNC_TIMEOUT\n");
        return 5;
    }
    uint32_t ntp_seconds = read_be32(response + 40);
    if (ntp_seconds < 2208988800u) {
        put_fd(1, "RADIX_SERVICE_TIME_SYNC_BAD_SAMPLE\n");
        return 6;
    }
    uint64_t unix_seconds = (uint64_t)(ntp_seconds - 2208988800u);
    if (set_clock) {
        struct timeval tv;
        tv.tv_sec = (long)unix_seconds;
        tv.tv_usec = 0;
        if (settimeofday(&tv, 0) != 0) {
            put_fd(1, "RADIX_SERVICE_TIME_SYNC_SETTIME_FAIL\n");
            return 7;
        }
    }
    put_fd(1, "RADIX_SERVICE_TZDATA_OK\n");
    put_fd(1, "RADIX_SERVICE_TIME_SYNC_OK\n");
    return 0;
}

int main(int argc, char **argv) {
    const char *config_path = argc > 1 ? argv[1] : "/etc/radix/time/time-sync.json";
    for (;;) {
        char json[1024];
        char server[96] = "time.google.com";
        char timezone[96] = "America/Los_Angeles";
        int port = 123;
        int set_clock = 1;
        int interval = 3600;
        if (read_file(config_path, json, sizeof(json)) >= 0) {
            (void)json_string(json, "server", server, sizeof(server));
            (void)json_string(json, "timezone", timezone, sizeof(timezone));
            port = json_int(json, "port", port);
            set_clock = json_int(json, "set_clock", 1);
            interval = json_int(json, "poll_interval_seconds", interval);
            interval = json_int(json, "sync_interval_seconds", interval);
        }
        if (interval < 60) interval = 60;
        install_timezone(timezone);
        int status = sync_time_once(server, port, set_clock);
        if (status != 0) return status;
        service_sleep_seconds(interval);
    }
}
