#include <radixkernel/radixkernel.h>

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#if __has_include("rkconfig.h")
#include "rkconfig.h"
#endif

#if defined(__cplusplus) && defined(__STDC_HOSTED__) && __STDC_HOSTED__ == 0
extern "C" void *malloc(size_t);
extern "C" void free(void*);
#endif

#ifndef RADIX_RKCONFIG_NET_IPV4_A
#define RADIX_RKCONFIG_NET_IPV4_A 10
#define RADIX_RKCONFIG_NET_IPV4_B 0
#define RADIX_RKCONFIG_NET_IPV4_C 2
#define RADIX_RKCONFIG_NET_IPV4_D 15
#endif

#ifndef RADIX_RKCONFIG_NET_NETMASK_A
#define RADIX_RKCONFIG_NET_NETMASK_A 255
#define RADIX_RKCONFIG_NET_NETMASK_B 255
#define RADIX_RKCONFIG_NET_NETMASK_C 255
#define RADIX_RKCONFIG_NET_NETMASK_D 0
#endif

#ifndef RADIX_RKCONFIG_NET_GATEWAY_A
#define RADIX_RKCONFIG_NET_GATEWAY_A 10
#define RADIX_RKCONFIG_NET_GATEWAY_B 0
#define RADIX_RKCONFIG_NET_GATEWAY_C 2
#define RADIX_RKCONFIG_NET_GATEWAY_D 2
#endif

#ifndef RADIX_RKCONFIG_NET_NTP_A
#define RADIX_RKCONFIG_NET_NTP_A 10
#define RADIX_RKCONFIG_NET_NTP_B 0
#define RADIX_RKCONFIG_NET_NTP_C 2
#define RADIX_RKCONFIG_NET_NTP_D 2
#endif

#ifndef RADIX_RKCONFIG_NET_NTP_PORT
#define RADIX_RKCONFIG_NET_NTP_PORT 12300
#endif

#ifndef RADIX_KERNEL_MAX_TASKS
#define RADIX_KERNEL_MAX_TASKS 16u
#endif

#ifndef RADIX_KERNEL_MAX_DEVICES
#define RADIX_KERNEL_MAX_DEVICES 24u
#endif

#ifndef RADIX_KERNEL_MAX_COMMANDS
#define RADIX_KERNEL_MAX_COMMANDS 48u
#endif

#ifndef RADIX_KERNEL_MAX_MOUNTS
#define RADIX_KERNEL_MAX_MOUNTS 4u
#endif

#ifndef RADIX_KERNEL_MAX_VFS_PROVIDERS
#define RADIX_KERNEL_MAX_VFS_PROVIDERS 4u
#endif

#ifndef RADIX_KERNEL_MAX_VFS_FILES
#define RADIX_KERNEL_MAX_VFS_FILES 16u
#endif

#ifndef RADIX_KERNEL_MAX_DIR_HANDLES
#define RADIX_KERNEL_MAX_DIR_HANDLES 8u
#endif

#ifndef RADIX_KERNEL_MAX_DIR_ENTRIES
#define RADIX_KERNEL_MAX_DIR_ENTRIES 64u
#endif

#ifndef RADIX_KERNEL_MAX_PROGRAMS
#define RADIX_KERNEL_MAX_PROGRAMS 8u
#endif

#ifndef RADIX_KERNEL_MAX_TTYS
#define RADIX_KERNEL_MAX_TTYS 8u
#endif

#ifndef RADIX_KERNEL_MAX_PTYS
#define RADIX_KERNEL_MAX_PTYS 4u
#endif

#ifndef RADIX_KERNEL_MAX_POSIX_FDS
#define RADIX_KERNEL_MAX_POSIX_FDS 32u
#endif

#ifndef RADIX_KERNEL_MAX_PIPES
#define RADIX_KERNEL_MAX_PIPES 8u
#endif

#ifndef RADIX_KERNEL_MAX_SHM_OBJECTS
#define RADIX_KERNEL_MAX_SHM_OBJECTS 8u
#endif

#ifndef RADIX_KERNEL_MAX_UDP_SOCKETS
#define RADIX_KERNEL_MAX_UDP_SOCKETS 8u
#endif

#ifndef RADIX_KERNEL_MAX_UDP_DATAGRAMS
#define RADIX_KERNEL_MAX_UDP_DATAGRAMS 8u
#endif

#ifndef RADIX_KERNEL_UDP_PAYLOAD_BYTES
#define RADIX_KERNEL_UDP_PAYLOAD_BYTES 512u
#endif

#ifndef RADIX_KERNEL_PIPE_BUFFER_BYTES
#define RADIX_KERNEL_PIPE_BUFFER_BYTES 512u
#endif

#ifndef RADIX_KERNEL_MAX_PROCESSES
#define RADIX_KERNEL_MAX_PROCESSES 16u
#endif

#ifndef RADIX_KERNEL_MAX_MODULES
#define RADIX_KERNEL_MAX_MODULES 16u
#endif

#ifndef RADIX_KERNEL_MAX_SERVICES
#define RADIX_KERNEL_MAX_SERVICES 16u
#endif

#ifndef RADIX_KERNEL_MAX_IRQS
#define RADIX_KERNEL_MAX_IRQS 64u
#endif

#ifndef RADIX_KERNEL_MAX_PERF_COUNTERS
#define RADIX_KERNEL_MAX_PERF_COUNTERS 32u
#endif

#ifndef RADIX_KERNEL_MAX_WORK_ITEMS
#define RADIX_KERNEL_MAX_WORK_ITEMS 32u
#endif

#ifndef RADIX_KERNEL_MAX_WAIT_QUEUES
#define RADIX_KERNEL_MAX_WAIT_QUEUES 8u
#endif

#ifndef RADIX_KERNEL_MAX_TIMER_SOURCES
#define RADIX_KERNEL_MAX_TIMER_SOURCES 4u
#endif

#ifndef RADIX_KERNEL_MAX_INPUT_QUEUES
#define RADIX_KERNEL_MAX_INPUT_QUEUES 8u
#endif

#ifndef RADIX_KERNEL_MAX_LOG_ENTRIES
#define RADIX_KERNEL_MAX_LOG_ENTRIES 256u
#endif

#ifndef RADIX_KERNEL_INPUT_QUEUE_EVENTS
#define RADIX_KERNEL_INPUT_QUEUE_EVENTS 64u
#endif

#ifndef RADIX_KERNEL_VFS_FILE_BYTES
#define RADIX_KERNEL_VFS_FILE_BYTES 4096u
#endif

#ifndef RADIX_KERNEL_TTY_BUFFER_BYTES
#define RADIX_KERNEL_TTY_BUFFER_BYTES 1024u
#endif

#ifndef RADIX_KERNEL_TTY_LINE_BYTES
#define RADIX_KERNEL_TTY_LINE_BYTES 256u
#endif

#ifndef RADIX_KERNEL_MAX_CORES
#define RADIX_KERNEL_MAX_CORES 4u
#endif

#ifndef RADIX_KERNEL_ARCH_CONTEXT_WORDS
#define RADIX_KERNEL_ARCH_CONTEXT_WORDS 8u
#endif

#ifndef RADIX_KERNEL_TASK_STACK_BYTES
#define RADIX_KERNEL_TASK_STACK_BYTES 8192u
#endif

#ifndef RADIX_KERNEL_TASK_DYNAMIC_STACKS
#define RADIX_KERNEL_TASK_DYNAMIC_STACKS 0
#endif

extern "C" uint64_t rad_hal_time_micros(void) __attribute__((weak));
extern "C" void rad_hal_sleep_us(uint32_t microseconds) __attribute__((weak));
extern "C" rad_status_t rad_hal_register_default_devices(void) __attribute__((weak));
extern "C" rad_status_t rad_hal_mount_sd(const rad_sd_config_t *config) __attribute__((weak));
extern "C" uint32_t rad_hal_core_count(void) __attribute__((weak));
extern "C" uint32_t rad_hal_current_core(void) __attribute__((weak));
extern "C" rad_status_t rad_hal_start_worker_core(uint32_t core, void (*entry)(uint32_t core)) __attribute__((weak));
extern "C" void rad_hal_worker_wait(void) __attribute__((weak));
extern "C" void rad_hal_worker_wake(void) __attribute__((weak));
extern "C" rad_status_t rad_hal_irq_enable(uint32_t irq) __attribute__((weak));
extern "C" rad_status_t rad_hal_irq_disable(uint32_t irq) __attribute__((weak));
extern "C" void rad_hal_console_write(const char *text) __attribute__((weak));
extern "C" void rad_hal_early_console_write(const char *text) __attribute__((weak));
extern "C" rad_status_t rad_hal_interrupts_enable(void) __attribute__((weak));
extern "C" rad_status_t rad_hal_interrupts_disable(void) __attribute__((weak));
extern "C" int rad_hal_interrupts_enabled(void) __attribute__((weak));
extern "C" void rad_hal_cpu_idle(void) __attribute__((weak));
extern "C" void rad_hal_cpu_halt_forever(void) __attribute__((weak));
extern "C" int rad_arch_preemption_supported(void) __attribute__((weak));
extern "C" const char *rad_arch_scheduler_name(void) __attribute__((weak));
extern "C" void rad_arch_scheduler_tick(uint32_t core) __attribute__((weak));
extern "C" int rad_arch_task_context_supported(void) __attribute__((weak));
extern "C" void rad_arch_task_context_init(uintptr_t *context, void *stack_top, void (*entry)(void)) __attribute__((weak));
extern "C" void rad_arch_task_context_switch(uintptr_t *old_context, uintptr_t *new_context) __attribute__((weak));
extern "C" void rad_arch_task_context_resumed(void *user_context) __attribute__((weak));
extern "C" void rad_overlay_reset(void);
extern "C" void rad_overlay_register_terminal_commands(void);

struct rad_task_handle {
    uint64_t id;
    char name[64];
    rad_task_entry_t entry;
    void *context;
    int running;
    int finished;
    int detached;
    rad_task_state_t state;
    int target_core;
    int current_core;
    int preempt_pinned;
    int preempt_saved_target_core;
    int priority;
    size_t stack_size;
    void *user_context;
    uint64_t wake_micros;
    int arch_context_ready;
    int32_t process_pid;
    uintptr_t arch_context[RADIX_KERNEL_ARCH_CONTEXT_WORDS];
#if RADIX_KERNEL_TASK_DYNAMIC_STACKS
    uint8_t *arch_stack;
    uint8_t *arch_stack_allocation;
    size_t arch_stack_bytes;
#else
    alignas(16) uint8_t arch_stack[RADIX_KERNEL_TASK_STACK_BYTES];
#endif
};

constexpr uint8_t TaskStackCanary = 0xa5u;
constexpr size_t TaskStackGuardBytes = RADIX_KERNEL_TASK_STACK_BYTES < 256u ? RADIX_KERNEL_TASK_STACK_BYTES : 256u;

struct rad_mutex_handle {
    volatile int locked;
};

struct rad_event_handle {
    volatile int signaled;
};

struct rad_wait_queue_handle {
    size_t index;
};

struct rad_input_queue_handle {
    size_t index;
};

struct rad_file_handle {
    size_t entry_index;
    size_t position;
    uint32_t flags;
    void *backend_file;
    size_t provider_index;
    int backend;
};

struct rad_dir_handle {
    char path[96];
    size_t cursor;
    size_t count;
    rad_vfs_dirent_t entries[RADIX_KERNEL_MAX_DIR_ENTRIES];
};

struct rad_program_handle {
    size_t index;
};

struct rad_tty_handle {
    size_t index;
};

struct rad_device_record {
    char name[64];
    rad_device_type_t type;
    rad_device_ops_t ops;
    int used;
};

struct rad_device_handle {
    rad_device_record record;
};

struct rad_pty_handle {
    size_t index;
};

namespace {
struct CommandRecord {
    char name[32];
    char description[96];
    rad_terminal_handler_t handler;
    void *context;
    int used;
};

struct MountRecord {
    char mount_point[32];
    rad_sd_config_t sd;
    size_t provider_index;
    int has_provider;
    int used;
};

struct VfsProviderRecord {
    char mount_point[32];
    rad_vfs_backend_ops_t ops;
    volatile int io_lock;
    int used;
};

struct VfsFileRecord {
    char path[96];
    uint8_t data[RADIX_KERNEL_VFS_FILE_BYTES];
    size_t size;
    int is_directory;
    uint32_t mode;
    rad_uid_t uid;
    rad_gid_t gid;
    int used;
};

struct ProgramRecord {
    uint64_t id;
    char path[128];
    char name[64];
    rad_program_state_t state;
    rad_task_t task;
    int exit_code;
    uint8_t *image;
    size_t image_size;
    uintptr_t entry;
    int used;
};

struct ProgramLaunchContext {
    ProgramRecord *program;
    int argc;
    char args[8][64];
};

struct TtyRecord {
    char name[64];
    char input_buffer[RADIX_KERNEL_TTY_BUFFER_BYTES];
    size_t input_size;
    char line_buffer[RADIX_KERNEL_TTY_LINE_BYTES];
    size_t line_size;
    char output_buffer[RADIX_KERNEL_TTY_BUFFER_BYTES];
    size_t output_size;
    char attached_device_name[64];
    uint32_t mode;
    rad_tty_termios_t termios;
    rad_tty_window_size_t window;
    rad_tty_output_t output;
    void *output_context;
    uint32_t pty_id;
    int used;
};

struct PtyRecord {
    uint32_t id;
    char name[32];
    char slave_name[64];
    char master_buffer[RADIX_KERNEL_TTY_BUFFER_BYTES];
    size_t master_size;
    rad_tty_window_size_t window;
    int used;
};

struct PipeRecord {
    uint8_t buffer[RADIX_KERNEL_PIPE_BUFFER_BYTES];
    size_t read_pos;
    size_t size;
    int read_refs;
    int write_refs;
    int used;
};

enum FdKind {
    FD_EMPTY = 0,
    FD_FILE = 1,
    FD_DEVICE = 2,
    FD_PIPE = 3,
    FD_SHM = 4,
    FD_SOCKET = 5,
    FD_DIR = 6
};

struct UdpDatagram {
    rad_sockaddr_in_t from;
    size_t size;
    uint8_t payload[RADIX_KERNEL_UDP_PAYLOAD_BYTES];
    int used;
};

struct SocketRecord {
    int used;
    int domain;
    int type;
    int protocol;
    rad_tcp_state_t tcp_state;
    uint16_t local_port;
    uint16_t remote_port;
    rad_ipv4_address_t local_address;
    rad_ipv4_address_t remote_address;
    UdpDatagram datagrams[RADIX_KERNEL_MAX_UDP_DATAGRAMS];
    uint8_t stream_rx[RADIX_KERNEL_PIPE_BUFFER_BYTES];
    size_t stream_rx_size;
    size_t pending_accepts[RADIX_KERNEL_MAX_UDP_SOCKETS];
    size_t pending_count;
    size_t peer_index;
    int shutdown_read;
    int shutdown_write;
};

struct ArpEntry {
    rad_ipv4_address_t ipv4;
    rad_mac_address_t mac;
    int valid;
};

struct FdRecord {
    FdKind kind;
    rad_file_t file;
    rad_dir_t dir;
    rad_device_t device;
    size_t pipe_index;
    size_t shm_index;
    size_t socket_index;
    int pipe_write_end;
    uint32_t flags;
    uint32_t descriptor_flags;
    int refs;
    int32_t owner_fd;
    int32_t owner_pid;
    int32_t local_fd;
    int used;
};

struct ShmRecord {
    char name[RAD_SHM_NAME_MAX];
    size_t byte_size;
    size_t page_count;
    uintptr_t pages[RAD_SHM_MAX_PAGES];
    int linked;
    int used;
};

struct ProcessRecord {
    int32_t pid;
    int32_t parent_pid;
    rad_process_state_t state;
    int32_t exit_code;
    rad_credentials_t credentials;
    char path[128];
    rad_task_t task;
    int used;
};

struct ModuleRecord {
    rad_module_descriptor_t descriptor;
    char name[RAD_MODULE_NAME_MAX];
    char bus[RAD_DRIVER_NAME_MAX];
    char compatible[RAD_COMPATIBLE_MAX];
    rad_module_state_t state;
    rad_status_t last_status;
    int used;
};

struct LogRecord {
    rad_log_entry_t entry;
    int used;
};

struct IrqRecord {
    uint32_t irq;
    char name[RAD_IRQ_NAME_MAX];
    rad_irq_handler_t handler;
    void *context;
    int enabled;
    uint64_t count;
    uint64_t unhandled_count;
    int used;
};

struct PerfCounterRecord {
    char name[RAD_PERF_NAME_MAX];
    uint64_t value;
    int used;
};

struct WorkItem {
    char name[RAD_PERF_NAME_MAX];
    rad_work_handler_t handler;
    void *context;
    int used;
};

struct WaitQueueRecord {
    volatile uint64_t signals;
    int used;
};

struct TimerSourceRecord {
    char name[RAD_TIMER_NAME_MAX];
    rad_timer_source_ops_t ops;
    uint32_t frequency_hz;
    int supports_oneshot;
    int active;
    uint64_t ticks;
    int used;
};

struct InputQueueRecord {
    char name[RAD_INPUT_QUEUE_NAME_MAX];
    rad_input_event_t events[RADIX_KERNEL_INPUT_QUEUE_EVENTS];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t capacity;
    int used;
};

struct KernelState {
    int initialized;
    int shutdown_requested;
    char backend[32];
    uint64_t next_task_id;
    uint64_t start_micros;
    int64_t realtime_offset_micros;
    rad_task_handle tasks[RADIX_KERNEL_MAX_TASKS];
    rad_device_record devices[RADIX_KERNEL_MAX_DEVICES];
    CommandRecord commands[RADIX_KERNEL_MAX_COMMANDS];
    MountRecord mounts[RADIX_KERNEL_MAX_MOUNTS];
    VfsProviderRecord providers[RADIX_KERNEL_MAX_VFS_PROVIDERS];
    VfsFileRecord files[RADIX_KERNEL_MAX_VFS_FILES];
    ProgramRecord programs[RADIX_KERNEL_MAX_PROGRAMS];
    TtyRecord ttys[RADIX_KERNEL_MAX_TTYS];
    PtyRecord ptys[RADIX_KERNEL_MAX_PTYS];
    PipeRecord pipes[RADIX_KERNEL_MAX_PIPES];
    ShmRecord shm_objects[RADIX_KERNEL_MAX_SHM_OBJECTS];
    SocketRecord sockets[RADIX_KERNEL_MAX_UDP_SOCKETS];
    ArpEntry arp_cache[8];
    rad_net_stack_config_t net_config;
    rad_ntp_status_t ntp_status;
    uint64_t net_ethernet_rx;
    uint64_t net_ethernet_tx;
    uint64_t net_arp_rx;
    uint64_t net_arp_tx;
    uint64_t net_ipv4_rx;
    uint64_t net_ipv4_tx;
    uint64_t net_udp_rx;
    uint64_t net_udp_tx;
    uint64_t net_icmp_rx;
    uint64_t net_icmp_tx;
    FdRecord fds[RADIX_KERNEL_MAX_POSIX_FDS];
    ProcessRecord processes[RADIX_KERNEL_MAX_PROCESSES];
    ModuleRecord modules[RADIX_KERNEL_MAX_MODULES];
    LogRecord logs[RADIX_KERNEL_MAX_LOG_ENTRIES];
    uint64_t next_log_sequence;
    IrqRecord irqs[RADIX_KERNEL_MAX_IRQS];
    PerfCounterRecord perf_counters[RADIX_KERNEL_MAX_PERF_COUNTERS];
    WorkItem work_items[RADIX_KERNEL_MAX_WORK_ITEMS];
    uint32_t work_head;
    uint32_t work_tail;
    uint32_t work_count;
    WaitQueueRecord wait_queues[RADIX_KERNEL_MAX_WAIT_QUEUES];
    rad_wait_queue_handle wait_queue_handles[RADIX_KERNEL_MAX_WAIT_QUEUES];
    TimerSourceRecord timers[RADIX_KERNEL_MAX_TIMER_SOURCES];
    uint64_t timer_elapsed_micros;
    InputQueueRecord input_queues[RADIX_KERNEL_MAX_INPUT_QUEUES];
    rad_input_queue_handle input_queue_handles[RADIX_KERNEL_MAX_INPUT_QUEUES];
    rad_memory_stats_t memory;
    rad_boot_info_t boot;
    int has_boot;
    volatile int task_lock;
    volatile int runtime_lock;
    uint64_t next_program_id;
    uint32_t detected_cores;
    uint32_t worker_running_mask;
    volatile int preemption_enabled;
    uint64_t context_switches;
    uint64_t scheduler_ticks;
    char cwd[96];
    char attached_terminal_device[64];
    char terminal_line[256];
    size_t terminal_line_size;
    int32_t current_pid;
    int32_t next_pid;
    rad_process_arch_ops_t process_arch_ops;
    int has_process_arch_ops;
};

KernelState g_state{};
uint64_t g_current_task_id[RADIX_KERNEL_MAX_CORES]{};
int32_t g_current_pid[RADIX_KERNEL_MAX_CORES]{};
uintptr_t g_scheduler_context[RADIX_KERNEL_MAX_CORES][RADIX_KERNEL_ARCH_CONTEXT_WORDS]{};
volatile int g_scheduler_in_irq[RADIX_KERNEL_MAX_CORES]{};
uint32_t g_last_task_index[RADIX_KERNEL_MAX_CORES]{};
volatile int g_ap_worker_task_seen = 0;
volatile int g_context_dispatch_seen = 0;
volatile int g_preempt_sched_seen = 0;
volatile int g_ap_preempt_sched_seen = 0;

uint64_t hal_now() {
    return rad_hal_time_micros ? rad_hal_time_micros() : 0;
}

void hal_sleep(uint32_t micros) {
    if (rad_hal_sleep_us) rad_hal_sleep_us(micros);
}

uint32_t hal_core_count() {
    uint32_t count = rad_hal_core_count ? rad_hal_core_count() : 1u;
    if (count == 0) count = 1;
    if (count > RADIX_KERNEL_MAX_CORES) count = RADIX_KERNEL_MAX_CORES;
    return count;
}

uint32_t hal_current_core() {
    uint32_t core = rad_hal_current_core ? rad_hal_current_core() : RAD_TASK_CORE_SERVICE;
    return core < RADIX_KERNEL_MAX_CORES ? core : RAD_TASK_CORE_SERVICE;
}

int32_t current_process_pid() {
    const uint32_t core = hal_current_core();
    int32_t pid = g_current_pid[core];
    if (pid >= 0) return pid;
    return g_state.current_pid >= 0 ? g_state.current_pid : 0;
}

bool arch_context_enabled() {
    return rad_arch_task_context_supported
        && rad_arch_task_context_supported()
        && rad_arch_task_context_init
        && rad_arch_task_context_switch;
}

void lock_tasks();
void unlock_tasks();

const char *scheduler_arch_name() {
    if (rad_arch_scheduler_name) {
        const char *name = rad_arch_scheduler_name();
        if (name && *name) return name;
    }
    return "portable-cooperative";
}

int scheduler_preemption_supported() {
    return rad_arch_preemption_supported ? rad_arch_preemption_supported() : 0;
}

void scheduler_count_threads(rad_scheduler_info_t *info) {
    if (!info) return;
    lock_tasks();
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TASKS; ++i) {
        const rad_task_handle& task = g_state.tasks[i];
        if (!task.entry) continue;
        switch (task.state) {
        case RAD_TASK_READY:
        case RAD_TASK_NEW:
            ++info->ready_threads;
            break;
        case RAD_TASK_RUNNING:
            ++info->running_threads;
            break;
        case RAD_TASK_BLOCKED:
            ++info->blocked_threads;
            break;
        case RAD_TASK_SLEEPING:
            ++info->sleeping_threads;
            break;
        case RAD_TASK_FINISHED:
        case RAD_TASK_DETACHED:
            ++info->exited_threads;
            break;
        default:
            break;
        }
    }
    unlock_tasks();
}

void lock_tasks() {
    while (__atomic_test_and_set(&g_state.task_lock, __ATOMIC_ACQUIRE)) {
        hal_sleep(0);
    }
}

void unlock_tasks() {
    __atomic_clear(&g_state.task_lock, __ATOMIC_RELEASE);
}

void lock_runtime() {
    while (__atomic_test_and_set(&g_state.runtime_lock, __ATOMIC_ACQUIRE)) {
        hal_sleep(0);
    }
}

void unlock_runtime() {
    __atomic_clear(&g_state.runtime_lock, __ATOMIC_RELEASE);
}

void lock_provider(VfsProviderRecord *provider) {
    if (!provider) return;
    while (__atomic_test_and_set(&provider->io_lock, __ATOMIC_ACQUIRE)) {
        hal_sleep(0);
    }
}

void unlock_provider(VfsProviderRecord *provider) {
    if (!provider) return;
    __atomic_clear(&provider->io_lock, __ATOMIC_RELEASE);
}

void copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

int text_contains(const char *text, const char *needle) {
    if (!text || !needle) return 0;
    return strstr(text, needle) != nullptr;
}

ModuleRecord *find_module(const char *name) {
    if (!name) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_MODULES; ++i) {
        if (g_state.modules[i].used && strcmp(g_state.modules[i].name, name) == 0) return &g_state.modules[i];
    }
    return nullptr;
}

const char *service_state_name(rad_service_state_t state) {
    switch (state) {
    case RAD_SERVICE_REGISTERED: return "registered";
    case RAD_SERVICE_CONFIGURED: return "configured";
    case RAD_SERVICE_RUNNING: return "running";
    case RAD_SERVICE_FAILED: return "failed";
    case RAD_SERVICE_STOPPED: return "stopped";
    default: return "unknown";
    }
}

const char *rad_log_level_name(rad_log_level_t level) {
    switch (level) {
    case RAD_LOG_TRACE: return "TRACE";
    case RAD_LOG_DEBUG: return "DEBUG";
    case RAD_LOG_INFO: return "INFO";
    case RAD_LOG_WARNING: return "WARNING";
    case RAD_LOG_ERROR: return "ERROR";
    case RAD_LOG_CRITICAL: return "CRITICAL";
    default: return "INFO";
    }
}

void exit_modules_reverse(void) {
    for (size_t i = RADIX_KERNEL_MAX_MODULES; i > 0; --i) {
        ModuleRecord& module = g_state.modules[i - 1];
        if (!module.used || module.state != RAD_MODULE_INITIALIZED) continue;
        if (module.descriptor.exit) module.descriptor.exit(module.descriptor.context);
        module.state = RAD_MODULE_EXITED;
    }
}

void normalize_path(char *dst, size_t dst_size, const char *path) {
    if (!dst || dst_size == 0) return;
    if (!path || !*path) path = g_state.cwd[0] ? g_state.cwd : "/";

    char joined[160];
    if (path[0] == '/') {
        copy_string(joined, sizeof(joined), path);
    } else {
        size_t pos = 0;
        const char *base = g_state.cwd[0] ? g_state.cwd : "/";
        for (const char *c = base; *c && pos + 1 < sizeof(joined); ++c) joined[pos++] = *c;
        if (pos > 1 && joined[pos - 1] != '/' && pos + 1 < sizeof(joined)) joined[pos++] = '/';
        for (const char *c = path; *c && pos + 1 < sizeof(joined); ++c) joined[pos++] = *c;
        joined[pos] = '\0';
    }

    char scratch[160];
    copy_string(scratch, sizeof(scratch), joined);
    const char *parts[16]{};
    size_t part_count = 0;
    char *cursor = scratch;
    while (*cursor && part_count < 16) {
        while (*cursor == '/') ++cursor;
        if (!*cursor) break;
        char *start = cursor;
        while (*cursor && *cursor != '/') ++cursor;
        if (*cursor) *cursor++ = '\0';
        if (strcmp(start, ".") == 0) continue;
        if (strcmp(start, "..") == 0) {
            if (part_count) --part_count;
            continue;
        }
        parts[part_count++] = start;
    }

    size_t pos = 0;
    dst[pos++] = '/';
    for (size_t i = 0; i < part_count && pos + 1 < dst_size; ++i) {
        if (i && pos + 1 < dst_size) dst[pos++] = '/';
        for (const char *c = parts[i]; *c && pos + 1 < dst_size; ++c) dst[pos++] = *c;
    }
    dst[pos] = '\0';
}

int is_child_name(const char *parent, const char *path, const char **name_out) {
    if (!parent || !path) return 0;
    if (strcmp(parent, "/") == 0) {
        if (path[0] != '/' || path[1] == '\0') return 0;
        const char *name = path + 1;
        if (strchr(name, '/')) return 0;
        if (name_out) *name_out = name;
        return 1;
    }
    const size_t prefix_len = strlen(parent);
    if (strncmp(path, parent, prefix_len) != 0 || path[prefix_len] != '/') return 0;
    const char *name = path + prefix_len + 1;
    if (!*name || strchr(name, '/')) return 0;
    if (name_out) *name_out = name;
    return 1;
}

void parent_path(char *dst, size_t dst_size, const char *path) {
    copy_string(dst, dst_size, path);
    char *slash = strrchr(dst, '/');
    if (!slash || slash == dst) copy_string(dst, dst_size, "/");
    else *slash = '\0';
}

VfsFileRecord *find_file_or_dir(const char *path) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_VFS_FILES; ++i) {
        if (g_state.files[i].used && strcmp(g_state.files[i].path, path) == 0) return &g_state.files[i];
    }
    return nullptr;
}

int has_children(const char *path) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_VFS_FILES; ++i) {
        if (g_state.files[i].used && is_child_name(path, g_state.files[i].path, nullptr)) return 1;
    }
    return 0;
}

void fill_stat(const VfsFileRecord& record, rad_vfs_stat_t *stat) {
    if (!stat) return;
    stat->is_directory = record.is_directory;
    stat->size = record.is_directory ? 0 : record.size;
    stat->mode = record.mode ? record.mode : (record.is_directory ? 0040755u : 0100644u);
    stat->uid = record.uid;
    stat->gid = record.gid;
    stat->mtime_millis = 0;
}

ProcessRecord *find_process_record(int32_t pid) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PROCESSES; ++i) {
        if (g_state.processes[i].used && g_state.processes[i].pid == pid) return &g_state.processes[i];
    }
    return nullptr;
}

rad_credentials_t current_credentials() {
    const ProcessRecord *process = find_process_record(current_process_pid());
    if (process) return process->credentials;
    return rad_credentials_t{0, 0, 0, 0};
}

int mode_allows(const rad_vfs_stat_t& stat, uint32_t want) {
    const rad_credentials_t credentials = current_credentials();
    if (credentials.euid == 0) return 1;
    const uint32_t mode = stat.mode & 0777u;
    uint32_t shift = 0;
    if (credentials.euid == stat.uid) shift = 6;
    else if (credentials.egid == stat.gid) shift = 3;
    else shift = 0;
    return (((mode >> shift) & want) == want);
}

int can_read_stat(const rad_vfs_stat_t& stat) {
    return mode_allows(stat, 4u);
}

int can_write_stat(const rad_vfs_stat_t& stat) {
    return mode_allows(stat, 2u);
}

int can_execute_stat(const rad_vfs_stat_t& stat) {
    return mode_allows(stat, 1u);
}

VfsFileRecord *ensure_directory(const char *path) {
    VfsFileRecord *existing = find_file_or_dir(path);
    if (existing) return existing->is_directory ? existing : nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_VFS_FILES; ++i) {
        if (g_state.files[i].used) continue;
        g_state.files[i].used = 1;
        g_state.files[i].is_directory = 1;
        g_state.files[i].size = 0;
        g_state.files[i].mode = 0040755u;
        const rad_credentials_t credentials = current_credentials();
        g_state.files[i].uid = credentials.euid;
        g_state.files[i].gid = credentials.egid;
        copy_string(g_state.files[i].path, sizeof(g_state.files[i].path), path);
        return &g_state.files[i];
    }
    return nullptr;
}

int mounted_path(const char *path) {
    if (!path) return 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_MOUNTS; ++i) {
        if (!g_state.mounts[i].used) continue;
        const char *mount = g_state.mounts[i].mount_point;
        const size_t len = strlen(mount);
        if (strcmp(mount, "/") == 0) return 1;
        if (strncmp(path, mount, len) == 0 && (path[len] == '\0' || path[len] == '/')) return 1;
    }
    return 0;
}

VfsProviderRecord *provider_for_path(const char *path, const char **relative_out) {
    if (!path) return nullptr;
    size_t best = 0;
    VfsProviderRecord *provider = nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_VFS_PROVIDERS; ++i) {
        if (!g_state.providers[i].used) continue;
        const char *mount = g_state.providers[i].mount_point;
        const size_t len = strlen(mount);
        if (len < best) continue;
        if (strcmp(mount, "/") == 0) {
            best = len;
            provider = &g_state.providers[i];
            continue;
        }
        if (strncmp(path, mount, len) == 0 && (path[len] == '\0' || path[len] == '/')) {
            best = len;
            provider = &g_state.providers[i];
        }
    }
    if (provider && relative_out) {
        const char *rest = path + best;
        if (strcmp(provider->mount_point, "/") == 0) {
            *relative_out = path[0] == '/' ? path + 1 : path;
        } else {
            *relative_out = (*rest == '/') ? rest + 1 : "";
        }
    }
    return provider;
}

size_t provider_index(const VfsProviderRecord *provider) {
    return provider ? static_cast<size_t>(provider - g_state.providers) : RADIX_KERNEL_MAX_VFS_PROVIDERS;
}

VfsFileRecord *find_file(const char *path) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_VFS_FILES; ++i) {
        if (g_state.files[i].used && !g_state.files[i].is_directory && strcmp(g_state.files[i].path, path) == 0) return &g_state.files[i];
    }
    return nullptr;
}

VfsFileRecord *allocate_file(const char *path) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_VFS_FILES; ++i) {
        if (g_state.files[i].used) continue;
        g_state.files[i].used = 1;
        g_state.files[i].is_directory = 0;
        g_state.files[i].size = 0;
        g_state.files[i].mode = 0100644u;
        const rad_credentials_t credentials = current_credentials();
        g_state.files[i].uid = credentials.euid;
        g_state.files[i].gid = credentials.egid;
        copy_string(g_state.files[i].path, sizeof(g_state.files[i].path), path);
        return &g_state.files[i];
    }
    return nullptr;
}

void write_line(rad_terminal_write_t write, void *context, const char *text) {
    if (!write) return;
    write(text ? text : "", context);
    write("\n", context);
}

void buffer_append(char *dst, size_t capacity, size_t *size, const void *data, size_t count) {
    if (!dst || !size || !data || capacity == 0) return;
    size_t available = *size < capacity ? capacity - *size : 0;
    if (count > available) count = available;
    if (count) {
        memcpy(dst + *size, data, count);
        *size += count;
    }
}

size_t buffer_take(char *dst, size_t dst_size, char *src, size_t *src_size) {
    if (!src || !src_size) return 0;
    size_t count = *src_size;
    if (count > dst_size) count = dst_size;
    if (dst && count) memcpy(dst, src, count);
    if (count < *src_size) memmove(src, src + count, *src_size - count);
    *src_size -= count;
    return count;
}

constexpr uint32_t TtyIflagIcrnl = 0x00000004u;
constexpr uint32_t TtyOflagOpost = 0x00000001u;
constexpr uint32_t TtyLflagIcanon = 0x00000001u;
constexpr uint32_t TtyLflagEcho = 0x00000002u;
constexpr uint32_t TtyLflagIsig = 0x00000008u;
constexpr uint32_t TtyLflagIexten = 0x00000010u;
constexpr uint32_t TtySpeedDefault = 9600u;
constexpr size_t TtyCcVintr = 0u;
constexpr size_t TtyCcVeof = 1u;
constexpr size_t TtyCcVerase = 2u;
constexpr size_t TtyCcVmin = 3u;
constexpr size_t TtyCcVtime = 4u;
constexpr size_t TtyCcVkill = 5u;

rad_tty_termios_t default_tty_termios(void) {
    rad_tty_termios_t termios{};
    termios.input_flags = TtyIflagIcrnl;
    termios.output_flags = TtyOflagOpost;
    termios.local_flags = TtyLflagIcanon | TtyLflagEcho | TtyLflagIsig | TtyLflagIexten;
    termios.input_speed = TtySpeedDefault;
    termios.output_speed = TtySpeedDefault;
    termios.control_chars[TtyCcVintr] = 3;
    termios.control_chars[TtyCcVeof] = 4;
    termios.control_chars[TtyCcVerase] = 0x7f;
    termios.control_chars[TtyCcVmin] = 1;
    termios.control_chars[TtyCcVtime] = 0;
    termios.control_chars[TtyCcVkill] = 21;
    return termios;
}

uint32_t tty_mode_from_termios(const rad_tty_termios_t& termios) {
    uint32_t mode = 0;
    if (termios.local_flags & TtyLflagIcanon) mode |= RAD_TTY_MODE_CANONICAL;
    if (termios.local_flags & TtyLflagEcho) mode |= RAD_TTY_MODE_ECHO;
    if (termios.input_flags & TtyIflagIcrnl) mode |= RAD_TTY_MODE_CRLF;
    return mode;
}

void tty_termios_from_mode(rad_tty_termios_t& termios, uint32_t mode) {
    if (mode & RAD_TTY_MODE_CANONICAL) termios.local_flags |= TtyLflagIcanon;
    else termios.local_flags &= ~TtyLflagIcanon;
    if (mode & RAD_TTY_MODE_ECHO) termios.local_flags |= TtyLflagEcho;
    else termios.local_flags &= ~TtyLflagEcho;
    if (mode & RAD_TTY_MODE_CRLF) termios.input_flags |= TtyIflagIcrnl;
    else termios.input_flags &= ~TtyIflagIcrnl;
}

TtyRecord *find_tty(const char *name, size_t *index_out) {
    if (!name) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TTYS; ++i) {
        if (g_state.ttys[i].used && strcmp(g_state.ttys[i].name, name) == 0) {
            if (index_out) *index_out = i;
            return &g_state.ttys[i];
        }
    }
    return nullptr;
}

TtyRecord *ensure_tty(const char *name, size_t *index_out) {
    TtyRecord *existing = find_tty(name, index_out);
    if (existing) return existing;
    if (!name || !*name) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TTYS; ++i) {
        if (g_state.ttys[i].used) continue;
        TtyRecord& tty = g_state.ttys[i];
        memset(&tty, 0, sizeof(tty));
        tty.used = 1;
        copy_string(tty.name, sizeof(tty.name), name);
        tty.termios = default_tty_termios();
        tty.mode = tty_mode_from_termios(tty.termios);
        tty.window.rows = 30;
        tty.window.columns = 120;
        tty.window.x_pixels = 960;
        tty.window.y_pixels = 480;
        if (index_out) *index_out = i;
        return &tty;
    }
    return nullptr;
}

rad_status_t tty_flush_record(TtyRecord *tty, uint32_t queues) {
    if (!tty) return RAD_STATUS_INVALID_ARGUMENT;
    if (queues == 0) queues = RAD_TTY_FLUSH_INPUT | RAD_TTY_FLUSH_OUTPUT;
    if (queues & RAD_TTY_FLUSH_INPUT) {
        tty->input_size = 0;
        tty->line_size = 0;
    }
    if (queues & RAD_TTY_FLUSH_OUTPUT) {
        tty->output_size = 0;
        if (tty->pty_id) {
            for (size_t i = 0; i < RADIX_KERNEL_MAX_PTYS; ++i) {
                if (g_state.ptys[i].used && g_state.ptys[i].id == tty->pty_id) {
                    g_state.ptys[i].master_size = 0;
                    break;
                }
            }
        }
    }
    return RAD_STATUS_OK;
}

PtyRecord *find_pty(size_t index) {
    if (index >= RADIX_KERNEL_MAX_PTYS || !g_state.ptys[index].used) return nullptr;
    return &g_state.ptys[index];
}

void append_tty_output(TtyRecord *tty, const void *data, size_t size) {
    if (!tty || (!data && size)) return;
    buffer_append(tty->output_buffer, sizeof(tty->output_buffer), &tty->output_size, data, size);
    if (tty->pty_id) {
        for (size_t i = 0; i < RADIX_KERNEL_MAX_PTYS; ++i) {
            if (g_state.ptys[i].used && g_state.ptys[i].id == tty->pty_id) {
                buffer_append(g_state.ptys[i].master_buffer, sizeof(g_state.ptys[i].master_buffer),
                    &g_state.ptys[i].master_size, data, size);
                break;
            }
        }
    }
}

rad_status_t tty_read_record(TtyRecord *tty, void *buffer, size_t size, size_t *bytes_read) {
    if (!tty || (!buffer && size)) return RAD_STATUS_INVALID_ARGUMENT;
    size_t count = buffer_take(static_cast<char*>(buffer), size, tty->input_buffer, &tty->input_size);
    if (bytes_read) *bytes_read = count;
    return RAD_STATUS_OK;
}

bool tty_record_input_ready(const TtyRecord *tty) {
    return tty && tty->input_size > 0;
}

TtyRecord *tty_record_from_device(rad_device_t device) {
    if (!device || device->record.type != RAD_DEVICE_TTY) return nullptr;
    return static_cast<TtyRecord*>(device->record.ops.context);
}

rad_status_t tty_write_record(TtyRecord *tty, const void *buffer, size_t size, size_t *bytes_written) {
    if (!tty || (!buffer && size)) return RAD_STATUS_INVALID_ARGUMENT;
    append_tty_output(tty, buffer, size);
    if (tty->output && size) tty->output(buffer, size, tty->output_context);
    if (bytes_written) *bytes_written = size;
    return RAD_STATUS_OK;
}

rad_status_t tty_push_input_record(TtyRecord *tty, const void *buffer, size_t size, size_t *bytes_consumed) {
    if (!tty || (!buffer && size)) return RAD_STATUS_INVALID_ARGUMENT;
    char echo[RADIX_KERNEL_TTY_LINE_BYTES]{};
    size_t echo_size = 0;
    const char *bytes = static_cast<const char*>(buffer);
    for (size_t i = 0; i < size; ++i) {
        char ch = bytes[i];
        if (ch == '\r') ch = '\n';
        if ((ch == '\b' || ch == 0x7f) && (tty->mode & RAD_TTY_MODE_CANONICAL)) {
            if (tty->line_size) {
                --tty->line_size;
                if (tty->mode & RAD_TTY_MODE_ECHO) buffer_append(echo, sizeof(echo), &echo_size, "\b \b", 3);
            }
            continue;
        }
        if (tty->mode & RAD_TTY_MODE_CANONICAL) {
            if (ch == '\n') {
                buffer_append(tty->input_buffer, sizeof(tty->input_buffer), &tty->input_size, tty->line_buffer, tty->line_size);
                buffer_append(tty->input_buffer, sizeof(tty->input_buffer), &tty->input_size, "\n", 1);
                tty->line_size = 0;
                if (tty->mode & RAD_TTY_MODE_ECHO) buffer_append(echo, sizeof(echo), &echo_size, "\n", 1);
                continue;
            }
            if (isprint(static_cast<unsigned char>(ch)) || ch == '\t') {
                buffer_append(tty->line_buffer, sizeof(tty->line_buffer), &tty->line_size, &ch, 1);
                if (tty->mode & RAD_TTY_MODE_ECHO) buffer_append(echo, sizeof(echo), &echo_size, &ch, 1);
            }
        } else {
            buffer_append(tty->input_buffer, sizeof(tty->input_buffer), &tty->input_size, &ch, 1);
            if (tty->mode & RAD_TTY_MODE_ECHO) buffer_append(echo, sizeof(echo), &echo_size, &ch, 1);
        }
    }
    if (echo_size) append_tty_output(tty, echo, echo_size);
    if (tty->output && echo_size) tty->output(echo, echo_size, tty->output_context);
    if (bytes_consumed) *bytes_consumed = size;
    return RAD_STATUS_OK;
}

rad_status_t tty_device_read(void *context, void *buffer, size_t size, size_t *bytes_read) {
    return tty_read_record(static_cast<TtyRecord*>(context), buffer, size, bytes_read);
}

rad_status_t tty_device_write(void *context, const void *buffer, size_t size, size_t *bytes_written) {
    return tty_write_record(static_cast<TtyRecord*>(context), buffer, size, bytes_written);
}

rad_status_t tty_device_ioctl(void *context, uint32_t request, void *argument) {
    auto *tty = static_cast<TtyRecord*>(context);
    if (!tty) return RAD_STATUS_INVALID_ARGUMENT;
    switch (request) {
    case RAD_DEVICE_IOCTL_TTY_GET_WINSIZE:
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        *static_cast<rad_tty_window_size_t*>(argument) = tty->window;
        return RAD_STATUS_OK;
    case RAD_DEVICE_IOCTL_TTY_SET_WINSIZE:
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        tty->window = *static_cast<const rad_tty_window_size_t*>(argument);
        return tty->window.rows && tty->window.columns ? RAD_STATUS_OK : RAD_STATUS_INVALID_ARGUMENT;
    case RAD_DEVICE_IOCTL_TTY_GET_MODE:
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        *static_cast<uint32_t*>(argument) = tty->mode;
        return RAD_STATUS_OK;
    case RAD_DEVICE_IOCTL_TTY_SET_MODE:
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        tty->mode = *static_cast<const uint32_t*>(argument);
        tty_termios_from_mode(tty->termios, tty->mode);
        tty->line_size = 0;
        return RAD_STATUS_OK;
    case RAD_DEVICE_IOCTL_TTY_GET_TERMIOS:
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        *static_cast<rad_tty_termios_t*>(argument) = tty->termios;
        return RAD_STATUS_OK;
    case RAD_DEVICE_IOCTL_TTY_SET_TERMIOS:
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        tty->termios = *static_cast<const rad_tty_termios_t*>(argument);
        if (tty->termios.input_speed == 0) tty->termios.input_speed = TtySpeedDefault;
        if (tty->termios.output_speed == 0) tty->termios.output_speed = TtySpeedDefault;
        tty->mode = tty_mode_from_termios(tty->termios);
        tty->line_size = 0;
        return RAD_STATUS_OK;
    case RAD_DEVICE_IOCTL_TTY_FLUSH:
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        return tty_flush_record(tty, *static_cast<const uint32_t*>(argument));
    default:
        return RAD_STATUS_NOT_SUPPORTED;
    }
}

rad_status_t input_queue_device_read(void *context, void *buffer, size_t size, size_t *bytes_read) {
    auto *queue = static_cast<rad_input_queue_t>(context);
    if (!queue || (!buffer && size)) return RAD_STATUS_INVALID_ARGUMENT;
    size_t count = 0;
    if (buffer && size >= sizeof(rad_input_event_t)) {
        (void)rad_input_queue_read(queue, static_cast<rad_input_event_t*>(buffer), size / sizeof(rad_input_event_t), 0, &count);
    }
    if (bytes_read) *bytes_read = count * sizeof(rad_input_event_t);
    return RAD_STATUS_OK;
}

void serial_tty_output(const void *data, size_t size, void *context) {
    const char *device_name = static_cast<const char*>(context);
    if (!device_name || (!data && size)) return;
    rad_device_t device = nullptr;
    if (rad_device_open(device_name, &device) != RAD_STATUS_OK) return;
    size_t ignored = 0;
    rad_device_write(device, data, size, &ignored);
    rad_device_close(device);
}

void register_tty_device(const char *name) {
    size_t index = 0;
    TtyRecord *tty = ensure_tty(name, &index);
    if (!tty) return;
    rad_device_ops_t ops{};
    ops.context = tty;
    ops.read = tty_device_read;
    ops.write = tty_device_write;
    ops.ioctl = tty_device_ioctl;
    rad_device_register(name, RAD_DEVICE_TTY, &ops);
}

void close_fd_record(FdRecord& fd) {
    if (!fd.used) return;
    --fd.refs;
    if (fd.refs > 0) {
        fd.local_fd = -1;
        fd.owner_pid = -1;
        return;
    }
    if (fd.refs <= 0) {
        if (fd.file) rad_vfs_close(fd.file);
        if (fd.dir) rad_vfs_closedir(fd.dir);
        if (fd.device) rad_device_close(fd.device);
        if (fd.kind == FD_PIPE && fd.pipe_index < RADIX_KERNEL_MAX_PIPES) {
            PipeRecord& pipe = g_state.pipes[fd.pipe_index];
            if (fd.pipe_write_end) {
                if (pipe.write_refs > 0) --pipe.write_refs;
            } else if (pipe.read_refs > 0) {
                --pipe.read_refs;
            }
            if (pipe.read_refs == 0 && pipe.write_refs == 0) memset(&pipe, 0, sizeof(pipe));
        }
        if (fd.kind == FD_SOCKET && fd.socket_index < RADIX_KERNEL_MAX_UDP_SOCKETS) {
            SocketRecord& socket = g_state.sockets[fd.socket_index];
            if (socket.peer_index < RADIX_KERNEL_MAX_UDP_SOCKETS && g_state.sockets[socket.peer_index].used) {
                g_state.sockets[socket.peer_index].peer_index = RADIX_KERNEL_MAX_UDP_SOCKETS;
            }
            for (size_t i = 0; i < socket.pending_count; ++i) {
                const size_t pending = socket.pending_accepts[i];
                if (pending < RADIX_KERNEL_MAX_UDP_SOCKETS) {
                    SocketRecord& pending_socket = g_state.sockets[pending];
                    if (pending_socket.peer_index < RADIX_KERNEL_MAX_UDP_SOCKETS && g_state.sockets[pending_socket.peer_index].used) {
                        g_state.sockets[pending_socket.peer_index].peer_index = RADIX_KERNEL_MAX_UDP_SOCKETS;
                    }
                    memset(&pending_socket, 0, sizeof(pending_socket));
                }
            }
            memset(&g_state.sockets[fd.socket_index], 0, sizeof(g_state.sockets[fd.socket_index]));
        }
    }
    memset(&fd, 0, sizeof(fd));
}

void close_fd_index(int32_t fd) {
    if (fd < 0 || fd >= static_cast<int32_t>(RADIX_KERNEL_MAX_POSIX_FDS) || !g_state.fds[fd].used) return;
    FdRecord& record = g_state.fds[fd];
    if (record.owner_fd != fd && record.owner_fd >= 0 && record.owner_fd < static_cast<int32_t>(RADIX_KERNEL_MAX_POSIX_FDS)) {
        FdRecord& owner = g_state.fds[record.owner_fd];
        if (owner.used && owner.refs > 0) --owner.refs;
        memset(&record, 0, sizeof(record));
        return;
    }
    if (record.refs > 1) {
        int32_t promoted = -1;
        for (int32_t i = 0; i < static_cast<int32_t>(RADIX_KERNEL_MAX_POSIX_FDS); ++i) {
            if (i == fd) continue;
            FdRecord& candidate = g_state.fds[i];
            if (candidate.used && candidate.owner_fd == fd) {
                promoted = i;
                break;
            }
        }
        if (promoted >= 0) {
            FdRecord& replacement = g_state.fds[promoted];
            replacement.owner_fd = promoted;
            replacement.refs = record.refs - 1;
            for (int32_t i = 0; i < static_cast<int32_t>(RADIX_KERNEL_MAX_POSIX_FDS); ++i) {
                if (i != promoted && g_state.fds[i].used && g_state.fds[i].owner_fd == fd) {
                    g_state.fds[i].owner_fd = promoted;
                }
            }
            memset(&record, 0, sizeof(record));
            return;
        }
    }
    close_fd_record(record);
}

int32_t find_fd_slot(int32_t local_fd, int32_t pid, int include_global) {
    if (local_fd < 0) return -1;
    int32_t global = -1;
    for (int32_t i = 0; i < static_cast<int32_t>(RADIX_KERNEL_MAX_POSIX_FDS); ++i) {
        FdRecord& record = g_state.fds[i];
        if (!record.used || record.local_fd != local_fd) continue;
        if (record.owner_pid == pid) return i;
        if (include_global && record.owner_pid == 0) global = i;
    }
    return global;
}

int32_t fd_owner_index(int32_t slot) {
    if (slot < 0 || slot >= static_cast<int32_t>(RADIX_KERNEL_MAX_POSIX_FDS) || !g_state.fds[slot].used) return -1;
    const int32_t owner = g_state.fds[slot].owner_fd;
    if (owner >= 0 && owner < static_cast<int32_t>(RADIX_KERNEL_MAX_POSIX_FDS) && g_state.fds[owner].used) return owner;
    return slot;
}

void close_process_fds(int32_t pid, int close_on_exec_only) {
    for (int32_t slot = 0; slot < static_cast<int32_t>(RADIX_KERNEL_MAX_POSIX_FDS); ++slot) {
        FdRecord& record = g_state.fds[slot];
        if (!record.used || record.owner_pid != pid) continue;
        if (close_on_exec_only && !(record.descriptor_flags & RAD_FD_CLOEXEC)) continue;
        close_fd_index(slot);
    }
}

void init_process_table(void) {
    memset(g_state.fds, 0, sizeof(g_state.fds));
    memset(g_state.pipes, 0, sizeof(g_state.pipes));
    memset(g_state.shm_objects, 0, sizeof(g_state.shm_objects));
    memset(g_state.sockets, 0, sizeof(g_state.sockets));
    memset(g_state.processes, 0, sizeof(g_state.processes));
    memset(&g_state.process_arch_ops, 0, sizeof(g_state.process_arch_ops));
    g_state.has_process_arch_ops = 0;
    g_state.current_pid = 0;
    g_state.next_pid = 2;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_CORES; ++i) g_current_pid[i] = 0;
    g_state.processes[0].used = 1;
    g_state.processes[0].pid = 0;
    g_state.processes[0].parent_pid = 0;
    g_state.processes[0].state = RAD_PROCESS_RUNNING;
    g_state.processes[0].credentials = rad_credentials_t{0, 0, 0, 0};
    copy_string(g_state.processes[0].path, sizeof(g_state.processes[0].path), "[kernel]");
}

int32_t install_fd_record(const FdRecord& source, int32_t requested) {
    const int32_t pid = source.owner_pid >= 0 ? source.owner_pid : current_process_pid();
    int32_t local_fd = requested;
    if (local_fd < 0) {
        for (int32_t candidate = 3; candidate < static_cast<int32_t>(RADIX_KERNEL_MAX_POSIX_FDS); ++candidate) {
            if (find_fd_slot(candidate, pid, false) < 0) {
                local_fd = candidate;
                break;
            }
        }
        if (local_fd < 0) return RAD_STATUS_NO_MEMORY;
    }
    if (requested >= 0) {
        if (requested >= static_cast<int32_t>(RADIX_KERNEL_MAX_POSIX_FDS)) return RAD_STATUS_NO_MEMORY;
        const int32_t existing = find_fd_slot(requested, pid, false);
        if (existing >= 0) close_fd_index(existing);
    }
    for (int32_t slot = 0; slot < static_cast<int32_t>(RADIX_KERNEL_MAX_POSIX_FDS); ++slot) {
        if (g_state.fds[slot].used) continue;
        g_state.fds[slot] = source;
        g_state.fds[slot].used = 1;
        g_state.fds[slot].owner_pid = pid;
        g_state.fds[slot].local_fd = local_fd;
        if (g_state.fds[slot].owner_fd < 0) g_state.fds[slot].owner_fd = slot;
        return local_fd;
    }
    return RAD_STATUS_NO_MEMORY;
}

FdRecord *lookup_fd_record(int32_t fd) {
    const int32_t current = current_process_pid();
    const int32_t slot = find_fd_slot(fd, current, true);
    if (slot < 0) return nullptr;
    const int32_t owner = fd_owner_index(slot);
    return owner >= 0 ? &g_state.fds[owner] : nullptr;
}

intptr_t pipe_read(FdRecord *record, void *buffer, size_t size) {
    if (!record || record->kind != FD_PIPE || record->pipe_write_end || record->pipe_index >= RADIX_KERNEL_MAX_PIPES) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    PipeRecord& pipe = g_state.pipes[record->pipe_index];
    if (!pipe.used || !buffer) return RAD_STATUS_INVALID_ARGUMENT;
    const size_t count = pipe.size < size ? pipe.size : size;
    if (count) {
        const size_t first = count < (RADIX_KERNEL_PIPE_BUFFER_BYTES - pipe.read_pos) ? count : (RADIX_KERNEL_PIPE_BUFFER_BYTES - pipe.read_pos);
        memcpy(buffer, pipe.buffer + pipe.read_pos, first);
        if (count > first) memcpy(static_cast<uint8_t*>(buffer) + first, pipe.buffer, count - first);
        pipe.read_pos = (pipe.read_pos + count) % RADIX_KERNEL_PIPE_BUFFER_BYTES;
        pipe.size -= count;
    }
    return static_cast<intptr_t>(count);
}

intptr_t pipe_write(FdRecord *record, const void *buffer, size_t size) {
    if (!record || record->kind != FD_PIPE || !record->pipe_write_end || record->pipe_index >= RADIX_KERNEL_MAX_PIPES) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    PipeRecord& pipe = g_state.pipes[record->pipe_index];
    if (!pipe.used || (!buffer && size)) return RAD_STATUS_INVALID_ARGUMENT;
    if (pipe.read_refs <= 0) return RAD_STATUS_NOT_FOUND;
    const size_t capacity = RADIX_KERNEL_PIPE_BUFFER_BYTES - pipe.size;
    const size_t count = capacity < size ? capacity : size;
    const size_t write_pos = (pipe.read_pos + pipe.size) % RADIX_KERNEL_PIPE_BUFFER_BYTES;
    if (count) {
        const size_t first = count < (RADIX_KERNEL_PIPE_BUFFER_BYTES - write_pos) ? count : (RADIX_KERNEL_PIPE_BUFFER_BYTES - write_pos);
        memcpy(pipe.buffer + write_pos, buffer, first);
        if (count > first) memcpy(pipe.buffer, static_cast<const uint8_t*>(buffer) + first, count - first);
        pipe.size += count;
    }
    return static_cast<intptr_t>(count);
}

uint16_t ipv4_checksum(const void *data, size_t size) {
    const auto *bytes = static_cast<const uint8_t*>(data);
    uint32_t sum = 0;
    for (size_t i = 0; i + 1u < size; i += 2u) {
        sum += static_cast<uint16_t>((bytes[i] << 8u) | bytes[i + 1u]);
    }
    if (size & 1u) sum += static_cast<uint16_t>(bytes[size - 1u] << 8u);
    while (sum >> 16u) sum = (sum & 0xffffu) + (sum >> 16u);
    return static_cast<uint16_t>(~sum);
}

rad_ipv4_address_t radix_default_ipv4_address() {
    return rad_ipv4_address_t{{RADIX_RKCONFIG_NET_IPV4_A, RADIX_RKCONFIG_NET_IPV4_B, RADIX_RKCONFIG_NET_IPV4_C, RADIX_RKCONFIG_NET_IPV4_D}};
}

rad_ipv4_address_t radix_default_netmask() {
    return rad_ipv4_address_t{{RADIX_RKCONFIG_NET_NETMASK_A, RADIX_RKCONFIG_NET_NETMASK_B, RADIX_RKCONFIG_NET_NETMASK_C, RADIX_RKCONFIG_NET_NETMASK_D}};
}

rad_ipv4_address_t radix_default_gateway() {
    return rad_ipv4_address_t{{RADIX_RKCONFIG_NET_GATEWAY_A, RADIX_RKCONFIG_NET_GATEWAY_B, RADIX_RKCONFIG_NET_GATEWAY_C, RADIX_RKCONFIG_NET_GATEWAY_D}};
}

rad_ipv4_address_t radix_default_ntp_server() {
    return rad_ipv4_address_t{{RADIX_RKCONFIG_NET_NTP_A, RADIX_RKCONFIG_NET_NTP_B, RADIX_RKCONFIG_NET_NTP_C, RADIX_RKCONFIG_NET_NTP_D}};
}

void initialize_net_config() {
    memset(&g_state.net_config, 0, sizeof(g_state.net_config));
    g_state.net_config.size = sizeof(g_state.net_config);
    g_state.net_config.ipv4 = radix_default_ipv4_address();
    g_state.net_config.netmask = radix_default_netmask();
    g_state.net_config.gateway = radix_default_gateway();
    g_state.net_config.ntp_server = radix_default_ntp_server();
    g_state.net_config.ntp_port = RADIX_RKCONFIG_NET_NTP_PORT;
}

const rad_net_stack_config_t& active_net_config() {
    if (g_state.net_config.size < sizeof(uint32_t)) initialize_net_config();
    return g_state.net_config;
}

int ipv4_equal(rad_ipv4_address_t a, rad_ipv4_address_t b) {
    return memcmp(a.bytes, b.bytes, sizeof(a.bytes)) == 0;
}

int ipv4_is_zero(rad_ipv4_address_t a) {
    return (a.bytes[0] | a.bytes[1] | a.bytes[2] | a.bytes[3]) == 0;
}

uint16_t read_be16(const uint8_t *p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8u) | p[1]);
}

uint32_t read_be32(const uint8_t *p) {
    return (static_cast<uint32_t>(p[0]) << 24u)
        | (static_cast<uint32_t>(p[1]) << 16u)
        | (static_cast<uint32_t>(p[2]) << 8u)
        | static_cast<uint32_t>(p[3]);
}

void write_be16(uint8_t *p, uint16_t value) {
    p[0] = static_cast<uint8_t>(value >> 8u);
    p[1] = static_cast<uint8_t>(value);
}

uint32_t ipv4_to_u32(rad_ipv4_address_t address) {
    return (static_cast<uint32_t>(address.bytes[0]) << 24u)
        | (static_cast<uint32_t>(address.bytes[1]) << 16u)
        | (static_cast<uint32_t>(address.bytes[2]) << 8u)
        | static_cast<uint32_t>(address.bytes[3]);
}

int ipv4_same_subnet(rad_ipv4_address_t a, rad_ipv4_address_t b, rad_ipv4_address_t mask) {
    return (ipv4_to_u32(a) & ipv4_to_u32(mask)) == (ipv4_to_u32(b) & ipv4_to_u32(mask));
}

uint16_t next_ipv4_id() {
    static uint16_t s_id = 1;
    return s_id++;
}

ArpEntry *find_arp_entry(rad_ipv4_address_t address) {
    for (size_t i = 0; i < sizeof(g_state.arp_cache) / sizeof(g_state.arp_cache[0]); ++i) {
        ArpEntry& entry = g_state.arp_cache[i];
        if (entry.valid && ipv4_equal(entry.ipv4, address)) return &entry;
    }
    return nullptr;
}

void update_arp_cache(rad_ipv4_address_t address, rad_mac_address_t mac) {
    if (ipv4_is_zero(address)) return;
    ArpEntry *entry = find_arp_entry(address);
    if (!entry) {
        for (size_t i = 0; i < sizeof(g_state.arp_cache) / sizeof(g_state.arp_cache[0]); ++i) {
            if (!g_state.arp_cache[i].valid) {
                entry = &g_state.arp_cache[i];
                break;
            }
        }
    }
    if (!entry) entry = &g_state.arp_cache[0];
    entry->ipv4 = address;
    entry->mac = mac;
    entry->valid = 1;
}

uint16_t arp_entry_count() {
    uint16_t count = 0;
    for (size_t i = 0; i < sizeof(g_state.arp_cache) / sizeof(g_state.arp_cache[0]); ++i) {
        if (g_state.arp_cache[i].valid) ++count;
    }
    return count;
}

rad_status_t send_arp_packet(rad_ipv4_address_t target_ip, const rad_mac_address_t *target_mac, uint16_t opcode) {
    rad_device_t device = nullptr;
    if (rad_net_open("/dev/net0", &device) != RAD_STATUS_OK) return RAD_STATUS_NOT_FOUND;
    rad_net_link_info_t info{};
    if (rad_net_link_info(device, &info) != RAD_STATUS_OK) {
        rad_device_close(device);
        return RAD_STATUS_NOT_FOUND;
    }
    uint8_t frame[60]{};
    if (target_mac) memcpy(frame, target_mac->bytes, 6u);
    else memset(frame, 0xff, 6u);
    memcpy(frame + 6u, info.mac.bytes, 6u);
    frame[12] = 0x08;
    frame[13] = 0x06;
    write_be16(frame + 14u, 1u);
    write_be16(frame + 16u, 0x0800u);
    frame[18] = 6u;
    frame[19] = 4u;
    write_be16(frame + 20u, opcode);
    memcpy(frame + 22u, info.mac.bytes, 6u);
    const rad_ipv4_address_t source = radix_default_ipv4_address();
    memcpy(frame + 28u, source.bytes, 4u);
    if (target_mac) memcpy(frame + 32u, target_mac->bytes, 6u);
    memcpy(frame + 38u, target_ip.bytes, 4u);
    const rad_status_t status = rad_net_send(device, frame, sizeof(frame));
    rad_device_close(device);
    if (status == RAD_STATUS_OK) {
        ++g_state.net_ethernet_tx;
        ++g_state.net_arp_tx;
    }
    return status;
}

SocketRecord *socket_from_fd(FdRecord *record) {
    if (!record || record->kind != FD_SOCKET || record->socket_index >= RADIX_KERNEL_MAX_UDP_SOCKETS) return nullptr;
    SocketRecord& socket = g_state.sockets[record->socket_index];
    return socket.used ? &socket : nullptr;
}

SocketRecord *find_socket(int type, uint16_t port) {
    if (!port) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_UDP_SOCKETS; ++i) {
        SocketRecord& socket = g_state.sockets[i];
        if (socket.used && socket.type == type && socket.local_port == port) return &socket;
    }
    return nullptr;
}

SocketRecord *find_udp_socket(uint16_t port) {
    if (!port) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_UDP_SOCKETS; ++i) {
        SocketRecord& socket = g_state.sockets[i];
        if (socket.used && socket.type == RAD_SOCK_DGRAM && socket.local_port == port) return &socket;
    }
    return nullptr;
}

rad_status_t enqueue_udp_datagram(SocketRecord *socket, const rad_sockaddr_in_t *from, const void *payload, size_t size) {
    if (!socket || !from || (!payload && size)) return RAD_STATUS_INVALID_ARGUMENT;
    if (size > RADIX_KERNEL_UDP_PAYLOAD_BYTES) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_UDP_DATAGRAMS; ++i) {
        UdpDatagram& datagram = socket->datagrams[i];
        if (datagram.used) continue;
        memset(&datagram, 0, sizeof(datagram));
        datagram.used = 1;
        datagram.from = *from;
        datagram.size = size;
        if (size) memcpy(datagram.payload, payload, size);
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t dispatch_udp_datagram(rad_ipv4_address_t source_address, uint16_t source_port, uint16_t destination_port, const void *payload, size_t size) {
    SocketRecord *socket = find_udp_socket(destination_port);
    if (!socket) return RAD_STATUS_NOT_FOUND;
    rad_sockaddr_in_t from{};
    from.family = RAD_AF_INET;
    from.port = source_port;
    from.address = source_address;
    return enqueue_udp_datagram(socket, &from, payload, size);
}

rad_status_t send_icmp_echo_reply(const uint8_t *request, size_t length) {
    if (!request || length < 14u + 20u + 8u) return RAD_STATUS_INVALID_ARGUMENT;
    const size_t ip = 14u;
    const size_t ihl = static_cast<size_t>(request[ip] & 0x0fu) * 4u;
    const uint16_t ip_total = read_be16(request + ip + 2u);
    if (ihl < 20u || length < 14u + ip_total || ip_total < ihl + 8u) return RAD_STATUS_INVALID_ARGUMENT;
    rad_device_t device = nullptr;
    if (rad_net_open("/dev/net0", &device) != RAD_STATUS_OK) return RAD_STATUS_NOT_FOUND;
    rad_net_link_info_t info{};
    if (rad_net_link_info(device, &info) != RAD_STATUS_OK) {
        rad_device_close(device);
        return RAD_STATUS_NOT_FOUND;
    }
    uint8_t frame[1514]{};
    const size_t frame_len = 14u + ip_total;
    memcpy(frame, request + 6u, 6u);
    memcpy(frame + 6u, info.mac.bytes, 6u);
    frame[12] = 0x08;
    frame[13] = 0x00;
    memcpy(frame + ip, request + ip, ip_total);
    memcpy(frame + ip + 12u, request + ip + 16u, 4u);
    memcpy(frame + ip + 16u, request + ip + 12u, 4u);
    frame[ip + 8u] = 64u;
    frame[ip + 10u] = 0;
    frame[ip + 11u] = 0;
    const uint16_t ip_sum = ipv4_checksum(frame + ip, ihl);
    write_be16(frame + ip + 10u, ip_sum);
    const size_t icmp = ip + ihl;
    frame[icmp] = 0u;
    frame[icmp + 2u] = 0u;
    frame[icmp + 3u] = 0u;
    const uint16_t icmp_sum = ipv4_checksum(frame + icmp, ip_total - ihl);
    write_be16(frame + icmp + 2u, icmp_sum);
    const rad_status_t status = rad_net_send(device, frame, frame_len);
    rad_device_close(device);
    if (status == RAD_STATUS_OK) {
        ++g_state.net_ethernet_tx;
        ++g_state.net_ipv4_tx;
        ++g_state.net_icmp_tx;
    }
    return status;
}

rad_status_t parse_arp_frame(const uint8_t *bytes, size_t length) {
    if (!bytes || length < 42u) return RAD_STATUS_INVALID_ARGUMENT;
    if (read_be16(bytes + 14u) != 1u || read_be16(bytes + 16u) != 0x0800u || bytes[18] != 6u || bytes[19] != 4u) {
        return RAD_STATUS_NOT_SUPPORTED;
    }
    ++g_state.net_arp_rx;
    const uint16_t opcode = read_be16(bytes + 20u);
    rad_mac_address_t sender_mac{};
    memcpy(sender_mac.bytes, bytes + 22u, 6u);
    rad_ipv4_address_t sender_ip{{bytes[28], bytes[29], bytes[30], bytes[31]}};
    rad_ipv4_address_t target_ip{{bytes[38], bytes[39], bytes[40], bytes[41]}};
    update_arp_cache(sender_ip, sender_mac);
    if (opcode == 1u && ipv4_equal(target_ip, radix_default_ipv4_address())) {
        send_arp_packet(sender_ip, &sender_mac, 2u);
    }
    return RAD_STATUS_OK;
}

rad_status_t parse_ipv4_frame(const uint8_t *bytes, size_t length) {
    if (!bytes || length < 34u) return RAD_STATUS_INVALID_ARGUMENT;
    const size_t ip_offset = 14u;
    const uint8_t version_ihl = bytes[ip_offset];
    const size_t ihl = static_cast<size_t>(version_ihl & 0x0fu) * 4u;
    if ((version_ihl >> 4u) != 4u || ihl < 20u || length < ip_offset + ihl) return RAD_STATUS_INVALID_ARGUMENT;
    const uint16_t ip_total = read_be16(bytes + ip_offset + 2u);
    if (ip_total < ihl || length < ip_offset + ip_total) return RAD_STATUS_INVALID_ARGUMENT;
    if (ipv4_checksum(bytes + ip_offset, ihl) != 0u) return RAD_STATUS_INVALID_ARGUMENT;
    rad_mac_address_t source_mac{};
    memcpy(source_mac.bytes, bytes + 6u, 6u);
    rad_ipv4_address_t source{{bytes[ip_offset + 12u], bytes[ip_offset + 13u], bytes[ip_offset + 14u], bytes[ip_offset + 15u]}};
    rad_ipv4_address_t destination{{bytes[ip_offset + 16u], bytes[ip_offset + 17u], bytes[ip_offset + 18u], bytes[ip_offset + 19u]}};
    update_arp_cache(source, source_mac);
    if (!ipv4_equal(destination, radix_default_ipv4_address()) && !ipv4_equal(destination, rad_ipv4_address_t{{255, 255, 255, 255}})) {
        return RAD_STATUS_NOT_SUPPORTED;
    }
    ++g_state.net_ipv4_rx;
    const uint8_t protocol = bytes[ip_offset + 9u];
    if (protocol == RAD_IPPROTO_UDP) {
        const size_t udp_offset = ip_offset + ihl;
        if (ip_total < ihl + 8u) return RAD_STATUS_INVALID_ARGUMENT;
        const uint16_t source_port = read_be16(bytes + udp_offset);
        const uint16_t destination_port = read_be16(bytes + udp_offset + 2u);
        const uint16_t udp_length = read_be16(bytes + udp_offset + 4u);
        if (udp_length < 8u || ip_total < ihl + udp_length) return RAD_STATUS_INVALID_ARGUMENT;
        ++g_state.net_udp_rx;
        return dispatch_udp_datagram(source, source_port, destination_port, bytes + udp_offset + 8u, udp_length - 8u);
    }
    if (protocol == 1u) {
        const size_t icmp_offset = ip_offset + ihl;
        if (ip_total < ihl + 8u) return RAD_STATUS_INVALID_ARGUMENT;
        ++g_state.net_icmp_rx;
        if (bytes[icmp_offset] == 8u) return send_icmp_echo_reply(bytes, length);
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t parse_ethernet_frame(const void *frame, size_t length) {
    const auto *bytes = static_cast<const uint8_t*>(frame);
    if (!bytes || length < 14u) return RAD_STATUS_INVALID_ARGUMENT;
    ++g_state.net_ethernet_rx;
    const uint16_t ethertype = read_be16(bytes + 12u);
    if (ethertype == 0x0806u) return parse_arp_frame(bytes, length);
    if (ethertype == 0x0800u) return parse_ipv4_frame(bytes, length);
    return RAD_STATUS_NOT_SUPPORTED;
}

void poll_network_for_udp() {
    rad_device_t device = nullptr;
    if (rad_net_open("/dev/net0", &device) != RAD_STATUS_OK) return;
    rad_net_poll(device);
    uint8_t frame[1536];
    for (uint32_t i = 0; i < 16u; ++i) {
        const intptr_t received = rad_net_receive(device, frame, sizeof(frame));
        if (received <= 0) break;
        parse_ethernet_frame(frame, static_cast<size_t>(received));
    }
    rad_device_close(device);
}

rad_status_t send_udp_frame(const SocketRecord *socket, const rad_sockaddr_in_t *destination, const void *payload, size_t size) {
    if (!socket || !destination || (!payload && size) || size > 1400u) return RAD_STATUS_INVALID_ARGUMENT;
    rad_device_t device = nullptr;
    if (rad_net_open("/dev/net0", &device) != RAD_STATUS_OK) return RAD_STATUS_NOT_FOUND;
    rad_net_link_info_t info{};
    if (rad_net_link_info(device, &info) != RAD_STATUS_OK) {
        rad_device_close(device);
        return RAD_STATUS_NOT_FOUND;
    }
    uint8_t frame[1514]{};
    const rad_net_stack_config_t& config = active_net_config();
    const rad_ipv4_address_t source = socket->local_address.bytes[0] ? socket->local_address : config.ipv4;
    const rad_ipv4_address_t next_hop = ipv4_same_subnet(source, destination->address, config.netmask)
        ? destination->address
        : config.gateway;
    ArpEntry *entry = find_arp_entry(next_hop);
    if (!entry) {
        send_arp_packet(next_hop, nullptr, 1u);
        for (uint32_t i = 0; i < 20u && !entry; ++i) {
            rad_net_poll(device);
            uint8_t rx[1536];
            const intptr_t received = rad_net_receive(device, rx, sizeof(rx));
            if (received > 0) parse_ethernet_frame(rx, static_cast<size_t>(received));
            entry = find_arp_entry(next_hop);
            if (!entry) rad_sleep_ms(1);
        }
    }
    if (entry) memcpy(frame, entry->mac.bytes, 6u);
    else memset(frame, 0xff, 6u);
    memcpy(frame + 6u, info.mac.bytes, 6u);
    frame[12] = 0x08;
    frame[13] = 0x00;
    const size_t ip = 14u;
    const size_t udp = ip + 20u;
    const uint16_t ip_total = static_cast<uint16_t>(20u + 8u + size);
    frame[ip] = 0x45;
    const uint16_t packet_id = next_ipv4_id();
    write_be16(frame + ip + 4u, packet_id);
    write_be16(frame + ip + 6u, 0x4000u);
    frame[ip + 2u] = static_cast<uint8_t>(ip_total >> 8u);
    frame[ip + 3u] = static_cast<uint8_t>(ip_total);
    frame[ip + 8u] = 64u;
    frame[ip + 9u] = RAD_IPPROTO_UDP;
    memcpy(frame + ip + 12u, source.bytes, 4u);
    memcpy(frame + ip + 16u, destination->address.bytes, 4u);
    const uint16_t sum = ipv4_checksum(frame + ip, 20u);
    frame[ip + 10u] = static_cast<uint8_t>(sum >> 8u);
    frame[ip + 11u] = static_cast<uint8_t>(sum);
    const uint16_t udp_length = static_cast<uint16_t>(8u + size);
    frame[udp] = static_cast<uint8_t>(socket->local_port >> 8u);
    frame[udp + 1u] = static_cast<uint8_t>(socket->local_port);
    frame[udp + 2u] = static_cast<uint8_t>(destination->port >> 8u);
    frame[udp + 3u] = static_cast<uint8_t>(destination->port);
    frame[udp + 4u] = static_cast<uint8_t>(udp_length >> 8u);
    frame[udp + 5u] = static_cast<uint8_t>(udp_length);
    if (size) memcpy(frame + udp + 8u, payload, size);
    const rad_status_t status = rad_net_send(device, frame, 14u + ip_total);
    rad_device_close(device);
    if (status == RAD_STATUS_OK) {
        ++g_state.net_ethernet_tx;
        ++g_state.net_ipv4_tx;
        ++g_state.net_udp_tx;
    }
    return status;
}

rad_status_t net_stack_info_internal(rad_net_stack_info_t *info) {
    if (!info) return RAD_STATUS_INVALID_ARGUMENT;
    memset(info, 0, sizeof(*info));
    info->size = sizeof(*info);
    const rad_net_stack_config_t& config = active_net_config();
    info->ipv4 = config.ipv4;
    info->netmask = config.netmask;
    info->gateway = config.gateway;
    info->ntp_server = config.ntp_server;
    info->ntp_port = config.ntp_port;
    info->arp_entries = arp_entry_count();
    info->ethernet_rx = g_state.net_ethernet_rx;
    info->ethernet_tx = g_state.net_ethernet_tx;
    info->arp_rx = g_state.net_arp_rx;
    info->arp_tx = g_state.net_arp_tx;
    info->ipv4_rx = g_state.net_ipv4_rx;
    info->ipv4_tx = g_state.net_ipv4_tx;
    info->udp_rx = g_state.net_udp_rx;
    info->udp_tx = g_state.net_udp_tx;
    info->icmp_rx = g_state.net_icmp_rx;
    info->icmp_tx = g_state.net_icmp_tx;
    return RAD_STATUS_OK;
}

rad_status_t net_configure_internal(const rad_net_stack_config_t *config) {
    if (!config || config->size < offsetof(rad_net_stack_config_t, flags)) return RAD_STATUS_INVALID_ARGUMENT;
    rad_net_stack_config_t next{};
    next.size = sizeof(next);
    next.ipv4 = ipv4_is_zero(config->ipv4) ? radix_default_ipv4_address() : config->ipv4;
    next.netmask = ipv4_is_zero(config->netmask) ? radix_default_netmask() : config->netmask;
    next.gateway = ipv4_is_zero(config->gateway) ? radix_default_gateway() : config->gateway;
    next.ntp_server = ipv4_is_zero(config->ntp_server) ? radix_default_ntp_server() : config->ntp_server;
    next.ntp_port = config->ntp_port ? config->ntp_port : RADIX_RKCONFIG_NET_NTP_PORT;
    next.flags = config->flags;
    g_state.net_config = next;
    memset(g_state.arp_cache, 0, sizeof(g_state.arp_cache));
    return RAD_STATUS_OK;
}

rad_status_t net_ntp_status_internal(rad_ntp_status_t *status) {
    if (!status) return RAD_STATUS_INVALID_ARGUMENT;
    *status = g_state.ntp_status;
    status->size = sizeof(*status);
    return RAD_STATUS_OK;
}

rad_status_t net_ntp_query_internal(rad_ntp_query_t *query) {
    if (!query || query->size < sizeof(rad_ntp_query_t)) return RAD_STATUS_INVALID_ARGUMENT;
    const rad_net_stack_config_t& config = active_net_config();
    rad_ipv4_address_t server_ip = ipv4_is_zero(query->server) ? config.ntp_server : query->server;
    uint16_t server_port = query->port ? query->port : config.ntp_port;
    uint16_t timeout_ms = query->timeout_ms ? query->timeout_ms : 1000u;
    int32_t fd = rad_socket_create(RAD_AF_INET, RAD_SOCK_DGRAM, RAD_IPPROTO_UDP);
    if (fd < 0) return static_cast<rad_status_t>(fd);
    rad_sockaddr_in_t local{};
    local.family = RAD_AF_INET;
    local.port = 49123u;
    local.address = config.ipv4;
    if (rad_socket_bind(fd, &local, sizeof(local)) != RAD_STATUS_OK) {
        local.port = static_cast<uint16_t>(49152u + (rad_time_millis() % 1024u));
        rad_socket_bind(fd, &local, sizeof(local));
    }
    uint8_t request[48]{};
    request[0] = 0x1bu;
    rad_sockaddr_in_t destination{};
    destination.family = RAD_AF_INET;
    destination.port = server_port;
    destination.address = server_ip;
    ++g_state.ntp_status.queries;
    g_state.ntp_status.server = server_ip;
    g_state.ntp_status.port = server_port;
    const intptr_t sent = rad_socket_sendto(fd, request, sizeof(request), 0, &destination, sizeof(destination));
    if (sent != static_cast<intptr_t>(sizeof(request))) {
        rad_fd_close(fd);
        net_ntp_status_internal(&query->status);
        return sent < 0 ? static_cast<rad_status_t>(sent) : RAD_STATUS_ERROR;
    }
    uint8_t response[64]{};
    rad_sockaddr_in_t from{};
    size_t from_len = sizeof(from);
    const uint64_t start = rad_time_millis();
    const uint32_t max_attempts = static_cast<uint32_t>(timeout_ms ? timeout_ms : 1000u) + 32u;
    for (uint32_t attempt = 0; attempt < max_attempts; ++attempt) {
        const intptr_t received = rad_socket_recvfrom(fd, response, sizeof(response), 0, &from, &from_len);
        if (received >= 48 && from.port == server_port && ipv4_equal(from.address, server_ip)) {
            const uint32_t ntp_seconds = read_be32(response + 40u);
            if (ntp_seconds >= 2208988800u) {
                const uint64_t unix_seconds = static_cast<uint64_t>(ntp_seconds - 2208988800u);
                g_state.ntp_status.valid = 1u;
                g_state.ntp_status.last_unix_seconds = unix_seconds;
                g_state.ntp_status.offset_millis = static_cast<int64_t>(unix_seconds * 1000u) - static_cast<int64_t>(rad_time_millis());
                ++g_state.ntp_status.responses;
                rad_fd_close(fd);
                net_ntp_status_internal(&query->status);
                return RAD_STATUS_OK;
            }
        }
        if (received < 0 && received != RAD_STATUS_NOT_FOUND) {
            rad_fd_close(fd);
            net_ntp_status_internal(&query->status);
            return static_cast<rad_status_t>(received);
        }
        if (attempt > 8u && rad_time_millis() - start > timeout_ms) break;
        rad_sleep_ms(1);
    }
    rad_fd_close(fd);
    net_ntp_status_internal(&query->status);
    return RAD_STATUS_TIMEOUT;
}

void open_stdio_fd(int32_t fd) {
    FdRecord record{};
    record.kind = FD_DEVICE;
    record.refs = 1;
    record.owner_fd = -1;
    record.owner_pid = 0;
    record.local_fd = fd;
    if (rad_device_open("/dev/console", &record.device) == RAD_STATUS_OK) {
        install_fd_record(record, fd);
    }
}

void initialize_stdio_fds(void) {
    open_stdio_fd(0);
    open_stdio_fd(1);
    open_stdio_fd(2);
}

void append_char(char *dst, size_t dst_size, size_t& pos, char ch) {
    if (!dst || pos + 1 >= dst_size) return;
    dst[pos++] = ch;
    dst[pos] = '\0';
}

void append_text(char *dst, size_t dst_size, size_t& pos, const char *text) {
    if (!text) return;
    while (*text) append_char(dst, dst_size, pos, *text++);
}

void append_u64(char *dst, size_t dst_size, size_t& pos, uint64_t value) {
    char tmp[24];
    size_t count = 0;
    do {
        tmp[count++] = static_cast<char>('0' + (value % 10u));
        value /= 10u;
    } while (value && count < sizeof(tmp));
    while (count) append_char(dst, dst_size, pos, tmp[--count]);
}

void append_i64(char *dst, size_t dst_size, size_t& pos, int64_t value) {
    if (value < 0) {
        append_char(dst, dst_size, pos, '-');
        append_u64(dst, dst_size, pos, static_cast<uint64_t>(-value));
    } else {
        append_u64(dst, dst_size, pos, static_cast<uint64_t>(value));
    }
}

void append_hex32(char *dst, size_t dst_size, size_t& pos, uint32_t value) {
    const char hex[] = "0123456789abcdef";
    bool started = false;
    for (int shift = 28; shift >= 0; shift -= 4) {
        const uint32_t nibble = (value >> shift) & 0xfu;
        if (nibble || started || shift == 0) {
            append_char(dst, dst_size, pos, hex[nibble]);
            started = true;
        }
    }
}

void append_hex64(char *dst, size_t dst_size, size_t& pos, uint64_t value) {
    const char hex[] = "0123456789abcdef";
    bool started = false;
    for (int shift = 60; shift >= 0; shift -= 4) {
        const uint64_t nibble = (value >> shift) & 0xfu;
        if (nibble || started || shift == 0) {
            append_char(dst, dst_size, pos, hex[nibble]);
            started = true;
        }
    }
}

uint64_t parse_u64(const char *text, uint64_t fallback) {
    if (!text || !*text) return fallback;
    uint64_t value = 0;
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9') return fallback;
        value = value * 10u + static_cast<uint64_t>(*p - '0');
    }
    return value;
}

const char *task_state_name(rad_task_state_t state) {
    switch (state) {
    case RAD_TASK_NEW: return "new";
    case RAD_TASK_READY: return "ready";
    case RAD_TASK_RUNNING: return "running";
    case RAD_TASK_SLEEPING: return "sleeping";
    case RAD_TASK_BLOCKED: return "blocked";
    case RAD_TASK_FINISHED: return "finished";
    case RAD_TASK_DETACHED: return "detached";
    }
    return "unknown";
}

const char *program_state_name(rad_program_state_t state) {
    switch (state) {
    case RAD_PROGRAM_LOADED: return "loaded";
    case RAD_PROGRAM_RUNNING: return "running";
    case RAD_PROGRAM_FINISHED: return "finished";
    case RAD_PROGRAM_FAILED: return "failed";
    }
    return "unknown";
}

#pragma pack(push, 1)
struct Elf32Header {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

struct Elf32ProgramHeader {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
};

struct Elf64Header {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

struct Elf64ProgramHeader {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};
#pragma pack(pop)

constexpr uint32_t ELF_PT_LOAD = 1u;
constexpr uint16_t ELF_EM_ARM = 40u;
constexpr uint16_t ELF_EM_AARCH64 = 183u;

bool elf_ident_ok(const uint8_t *image, size_t size) {
    return size >= 16
        && image[0] == 0x7fu
        && image[1] == 'E'
        && image[2] == 'L'
        && image[3] == 'F'
        && image[5] == 1u;
}

void program_name_from_path(char *dst, size_t dst_size, const char *path) {
    const char *name = path ? strrchr(path, '/') : nullptr;
    copy_string(dst, dst_size, name ? name + 1 : path);
}

rad_status_t load_elf32_image(ProgramRecord& program, const uint8_t *file_image, size_t file_size) {
    if (file_size < sizeof(Elf32Header)) return RAD_STATUS_INVALID_ARGUMENT;
    const auto *header = reinterpret_cast<const Elf32Header*>(file_image);
    if (!elf_ident_ok(file_image, file_size) || header->ident[4] != 1u || header->machine != ELF_EM_ARM) {
        return RAD_STATUS_NOT_SUPPORTED;
    }
    if (header->phoff + static_cast<size_t>(header->phnum) * header->phentsize > file_size
        || header->phentsize < sizeof(Elf32ProgramHeader)) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }

    uint32_t min_vaddr = UINT32_MAX;
    uint32_t max_vaddr = 0;
    for (uint16_t i = 0; i < header->phnum; ++i) {
        const auto *ph = reinterpret_cast<const Elf32ProgramHeader*>(file_image + header->phoff + i * header->phentsize);
        if (ph->type != ELF_PT_LOAD) continue;
        if (ph->offset + ph->filesz > file_size || ph->filesz > ph->memsz) return RAD_STATUS_INVALID_ARGUMENT;
        if (ph->vaddr < min_vaddr) min_vaddr = ph->vaddr;
        if (ph->vaddr + ph->memsz > max_vaddr) max_vaddr = ph->vaddr + ph->memsz;
    }
    if (min_vaddr == UINT32_MAX || max_vaddr <= min_vaddr || header->entry < min_vaddr || header->entry >= max_vaddr) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }

    const size_t image_size = static_cast<size_t>(max_vaddr - min_vaddr);
    uint8_t *image = static_cast<uint8_t*>(rad_memory_alloc(image_size));
    if (!image) return RAD_STATUS_NO_MEMORY;
    memset(image, 0, image_size);
    for (uint16_t i = 0; i < header->phnum; ++i) {
        const auto *ph = reinterpret_cast<const Elf32ProgramHeader*>(file_image + header->phoff + i * header->phentsize);
        if (ph->type != ELF_PT_LOAD) continue;
        memcpy(image + (ph->vaddr - min_vaddr), file_image + ph->offset, ph->filesz);
    }
    program.image = image;
    program.image_size = image_size;
    program.entry = reinterpret_cast<uintptr_t>(image + (header->entry - min_vaddr));
    return RAD_STATUS_OK;
}

rad_status_t load_elf64_image(ProgramRecord& program, const uint8_t *file_image, size_t file_size) {
    if (file_size < sizeof(Elf64Header)) return RAD_STATUS_INVALID_ARGUMENT;
    const auto *header = reinterpret_cast<const Elf64Header*>(file_image);
    if (!elf_ident_ok(file_image, file_size) || header->ident[4] != 2u || header->machine != ELF_EM_AARCH64) {
        return RAD_STATUS_NOT_SUPPORTED;
    }
    if (header->phoff > file_size
        || static_cast<uint64_t>(header->phnum) * header->phentsize > file_size - header->phoff
        || header->phentsize < sizeof(Elf64ProgramHeader)) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }

    uint64_t min_vaddr = UINT64_MAX;
    uint64_t max_vaddr = 0;
    for (uint16_t i = 0; i < header->phnum; ++i) {
        const auto *ph = reinterpret_cast<const Elf64ProgramHeader*>(file_image + header->phoff + i * header->phentsize);
        if (ph->type != ELF_PT_LOAD) continue;
        if (ph->offset > file_size || ph->filesz > file_size - ph->offset || ph->filesz > ph->memsz) return RAD_STATUS_INVALID_ARGUMENT;
        if (ph->vaddr < min_vaddr) min_vaddr = ph->vaddr;
        if (ph->vaddr + ph->memsz > max_vaddr) max_vaddr = ph->vaddr + ph->memsz;
    }
    if (min_vaddr == UINT64_MAX || max_vaddr <= min_vaddr || header->entry < min_vaddr || header->entry >= max_vaddr) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    if (max_vaddr - min_vaddr > static_cast<uint64_t>(SIZE_MAX)) return RAD_STATUS_NO_MEMORY;

    const size_t image_size = static_cast<size_t>(max_vaddr - min_vaddr);
    uint8_t *image = static_cast<uint8_t*>(rad_memory_alloc(image_size));
    if (!image) return RAD_STATUS_NO_MEMORY;
    memset(image, 0, image_size);
    for (uint16_t i = 0; i < header->phnum; ++i) {
        const auto *ph = reinterpret_cast<const Elf64ProgramHeader*>(file_image + header->phoff + i * header->phentsize);
        if (ph->type != ELF_PT_LOAD) continue;
        memcpy(image + static_cast<size_t>(ph->vaddr - min_vaddr), file_image + ph->offset, static_cast<size_t>(ph->filesz));
    }
    program.image = image;
    program.image_size = image_size;
    program.entry = reinterpret_cast<uintptr_t>(image + static_cast<size_t>(header->entry - min_vaddr));
    return RAD_STATUS_OK;
}

rad_status_t load_native_elf_image(ProgramRecord& program, const uint8_t *file_image, size_t file_size) {
#if defined(__aarch64__) || defined(__AARCH64EL__)
    return load_elf64_image(program, file_image, file_size);
#else
    return load_elf32_image(program, file_image, file_size);
#endif
}

void program_task_entry(void *context) {
    auto *launch = static_cast<ProgramLaunchContext*>(context);
    auto *program = launch ? launch->program : nullptr;
    if (!program || !program->entry) {
        rad_memory_free(launch);
        return;
    }
    program->state = RAD_PROGRAM_RUNNING;
    const char *argv[8]{};
    const int argc = launch->argc;
    for (int i = 0; i < argc && i < 8; ++i) argv[i] = launch->args[i];
    using Entry = int (*)(int, const char **);
    auto entry = reinterpret_cast<Entry>(program->entry);
    program->exit_code = entry(argc, argv);
    program->state = RAD_PROGRAM_FINISHED;
    rad_memory_free(launch);
}

rad_status_t cmd_help(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_COMMANDS; ++i) {
        if (!g_state.commands[i].used) continue;
        write(g_state.commands[i].name, write_context);
        write(" - ", write_context);
        write_line(write, write_context, g_state.commands[i].description);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_mem(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    char line[128];
    size_t pos = 0;
    line[0] = '\0';
    append_text(line, sizeof(line), pos, "live=");
    append_u64(line, sizeof(line), pos, g_state.memory.bytes_live);
    append_text(line, sizeof(line), pos, " peak=");
    append_u64(line, sizeof(line), pos, g_state.memory.peak_bytes_live);
    append_text(line, sizeof(line), pos, " allocs=");
    append_u64(line, sizeof(line), pos, g_state.memory.allocations);
    append_text(line, sizeof(line), pos, " frees=");
    append_u64(line, sizeof(line), pos, g_state.memory.frees);
    write_line(write, write_context, line);
    return RAD_STATUS_OK;
}

rad_status_t cmd_devices(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_DEVICES; ++i) {
        if (!g_state.devices[i].used) continue;
        write(g_state.devices[i].name, write_context);
        write(" type=", write_context);
        char type[16];
        size_t pos = 0;
        type[0] = '\0';
        append_u64(type, sizeof(type), pos, static_cast<uint64_t>(g_state.devices[i].type));
        write_line(write, write_context, type);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_irq(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_irq_info_t irqs[RADIX_KERNEL_MAX_IRQS]{};
    const size_t count = rad_irq_list(irqs, RADIX_KERNEL_MAX_IRQS);
    for (size_t i = 0; i < count && i < RADIX_KERNEL_MAX_IRQS; ++i) {
        char line[160];
        size_t pos = 0;
        line[0] = '\0';
        append_u64(line, sizeof(line), pos, irqs[i].irq);
        append_text(line, sizeof(line), pos, " ");
        append_text(line, sizeof(line), pos, irqs[i].name);
        append_text(line, sizeof(line), pos, " enabled=");
        append_u64(line, sizeof(line), pos, static_cast<uint64_t>(irqs[i].enabled));
        append_text(line, sizeof(line), pos, " count=");
        append_u64(line, sizeof(line), pos, irqs[i].count);
        append_text(line, sizeof(line), pos, " unhandled=");
        append_u64(line, sizeof(line), pos, irqs[i].unhandled_count);
        write_line(write, write_context, line);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_time(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    char line[64];
    size_t pos = 0;
    line[0] = '\0';
    append_u64(line, sizeof(line), pos, rad_time_millis());
    append_text(line, sizeof(line), pos, " ms");
    write_line(write, write_context, line);
    return RAD_STATUS_OK;
}

rad_status_t cmd_perf(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_perf_counter_info_t counters[RADIX_KERNEL_MAX_PERF_COUNTERS]{};
    const size_t count = rad_perf_counter_list(counters, RADIX_KERNEL_MAX_PERF_COUNTERS);
    for (size_t i = 0; i < count && i < RADIX_KERNEL_MAX_PERF_COUNTERS; ++i) {
        char line[96];
        size_t pos = 0;
        line[0] = '\0';
        append_text(line, sizeof(line), pos, counters[i].name);
        append_text(line, sizeof(line), pos, "=");
        append_u64(line, sizeof(line), pos, counters[i].value);
        write_line(write, write_context, line);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_latency(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    char line[96];
    size_t pos = 0;
    line[0] = '\0';
    append_text(line, sizeof(line), pos, "time.us=");
    append_u64(line, sizeof(line), pos, rad_time_micros());
    write_line(write, write_context, line);

    rad_perf_counter_info_t counters[RADIX_KERNEL_MAX_PERF_COUNTERS]{};
    const size_t count = rad_perf_counter_list(counters, RADIX_KERNEL_MAX_PERF_COUNTERS);
    for (size_t i = 0; i < count && i < RADIX_KERNEL_MAX_PERF_COUNTERS; ++i) {
        if (!text_contains(counters[i].name, "latency")
            && !text_contains(counters[i].name, "timer")
            && !text_contains(counters[i].name, "irq")) {
            continue;
        }
        pos = 0;
        line[0] = '\0';
        append_text(line, sizeof(line), pos, counters[i].name);
        append_text(line, sizeof(line), pos, "=");
        append_u64(line, sizeof(line), pos, counters[i].value);
        write_line(write, write_context, line);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_mounts(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_MOUNTS; ++i) {
        if (g_state.mounts[i].used) write_line(write, write_context, g_state.mounts[i].mount_point);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_services(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_service_info_t services[RADIX_KERNEL_MAX_SERVICES]{};
    const size_t count = rad_service_list(services, RADIX_KERNEL_MAX_SERVICES);
    for (size_t i = 0; i < count && i < RADIX_KERNEL_MAX_SERVICES; ++i) {
        char line[256];
        size_t pos = 0;
        line[0] = '\0';
        append_text(line, sizeof(line), pos, services[i].name);
        append_text(line, sizeof(line), pos, " state=");
        append_text(line, sizeof(line), pos, service_state_name(services[i].state));
        append_text(line, sizeof(line), pos, " order=");
        append_i64(line, sizeof(line), pos, services[i].order);
        append_text(line, sizeof(line), pos, " autostart=");
        append_i64(line, sizeof(line), pos, services[i].autostart);
        append_text(line, sizeof(line), pos, " status=");
        append_i64(line, sizeof(line), pos, services[i].last_status);
        if (services[i].capability[0]) {
            append_text(line, sizeof(line), pos, " cap=");
            append_text(line, sizeof(line), pos, services[i].capability);
        }
        if (services[i].backend[0]) {
            append_text(line, sizeof(line), pos, " backend=");
            append_text(line, sizeof(line), pos, services[i].backend);
        }
        write_line(write, write_context, line);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_dmesg(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_log_entry_t entries[16]{};
    uint64_t after = 0;
    for (;;) {
        const size_t count = rad_log_read(entries, 16, after);
        if (!count) break;
        for (size_t i = 0; i < count && i < 16; ++i) {
            after = entries[i].sequence;
            char line[320];
            size_t pos = 0;
            line[0] = '\0';
            append_u64(line, sizeof(line), pos, entries[i].time_millis);
            append_text(line, sizeof(line), pos, "ms [");
            append_text(line, sizeof(line), pos, rad_log_level_name(entries[i].level));
            append_text(line, sizeof(line), pos, "] [");
            append_text(line, sizeof(line), pos, entries[i].category);
            append_text(line, sizeof(line), pos, "] ");
            append_text(line, sizeof(line), pos, entries[i].message);
            write_line(write, write_context, line);
        }
    }
    return RAD_STATUS_OK;
}

rad_status_t write_file_to_terminal(const char *path, rad_terminal_write_t write, void *write_context) {
    rad_file_t file = nullptr;
    rad_status_t status = rad_vfs_open(path, RAD_VFS_READ, &file);
    if (status != RAD_STATUS_OK) return status;
    char buffer[256];
    size_t done = 0;
    while ((status = rad_vfs_read(file, buffer, sizeof(buffer) - 1u, &done)) == RAD_STATUS_OK && done > 0) {
        buffer[done] = '\0';
        write(buffer, write_context);
    }
    rad_vfs_close(file);
    return status == RAD_STATUS_OK ? RAD_STATUS_OK : status;
}

rad_status_t cmd_logread(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void*) {
    char path[128];
    if (argc < 2) {
        copy_string(path, sizeof(path), "/var/log/radix/init.log");
    } else if (strcmp(argv[1], "kernel") == 0) {
        copy_string(path, sizeof(path), "/var/log/radix/rkernel.log");
    } else if (strcmp(argv[1], "init") == 0) {
        copy_string(path, sizeof(path), "/var/log/radix/init.log");
    } else {
        size_t pos = 0;
        path[0] = '\0';
        append_text(path, sizeof(path), pos, "/var/log/radix/");
        append_text(path, sizeof(path), pos, argv[1]);
        append_text(path, sizeof(path), pos, ".log");
    }
    return write_file_to_terminal(path, write, write_context);
}

rad_status_t cmd_initctl(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void*) {
    if (argc < 2 || strcmp(argv[1], "list") == 0) {
        return write_file_to_terminal("/run/radinit/status.txt", write, write_context);
    }
    if (strcmp(argv[1], "status") == 0) {
        if (argc < 3) return RAD_STATUS_INVALID_ARGUMENT;
        rad_file_t file = nullptr;
        rad_status_t status = rad_vfs_open("/run/radinit/status.txt", RAD_VFS_READ, &file);
        if (status != RAD_STATUS_OK) return status;
        char text[1024];
        size_t done = 0;
        status = rad_vfs_read(file, text, sizeof(text) - 1u, &done);
        rad_vfs_close(file);
        if (status != RAD_STATUS_OK) return status;
        text[done] = '\0';
        char *line = text;
        while (*line) {
            char *next = strchr(line, '\n');
            if (next) *next = '\0';
            const size_t name_len = strlen(argv[2]);
            if (strncmp(line, argv[2], name_len) == 0 && (line[name_len] == '\0' || line[name_len] == ' ')) {
                write_line(write, write_context, line);
                return RAD_STATUS_OK;
            }
            if (!next) break;
            line = next + 1;
        }
        return RAD_STATUS_NOT_FOUND;
    }
    if (strcmp(argv[1], "start") == 0 || strcmp(argv[1], "stop") == 0) {
        write_line(write, write_context, "initctl control operations are not supported yet");
        return RAD_STATUS_NOT_SUPPORTED;
    }
    return RAD_STATUS_INVALID_ARGUMENT;
}

struct CmdLsContext {
    rad_terminal_write_t write;
    void *write_context;
};

int cmd_ls_writer(const char *name, const rad_vfs_stat_t *stat, void *context) {
    auto *ls = static_cast<CmdLsContext*>(context);
    if (!ls || !ls->write || !name) return 0;
    ls->write(name, ls->write_context);
    if (stat && stat->is_directory) ls->write("/", ls->write_context);
    ls->write("\n", ls->write_context);
    return 1;
}

rad_status_t cmd_ls(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void*) {
    const char *path = argc > 1 ? argv[1] : ".";
    CmdLsContext context{write, write_context};
    return rad_vfs_list(path, cmd_ls_writer, &context);
}

rad_status_t cmd_pwd(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    char cwd[96];
    const rad_status_t status = rad_vfs_getcwd(cwd, sizeof(cwd));
    if (status == RAD_STATUS_OK) write_line(write, write_context, cwd);
    return status;
}

rad_status_t cmd_cd(int argc, const char **argv, rad_terminal_write_t, void*, void*) {
    return rad_vfs_chdir(argc > 1 ? argv[1] : "/");
}

rad_status_t cmd_mkdir(int argc, const char **argv, rad_terminal_write_t, void*, void*) {
    if (argc < 2) return RAD_STATUS_INVALID_ARGUMENT;
    for (int i = 1; i < argc; ++i) {
        const rad_status_t status = rad_vfs_mkdir(argv[i]);
        if (status != RAD_STATUS_OK) return status;
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_rmdir(int argc, const char **argv, rad_terminal_write_t, void*, void*) {
    if (argc < 2) return RAD_STATUS_INVALID_ARGUMENT;
    for (int i = 1; i < argc; ++i) {
        const rad_status_t status = rad_vfs_rmdir(argv[i]);
        if (status != RAD_STATUS_OK) return status;
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_rm(int argc, const char **argv, rad_terminal_write_t, void*, void*) {
    if (argc < 2) return RAD_STATUS_INVALID_ARGUMENT;
    for (int i = 1; i < argc; ++i) {
        const rad_status_t status = rad_vfs_remove(argv[i]);
        if (status != RAD_STATUS_OK) return status;
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_touch(int argc, const char **argv, rad_terminal_write_t, void*, void*) {
    if (argc < 2) return RAD_STATUS_INVALID_ARGUMENT;
    for (int i = 1; i < argc; ++i) {
        rad_file_t file = nullptr;
        const rad_status_t status = rad_vfs_open(argv[i], RAD_VFS_CREATE | RAD_VFS_WRITE, &file);
        if (status != RAD_STATUS_OK) return status;
        rad_vfs_close(file);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_cat(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void*) {
    if (argc < 2) return RAD_STATUS_INVALID_ARGUMENT;
    char buffer[256];
    for (int i = 1; i < argc; ++i) {
        rad_file_t file = nullptr;
        rad_status_t status = rad_vfs_open(argv[i], RAD_VFS_READ, &file);
        if (status != RAD_STATUS_OK) return status;
        size_t done = 0;
        while ((status = rad_vfs_read(file, buffer, sizeof(buffer), &done)) == RAD_STATUS_OK && done > 0) {
            char saved = 0;
            if (done < sizeof(buffer)) {
                saved = buffer[done];
                buffer[done] = '\0';
                write(buffer, write_context);
                buffer[done] = saved;
            } else {
                char chunk[257];
                memcpy(chunk, buffer, done);
                chunk[done] = '\0';
                write(chunk, write_context);
            }
        }
        rad_vfs_close(file);
        if (status != RAD_STATUS_OK) return status;
    }
    return RAD_STATUS_OK;
}

rad_status_t copy_vfs_file(const char *src, const char *dst) {
    rad_file_t in = nullptr;
    rad_status_t status = rad_vfs_open(src, RAD_VFS_READ, &in);
    if (status != RAD_STATUS_OK) return status;
    rad_file_t out = nullptr;
    status = rad_vfs_open(dst, RAD_VFS_CREATE | RAD_VFS_WRITE | RAD_VFS_TRUNCATE, &out);
    if (status != RAD_STATUS_OK) {
        rad_vfs_close(in);
        return status;
    }
    char buffer[256];
    size_t done = 0;
    while ((status = rad_vfs_read(in, buffer, sizeof(buffer), &done)) == RAD_STATUS_OK && done > 0) {
        size_t written = 0;
        status = rad_vfs_write(out, buffer, done, &written);
        if (status != RAD_STATUS_OK || written != done) {
            status = status == RAD_STATUS_OK ? RAD_STATUS_ERROR : status;
            break;
        }
    }
    rad_vfs_close(in);
    rad_vfs_close(out);
    return status;
}

rad_status_t cmd_cp(int argc, const char **argv, rad_terminal_write_t, void*, void*) {
    return argc == 3 ? copy_vfs_file(argv[1], argv[2]) : RAD_STATUS_INVALID_ARGUMENT;
}

rad_status_t cmd_mv(int argc, const char **argv, rad_terminal_write_t, void*, void*) {
    return argc == 3 ? rad_vfs_rename(argv[1], argv[2]) : RAD_STATUS_INVALID_ARGUMENT;
}

rad_status_t cmd_echo(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void*) {
    for (int i = 1; i < argc; ++i) {
        if (i > 1) write(" ", write_context);
        write(argv[i], write_context);
    }
    write("\n", write_context);
    return RAD_STATUS_OK;
}

rad_status_t cmd_head_tail(int argc, const char **argv, rad_terminal_write_t write, void *write_context, int tail) {
    int lines = 10;
    int path_index = 1;
    if (argc >= 4 && strcmp(argv[1], "-n") == 0) {
        lines = static_cast<int>(parse_u64(argv[2], 10));
        path_index = 3;
    }
    if (argc <= path_index || lines < 0) return RAD_STATUS_INVALID_ARGUMENT;
    rad_file_t file = nullptr;
    rad_status_t status = rad_vfs_open(argv[path_index], RAD_VFS_READ, &file);
    if (status != RAD_STATUS_OK) return status;
    char text[2048];
    size_t total = 0;
    size_t done = 0;
    while (total < sizeof(text) - 1u && (status = rad_vfs_read(file, text + total, sizeof(text) - 1u - total, &done)) == RAD_STATUS_OK && done > 0) total += done;
    rad_vfs_close(file);
    text[total] = '\0';
    if (status != RAD_STATUS_OK) return status;
    const char *start = text;
    if (tail) {
        int seen = 0;
        for (size_t i = total; i > 0; --i) {
            if (text[i - 1u] == '\n' && ++seen > lines) {
                start = text + i;
                break;
            }
        }
        write(start, write_context);
        return RAD_STATUS_OK;
    }
    int emitted = 0;
    for (const char *p = text; *p && emitted < lines; ++p) {
        char one[2] = {*p, '\0'};
        write(one, write_context);
        if (*p == '\n') ++emitted;
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_head(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void*) {
    return cmd_head_tail(argc, argv, write, write_context, 0);
}

rad_status_t cmd_tail(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void*) {
    return cmd_head_tail(argc, argv, write, write_context, 1);
}

rad_status_t cmd_wc(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void*) {
    if (argc < 2) return RAD_STATUS_INVALID_ARGUMENT;
    rad_file_t file = nullptr;
    rad_status_t status = rad_vfs_open(argv[1], RAD_VFS_READ, &file);
    if (status != RAD_STATUS_OK) return status;
    uint64_t bytes = 0;
    uint64_t lines = 0;
    uint64_t words = 0;
    int in_word = 0;
    char buffer[256];
    size_t done = 0;
    while ((status = rad_vfs_read(file, buffer, sizeof(buffer), &done)) == RAD_STATUS_OK && done > 0) {
        bytes += done;
        for (size_t i = 0; i < done; ++i) {
            const char ch = buffer[i];
            if (ch == '\n') ++lines;
            const int space = ch == ' ' || ch == '\n' || ch == '\t' || ch == '\r';
            if (space) in_word = 0;
            else if (!in_word) {
                in_word = 1;
                ++words;
            }
        }
    }
    rad_vfs_close(file);
    if (status != RAD_STATUS_OK) return status;
    char line[96];
    size_t pos = 0;
    line[0] = '\0';
    append_u64(line, sizeof(line), pos, lines);
    append_text(line, sizeof(line), pos, " ");
    append_u64(line, sizeof(line), pos, words);
    append_text(line, sizeof(line), pos, " ");
    append_u64(line, sizeof(line), pos, bytes);
    append_text(line, sizeof(line), pos, " ");
    append_text(line, sizeof(line), pos, argv[1]);
    write_line(write, write_context, line);
    return RAD_STATUS_OK;
}

rad_status_t cmd_basename(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void*) {
    if (argc < 2) return RAD_STATUS_INVALID_ARGUMENT;
    const char *name = strrchr(argv[1], '/');
    write_line(write, write_context, name && name[1] ? name + 1 : argv[1]);
    return RAD_STATUS_OK;
}

rad_status_t cmd_dirname(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void*) {
    if (argc < 2) return RAD_STATUS_INVALID_ARGUMENT;
    char path[96];
    copy_string(path, sizeof(path), argv[1]);
    char *slash = strrchr(path, '/');
    if (!slash) write_line(write, write_context, ".");
    else if (slash == path) {
        slash[1] = '\0';
        write_line(write, write_context, path);
    } else {
        *slash = '\0';
        write_line(write, write_context, path);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_env(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    write_line(write, write_context, "PATH=/bin:/usr/bin");
    return RAD_STATUS_OK;
}

rad_status_t cmd_true(int, const char**, rad_terminal_write_t, void*, void*) {
    return RAD_STATUS_OK;
}

rad_status_t cmd_false(int, const char**, rad_terminal_write_t, void*, void*) {
    return RAD_STATUS_ERROR;
}

rad_status_t cmd_exit(int, const char**, rad_terminal_write_t, void*, void*) {
    return RAD_STATUS_OK;
}

rad_status_t cmd_bootinfo(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    if (!g_state.has_boot) return RAD_STATUS_NOT_FOUND;
    write(write_context ? "backend=" : "backend=", write_context);
    write_line(write, write_context, g_state.boot.backend);
    write(write_context ? "board=" : "board=", write_context);
    write_line(write, write_context, g_state.boot.board);
    write(write_context ? "console=" : "console=", write_context);
    write_line(write, write_context, g_state.boot.console_device);
    write(write_context ? "sd_mode=" : "sd_mode=", write_context);
    write_line(write, write_context, g_state.boot.sd_mode);
    return RAD_STATUS_OK;
}

rad_status_t cmd_tasks(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_task_info_t tasks[RADIX_KERNEL_MAX_TASKS]{};
    const size_t count = rad_task_list(tasks, RADIX_KERNEL_MAX_TASKS);
    for (size_t i = 0; i < count && i < RADIX_KERNEL_MAX_TASKS; ++i) {
        char line[160];
        size_t pos = 0;
        line[0] = '\0';
        append_u64(line, sizeof(line), pos, tasks[i].id);
        append_char(line, sizeof(line), pos, ' ');
        append_text(line, sizeof(line), pos, tasks[i].name);
        append_text(line, sizeof(line), pos, " state=");
        append_text(line, sizeof(line), pos, task_state_name(tasks[i].state));
        append_text(line, sizeof(line), pos, " target_core=");
        append_i64(line, sizeof(line), pos, tasks[i].target_core);
        append_text(line, sizeof(line), pos, " current_core=");
        append_i64(line, sizeof(line), pos, tasks[i].current_core);
        append_text(line, sizeof(line), pos, " running=");
        append_u64(line, sizeof(line), pos, static_cast<uint64_t>(tasks[i].running));
        append_text(line, sizeof(line), pos, " finished=");
        append_u64(line, sizeof(line), pos, static_cast<uint64_t>(tasks[i].finished));
        write_line(write, write_context, line);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_cores(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_core_info_t info{};
    const rad_status_t status = rad_core_info_get(&info);
    if (status != RAD_STATUS_OK) return status;
    char line[128];
    size_t pos = 0;
    line[0] = '\0';
    append_text(line, sizeof(line), pos, "detected=");
    append_u64(line, sizeof(line), pos, info.detected_cores);
    append_text(line, sizeof(line), pos, " service_core=");
    append_u64(line, sizeof(line), pos, info.service_core);
    append_text(line, sizeof(line), pos, " worker_cores=");
    append_u64(line, sizeof(line), pos, info.worker_cores);
    append_text(line, sizeof(line), pos, " worker_mask=0x");
    append_hex32(line, sizeof(line), pos, info.worker_running_mask);
    write_line(write, write_context, line);
    return RAD_STATUS_OK;
}

rad_status_t cmd_sched(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    rad_scheduler_info_t info{};
    const rad_status_t status = rad_scheduler_info_get(&info);
    if (status != RAD_STATUS_OK) return status;
    char line[256];
    size_t pos = 0;
    append_text(line, sizeof(line), pos, "mode=");
    append_text(line, sizeof(line), pos, info.mode == RAD_SCHEDULER_PREEMPTIVE ? "preemptive" : "cooperative");
    append_text(line, sizeof(line), pos, " arch=");
    append_text(line, sizeof(line), pos, info.arch);
    append_text(line, sizeof(line), pos, " cores=");
    append_u64(line, sizeof(line), pos, info.detected_cores);
    append_text(line, sizeof(line), pos, " workers=0x");
    append_hex32(line, sizeof(line), pos, info.worker_running_mask);
    append_text(line, sizeof(line), pos, " online=0x");
    append_hex32(line, sizeof(line), pos, info.online_core_mask);
    append_text(line, sizeof(line), pos, " current=");
    append_u64(line, sizeof(line), pos, info.current_core);
    append_text(line, sizeof(line), pos, " preempt=");
    append_u64(line, sizeof(line), pos, static_cast<uint64_t>(info.preemption_enabled));
    write_line(write, write_context, line);

    pos = 0;
    line[0] = '\0';
    append_text(line, sizeof(line), pos, "threads running=");
    append_u64(line, sizeof(line), pos, info.running_threads);
    append_text(line, sizeof(line), pos, " ready=");
    append_u64(line, sizeof(line), pos, info.ready_threads);
    append_text(line, sizeof(line), pos, " blocked=");
    append_u64(line, sizeof(line), pos, info.blocked_threads);
    append_text(line, sizeof(line), pos, " sleeping=");
    append_u64(line, sizeof(line), pos, info.sleeping_threads);
    append_text(line, sizeof(line), pos, " exited=");
    append_u64(line, sizeof(line), pos, info.exited_threads);
    append_text(line, sizeof(line), pos, " processes=");
    append_u64(line, sizeof(line), pos, info.process_count);
    append_text(line, sizeof(line), pos, " switches=");
    append_u64(line, sizeof(line), pos, info.context_switches);
    append_text(line, sizeof(line), pos, " ticks=");
    append_u64(line, sizeof(line), pos, info.scheduler_ticks);
    write_line(write, write_context, line);
    return RAD_STATUS_OK;
}

rad_status_t cmd_programs(int, const char**, rad_terminal_write_t write, void *write_context, void*) {
    char line[192];
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PROGRAMS; ++i) {
        if (!g_state.programs[i].used) continue;
        size_t pos = 0;
        line[0] = '\0';
        append_text(line, sizeof(line), pos, "#");
        append_u64(line, sizeof(line), pos, g_state.programs[i].id);
        append_text(line, sizeof(line), pos, " ");
        append_text(line, sizeof(line), pos, g_state.programs[i].name);
        append_text(line, sizeof(line), pos, " state=");
        append_text(line, sizeof(line), pos, program_state_name(g_state.programs[i].state));
        append_text(line, sizeof(line), pos, " task=");
        append_u64(line, sizeof(line), pos, g_state.programs[i].task ? g_state.programs[i].task->id : 0);
        write_line(write, write_context, line);
    }
    return RAD_STATUS_OK;
}

rad_status_t cmd_run(int argc, const char **argv, rad_terminal_write_t write, void *write_context, void*) {
    if (argc < 2) return RAD_STATUS_INVALID_ARGUMENT;
    rad_program_t program = nullptr;
    rad_status_t status = rad_program_load(argv[1], &program);
    if (status != RAD_STATUS_OK) return status;
    rad_task_t task = nullptr;
    status = rad_program_spawn(program, argc - 2, argc > 2 ? &argv[2] : nullptr, &task);
    if (status == RAD_STATUS_OK) {
        write_line(write, write_context, "program scheduled");
    }
    return status;
}

bool task_matches_core(const rad_task_handle& task, uint32_t core) {
    if (!task.entry || task.finished || task.running) return false;
    if (task.state == RAD_TASK_SLEEPING && task.wake_micros > hal_now()) return false;
    if (task.target_core == RAD_TASK_CORE_ANY) {
        return g_state.worker_running_mask ? core != RAD_TASK_CORE_SERVICE : true;
    }
    return task.target_core == static_cast<int>(core);
}

rad_task_handle *find_task_by_id(uint64_t id) {
    if (!id) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TASKS; ++i) {
        if (g_state.tasks[i].entry && g_state.tasks[i].id == id) return &g_state.tasks[i];
    }
    return nullptr;
}

size_t effective_task_stack_size(const rad_task_config_t *config) {
    size_t size = config && config->stack_size ? config->stack_size : static_cast<size_t>(RADIX_KERNEL_TASK_STACK_BYTES);
    if (size < 4096u) size = 4096u;
    return (size + 15u) & ~static_cast<size_t>(15u);
}

uint8_t *align_stack_base(uint8_t *base) {
    const uintptr_t value = reinterpret_cast<uintptr_t>(base);
    return reinterpret_cast<uint8_t*>((value + 15u) & ~static_cast<uintptr_t>(15u));
}

uint8_t *task_stack_base(rad_task_handle& task) {
#if RADIX_KERNEL_TASK_DYNAMIC_STACKS
    return task.arch_stack;
#else
    return task.arch_stack;
#endif
}

const uint8_t *task_stack_base(const rad_task_handle& task) {
#if RADIX_KERNEL_TASK_DYNAMIC_STACKS
    return task.arch_stack;
#else
    return task.arch_stack;
#endif
}

size_t task_stack_capacity(const rad_task_handle& task) {
#if RADIX_KERNEL_TASK_DYNAMIC_STACKS
    return task.arch_stack_bytes;
#else
    (void)task;
    return sizeof(task.arch_stack);
#endif
}

void *task_stack_top(rad_task_handle& task) {
    uint8_t *base = task_stack_base(task);
    const size_t capacity = task_stack_capacity(task);
    uintptr_t top = reinterpret_cast<uintptr_t>(base + capacity);
    top &= ~static_cast<uintptr_t>(15u);
    return reinterpret_cast<void*>(top);
}

#if RADIX_KERNEL_TASK_DYNAMIC_STACKS
bool allocate_task_stack(size_t size, uint8_t **allocation, uint8_t **stack_base) {
    if (!allocation || !stack_base) return false;
    *allocation = static_cast<uint8_t*>(malloc(size + 16u));
    if (!*allocation) return false;
    *stack_base = align_stack_base(*allocation);
    return true;
}

void release_task_stack(rad_task_handle& task) {
    if (task.arch_stack_allocation) {
        free(task.arch_stack_allocation);
        task.arch_stack_allocation = nullptr;
        task.arch_stack = nullptr;
        task.arch_stack_bytes = 0;
    }
}
#else
void release_task_stack(rad_task_handle&) {}
#endif

void initialize_task_stack_guard(rad_task_handle& task) {
    uint8_t *base = task_stack_base(task);
    const size_t capacity = task_stack_capacity(task);
    if (base && capacity) memset(base, TaskStackCanary, capacity);
}

void check_task_stack_guard(const rad_task_handle& task) {
    const uint8_t *base = task_stack_base(task);
    const size_t guard = task_stack_capacity(task) < TaskStackGuardBytes ? task_stack_capacity(task) : TaskStackGuardBytes;
    for (size_t i = 0; base && i < guard; ++i) {
        if (base[i] == TaskStackCanary) continue;
        rad_kprintk(RKERN_WARN, "RADIX_TASK_STACK_LOW task=%s id=%llu\n",
            task.name,
            static_cast<unsigned long long>(task.id));
        return;
    }
}

void task_context_entry(void) {
    const uint32_t core = hal_current_core();
    const uint64_t task_id = g_current_task_id[core];
    rad_task_handle *task = find_task_by_id(task_id);
    if (task && task->entry) task->entry(task->context);

    lock_tasks();
    if (task) {
        task->finished = 1;
        task->state = task->detached ? RAD_TASK_DETACHED : RAD_TASK_FINISHED;
    }
    g_current_task_id[core] = 0;
    g_current_pid[core] = 0;
    unlock_tasks();
    if (rad_hal_worker_wake) rad_hal_worker_wake();
    rad_arch_task_context_switch(task ? task->arch_context : g_scheduler_context[core], g_scheduler_context[core]);
    rad_cpu_halt_forever();
}

bool run_one_task_on_core(uint32_t core) {
    rad_task_handle *selected = nullptr;
    size_t selected_index = 0;
    lock_tasks();
    const size_t start = (static_cast<size_t>(g_last_task_index[core]) + 1u) % RADIX_KERNEL_MAX_TASKS;
    for (size_t offset = 0; offset < RADIX_KERNEL_MAX_TASKS; ++offset) {
        const size_t i = (start + offset) % RADIX_KERNEL_MAX_TASKS;
        rad_task_handle& task = g_state.tasks[i];
        if (!task_matches_core(task, core)) continue;
        selected = &task;
        selected_index = i;
        selected->running = 1;
        selected->state = RAD_TASK_RUNNING;
        selected->current_core = static_cast<int>(core);
        if (selected->preempt_pinned && selected->target_core == static_cast<int>(core)) {
            selected->target_core = selected->preempt_saved_target_core;
            selected->preempt_pinned = 0;
            selected->preempt_saved_target_core = RAD_TASK_CORE_ANY;
        }
        g_current_task_id[core] = selected->id;
        g_current_pid[core] = selected->process_pid >= 0 ? selected->process_pid : 0;
        g_last_task_index[core] = static_cast<uint32_t>(selected_index);
        break;
    }
    unlock_tasks();

    if (!selected) return false;
    __atomic_fetch_add(&g_state.context_switches, 1u, __ATOMIC_RELAXED);
    if (core > 0 && !__atomic_exchange_n(&g_ap_worker_task_seen, 1, __ATOMIC_ACQ_REL)) {
        rad_debug_marker("RADIX_AP_WORKER_TASK_OK");
    }
    if (arch_context_enabled()) {
        if (!selected->arch_context_ready) {
            memset(selected->arch_context, 0, sizeof(selected->arch_context));
            rad_arch_task_context_init(selected->arch_context,
                task_stack_top(*selected),
                task_context_entry);
            selected->arch_context_ready = 1;
        }
        if (rad_arch_task_context_resumed) rad_arch_task_context_resumed(selected->user_context);
        if (!__atomic_exchange_n(&g_context_dispatch_seen, 1, __ATOMIC_ACQ_REL)) {
            rad_debug_marker("RADIX_CONTEXT_DISPATCH_OK");
        }
        rad_arch_task_context_switch(g_scheduler_context[core], selected->arch_context);
        check_task_stack_guard(*selected);
        if (selected->finished && selected->running) {
            lock_tasks();
            if (selected->finished && selected->running) {
                selected->running = 0;
                selected->current_core = RAD_TASK_CORE_ANY;
            }
            unlock_tasks();
        }
        return true;
    }

    selected->entry(selected->context);
    lock_tasks();
    selected->running = 0;
    selected->finished = 1;
    selected->state = selected->detached ? RAD_TASK_DETACHED : RAD_TASK_FINISHED;
    selected->current_core = RAD_TASK_CORE_ANY;
    g_current_task_id[core] = 0;
    unlock_tasks();
    if (rad_hal_worker_wake) rad_hal_worker_wake();
    return true;
}

void worker_loop(uint32_t core) {
    if (core >= RADIX_KERNEL_MAX_CORES) return;
    while (g_state.initialized && !g_state.shutdown_requested) {
        if (!run_one_task_on_core(core)) {
            if (rad_hal_worker_wait) {
                rad_hal_worker_wait();
            } else {
                hal_sleep(1000u);
            }
        }
    }
}

void start_workers() {
    g_state.detected_cores = hal_core_count();
    g_state.worker_running_mask = 0;
    if (!rad_hal_start_worker_core) return;
    for (uint32_t core = 1; core < g_state.detected_cores; ++core) {
        if (rad_hal_start_worker_core(core, worker_loop) == RAD_STATUS_OK) {
            g_state.worker_running_mask |= (1u << core);
        }
    }
}

void register_builtins() {
    rad_terminal_register_command("help", "List terminal commands", cmd_help, nullptr);
    rad_terminal_register_command("tasks", "List kernel tasks", cmd_tasks, nullptr);
    rad_terminal_register_command("cores", "Show detected cores and workers", cmd_cores, nullptr);
    rad_terminal_register_command("sched", "Show scheduler backend and thread state", cmd_sched, nullptr);
    rad_terminal_register_command("mem", "Show memory counters", cmd_mem, nullptr);
    rad_terminal_register_command("devices", "List registered devices", cmd_devices, nullptr);
    rad_terminal_register_command("irq", "List registered IRQ lines", cmd_irq, nullptr);
    rad_terminal_register_command("time", "Show monotonic kernel time", cmd_time, nullptr);
    rad_terminal_register_command("perf", "List runtime performance counters", cmd_perf, nullptr);
    rad_terminal_register_command("latency", "Show latency-sensitive runtime counters", cmd_latency, nullptr);
    rad_terminal_register_command("mounts", "Show VFS mounts", cmd_mounts, nullptr);
    rad_terminal_register_command("services", "List runtime services", cmd_services, nullptr);
    rad_terminal_register_command("dmesg", "Print kernel log ring", cmd_dmesg, nullptr);
    rad_terminal_register_command("initctl", "Inspect radinit services", cmd_initctl, nullptr);
    rad_terminal_register_command("logread", "Read RADix text logs", cmd_logread, nullptr);
    rad_terminal_register_command("pwd", "Print current directory", cmd_pwd, nullptr);
    rad_terminal_register_command("ls", "List a VFS directory", cmd_ls, nullptr);
    rad_terminal_register_command("cd", "Change current directory", cmd_cd, nullptr);
    rad_terminal_register_command("mkdir", "Create directories", cmd_mkdir, nullptr);
    rad_terminal_register_command("rmdir", "Remove empty directories", cmd_rmdir, nullptr);
    rad_terminal_register_command("touch", "Create files", cmd_touch, nullptr);
    rad_terminal_register_command("cat", "Print files", cmd_cat, nullptr);
    rad_terminal_register_command("echo", "Print arguments", cmd_echo, nullptr);
    rad_terminal_register_command("rm", "Remove files", cmd_rm, nullptr);
    rad_terminal_register_command("mv", "Rename a file", cmd_mv, nullptr);
    rad_terminal_register_command("cp", "Copy a file", cmd_cp, nullptr);
    rad_terminal_register_command("head", "Print first file lines", cmd_head, nullptr);
    rad_terminal_register_command("tail", "Print last file lines", cmd_tail, nullptr);
    rad_terminal_register_command("wc", "Count file lines words bytes", cmd_wc, nullptr);
    rad_terminal_register_command("basename", "Print final path component", cmd_basename, nullptr);
    rad_terminal_register_command("dirname", "Print parent path", cmd_dirname, nullptr);
    rad_terminal_register_command("env", "Print environment", cmd_env, nullptr);
    rad_terminal_register_command("true", "Return success", cmd_true, nullptr);
    rad_terminal_register_command("false", "Return failure", cmd_false, nullptr);
    rad_terminal_register_command("exit", "Exit shell", cmd_exit, nullptr);
    rad_terminal_register_command("bootinfo", "Show boot handoff information", cmd_bootinfo, nullptr);
    rad_terminal_register_command("programs", "List loaded programs", cmd_programs, nullptr);
    rad_terminal_register_command("run", "Run a program from VFS", cmd_run, nullptr);
    rad_overlay_register_terminal_commands();
}
}

extern "C" {

rad_status_t rad_net_stack_info(rad_net_stack_info_t *info) {
    return net_stack_info_internal(info);
}

rad_status_t rad_net_configure(const rad_net_stack_config_t *config) {
    return net_configure_internal(config);
}

rad_status_t rad_net_ntp_status(rad_ntp_status_t *status) {
    return net_ntp_status_internal(status);
}

rad_status_t rad_net_ntp_query(rad_ntp_query_t *query) {
    return net_ntp_query_internal(query);
}

rad_status_t rad_kernel_init(const rad_kernel_config_t *config) {
    memset(&g_state, 0, sizeof(g_state));
    memset(g_current_task_id, 0, sizeof(g_current_task_id));
    memset(g_scheduler_context, 0, sizeof(g_scheduler_context));
    for (size_t i = 0; i < RADIX_KERNEL_MAX_CORES; ++i) g_scheduler_in_irq[i] = 0;
    memset(g_last_task_index, 0xff, sizeof(g_last_task_index));
    g_state.initialized = 1;
    g_state.next_task_id = 1;
    g_state.next_program_id = 1;
    g_state.preemption_enabled = 1;
    g_state.start_micros = hal_now();
    initialize_net_config();
    g_state.detected_cores = hal_core_count();
    copy_string(g_state.cwd, sizeof(g_state.cwd), "/");
    copy_string(g_state.backend, sizeof(g_state.backend), config && config->backend_name ? config->backend_name : "embedded");
    if (config && config->boot_info) {
        g_state.boot = *static_cast<const rad_boot_info_t*>(config->boot_info);
    } else {
        g_state.boot.size = sizeof(rad_boot_info_t);
        g_state.boot.version = 1;
        copy_string(g_state.boot.backend, sizeof(g_state.boot.backend), g_state.backend);
        copy_string(g_state.boot.board, sizeof(g_state.boot.board), "unknown");
        copy_string(g_state.boot.console_device, sizeof(g_state.boot.console_device), "/dev/console");
        copy_string(g_state.boot.sd_mode, sizeof(g_state.boot.sd_mode), "auto");
    }
    g_state.has_boot = 1;
    rad_overlay_reset();
    register_tty_device("/dev/console");
    register_tty_device("/dev/tty0");
    init_process_table();
    initialize_stdio_fds();
    register_builtins();
    if (rad_hal_register_default_devices) rad_hal_register_default_devices();
    rad_overlay_apply_boot();
    start_workers();
    return RAD_STATUS_OK;
}

void rad_kernel_shutdown(void) {
    g_state.shutdown_requested = 1;
    if (rad_hal_worker_wake) rad_hal_worker_wake();
    exit_modules_reverse();
    g_state.initialized = 0;
}

int rad_kernel_is_initialized(void) {
    return g_state.initialized;
}

const char *rad_kernel_backend_name(void) {
    return g_state.backend;
}

const char *rad_kernel_version_string(void) {
    return "0.1.3";
}

rad_log_level_t kernel_print_to_log_level(rad_kernel_print_level_t level) {
    switch (level) {
    case RKERN_DBG: return RAD_LOG_DEBUG;
    case RKERN_WARN: return RAD_LOG_WARNING;
    case RKERN_ERR: return RAD_LOG_ERROR;
    case RKERN_STAT:
    default: return RAD_LOG_INFO;
    }
}

const char *kernel_print_prefix(rad_kernel_print_level_t level) {
    switch (level) {
    case RKERN_DBG: return "[DBG] ";
    case RKERN_WARN: return "[WARN] ";
    case RKERN_ERR: return "[ERR] ";
    case RKERN_STAT:
    default: return "";
    }
}

int rad_vkprintk(rad_kernel_print_level_t level, const char *format, va_list args) {
    if (!format) return 0;
    char buffer[512];
    size_t pos = 0;
    buffer[0] = '\0';
    append_text(buffer, sizeof(buffer), pos, kernel_print_prefix(level));
    int logical_written = 0;
    for (const char *cursor = format; *cursor; ++cursor) {
        if (*cursor != '%') {
            append_char(buffer, sizeof(buffer), pos, *cursor);
            ++logical_written;
            continue;
        }
        ++cursor;
        if (!*cursor) break;
        int long_count = 0;
        int size_t_arg = 0;
        if (*cursor == 'z') {
            size_t_arg = 1;
            ++cursor;
        } else {
            while (*cursor == 'l' && long_count < 2) {
                ++long_count;
                ++cursor;
            }
        }
        switch (*cursor) {
        case '%':
            append_char(buffer, sizeof(buffer), pos, '%');
            ++logical_written;
            break;
        case 'c': {
            const char ch = static_cast<char>(va_arg(args, int));
            append_char(buffer, sizeof(buffer), pos, ch);
            ++logical_written;
            break;
        }
        case 's': {
            const char *text = va_arg(args, const char*);
            const size_t before = pos;
            append_text(buffer, sizeof(buffer), pos, text ? text : "(null)");
            logical_written += static_cast<int>(pos - before);
            break;
        }
        case 'd':
        case 'i': {
            int64_t value = 0;
            if (size_t_arg) value = static_cast<int64_t>(va_arg(args, size_t));
            else if (long_count >= 2) value = va_arg(args, long long);
            else if (long_count == 1) value = va_arg(args, long);
            else value = va_arg(args, int);
            const size_t before = pos;
            append_i64(buffer, sizeof(buffer), pos, value);
            logical_written += static_cast<int>(pos - before);
            break;
        }
        case 'u': {
            uint64_t value = 0;
            if (size_t_arg) value = static_cast<uint64_t>(va_arg(args, size_t));
            else if (long_count >= 2) value = va_arg(args, unsigned long long);
            else if (long_count == 1) value = va_arg(args, unsigned long);
            else value = va_arg(args, unsigned int);
            const size_t before = pos;
            append_u64(buffer, sizeof(buffer), pos, value);
            logical_written += static_cast<int>(pos - before);
            break;
        }
        case 'x':
        case 'X': {
            uint64_t value = 0;
            if (size_t_arg) value = static_cast<uint64_t>(va_arg(args, size_t));
            else if (long_count >= 2) value = va_arg(args, unsigned long long);
            else if (long_count == 1) value = va_arg(args, unsigned long);
            else value = va_arg(args, unsigned int);
            const size_t before = pos;
            append_hex64(buffer, sizeof(buffer), pos, value);
            logical_written += static_cast<int>(pos - before);
            break;
        }
        case 'p': {
            const uintptr_t value = reinterpret_cast<uintptr_t>(va_arg(args, void*));
            const size_t before = pos;
            append_text(buffer, sizeof(buffer), pos, "0x");
            append_hex64(buffer, sizeof(buffer), pos, value);
            logical_written += static_cast<int>(pos - before);
            break;
        }
        default:
            append_char(buffer, sizeof(buffer), pos, '%');
            append_char(buffer, sizeof(buffer), pos, *cursor);
            logical_written += 2;
            break;
        }
    }
    if (pos > 0) {
        rad_log_write(kernel_print_to_log_level(level), "kernel", buffer);
        if (rad_hal_console_write) rad_hal_console_write(buffer);
    }
    return logical_written;
}

int rad_vprintk(const char *format, va_list args) {
    return rad_vkprintk(RKERN_STAT, format, args);
}

int rad_printk(const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int written = rad_vprintk(format, args);
    va_end(args);
    return written;
}

int rad_kprintk(rad_kernel_print_level_t level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int written = rad_vkprintk(level, format, args);
    va_end(args);
    return written;
}

int rad_early_vprintk(const char *format, va_list args) {
    if (!format) return 0;
    char buffer[512];
    size_t pos = 0;
    buffer[0] = '\0';
    int logical_written = 0;
    for (const char *cursor = format; *cursor; ++cursor) {
        if (*cursor != '%') {
            append_char(buffer, sizeof(buffer), pos, *cursor);
            ++logical_written;
            continue;
        }
        ++cursor;
        if (!*cursor) break;
        int long_count = 0;
        int size_t_arg = 0;
        if (*cursor == 'z') {
            size_t_arg = 1;
            ++cursor;
        } else {
            while (*cursor == 'l' && long_count < 2) {
                ++long_count;
                ++cursor;
            }
        }
        switch (*cursor) {
        case '%': append_char(buffer, sizeof(buffer), pos, '%'); ++logical_written; break;
        case 'c': {
            const char ch = static_cast<char>(va_arg(args, int));
            append_char(buffer, sizeof(buffer), pos, ch);
            ++logical_written;
            break;
        }
        case 's': {
            const char *text = va_arg(args, const char*);
            const size_t before = pos;
            append_text(buffer, sizeof(buffer), pos, text ? text : "(null)");
            logical_written += static_cast<int>(pos - before);
            break;
        }
        case 'd':
        case 'i': {
            int64_t value = 0;
            if (size_t_arg) value = static_cast<int64_t>(va_arg(args, size_t));
            else if (long_count >= 2) value = va_arg(args, long long);
            else if (long_count == 1) value = va_arg(args, long);
            else value = va_arg(args, int);
            const size_t before = pos;
            append_i64(buffer, sizeof(buffer), pos, value);
            logical_written += static_cast<int>(pos - before);
            break;
        }
        case 'u': {
            uint64_t value = 0;
            if (size_t_arg) value = static_cast<uint64_t>(va_arg(args, size_t));
            else if (long_count >= 2) value = va_arg(args, unsigned long long);
            else if (long_count == 1) value = va_arg(args, unsigned long);
            else value = va_arg(args, unsigned int);
            const size_t before = pos;
            append_u64(buffer, sizeof(buffer), pos, value);
            logical_written += static_cast<int>(pos - before);
            break;
        }
        case 'x':
        case 'X': {
            uint64_t value = 0;
            if (size_t_arg) value = static_cast<uint64_t>(va_arg(args, size_t));
            else if (long_count >= 2) value = va_arg(args, unsigned long long);
            else if (long_count == 1) value = va_arg(args, unsigned long);
            else value = va_arg(args, unsigned int);
            const size_t before = pos;
            append_hex64(buffer, sizeof(buffer), pos, value);
            logical_written += static_cast<int>(pos - before);
            break;
        }
        case 'p': {
            const uintptr_t value = reinterpret_cast<uintptr_t>(va_arg(args, void*));
            const size_t before = pos;
            append_text(buffer, sizeof(buffer), pos, "0x");
            append_hex64(buffer, sizeof(buffer), pos, value);
            logical_written += static_cast<int>(pos - before);
            break;
        }
        default:
            append_char(buffer, sizeof(buffer), pos, '%');
            append_char(buffer, sizeof(buffer), pos, *cursor);
            logical_written += 2;
            break;
        }
    }
    if (pos > 0) {
        rad_log_write(RAD_LOG_INFO, "kernel", buffer);
        if (rad_hal_early_console_write) rad_hal_early_console_write(buffer);
        else if (rad_hal_console_write) rad_hal_console_write(buffer);
    }
    return logical_written;
}

int rad_early_printk(const char *format, ...) {
    va_list args;
    va_start(args, format);
    const int written = rad_early_vprintk(format, args);
    va_end(args);
    return written;
}

void rad_debug_marker(const char *marker) {
    if (!marker || !*marker) return;
    if (g_state.initialized) rad_printk("%s%s", marker, strchr(marker, '\n') ? "" : "\n");
    else rad_early_printk("%s%s", marker, strchr(marker, '\n') ? "" : "\n");
}

rad_status_t rad_log_write(rad_log_level_t level, const char *category, const char *message) {
    if (!message) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t sequence = __atomic_add_fetch(&g_state.next_log_sequence, 1u, __ATOMIC_RELAXED);
    LogRecord& record = g_state.logs[sequence % RADIX_KERNEL_MAX_LOG_ENTRIES];
    memset(&record, 0, sizeof(record));
    record.used = 1;
    record.entry.sequence = sequence;
    record.entry.time_millis = rad_time_millis();
    record.entry.level = level;
    copy_string(record.entry.category, sizeof(record.entry.category), category && *category ? category : "kernel");
    copy_string(record.entry.message, sizeof(record.entry.message), message);
    return RAD_STATUS_OK;
}

size_t rad_log_read(rad_log_entry_t *entries, size_t capacity, uint64_t after_sequence) {
    size_t count = 0;
    const uint64_t next = g_state.next_log_sequence;
    const uint64_t first = next > RADIX_KERNEL_MAX_LOG_ENTRIES ? next - RADIX_KERNEL_MAX_LOG_ENTRIES + 1u : 1u;
    for (uint64_t sequence = first; sequence <= next; ++sequence) {
        const LogRecord& record = g_state.logs[sequence % RADIX_KERNEL_MAX_LOG_ENTRIES];
        if (!record.used || record.entry.sequence != sequence || sequence <= after_sequence) continue;
        if (entries && count < capacity) entries[count] = record.entry;
        ++count;
    }
    return count;
}

rad_status_t rad_log_flush_to_path(const char *path) {
    if (!path || !*path) return RAD_STATUS_INVALID_ARGUMENT;
    rad_file_t file = nullptr;
    rad_status_t status = rad_vfs_open(path, RAD_VFS_WRITE | RAD_VFS_CREATE | RAD_VFS_TRUNCATE, &file);
    if (status != RAD_STATUS_OK) return status;
    rad_log_entry_t entries[16]{};
    uint64_t after = 0;
    for (;;) {
        const size_t count = rad_log_read(entries, 16, after);
        if (!count) break;
        for (size_t i = 0; i < count && i < 16; ++i) {
            after = entries[i].sequence;
            char line[320];
            size_t pos = 0;
            line[0] = '\0';
            append_u64(line, sizeof(line), pos, entries[i].time_millis);
            append_text(line, sizeof(line), pos, "ms [");
            append_text(line, sizeof(line), pos, rad_log_level_name(entries[i].level));
            append_text(line, sizeof(line), pos, "] [");
            append_text(line, sizeof(line), pos, entries[i].category);
            append_text(line, sizeof(line), pos, "] ");
            append_text(line, sizeof(line), pos, entries[i].message);
            if (pos == 0 || line[pos - 1u] != '\n') append_char(line, sizeof(line), pos, '\n');
            size_t written = 0;
            status = rad_vfs_write(file, line, pos, &written);
            if (status != RAD_STATUS_OK || written != pos) {
                rad_vfs_close(file);
                return status == RAD_STATUS_OK ? RAD_STATUS_ERROR : status;
            }
        }
    }
    rad_vfs_fsync(file);
    rad_vfs_close(file);
    return RAD_STATUS_OK;
}

rad_status_t rad_cpu_interrupts_enable(void) {
    return rad_hal_interrupts_enable ? rad_hal_interrupts_enable() : RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t rad_cpu_interrupts_disable(void) {
    return rad_hal_interrupts_disable ? rad_hal_interrupts_disable() : RAD_STATUS_NOT_SUPPORTED;
}

int rad_cpu_interrupts_enabled(void) {
    return rad_hal_interrupts_enabled ? rad_hal_interrupts_enabled() : 0;
}

void rad_cpu_idle(void) {
    if (rad_hal_cpu_idle) {
        rad_hal_cpu_idle();
        return;
    }
    hal_sleep(0);
}

void rad_cpu_halt_forever(void) {
    if (rad_hal_cpu_halt_forever) rad_hal_cpu_halt_forever();
    for (;;) rad_cpu_idle();
}

rad_status_t rad_kernel_poll(void) {
    if (!g_state.initialized) return RAD_STATUS_NOT_INITIALIZED;
    rad_work_poll(64, nullptr);
    rad_service_poll_all();
    for (uint32_t i = 0; i < 16u && run_one_task_on_core(RAD_TASK_CORE_SERVICE); ++i) {}
    rad_work_poll(64, nullptr);
    rad_terminal_poll_attached();
    return RAD_STATUS_OK;
}

rad_status_t rad_kernel_run(void) {
    if (!g_state.initialized) return RAD_STATUS_NOT_INITIALIZED;
    while (!g_state.shutdown_requested) {
        rad_kernel_poll();
        rad_sleep_ms(1);
    }
    return RAD_STATUS_OK;
}

void rad_kernel_request_shutdown(void) {
    g_state.shutdown_requested = 1;
}

int rad_kernel_is_shutdown_requested(void) {
    return g_state.shutdown_requested;
}

rad_status_t rad_boot_info_set(const rad_boot_info_t *boot_info) {
    if (!boot_info) return RAD_STATUS_INVALID_ARGUMENT;
    g_state.boot = *boot_info;
    g_state.boot.size = sizeof(rad_boot_info_t);
    if (g_state.boot.version == 0) g_state.boot.version = 1;
    g_state.has_boot = 1;
    return RAD_STATUS_OK;
}

rad_status_t rad_boot_info_get(rad_boot_info_t *boot_info) {
    if (!boot_info) return RAD_STATUS_INVALID_ARGUMENT;
    if (!g_state.has_boot) return RAD_STATUS_NOT_FOUND;
    *boot_info = g_state.boot;
    return RAD_STATUS_OK;
}

const char *rad_boot_arg_get(const char *key) {
    if (!key || !g_state.has_boot) return nullptr;
    for (uint32_t i = 0; i < g_state.boot.arg_count && i < RAD_BOOT_MAX_ARGS; ++i) {
        if (strcmp(g_state.boot.args[i].key, key) == 0) return g_state.boot.args[i].value;
    }
    return nullptr;
}

uint64_t rad_time_micros(void) {
    const uint64_t now = hal_now();
    return now >= g_state.start_micros ? now - g_state.start_micros : now;
}

uint64_t rad_time_millis(void) {
    return rad_time_micros() / 1000u;
}

uint64_t rad_realtime_micros(void) {
    const uint64_t monotonic = rad_time_micros();
    if (g_state.realtime_offset_micros < 0) {
        const uint64_t offset = static_cast<uint64_t>(-g_state.realtime_offset_micros);
        return monotonic > offset ? monotonic - offset : 0;
    }
    return monotonic + static_cast<uint64_t>(g_state.realtime_offset_micros);
}

rad_status_t rad_realtime_set_micros(uint64_t unix_micros) {
    if (current_credentials().euid != 0) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t monotonic = rad_time_micros();
    g_state.realtime_offset_micros = unix_micros >= monotonic
        ? static_cast<int64_t>(unix_micros - monotonic)
        : -static_cast<int64_t>(monotonic - unix_micros);
    return RAD_STATUS_OK;
}

void rad_sleep_ms(uint32_t milliseconds) {
    const uint32_t core = hal_current_core();
    if (arch_context_enabled() && g_current_task_id[core]) {
        rad_task_sleep_us(milliseconds * 1000u);
        return;
    }
    hal_sleep(milliseconds * 1000u);
}

void rad_sleep_us(uint32_t microseconds) {
    const uint32_t core = hal_current_core();
    if (arch_context_enabled() && g_current_task_id[core]) {
        rad_task_sleep_us(microseconds);
        return;
    }
    hal_sleep(microseconds);
}

uint64_t rad_perf_now_cycles(void) {
    return hal_now();
}

void rad_perf_counter_add(const char *name, uint64_t delta) {
    if (!name || !*name) return;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PERF_COUNTERS; ++i) {
        PerfCounterRecord& counter = g_state.perf_counters[i];
        if (counter.used && strncmp(counter.name, name, RAD_PERF_NAME_MAX) == 0) {
            __atomic_fetch_add(&counter.value, delta, __ATOMIC_RELAXED);
            return;
        }
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PERF_COUNTERS; ++i) {
        PerfCounterRecord& counter = g_state.perf_counters[i];
        int expected = 0;
        if (!__atomic_compare_exchange_n(&counter.used, &expected, 1, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) continue;
        copy_string(counter.name, sizeof(counter.name), name);
        __atomic_store_n(&counter.value, delta, __ATOMIC_RELAXED);
        return;
    }
}

size_t rad_perf_counter_list(rad_perf_counter_info_t *counters, size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PERF_COUNTERS; ++i) {
        const PerfCounterRecord& counter = g_state.perf_counters[i];
        if (!counter.used) continue;
        if (counters && count < capacity) {
            copy_string(counters[count].name, sizeof(counters[count].name), counter.name);
            counters[count].value = __atomic_load_n(&counter.value, __ATOMIC_RELAXED);
        }
        ++count;
    }
    return count;
}

rad_status_t rad_work_submit(const char *name, rad_work_handler_t handler, void *context) {
    if (!handler) return RAD_STATUS_INVALID_ARGUMENT;
    if (!g_state.initialized) return RAD_STATUS_NOT_INITIALIZED;
    lock_runtime();
    if (g_state.work_count >= RADIX_KERNEL_MAX_WORK_ITEMS) {
        unlock_runtime();
        return RAD_STATUS_NO_MEMORY;
    }
    WorkItem& item = g_state.work_items[g_state.work_tail];
    memset(&item, 0, sizeof(item));
    item.used = 1;
    item.handler = handler;
    item.context = context;
    copy_string(item.name, sizeof(item.name), name ? name : "work");
    g_state.work_tail = (g_state.work_tail + 1u) % RADIX_KERNEL_MAX_WORK_ITEMS;
    ++g_state.work_count;
    unlock_runtime();
    rad_perf_counter_add("work.submitted", 1);
    if (rad_hal_worker_wake) rad_hal_worker_wake();
    return RAD_STATUS_OK;
}

rad_status_t rad_work_poll(size_t budget, size_t *ran) {
    if (!g_state.initialized) return RAD_STATUS_NOT_INITIALIZED;
    if (budget == 0) budget = RADIX_KERNEL_MAX_WORK_ITEMS;
    size_t count = 0;
    while (count < budget) {
        WorkItem item{};
        lock_runtime();
        if (g_state.work_count == 0) {
            unlock_runtime();
            break;
        }
        item = g_state.work_items[g_state.work_head];
        memset(&g_state.work_items[g_state.work_head], 0, sizeof(g_state.work_items[g_state.work_head]));
        g_state.work_head = (g_state.work_head + 1u) % RADIX_KERNEL_MAX_WORK_ITEMS;
        --g_state.work_count;
        unlock_runtime();
        if (item.handler) item.handler(item.context);
        ++count;
        rad_perf_counter_add("work.ran", 1);
    }
    if (ran) *ran = count;
    return RAD_STATUS_OK;
}

rad_status_t rad_wait_queue_create(rad_wait_queue_t *queue) {
    if (!queue) return RAD_STATUS_INVALID_ARGUMENT;
    lock_runtime();
    for (size_t i = 0; i < RADIX_KERNEL_MAX_WAIT_QUEUES; ++i) {
        if (g_state.wait_queues[i].used) continue;
        g_state.wait_queues[i].used = 1;
        g_state.wait_queues[i].signals = 0;
        g_state.wait_queue_handles[i].index = i;
        *queue = &g_state.wait_queue_handles[i];
        unlock_runtime();
        return RAD_STATUS_OK;
    }
    unlock_runtime();
    return RAD_STATUS_NO_MEMORY;
}

void rad_wait_queue_destroy(rad_wait_queue_t queue) {
    if (!queue || queue->index >= RADIX_KERNEL_MAX_WAIT_QUEUES) return;
    lock_runtime();
    memset(&g_state.wait_queues[queue->index], 0, sizeof(g_state.wait_queues[queue->index]));
    memset(&g_state.wait_queue_handles[queue->index], 0, sizeof(g_state.wait_queue_handles[queue->index]));
    unlock_runtime();
}

rad_status_t rad_wait_queue_wait(rad_wait_queue_t queue, uint32_t timeout_ms) {
    if (!queue || queue->index >= RADIX_KERNEL_MAX_WAIT_QUEUES) return RAD_STATUS_INVALID_ARGUMENT;
    WaitQueueRecord& record = g_state.wait_queues[queue->index];
    if (!record.used) return RAD_STATUS_INVALID_ARGUMENT;
    rad_perf_counter_add("wait.queue.waits", 1);
    const uint64_t start = rad_time_millis();
    for (;;) {
        uint64_t signals = __atomic_load_n(&record.signals, __ATOMIC_ACQUIRE);
        while (signals > 0) {
            if (__atomic_compare_exchange_n(&record.signals, &signals, signals - 1, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
                return RAD_STATUS_OK;
            }
        }
        if (timeout_ms != RAD_WAIT_FOREVER && rad_time_millis() - start >= timeout_ms) {
            rad_perf_counter_add("wait.queue.timeouts", 1);
            return RAD_STATUS_TIMEOUT;
        }
        if (hal_current_core() == RAD_TASK_CORE_SERVICE) {
            rad_work_poll(8, nullptr);
            rad_terminal_poll_attached();
        }
        hal_sleep(1000u);
    }
}

rad_status_t rad_wait_queue_wake_one(rad_wait_queue_t queue) {
    if (!queue || queue->index >= RADIX_KERNEL_MAX_WAIT_QUEUES) return RAD_STATUS_INVALID_ARGUMENT;
    WaitQueueRecord& record = g_state.wait_queues[queue->index];
    if (!record.used) return RAD_STATUS_INVALID_ARGUMENT;
    __atomic_fetch_add(&record.signals, 1u, __ATOMIC_RELEASE);
    rad_perf_counter_add("wait.queue.wakes", 1);
    if (rad_hal_worker_wake) rad_hal_worker_wake();
    return RAD_STATUS_OK;
}

rad_status_t rad_wait_queue_wake_all(rad_wait_queue_t queue) {
    if (!queue || queue->index >= RADIX_KERNEL_MAX_WAIT_QUEUES) return RAD_STATUS_INVALID_ARGUMENT;
    WaitQueueRecord& record = g_state.wait_queues[queue->index];
    if (!record.used) return RAD_STATUS_INVALID_ARGUMENT;
    __atomic_fetch_add(&record.signals, static_cast<uint64_t>(RADIX_KERNEL_MAX_TASKS), __ATOMIC_RELEASE);
    rad_perf_counter_add("wait.queue.wakes", 1);
    if (rad_hal_worker_wake) rad_hal_worker_wake();
    return RAD_STATUS_OK;
}

rad_status_t rad_timer_source_register(const rad_timer_source_config_t *config, const rad_timer_source_ops_t *ops) {
    if (!config || !config->name || !*config->name) return RAD_STATUS_INVALID_ARGUMENT;
    lock_runtime();
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TIMER_SOURCES; ++i) {
        if (g_state.timers[i].used && strcmp(g_state.timers[i].name, config->name) == 0) {
            unlock_runtime();
            return RAD_STATUS_ALREADY_EXISTS;
        }
    }
    TimerSourceRecord *slot = nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TIMER_SOURCES; ++i) {
        if (!g_state.timers[i].used) {
            slot = &g_state.timers[i];
            break;
        }
    }
    if (!slot) {
        unlock_runtime();
        return RAD_STATUS_NO_MEMORY;
    }
    memset(slot, 0, sizeof(*slot));
    slot->used = 1;
    slot->active = 1;
    slot->frequency_hz = config->frequency_hz;
    slot->supports_oneshot = config->supports_oneshot;
    copy_string(slot->name, sizeof(slot->name), config->name);
    if (ops) slot->ops = *ops;
    unlock_runtime();
    if (slot->ops.start_periodic) {
        const rad_status_t status = slot->ops.start_periodic(slot->ops.context, slot->frequency_hz);
        if (status != RAD_STATUS_OK) {
            rad_timer_source_unregister(config->name);
            return status;
        }
    }
    rad_perf_counter_add("timer.sources", 1);
    return RAD_STATUS_OK;
}

rad_status_t rad_timer_source_unregister(const char *name) {
    if (!name) return RAD_STATUS_INVALID_ARGUMENT;
    lock_runtime();
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TIMER_SOURCES; ++i) {
        if (g_state.timers[i].used && strcmp(g_state.timers[i].name, name) == 0) {
            memset(&g_state.timers[i], 0, sizeof(g_state.timers[i]));
            unlock_runtime();
            return RAD_STATUS_OK;
        }
    }
    unlock_runtime();
    return RAD_STATUS_NOT_FOUND;
}

void rad_timer_tick(uint64_t elapsed_micros) {
    __atomic_fetch_add(&g_state.timer_elapsed_micros, elapsed_micros, __ATOMIC_RELAXED);
    __atomic_fetch_add(&g_state.scheduler_ticks, 1u, __ATOMIC_RELAXED);
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TIMER_SOURCES; ++i) {
        TimerSourceRecord& timer = g_state.timers[i];
        if (timer.used && timer.active) __atomic_fetch_add(&timer.ticks, 1u, __ATOMIC_RELAXED);
    }
}

rad_status_t rad_timer_schedule_oneshot(uint64_t delay_micros) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TIMER_SOURCES; ++i) {
        TimerSourceRecord& timer = g_state.timers[i];
        if (!timer.used || !timer.active || !timer.supports_oneshot || !timer.ops.schedule_oneshot) continue;
        return timer.ops.schedule_oneshot(timer.ops.context, delay_micros);
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t rad_timer_cancel_oneshot(void) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TIMER_SOURCES; ++i) {
        TimerSourceRecord& timer = g_state.timers[i];
        if (!timer.used || !timer.active || !timer.supports_oneshot || !timer.ops.cancel_oneshot) continue;
        return timer.ops.cancel_oneshot(timer.ops.context);
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

size_t rad_timer_list(rad_timer_source_info_t *timers, size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TIMER_SOURCES; ++i) {
        const TimerSourceRecord& timer = g_state.timers[i];
        if (!timer.used) continue;
        if (timers && count < capacity) {
            copy_string(timers[count].name, sizeof(timers[count].name), timer.name);
            timers[count].frequency_hz = timer.frequency_hz;
            timers[count].supports_oneshot = timer.supports_oneshot;
            timers[count].active = timer.active;
            timers[count].ticks = timer.ticks;
        }
        ++count;
    }
    return count;
}

rad_status_t rad_task_create_config(rad_task_t *task, const rad_task_config_t *config, rad_task_entry_t entry, void *context) {
    if (!task || !entry) return RAD_STATUS_INVALID_ARGUMENT;
    if (!g_state.initialized) {
        rad_kprintk(RKERN_ERR,
            "RADIX_TASK_CREATE_NOT_INITIALIZED init=%d shutdown=%d next_task=%llu cores=%u workers=0x%x current_pid=%d\n",
            g_state.initialized,
            g_state.shutdown_requested,
            static_cast<unsigned long long>(g_state.next_task_id),
            g_state.detected_cores,
            g_state.worker_running_mask,
            current_process_pid());
        return RAD_STATUS_NOT_INITIALIZED;
    }
    const int target_core = config ? config->target_core : RAD_TASK_CORE_SERVICE;
    if (target_core >= static_cast<int>(g_state.detected_cores)) return RAD_STATUS_NOT_SUPPORTED;
    if (target_core > 0 && !(g_state.worker_running_mask & (1u << static_cast<uint32_t>(target_core)))) {
        return RAD_STATUS_NOT_SUPPORTED;
    }
    const size_t stack_bytes = effective_task_stack_size(config);
#if RADIX_KERNEL_TASK_DYNAMIC_STACKS
    uint8_t *stack_allocation = nullptr;
    uint8_t *stack_base = nullptr;
    if (!allocate_task_stack(stack_bytes, &stack_allocation, &stack_base)) {
        return RAD_STATUS_NO_MEMORY;
    }
#endif
    lock_tasks();
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TASKS; ++i) {
        if (g_state.tasks[i].entry && (!g_state.tasks[i].finished || g_state.tasks[i].running)) continue;
        rad_task_handle& handle = g_state.tasks[i];
        release_task_stack(handle);
        memset(&handle, 0, sizeof(handle));
#if RADIX_KERNEL_TASK_DYNAMIC_STACKS
        handle.arch_stack_allocation = stack_allocation;
        handle.arch_stack = stack_base;
        handle.arch_stack_bytes = stack_bytes;
#endif
        handle.id = g_state.next_task_id++;
        copy_string(handle.name, sizeof(handle.name), config && config->name ? config->name : "task");
        handle.entry = entry;
        handle.context = context;
        handle.state = RAD_TASK_READY;
        handle.target_core = target_core;
        handle.current_core = RAD_TASK_CORE_ANY;
        handle.priority = config ? config->priority : 0;
        handle.stack_size = stack_bytes;
        handle.user_context = config ? config->user_context : nullptr;
        initialize_task_stack_guard(handle);
        *task = &handle;
        unlock_tasks();
        if (rad_hal_worker_wake) rad_hal_worker_wake();
        return RAD_STATUS_OK;
    }
    unlock_tasks();
#if RADIX_KERNEL_TASK_DYNAMIC_STACKS
    free(stack_allocation);
#endif
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_task_create(rad_task_t *task, const char *name, rad_task_entry_t entry, void *context, size_t stack_size) {
    rad_task_config_t config{};
    config.size = sizeof(config);
    config.name = name;
    config.stack_size = stack_size;
    config.target_core = RAD_TASK_CORE_SERVICE;
    return rad_task_create_config(task, &config, entry, context);
}

rad_status_t rad_task_join(rad_task_t task) {
    if (!task) return RAD_STATUS_INVALID_ARGUMENT;
    if (task->detached) return RAD_STATUS_INVALID_ARGUMENT;
    while (!task->finished) {
        const uint32_t current_core = hal_current_core();
        if (current_core == RAD_TASK_CORE_SERVICE) {
            rad_kernel_poll();
        } else if (rad_hal_worker_wait) {
            rad_hal_worker_wait();
        } else {
            hal_sleep(1000u);
        }
    }
    return RAD_STATUS_OK;
}

void rad_task_detach(rad_task_t task) {
    if (!task) return;
    task->detached = 1;
    if (task->finished) task->state = RAD_TASK_DETACHED;
}

uint64_t rad_task_current_id(void) {
    return g_current_task_id[hal_current_core()];
}

int rad_task_current_core(void) {
    return static_cast<int>(hal_current_core());
}

void rad_task_yield(void) {
    const uint32_t core = hal_current_core();
    if (arch_context_enabled()) {
        const uint64_t current_id = g_current_task_id[core];
        rad_task_handle *task = nullptr;
        if (current_id && !__atomic_load_n(&g_state.task_lock, __ATOMIC_RELAXED)) {
            lock_tasks();
            task = find_task_by_id(current_id);
            if (task && task->running && !task->finished) {
                task->running = 0;
                task->state = RAD_TASK_READY;
                task->current_core = RAD_TASK_CORE_ANY;
                if (g_scheduler_in_irq[core] && !task->preempt_pinned) {
                    task->preempt_saved_target_core = task->target_core;
                    task->target_core = static_cast<int>(core);
                    task->preempt_pinned = 1;
                }
                g_current_task_id[core] = 0;
                g_current_pid[core] = 0;
            } else {
                task = nullptr;
            }
            unlock_tasks();
        }
        if (task) {
            rad_arch_task_context_switch(task->arch_context, g_scheduler_context[core]);
            return;
        }
    }
    if (!run_one_task_on_core(core)) hal_sleep(0);
}

void rad_task_sleep_ms(uint32_t milliseconds) {
    rad_task_sleep_us(milliseconds * 1000u);
}

void rad_task_sleep_us(uint32_t microseconds) {
    const uint32_t core = hal_current_core();
    const uint64_t current_id = g_current_task_id[core];
    if (arch_context_enabled() && current_id) {
        rad_task_handle *task = nullptr;
        lock_tasks();
        task = find_task_by_id(current_id);
        if (task && task->running) {
            task->running = 0;
            task->state = RAD_TASK_SLEEPING;
            task->wake_micros = hal_now() + microseconds;
            task->current_core = RAD_TASK_CORE_ANY;
            g_current_task_id[core] = 0;
            g_current_pid[core] = 0;
        } else {
            task = nullptr;
        }
        unlock_tasks();
        if (task) {
            rad_arch_task_context_switch(task->arch_context, g_scheduler_context[core]);
            return;
        }
    }
    lock_tasks();
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TASKS; ++i) {
        if (g_state.tasks[i].id == current_id && g_state.tasks[i].running) {
            g_state.tasks[i].state = RAD_TASK_SLEEPING;
            g_state.tasks[i].wake_micros = hal_now() + microseconds;
            break;
        }
    }
    unlock_tasks();
    hal_sleep(microseconds);
    lock_tasks();
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TASKS; ++i) {
        if (g_state.tasks[i].id == current_id && g_state.tasks[i].running) {
            g_state.tasks[i].state = RAD_TASK_RUNNING;
            break;
        }
    }
    unlock_tasks();
}

size_t rad_task_list(rad_task_info_t *tasks, size_t capacity) {
    size_t count = 0;
    lock_tasks();
    for (size_t i = 0; i < RADIX_KERNEL_MAX_TASKS; ++i) {
        if (!g_state.tasks[i].entry) continue;
        if (tasks && count < capacity) {
            tasks[count].id = g_state.tasks[i].id;
            copy_string(tasks[count].name, sizeof(tasks[count].name), g_state.tasks[i].name);
            tasks[count].running = g_state.tasks[i].running;
            tasks[count].finished = g_state.tasks[i].finished;
            tasks[count].state = g_state.tasks[i].state;
            tasks[count].target_core = g_state.tasks[i].target_core;
            tasks[count].current_core = g_state.tasks[i].current_core;
            tasks[count].priority = g_state.tasks[i].priority;
            tasks[count].stack_size = g_state.tasks[i].stack_size;
            tasks[count].detached = g_state.tasks[i].detached;
        }
        ++count;
    }
    unlock_tasks();
    return count;
}

rad_status_t rad_core_info_get(rad_core_info_t *info) {
    if (!info) return RAD_STATUS_INVALID_ARGUMENT;
    info->detected_cores = g_state.detected_cores ? g_state.detected_cores : hal_core_count();
    info->service_core = RAD_TASK_CORE_SERVICE;
    info->worker_running_mask = g_state.worker_running_mask;
    info->worker_cores = 0;
    for (uint32_t core = 1; core < RADIX_KERNEL_MAX_CORES; ++core) {
        if (g_state.worker_running_mask & (1u << core)) ++info->worker_cores;
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_scheduler_info_get(rad_scheduler_info_t *info) {
    if (!info) return RAD_STATUS_INVALID_ARGUMENT;
    memset(info, 0, sizeof(*info));
    info->detected_cores = g_state.detected_cores ? g_state.detected_cores : hal_core_count();
    info->worker_running_mask = g_state.worker_running_mask;
    info->preemption_supported = scheduler_preemption_supported();
    info->preemption_enabled = g_state.preemption_enabled && info->preemption_supported;
    info->mode = info->preemption_enabled ? RAD_SCHEDULER_PREEMPTIVE : RAD_SCHEDULER_COOPERATIVE;
    copy_string(info->arch, sizeof(info->arch), scheduler_arch_name());
    info->online_core_mask = rad_scheduler_online_core_mask();
    info->current_core = rad_scheduler_current_core();
    info->context_switches = __atomic_load_n(&g_state.context_switches, __ATOMIC_RELAXED);
    info->scheduler_ticks = __atomic_load_n(&g_state.scheduler_ticks, __ATOMIC_RELAXED);
    scheduler_count_threads(info);
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PROCESSES; ++i) {
        if (g_state.processes[i].used) ++info->process_count;
    }
    return RAD_STATUS_OK;
}

void rad_scheduler_yield_from_irq(void) {
    __atomic_fetch_add(&g_state.scheduler_ticks, 1u, __ATOMIC_RELAXED);
    const uint32_t core = hal_current_core();
    if (!arch_context_enabled() || !g_state.preemption_enabled || core >= RADIX_KERNEL_MAX_CORES) return;
    if (!g_current_task_id[core]) return;
    if (__atomic_load_n(&g_state.task_lock, __ATOMIC_RELAXED)) return;
    if (__atomic_exchange_n(&g_scheduler_in_irq[core], 1, __ATOMIC_ACQUIRE)) return;
    if (!__atomic_exchange_n(&g_preempt_sched_seen, 1, __ATOMIC_ACQ_REL)) {
        rad_debug_marker("RADIX_PREEMPT_SCHED_OK");
    }
    if (core > 0 && !__atomic_exchange_n(&g_ap_preempt_sched_seen, 1, __ATOMIC_ACQ_REL)) {
        rad_debug_marker("RADIX_AP_PREEMPT_SCHED_OK");
    }
    if (core == RAD_TASK_CORE_SERVICE) {
        __atomic_store_n(&g_scheduler_in_irq[core], 0, __ATOMIC_RELEASE);
        return;
    }
    rad_task_yield();
    __atomic_store_n(&g_scheduler_in_irq[core], 0, __ATOMIC_RELEASE);
}

void rad_scheduler_set_preemption_enabled(int enabled) {
    g_state.preemption_enabled = enabled ? 1 : 0;
}

uint32_t rad_scheduler_current_core(void) {
    return hal_current_core();
}

uint32_t rad_scheduler_online_core_mask(void) {
    return 1u | g_state.worker_running_mask;
}

rad_status_t rad_mutex_create(rad_mutex_t *mutex) {
    if (!mutex) return RAD_STATUS_INVALID_ARGUMENT;
    *mutex = static_cast<rad_mutex_t>(rad_memory_alloc(sizeof(rad_mutex_handle)));
    if (!*mutex) return RAD_STATUS_NO_MEMORY;
    (*mutex)->locked = 0;
    return RAD_STATUS_OK;
}

void rad_mutex_destroy(rad_mutex_t mutex) {
    rad_memory_free(mutex);
}

rad_status_t rad_mutex_lock(rad_mutex_t mutex) {
    if (!mutex) return RAD_STATUS_INVALID_ARGUMENT;
    while (__atomic_test_and_set(&mutex->locked, __ATOMIC_ACQUIRE)) rad_task_yield();
    return RAD_STATUS_OK;
}

rad_status_t rad_mutex_unlock(rad_mutex_t mutex) {
    if (!mutex) return RAD_STATUS_INVALID_ARGUMENT;
    __atomic_clear(&mutex->locked, __ATOMIC_RELEASE);
    return RAD_STATUS_OK;
}

rad_status_t rad_event_create(rad_event_t *event, int initially_signaled) {
    if (!event) return RAD_STATUS_INVALID_ARGUMENT;
    *event = static_cast<rad_event_t>(rad_memory_alloc(sizeof(rad_event_handle)));
    if (!*event) return RAD_STATUS_NO_MEMORY;
    (*event)->signaled = initially_signaled ? 1 : 0;
    return RAD_STATUS_OK;
}

void rad_event_destroy(rad_event_t event) {
    rad_memory_free(event);
}

rad_status_t rad_event_wait(rad_event_t event, uint32_t timeout_ms) {
    if (!event) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t start = rad_time_millis();
    while (!event->signaled) {
        rad_task_yield();
        if (timeout_ms != UINT32_MAX && rad_time_millis() - start >= timeout_ms) return RAD_STATUS_TIMEOUT;
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_event_signal(rad_event_t event) {
    if (!event) return RAD_STATUS_INVALID_ARGUMENT;
    event->signaled = 1;
    return RAD_STATUS_OK;
}

rad_status_t rad_event_reset(rad_event_t event) {
    if (!event) return RAD_STATUS_INVALID_ARGUMENT;
    event->signaled = 0;
    return RAD_STATUS_OK;
}

void *rad_memory_alloc(size_t size) {
    const size_t total = sizeof(size_t) + size;
    uint8_t *raw = static_cast<uint8_t*>(malloc(total));
    if (!raw) return nullptr;
    memcpy(raw, &size, sizeof(size));
    g_state.memory.allocations++;
    g_state.memory.bytes_allocated += size;
    g_state.memory.bytes_live += size;
    if (g_state.memory.bytes_live > g_state.memory.peak_bytes_live) g_state.memory.peak_bytes_live = g_state.memory.bytes_live;
    return raw + sizeof(size_t);
}

void rad_memory_free(void *pointer) {
    if (!pointer) return;
    uint8_t *raw = static_cast<uint8_t*>(pointer) - sizeof(size_t);
    size_t size = 0;
    memcpy(&size, raw, sizeof(size));
    g_state.memory.frees++;
    g_state.memory.bytes_freed += size;
    g_state.memory.bytes_live = g_state.memory.bytes_live >= size ? g_state.memory.bytes_live - size : 0;
    free(raw);
}

void rad_memory_get_stats(rad_memory_stats_t *stats) {
    if (stats) *stats = g_state.memory;
}

rad_status_t rad_vfs_mount_host(const char*, const char*) {
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t rad_vfs_mount_sd(const rad_sd_config_t *config) {
    if (!config || !config->mount_point) return RAD_STATUS_INVALID_ARGUMENT;
    if (rad_hal_mount_sd) {
        const rad_status_t status = rad_hal_mount_sd(config);
        if (status != RAD_STATUS_OK && status != RAD_STATUS_NOT_SUPPORTED) return status;
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_MOUNTS; ++i) {
        if (g_state.mounts[i].used) continue;
        g_state.mounts[i].used = 1;
        copy_string(g_state.mounts[i].mount_point, sizeof(g_state.mounts[i].mount_point), config->mount_point);
        g_state.mounts[i].sd = *config;
        char normalized[96];
        normalize_path(normalized, sizeof(normalized), config->mount_point);
        ensure_directory(normalized);
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_vfs_mount_provider(const char *mount_point, const rad_vfs_backend_ops_t *ops) {
    if (!mount_point || !ops || !ops->open || !ops->read || !ops->write || !ops->close || !ops->stat) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    char normalized[96];
    normalize_path(normalized, sizeof(normalized), mount_point);
    for (size_t i = 0; i < RADIX_KERNEL_MAX_VFS_PROVIDERS; ++i) {
        if (g_state.providers[i].used && strcmp(g_state.providers[i].mount_point, normalized) == 0) {
            return RAD_STATUS_ALREADY_EXISTS;
        }
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_VFS_PROVIDERS; ++i) {
        if (g_state.providers[i].used) continue;
        g_state.providers[i].used = 1;
        copy_string(g_state.providers[i].mount_point, sizeof(g_state.providers[i].mount_point), normalized);
        g_state.providers[i].ops = *ops;
        ensure_directory(normalized);
        for (size_t m = 0; m < RADIX_KERNEL_MAX_MOUNTS; ++m) {
            if (g_state.mounts[m].used && strcmp(g_state.mounts[m].mount_point, normalized) == 0) {
                g_state.mounts[m].has_provider = 1;
                g_state.mounts[m].provider_index = i;
                return RAD_STATUS_OK;
            }
        }
        for (size_t m = 0; m < RADIX_KERNEL_MAX_MOUNTS; ++m) {
            if (g_state.mounts[m].used) continue;
            g_state.mounts[m].used = 1;
            copy_string(g_state.mounts[m].mount_point, sizeof(g_state.mounts[m].mount_point), normalized);
            g_state.mounts[m].has_provider = 1;
            g_state.mounts[m].provider_index = i;
            return RAD_STATUS_OK;
        }
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_vfs_unmount(const char *mount_point) {
    if (!mount_point) return RAD_STATUS_INVALID_ARGUMENT;
    char normalized[96];
    normalize_path(normalized, sizeof(normalized), mount_point);
    int removed = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_VFS_PROVIDERS; ++i) {
        if (g_state.providers[i].used && strcmp(g_state.providers[i].mount_point, normalized) == 0) {
            memset(&g_state.providers[i], 0, sizeof(g_state.providers[i]));
            removed = 1;
        }
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_MOUNTS; ++i) {
        if (g_state.mounts[i].used && strcmp(g_state.mounts[i].mount_point, normalized) == 0) {
            g_state.mounts[i].used = 0;
            return RAD_STATUS_OK;
        }
    }
    return removed ? RAD_STATUS_OK : RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_vfs_open(const char *path, uint32_t flags, rad_file_t *file) {
    if (!path || !file) return RAD_STATUS_INVALID_ARGUMENT;
    char normalized[96];
    normalize_path(normalized, sizeof(normalized), path);
    const char *relative = nullptr;
    VfsProviderRecord *provider = provider_for_path(normalized, &relative);
    if (provider) {
        if (!(flags & RAD_VFS_CREATE) && provider->ops.stat) {
            rad_vfs_stat_t stat{};
            lock_provider(provider);
            const rad_status_t stat_status = provider->ops.stat(provider->ops.context, relative, &stat);
            unlock_provider(provider);
            if (stat_status != RAD_STATUS_OK) return stat_status;
            if ((flags & RAD_VFS_READ) && !can_read_stat(stat)) return RAD_STATUS_INVALID_ARGUMENT;
            if ((flags & (RAD_VFS_WRITE | RAD_VFS_TRUNCATE | RAD_VFS_APPEND)) && !can_write_stat(stat)) return RAD_STATUS_INVALID_ARGUMENT;
        }
        void *backend_file = nullptr;
        lock_provider(provider);
        const rad_status_t status = provider->ops.open(provider->ops.context, relative, flags, &backend_file);
        unlock_provider(provider);
        if (status != RAD_STATUS_OK) return status;
        rad_file_t handle = static_cast<rad_file_t>(rad_memory_alloc(sizeof(rad_file_handle)));
        if (!handle) {
            lock_provider(provider);
            if (provider->ops.close) provider->ops.close(backend_file);
            unlock_provider(provider);
            rad_debug_marker("RADIX_VFS_HANDLE_ALLOC_FAIL");
            return RAD_STATUS_NO_MEMORY;
        }
        memset(handle, 0, sizeof(*handle));
        handle->backend = 1;
        handle->backend_file = backend_file;
        handle->provider_index = provider_index(provider);
        handle->flags = flags;
        *file = handle;
        return RAD_STATUS_OK;
    }
    if (!mounted_path(normalized)) return RAD_STATUS_NOT_FOUND;
    VfsFileRecord *record = find_file_or_dir(normalized);
    if (record && record->is_directory) return RAD_STATUS_INVALID_ARGUMENT;
    if (!record) {
        if (!(flags & RAD_VFS_CREATE)) return RAD_STATUS_NOT_FOUND;
        char parent[96];
        parent_path(parent, sizeof(parent), normalized);
        VfsFileRecord *parent_record = find_file_or_dir(parent);
        if (!parent_record || !parent_record->is_directory) return RAD_STATUS_NOT_FOUND;
        rad_vfs_stat_t parent_stat{};
        fill_stat(*parent_record, &parent_stat);
        if (!can_write_stat(parent_stat) || !can_execute_stat(parent_stat)) return RAD_STATUS_INVALID_ARGUMENT;
        record = allocate_file(normalized);
        if (!record) return RAD_STATUS_NO_MEMORY;
    } else {
        rad_vfs_stat_t stat{};
        fill_stat(*record, &stat);
        if ((flags & RAD_VFS_READ) && !can_read_stat(stat)) return RAD_STATUS_INVALID_ARGUMENT;
        if ((flags & (RAD_VFS_WRITE | RAD_VFS_TRUNCATE | RAD_VFS_APPEND)) && !can_write_stat(stat)) return RAD_STATUS_INVALID_ARGUMENT;
    }
    if (flags & RAD_VFS_TRUNCATE) record->size = 0;
    rad_file_t handle = static_cast<rad_file_t>(rad_memory_alloc(sizeof(rad_file_handle)));
    if (!handle) return RAD_STATUS_NO_MEMORY;
    memset(handle, 0, sizeof(*handle));
    handle->entry_index = static_cast<size_t>(record - g_state.files);
    handle->position = (flags & RAD_VFS_APPEND) ? record->size : 0;
    handle->flags = flags;
    *file = handle;
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_read(rad_file_t file, void *buffer, size_t size, size_t *bytes_read) {
    if (!file || !buffer) return RAD_STATUS_INVALID_ARGUMENT;
    if (file->backend) {
        if (file->provider_index >= RADIX_KERNEL_MAX_VFS_PROVIDERS || !g_state.providers[file->provider_index].used) {
            return RAD_STATUS_NOT_FOUND;
        }
        auto& ops = g_state.providers[file->provider_index].ops;
        VfsProviderRecord *provider = &g_state.providers[file->provider_index];
        lock_provider(provider);
        const rad_status_t status = ops.read ? ops.read(file->backend_file, buffer, size, bytes_read) : RAD_STATUS_NOT_SUPPORTED;
        unlock_provider(provider);
        return status;
    }
    if (!(file->flags & RAD_VFS_READ)) return RAD_STATUS_INVALID_ARGUMENT;
    if (file->entry_index >= RADIX_KERNEL_MAX_VFS_FILES) return RAD_STATUS_INVALID_ARGUMENT;
    VfsFileRecord& record = g_state.files[file->entry_index];
    if (!record.used) return RAD_STATUS_NOT_FOUND;
    const size_t available = file->position < record.size ? record.size - file->position : 0;
    const size_t count = available < size ? available : size;
    if (count) memcpy(buffer, record.data + file->position, count);
    file->position += count;
    if (bytes_read) *bytes_read = count;
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_write(rad_file_t file, const void *buffer, size_t size, size_t *bytes_written) {
    if (!file || !buffer) return RAD_STATUS_INVALID_ARGUMENT;
    if (file->backend) {
        if (file->provider_index >= RADIX_KERNEL_MAX_VFS_PROVIDERS || !g_state.providers[file->provider_index].used) {
            return RAD_STATUS_NOT_FOUND;
        }
        auto& ops = g_state.providers[file->provider_index].ops;
        VfsProviderRecord *provider = &g_state.providers[file->provider_index];
        lock_provider(provider);
        const rad_status_t status = ops.write ? ops.write(file->backend_file, buffer, size, bytes_written) : RAD_STATUS_NOT_SUPPORTED;
        unlock_provider(provider);
        return status;
    }
    if (!(file->flags & RAD_VFS_WRITE)) return RAD_STATUS_INVALID_ARGUMENT;
    if (file->entry_index >= RADIX_KERNEL_MAX_VFS_FILES) return RAD_STATUS_INVALID_ARGUMENT;
    VfsFileRecord& record = g_state.files[file->entry_index];
    if (!record.used) return RAD_STATUS_NOT_FOUND;
    if (file->flags & RAD_VFS_APPEND) file->position = record.size;
    if (file->position >= RADIX_KERNEL_VFS_FILE_BYTES) {
        if (bytes_written) *bytes_written = 0;
        return RAD_STATUS_NO_MEMORY;
    }
    const size_t capacity = RADIX_KERNEL_VFS_FILE_BYTES - file->position;
    const size_t count = capacity < size ? capacity : size;
    if (count) memcpy(record.data + file->position, buffer, count);
    file->position += count;
    if (file->position > record.size) record.size = file->position;
    if (bytes_written) *bytes_written = count;
    return count == size ? RAD_STATUS_OK : RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_vfs_seek(rad_file_t file, int64_t offset, rad_seek_origin_t origin) {
    if (!file) return RAD_STATUS_INVALID_ARGUMENT;
    if (file->backend) {
        if (file->provider_index >= RADIX_KERNEL_MAX_VFS_PROVIDERS || !g_state.providers[file->provider_index].used) {
            return RAD_STATUS_NOT_FOUND;
        }
        auto& ops = g_state.providers[file->provider_index].ops;
        VfsProviderRecord *provider = &g_state.providers[file->provider_index];
        lock_provider(provider);
        const rad_status_t status = ops.seek ? ops.seek(file->backend_file, offset, origin) : RAD_STATUS_NOT_SUPPORTED;
        unlock_provider(provider);
        return status;
    }
    if (file->entry_index >= RADIX_KERNEL_MAX_VFS_FILES) return RAD_STATUS_INVALID_ARGUMENT;
    VfsFileRecord& record = g_state.files[file->entry_index];
    int64_t base = 0;
    if (origin == RAD_SEEK_CUR) base = static_cast<int64_t>(file->position);
    if (origin == RAD_SEEK_END) base = static_cast<int64_t>(record.size);
    const int64_t next = base + offset;
    if (next < 0 || next > static_cast<int64_t>(RADIX_KERNEL_VFS_FILE_BYTES)) return RAD_STATUS_INVALID_ARGUMENT;
    file->position = static_cast<size_t>(next);
    return RAD_STATUS_OK;
}

uint64_t rad_vfs_tell(rad_file_t file) {
    if (file && file->backend) {
        if (file->provider_index >= RADIX_KERNEL_MAX_VFS_PROVIDERS || !g_state.providers[file->provider_index].used) {
            return 0;
        }
        auto& ops = g_state.providers[file->provider_index].ops;
        VfsProviderRecord *provider = &g_state.providers[file->provider_index];
        lock_provider(provider);
        const uint64_t position = ops.tell ? ops.tell(file->backend_file) : 0;
        unlock_provider(provider);
        return position;
    }
    return file ? file->position : 0;
}

void rad_vfs_close(rad_file_t file) {
    if (file && file->backend && file->provider_index < RADIX_KERNEL_MAX_VFS_PROVIDERS && g_state.providers[file->provider_index].used) {
        auto& ops = g_state.providers[file->provider_index].ops;
        VfsProviderRecord *provider = &g_state.providers[file->provider_index];
        lock_provider(provider);
        if (ops.close) ops.close(file->backend_file);
        unlock_provider(provider);
    }
    rad_memory_free(file);
}

rad_status_t rad_vfs_fsync(rad_file_t file) {
    if (!file) return RAD_STATUS_INVALID_ARGUMENT;
    if (file->backend) {
        if (file->provider_index >= RADIX_KERNEL_MAX_VFS_PROVIDERS || !g_state.providers[file->provider_index].used) {
            return RAD_STATUS_NOT_FOUND;
        }
        auto& ops = g_state.providers[file->provider_index].ops;
        VfsProviderRecord *provider = &g_state.providers[file->provider_index];
        lock_provider(provider);
        const rad_status_t status = ops.fsync ? ops.fsync(file->backend_file) : RAD_STATUS_OK;
        unlock_provider(provider);
        return status;
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_stat(const char *path, rad_vfs_stat_t *stat) {
    if (!path || !stat) return RAD_STATUS_INVALID_ARGUMENT;
    char normalized[96];
    normalize_path(normalized, sizeof(normalized), path);
    const char *relative = nullptr;
    VfsProviderRecord *provider = provider_for_path(normalized, &relative);
    if (provider) {
        lock_provider(provider);
        const rad_status_t status = provider->ops.stat ? provider->ops.stat(provider->ops.context, relative, stat) : RAD_STATUS_NOT_SUPPORTED;
        unlock_provider(provider);
        return status;
    }
    if (strcmp(normalized, "/") == 0) {
        stat->is_directory = 1;
        stat->size = 0;
        stat->mode = 0040755u;
        stat->uid = 0;
        stat->gid = 0;
        stat->mtime_millis = 0;
        return RAD_STATUS_OK;
    }
    if (!mounted_path(normalized)) return RAD_STATUS_NOT_FOUND;
    VfsFileRecord *record = find_file_or_dir(normalized);
    if (record) {
        fill_stat(*record, stat);
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_vfs_list(const char *path, rad_vfs_list_callback_t callback, void *context) {
    if (!path || !callback) return RAD_STATUS_INVALID_ARGUMENT;
    char normalized[96];
    normalize_path(normalized, sizeof(normalized), path);
    const char *relative = nullptr;
    VfsProviderRecord *provider = provider_for_path(normalized, &relative);
    if (provider) {
        lock_provider(provider);
        const rad_status_t status = provider->ops.list ? provider->ops.list(provider->ops.context, relative, callback, context) : RAD_STATUS_NOT_SUPPORTED;
        unlock_provider(provider);
        return status;
    }
    if (strcmp(normalized, "/") != 0 && !mounted_path(normalized)) return RAD_STATUS_NOT_FOUND;
    rad_vfs_stat_t dir_stat{};
    if (rad_vfs_stat(normalized, &dir_stat) != RAD_STATUS_OK || !dir_stat.is_directory) return RAD_STATUS_NOT_FOUND;
    if (!can_read_stat(dir_stat) || !can_execute_stat(dir_stat)) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_VFS_FILES; ++i) {
        if (!g_state.files[i].used) continue;
        const char *name = nullptr;
        if (!is_child_name(normalized, g_state.files[i].path, &name)) continue;
        rad_vfs_stat_t stat{};
        fill_stat(g_state.files[i], &stat);
        if (!callback(name, &stat, context)) break;
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_mkdir(const char *path) {
    if (!path) return RAD_STATUS_INVALID_ARGUMENT;
    char normalized[96];
    normalize_path(normalized, sizeof(normalized), path);
    const char *relative = nullptr;
    VfsProviderRecord *provider = provider_for_path(normalized, &relative);
    if (provider) {
        lock_provider(provider);
        const rad_status_t status = provider->ops.mkdir ? provider->ops.mkdir(provider->ops.context, relative) : RAD_STATUS_NOT_SUPPORTED;
        unlock_provider(provider);
        return status;
    }
    if (!mounted_path(normalized)) return RAD_STATUS_NOT_FOUND;
    char parent[96];
    parent_path(parent, sizeof(parent), normalized);
    VfsFileRecord *parent_record = find_file_or_dir(parent);
    if (!parent_record || !parent_record->is_directory) return RAD_STATUS_NOT_FOUND;
    rad_vfs_stat_t parent_stat{};
    fill_stat(*parent_record, &parent_stat);
    if (!can_write_stat(parent_stat) || !can_execute_stat(parent_stat)) return RAD_STATUS_INVALID_ARGUMENT;
    VfsFileRecord *existing = find_file_or_dir(normalized);
    if (existing) return existing->is_directory ? RAD_STATUS_OK : RAD_STATUS_ALREADY_EXISTS;
    return ensure_directory(normalized) ? RAD_STATUS_OK : RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_vfs_remove(const char *path) {
    if (!path) return RAD_STATUS_INVALID_ARGUMENT;
    char normalized[96];
    normalize_path(normalized, sizeof(normalized), path);
    const char *relative = nullptr;
    VfsProviderRecord *provider = provider_for_path(normalized, &relative);
    if (provider) {
        lock_provider(provider);
        const rad_status_t status = provider->ops.remove ? provider->ops.remove(provider->ops.context, relative) : RAD_STATUS_NOT_SUPPORTED;
        unlock_provider(provider);
        return status;
    }
    VfsFileRecord *record = find_file_or_dir(normalized);
    if (!record) return RAD_STATUS_NOT_FOUND;
    if (record->is_directory && has_children(normalized)) return RAD_STATUS_INVALID_ARGUMENT;
    char parent[96];
    parent_path(parent, sizeof(parent), normalized);
    VfsFileRecord *parent_record = find_file_or_dir(parent);
    if (parent_record) {
        rad_vfs_stat_t parent_stat{};
        fill_stat(*parent_record, &parent_stat);
        if (!can_write_stat(parent_stat) || !can_execute_stat(parent_stat)) return RAD_STATUS_INVALID_ARGUMENT;
    }
    memset(record, 0, sizeof(*record));
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_rename(const char *old_path, const char *new_path) {
    if (!old_path || !new_path) return RAD_STATUS_INVALID_ARGUMENT;
    char old_normalized[96];
    char new_normalized[96];
    normalize_path(old_normalized, sizeof(old_normalized), old_path);
    normalize_path(new_normalized, sizeof(new_normalized), new_path);
    const char *old_relative = nullptr;
    const char *new_relative = nullptr;
    VfsProviderRecord *old_provider = provider_for_path(old_normalized, &old_relative);
    VfsProviderRecord *new_provider = provider_for_path(new_normalized, &new_relative);
    if (old_provider || new_provider) {
        if (old_provider != new_provider || !old_provider) return RAD_STATUS_INVALID_ARGUMENT;
        lock_provider(old_provider);
        const rad_status_t status = old_provider->ops.rename
            ? old_provider->ops.rename(old_provider->ops.context, old_relative, new_relative)
            : RAD_STATUS_NOT_SUPPORTED;
        unlock_provider(old_provider);
        return status;
    }
    VfsFileRecord *record = find_file_or_dir(old_normalized);
    if (!record) return RAD_STATUS_NOT_FOUND;
    if (find_file_or_dir(new_normalized)) return RAD_STATUS_ALREADY_EXISTS;
    char parent[96];
    parent_path(parent, sizeof(parent), new_normalized);
    VfsFileRecord *parent_record = find_file_or_dir(parent);
    if (!parent_record || !parent_record->is_directory) return RAD_STATUS_NOT_FOUND;
    rad_vfs_stat_t parent_stat{};
    fill_stat(*parent_record, &parent_stat);
    if (!can_write_stat(parent_stat) || !can_execute_stat(parent_stat)) return RAD_STATUS_INVALID_ARGUMENT;
    copy_string(record->path, sizeof(record->path), new_normalized);
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_rmdir(const char *path) {
    if (!path) return RAD_STATUS_INVALID_ARGUMENT;
    char normalized[96];
    normalize_path(normalized, sizeof(normalized), path);
    const char *relative = nullptr;
    VfsProviderRecord *provider = provider_for_path(normalized, &relative);
    if (provider) {
        lock_provider(provider);
        const rad_status_t status = provider->ops.rmdir ? provider->ops.rmdir(provider->ops.context, relative) : RAD_STATUS_NOT_SUPPORTED;
        unlock_provider(provider);
        return status;
    }
    VfsFileRecord *record = find_file_or_dir(normalized);
    if (!record || !record->is_directory) return RAD_STATUS_NOT_FOUND;
    if (has_children(normalized)) return RAD_STATUS_INVALID_ARGUMENT;
    char parent[96];
    parent_path(parent, sizeof(parent), normalized);
    VfsFileRecord *parent_record = find_file_or_dir(parent);
    if (parent_record) {
        rad_vfs_stat_t parent_stat{};
        fill_stat(*parent_record, &parent_stat);
        if (!can_write_stat(parent_stat) || !can_execute_stat(parent_stat)) return RAD_STATUS_INVALID_ARGUMENT;
    }
    memset(record, 0, sizeof(*record));
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_truncate(const char *path, uint64_t size) {
    if (!path) return RAD_STATUS_INVALID_ARGUMENT;
    char normalized[96];
    normalize_path(normalized, sizeof(normalized), path);
    const char *relative = nullptr;
    VfsProviderRecord *provider = provider_for_path(normalized, &relative);
    if (provider) {
        lock_provider(provider);
        const rad_status_t status = provider->ops.truncate ? provider->ops.truncate(provider->ops.context, relative, size) : RAD_STATUS_NOT_SUPPORTED;
        unlock_provider(provider);
        return status;
    }
    if (size > RADIX_KERNEL_VFS_FILE_BYTES) return RAD_STATUS_NO_MEMORY;
    VfsFileRecord *record = find_file_or_dir(normalized);
    if (!record || record->is_directory) return RAD_STATUS_NOT_FOUND;
    rad_vfs_stat_t stat{};
    fill_stat(*record, &stat);
    if (!can_write_stat(stat)) return RAD_STATUS_INVALID_ARGUMENT;
    if (size > record->size) memset(record->data + record->size, 0, static_cast<size_t>(size - record->size));
    record->size = static_cast<size_t>(size);
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_readlink(const char *path, char *buffer, size_t size) {
    if (!path || !buffer || size == 0) return RAD_STATUS_INVALID_ARGUMENT;
    char normalized[96];
    normalize_path(normalized, sizeof(normalized), path);
    const char *relative = nullptr;
    VfsProviderRecord *provider = provider_for_path(normalized, &relative);
    if (provider) {
        lock_provider(provider);
        const rad_status_t status = provider->ops.readlink ? provider->ops.readlink(provider->ops.context, relative, buffer, size) : RAD_STATUS_NOT_SUPPORTED;
        unlock_provider(provider);
        return status;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t rad_vfs_symlink(const char *target, const char *link_path) {
    if (!target || !link_path) return RAD_STATUS_INVALID_ARGUMENT;
    char normalized[96];
    normalize_path(normalized, sizeof(normalized), link_path);
    const char *relative = nullptr;
    VfsProviderRecord *provider = provider_for_path(normalized, &relative);
    if (provider) {
        lock_provider(provider);
        const rad_status_t status = provider->ops.symlink ? provider->ops.symlink(provider->ops.context, target, relative) : RAD_STATUS_NOT_SUPPORTED;
        unlock_provider(provider);
        return status;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t rad_vfs_link(const char *old_path, const char *new_path) {
    if (!old_path || !new_path) return RAD_STATUS_INVALID_ARGUMENT;
    char old_normalized[96];
    char new_normalized[96];
    normalize_path(old_normalized, sizeof(old_normalized), old_path);
    normalize_path(new_normalized, sizeof(new_normalized), new_path);
    const char *old_relative = nullptr;
    const char *new_relative = nullptr;
    VfsProviderRecord *old_provider = provider_for_path(old_normalized, &old_relative);
    VfsProviderRecord *new_provider = provider_for_path(new_normalized, &new_relative);
    if (old_provider || new_provider) {
        if (old_provider != new_provider || !old_provider) return RAD_STATUS_INVALID_ARGUMENT;
        lock_provider(old_provider);
        const rad_status_t status = old_provider->ops.link ? old_provider->ops.link(old_provider->ops.context, old_relative, new_relative) : RAD_STATUS_NOT_SUPPORTED;
        unlock_provider(old_provider);
        return status;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t rad_vfs_chmod(const char *path, uint32_t mode) {
    if (!path) return RAD_STATUS_INVALID_ARGUMENT;
    char normalized[96];
    normalize_path(normalized, sizeof(normalized), path);
    const char *relative = nullptr;
    VfsProviderRecord *provider = provider_for_path(normalized, &relative);
    if (provider) {
        lock_provider(provider);
        const rad_status_t status = provider->ops.chmod ? provider->ops.chmod(provider->ops.context, relative, mode) : RAD_STATUS_NOT_SUPPORTED;
        unlock_provider(provider);
        return status;
    }
    VfsFileRecord *record = find_file_or_dir(normalized);
    if (!record) return RAD_STATUS_NOT_FOUND;
    rad_vfs_stat_t stat{};
    fill_stat(*record, &stat);
    if (current_credentials().euid != 0 && current_credentials().euid != stat.uid) return RAD_STATUS_INVALID_ARGUMENT;
    record->mode = (record->mode & 0170000u) | (mode & 07777u);
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_chdir(const char *path) {
    if (!path) return RAD_STATUS_INVALID_ARGUMENT;
    char normalized[96];
    normalize_path(normalized, sizeof(normalized), path);
    rad_vfs_stat_t stat{};
    if (rad_vfs_stat(normalized, &stat) != RAD_STATUS_OK || !stat.is_directory) return RAD_STATUS_NOT_FOUND;
    if (!can_execute_stat(stat)) return RAD_STATUS_INVALID_ARGUMENT;
    copy_string(g_state.cwd, sizeof(g_state.cwd), normalized);
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_getcwd(char *buffer, size_t size) {
    if (!buffer || size == 0) return RAD_STATUS_INVALID_ARGUMENT;
    copy_string(buffer, size, g_state.cwd[0] ? g_state.cwd : "/");
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_opendir(const char *path, rad_dir_t *dir) {
    if (!path || !dir) return RAD_STATUS_INVALID_ARGUMENT;
    char normalized[96];
    normalize_path(normalized, sizeof(normalized), path);
    rad_vfs_stat_t stat{};
    if (rad_vfs_stat(normalized, &stat) != RAD_STATUS_OK || !stat.is_directory) return RAD_STATUS_NOT_FOUND;
    rad_dir_t handle = static_cast<rad_dir_t>(rad_memory_alloc(sizeof(rad_dir_handle)));
    if (!handle) return RAD_STATUS_NO_MEMORY;
    memset(handle, 0, sizeof(*handle));
    copy_string(handle->path, sizeof(handle->path), normalized);
    const rad_status_t list_status = rad_vfs_list(normalized, [](const char *name, const rad_vfs_stat_t *child_stat, void *context) -> int {
        auto *snapshot = static_cast<rad_dir_t>(context);
        if (!snapshot || snapshot->count >= RADIX_KERNEL_MAX_DIR_ENTRIES) return 0;
        rad_vfs_dirent_t& entry = snapshot->entries[snapshot->count++];
        memset(&entry, 0, sizeof(entry));
        copy_string(entry.name, sizeof(entry.name), name);
        if (child_stat) entry.stat = *child_stat;
        return 1;
    }, handle);
    if (list_status != RAD_STATUS_OK) {
        rad_memory_free(handle);
        return list_status;
    }
    *dir = handle;
    return RAD_STATUS_OK;
}

rad_status_t rad_vfs_readdir(rad_dir_t dir, rad_vfs_dirent_t *entry) {
    if (!dir || !entry) return RAD_STATUS_INVALID_ARGUMENT;
    if (dir->cursor >= dir->count) return RAD_STATUS_NOT_FOUND;
    *entry = dir->entries[dir->cursor++];
    return RAD_STATUS_OK;
}

void rad_vfs_closedir(rad_dir_t dir) {
    rad_memory_free(dir);
}

int32_t rad_process_current_pid(void) {
    return current_process_pid();
}

int32_t rad_process_parent_pid(void) {
    const int32_t current = current_process_pid();
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PROCESSES; ++i) {
        if (g_state.processes[i].used && g_state.processes[i].pid == current) return g_state.processes[i].parent_pid;
    }
    return 0;
}

int32_t rad_process_fork(void) {
    return rad_process_fork_from_arch_frame(nullptr);
}

int32_t rad_process_create(const char *path, int32_t parent_pid) {
    if (!path || !*path) return RAD_STATUS_INVALID_ARGUMENT;
    if (parent_pid < 0) parent_pid = current_process_pid();
    if ((strcmp(path, "/bin/init") == 0 || strcmp(path, "/sbin/radinit") == 0) && !find_process_record(1)) {
        for (size_t i = 0; i < RADIX_KERNEL_MAX_PROCESSES; ++i) {
            if (g_state.processes[i].used) continue;
            ProcessRecord& process = g_state.processes[i];
            memset(&process, 0, sizeof(process));
            process.used = 1;
            process.pid = 1;
            process.parent_pid = 0;
            process.state = RAD_PROCESS_RUNNING;
            process.exit_code = 0;
            process.credentials = rad_credentials_t{0, 0, 0, 0};
            copy_string(process.path, sizeof(process.path), path);
            return 1;
        }
        return RAD_STATUS_NO_MEMORY;
    }
    if (parent_pid <= 0 && strcmp(path, "/bin/init") != 0 && strcmp(path, "/sbin/radinit") != 0) {
        if (find_process_record(1)) parent_pid = 1;
    }
    if (g_state.next_pid < 2) g_state.next_pid = 2;
    while (find_process_record(g_state.next_pid)) ++g_state.next_pid;
    if (g_state.next_pid == 1) ++g_state.next_pid;
    rad_credentials_t credentials = rad_credentials_t{0, 0, 0, 0};
    if (ProcessRecord *parent = find_process_record(parent_pid)) credentials = parent->credentials;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PROCESSES; ++i) {
        if (g_state.processes[i].used) continue;
        ProcessRecord& process = g_state.processes[i];
        memset(&process, 0, sizeof(process));
        process.used = 1;
        process.pid = g_state.next_pid++;
        process.parent_pid = parent_pid;
        process.state = RAD_PROCESS_RUNNING;
        process.exit_code = 0;
        process.credentials = credentials;
        copy_string(process.path, sizeof(process.path), path);
        return process.pid;
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_process_attach_task(int32_t pid, rad_task_t task) {
    if (pid <= 0 || !task) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PROCESSES; ++i) {
        ProcessRecord& process = g_state.processes[i];
        if (!process.used || process.pid != pid) continue;
        process.task = task;
        task->process_pid = pid;
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_FOUND;
}

void rad_process_set_current_pid(int32_t pid) {
    const uint32_t core = hal_current_core();
    g_current_pid[core] = pid >= 0 ? pid : 0;
    const uint64_t task_id = g_current_task_id[core];
    rad_task_handle *task = find_task_by_id(task_id);
    if (task) task->process_pid = g_current_pid[core];
}

rad_status_t rad_process_mark_exec(int32_t pid, const char *path) {
    if (pid <= 0 || !path || !*path) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PROCESSES; ++i) {
        ProcessRecord& process = g_state.processes[i];
        if (!process.used || process.pid != pid) continue;
        normalize_path(process.path, sizeof(process.path), path);
        process.state = RAD_PROCESS_RUNNING;
        process.exit_code = 0;
        close_process_fds(pid, 1);
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_process_clone_fds(int32_t parent_pid, int32_t child_pid) {
    if (parent_pid <= 0 || child_pid <= 0 || parent_pid == child_pid) return RAD_STATUS_INVALID_ARGUMENT;
    for (int32_t slot = 0; slot < static_cast<int32_t>(RADIX_KERNEL_MAX_POSIX_FDS); ++slot) {
        FdRecord& record = g_state.fds[slot];
        if (!record.used || record.owner_pid != parent_pid) continue;
        const int32_t owner = fd_owner_index(slot);
        if (owner < 0) continue;
        FdRecord copy = record;
        copy.owner_pid = child_pid;
        copy.owner_fd = owner;
        copy.refs = 1;
        copy.descriptor_flags = record.descriptor_flags;
        if (g_state.fds[owner].used) ++g_state.fds[owner].refs;
        const int32_t installed = install_fd_record(copy, record.local_fd);
        if (installed < 0) return static_cast<rad_status_t>(installed);
    }
    return RAD_STATUS_OK;
}

int32_t rad_process_execve(const char *path, const char *const[]) {
    if (!path || !*path) return RAD_STATUS_INVALID_ARGUMENT;
    rad_vfs_stat_t stat{};
    rad_status_t status = rad_vfs_stat(path, &stat);
    if (status != RAD_STATUS_OK) return status;
    if (stat.is_directory) return RAD_STATUS_INVALID_ARGUMENT;
    return rad_process_mark_exec(current_process_pid(), path);
}

int32_t rad_process_waitpid(int32_t pid, int32_t *status, uint32_t options) {
    const int32_t current = current_process_pid();
    for (;;) {
        int found_child = 0;
        for (size_t i = 0; i < RADIX_KERNEL_MAX_PROCESSES; ++i) {
            ProcessRecord& process = g_state.processes[i];
            if (!process.used || process.parent_pid != current) continue;
            if (pid > 0 && process.pid != pid) continue;
            found_child = 1;
            if (process.state != RAD_PROCESS_ZOMBIE) continue;
            if (status) *status = process.exit_code;
            const int32_t done = process.pid;
            const int32_t exit_code = process.exit_code;
            close_process_fds(done, 0);
            if (g_state.has_process_arch_ops && g_state.process_arch_ops.process_reaped) {
                g_state.process_arch_ops.process_reaped(g_state.process_arch_ops.context, done, exit_code);
            }
            memset(&process, 0, sizeof(process));
            return done;
        }
        if (!found_child) return RAD_STATUS_NOT_FOUND;
        if (options & RAD_WAIT_NOHANG) return 0;
        rad_task_sleep_ms(1);
    }
}

void rad_process_exit(int32_t status) {
    const int32_t current = current_process_pid();
    if (current == 0) return;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PROCESSES; ++i) {
        if (!g_state.processes[i].used || g_state.processes[i].pid != current) continue;
        g_state.processes[i].state = RAD_PROCESS_ZOMBIE;
        g_state.processes[i].exit_code = status;
        return;
    }
}

int32_t rad_process_kill(int32_t pid, int32_t signal_number) {
    if (pid <= 1) return RAD_STATUS_INVALID_ARGUMENT;
    if (signal_number <= 0 || signal_number >= 64) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PROCESSES; ++i) {
        ProcessRecord& process = g_state.processes[i];
        if (!process.used || process.pid != pid) continue;
        if (process.state == RAD_PROCESS_ZOMBIE) return RAD_STATUS_OK;
        process.state = RAD_PROCESS_ZOMBIE;
        process.exit_code = 128 + signal_number;
        if (process.task) {
            process.task->finished = 1;
            process.task->state = RAD_TASK_FINISHED;
            process.task->current_core = RAD_TASK_CORE_ANY;
        }
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_FOUND;
}

size_t rad_process_list(rad_process_info_t *processes, size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PROCESSES; ++i) {
        const ProcessRecord& process = g_state.processes[i];
        if (!process.used) continue;
        if (processes && count < capacity) {
            processes[count].pid = process.pid;
            processes[count].parent_pid = process.parent_pid;
            processes[count].exited = process.state == RAD_PROCESS_ZOMBIE;
            processes[count].exit_code = process.exit_code;
            processes[count].state = process.state;
            processes[count].credentials = process.credentials;
            copy_string(processes[count].path, sizeof(processes[count].path), process.path);
        }
        ++count;
    }
    return count;
}

rad_status_t rad_process_arch_register(const rad_process_arch_ops_t *ops) {
    if (!ops || ops->size < sizeof(rad_process_arch_ops_t)) return RAD_STATUS_INVALID_ARGUMENT;
    g_state.process_arch_ops = *ops;
    g_state.has_process_arch_ops = 1;
    return RAD_STATUS_OK;
}

int32_t rad_process_fork_from_arch_frame(void *trap_frame) {
    if (!g_state.has_process_arch_ops || !g_state.process_arch_ops.fork_from_frame) {
        return RAD_STATUS_NOT_SUPPORTED;
    }
    return g_state.process_arch_ops.fork_from_frame(g_state.process_arch_ops.context, trap_frame);
}

int32_t rad_process_reap(int32_t pid, int32_t *status) {
    return rad_process_waitpid(pid, status, 0);
}

rad_status_t rad_process_get_credentials(int32_t pid, rad_credentials_t *credentials) {
    if (!credentials) return RAD_STATUS_INVALID_ARGUMENT;
    if (pid <= 0) pid = current_process_pid();
    ProcessRecord *process = find_process_record(pid);
    if (!process) return RAD_STATUS_NOT_FOUND;
    *credentials = process->credentials;
    return RAD_STATUS_OK;
}

rad_status_t rad_process_set_credentials(int32_t pid, const rad_credentials_t *credentials) {
    if (!credentials) return RAD_STATUS_INVALID_ARGUMENT;
    if (pid <= 0) pid = current_process_pid();
    ProcessRecord *process = find_process_record(pid);
    if (!process) return RAD_STATUS_NOT_FOUND;
    if (current_credentials().euid != 0 && pid != current_process_pid()) return RAD_STATUS_INVALID_ARGUMENT;
    if (current_credentials().euid != 0 && (credentials->uid != process->credentials.uid || credentials->euid != process->credentials.uid)) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    process->credentials = *credentials;
    return RAD_STATUS_OK;
}

rad_uid_t rad_process_getuid(void) {
    return current_credentials().uid;
}

rad_uid_t rad_process_geteuid(void) {
    return current_credentials().euid;
}

rad_gid_t rad_process_getgid(void) {
    return current_credentials().gid;
}

rad_gid_t rad_process_getegid(void) {
    return current_credentials().egid;
}

rad_status_t rad_process_setuid(rad_uid_t uid) {
    ProcessRecord *process = find_process_record(current_process_pid());
    if (!process) return RAD_STATUS_NOT_FOUND;
    if (process->credentials.euid != 0 && uid != process->credentials.uid) return RAD_STATUS_INVALID_ARGUMENT;
    process->credentials.uid = uid;
    process->credentials.euid = uid;
    return RAD_STATUS_OK;
}

rad_status_t rad_process_setgid(rad_gid_t gid) {
    ProcessRecord *process = find_process_record(current_process_pid());
    if (!process) return RAD_STATUS_NOT_FOUND;
    if (process->credentials.euid != 0 && gid != process->credentials.gid) return RAD_STATUS_INVALID_ARGUMENT;
    process->credentials.gid = gid;
    process->credentials.egid = gid;
    return RAD_STATUS_OK;
}

rad_status_t rad_module_register(const rad_module_descriptor_t *descriptor) {
    if (!descriptor || !descriptor->name || !*descriptor->name) return RAD_STATUS_INVALID_ARGUMENT;
    if (descriptor->size && descriptor->size < sizeof(rad_module_descriptor_t)) return RAD_STATUS_INVALID_ARGUMENT;
    if (find_module(descriptor->name)) return RAD_STATUS_ALREADY_EXISTS;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_MODULES; ++i) {
        ModuleRecord& module = g_state.modules[i];
        if (module.used) continue;
        memset(&module, 0, sizeof(module));
        module.used = 1;
        module.descriptor = *descriptor;
        copy_string(module.name, sizeof(module.name), descriptor->name);
        copy_string(module.bus, sizeof(module.bus), descriptor->bus);
        copy_string(module.compatible, sizeof(module.compatible), descriptor->compatible);
        module.state = RAD_MODULE_REGISTERED;
        module.last_status = RAD_STATUS_OK;
        if (descriptor->init) {
            module.last_status = descriptor->init(descriptor->context);
            module.state = module.last_status == RAD_STATUS_OK ? RAD_MODULE_INITIALIZED : RAD_MODULE_FAILED;
            return module.last_status;
        }
        module.state = RAD_MODULE_INITIALIZED;
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_module_unregister(const char *name) {
    ModuleRecord *module = find_module(name);
    if (!module) return RAD_STATUS_NOT_FOUND;
    if (module->state == RAD_MODULE_INITIALIZED && module->descriptor.exit) {
        module->descriptor.exit(module->descriptor.context);
    }
    module->state = RAD_MODULE_EXITED;
    memset(module, 0, sizeof(*module));
    return RAD_STATUS_OK;
}

size_t rad_module_list(rad_module_info_t *modules, size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_MODULES; ++i) {
        const ModuleRecord& module = g_state.modules[i];
        if (!module.used) continue;
        if (modules && count < capacity) {
            copy_string(modules[count].name, sizeof(modules[count].name), module.name);
            copy_string(modules[count].bus, sizeof(modules[count].bus), module.bus);
            copy_string(modules[count].compatible, sizeof(modules[count].compatible), module.compatible);
            modules[count].state = module.state;
            modules[count].last_status = module.last_status;
        }
        ++count;
    }
    return count;
}

rad_status_t rad_irq_register(uint32_t irq, const char *name, rad_irq_handler_t handler, void *context) {
    if (!name || !*name || !handler) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_IRQS; ++i) {
        if (g_state.irqs[i].used && g_state.irqs[i].irq == irq) return RAD_STATUS_ALREADY_EXISTS;
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_IRQS; ++i) {
        IrqRecord& record = g_state.irqs[i];
        if (record.used) continue;
        memset(&record, 0, sizeof(record));
        record.used = 1;
        record.irq = irq;
        copy_string(record.name, sizeof(record.name), name);
        record.handler = handler;
        record.context = context;
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_irq_unregister(uint32_t irq) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_IRQS; ++i) {
        if (!g_state.irqs[i].used || g_state.irqs[i].irq != irq) continue;
        memset(&g_state.irqs[i], 0, sizeof(g_state.irqs[i]));
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_irq_enable(uint32_t irq) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_IRQS; ++i) {
        IrqRecord& record = g_state.irqs[i];
        if (!record.used || record.irq != irq) continue;
        record.enabled = 1;
        if (rad_hal_irq_enable) {
            const rad_status_t status = rad_hal_irq_enable(irq);
            if (status != RAD_STATUS_OK && status != RAD_STATUS_NOT_SUPPORTED) return status;
        }
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_irq_disable(uint32_t irq) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_IRQS; ++i) {
        IrqRecord& record = g_state.irqs[i];
        if (!record.used || record.irq != irq) continue;
        record.enabled = 0;
        if (rad_hal_irq_disable) {
            const rad_status_t status = rad_hal_irq_disable(irq);
            if (status != RAD_STATUS_OK && status != RAD_STATUS_NOT_SUPPORTED) return status;
        }
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_irq_dispatch(uint32_t irq) {
    for (size_t i = 0; i < RADIX_KERNEL_MAX_IRQS; ++i) {
        IrqRecord& record = g_state.irqs[i];
        if (!record.used || record.irq != irq) continue;
        if (!record.enabled || !record.handler) {
            ++record.unhandled_count;
            return RAD_STATUS_NOT_SUPPORTED;
        }
        ++record.count;
        record.handler(irq, record.context);
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_FOUND;
}

size_t rad_irq_list(rad_irq_info_t *irqs, size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_IRQS; ++i) {
        const IrqRecord& record = g_state.irqs[i];
        if (!record.used) continue;
        if (irqs && count < capacity) {
            irqs[count].irq = record.irq;
            copy_string(irqs[count].name, sizeof(irqs[count].name), record.name);
            irqs[count].registered = 1;
            irqs[count].enabled = record.enabled;
            irqs[count].count = record.count;
            irqs[count].unhandled_count = record.unhandled_count;
        }
        ++count;
    }
    return count;
}

int32_t rad_fd_open(const char *path, uint32_t flags) {
    if (!path) return RAD_STATUS_INVALID_ARGUMENT;
    FdRecord record{};
    record.flags = flags;
    record.refs = 1;
    record.owner_fd = -1;
    record.owner_pid = current_process_pid();
    record.local_fd = -1;
    if (strncmp(path, "/dev/", 5) == 0) {
        rad_status_t status = rad_device_open(path, &record.device);
        if (status != RAD_STATUS_OK) return status;
        record.kind = FD_DEVICE;
    } else if (flags & RAD_VFS_DIRECTORY) {
        rad_status_t status = rad_vfs_opendir(path, &record.dir);
        if (status != RAD_STATUS_OK) return status;
        record.kind = FD_DIR;
    } else {
        rad_status_t status = rad_vfs_open(path, flags, &record.file);
        if (status != RAD_STATUS_OK) return status;
        record.kind = FD_FILE;
    }
    return install_fd_record(record, -1);
}

int32_t rad_fd_close(int32_t fd) {
    const int32_t slot = find_fd_slot(fd, current_process_pid(), true);
    if (slot < 0) return RAD_STATUS_NOT_FOUND;
    close_fd_index(slot);
    return RAD_STATUS_OK;
}

intptr_t rad_fd_read(int32_t fd, void *buffer, size_t size) {
    FdRecord *record = lookup_fd_record(fd);
    if (!record) return RAD_STATUS_NOT_FOUND;
    size_t done = 0;
    if (record->kind == FD_FILE) {
        const rad_status_t status = rad_vfs_read(record->file, buffer, size, &done);
        return status == RAD_STATUS_OK ? static_cast<intptr_t>(done) : static_cast<intptr_t>(status);
    }
    if (record->kind == FD_DEVICE) {
        TtyRecord *tty = tty_record_from_device(record->device);
        for (;;) {
            const rad_status_t status = rad_device_read(record->device, buffer, size, &done);
            if (status != RAD_STATUS_OK) return static_cast<intptr_t>(status);
            if (done > 0 || size == 0 || !tty) return static_cast<intptr_t>(done);
            if (record->flags & RAD_FD_NONBLOCK) return RAD_STATUS_TIMEOUT;
            rad_task_yield();
        }
    }
    if (record->kind == FD_PIPE) return pipe_read(record, buffer, size);
    return RAD_STATUS_INVALID_ARGUMENT;
}

intptr_t rad_fd_write(int32_t fd, const void *buffer, size_t size) {
    FdRecord *record = lookup_fd_record(fd);
    if (!record) return RAD_STATUS_NOT_FOUND;
    size_t done = 0;
    if (record->kind == FD_FILE) {
        const rad_status_t status = rad_vfs_write(record->file, buffer, size, &done);
        return status == RAD_STATUS_OK ? static_cast<intptr_t>(done) : static_cast<intptr_t>(status);
    }
    if (record->kind == FD_DEVICE) {
        const rad_status_t status = rad_device_write(record->device, buffer, size, &done);
        return status == RAD_STATUS_OK ? static_cast<intptr_t>(done) : static_cast<intptr_t>(status);
    }
    if (record->kind == FD_PIPE) return pipe_write(record, buffer, size);
    return RAD_STATUS_INVALID_ARGUMENT;
}

intptr_t rad_fd_getdents(int32_t fd, rad_dirent_user_t *entries, size_t capacity) {
    FdRecord *record = lookup_fd_record(fd);
    if (!record || !entries) return RAD_STATUS_INVALID_ARGUMENT;
    if (record->kind != FD_DIR) return RAD_STATUS_NOT_SUPPORTED;
    size_t count = 0;
    while (count < capacity) {
        rad_vfs_dirent_t entry{};
        const rad_status_t status = rad_vfs_readdir(record->dir, &entry);
        if (status == RAD_STATUS_NOT_FOUND) break;
        if (status != RAD_STATUS_OK) return status;
        entries[count].type = entry.stat.is_directory ? 2u : 1u;
        copy_string(entries[count].name, sizeof(entries[count].name), entry.name);
        ++count;
    }
    return static_cast<intptr_t>(count);
}

int64_t rad_fd_lseek(int32_t fd, int64_t offset, rad_seek_origin_t origin) {
    FdRecord *record = lookup_fd_record(fd);
    if (!record) return RAD_STATUS_NOT_FOUND;
    if (record->kind != FD_FILE) return RAD_STATUS_NOT_SUPPORTED;
    rad_status_t status = rad_vfs_seek(record->file, offset, origin);
    return status == RAD_STATUS_OK ? static_cast<int64_t>(rad_vfs_tell(record->file)) : static_cast<int64_t>(status);
}

int32_t rad_fd_ioctl(int32_t fd, uint32_t request, void *argument) {
    FdRecord *record = lookup_fd_record(fd);
    if (!record) return RAD_STATUS_NOT_FOUND;
    if (record->kind != FD_DEVICE) return RAD_STATUS_NOT_SUPPORTED;
    return rad_device_ioctl(record->device, request, argument);
}

int32_t rad_fd_stat(const char *path, rad_vfs_stat_t *stat) {
    return rad_vfs_stat(path, stat);
}

int32_t rad_fd_fstat(int32_t fd, rad_vfs_stat_t *stat) {
    if (!stat) return RAD_STATUS_INVALID_ARGUMENT;
    FdRecord *record = lookup_fd_record(fd);
    if (!record) return RAD_STATUS_NOT_FOUND;
    if (record->kind == FD_DEVICE) {
        memset(stat, 0, sizeof(*stat));
        stat->mode = 0020000u;
        stat->uid = 0;
        stat->gid = 0;
        return RAD_STATUS_OK;
    }
    if (record->kind == FD_PIPE) {
        memset(stat, 0, sizeof(*stat));
        stat->mode = 0010000u;
        stat->uid = 0;
        stat->gid = 0;
        return RAD_STATUS_OK;
    }
    if (record->kind == FD_SHM) {
        memset(stat, 0, sizeof(*stat));
        stat->mode = 0100000u;
        stat->uid = 0;
        stat->gid = 0;
        return RAD_STATUS_OK;
    }
    if (record->kind == FD_DIR) {
        memset(stat, 0, sizeof(*stat));
        stat->is_directory = 1;
        stat->mode = 0040755u;
        stat->uid = 0;
        stat->gid = 0;
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

int32_t rad_pipe_create(int32_t pipefd[2]) {
    if (!pipefd) return RAD_STATUS_INVALID_ARGUMENT;
    size_t pipe_index = RADIX_KERNEL_MAX_PIPES;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PIPES; ++i) {
        if (!g_state.pipes[i].used) {
            pipe_index = i;
            break;
        }
    }
    if (pipe_index >= RADIX_KERNEL_MAX_PIPES) return RAD_STATUS_NO_MEMORY;
    PipeRecord& pipe = g_state.pipes[pipe_index];
    memset(&pipe, 0, sizeof(pipe));
    pipe.used = 1;
    pipe.read_refs = 1;
    pipe.write_refs = 1;

    FdRecord read_end{};
    read_end.kind = FD_PIPE;
    read_end.pipe_index = pipe_index;
    read_end.pipe_write_end = 0;
    read_end.flags = RAD_VFS_READ;
    read_end.refs = 1;
    read_end.owner_fd = -1;
    read_end.owner_pid = current_process_pid();
    read_end.local_fd = -1;
    const int32_t read_fd = install_fd_record(read_end, -1);
    if (read_fd < 0) {
        memset(&pipe, 0, sizeof(pipe));
        return read_fd;
    }

    FdRecord write_end{};
    write_end.kind = FD_PIPE;
    write_end.pipe_index = pipe_index;
    write_end.pipe_write_end = 1;
    write_end.flags = RAD_VFS_WRITE;
    write_end.refs = 1;
    write_end.owner_fd = -1;
    write_end.owner_pid = current_process_pid();
    write_end.local_fd = -1;
    const int32_t write_fd = install_fd_record(write_end, -1);
    if (write_fd < 0) {
        rad_fd_close(read_fd);
        memset(&pipe, 0, sizeof(pipe));
        return write_fd;
    }
    pipefd[0] = read_fd;
    pipefd[1] = write_fd;
    return RAD_STATUS_OK;
}

int32_t rad_socket_create(int domain, int type, int protocol) {
    if (domain != static_cast<int>(RAD_AF_INET)) return RAD_STATUS_NOT_SUPPORTED;
    if (type == static_cast<int>(RAD_SOCK_DGRAM)) {
        if (protocol != 0 && protocol != static_cast<int>(RAD_IPPROTO_UDP)) return RAD_STATUS_NOT_SUPPORTED;
        protocol = RAD_IPPROTO_UDP;
    } else if (type == static_cast<int>(RAD_SOCK_STREAM)) {
        if (protocol != 0 && protocol != static_cast<int>(RAD_IPPROTO_TCP)) return RAD_STATUS_NOT_SUPPORTED;
        protocol = RAD_IPPROTO_TCP;
    } else {
        return RAD_STATUS_NOT_SUPPORTED;
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_UDP_SOCKETS; ++i) {
        SocketRecord& socket = g_state.sockets[i];
        if (socket.used) continue;
        memset(&socket, 0, sizeof(socket));
        socket.used = 1;
        socket.domain = domain;
        socket.type = type;
        socket.protocol = protocol;
        socket.local_address = radix_default_ipv4_address();
        socket.peer_index = RADIX_KERNEL_MAX_UDP_SOCKETS;
        for (size_t p = 0; p < RADIX_KERNEL_MAX_UDP_SOCKETS; ++p) socket.pending_accepts[p] = RADIX_KERNEL_MAX_UDP_SOCKETS;
        FdRecord record{};
        record.kind = FD_SOCKET;
        record.socket_index = i;
        record.flags = RAD_VFS_READ | RAD_VFS_WRITE;
        record.refs = 1;
        record.owner_fd = -1;
        record.owner_pid = current_process_pid();
        record.local_fd = -1;
        const int32_t fd = install_fd_record(record, -1);
        if (fd < 0) memset(&socket, 0, sizeof(socket));
        return fd;
    }
    return RAD_STATUS_NO_MEMORY;
}

int32_t rad_socket_bind(int32_t fd, const rad_sockaddr_in_t *address, size_t address_length) {
    if (!address || address_length < sizeof(rad_sockaddr_in_t) || address->family != RAD_AF_INET || address->port == 0) return RAD_STATUS_INVALID_ARGUMENT;
    FdRecord *record = lookup_fd_record(fd);
    SocketRecord *socket = socket_from_fd(record);
    if (!socket) return RAD_STATUS_NOT_FOUND;
    if (find_socket(socket->type, address->port) && find_socket(socket->type, address->port) != socket) return RAD_STATUS_ALREADY_EXISTS;
    socket->local_port = address->port;
    socket->local_address = address->address.bytes[0] ? address->address : radix_default_ipv4_address();
    return RAD_STATUS_OK;
}

int32_t rad_socket_listen(int32_t fd, int backlog) {
    SocketRecord *socket = socket_from_fd(lookup_fd_record(fd));
    if (!socket) return RAD_STATUS_NOT_FOUND;
    if (socket->type != static_cast<int>(RAD_SOCK_STREAM) || !socket->local_port || backlog < 0) return RAD_STATUS_INVALID_ARGUMENT;
    socket->tcp_state = RAD_TCP_LISTEN;
    return RAD_STATUS_OK;
}

int32_t rad_socket_accept(int32_t fd, rad_sockaddr_in_t *address, size_t *address_length) {
    SocketRecord *listener = socket_from_fd(lookup_fd_record(fd));
    if (!listener) return RAD_STATUS_NOT_FOUND;
    if (listener->type != static_cast<int>(RAD_SOCK_STREAM) || listener->tcp_state != RAD_TCP_LISTEN) return RAD_STATUS_INVALID_ARGUMENT;
    if (listener->pending_count == 0) return RAD_STATUS_NOT_FOUND;
    const size_t accepted_index = listener->pending_accepts[0];
    if (accepted_index >= RADIX_KERNEL_MAX_UDP_SOCKETS || !g_state.sockets[accepted_index].used) return RAD_STATUS_NOT_FOUND;
    for (size_t i = 1; i < listener->pending_count; ++i) listener->pending_accepts[i - 1] = listener->pending_accepts[i];
    --listener->pending_count;
    FdRecord record{};
    record.kind = FD_SOCKET;
    record.socket_index = accepted_index;
    record.flags = RAD_VFS_READ | RAD_VFS_WRITE;
    record.refs = 1;
    record.owner_fd = -1;
    record.owner_pid = current_process_pid();
    record.local_fd = -1;
    SocketRecord& accepted = g_state.sockets[accepted_index];
    if (address && address_length && *address_length >= sizeof(rad_sockaddr_in_t)) {
        address->family = RAD_AF_INET;
        address->port = accepted.remote_port;
        address->address = accepted.remote_address;
        *address_length = sizeof(rad_sockaddr_in_t);
    }
    return install_fd_record(record, -1);
}

int32_t rad_socket_connect(int32_t fd, const rad_sockaddr_in_t *address, size_t address_length) {
    if (!address || address_length < sizeof(rad_sockaddr_in_t) || address->family != RAD_AF_INET || address->port == 0) return RAD_STATUS_INVALID_ARGUMENT;
    FdRecord *client_fd = lookup_fd_record(fd);
    SocketRecord *client = socket_from_fd(client_fd);
    if (!client || client->type != static_cast<int>(RAD_SOCK_STREAM)) return RAD_STATUS_INVALID_ARGUMENT;
    SocketRecord *listener = find_socket(static_cast<int>(RAD_SOCK_STREAM), address->port);
    if (!listener || listener->tcp_state != RAD_TCP_LISTEN || listener->pending_count >= RADIX_KERNEL_MAX_UDP_SOCKETS) return RAD_STATUS_NOT_FOUND;
    size_t server_index = RADIX_KERNEL_MAX_UDP_SOCKETS;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_UDP_SOCKETS; ++i) {
        if (!g_state.sockets[i].used) {
            server_index = i;
            break;
        }
    }
    if (server_index >= RADIX_KERNEL_MAX_UDP_SOCKETS) return RAD_STATUS_NO_MEMORY;
    if (!client->local_port) client->local_port = static_cast<uint16_t>(50000u + (client_fd->socket_index % 1000u));
    client->remote_port = address->port;
    client->remote_address = address->address;
    client->tcp_state = RAD_TCP_ESTABLISHED;
    SocketRecord& server = g_state.sockets[server_index];
    memset(&server, 0, sizeof(server));
    server.used = 1;
    server.domain = RAD_AF_INET;
    server.type = RAD_SOCK_STREAM;
    server.protocol = RAD_IPPROTO_TCP;
    server.tcp_state = RAD_TCP_ESTABLISHED;
    server.local_port = listener->local_port;
    server.local_address = listener->local_address;
    server.remote_port = client->local_port;
    server.remote_address = client->local_address;
    server.peer_index = client_fd->socket_index;
    client->peer_index = server_index;
    listener->pending_accepts[listener->pending_count++] = server_index;
    return RAD_STATUS_OK;
}

intptr_t rad_socket_sendto(int32_t fd, const void *buffer, size_t size, uint32_t, const rad_sockaddr_in_t *address, size_t address_length) {
    if ((!buffer && size) || !address || address_length < sizeof(rad_sockaddr_in_t) || address->family != RAD_AF_INET || address->port == 0) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    FdRecord *record = lookup_fd_record(fd);
    SocketRecord *socket = socket_from_fd(record);
    if (!socket) return RAD_STATUS_NOT_FOUND;
    if (!socket->local_port) socket->local_port = static_cast<uint16_t>(49152u + (record->socket_index % 1024u));
    const rad_net_stack_config_t& config = active_net_config();
    if (!socket->local_address.bytes[0]) socket->local_address = config.ipv4;
    if (ipv4_equal(address->address, socket->local_address) || ipv4_equal(address->address, config.ipv4)) {
        rad_sockaddr_in_t from{};
        from.family = RAD_AF_INET;
        from.port = socket->local_port;
        from.address = socket->local_address;
        if (SocketRecord *target = find_udp_socket(address->port)) {
            if (enqueue_udp_datagram(target, &from, buffer, size) == RAD_STATUS_OK) return static_cast<intptr_t>(size);
        }
    }
    const rad_status_t status = send_udp_frame(socket, address, buffer, size);
    return status == RAD_STATUS_OK || status == RAD_STATUS_NOT_FOUND ? static_cast<intptr_t>(size) : static_cast<intptr_t>(status);
}

intptr_t rad_socket_recvfrom(int32_t fd, void *buffer, size_t size, uint32_t, rad_sockaddr_in_t *address, size_t *address_length) {
    if (!buffer && size) return RAD_STATUS_INVALID_ARGUMENT;
    FdRecord *record = lookup_fd_record(fd);
    SocketRecord *socket = socket_from_fd(record);
    if (!socket) return RAD_STATUS_NOT_FOUND;
    poll_network_for_udp();
    for (size_t i = 0; i < RADIX_KERNEL_MAX_UDP_DATAGRAMS; ++i) {
        UdpDatagram& datagram = socket->datagrams[i];
        if (!datagram.used) continue;
        const size_t count = datagram.size < size ? datagram.size : size;
        if (count) memcpy(buffer, datagram.payload, count);
        if (address && address_length && *address_length >= sizeof(rad_sockaddr_in_t)) {
            *address = datagram.from;
            *address_length = sizeof(rad_sockaddr_in_t);
        }
        memset(&datagram, 0, sizeof(datagram));
        return static_cast<intptr_t>(count);
    }
    return 0;
}

intptr_t rad_socket_send(int32_t fd, const void *buffer, size_t size, uint32_t) {
    if (!buffer && size) return RAD_STATUS_INVALID_ARGUMENT;
    SocketRecord *socket = socket_from_fd(lookup_fd_record(fd));
    if (!socket) return RAD_STATUS_NOT_FOUND;
    if (socket->type != static_cast<int>(RAD_SOCK_STREAM) || socket->tcp_state != RAD_TCP_ESTABLISHED || socket->peer_index >= RADIX_KERNEL_MAX_UDP_SOCKETS) return RAD_STATUS_INVALID_ARGUMENT;
    SocketRecord& peer = g_state.sockets[socket->peer_index];
    if (!peer.used || socket->shutdown_write || peer.shutdown_read) return RAD_STATUS_NOT_FOUND;
    const size_t capacity = RADIX_KERNEL_PIPE_BUFFER_BYTES - peer.stream_rx_size;
    const size_t count = capacity < size ? capacity : size;
    if (count) {
        memcpy(peer.stream_rx + peer.stream_rx_size, buffer, count);
        peer.stream_rx_size += count;
    }
    return static_cast<intptr_t>(count);
}

intptr_t rad_socket_recv(int32_t fd, void *buffer, size_t size, uint32_t) {
    if (!buffer && size) return RAD_STATUS_INVALID_ARGUMENT;
    SocketRecord *socket = socket_from_fd(lookup_fd_record(fd));
    if (!socket) return RAD_STATUS_NOT_FOUND;
    if (socket->type != static_cast<int>(RAD_SOCK_STREAM)) return RAD_STATUS_INVALID_ARGUMENT;
    const size_t count = socket->stream_rx_size < size ? socket->stream_rx_size : size;
    if (count) {
        memcpy(buffer, socket->stream_rx, count);
        memmove(socket->stream_rx, socket->stream_rx + count, socket->stream_rx_size - count);
        socket->stream_rx_size -= count;
    }
    return static_cast<intptr_t>(count);
}

int32_t rad_socket_shutdown(int32_t fd, int how) {
    SocketRecord *socket = socket_from_fd(lookup_fd_record(fd));
    if (!socket) return RAD_STATUS_NOT_FOUND;
    if (how == 0 || how == 2) socket->shutdown_read = 1;
    if (how == 1 || how == 2) socket->shutdown_write = 1;
    if (socket->tcp_state == RAD_TCP_ESTABLISHED) socket->tcp_state = RAD_TCP_FIN_WAIT;
    return RAD_STATUS_OK;
}

int32_t rad_socket_get_info(int32_t fd, rad_socket_info_t *info) {
    if (!info) return RAD_STATUS_INVALID_ARGUMENT;
    SocketRecord *socket = socket_from_fd(lookup_fd_record(fd));
    if (!socket) return RAD_STATUS_NOT_FOUND;
    memset(info, 0, sizeof(*info));
    info->size = sizeof(*info);
    info->domain = socket->domain;
    info->type = socket->type;
    info->protocol = socket->protocol;
    info->tcp_state = socket->tcp_state;
    info->local_port = socket->local_port;
    info->remote_port = socket->remote_port;
    info->local_address = socket->local_address;
    info->remote_address = socket->remote_address;
    return RAD_STATUS_OK;
}

int32_t rad_socket_setsockopt(int32_t fd, int, int, const void*, size_t) {
    return socket_from_fd(lookup_fd_record(fd)) ? RAD_STATUS_OK : RAD_STATUS_NOT_FOUND;
}

int32_t rad_socket_getsockopt(int32_t fd, int, int, void *value, size_t *value_length) {
    if (!socket_from_fd(lookup_fd_record(fd))) return RAD_STATUS_NOT_FOUND;
    if (!value || !value_length || *value_length < sizeof(int32_t)) return RAD_STATUS_INVALID_ARGUMENT;
    int32_t zero = 0;
    memcpy(value, &zero, sizeof(zero));
    *value_length = sizeof(zero);
    return RAD_STATUS_OK;
}

ShmRecord *find_shm_by_name(const char *name) {
    if (!name || !*name) return nullptr;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SHM_OBJECTS; ++i) {
        if (g_state.shm_objects[i].used && g_state.shm_objects[i].linked && strcmp(g_state.shm_objects[i].name, name) == 0) {
            return &g_state.shm_objects[i];
        }
    }
    return nullptr;
}

int32_t install_shm_fd(size_t shm_index, uint32_t flags) {
    if (shm_index >= RADIX_KERNEL_MAX_SHM_OBJECTS || !g_state.shm_objects[shm_index].used) return RAD_STATUS_INVALID_ARGUMENT;
    FdRecord record{};
    record.kind = FD_SHM;
    record.shm_index = shm_index;
    record.flags = flags;
    record.refs = 1;
    record.owner_fd = -1;
    record.owner_pid = current_process_pid();
    record.local_fd = -1;
    return install_fd_record(record, -1);
}

int32_t rad_shm_open(const char *name, size_t byte_size, uint32_t flags) {
    if (!name || !*name || byte_size == 0) return RAD_STATUS_INVALID_ARGUMENT;
    size_t length = strlen(name);
    if (length >= RAD_SHM_NAME_MAX) return RAD_STATUS_INVALID_ARGUMENT;
    size_t page_count = (byte_size + 4095u) / 4096u;
    if (page_count == 0 || page_count > RAD_SHM_MAX_PAGES) return RAD_STATUS_INVALID_ARGUMENT;
    ShmRecord *existing = find_shm_by_name(name);
    if (existing) return install_shm_fd(static_cast<size_t>(existing - g_state.shm_objects), flags);
    for (size_t i = 0; i < RADIX_KERNEL_MAX_SHM_OBJECTS; ++i) {
        ShmRecord& shm = g_state.shm_objects[i];
        if (shm.used) continue;
        memset(&shm, 0, sizeof(shm));
        shm.used = 1;
        shm.linked = 1;
        shm.byte_size = byte_size;
        shm.page_count = page_count;
        copy_string(shm.name, sizeof(shm.name), name);
        return install_shm_fd(i, flags);
    }
    return RAD_STATUS_NO_MEMORY;
}

int32_t rad_shm_unlink(const char *name) {
    ShmRecord *shm = find_shm_by_name(name);
    if (!shm) return RAD_STATUS_NOT_FOUND;
    memset(shm, 0, sizeof(*shm));
    return RAD_STATUS_OK;
}

int32_t rad_shm_get_info(int32_t fd, rad_shm_info_t *info) {
    if (!info) return RAD_STATUS_INVALID_ARGUMENT;
    FdRecord *record = lookup_fd_record(fd);
    if (!record || record->kind != FD_SHM || record->shm_index >= RADIX_KERNEL_MAX_SHM_OBJECTS) return RAD_STATUS_NOT_FOUND;
    ShmRecord& shm = g_state.shm_objects[record->shm_index];
    if (!shm.used) return RAD_STATUS_NOT_FOUND;
    memset(info, 0, sizeof(*info));
    info->size = sizeof(*info);
    copy_string(info->name, sizeof(info->name), shm.name);
    info->byte_size = shm.byte_size;
    info->page_count = shm.page_count;
    return RAD_STATUS_OK;
}

int32_t rad_shm_set_page(int32_t fd, size_t page_index, uintptr_t page_token) {
    FdRecord *record = lookup_fd_record(fd);
    if (!record || record->kind != FD_SHM || record->shm_index >= RADIX_KERNEL_MAX_SHM_OBJECTS) return RAD_STATUS_NOT_FOUND;
    ShmRecord& shm = g_state.shm_objects[record->shm_index];
    if (!shm.used || page_index >= shm.page_count || page_index >= RAD_SHM_MAX_PAGES) return RAD_STATUS_INVALID_ARGUMENT;
    shm.pages[page_index] = page_token;
    return RAD_STATUS_OK;
}

int32_t rad_shm_get_page(int32_t fd, size_t page_index, uintptr_t *page_token) {
    if (!page_token) return RAD_STATUS_INVALID_ARGUMENT;
    FdRecord *record = lookup_fd_record(fd);
    if (!record || record->kind != FD_SHM || record->shm_index >= RADIX_KERNEL_MAX_SHM_OBJECTS) return RAD_STATUS_NOT_FOUND;
    ShmRecord& shm = g_state.shm_objects[record->shm_index];
    if (!shm.used || page_index >= shm.page_count || page_index >= RAD_SHM_MAX_PAGES || !shm.pages[page_index]) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    *page_token = shm.pages[page_index];
    return RAD_STATUS_OK;
}

void *rad_shm_kernel_pointer(int32_t fd) {
    uintptr_t page = 0;
    if (rad_shm_get_page(fd, 0, &page) != RAD_STATUS_OK) return nullptr;
    return reinterpret_cast<void*>(page);
}

int32_t rad_fd_dup(int32_t fd) {
    const int32_t slot = find_fd_slot(fd, current_process_pid(), true);
    const int32_t owner_slot = fd_owner_index(slot);
    if (owner_slot < 0) return RAD_STATUS_NOT_FOUND;
    FdRecord& owner = g_state.fds[owner_slot];
    FdRecord copy = owner;
    ++owner.refs;
    copy.owner_fd = owner_slot;
    copy.local_fd = -1;
    copy.owner_pid = current_process_pid();
    return install_fd_record(copy, -1);
}

int32_t rad_fd_dup2(int32_t old_fd, int32_t new_fd) {
    if (new_fd < 0) return RAD_STATUS_INVALID_ARGUMENT;
    if (old_fd == new_fd) return new_fd;
    const int32_t slot = find_fd_slot(old_fd, current_process_pid(), true);
    const int32_t owner_slot = fd_owner_index(slot);
    if (owner_slot < 0) return RAD_STATUS_NOT_FOUND;
    FdRecord& owner = g_state.fds[owner_slot];
    FdRecord copy = owner;
    ++owner.refs;
    copy.owner_fd = owner_slot;
    copy.local_fd = new_fd;
    copy.owner_pid = current_process_pid();
    return install_fd_record(copy, new_fd);
}

int32_t rad_fd_fcntl(int32_t fd, uint32_t command, uintptr_t argument) {
    const int32_t slot = find_fd_slot(fd, current_process_pid(), true);
    if (slot < 0) return RAD_STATUS_NOT_FOUND;
    FdRecord& descriptor = g_state.fds[slot];
    const int32_t owner = fd_owner_index(slot);
    FdRecord *object = owner >= 0 ? &g_state.fds[owner] : nullptr;
    switch (command) {
    case RAD_FCNTL_GETFD:
        return static_cast<int32_t>(descriptor.descriptor_flags);
    case RAD_FCNTL_SETFD:
        descriptor.descriptor_flags = static_cast<uint32_t>(argument) & RAD_FD_CLOEXEC;
        return RAD_STATUS_OK;
    case RAD_FCNTL_GETFL:
        return object ? static_cast<int32_t>(object->flags) : RAD_STATUS_NOT_FOUND;
    case RAD_FCNTL_SETFL:
        if (!object) return RAD_STATUS_NOT_FOUND;
        object->flags = (object->flags & ~RAD_FD_NONBLOCK) | (static_cast<uint32_t>(argument) & RAD_FD_NONBLOCK);
        return RAD_STATUS_OK;
    default:
        return RAD_STATUS_NOT_SUPPORTED;
    }
}

int16_t fd_poll_events(FdRecord *record, int16_t requested) {
    if (!record || !record->used) return RAD_POLLNVAL;
    int16_t ready = 0;
    switch (record->kind) {
    case FD_FILE:
    case FD_DIR:
    case FD_SHM:
        if (requested & RAD_POLLIN) ready |= RAD_POLLIN;
        if (requested & RAD_POLLOUT) ready |= RAD_POLLOUT;
        break;
    case FD_DEVICE:
        if (requested & RAD_POLLOUT) ready |= RAD_POLLOUT;
        if (requested & RAD_POLLIN) {
            TtyRecord *tty = tty_record_from_device(record->device);
            if (!tty || tty_record_input_ready(tty)) ready |= RAD_POLLIN;
        }
        break;
    case FD_PIPE:
        if (record->pipe_index >= RADIX_KERNEL_MAX_PIPES || !g_state.pipes[record->pipe_index].used) {
            ready |= RAD_POLLNVAL;
            break;
        }
        if (record->pipe_write_end) {
            PipeRecord& pipe = g_state.pipes[record->pipe_index];
            if ((requested & RAD_POLLOUT) && pipe.read_refs > 0 && pipe.size < RADIX_KERNEL_PIPE_BUFFER_BYTES) ready |= RAD_POLLOUT;
            if (pipe.read_refs <= 0) ready |= RAD_POLLHUP;
        } else {
            PipeRecord& pipe = g_state.pipes[record->pipe_index];
            if ((requested & RAD_POLLIN) && pipe.size > 0) ready |= RAD_POLLIN;
            if (pipe.write_refs <= 0) ready |= RAD_POLLHUP;
        }
        break;
    case FD_SOCKET:
        if (record->socket_index >= RADIX_KERNEL_MAX_UDP_SOCKETS || !g_state.sockets[record->socket_index].used) {
            ready |= RAD_POLLNVAL;
            break;
        }
        {
            SocketRecord& socket = g_state.sockets[record->socket_index];
            if (requested & RAD_POLLOUT) ready |= RAD_POLLOUT;
            if (requested & RAD_POLLIN) {
                if (socket.pending_count > 0 || socket.stream_rx_size > 0) ready |= RAD_POLLIN;
                for (size_t i = 0; i < RADIX_KERNEL_MAX_UDP_DATAGRAMS; ++i) {
                    if (socket.datagrams[i].used) {
                        ready |= RAD_POLLIN;
                        break;
                    }
                }
            }
            if (socket.shutdown_read || socket.shutdown_write || socket.tcp_state == RAD_TCP_FIN_WAIT) ready |= RAD_POLLHUP;
        }
        break;
    default:
        ready |= RAD_POLLNVAL;
        break;
    }
    return ready;
}

int32_t rad_fd_poll(rad_pollfd_t *fds, size_t count, int32_t timeout_ms) {
    if (!fds && count) return RAD_STATUS_INVALID_ARGUMENT;
    if (count > 64u) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t start = rad_time_millis();
    for (;;) {
        int32_t ready_count = 0;
        for (size_t i = 0; i < count; ++i) {
            fds[i].revents = 0;
            if (fds[i].fd < 0) continue;
            FdRecord *record = lookup_fd_record(fds[i].fd);
            fds[i].revents = fd_poll_events(record, fds[i].events);
            if (fds[i].revents) ++ready_count;
        }
        if (ready_count > 0 || timeout_ms == 0) return ready_count;
        if (timeout_ms > 0 && rad_time_millis() - start >= static_cast<uint64_t>(timeout_ms)) return 0;
        rad_task_sleep_ms(1);
    }
}

intptr_t rad_syscall_dispatch(uintptr_t number, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5) {
    switch (number) {
    case RAD_SYSCALL_READ: return rad_fd_read(static_cast<int32_t>(arg0), reinterpret_cast<void*>(arg1), static_cast<size_t>(arg2));
    case RAD_SYSCALL_WRITE: return rad_fd_write(static_cast<int32_t>(arg0), reinterpret_cast<const void*>(arg1), static_cast<size_t>(arg2));
    case RAD_SYSCALL_OPEN: return rad_fd_open(reinterpret_cast<const char*>(arg0), static_cast<uint32_t>(arg1));
    case RAD_SYSCALL_CLOSE: return rad_fd_close(static_cast<int32_t>(arg0));
    case RAD_SYSCALL_IOCTL: return rad_fd_ioctl(static_cast<int32_t>(arg0), static_cast<uint32_t>(arg1), reinterpret_cast<void*>(arg2));
    case RAD_SYSCALL_LSEEK: return static_cast<intptr_t>(rad_fd_lseek(static_cast<int32_t>(arg0), static_cast<int64_t>(arg1), static_cast<rad_seek_origin_t>(arg2)));
    case RAD_SYSCALL_STAT: return rad_fd_stat(reinterpret_cast<const char*>(arg0), reinterpret_cast<rad_vfs_stat_t*>(arg1));
    case RAD_SYSCALL_FSTAT: return rad_fd_fstat(static_cast<int32_t>(arg0), reinterpret_cast<rad_vfs_stat_t*>(arg1));
    case RAD_SYSCALL_GETTIMEOFDAY: {
        auto *tv = reinterpret_cast<rad_posix_timeval_t*>(arg0);
        if (!tv) return RAD_STATUS_INVALID_ARGUMENT;
        const uint64_t micros = rad_realtime_micros();
        tv->tv_sec = static_cast<int64_t>(micros / 1000000u);
        tv->tv_usec = static_cast<int64_t>(micros % 1000000u);
        return RAD_STATUS_OK;
    }
    case RAD_SYSCALL_SETTIMEOFDAY: {
        auto *tv = reinterpret_cast<const rad_posix_timeval_t*>(arg0);
        if (!tv || tv->tv_sec < 0 || tv->tv_usec < 0 || tv->tv_usec >= 1000000) return RAD_STATUS_INVALID_ARGUMENT;
        return rad_realtime_set_micros(static_cast<uint64_t>(tv->tv_sec) * 1000000u + static_cast<uint64_t>(tv->tv_usec));
    }
    case RAD_SYSCALL_NANOSLEEP:
        rad_sleep_us(static_cast<uint32_t>(arg0 / 1000u));
        return RAD_STATUS_OK;
    case RAD_SYSCALL_EXIT:
        rad_process_exit(static_cast<int32_t>(arg0));
        return RAD_STATUS_OK;
    case RAD_SYSCALL_FORK: return rad_process_fork();
    case RAD_SYSCALL_EXECVE: return rad_process_execve(reinterpret_cast<const char*>(arg0), reinterpret_cast<const char *const*>(arg1));
    case RAD_SYSCALL_WAITPID: return rad_process_waitpid(static_cast<int32_t>(arg0), reinterpret_cast<int32_t*>(arg1), static_cast<uint32_t>(arg2));
    case RAD_SYSCALL_KILL: return rad_process_kill(static_cast<int32_t>(arg0), static_cast<int32_t>(arg1));
    case RAD_SYSCALL_GETPID: return rad_process_current_pid();
    case RAD_SYSCALL_GETPPID: return rad_process_parent_pid();
    case RAD_SYSCALL_DUP: return rad_fd_dup(static_cast<int32_t>(arg0));
    case RAD_SYSCALL_DUP2: return rad_fd_dup2(static_cast<int32_t>(arg0), static_cast<int32_t>(arg1));
    case RAD_SYSCALL_CHDIR: return rad_vfs_chdir(reinterpret_cast<const char*>(arg0));
    case RAD_SYSCALL_GETCWD: return rad_vfs_getcwd(reinterpret_cast<char*>(arg0), static_cast<size_t>(arg1));
    case RAD_SYSCALL_BRK: return 0;
    case RAD_SYSCALL_PIPE: return rad_pipe_create(reinterpret_cast<int32_t*>(arg0));
    case RAD_SYSCALL_FCNTL: return rad_fd_fcntl(static_cast<int32_t>(arg0), static_cast<uint32_t>(arg1), arg2);
    case RAD_SYSCALL_GETDENTS: return rad_fd_getdents(static_cast<int32_t>(arg0), reinterpret_cast<rad_dirent_user_t*>(arg1), static_cast<size_t>(arg2));
    case RAD_SYSCALL_REMOVE: return rad_vfs_remove(reinterpret_cast<const char*>(arg0));
    case RAD_SYSCALL_MKDIR: return rad_vfs_mkdir(reinterpret_cast<const char*>(arg0));
    case RAD_SYSCALL_RMDIR: return rad_vfs_rmdir(reinterpret_cast<const char*>(arg0));
    case RAD_SYSCALL_RENAME: return rad_vfs_rename(reinterpret_cast<const char*>(arg0), reinterpret_cast<const char*>(arg1));
    case RAD_SYSCALL_TRUNCATE: return rad_vfs_truncate(reinterpret_cast<const char*>(arg0), static_cast<uint64_t>(arg1));
    case RAD_SYSCALL_LOG_READ: return static_cast<intptr_t>(rad_log_read(reinterpret_cast<rad_log_entry_t*>(arg0), static_cast<size_t>(arg1), static_cast<uint64_t>(arg2)));
    case RAD_SYSCALL_LOG_FLUSH: return rad_log_flush_to_path(reinterpret_cast<const char*>(arg0));
    case RAD_SYSCALL_ACCESS: {
        rad_vfs_stat_t stat{};
        return rad_vfs_stat(reinterpret_cast<const char*>(arg0), &stat);
    }
    case RAD_SYSCALL_ISATTY: {
        rad_vfs_stat_t stat{};
        const int32_t status = rad_fd_fstat(static_cast<int32_t>(arg0), &stat);
        return status == RAD_STATUS_OK && ((stat.mode & 0170000u) == 0020000u) ? 1 : 0;
    }
    case RAD_SYSCALL_SOCKET: return rad_socket_create(static_cast<int>(arg0), static_cast<int>(arg1), static_cast<int>(arg2));
    case RAD_SYSCALL_BIND: return rad_socket_bind(static_cast<int32_t>(arg0), reinterpret_cast<const rad_sockaddr_in_t*>(arg1), static_cast<size_t>(arg2));
    case RAD_SYSCALL_CONNECT: return rad_socket_connect(static_cast<int32_t>(arg0), reinterpret_cast<const rad_sockaddr_in_t*>(arg1), static_cast<size_t>(arg2));
    case RAD_SYSCALL_LISTEN: return rad_socket_listen(static_cast<int32_t>(arg0), static_cast<int>(arg1));
    case RAD_SYSCALL_ACCEPT: return rad_socket_accept(static_cast<int32_t>(arg0), reinterpret_cast<rad_sockaddr_in_t*>(arg1), reinterpret_cast<size_t*>(arg2));
    case RAD_SYSCALL_SHUTDOWN: return rad_socket_shutdown(static_cast<int32_t>(arg0), static_cast<int>(arg1));
    case RAD_SYSCALL_SENDTO:
        return arg4
            ? rad_socket_sendto(static_cast<int32_t>(arg0), reinterpret_cast<const void*>(arg1), static_cast<size_t>(arg2), static_cast<uint32_t>(arg3), reinterpret_cast<const rad_sockaddr_in_t*>(arg4), static_cast<size_t>(arg5))
            : rad_socket_send(static_cast<int32_t>(arg0), reinterpret_cast<const void*>(arg1), static_cast<size_t>(arg2), static_cast<uint32_t>(arg3));
    case RAD_SYSCALL_RECVFROM:
        return arg4
            ? rad_socket_recvfrom(static_cast<int32_t>(arg0), reinterpret_cast<void*>(arg1), static_cast<size_t>(arg2), static_cast<uint32_t>(arg3), reinterpret_cast<rad_sockaddr_in_t*>(arg4), reinterpret_cast<size_t*>(arg5))
            : rad_socket_recv(static_cast<int32_t>(arg0), reinterpret_cast<void*>(arg1), static_cast<size_t>(arg2), static_cast<uint32_t>(arg3));
    case RAD_SYSCALL_SETSOCKOPT: return rad_socket_setsockopt(static_cast<int32_t>(arg0), static_cast<int>(arg1), static_cast<int>(arg2), reinterpret_cast<const void*>(arg3), static_cast<size_t>(arg4));
    case RAD_SYSCALL_GETSOCKOPT: return rad_socket_getsockopt(static_cast<int32_t>(arg0), static_cast<int>(arg1), static_cast<int>(arg2), reinterpret_cast<void*>(arg3), reinterpret_cast<size_t*>(arg4));
    case RAD_SYSCALL_SHM_OPEN: return rad_shm_open(reinterpret_cast<const char*>(arg0), static_cast<size_t>(arg1), static_cast<uint32_t>(arg2));
    case RAD_SYSCALL_SHM_UNLINK: return rad_shm_unlink(reinterpret_cast<const char*>(arg0));
    case RAD_SYSCALL_POLL: return rad_fd_poll(reinterpret_cast<rad_pollfd_t*>(arg0), static_cast<size_t>(arg1), static_cast<int32_t>(arg2));
    case RAD_SYSCALL_GETUID: return static_cast<intptr_t>(rad_process_getuid());
    case RAD_SYSCALL_GETEUID: return static_cast<intptr_t>(rad_process_geteuid());
    case RAD_SYSCALL_GETGID: return static_cast<intptr_t>(rad_process_getgid());
    case RAD_SYSCALL_GETEGID: return static_cast<intptr_t>(rad_process_getegid());
    case RAD_SYSCALL_SETUID: return rad_process_setuid(static_cast<rad_uid_t>(arg0));
    case RAD_SYSCALL_SETGID: return rad_process_setgid(static_cast<rad_gid_t>(arg0));
    case RAD_SYSCALL_CHMOD: return rad_vfs_chmod(reinterpret_cast<const char*>(arg0), static_cast<uint32_t>(arg1));
    case RAD_SYSCALL_LINK: return rad_vfs_link(reinterpret_cast<const char*>(arg0), reinterpret_cast<const char*>(arg1));
    case RAD_SYSCALL_SYMLINK: return rad_vfs_symlink(reinterpret_cast<const char*>(arg0), reinterpret_cast<const char*>(arg1));
    case RAD_SYSCALL_READLINK: return rad_vfs_readlink(reinterpret_cast<const char*>(arg0), reinterpret_cast<char*>(arg1), static_cast<size_t>(arg2));
    case RAD_SYSCALL_FSYNC: {
        FdRecord *record = lookup_fd_record(static_cast<int32_t>(arg0));
        return record && record->kind == FD_FILE ? rad_vfs_fsync(record->file) : RAD_STATUS_NOT_SUPPORTED;
    }
    case RAD_SYSCALL_MMAP:
    case RAD_SYSCALL_MUNMAP:
        return RAD_STATUS_NOT_SUPPORTED;
    default: return RAD_STATUS_NOT_SUPPORTED;
    }
}

rad_status_t rad_program_load(const char *path, rad_program_t *program) {
    if (!path || !program) return RAD_STATUS_INVALID_ARGUMENT;
    *program = nullptr;
    rad_vfs_stat_t stat{};
    rad_status_t status = rad_vfs_stat(path, &stat);
    if (status != RAD_STATUS_OK) return status;
    if (stat.is_directory || stat.size == 0) return RAD_STATUS_INVALID_ARGUMENT;
    if (stat.size > static_cast<uint64_t>(SIZE_MAX)) return RAD_STATUS_NO_MEMORY;

    rad_file_t file = nullptr;
    status = rad_vfs_open(path, RAD_VFS_READ, &file);
    if (status != RAD_STATUS_OK) return status;
    auto *file_image = static_cast<uint8_t*>(rad_memory_alloc(static_cast<size_t>(stat.size)));
    if (!file_image) {
        rad_vfs_close(file);
        return RAD_STATUS_NO_MEMORY;
    }
    size_t bytes_read = 0;
    status = rad_vfs_read(file, file_image, static_cast<size_t>(stat.size), &bytes_read);
    rad_vfs_close(file);
    if (status != RAD_STATUS_OK || bytes_read != static_cast<size_t>(stat.size)) {
        rad_memory_free(file_image);
        return status == RAD_STATUS_OK ? RAD_STATUS_ERROR : status;
    }

    for (size_t i = 0; i < RADIX_KERNEL_MAX_PROGRAMS; ++i) {
        if (g_state.programs[i].used) continue;
        ProgramRecord& record = g_state.programs[i];
        memset(&record, 0, sizeof(record));
        record.used = 1;
        record.id = g_state.next_program_id++;
        record.state = RAD_PROGRAM_LOADED;
        record.exit_code = 0;
        char normalized[96];
        normalize_path(normalized, sizeof(normalized), path);
        copy_string(record.path, sizeof(record.path), normalized);
        program_name_from_path(record.name, sizeof(record.name), normalized);
        status = load_native_elf_image(record, file_image, static_cast<size_t>(stat.size));
        rad_memory_free(file_image);
        if (status != RAD_STATUS_OK) {
            if (record.image) rad_memory_free(record.image);
            memset(&record, 0, sizeof(record));
            return status;
        }
        auto *handle = static_cast<rad_program_handle*>(rad_memory_alloc(sizeof(rad_program_handle)));
        if (!handle) {
            rad_memory_free(record.image);
            memset(&record, 0, sizeof(record));
            return RAD_STATUS_NO_MEMORY;
        }
        handle->index = i;
        *program = handle;
        return RAD_STATUS_OK;
    }

    rad_memory_free(file_image);
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_program_spawn(rad_program_t program, int argc, const char **argv, rad_task_t *task) {
    if (!program || program->index >= RADIX_KERNEL_MAX_PROGRAMS) return RAD_STATUS_INVALID_ARGUMENT;
    ProgramRecord& record = g_state.programs[program->index];
    if (!record.used || !record.entry) return RAD_STATUS_INVALID_ARGUMENT;
    if (record.state == RAD_PROGRAM_RUNNING) return RAD_STATUS_INVALID_ARGUMENT;

    auto *launch = static_cast<ProgramLaunchContext*>(rad_memory_alloc(sizeof(ProgramLaunchContext)));
    if (!launch) return RAD_STATUS_NO_MEMORY;
    memset(launch, 0, sizeof(*launch));
    launch->program = &record;
    launch->argc = argc < 0 ? 0 : (argc > 8 ? 8 : argc);
    for (int i = 0; i < launch->argc; ++i) {
        copy_string(launch->args[i], sizeof(launch->args[i]), argv ? argv[i] : "");
    }

    rad_task_config_t config{};
    config.size = sizeof(config);
    config.name = record.name[0] ? record.name : "program";
    config.stack_size = 8192;
    config.target_core = RAD_TASK_CORE_ANY;
    config.priority = 0;
    const rad_status_t status = rad_task_create_config(&record.task, &config, program_task_entry, launch);
    if (status != RAD_STATUS_OK) {
        rad_memory_free(launch);
        return status;
    }
    if (task) *task = record.task;
    return RAD_STATUS_OK;
}

void rad_program_unload(rad_program_t program) {
    if (!program || program->index >= RADIX_KERNEL_MAX_PROGRAMS) return;
    ProgramRecord& record = g_state.programs[program->index];
    if (record.used && record.state != RAD_PROGRAM_RUNNING) {
        if (record.image) rad_memory_free(record.image);
        memset(&record, 0, sizeof(record));
    }
    rad_memory_free(program);
}

size_t rad_program_list(rad_program_info_t *programs, size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PROGRAMS; ++i) {
        if (!g_state.programs[i].used) continue;
        if (programs && count < capacity) {
            programs[count].id = g_state.programs[i].id;
            copy_string(programs[count].path, sizeof(programs[count].path), g_state.programs[i].path);
            copy_string(programs[count].name, sizeof(programs[count].name), g_state.programs[i].name);
            programs[count].state = g_state.programs[i].state;
            programs[count].task_id = g_state.programs[i].task ? g_state.programs[i].task->id : 0;
            programs[count].exit_code = g_state.programs[i].exit_code;
        }
        ++count;
    }
    return count;
}

rad_status_t rad_device_register(const char *name, rad_device_type_t type, const rad_device_ops_t *ops) {
    if (!name || !ops) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_DEVICES; ++i) {
        if (g_state.devices[i].used && strcmp(g_state.devices[i].name, name) == 0) return RAD_STATUS_ALREADY_EXISTS;
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_DEVICES; ++i) {
        if (g_state.devices[i].used) continue;
        g_state.devices[i].used = 1;
        copy_string(g_state.devices[i].name, sizeof(g_state.devices[i].name), name);
        g_state.devices[i].type = type;
        g_state.devices[i].ops = *ops;
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_device_unregister(const char *name) {
    if (!name) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_DEVICES; ++i) {
        if (g_state.devices[i].used && strcmp(g_state.devices[i].name, name) == 0) {
            g_state.devices[i].used = 0;
            return RAD_STATUS_OK;
        }
    }
    return RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_device_open(const char *name, rad_device_t *device) {
    if (!name || !device) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_DEVICES; ++i) {
        if (!g_state.devices[i].used || strcmp(g_state.devices[i].name, name) != 0) continue;
        *device = static_cast<rad_device_t>(rad_memory_alloc(sizeof(rad_device_handle)));
        if (!*device) return RAD_STATUS_NO_MEMORY;
        (*device)->record = g_state.devices[i];
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_FOUND;
}

void rad_device_close(rad_device_t device) { rad_memory_free(device); }

rad_status_t rad_device_read(rad_device_t device, void *buffer, size_t size, size_t *bytes_read) {
    if (!device || !device->record.ops.read) return RAD_STATUS_NOT_SUPPORTED;
    return device->record.ops.read(device->record.ops.context, buffer, size, bytes_read);
}

rad_status_t rad_device_write(rad_device_t device, const void *buffer, size_t size, size_t *bytes_written) {
    if (!device || !device->record.ops.write) return RAD_STATUS_NOT_SUPPORTED;
    return device->record.ops.write(device->record.ops.context, buffer, size, bytes_written);
}

rad_status_t rad_device_ioctl(rad_device_t device, uint32_t request, void *argument) {
    if (!device || !device->record.ops.ioctl) return RAD_STATUS_NOT_SUPPORTED;
    return device->record.ops.ioctl(device->record.ops.context, request, argument);
}

size_t rad_device_list(char names[][64], rad_device_type_t *types, size_t capacity) {
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_DEVICES; ++i) {
        if (!g_state.devices[i].used) continue;
        if (count < capacity) {
            if (names) copy_string(names[count], 64, g_state.devices[i].name);
            if (types) types[count] = g_state.devices[i].type;
        }
        ++count;
    }
    return count;
}

rad_status_t rad_input_device_register(const char *name, const rad_device_ops_t *ops) {
    return rad_device_register(name, RAD_DEVICE_INPUT, ops);
}

rad_status_t rad_input_device_unregister(const char *name) {
    return rad_device_unregister(name);
}

rad_status_t rad_input_open(const char *name, rad_device_t *device) {
    if (!name || !device) return RAD_STATUS_INVALID_ARGUMENT;
    rad_status_t status = rad_device_open(name, device);
    if (status != RAD_STATUS_OK) return status;
    if ((*device)->record.type != RAD_DEVICE_INPUT) {
        rad_device_close(*device);
        *device = nullptr;
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_input_read_event(rad_device_t device, rad_input_event_t *event) {
    if (!device || !event) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_INPUT) return RAD_STATUS_INVALID_ARGUMENT;
    size_t bytes = 0;
    const rad_status_t status = rad_device_read(device, event, sizeof(*event), &bytes);
    if (status != RAD_STATUS_OK) return status;
    if (bytes == 0) return RAD_STATUS_TIMEOUT;
    if (bytes < sizeof(*event)) return RAD_STATUS_ERROR;
    return RAD_STATUS_OK;
}

rad_status_t rad_input_queue_create(const char *name, size_t capacity, rad_input_queue_t *queue) {
    if (!queue) return RAD_STATUS_INVALID_ARGUMENT;
    if (capacity == 0 || capacity > RADIX_KERNEL_INPUT_QUEUE_EVENTS) capacity = RADIX_KERNEL_INPUT_QUEUE_EVENTS;
    lock_runtime();
    for (size_t i = 0; i < RADIX_KERNEL_MAX_INPUT_QUEUES; ++i) {
        if (g_state.input_queues[i].used) continue;
        InputQueueRecord& record = g_state.input_queues[i];
        memset(&record, 0, sizeof(record));
        record.used = 1;
        record.capacity = static_cast<uint32_t>(capacity);
        copy_string(record.name, sizeof(record.name), name ? name : "input");
        g_state.input_queue_handles[i].index = i;
        *queue = &g_state.input_queue_handles[i];
        unlock_runtime();
        return RAD_STATUS_OK;
    }
    unlock_runtime();
    return RAD_STATUS_NO_MEMORY;
}

void rad_input_queue_destroy(rad_input_queue_t queue) {
    if (!queue || queue->index >= RADIX_KERNEL_MAX_INPUT_QUEUES) return;
    lock_runtime();
    memset(&g_state.input_queues[queue->index], 0, sizeof(g_state.input_queues[queue->index]));
    memset(&g_state.input_queue_handles[queue->index], 0, sizeof(g_state.input_queue_handles[queue->index]));
    unlock_runtime();
}

rad_status_t rad_input_queue_push(rad_input_queue_t queue, const rad_input_event_t *event) {
    if (!queue || queue->index >= RADIX_KERNEL_MAX_INPUT_QUEUES || !event) return RAD_STATUS_INVALID_ARGUMENT;
    lock_runtime();
    InputQueueRecord& record = g_state.input_queues[queue->index];
    if (!record.used) {
        unlock_runtime();
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    if (record.count >= record.capacity) {
        unlock_runtime();
        return RAD_STATUS_NO_MEMORY;
    }
    record.events[record.head] = *event;
    record.head = (record.head + 1u) % record.capacity;
    ++record.count;
    unlock_runtime();
    rad_perf_counter_add("input.events", 1);
    return RAD_STATUS_OK;
}

rad_status_t rad_input_queue_read(rad_input_queue_t queue, rad_input_event_t *events, size_t capacity, uint32_t timeout_ms, size_t *count) {
    if (!queue || queue->index >= RADIX_KERNEL_MAX_INPUT_QUEUES || (!events && capacity)) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t start = rad_time_millis();
    size_t read = 0;
    for (;;) {
        lock_runtime();
        InputQueueRecord& record = g_state.input_queues[queue->index];
        if (!record.used) {
            unlock_runtime();
            return RAD_STATUS_INVALID_ARGUMENT;
        }
        while (read < capacity && record.count > 0) {
            events[read++] = record.events[record.tail];
            record.tail = (record.tail + 1u) % record.capacity;
            --record.count;
        }
        unlock_runtime();
        if (read || timeout_ms == 0) break;
        if (timeout_ms != RAD_WAIT_FOREVER && rad_time_millis() - start >= timeout_ms) break;
        rad_cpu_idle();
    }
    if (count) *count = read;
    return read ? RAD_STATUS_OK : RAD_STATUS_TIMEOUT;
}

rad_status_t rad_input_device_register_queue(const char *name, rad_input_queue_t queue) {
    if (!name || !queue) return RAD_STATUS_INVALID_ARGUMENT;
    rad_device_ops_t ops{};
    ops.context = queue;
    ops.read = input_queue_device_read;
    return rad_input_device_register(name, &ops);
}

rad_status_t rad_block_device_register(const char *name, const rad_device_ops_t *ops) {
    return rad_device_register(name, RAD_DEVICE_BLOCK, ops);
}

rad_status_t rad_block_open(const char *name, rad_device_t *device) {
    if (!name || !device) return RAD_STATUS_INVALID_ARGUMENT;
    rad_status_t status = rad_device_open(name, device);
    if (status != RAD_STATUS_OK) return status;
    if ((*device)->record.type != RAD_DEVICE_BLOCK) {
        rad_device_close(*device);
        *device = nullptr;
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_block_info(rad_device_t device, rad_block_info_t *info) {
    if (!device || !info) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_BLOCK) return RAD_STATUS_INVALID_ARGUMENT;
    info->size = sizeof(*info);
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_BLOCK_INFO, info);
}

rad_status_t rad_block_read(rad_device_t device, uint64_t sector, uint32_t sector_count, void *buffer) {
    if (!device || !buffer || sector_count == 0) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_BLOCK) return RAD_STATUS_INVALID_ARGUMENT;
    rad_block_request_t request{};
    request.size = sizeof(request);
    request.sector = sector;
    request.sector_count = sector_count;
    request.buffer = buffer;
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_BLOCK_READ, &request);
}

rad_status_t rad_block_write(rad_device_t device, uint64_t sector, uint32_t sector_count, const void *buffer) {
    if (!device || !buffer || sector_count == 0) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_BLOCK) return RAD_STATUS_INVALID_ARGUMENT;
    rad_block_request_t request{};
    request.size = sizeof(request);
    request.sector = sector;
    request.sector_count = sector_count;
    request.buffer = const_cast<void*>(buffer);
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_BLOCK_WRITE, &request);
}

rad_status_t rad_block_flush(rad_device_t device) {
    if (!device) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_BLOCK) return RAD_STATUS_INVALID_ARGUMENT;
    const rad_status_t status = rad_device_ioctl(device, RAD_DEVICE_IOCTL_BLOCK_FLUSH, nullptr);
    return status == RAD_STATUS_NOT_SUPPORTED ? RAD_STATUS_OK : status;
}

rad_status_t rad_net_device_register(const char *name, const rad_device_ops_t *ops) {
    return rad_device_register(name, RAD_DEVICE_NETWORK, ops);
}

rad_status_t rad_net_open(const char *name, rad_device_t *device) {
    if (!name || !device) return RAD_STATUS_INVALID_ARGUMENT;
    rad_status_t status = rad_device_open(name, device);
    if (status != RAD_STATUS_OK) return status;
    if ((*device)->record.type != RAD_DEVICE_NETWORK) {
        rad_device_close(*device);
        *device = nullptr;
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_net_link_info(rad_device_t device, rad_net_link_info_t *info) {
    if (!device || !info) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_NETWORK) return RAD_STATUS_INVALID_ARGUMENT;
    info->size = sizeof(*info);
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_NET_LINK_INFO, info);
}

rad_status_t rad_net_send(rad_device_t device, const void *data, size_t length) {
    if (!device || !data || !length) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_NETWORK) return RAD_STATUS_INVALID_ARGUMENT;
    rad_net_packet_t packet{};
    packet.size = sizeof(packet);
    packet.data = const_cast<void*>(data);
    packet.length = length;
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_NET_SEND, &packet);
}

intptr_t rad_net_receive(rad_device_t device, void *data, size_t length) {
    if (!device || !data || !length) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_NETWORK) return RAD_STATUS_INVALID_ARGUMENT;
    rad_net_packet_t packet{};
    packet.size = sizeof(packet);
    packet.data = data;
    packet.length = length;
    const rad_status_t status = rad_device_ioctl(device, RAD_DEVICE_IOCTL_NET_RECV, &packet);
    return status == RAD_STATUS_OK ? static_cast<intptr_t>(packet.length) : static_cast<intptr_t>(status);
}

rad_status_t rad_net_poll(rad_device_t device) {
    if (!device) return RAD_STATUS_INVALID_ARGUMENT;
    if (device->record.type != RAD_DEVICE_NETWORK) return RAD_STATUS_INVALID_ARGUMENT;
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_NET_POLL, nullptr);
}

rad_status_t rad_spi_transfer_device(rad_device_t device, const rad_spi_transfer_t *transfer) {
    if (!transfer) return RAD_STATUS_INVALID_ARGUMENT;
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_SPI_TRANSFER, const_cast<rad_spi_transfer_t*>(transfer));
}

rad_status_t rad_audio_configure(rad_device_t device, const rad_audio_format_t *format) {
    if (!format) return RAD_STATUS_INVALID_ARGUMENT;
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_AUDIO_CONFIGURE, const_cast<rad_audio_format_t*>(format));
}

rad_status_t rad_serial_configure(rad_device_t device, const rad_serial_config_t *config) {
    if (!config) return RAD_STATUS_INVALID_ARGUMENT;
    return rad_device_ioctl(device, RAD_DEVICE_IOCTL_SERIAL_CONFIGURE, const_cast<rad_serial_config_t*>(config));
}

rad_status_t rad_tty_open(const char *name, rad_tty_t *tty) {
    if (!name || !tty) return RAD_STATUS_INVALID_ARGUMENT;
    size_t index = 0;
    if (!find_tty(name, &index)) return RAD_STATUS_NOT_FOUND;
    auto *handle = static_cast<rad_tty_t>(rad_memory_alloc(sizeof(rad_tty_handle)));
    if (!handle) return RAD_STATUS_NO_MEMORY;
    handle->index = index;
    *tty = handle;
    return RAD_STATUS_OK;
}

void rad_tty_close(rad_tty_t tty) {
    rad_memory_free(tty);
}

rad_status_t rad_tty_read(rad_tty_t tty, void *buffer, size_t size, size_t *bytes_read) {
    if (!tty) return RAD_STATUS_INVALID_ARGUMENT;
    TtyRecord *record = tty->index < RADIX_KERNEL_MAX_TTYS ? &g_state.ttys[tty->index] : nullptr;
    return record && record->used ? tty_read_record(record, buffer, size, bytes_read) : RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_tty_write(rad_tty_t tty, const void *buffer, size_t size, size_t *bytes_written) {
    if (!tty) return RAD_STATUS_INVALID_ARGUMENT;
    TtyRecord *record = tty->index < RADIX_KERNEL_MAX_TTYS ? &g_state.ttys[tty->index] : nullptr;
    return record && record->used ? tty_write_record(record, buffer, size, bytes_written) : RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_tty_push_input(rad_tty_t tty, const void *buffer, size_t size, size_t *bytes_consumed) {
    if (!tty) return RAD_STATUS_INVALID_ARGUMENT;
    TtyRecord *record = tty->index < RADIX_KERNEL_MAX_TTYS ? &g_state.ttys[tty->index] : nullptr;
    return record && record->used ? tty_push_input_record(record, buffer, size, bytes_consumed) : RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_tty_set_output_callback(rad_tty_t tty, rad_tty_output_t output, void *context) {
    if (!tty || tty->index >= RADIX_KERNEL_MAX_TTYS || !g_state.ttys[tty->index].used) return RAD_STATUS_INVALID_ARGUMENT;
    g_state.ttys[tty->index].output = output;
    g_state.ttys[tty->index].output_context = context;
    return RAD_STATUS_OK;
}

rad_status_t rad_tty_set_window_size(rad_tty_t tty, const rad_tty_window_size_t *size) {
    if (!tty || tty->index >= RADIX_KERNEL_MAX_TTYS || !g_state.ttys[tty->index].used || !size || !size->rows || !size->columns) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    g_state.ttys[tty->index].window = *size;
    return RAD_STATUS_OK;
}

rad_status_t rad_tty_get_window_size(rad_tty_t tty, rad_tty_window_size_t *size) {
    if (!tty || tty->index >= RADIX_KERNEL_MAX_TTYS || !g_state.ttys[tty->index].used || !size) return RAD_STATUS_INVALID_ARGUMENT;
    *size = g_state.ttys[tty->index].window;
    return RAD_STATUS_OK;
}

rad_status_t rad_tty_set_mode(rad_tty_t tty, uint32_t mode) {
    if (!tty || tty->index >= RADIX_KERNEL_MAX_TTYS || !g_state.ttys[tty->index].used) return RAD_STATUS_INVALID_ARGUMENT;
    g_state.ttys[tty->index].mode = mode;
    tty_termios_from_mode(g_state.ttys[tty->index].termios, mode);
    g_state.ttys[tty->index].line_size = 0;
    return RAD_STATUS_OK;
}

rad_status_t rad_tty_get_mode(rad_tty_t tty, uint32_t *mode) {
    if (!tty || tty->index >= RADIX_KERNEL_MAX_TTYS || !g_state.ttys[tty->index].used || !mode) return RAD_STATUS_INVALID_ARGUMENT;
    *mode = g_state.ttys[tty->index].mode;
    return RAD_STATUS_OK;
}

rad_status_t rad_tty_set_termios(rad_tty_t tty, const rad_tty_termios_t *termios) {
    if (!tty || tty->index >= RADIX_KERNEL_MAX_TTYS || !g_state.ttys[tty->index].used || !termios) return RAD_STATUS_INVALID_ARGUMENT;
    g_state.ttys[tty->index].termios = *termios;
    if (g_state.ttys[tty->index].termios.input_speed == 0) g_state.ttys[tty->index].termios.input_speed = TtySpeedDefault;
    if (g_state.ttys[tty->index].termios.output_speed == 0) g_state.ttys[tty->index].termios.output_speed = TtySpeedDefault;
    g_state.ttys[tty->index].mode = tty_mode_from_termios(g_state.ttys[tty->index].termios);
    g_state.ttys[tty->index].line_size = 0;
    return RAD_STATUS_OK;
}

rad_status_t rad_tty_get_termios(rad_tty_t tty, rad_tty_termios_t *termios) {
    if (!tty || tty->index >= RADIX_KERNEL_MAX_TTYS || !g_state.ttys[tty->index].used || !termios) return RAD_STATUS_INVALID_ARGUMENT;
    *termios = g_state.ttys[tty->index].termios;
    return RAD_STATUS_OK;
}

rad_status_t rad_tty_flush(rad_tty_t tty, uint32_t queues) {
    if (!tty || tty->index >= RADIX_KERNEL_MAX_TTYS || !g_state.ttys[tty->index].used) return RAD_STATUS_INVALID_ARGUMENT;
    return tty_flush_record(&g_state.ttys[tty->index], queues);
}

rad_status_t rad_pty_open_pair(const char *name, rad_pty_t *pty) {
    if (!pty) return RAD_STATUS_INVALID_ARGUMENT;
    uint32_t next_id = 1;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PTYS; ++i) {
        if (g_state.ptys[i].used && g_state.ptys[i].id >= next_id) next_id = g_state.ptys[i].id + 1;
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_PTYS; ++i) {
        if (g_state.ptys[i].used) continue;
        PtyRecord& record = g_state.ptys[i];
        memset(&record, 0, sizeof(record));
        record.used = 1;
        record.id = next_id;
        copy_string(record.name, sizeof(record.name), name && *name ? name : "pty");
        snprintf(record.slave_name, sizeof(record.slave_name), "/dev/pts/%u", static_cast<unsigned>(next_id));
        record.window.rows = 30;
        record.window.columns = 120;
        record.window.x_pixels = 960;
        record.window.y_pixels = 480;
        size_t tty_index = 0;
        TtyRecord *slave = ensure_tty(record.slave_name, &tty_index);
        if (!slave) {
            memset(&record, 0, sizeof(record));
            return RAD_STATUS_NO_MEMORY;
        }
        register_tty_device(record.slave_name);
        slave->pty_id = record.id;
        slave->window = record.window;
        auto *handle = static_cast<rad_pty_t>(rad_memory_alloc(sizeof(rad_pty_handle)));
        if (!handle) {
            slave->pty_id = 0;
            memset(&record, 0, sizeof(record));
            return RAD_STATUS_NO_MEMORY;
        }
        handle->index = i;
        *pty = handle;
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NO_MEMORY;
}

void rad_pty_close(rad_pty_t pty) {
    if (!pty) return;
    PtyRecord *record = find_pty(pty->index);
    if (record) {
        TtyRecord *slave = find_tty(record->slave_name, nullptr);
        if (slave) slave->pty_id = 0;
        memset(record, 0, sizeof(*record));
    }
    rad_memory_free(pty);
}

rad_status_t rad_pty_read_master(rad_pty_t pty, void *buffer, size_t size, size_t *bytes_read) {
    PtyRecord *record = pty ? find_pty(pty->index) : nullptr;
    if (!record || (!buffer && size)) return RAD_STATUS_INVALID_ARGUMENT;
    size_t count = buffer_take(static_cast<char*>(buffer), size, record->master_buffer, &record->master_size);
    if (bytes_read) *bytes_read = count;
    return RAD_STATUS_OK;
}

rad_status_t rad_pty_write_master(rad_pty_t pty, const void *buffer, size_t size, size_t *bytes_written) {
    PtyRecord *record = pty ? find_pty(pty->index) : nullptr;
    if (!record) return RAD_STATUS_INVALID_ARGUMENT;
    TtyRecord *slave = find_tty(record->slave_name, nullptr);
    return tty_push_input_record(slave, buffer, size, bytes_written);
}

rad_status_t rad_pty_read_slave(rad_pty_t pty, void *buffer, size_t size, size_t *bytes_read) {
    PtyRecord *record = pty ? find_pty(pty->index) : nullptr;
    if (!record) return RAD_STATUS_INVALID_ARGUMENT;
    TtyRecord *slave = find_tty(record->slave_name, nullptr);
    return tty_read_record(slave, buffer, size, bytes_read);
}

rad_status_t rad_pty_write_slave(rad_pty_t pty, const void *buffer, size_t size, size_t *bytes_written) {
    PtyRecord *record = pty ? find_pty(pty->index) : nullptr;
    if (!record) return RAD_STATUS_INVALID_ARGUMENT;
    TtyRecord *slave = find_tty(record->slave_name, nullptr);
    return tty_write_record(slave, buffer, size, bytes_written);
}

rad_status_t rad_pty_resize(rad_pty_t pty, const rad_tty_window_size_t *size) {
    PtyRecord *record = pty ? find_pty(pty->index) : nullptr;
    if (!record || !size || !size->rows || !size->columns) return RAD_STATUS_INVALID_ARGUMENT;
    record->window = *size;
    TtyRecord *slave = find_tty(record->slave_name, nullptr);
    if (slave) slave->window = *size;
    return RAD_STATUS_OK;
}

rad_status_t rad_pty_slave_name(rad_pty_t pty, char *buffer, size_t size) {
    PtyRecord *record = pty ? find_pty(pty->index) : nullptr;
    if (!record || !buffer || size == 0) return RAD_STATUS_INVALID_ARGUMENT;
    copy_string(buffer, size, record->slave_name);
    return RAD_STATUS_OK;
}

rad_status_t rad_terminal_register_command(const char *name, const char *description, rad_terminal_handler_t handler, void *context) {
    if (!name || !handler) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_COMMANDS; ++i) {
        if (g_state.commands[i].used && strcmp(g_state.commands[i].name, name) == 0) return RAD_STATUS_ALREADY_EXISTS;
    }
    for (size_t i = 0; i < RADIX_KERNEL_MAX_COMMANDS; ++i) {
        if (g_state.commands[i].used) continue;
        g_state.commands[i].used = 1;
        copy_string(g_state.commands[i].name, sizeof(g_state.commands[i].name), name);
        copy_string(g_state.commands[i].description, sizeof(g_state.commands[i].description), description);
        g_state.commands[i].handler = handler;
        g_state.commands[i].context = context;
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t rad_terminal_execute(const char *line, rad_terminal_write_t write, void *write_context) {
    if (!line) return RAD_STATUS_INVALID_ARGUMENT;
    const char *argv[16]{};
    char local[192];
    copy_string(local, sizeof(local), line);
    int argc = 0;
    char *cursor = local;
    while (*cursor && argc < 16) {
        while (*cursor == ' ' || *cursor == '\t') ++cursor;
        if (!*cursor) break;
        argv[argc++] = cursor;
        while (*cursor && *cursor != ' ' && *cursor != '\t') ++cursor;
        if (*cursor) {
            *cursor = '\0';
            ++cursor;
        }
    }
    if (argc == 0) return RAD_STATUS_OK;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_COMMANDS; ++i) {
        if (!g_state.commands[i].used || strcmp(g_state.commands[i].name, argv[0]) != 0) continue;
        return g_state.commands[i].handler(argc, argv, write, write_context, g_state.commands[i].context);
    }
    return RAD_STATUS_NOT_FOUND;
}

rad_status_t rad_terminal_poll_tty(rad_tty_t tty) {
    if (!tty) return RAD_STATUS_INVALID_ARGUMENT;
    char line[256];
    size_t line_size = 0;
    char ch = 0;
    size_t done = 0;
    while (rad_tty_read(tty, &ch, 1, &done) == RAD_STATUS_OK && done == 1) {
        if (ch == '\r') continue;
        if (ch != '\n') {
            if (line_size + 1 < sizeof(line)) line[line_size++] = ch;
            continue;
        }
        line[line_size] = '\0';
        auto writer = [](const char *text, void *context) {
            size_t ignored = 0;
            rad_tty_write(static_cast<rad_tty_t>(context), text, strlen(text), &ignored);
        };
        rad_status_t command_status = rad_terminal_execute(line, writer, tty);
        size_t ignored = 0;
        if (command_status == RAD_STATUS_NOT_FOUND && line_size) {
            rad_tty_write(tty, "command not found: ", 19, &ignored);
            rad_tty_write(tty, line, line_size, &ignored);
            rad_tty_write(tty, "\n", 1, &ignored);
        } else {
            rad_tty_write(tty, "\n", 1, &ignored);
        }
        line_size = 0;
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_terminal_attach_device(const char *device_name) {
    if (!device_name) return RAD_STATUS_INVALID_ARGUMENT;
    rad_device_t device = nullptr;
    rad_status_t status = rad_device_open(device_name, &device);
    if (status != RAD_STATUS_OK) return status;
    const bool is_serial = device->record.type == RAD_DEVICE_SERIAL;
    rad_device_close(device);
    if (!is_serial) return RAD_STATUS_INVALID_ARGUMENT;
    copy_string(g_state.attached_terminal_device, sizeof(g_state.attached_terminal_device), device_name);
    g_state.terminal_line_size = 0;
    g_state.terminal_line[0] = '\0';
    TtyRecord *tty = ensure_tty(device_name, nullptr);
    if (tty) {
        copy_string(tty->attached_device_name, sizeof(tty->attached_device_name), device_name);
        tty->output = serial_tty_output;
        tty->output_context = tty->attached_device_name;
    }
    return RAD_STATUS_OK;
}

rad_status_t rad_terminal_poll_attached(void) {
    if (!g_state.attached_terminal_device[0]) return RAD_STATUS_OK;
    rad_device_t device = nullptr;
    if (rad_device_open(g_state.attached_terminal_device, &device) != RAD_STATUS_OK) return RAD_STATUS_NOT_FOUND;
    rad_tty_t tty = nullptr;
    if (rad_tty_open(g_state.attached_terminal_device, &tty) != RAD_STATUS_OK) {
        rad_device_close(device);
        return RAD_STATUS_NOT_FOUND;
    }
    char ch = 0;
    size_t done = 0;
    while (rad_device_read(device, &ch, 1, &done) == RAD_STATUS_OK && done == 1) {
        size_t consumed = 0;
        rad_tty_push_input(tty, &ch, 1, &consumed);
        rad_terminal_poll_tty(tty);
    }
    rad_tty_close(tty);
    rad_device_close(device);
    return RAD_STATUS_OK;
}

size_t rad_terminal_command_count(void) {
    size_t count = 0;
    for (size_t i = 0; i < RADIX_KERNEL_MAX_COMMANDS; ++i) {
        if (g_state.commands[i].used) ++count;
    }
    return count;
}

}
