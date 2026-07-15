#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define RADIX_IOCTL_TYPE_NET 'N'
#define RADIX_DEVICE_IOCTL_NET_LINK_INFO RADIX_IOR(RADIX_IOCTL_TYPE_NET, 1u, struct radix_net_link_info)
#define RADIX_DEVICE_IOCTL_NET_CONFIGURE RADIX_IOW(RADIX_IOCTL_TYPE_NET, 7u, struct radix_net_stack_config)

struct radix_ipv4 {
    uint8_t bytes[4];
};

struct radix_mac {
    uint8_t bytes[6];
};

struct radix_net_link_info {
    uint32_t size;
    struct radix_mac mac;
    uint32_t mtu;
    int link_up;
    uint64_t tx_packets;
    uint64_t rx_packets;
};

struct radix_net_stack_config {
    uint32_t size;
    struct radix_ipv4 ipv4;
    struct radix_ipv4 netmask;
    struct radix_ipv4 gateway;
    struct radix_ipv4 ntp_server;
    uint16_t ntp_port;
    uint16_t flags;
};

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

static int parse_ipv4(const char *text, struct radix_ipv4 *out) {
    if (!text || !out) return 0;
    uint32_t values[4] = {0, 0, 0, 0};
    const char *p = text;
    for (int i = 0; i < 4; ++i) {
        if (*p < '0' || *p > '9') return 0;
        while (*p >= '0' && *p <= '9') {
            values[i] = values[i] * 10u + (uint32_t)(*p - '0');
            if (values[i] > 255u) return 0;
            ++p;
        }
        if (i < 3) {
            if (*p != '.') return 0;
            ++p;
        }
    }
    out->bytes[0] = (uint8_t)values[0];
    out->bytes[1] = (uint8_t)values[1];
    out->bytes[2] = (uint8_t)values[2];
    out->bytes[3] = (uint8_t)values[3];
    return 1;
}

static void configure_from_json(struct radix_net_stack_config *config, const char *json) {
    char value[64];
    if (json_string(json, "ipv4", value, sizeof(value))) (void)parse_ipv4(value, &config->ipv4);
    if (json_string(json, "netmask", value, sizeof(value))) (void)parse_ipv4(value, &config->netmask);
    if (json_string(json, "gateway", value, sizeof(value))) (void)parse_ipv4(value, &config->gateway);
    if (json_string(json, "ntp_server_ip", value, sizeof(value))) (void)parse_ipv4(value, &config->ntp_server);
    int port = json_int(json, "ntp_port", config->ntp_port);
    if (port > 0 && port <= 65535) config->ntp_port = (uint16_t)port;
}

static void service_sleep_seconds(long seconds) {
    struct timespec ts;
    ts.tv_sec = seconds;
    ts.tv_nsec = 0;
    (void)nanosleep(&ts, 0);
}

int main(int argc, char **argv) {
    const char *config_path = argc > 1 ? argv[1] : "/etc/radix/network/netinterfaces.json";
    struct radix_net_stack_config config;
    memset(&config, 0, sizeof(config));
    config.size = sizeof(config);
    (void)parse_ipv4("10.0.2.15", &config.ipv4);
    (void)parse_ipv4("255.255.255.0", &config.netmask);
    (void)parse_ipv4("10.0.2.2", &config.gateway);
    (void)parse_ipv4("216.239.35.0", &config.ntp_server);
    config.ntp_port = 123;

    char json[1024];
    if (read_file(config_path, json, sizeof(json)) >= 0) configure_from_json(&config, json);

    int fd = open("/dev/net0", O_RDWR);
    if (fd < 0) {
        put_fd(1, "RADIX_SERVICE_NETWORK_NO_DEVICE\n");
        return 2;
    }
    struct radix_net_link_info link;
    memset(&link, 0, sizeof(link));
    link.size = sizeof(link);
    if (ioctl(fd, RADIX_DEVICE_IOCTL_NET_LINK_INFO, &link) < 0 || !link.link_up) {
        close(fd);
        put_fd(1, "RADIX_NETWORK_INTERFACE_DOWN\n");
        return 3;
    }
    if (ioctl(fd, RADIX_DEVICE_IOCTL_NET_CONFIGURE, &config) < 0) {
        close(fd);
        put_fd(1, "RADIX_SERVICE_NETWORK_CONFIG_FAIL\n");
        return 4;
    }
    close(fd);
    put_fd(1, "RADIX_NETWORK_INTERFACE_UP_OK\n");
    put_fd(1, "RADIX_SERVICE_NETWORK_OK\n");
    for (;;) {
        service_sleep_seconds(5);
        fd = open("/dev/net0", O_RDWR);
        if (fd < 0) continue;
        memset(&link, 0, sizeof(link));
        link.size = sizeof(link);
        (void)ioctl(fd, RADIX_DEVICE_IOCTL_NET_LINK_INFO, &link);
        close(fd);
    }
}
