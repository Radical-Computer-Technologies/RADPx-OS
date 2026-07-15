#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
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
    if (!p || *p != '"') return 0;
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

static void service_sleep_seconds(long seconds) {
    struct timespec ts;
    ts.tv_sec = seconds;
    ts.tv_nsec = 0;
    (void)nanosleep(&ts, 0);
}

static int write_resolver_config(const char *config_path) {
    char json[1024];
    char nameserver[64] = "10.0.2.3";
    char search[96] = "";
    if (read_file(config_path, json, sizeof(json)) >= 0) {
        (void)json_string(json, "nameserver", nameserver, sizeof(nameserver));
        (void)json_string(json, "search", search, sizeof(search));
    }
    int fd = open("/etc/resolv.conf", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        return 2;
    }
    put_fd(fd, "nameserver ");
    put_fd(fd, nameserver);
    put_fd(fd, "\n");
    if (search[0]) {
        put_fd(fd, "search ");
        put_fd(fd, search);
        put_fd(fd, "\n");
    }
    put_fd(fd, "options timeout:1 attempts:2\n");
    close(fd);
    return 0;
}

int main(int argc, char **argv) {
    const char *config_path = argc > 1 ? argv[1] : "/etc/radix/network/dns-resolver.json";
    if (write_resolver_config(config_path) != 0) {
        put_fd(1, "RADIX_SERVICE_DNS_RESOLVER_FAIL\n");
        return 2;
    }
    put_fd(1, "RADIX_SERVICE_DNS_RESOLVER_OK\n");
    for (;;) {
        service_sleep_seconds(30);
        (void)write_resolver_config(config_path);
    }
}
