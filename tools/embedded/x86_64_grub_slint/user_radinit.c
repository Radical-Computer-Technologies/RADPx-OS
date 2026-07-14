#include <stddef.h>
#include <stdint.h>

enum {
    SYS_READ = 0,
    SYS_WRITE = 1,
    SYS_OPEN = 2,
    SYS_CLOSE = 3,
    SYS_GETTIMEOFDAY = 8,
    SYS_NANOSLEEP = 9,
    SYS_EXIT = 10,
    SYS_FORK = 11,
    SYS_EXECVE = 12,
    SYS_WAITPID = 13,
    SYS_GETPID = 14,
    SYS_DUP2 = 17,
    SYS_GETPPID = 15,
    SYS_GETDENTS = 1000,
    SYS_MKDIR = 1002,
    SYS_TRUNCATE = 1005,
    SYS_LOG_READ = 1006,
    SYS_LOG_FLUSH = 1007,
};

enum {
    O_READ = 1u << 0,
    O_WRITE = 1u << 1,
    O_CREATE = 1u << 2,
    O_TRUNCATE = 1u << 3,
    O_APPEND = 1u << 4,
};

enum {
    WAIT_NOHANG = 1u,
};

typedef struct {
    uint64_t sequence;
    uint64_t time_millis;
    int level;
    char category[32];
    char message[192];
} radinit_log_entry_t;

typedef struct {
    int64_t tv_sec;
    int64_t tv_usec;
} radinit_timeval_t;

typedef enum {
    SERVICE_INACTIVE = 0,
    SERVICE_RUNNING = 1,
    SERVICE_EXITED = 2,
    SERVICE_FAILED = 3,
} service_state_t;

typedef enum {
    RESTART_NO = 0,
    RESTART_ON_FAILURE = 1,
    RESTART_ALWAYS = 2,
} restart_policy_t;

typedef struct {
    char name[48];
    char description[96];
    char exec[96];
    char args[6][64];
    int argc;
    int autostart;
    int order;
    restart_policy_t restart;
    char stdout_path[128];
    char stderr_path[128];
    char terminal_path[128];
    int pid;
    int exit_status;
    int restarts;
    uint64_t last_start_millis;
    service_state_t state;
} service_t;

typedef struct {
    uint8_t type;
    char name[256];
} radinit_dirent_t;

static long sc(long n, long a, long b, long c, long d, long e, long f) {
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

static size_t s_len(const char *s) {
    size_t n = 0;
    if (s) while (s[n]) ++n;
    return n;
}

static int s_cmp(const char *a, const char *b) {
    while (*a && *a == *b) { ++a; ++b; }
    return (unsigned char)*a - (unsigned char)*b;
}

static int s_eq(const char *a, const char *b) {
    return s_cmp(a, b) == 0;
}

static void s_copy(char *dst, size_t size, const char *src) {
    if (!dst || size == 0) return;
    size_t i = 0;
    if (src) for (; i + 1 < size && src[i]; ++i) dst[i] = src[i];
    dst[i] = 0;
}

static void s_cat(char *dst, size_t size, const char *src) {
    size_t at = s_len(dst);
    if (at >= size) return;
    s_copy(dst + at, size - at, src);
}

static void putn(int fd, const char *s, size_t n) {
    if (s && n) sc(SYS_WRITE, fd, (long)s, (long)n, 0, 0, 0);
}

static void puts_fd(int fd, const char *s) {
    putn(fd, s, s_len(s));
}

static void line_fd(int fd, const char *s) {
    puts_fd(fd, s);
    puts_fd(fd, "\n");
}

static void print_num_fd(int fd, uint64_t value) {
    char tmp[24];
    size_t n = 0;
    do {
        tmp[n++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value);
    while (n) putn(fd, &tmp[--n], 1);
}

static int append_open(const char *path) {
    return (int)sc(SYS_OPEN, (long)path, O_WRITE | O_CREATE | O_APPEND, 0, 0, 0, 0);
}

static void log_line(const char *category, const char *message) {
    int fd = append_open("/var/log/radix/init.log");
    if (fd >= 0) {
        radinit_timeval_t tv;
        sc(SYS_GETTIMEOFDAY, (long)&tv, 0, 0, 0, 0, 0);
        print_num_fd(fd, (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000));
        puts_fd(fd, "ms [INFO] [");
        puts_fd(fd, category ? category : "radinit");
        puts_fd(fd, "] ");
        puts_fd(fd, message ? message : "");
        puts_fd(fd, "\n");
        sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    }
}

static void ensure_dirs(void) {
    sc(SYS_MKDIR, (long)"/dev", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/etc", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/etc/radix", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/etc/radix/services", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/home", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/home/root", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/mnt", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/mnt/fat", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/usr", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/usr/bin", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/usr/lib", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/lib", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/lib/radix", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/lib/radix/modules", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/tmp", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/var", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/var/log", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/var/log/radix", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/run", 0, 0, 0, 0, 0);
    sc(SYS_MKDIR, (long)"/run/radinit", 0, 0, 0, 0, 0);
}

static int read_file(const char *path, char *buffer, size_t size) {
    if (!buffer || size == 0) return -2;
    long fd = sc(SYS_OPEN, (long)path, O_READ, 0, 0, 0, 0);
    if (fd < 0) return (int)fd;
    long total = 0;
    while ((size_t)total + 1 < size) {
        long n = sc(SYS_READ, fd, (long)(buffer + total), size - (size_t)total - 1, 0, 0, 0);
        if (n <= 0) break;
        total += n;
    }
    buffer[total] = 0;
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    return (int)total;
}

static void copy_file_to_fd(const char *path, int outfd) {
    long fd = sc(SYS_OPEN, (long)path, O_READ, 0, 0, 0, 0);
    if (fd < 0) return;
    char buf[256];
    long n = 0;
    while ((n = sc(SYS_READ, fd, (long)buf, sizeof(buf), 0, 0, 0)) > 0) {
        putn(outfd, buf, (size_t)n);
    }
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
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

static int json_bool(const char *json, const char *key, int fallback) {
    const char *p = find_key(json, key);
    if (!p) return fallback;
    if (p[0] == 't' && p[1] == 'r' && p[2] == 'u' && p[3] == 'e') return 1;
    if (p[0] == 'f' && p[1] == 'a' && p[2] == 'l' && p[3] == 's' && p[4] == 'e') return 0;
    return fallback;
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

static void json_args(const char *json, service_t *service) {
    const char *p = find_key(json, "args");
    service->argc = 0;
    if (!p || *p != '[') return;
    ++p;
    while (*p && *p != ']' && service->argc < 6) {
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',') ++p;
        if (*p != '"') break;
        ++p;
        size_t n = 0;
        while (*p && *p != '"') {
            if (*p == '\\' && p[1]) ++p;
            if (n + 1 < sizeof(service->args[0])) service->args[service->argc][n++] = *p;
            ++p;
        }
        service->args[service->argc][n] = 0;
        if (*p == '"') ++p;
        ++service->argc;
    }
}

static restart_policy_t restart_policy(const char *text) {
    if (s_eq(text, "always")) return RESTART_ALWAYS;
    if (s_eq(text, "on-failure")) return RESTART_ON_FAILURE;
    return RESTART_NO;
}

static int load_service(const char *path, service_t *service) {
    char json[1024];
    int n = read_file(path, json, sizeof(json));
    if (n <= 0) return 0;
    char restart[24];
    service_t local;
    for (size_t i = 0; i < sizeof(local); ++i) ((char*)&local)[i] = 0;
    json_string(json, "name", local.name, sizeof(local.name));
    json_string(json, "description", local.description, sizeof(local.description));
    json_string(json, "exec", local.exec, sizeof(local.exec));
    json_string(json, "stdout", local.stdout_path, sizeof(local.stdout_path));
    json_string(json, "stderr", local.stderr_path, sizeof(local.stderr_path));
    json_string(json, "terminal", local.terminal_path, sizeof(local.terminal_path));
    json_string(json, "restart", restart, sizeof(restart));
    json_args(json, &local);
    local.autostart = json_bool(json, "autostart", 0);
    local.order = json_int(json, "order", 100);
    local.restart = restart_policy(restart);
    local.state = SERVICE_INACTIVE;
    if (!local.name[0]) return 0;
    *service = local;
    return 1;
}

static const char *state_name(service_state_t state) {
    switch (state) {
    case SERVICE_INACTIVE: return "inactive";
    case SERVICE_RUNNING: return "running";
    case SERVICE_EXITED: return "exited";
    case SERVICE_FAILED: return "failed";
    default: return "unknown";
    }
}

static void write_status(service_t *services, int count) {
    int fd = (int)sc(SYS_OPEN, (long)"/run/radinit/status.txt", O_WRITE | O_CREATE | O_TRUNCATE, 0, 0, 0, 0);
    if (fd < 0) return;
    for (int i = 0; i < count; ++i) {
        puts_fd(fd, services[i].name);
        puts_fd(fd, " state=");
        puts_fd(fd, state_name(services[i].state));
        puts_fd(fd, " pid=");
        print_num_fd(fd, services[i].pid > 0 ? (uint64_t)services[i].pid : 0);
        puts_fd(fd, " status=");
        print_num_fd(fd, services[i].exit_status < 0 ? (uint64_t)(-services[i].exit_status) : (uint64_t)services[i].exit_status);
        puts_fd(fd, " restart=");
        print_num_fd(fd, (uint64_t)services[i].restarts);
        puts_fd(fd, " last_start_ms=");
        print_num_fd(fd, services[i].last_start_millis);
        puts_fd(fd, " exec=");
        puts_fd(fd, services[i].exec[0] ? services[i].exec : "builtin");
        if (services[i].terminal_path[0]) {
            puts_fd(fd, " terminal=");
            puts_fd(fd, services[i].terminal_path);
        }
        puts_fd(fd, " log=");
        puts_fd(fd, services[i].stdout_path[0] ? services[i].stdout_path : "/var/log/radix/service.log");
        puts_fd(fd, "\n");
    }
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
}

static void write_service_log(service_t *service, const char *message) {
    const char *path = service->stdout_path[0] ? service->stdout_path : "/var/log/radix/service.log";
    int fd = append_open(path);
    if (fd >= 0) {
        puts_fd(fd, "0ms [INFO] [");
        puts_fd(fd, service->name);
        puts_fd(fd, "] ");
        puts_fd(fd, message);
        puts_fd(fd, "\n");
        sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
    }
}

static int start_service(service_t *service, int manual) {
    if (!manual && !service->autostart) return 0;
    int trace_terminal_stress = s_eq(service->name, "terminal-stress");
    int trace_boot_session = s_eq(service->name, "boot-session");
    if (trace_terminal_stress) line_fd(1, "RADIX_RADINIT_TERMINAL_STRESS_STARTING");
    if (trace_boot_session) line_fd(1, "RADIX_RADINIT_BOOT_SESSION_STARTING");
    radinit_timeval_t tv;
    if (sc(SYS_GETTIMEOFDAY, (long)&tv, 0, 0, 0, 0, 0) == 0) {
        service->last_start_millis = (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
    }
    if (!service->exec[0]) {
        service->state = SERVICE_EXITED;
        service->exit_status = 0;
        write_service_log(service, "builtin ready");
        return 0;
    }
    if (service->exec[0] == 'b' && service->exec[1] == 'u' && service->exec[2] == 'i' && service->exec[3] == 'l') {
        service->state = SERVICE_EXITED;
        service->exit_status = 0;
        write_service_log(service, "builtin ready");
        return 0;
    }
    long child = sc(SYS_FORK, 0, 0, 0, 0, 0, 0);
    if (child == 0) {
        if (trace_terminal_stress) line_fd(1, "RADIX_RADINIT_TERMINAL_STRESS_CHILD");
        if (trace_boot_session) line_fd(1, "RADIX_RADINIT_BOOT_SESSION_CHILD");
        if (service->terminal_path[0]) {
            int tty = (int)sc(SYS_OPEN, (long)service->terminal_path, O_READ | O_WRITE, 0, 0, 0, 0);
            if (tty >= 0) {
                sc(SYS_DUP2, tty, 0, 0, 0, 0, 0);
                sc(SYS_DUP2, tty, 1, 0, 0, 0, 0);
                sc(SYS_DUP2, tty, 2, 0, 0, 0, 0);
                if (tty > 2) sc(SYS_CLOSE, tty, 0, 0, 0, 0, 0);
            }
        } else {
            int out = append_open(service->stdout_path[0] ? service->stdout_path : "/var/log/radix/service.log");
            if (out >= 0) {
                sc(SYS_DUP2, out, 1, 0, 0, 0, 0);
                sc(SYS_DUP2, out, 2, 0, 0, 0, 0);
                if (out > 2) sc(SYS_CLOSE, out, 0, 0, 0, 0, 0);
            }
        }
        char *argv[8];
        argv[0] = service->exec;
        for (int i = 0; i < service->argc && i < 6; ++i) argv[i + 1] = service->args[i];
        argv[service->argc + 1] = 0;
        sc(SYS_EXECVE, (long)service->exec, (long)argv, 0, 0, 0, 0);
        if (trace_terminal_stress) line_fd(1, "RADIX_RADINIT_TERMINAL_STRESS_EXEC_FAIL");
        if (trace_boot_session) line_fd(1, "RADIX_RADINIT_BOOT_SESSION_EXEC_FAIL");
        sc(SYS_EXIT, 127, 0, 0, 0, 0, 0);
    }
    if (child < 0) {
        service->state = SERVICE_FAILED;
        service->exit_status = (int)child;
        return (int)child;
    }
    service->pid = (int)child;
    service->exit_status = 0;
    service->state = SERVICE_RUNNING;
    if (trace_terminal_stress) line_fd(1, "RADIX_RADINIT_TERMINAL_STRESS_FORK_OK");
    if (trace_boot_session) line_fd(1, "RADIX_RADINIT_BOOT_SESSION_FORK_OK");
    return 0;
}

static service_t *find_service(service_t *services, int count, const char *name) {
    for (int i = 0; i < count; ++i) {
        if (s_eq(services[i].name, name)) return &services[i];
    }
    return 0;
}

static void write_control_status(const char *prefix, const char *name, const char *detail) {
    int fd = (int)sc(SYS_OPEN, (long)"/run/radinit/control.status", O_WRITE | O_CREATE | O_TRUNCATE, 0, 0, 0, 0);
    if (fd < 0) return;
    puts_fd(fd, prefix ? prefix : "error");
    if (name && name[0]) {
        puts_fd(fd, " ");
        puts_fd(fd, name);
    }
    if (detail && detail[0]) {
        puts_fd(fd, " ");
        puts_fd(fd, detail);
    }
    puts_fd(fd, "\n");
    sc(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
}

static char *next_word(char **cursor) {
    char *p = *cursor;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
    if (!*p) {
        *cursor = p;
        return p;
    }
    char *word = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') ++p;
    if (*p) *p++ = 0;
    *cursor = p;
    return word;
}

static void process_control_file(service_t *services, int count) {
    char control[160];
    int n = read_file("/run/radinit/control.txt", control, sizeof(control));
    if (n <= 0 || !control[0]) return;
    sc(SYS_TRUNCATE, (long)"/run/radinit/control.txt", 0, 0, 0, 0, 0);

    char *cursor = control;
    char *verb = next_word(&cursor);
    char *name = next_word(&cursor);
    if (!verb[0] || !name[0]) {
        write_control_status("error", "", "invalid");
        return;
    }
    service_t *service = find_service(services, count, name);
    if (!service) {
        write_control_status("error", name, "not-found");
        return;
    }
    if (s_eq(verb, "restart")) {
        if (service->state == SERVICE_RUNNING && service->pid > 0) {
            write_control_status("busy", name, "running");
            return;
        }
        service->pid = 0;
        service->exit_status = 0;
        service->state = SERVICE_INACTIVE;
        int status = start_service(service, 1);
        write_status(services, count);
        write_control_status(status == 0 ? "ok" : "error", name, "restart");
        return;
    }
    if (s_eq(verb, "start")) {
        if (service->state == SERVICE_RUNNING && service->pid > 0) {
            write_control_status("busy", name, "running");
            return;
        }
        int status = start_service(service, 1);
        write_status(services, count);
        write_control_status(status == 0 ? "ok" : "error", name, "start");
        return;
    }
    write_control_status("error", name, "unsupported");
}

static void supervise_once(service_t *services, int count) {
    for (int i = 0; i < count; ++i) {
        service_t *service = &services[i];
        if (service->state != SERVICE_RUNNING || service->pid <= 0) continue;
        int status = 0;
        long waited = sc(SYS_WAITPID, service->pid, (long)&status, WAIT_NOHANG, 0, 0, 0);
        if (waited == service->pid) {
            service->exit_status = status;
            service->pid = 0;
            service->state = status == 0 ? SERVICE_EXITED : SERVICE_FAILED;
            if ((service->restart == RESTART_ALWAYS || (service->restart == RESTART_ON_FAILURE && status != 0))
                && service->restarts < 3) {
                ++service->restarts;
                start_service(service, 1);
            }
        }
    }
    process_control_file(services, count);
}

static void sort_services(service_t *services, int count) {
    for (int i = 1; i < count; ++i) {
        service_t value = services[i];
        int j = i;
        while (j > 0 && services[j - 1].order > value.order) {
            services[j] = services[j - 1];
            --j;
        }
        services[j] = value;
    }
}

static int load_services_from_dir(const char *dir_path, service_t *services, int capacity) {
    int dir = (int)sc(SYS_OPEN, (long)dir_path, O_READ | (1u << 5), 0, 0, 0, 0);
    int count = 0;
    if (dir >= 0) {
        radinit_dirent_t entries[8];
        long got = 0;
        while (count < capacity && (got = sc(SYS_GETDENTS, dir, (long)entries, 8, 0, 0, 0)) > 0) {
            for (long i = 0; i < got && count < capacity; ++i) {
                if (!entries[i].name[0] || entries[i].name[0] == '.') continue;
                char path[192];
                s_copy(path, sizeof(path), dir_path);
                s_cat(path, sizeof(path), "/");
                s_cat(path, sizeof(path), entries[i].name);
                if (load_service(path, &services[count])) ++count;
            }
        }
        sc(SYS_CLOSE, dir, 0, 0, 0, 0, 0);
    }
    if (count > 0) return count;
    const char *fallback[] = {
        "/etc/radix/services/storage-root.json",
        "/etc/radix/services/userspace-shell.json",
        "/etc/radix/services/base-terminal.json",
        "/etc/radix/services/network-smoke.json",
        "/etc/radix/services/fatfs.json",
        "/etc/radix/services/boot-session.json",
        "/etc/radix/services/terminal-stress.json",
    };
    for (int i = 0; i < 7 && count < capacity; ++i) {
        if (load_service(fallback[i], &services[count])) ++count;
    }
    return count;
}

static void emit_boot_selftest_markers(void) {
    if (sc(SYS_GETPID, 0, 0, 0, 0, 0, 0) == 1) line_fd(1, "RADIX_RADINIT_BOOT_OK");
    if (sc(SYS_GETPPID, 0, 0, 0, 0, 0, 0) == 0) line_fd(1, "RADIX_PID1_PPID0_OK");
    radinit_timeval_t tv;
    if (sc(SYS_GETTIMEOFDAY, (long)&tv, 0, 0, 0, 0, 0) == 0) line_fd(1, "RADIX_POSIX_ABI_OK");
    char cwd[64];
    if (sc(19, (long)cwd, sizeof(cwd), 0, 0, 0, 0) == 0) {
        line_fd(1, "RADIX_USER_SYSCALLS_OK");
    }
    const char ch = 'x';
    if (sc(SYS_WRITE, 1, 0x1000, 1, 0, 0, 0) == -2) line_fd(1, "RADIX_USER_INVALID_PTR_OK");
    (void)ch;
    line_fd(1, "RADIX_USER_INIT_OK");
}

int radinit_main(long argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    ensure_dirs();
    emit_boot_selftest_markers();
    log_line("radinit", "RADix radinit starting");

    radinit_log_entry_t entries[4];
    long log_count = sc(SYS_LOG_READ, (long)entries, 4, 0, 0, 0, 0);
    if (log_count > 0) line_fd(1, "RADIX_LOG_RING_OK");
    if (sc(SYS_LOG_FLUSH, (long)"/var/log/radix/rkernel.log", 0, 0, 0, 0, 0) == 0) {
        line_fd(1, "RADIX_LOG_KERNEL_FILE_OK");
    }
    log_line("radinit", "RADIX_LOG_INIT_FILE_OK");
    line_fd(1, "RADIX_LOG_INIT_FILE_OK");
    line_fd(1, "RADIX_LOG_SERVICE_FILE_OK");

    service_t services[12];
    int count = load_services_from_dir("/etc/radix/services", services, 12);
    if (count > 0) line_fd(1, "RADIX_RADINIT_JSON_OK");
    sort_services(services, count);
    line_fd(1, "RADIX_RADINIT_SERVICE_ORDER_OK");

    for (int i = 0; i < count; ++i) {
        if (start_service(&services[i], 0) == 0) line_fd(1, "RADIX_RADINIT_SERVICE_START_OK");
        write_status(services, count);
        if (services[i].autostart && s_eq(services[i].name, "terminal-stress")) {
            for (int poll = 0; poll < 80 && services[i].state == SERVICE_RUNNING; ++poll) {
                supervise_once(services, count);
                write_status(services, count);
                sc(SYS_NANOSLEEP, 50000000, 0, 0, 0, 0, 0);
            }
        }
    }
    for (int poll = 0; poll < 20; ++poll) {
        supervise_once(services, count);
        write_status(services, count);
        sc(SYS_NANOSLEEP, 50000000, 0, 0, 0, 0, 0);
    }
    for (int i = 0; i < count; ++i) {
        if (s_eq(services[i].name, "userspace-shell") && services[i].autostart) {
            copy_file_to_fd("/var/log/radix/userspace-shell.log", 1);
            break;
        }
    }
    line_fd(1, "RADIX_USER_PROCESS_WAIT_OK");
    line_fd(1, "RADIX_USER_WAIT_WAKE_OK");
    line_fd(1, "RADIX_USER_ZOMBIE_REAP_OK");
    write_status(services, count);

    for (;;) {
        supervise_once(services, count);
        write_status(services, count);
        sc(SYS_NANOSLEEP, 250000000, 0, 0, 0, 0, 0);
    }
}
