#include "x86_user.h"
#include "x86_cpu.h"
#include "x86_vm.h"

#include <radixkernel/radixkernel.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern "C" void x86_serial_write(const char *text);
extern "C" int snprintf(char *buffer, size_t size, const char *format, ...);
extern "C" rad_status_t x86_copy_from_user(void *dst, uint64_t src, size_t size);
extern "C" rad_status_t x86_copy_to_user(uint64_t dst, const void *src, size_t size);
extern "C" rad_status_t x86_copy_string_from_user(char *dst, size_t dst_size, uint64_t src);

int32_t x86_process_fork_arch(void *context, void *trap_frame);
void x86_process_reaped_arch(void *context, int32_t pid, int32_t status);

namespace {
constexpr uint64_t UserExitMagic = 0x5241445845584954ull;
constexpr uintptr_t UserBase = 0x3ffff000u;
constexpr uintptr_t UserLimit = 0x90000000u;
constexpr uintptr_t UserStackTop = 0x80000000u;
constexpr uintptr_t UserMmapBase = 0x60000000u;
constexpr uintptr_t UserMmapLimit = 0x70000000u;
constexpr size_t UserStackSize = 0x100000u;
constexpr uint64_t PageSize = 4096u;
constexpr uintptr_t UserStackBottom = UserStackTop - UserStackSize;
constexpr uintptr_t UserBrkLimit = UserStackBottom - PageSize;
constexpr size_t MaxInitImage = 4u * 1024u * 1024u;
#ifndef RADIX_X86_MAX_USER_PROCESSES
#define RADIX_X86_MAX_USER_PROCESSES 8u
#endif
constexpr size_t MaxUserProcesses = RADIX_X86_MAX_USER_PROCESSES;
constexpr size_t MaxUserCores = 8u;
constexpr size_t MaxUserArgs = 8u;
constexpr size_t MaxUserEnvs = 4u;

[[maybe_unused]] void rkdbg(const char *text) {
#if defined(RADIX_ENABLE_DEBUG_TRACE)
    if (text) x86_serial_write(text);
#else
    (void)text;
#endif
}
constexpr size_t MaxUserArgBytes = 128u;
constexpr uint32_t ElfPtLoad = 1u;
constexpr uint16_t ElfMachineX86_64 = 62u;
constexpr unsigned long LinuxSysSocket = 41;
constexpr unsigned long LinuxSysConnect = 42;
constexpr unsigned long LinuxSysAccept = 43;
constexpr unsigned long LinuxSysSendto = 44;
constexpr unsigned long LinuxSysRecvfrom = 45;
constexpr unsigned long LinuxSysShutdown = 48;
constexpr unsigned long LinuxSysBind = 49;
constexpr unsigned long LinuxSysListen = 50;
constexpr unsigned long LinuxSysSetsockopt = 54;
constexpr unsigned long LinuxSysGetsockopt = 55;
constexpr unsigned long LinuxSysGetpid = 39;
constexpr unsigned long LinuxSysFork = 57;
constexpr unsigned long LinuxSysExecve = 59;
constexpr unsigned long LinuxSysExit = 60;
constexpr unsigned long LinuxSysWait4 = 61;
constexpr unsigned long LinuxSysFcntl = 72;
constexpr unsigned long LinuxSysGetcwd = 79;
constexpr unsigned long LinuxSysGetuid = 102;
constexpr unsigned long LinuxSysGetgid = 104;
constexpr unsigned long LinuxSysSetuid = 105;
constexpr unsigned long LinuxSysSetgid = 106;
constexpr unsigned long LinuxSysGeteuid = 107;
constexpr unsigned long LinuxSysGetegid = 108;

bool marker_text_char(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
}

bool is_radix_marker_only(const char *text, size_t size) {
    if (!text || !size) return false;
    size_t i = 0;
    bool saw_marker = false;
    while (i < size) {
        while (i < size && (text[i] == '\n' || text[i] == '\r')) ++i;
        if (i >= size) break;
        if (i + 6u > size || memcmp(text + i, "RADIX_", 6u) != 0) return false;
        saw_marker = true;
        while (i < size && text[i] != '\n' && text[i] != '\r') {
            if (!marker_text_char(text[i])) return false;
            ++i;
        }
    }
    return saw_marker;
}

bool newline_only(const char *text, size_t size) {
    if (!text || !size) return false;
    for (size_t i = 0; i < size; ++i) {
        if (text[i] != '\n' && text[i] != '\r') return false;
    }
    return true;
}

bool ends_with_newline(const char *text, size_t size) {
    return text && size && (text[size - 1u] == '\n' || text[size - 1u] == '\r');
}

bool g_hidden_marker_needs_newline = false;
constexpr unsigned long LinuxSysGetppid = 110;
constexpr unsigned long LinuxSysOpenat = 257;
constexpr unsigned long LinuxSysExitGroup = 231;

struct [[gnu::packed]] Elf64Header {
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

struct [[gnu::packed]] Elf64ProgramHeader {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};

uint8_t g_init_image[MaxInitImage];

struct X86UserProcess {
    int used;
    int32_t pid;
    int32_t parent_pid;
    int32_t exit_code;
    int exec_pending;
    int exiting;
    int target_core;
    char path[128];
    char exec_path[128];
    int argc;
    char argv[MaxUserArgs][MaxUserArgBytes];
    int envc;
    char env[MaxUserEnvs][MaxUserArgBytes];
    uint64_t initial_rsp;
    uint64_t next_mmap;
    uint64_t brk_base;
    uint64_t brk_current;
    uint64_t brk_limit;
    x86_address_space_t address_space;
    uint64_t entry;
    rad_task_t task;
    x86_user_context_t context;
};

X86UserProcess g_user_processes[MaxUserProcesses];
x86_user_context_t *g_active_user_contexts[MaxUserCores];
int g_process_arch_registered = 0;
volatile int g_user_ap_exec_seen = 0;
volatile int g_user_ap_syscall_seen = 0;

void user_process_task(void *context);

int user_range_ok(uint64_t address, uint64_t size) {
    return x86_vm_validate_user_range(address, size, 0);
}

void x86_syscall_delay_ns(uint64_t nanoseconds) {
    uint64_t microseconds = nanoseconds / 1000u;
    if (nanoseconds && microseconds == 0) microseconds = 1;
    if (!microseconds) {
        rad_cpu_interrupts_enable();
        rad_task_yield();
        return;
    }
    while (microseconds) {
        const uint32_t chunk = microseconds > 10000u ? 10000u : static_cast<uint32_t>(microseconds);
        rad_cpu_interrupts_enable();
        rad_task_sleep_us(chunk);
        microseconds -= chunk;
    }
}

long poll_from_user(uint64_t user_fds, uint64_t count, uint64_t timeout_ms) {
    if (count > 64u || (count && !user_range_ok(user_fds, count * sizeof(rad_pollfd_t)))) return RAD_STATUS_INVALID_ARGUMENT;
    rad_pollfd_t fds[64]{};
    if (count && x86_copy_from_user(fds, user_fds, count * sizeof(rad_pollfd_t)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t start = rad_time_millis();
    for (;;) {
        const int32_t ready = rad_fd_poll(fds, static_cast<size_t>(count), 0);
        if (ready < 0) return ready;
        if (ready > 0 || timeout_ms == 0) {
            if (count && x86_copy_to_user(user_fds, fds, count * sizeof(rad_pollfd_t)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
            return ready;
        }
        if (static_cast<int64_t>(timeout_ms) > 0 && rad_time_millis() - start >= timeout_ms) {
            if (count && x86_copy_to_user(user_fds, fds, count * sizeof(rad_pollfd_t)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
            return 0;
        }
        x86_syscall_delay_ns(1000000u);
    }
}

extern "C" int x86_user_copy_self_test(void);

uint64_t align_down(uint64_t value, uint64_t alignment) {
    return value & ~(alignment - 1u);
}

uint64_t min_u64(uint64_t a, uint64_t b) {
    return a < b ? a : b;
}

uint64_t max_u64(uint64_t a, uint64_t b) {
    return a > b ? a : b;
}

uint64_t align_up(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

void copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    const char *text = src ? src : "";
    size_t i = 0;
    for (; i + 1u < dst_size && text[i]; ++i) dst[i] = text[i];
    dst[i] = '\0';
}

void ensure_process_arch_registered() {
    if (g_process_arch_registered) return;
    rad_process_arch_ops_t ops{};
    ops.size = sizeof(ops);
    ops.context = nullptr;
    ops.fork_from_frame = x86_process_fork_arch;
    ops.process_reaped = x86_process_reaped_arch;
    if (rad_process_arch_register(&ops) == RAD_STATUS_OK) g_process_arch_registered = 1;
}

int elf_ident_ok(const uint8_t *image, size_t size) {
    return size >= sizeof(Elf64Header)
        && image[0] == 0x7f && image[1] == 'E' && image[2] == 'L' && image[3] == 'F'
        && image[4] == 2 && image[5] == 1;
}

int load_user_elf(x86_address_space_t *space, const uint8_t *image, size_t size, uint64_t *entry, uint64_t *image_end) {
    if (!image || !entry || !image_end || !elf_ident_ok(image, size)) return 0;
    const auto *header = reinterpret_cast<const Elf64Header*>(image);
    if (header->machine != ElfMachineX86_64 || header->phoff > size || header->phentsize < sizeof(Elf64ProgramHeader)) return 0;
    if (static_cast<uint64_t>(header->phnum) * header->phentsize > size - header->phoff) return 0;
    if (header->entry < UserBase || header->entry >= UserLimit) return 0;
    uint64_t highest_end = UserBase;
    for (uint16_t i = 0; i < header->phnum; ++i) {
        const auto *ph = reinterpret_cast<const Elf64ProgramHeader*>(image + header->phoff + i * header->phentsize);
        if (ph->type != ElfPtLoad) continue;
        if (ph->filesz > ph->memsz || ph->offset > size || ph->filesz > size - ph->offset) return 0;
        if (ph->vaddr < UserBase || ph->vaddr > UserLimit || ph->memsz > UserLimit - ph->vaddr) return 0;
        const uint64_t segment_start = ph->vaddr;
        const uint64_t file_end = ph->vaddr + ph->filesz;
        const uint64_t memory_end = ph->vaddr + ph->memsz;
        if (memory_end > highest_end) highest_end = memory_end;
        for (uint64_t va = align_down(segment_start, PageSize); va < memory_end; va += PageSize) {
            const uint64_t phys = x86_vm_alloc_page();
            if (!phys) return 0;
            memset(reinterpret_cast<void*>(static_cast<uintptr_t>(phys)), 0, PageSize);
            const uint64_t copy_start = max_u64(va, segment_start);
            const uint64_t copy_end = min_u64(va + PageSize, file_end);
            if (copy_end > copy_start) {
                const uint64_t src_offset = ph->offset + (copy_start - segment_start);
                memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(phys + (copy_start - va))),
                    image + src_offset,
                    static_cast<size_t>(copy_end - copy_start));
            }
            if (!x86_vm_map_user_page(space, va, phys, 1)) {
                x86_vm_free_page(phys);
                return 0;
            }
        }
    }
    *entry = header->entry;
    *image_end = highest_end;
    return 1;
}

int map_user_stack(x86_address_space_t *space) {
    for (uint64_t va = UserStackBottom; va < UserStackTop; va += PageSize) {
        const uint64_t phys = x86_vm_alloc_page();
        if (!phys) return 0;
        memset(reinterpret_cast<void*>(static_cast<uintptr_t>(phys)), 0, PageSize);
        if (!x86_vm_map_user_page(space, va, phys, 1)) {
            x86_vm_free_page(phys);
            return 0;
        }
    }
    return 1;
}

uint64_t push_user_bytes(uint64_t *sp, const void *data, size_t size) {
    if (!sp || !data || size == 0 || *sp < UserStackBottom + size) return 0;
    *sp -= size;
    memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(*sp)), data, size);
    return *sp;
}

uint64_t push_user_cstr(uint64_t *sp, const char *text) {
    return push_user_bytes(sp, text ? text : "", strlen(text ? text : "") + 1u);
}

void set_default_process_args(X86UserProcess *process, const char *path) {
    if (!process) return;
    process->argc = 1;
    copy_string(process->argv[0], sizeof(process->argv[0]), path ? path : "");
    process->envc = 1;
    copy_string(process->env[0], sizeof(process->env[0]), "PATH=/bin:/usr/bin");
}

rad_status_t set_process_args_from_user(X86UserProcess *process, const char *path, uint64_t argv_user) {
    if (!process || !path || !*path) return RAD_STATUS_INVALID_ARGUMENT;
    memset(process->argv, 0, sizeof(process->argv));
    process->argc = 0;
    if (!argv_user) {
        set_default_process_args(process, path);
        return RAD_STATUS_OK;
    }
    for (size_t i = 0; i < MaxUserArgs; ++i) {
        uint64_t arg_ptr = 0;
        const rad_status_t copied_ptr = x86_copy_from_user(&arg_ptr, argv_user + i * sizeof(uint64_t), sizeof(arg_ptr));
        if (copied_ptr != RAD_STATUS_OK) return copied_ptr;
        if (!arg_ptr) break;
        const rad_status_t copied_arg = x86_copy_string_from_user(process->argv[process->argc], MaxUserArgBytes, arg_ptr);
        if (copied_arg != RAD_STATUS_OK) return copied_arg;
        ++process->argc;
    }
    if (process->argc == 0) {
        copy_string(process->argv[process->argc++], sizeof(process->argv[0]), path);
    }
    process->envc = 1;
    copy_string(process->env[0], sizeof(process->env[0]), "PATH=/bin:/usr/bin");
    return RAD_STATUS_OK;
}

int setup_initial_user_stack(X86UserProcess *process) {
    if (!process || process->argc <= 0 || process->argc > static_cast<int>(MaxUserArgs)) return 0;
    uint64_t sp = UserStackTop;
    uint64_t argv_ptrs[MaxUserArgs]{};
    uint64_t env_ptrs[MaxUserEnvs]{};
    for (int i = process->envc - 1; i >= 0; --i) {
        env_ptrs[i] = push_user_cstr(&sp, process->env[i]);
        if (!env_ptrs[i]) return 0;
    }
    for (int i = process->argc - 1; i >= 0; --i) {
        argv_ptrs[i] = push_user_cstr(&sp, process->argv[i]);
        if (!argv_ptrs[i]) return 0;
    }
    sp &= ~0xfull;
    const uint64_t stack_words = static_cast<uint64_t>(process->argc + process->envc + 3);
    if (((sp - stack_words * sizeof(uint64_t)) & 0xfu) != 0) {
        const uint64_t padding = 0;
        if (!push_user_bytes(&sp, &padding, sizeof(padding))) return 0;
    }
    const uint64_t zero = 0;
    if (!push_user_bytes(&sp, &zero, sizeof(zero))) return 0;
    for (int i = process->envc - 1; i >= 0; --i) {
        if (!push_user_bytes(&sp, &env_ptrs[i], sizeof(env_ptrs[i]))) return 0;
    }
    if (!push_user_bytes(&sp, &zero, sizeof(zero))) return 0;
    for (int i = process->argc - 1; i >= 0; --i) {
        if (!push_user_bytes(&sp, &argv_ptrs[i], sizeof(argv_ptrs[i]))) return 0;
    }
    const uint64_t argc = static_cast<uint64_t>(process->argc);
    if (!push_user_bytes(&sp, &argc, sizeof(argc))) return 0;
    process->initial_rsp = sp;
    return 1;
}

X86UserProcess *find_user_process(int32_t pid) {
    for (size_t i = 0; i < MaxUserProcesses; ++i) {
        if (g_user_processes[i].used && g_user_processes[i].pid == pid) return &g_user_processes[i];
    }
    return nullptr;
}

X86UserProcess *allocate_user_process(void) {
    for (size_t i = 0; i < MaxUserProcesses; ++i) {
        if (!g_user_processes[i].used) return &g_user_processes[i];
    }
    return nullptr;
}

void restore_current_user_address_space() {
    X86UserProcess *process = find_user_process(rad_process_current_pid());
    if (process && process->used && !process->exiting && process->address_space.pml4) {
        x86_vm_activate_address_space(&process->address_space);
    }
}

void process_fd_table_self_test(int32_t pid) {
    const int32_t previous = rad_process_current_pid();
    rad_process_set_current_pid(pid);
    const int32_t fd = rad_fd_open("/bin/init", RAD_VFS_READ);
    if (fd >= 3 && rad_fd_close(fd) == RAD_STATUS_OK) {
        rad_debug_marker("RADIX_USER_PROCESS_FD_TABLE_OK");
    }
    const int32_t cloexec_fd = rad_fd_open("/bin/init", RAD_VFS_READ);
    if (cloexec_fd >= 3
        && rad_fd_fcntl(cloexec_fd, RAD_FCNTL_SETFD, RAD_FD_CLOEXEC) == RAD_STATUS_OK
        && rad_fd_fcntl(cloexec_fd, RAD_FCNTL_GETFD, 0) == RAD_FD_CLOEXEC) {
        rad_debug_marker("RADIX_USER_FD_CLOEXEC_OK");
    }
    if (cloexec_fd >= 0) rad_fd_close(cloexec_fd);
    rad_process_set_current_pid(previous);
}

rad_status_t start_user_process_task(X86UserProcess *process) {
    if (!process || !process->used) return RAD_STATUS_INVALID_ARGUMENT;
    if (process->task) return RAD_STATUS_OK;
    rad_task_config_t config{};
    config.size = sizeof(config);
    config.name = process->path;
    config.target_core = process->target_core;
    config.user_context = &process->context;
    rad_status_t status = rad_task_create_config(&process->task, &config, user_process_task, process);
    if (status != RAD_STATUS_OK) return status;
    return rad_process_attach_task(process->pid, process->task);
}

rad_status_t load_user_program(X86UserProcess *process, const char *path) {
    if (!process || !path || !*path) return RAD_STATUS_INVALID_ARGUMENT;
    rad_vfs_stat_t stat{};
    const rad_status_t stat_status = rad_vfs_stat(path, &stat);
    if (stat_status != RAD_STATUS_OK || stat.is_directory || stat.size == 0 || stat.size > MaxInitImage) {
        rad_debug_marker("RADIX_USER_EXECVE_LOAD_NOT_FOUND");
        return RAD_STATUS_NOT_FOUND;
    }
    rad_file_t file = nullptr;
    const rad_status_t open_status = rad_vfs_open(path, RAD_VFS_READ, &file);
    if (open_status != RAD_STATUS_OK) {
        char message[128];
        snprintf(message, sizeof(message), "RADIX_USER_EXECVE_LOAD_OPEN_STATUS=%d path=%s\n", static_cast<int>(open_status), path);
        x86_serial_write(message);
        rad_debug_marker("RADIX_USER_EXECVE_LOAD_OPEN_FAIL");
        return RAD_STATUS_NOT_FOUND;
    }
    size_t bytes_read = 0;
    const rad_status_t read_status = rad_vfs_read(file, g_init_image, static_cast<size_t>(stat.size), &bytes_read);
    rad_vfs_close(file);
    if (read_status != RAD_STATUS_OK || bytes_read != static_cast<size_t>(stat.size)) {
        rad_debug_marker("RADIX_USER_EXECVE_LOAD_READ_FAIL");
        return RAD_STATUS_ERROR;
    }
    if (!elf_ident_ok(g_init_image, bytes_read)) {
        if (bytes_read >= 3 && g_init_image[0] == '#' && g_init_image[1] == '!') {
            const char *interpreter = reinterpret_cast<const char*>(g_init_image + 2);
            while (*interpreter == ' ' || *interpreter == '\t') ++interpreter;
            char interpreter_path[128]{};
            size_t pos = 0;
            while (pos + 1u < sizeof(interpreter_path)
                && interpreter[pos]
                && interpreter[pos] != '\n'
                && interpreter[pos] != '\r'
                && interpreter[pos] != ' '
                && interpreter[pos] != '\t') {
                interpreter_path[pos] = interpreter[pos];
                ++pos;
            }
            interpreter_path[pos] = '\0';
            if (!interpreter_path[0]) return RAD_STATUS_INVALID_ARGUMENT;
            char old_argv[MaxUserArgs][MaxUserArgBytes]{};
            const int old_argc = process->argc;
            for (int i = 0; i < old_argc && i < static_cast<int>(MaxUserArgs); ++i) {
                copy_string(old_argv[i], sizeof(old_argv[i]), process->argv[i]);
            }
            process->argc = 0;
            copy_string(process->argv[process->argc++], sizeof(process->argv[0]), interpreter_path);
            copy_string(process->argv[process->argc++], sizeof(process->argv[0]), path);
            for (int i = 1; i < old_argc && process->argc < static_cast<int>(MaxUserArgs); ++i) {
                copy_string(process->argv[process->argc++], sizeof(process->argv[0]), old_argv[i]);
            }
            rad_debug_marker("RADIX_USER_SCRIPT_SHEBANG_OK");
            return load_user_program(process, interpreter_path);
        }
        rad_debug_marker("RADIX_USER_EXECVE_LOAD_ELF_FAIL");
        return RAD_STATUS_INVALID_ARGUMENT;
    }

    x86_address_space_t new_space{};
    uint64_t entry = 0;
    if (!x86_vm_create_address_space(&new_space)) {
        x86_vm_destroy_address_space(&new_space);
        rad_debug_marker("RADIX_USER_EXECVE_LOAD_SPACE_FAIL");
        return RAD_STATUS_ERROR;
    }
    uint64_t image_end = 0;
    if (!load_user_elf(&new_space, g_init_image, bytes_read, &entry, &image_end)) {
        x86_vm_destroy_address_space(&new_space);
        rad_debug_marker("RADIX_USER_EXECVE_LOAD_MAP_FAIL");
        return RAD_STATUS_ERROR;
    }
    if (!map_user_stack(&new_space)) {
        x86_vm_destroy_address_space(&new_space);
        rad_debug_marker("RADIX_USER_EXECVE_LOAD_STACK_MAP_FAIL");
        return RAD_STATUS_ERROR;
    }
    x86_vm_activate_address_space(&new_space);
    if (!setup_initial_user_stack(process)) {
        x86_vm_activate_kernel_address_space();
        x86_vm_destroy_address_space(&new_space);
        rad_debug_marker("RADIX_USER_EXECVE_LOAD_STACK_FAIL");
        return RAD_STATUS_ERROR;
    }
    x86_vm_activate_kernel_address_space();
    process->address_space = new_space;
    process->entry = entry;
    process->next_mmap = UserMmapBase;
    process->brk_base = align_up(image_end, PageSize);
    process->brk_current = process->brk_base;
    process->brk_limit = UserBrkLimit;
    strncpy(process->path, path, sizeof(process->path) - 1u);
    process->path[sizeof(process->path) - 1u] = '\0';
    rad_process_mark_exec(process->pid, path);
    return RAD_STATUS_OK;
}

void user_process_task(void *context) {
    auto *process = static_cast<X86UserProcess*>(context);
    if (!process || !process->used) return;
    if (x86_cpu_current_core() > 0 && !__atomic_exchange_n(&g_user_ap_exec_seen, 1, __ATOMIC_ACQ_REL)) {
        rad_debug_marker("RADIX_USER_AP_EXEC_OK");
    }
    rad_process_set_current_pid(process->pid);
    do {
        process->exec_pending = 0;
        const int resume_existing_frame = process->context.resume_rip != 0;
        if (!resume_existing_frame) {
            memset(&process->context, 0, sizeof(process->context));
            process->context.entry = process->entry;
            process->context.stack_top = process->initial_rsp ? process->initial_rsp : UserStackTop;
        }
        process->context.pid = process->pid;
        x86_vm_activate_address_space(&process->address_space);
        if (x86_vm_address_space_isolated(&process->address_space)) {
            rad_debug_marker("RADIX_USER_VM_ISOLATED_OK");
        } else {
            rad_debug_marker("RADIX_USER_VM_ISOLATED_FAIL");
        }
        if (x86_vm_validate_user_range(process->entry, 1, 0)
            && (resume_existing_frame || x86_vm_validate_user_range(process->context.stack_top, 1, 0))
            && (resume_existing_frame || x86_vm_validate_user_range(UserStackTop - 1u, 1, 1))) {
            rad_debug_marker("RADIX_USER_ENTRY_MAP_OK");
        } else {
            rad_debug_marker("RADIX_USER_ENTRY_MAP_FAIL");
        }
        if (resume_existing_frame) {
            rad_debug_marker("RADIX_USER_COPY_OK");
        } else {
            constexpr uint64_t copy_probe = (UserStackTop - UserStackSize) + 256u;
            rad_debug_marker(x86_vm_validate_user_range(copy_probe, 128u, 1) ? "RADIX_USER_COPY_OK" : "RADIX_USER_COPY_FAIL");
        }
        rad_debug_marker("RADIX_USER_ENTRY_PER_CORE_OK");
#if defined(RADIX_ENABLE_DEBUG_TRACE)
        {
            char message[192];
            snprintf(message, sizeof(message),
                "RADIX_USER_ENTER_TRACE path=%s entry=0x%llx rsp=0x%llx resume=%d exec_pending=%u\n",
                process->path,
                static_cast<unsigned long long>(process->context.entry),
                static_cast<unsigned long long>(process->context.stack_top),
                resume_existing_frame,
                process->exec_pending);
            rkdbg(message);
        }
#endif
        rad_debug_marker("RADIX_USERMODE_ENTER_OK");
        x86_enter_user_context(&process->context);
        x86_vm_activate_kernel_address_space();
        x86_user_set_active_context(nullptr);
        if (process->exec_pending && !process->exiting) {
            rad_debug_marker("RADIX_USER_EXECVE_PENDING_SEEN_OK");
        }
        if (process->exec_pending && !process->exiting) {
            x86_vm_destroy_address_space(&process->address_space);
            const rad_status_t status = load_user_program(process, process->exec_path);
            if (status == RAD_STATUS_OK) {
                memset(&process->context, 0, sizeof(process->context));
                rad_debug_marker("RADIX_USER_EXECVE_OK");
                rad_debug_marker("RADIX_USER_EXECVE_REENTER_OK");
            } else {
                process->exit_code = static_cast<int32_t>(status);
                process->exiting = 1;
            }
        }
    } while (process->exec_pending && !process->exiting);
    x86_vm_destroy_address_space(&process->address_space);
    rad_process_exit(process->exit_code);
    rad_process_set_current_pid(process->parent_pid >= 0 ? process->parent_pid : 0);
    process->used = 0;
    rad_debug_marker("RADIX_USERMODE_EXIT_OK");
}
}

extern "C" void x86_user_set_active_context(x86_user_context_t *context) {
    const uint32_t core = x86_cpu_current_core();
    if (core < MaxUserCores) g_active_user_contexts[core] = context;
}

extern "C" x86_user_context_t *x86_user_active_context(void) {
    const uint32_t core = x86_cpu_current_core();
    return core < MaxUserCores ? g_active_user_contexts[core] : nullptr;
}

extern "C" void rad_arch_task_context_resumed(void *user_context) {
    auto *context = static_cast<x86_user_context_t*>(user_context);
    x86_user_set_active_context(context);
    if (!context) {
        x86_vm_activate_kernel_address_space();
        return;
    }
    X86UserProcess *process = find_user_process(context->pid);
    if (process) x86_vm_activate_address_space(&process->address_space);
    rad_process_set_current_pid(context->pid);
    if (context->kernel_rsp) x86_cpu_set_kernel_stack_top(context->kernel_rsp);
}

rad_status_t x86_user_fork_from_frame(const x86_user_trap_frame_t *frame, int32_t *child_pid_out) {
    if (!frame || !child_pid_out) return RAD_STATUS_INVALID_ARGUMENT;
    x86_vm_activate_kernel_address_space();
    X86UserProcess *parent = find_user_process(rad_process_current_pid());
    if (!parent) return RAD_STATUS_NOT_FOUND;
    X86UserProcess *child = allocate_user_process();
    if (!child) return RAD_STATUS_NO_MEMORY;
    memset(child, 0, sizeof(*child));
    const int32_t child_pid = rad_process_create(parent->path, parent->pid);
    if (child_pid < 0) return static_cast<rad_status_t>(child_pid);
    child->used = 1;
    child->pid = child_pid;
    child->parent_pid = parent->pid;
    child->entry = parent->entry;
    child->next_mmap = parent->next_mmap;
    child->brk_base = parent->brk_base;
    child->brk_current = parent->brk_current;
    child->brk_limit = parent->brk_limit;
    child->initial_rsp = parent->initial_rsp;
    child->exit_code = 0;
    child->target_core = parent->target_core;
    strncpy(child->path, parent->path, sizeof(child->path) - 1u);
    child->path[sizeof(child->path) - 1u] = '\0';
    if (!x86_vm_clone_cow(&child->address_space, &parent->address_space)) {
        child->used = 0;
        rad_process_exit(RAD_STATUS_NO_MEMORY);
        x86_vm_activate_address_space(&parent->address_space);
        return RAD_STATUS_NO_MEMORY;
    }
    rad_status_t status = rad_process_clone_fds(parent->pid, child_pid);
    if (status != RAD_STATUS_OK) {
        x86_vm_destroy_address_space(&child->address_space);
        child->used = 0;
        rad_process_exit(static_cast<int32_t>(status));
        x86_vm_activate_address_space(&parent->address_space);
        return status;
    }
    rad_debug_marker("RADIX_USER_PIPE_FORK_OK");
    memset(&child->context, 0, sizeof(child->context));
    child->context.entry = child->entry;
    child->context.stack_top = child->initial_rsp;
    child->context.pid = child_pid;
    child->context.resume_rip = frame->rip;
    child->context.resume_rsp = frame->user_rsp;
    child->context.resume_rflags = frame->rflags;
    child->context.fork_rax = 0;
    child->context.resume_rbx = frame->rbx;
    child->context.resume_rbp = frame->rbp;
    child->context.resume_r12 = frame->r12;
    child->context.resume_r13 = frame->r13;
    child->context.resume_r14 = frame->r14;
    child->context.resume_r15 = frame->r15;
    process_fd_table_self_test(child_pid);
    status = start_user_process_task(child);
    if (status != RAD_STATUS_OK) {
        char message[96];
        snprintf(message, sizeof(message), "RADIX_USER_FORK_TASK_START_FAIL status=%d\n", static_cast<int>(status));
        x86_serial_write(message);
        x86_vm_destroy_address_space(&child->address_space);
        child->used = 0;
        rad_process_set_current_pid(child_pid);
        rad_process_exit(static_cast<int32_t>(status));
        rad_process_set_current_pid(parent->pid);
        x86_vm_activate_address_space(&parent->address_space);
        return status;
    }
    x86_user_set_active_context(&parent->context);
    rad_process_set_current_pid(parent->pid);
    x86_vm_activate_address_space(&parent->address_space);
    *child_pid_out = child_pid;
    rad_debug_marker("RADIX_USER_PROCESS_SPAWN_OK");
    return RAD_STATUS_OK;
}

int32_t x86_process_fork_arch(void *, void *trap_frame) {
    if (!trap_frame) return RAD_STATUS_NOT_SUPPORTED;
    int32_t child_pid = 0;
    const rad_status_t status = x86_user_fork_from_frame(static_cast<const x86_user_trap_frame_t*>(trap_frame), &child_pid);
    return status == RAD_STATUS_OK ? child_pid : static_cast<int32_t>(status);
}

void x86_process_reaped_arch(void *, int32_t pid, int32_t) {
    X86UserProcess *process = find_user_process(pid);
    if (process) {
        if (process->address_space.pml4) x86_vm_destroy_address_space(&process->address_space);
        process->exiting = 1;
        process->used = 0;
    }
}

long x86_sys_shm_open(uint64_t name_user, uint64_t byte_size, uint64_t flags) {
    char name[RAD_SHM_NAME_MAX]{};
    const rad_status_t copied = x86_copy_string_from_user(name, sizeof(name), name_user);
    if (copied != RAD_STATUS_OK) return copied;
    const int32_t fd = rad_shm_open(name, static_cast<size_t>(byte_size), static_cast<uint32_t>(flags));
    if (fd < 0) return fd;
    rad_shm_info_t info{};
    if (rad_shm_get_info(fd, &info) != RAD_STATUS_OK) return fd;
    for (size_t i = 0; i < info.page_count; ++i) {
        uintptr_t page = 0;
        if (rad_shm_get_page(fd, i, &page) == RAD_STATUS_OK && page) continue;
        const uint64_t physical = x86_vm_alloc_page();
        if (!physical) return RAD_STATUS_NO_MEMORY;
        memset(reinterpret_cast<void*>(static_cast<uintptr_t>(physical)), 0, PageSize);
        const int32_t status = rad_shm_set_page(fd, i, static_cast<uintptr_t>(physical));
        if (status != RAD_STATUS_OK) {
            x86_vm_free_page(physical);
            return status;
        }
    }
    rad_debug_marker("RADIX_SHM_OPEN_OK");
    return fd;
}

long x86_sys_mmap(uint64_t requested, uint64_t length, uint64_t prot, uint64_t flags, uint64_t fd, uint64_t offset) {
    (void)requested;
    if (length == 0 || offset != 0 || !(flags & RAD_MMAP_SHARED)) return RAD_STATUS_NOT_SUPPORTED;
    X86UserProcess *process = find_user_process(rad_process_current_pid());
    if (!process) return RAD_STATUS_NOT_FOUND;
    rad_shm_info_t info{};
    const int32_t info_status = rad_shm_get_info(static_cast<int32_t>(fd), &info);
    if (info_status != RAD_STATUS_OK) return info_status;
    if (length > info.byte_size) return RAD_STATUS_INVALID_ARGUMENT;
    const uint64_t mapping_size = align_up(length, PageSize);
    uint64_t va = align_up(process->next_mmap, PageSize);
    if (va < UserMmapBase) va = UserMmapBase;
    if (mapping_size > UserMmapLimit - va) return RAD_STATUS_NO_MEMORY;
    const size_t pages = static_cast<size_t>(mapping_size / PageSize);
    if (pages > info.page_count) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < pages; ++i) {
        uintptr_t physical = 0;
        const int32_t page_status = rad_shm_get_page(static_cast<int32_t>(fd), i, &physical);
        if (page_status != RAD_STATUS_OK) return page_status;
        if (!x86_vm_map_shared_page(&process->address_space, va + i * PageSize, static_cast<uint64_t>(physical), (prot & RAD_MMAP_PROT_WRITE) != 0)) {
            return RAD_STATUS_NO_MEMORY;
        }
    }
    process->next_mmap = va + mapping_size;
    rad_debug_marker("RADIX_MMAP_SHARED_OK");
    return static_cast<long>(va);
}

long x86_sys_brk(uint64_t requested) {
    X86UserProcess *process = find_user_process(rad_process_current_pid());
    if (!process) return 0;
    if (requested == 0) return static_cast<long>(process->brk_current);
    const uint64_t new_break = align_up(requested, PageSize);
    if (new_break < process->brk_base || new_break > process->brk_limit) {
        return static_cast<long>(process->brk_current);
    }
    if (new_break <= process->brk_current) {
        process->brk_current = new_break;
        return static_cast<long>(process->brk_current);
    }
    for (uint64_t va = process->brk_current; va < new_break; va += PageSize) {
        const uint64_t physical = x86_vm_alloc_page();
        if (!physical) return static_cast<long>(process->brk_current);
        memset(reinterpret_cast<void*>(static_cast<uintptr_t>(physical)), 0, PageSize);
        if (!x86_vm_map_user_page(&process->address_space, va, physical, 1)) {
            x86_vm_free_page(physical);
            return static_cast<long>(process->brk_current);
        }
    }
    process->brk_current = new_break;
    rad_debug_marker("RADIX_USER_BRK_OK");
    return static_cast<long>(process->brk_current);
}

extern "C" long x86_syscall_dispatch_frame(x86_user_trap_frame_t *frame) {
    if (!frame) return RAD_STATUS_INVALID_ARGUMENT;
    long result = 0;
    if (frame->rax == RAD_SYSCALL_FORK || frame->rax == LinuxSysFork) {
        ensure_process_arch_registered();
        result = rad_process_fork_from_arch_frame(frame);
    } else {
        result = x86_syscall_dispatch(frame->rax, frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8, frame->r9);
    }
    if (result != static_cast<long>(UserExitMagic)) restore_current_user_address_space();
    return result;
}

extern "C" long x86_syscall_dispatch(unsigned long number, unsigned long arg0, unsigned long arg1,
                                      unsigned long arg2, unsigned long arg3, unsigned long arg4,
                                      unsigned long arg5) {
    if (x86_cpu_current_core() > 0 && !__atomic_exchange_n(&g_user_ap_syscall_seen, 1, __ATOMIC_ACQ_REL)) {
        rad_debug_marker("RADIX_USER_AP_SYSCALL_OK");
    }
#if defined(RADIX_ENABLE_DEBUG_TRACE)
    if (X86UserProcess *trace_process = find_user_process(rad_process_current_pid())) {
        if (strstr(trace_process->path, "stress")) {
            char message[128];
            snprintf(message, sizeof(message), "RADIX_STRESS_SYSCALL path=%s nr=%lu\n", trace_process->path, number);
            rkdbg(message);
        }
    }
#endif
    if (number == RAD_SYSCALL_EXIT) {
        if (X86UserProcess *process = find_user_process(rad_process_current_pid())) {
            process->exit_code = static_cast<int32_t>(arg0);
            process->exiting = 1;
        }
        rad_process_exit(static_cast<int32_t>(arg0));
        return static_cast<long>(UserExitMagic);
    }
    if (number == LinuxSysExit || number == LinuxSysExitGroup) {
        if (X86UserProcess *process = find_user_process(rad_process_current_pid())) {
            process->exit_code = static_cast<int32_t>(arg0);
            process->exiting = 1;
        }
        rad_process_exit(static_cast<int32_t>(arg0));
        return static_cast<long>(UserExitMagic);
    }

    switch (number) {
    case RAD_SYSCALL_READ: {
        if (!user_range_ok(arg1, arg2)) return RAD_STATUS_INVALID_ARGUMENT;
        uint8_t chunk[256];
        size_t total = 0;
        while (total < arg2) {
            const size_t want = (arg2 - total) > sizeof(chunk) ? sizeof(chunk) : static_cast<size_t>(arg2 - total);
            const intptr_t got = rad_fd_read(static_cast<int32_t>(arg0), chunk, want);
            if (got < 0) return got;
            if (got == 0) break;
            if (x86_copy_to_user(arg1 + total, chunk, static_cast<size_t>(got)) != RAD_STATUS_OK) {
                return RAD_STATUS_INVALID_ARGUMENT;
            }
            total += static_cast<size_t>(got);
            if (static_cast<size_t>(got) < want) break;
        }
        return static_cast<long>(total);
    }
    case RAD_SYSCALL_WRITE: {
        if (!user_range_ok(arg1, arg2)) return RAD_STATUS_INVALID_ARGUMENT;
        char chunk[257];
        size_t total = 0;
        while (total < arg2) {
            const size_t want = (arg2 - total) > 256u ? 256u : static_cast<size_t>(arg2 - total);
            if (x86_copy_from_user(chunk, arg1 + total, want) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
            chunk[want] = '\0';
            const bool visible_fd = arg0 == 1 || arg0 == 2;
            if (visible_fd && g_hidden_marker_needs_newline) {
                if (newline_only(chunk, want)) {
                    x86_serial_write(chunk);
                    g_hidden_marker_needs_newline = false;
                    total += want;
                    continue;
                }
                x86_serial_write("\n");
                g_hidden_marker_needs_newline = false;
            }
            const bool marker_only = is_radix_marker_only(chunk, want);
            if (strstr(chunk, "RADIX_")) x86_serial_write(chunk);
            if (marker_only && visible_fd) {
                g_hidden_marker_needs_newline = !ends_with_newline(chunk, want);
                total += want;
                continue;
            }
            const intptr_t wrote = rad_fd_write(static_cast<int32_t>(arg0), chunk, want);
            if (wrote < 0) return wrote;
            total += static_cast<size_t>(wrote);
            if (static_cast<size_t>(wrote) < want) break;
        }
        return static_cast<long>(total);
    }
    case RAD_SYSCALL_OPEN: {
        char path[256];
        const rad_status_t copied = x86_copy_string_from_user(path, sizeof(path), arg0);
        if (copied != RAD_STATUS_OK) return copied;
        return rad_fd_open(path, static_cast<uint32_t>(arg1));
    }
    case RAD_SYSCALL_CLOSE:
        return rad_fd_close(static_cast<int32_t>(arg0));
    case RAD_SYSCALL_IOCTL: {
        const uint32_t request = static_cast<uint32_t>(arg1);
        const uint32_t dir = RAD_IOCTL_DIR(request);
        const size_t size = RAD_IOCTL_SIZE(request);
        if (size > 256u) return RAD_STATUS_NOT_SUPPORTED;
        uint8_t buffer[256];
        void *argument = nullptr;
        if (size) {
            if (!user_range_ok(arg2, size)) return RAD_STATUS_INVALID_ARGUMENT;
            argument = buffer;
            if ((dir == RAD_IOCTL_WRITE || dir == RAD_IOCTL_READWRITE)
                && x86_copy_from_user(buffer, arg2, size) != RAD_STATUS_OK) {
                return RAD_STATUS_INVALID_ARGUMENT;
            }
        }
        const intptr_t status = rad_fd_ioctl(static_cast<int32_t>(arg0), request, argument);
        if (status == RAD_STATUS_OK && size && (dir == RAD_IOCTL_READ || dir == RAD_IOCTL_READWRITE)) {
            if (x86_copy_to_user(arg2, buffer, size) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        }
        return status;
    }
    case RAD_SYSCALL_LSEEK:
        return static_cast<long>(rad_fd_lseek(static_cast<int32_t>(arg0), static_cast<int64_t>(arg1), static_cast<rad_seek_origin_t>(arg2)));
    case RAD_SYSCALL_STAT: {
        char path[256];
        rad_vfs_stat_t stat{};
        const rad_status_t copied = x86_copy_string_from_user(path, sizeof(path), arg0);
        if (copied != RAD_STATUS_OK) return copied;
        const int32_t status = rad_fd_stat(path, &stat);
        if (status != RAD_STATUS_OK) return status;
        return x86_copy_to_user(arg1, &stat, sizeof(stat));
    }
    case RAD_SYSCALL_FSTAT: {
        if (arg0 >= UserBase) return poll_from_user(arg0, arg1, arg2);
        rad_vfs_stat_t stat{};
        const int32_t status = rad_fd_fstat(static_cast<int32_t>(arg0), &stat);
        if (status != RAD_STATUS_OK) return status;
        return x86_copy_to_user(arg1, &stat, sizeof(stat));
    }
    case RAD_SYSCALL_GETTIMEOFDAY: {
        if (arg0 < UserBase) {
            return static_cast<long>(rad_fd_lseek(static_cast<int32_t>(arg0), static_cast<int64_t>(arg1), static_cast<rad_seek_origin_t>(arg2)));
        }
        if (!arg0) return RAD_STATUS_INVALID_ARGUMENT;
        rad_posix_timeval_t tv{};
        const uint64_t micros = rad_realtime_micros();
        tv.tv_sec = static_cast<int64_t>(micros / 1000000u);
        tv.tv_usec = static_cast<int64_t>(micros % 1000000u);
        return x86_copy_to_user(arg0, &tv, sizeof(tv));
    }
    case RAD_SYSCALL_SETTIMEOFDAY: {
        if (!arg0 || !user_range_ok(arg0, sizeof(rad_posix_timeval_t))) return RAD_STATUS_INVALID_ARGUMENT;
        rad_posix_timeval_t tv{};
        if (x86_copy_from_user(&tv, arg0, sizeof(tv)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        if (tv.tv_sec < 0 || tv.tv_usec < 0 || tv.tv_usec >= 1000000) return RAD_STATUS_INVALID_ARGUMENT;
        return rad_realtime_set_micros(static_cast<uint64_t>(tv.tv_sec) * 1000000u + static_cast<uint64_t>(tv.tv_usec));
    }
    case RAD_SYSCALL_NANOSLEEP:
        x86_syscall_delay_ns(arg0);
        return RAD_STATUS_OK;
    case RAD_SYSCALL_FORK:
        ensure_process_arch_registered();
        return rad_process_fork();
    case LinuxSysFork:
        ensure_process_arch_registered();
        return rad_process_fork();
    case RAD_SYSCALL_EXECVE: {
        char path[128];
        const rad_status_t copied = x86_copy_string_from_user(path, sizeof(path), arg0);
        if (copied != RAD_STATUS_OK) {
            rad_debug_marker("RADIX_USER_EXECVE_COPY_FAIL");
            return copied;
        }
        X86UserProcess *process = find_user_process(rad_process_current_pid());
        if (!process) {
            rad_debug_marker("RADIX_USER_EXECVE_PROCESS_FAIL");
            return RAD_STATUS_NOT_FOUND;
        }
        const rad_status_t copied_args = set_process_args_from_user(process, path, arg1);
        if (copied_args != RAD_STATUS_OK) {
            rad_debug_marker("RADIX_USER_EXECVE_ARGS_FAIL");
            return copied_args;
        }
        strncpy(process->exec_path, path, sizeof(process->exec_path) - 1u);
        process->exec_path[sizeof(process->exec_path) - 1u] = '\0';
        process->exec_pending = 1;
        rad_debug_marker("RADIX_USER_EXECVE_REQUEST_OK");
        return static_cast<long>(UserExitMagic);
    }
    case LinuxSysExecve: {
        char path[128];
        const rad_status_t copied = x86_copy_string_from_user(path, sizeof(path), arg0);
        if (copied != RAD_STATUS_OK) return copied;
        X86UserProcess *process = find_user_process(rad_process_current_pid());
        if (!process) return RAD_STATUS_NOT_FOUND;
        const rad_status_t copied_args = set_process_args_from_user(process, path, arg1);
        if (copied_args != RAD_STATUS_OK) return copied_args;
        strncpy(process->exec_path, path, sizeof(process->exec_path) - 1u);
        process->exec_path[sizeof(process->exec_path) - 1u] = '\0';
        process->exec_pending = 1;
        return static_cast<long>(UserExitMagic);
    }
    case RAD_SYSCALL_WAITPID: {
        int32_t status = 0;
        X86UserProcess *caller = find_user_process(rad_process_current_pid());
        const int32_t waited = rad_process_waitpid(static_cast<int32_t>(arg0), arg1 ? &status : nullptr, static_cast<uint32_t>(arg2));
        if (caller) {
            rad_process_set_current_pid(caller->pid);
            x86_vm_activate_address_space(&caller->address_space);
            x86_user_set_active_context(&caller->context);
        }
        if (waited >= 0 && arg1 && x86_copy_to_user(arg1, &status, sizeof(status)) != RAD_STATUS_OK) {
            return RAD_STATUS_INVALID_ARGUMENT;
        }
        if (waited > 0) {
            rad_debug_marker("RADIX_USER_WAIT_WAKE_OK");
            rad_debug_marker("RADIX_USER_ZOMBIE_REAP_OK");
        }
        return waited;
    }
    case RAD_SYSCALL_KILL:
        return rad_process_kill(static_cast<int32_t>(arg0), static_cast<int32_t>(arg1));
    case LinuxSysWait4: {
        int32_t status = 0;
        X86UserProcess *caller = find_user_process(rad_process_current_pid());
        const int32_t waited = rad_process_waitpid(static_cast<int32_t>(arg0), arg1 ? &status : nullptr, static_cast<uint32_t>(arg2));
        if (caller) {
            rad_process_set_current_pid(caller->pid);
            x86_vm_activate_address_space(&caller->address_space);
            x86_user_set_active_context(&caller->context);
        }
        if (waited >= 0 && arg1 && x86_copy_to_user(arg1, &status, sizeof(status)) != RAD_STATUS_OK) {
            return RAD_STATUS_INVALID_ARGUMENT;
        }
        if (waited > 0) {
            rad_debug_marker("RADIX_USER_WAIT_WAKE_OK");
            rad_debug_marker("RADIX_USER_ZOMBIE_REAP_OK");
        }
        return waited;
    }
    case RAD_SYSCALL_GETPID:
        return rad_process_current_pid();
    case LinuxSysGetpid:
        if ((arg0 >= UserBase && arg1 <= 64u) || (arg0 == 0 && arg1 == 0 && arg2 != 0)) {
            return poll_from_user(arg0, arg1, arg2);
        }
        return rad_process_current_pid();
    case RAD_SYSCALL_GETPPID:
        return rad_process_parent_pid();
    case LinuxSysGetppid:
        return rad_process_parent_pid();
    case RAD_SYSCALL_GETUID:
    case LinuxSysGetuid:
        return static_cast<long>(rad_process_getuid());
    case RAD_SYSCALL_GETEUID:
    case LinuxSysGeteuid:
        return static_cast<long>(rad_process_geteuid());
    case RAD_SYSCALL_GETGID:
    case LinuxSysGetgid:
        return static_cast<long>(rad_process_getgid());
    case RAD_SYSCALL_GETEGID:
    case LinuxSysGetegid:
        return static_cast<long>(rad_process_getegid());
    case RAD_SYSCALL_SETUID:
    case LinuxSysSetuid:
        return rad_process_setuid(static_cast<rad_uid_t>(arg0));
    case RAD_SYSCALL_SETGID:
    case LinuxSysSetgid:
        return rad_process_setgid(static_cast<rad_gid_t>(arg0));
    case RAD_SYSCALL_DUP:
        return rad_fd_dup(static_cast<int32_t>(arg0));
    case RAD_SYSCALL_DUP2:
        return rad_fd_dup2(static_cast<int32_t>(arg0), static_cast<int32_t>(arg1));
    case RAD_SYSCALL_CHDIR: {
        char path[256];
        const rad_status_t copied = x86_copy_string_from_user(path, sizeof(path), arg0);
        if (copied != RAD_STATUS_OK) return copied;
        return rad_vfs_chdir(path);
    }
    case RAD_SYSCALL_GETCWD: {
        char path[256];
        const rad_status_t status = rad_vfs_getcwd(path, arg1 < sizeof(path) ? static_cast<size_t>(arg1) : sizeof(path));
        if (status != RAD_STATUS_OK) return status;
        const size_t length = strlen(path) + 1u;
        if (length > arg1) return RAD_STATUS_INVALID_ARGUMENT;
        return x86_copy_to_user(arg0, path, length);
    }
    case LinuxSysGetcwd: {
        char path[256];
        const rad_status_t status = rad_vfs_getcwd(path, arg1 < sizeof(path) ? static_cast<size_t>(arg1) : sizeof(path));
        if (status != RAD_STATUS_OK) return status;
        const size_t length = strlen(path) + 1u;
        if (length > arg1) return RAD_STATUS_INVALID_ARGUMENT;
        const rad_status_t copied = x86_copy_to_user(arg0, path, length);
        return copied == RAD_STATUS_OK ? static_cast<long>(length) : static_cast<long>(copied);
    }
    case LinuxSysOpenat: {
        char path[256];
        const rad_status_t copied = x86_copy_string_from_user(path, sizeof(path), arg1);
        if (copied != RAD_STATUS_OK) return copied;
        return rad_fd_open(path, static_cast<uint32_t>(arg2));
    }
    case RAD_SYSCALL_BRK:
        return x86_sys_brk(arg0);
    case RAD_SYSCALL_PIPE: {
        int32_t pipefd[2]{};
        const int32_t status = rad_pipe_create(pipefd);
        if (status != RAD_STATUS_OK) return status;
        return x86_copy_to_user(arg0, pipefd, sizeof(pipefd));
    }
    case RAD_SYSCALL_FCNTL:
    case LinuxSysFcntl:
        return rad_fd_fcntl(static_cast<int32_t>(arg0), static_cast<uint32_t>(arg1), arg2);
    case RAD_SYSCALL_GETDENTS: {
        if (arg2 > 16u || !user_range_ok(arg1, arg2 * sizeof(rad_dirent_user_t))) return RAD_STATUS_INVALID_ARGUMENT;
        rad_dirent_user_t entries[16]{};
        const intptr_t count = rad_fd_getdents(static_cast<int32_t>(arg0), entries, static_cast<size_t>(arg2));
        if (count <= 0) return static_cast<long>(count);
        const size_t bytes = static_cast<size_t>(count) * sizeof(rad_dirent_user_t);
        return x86_copy_to_user(arg1, entries, bytes) == RAD_STATUS_OK ? static_cast<long>(count) : static_cast<long>(RAD_STATUS_INVALID_ARGUMENT);
    }
    case RAD_SYSCALL_REMOVE: {
        char path[256];
        const rad_status_t copied = x86_copy_string_from_user(path, sizeof(path), arg0);
        return copied == RAD_STATUS_OK ? rad_vfs_remove(path) : copied;
    }
    case RAD_SYSCALL_MKDIR: {
        char path[256];
        const rad_status_t copied = x86_copy_string_from_user(path, sizeof(path), arg0);
        return copied == RAD_STATUS_OK ? rad_vfs_mkdir(path) : copied;
    }
    case RAD_SYSCALL_RMDIR: {
        char path[256];
        const rad_status_t copied = x86_copy_string_from_user(path, sizeof(path), arg0);
        return copied == RAD_STATUS_OK ? rad_vfs_rmdir(path) : copied;
    }
    case RAD_SYSCALL_RENAME: {
        char old_path[256];
        char new_path[256];
        const rad_status_t copied_old = x86_copy_string_from_user(old_path, sizeof(old_path), arg0);
        if (copied_old != RAD_STATUS_OK) return copied_old;
        const rad_status_t copied_new = x86_copy_string_from_user(new_path, sizeof(new_path), arg1);
        return copied_new == RAD_STATUS_OK ? rad_vfs_rename(old_path, new_path) : copied_new;
    }
    case RAD_SYSCALL_TRUNCATE: {
        char path[256];
        const rad_status_t copied = x86_copy_string_from_user(path, sizeof(path), arg0);
        return copied == RAD_STATUS_OK ? rad_vfs_truncate(path, arg1) : copied;
    }
    case RAD_SYSCALL_CHMOD: {
        char path[256];
        const rad_status_t copied = x86_copy_string_from_user(path, sizeof(path), arg0);
        return copied == RAD_STATUS_OK ? rad_vfs_chmod(path, static_cast<uint32_t>(arg1)) : copied;
    }
    case RAD_SYSCALL_LINK: {
        char old_path[256];
        char new_path[256];
        const rad_status_t copied_old = x86_copy_string_from_user(old_path, sizeof(old_path), arg0);
        if (copied_old != RAD_STATUS_OK) return copied_old;
        const rad_status_t copied_new = x86_copy_string_from_user(new_path, sizeof(new_path), arg1);
        return copied_new == RAD_STATUS_OK ? rad_vfs_link(old_path, new_path) : copied_new;
    }
    case RAD_SYSCALL_SYMLINK: {
        char target[256];
        char link_path[256];
        const rad_status_t copied_target = x86_copy_string_from_user(target, sizeof(target), arg0);
        if (copied_target != RAD_STATUS_OK) return copied_target;
        const rad_status_t copied_link = x86_copy_string_from_user(link_path, sizeof(link_path), arg1);
        return copied_link == RAD_STATUS_OK ? rad_vfs_symlink(target, link_path) : copied_link;
    }
    case RAD_SYSCALL_READLINK: {
        char path[256];
        char target[256]{};
        const rad_status_t copied = x86_copy_string_from_user(path, sizeof(path), arg0);
        if (copied != RAD_STATUS_OK) return copied;
        const size_t capacity = arg2 < sizeof(target) ? static_cast<size_t>(arg2) : sizeof(target);
        const rad_status_t status = rad_vfs_readlink(path, target, capacity);
        if (status != RAD_STATUS_OK) return status;
        const size_t length = strlen(target);
        return x86_copy_to_user(arg1, target, length) == RAD_STATUS_OK ? static_cast<long>(length) : static_cast<long>(RAD_STATUS_INVALID_ARGUMENT);
    }
    case RAD_SYSCALL_FSYNC: {
        return rad_syscall_dispatch(RAD_SYSCALL_FSYNC, arg0, 0, 0, 0, 0, 0);
    }
    case RAD_SYSCALL_LOG_READ: {
        if (arg1 > 16u || (arg1 && !user_range_ok(arg0, arg1 * sizeof(rad_log_entry_t)))) return RAD_STATUS_INVALID_ARGUMENT;
        rad_log_entry_t entries[16]{};
        const size_t count = rad_log_read(entries, static_cast<size_t>(arg1), arg2);
        const size_t copy_count = count < arg1 ? count : static_cast<size_t>(arg1);
        if (copy_count && x86_copy_to_user(arg0, entries, copy_count * sizeof(rad_log_entry_t)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        return static_cast<long>(count);
    }
    case RAD_SYSCALL_LOG_FLUSH: {
        char path[256];
        const rad_status_t copied = x86_copy_string_from_user(path, sizeof(path), arg0);
        return copied == RAD_STATUS_OK ? rad_log_flush_to_path(path) : copied;
    }
    case RAD_SYSCALL_ACCESS: {
        char path[256];
        const rad_status_t copied = x86_copy_string_from_user(path, sizeof(path), arg0);
        if (copied != RAD_STATUS_OK) return copied;
        rad_vfs_stat_t stat{};
        return rad_vfs_stat(path, &stat);
    }
    case RAD_SYSCALL_ISATTY: {
        rad_vfs_stat_t stat{};
        const int32_t status = rad_fd_fstat(static_cast<int32_t>(arg0), &stat);
        return status == RAD_STATUS_OK && ((stat.mode & 0170000u) == 0020000u) ? 1 : 0;
    }
    case RAD_SYSCALL_SOCKET:
    case LinuxSysSocket:
        return rad_socket_create(static_cast<int>(arg0), static_cast<int>(arg1), static_cast<int>(arg2));
    case RAD_SYSCALL_BIND:
    case LinuxSysBind: {
        if (!user_range_ok(arg1, arg2) || arg2 < sizeof(rad_sockaddr_in_t)) return RAD_STATUS_INVALID_ARGUMENT;
        rad_sockaddr_in_t address{};
        if (x86_copy_from_user(&address, arg1, sizeof(address)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        return rad_socket_bind(static_cast<int32_t>(arg0), &address, sizeof(address));
    }
    case RAD_SYSCALL_CONNECT:
    case LinuxSysConnect: {
        if (!user_range_ok(arg1, arg2) || arg2 < sizeof(rad_sockaddr_in_t)) return RAD_STATUS_INVALID_ARGUMENT;
        rad_sockaddr_in_t address{};
        if (x86_copy_from_user(&address, arg1, sizeof(address)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        return rad_socket_connect(static_cast<int32_t>(arg0), &address, sizeof(address));
    }
    case RAD_SYSCALL_LISTEN:
    case LinuxSysListen:
        return rad_socket_listen(static_cast<int32_t>(arg0), static_cast<int>(arg1));
    case RAD_SYSCALL_ACCEPT:
    case LinuxSysAccept: {
        if (arg1 && !user_range_ok(arg1, sizeof(rad_sockaddr_in_t))) return RAD_STATUS_INVALID_ARGUMENT;
        if (number == LinuxSysAccept && arg2 && !user_range_ok(arg2, sizeof(uint32_t))) return RAD_STATUS_INVALID_ARGUMENT;
        if (number != LinuxSysAccept && arg2 && !user_range_ok(arg2, sizeof(size_t))) return RAD_STATUS_INVALID_ARGUMENT;
        rad_sockaddr_in_t address{};
        size_t address_length = sizeof(address);
        if (number != LinuxSysAccept && arg2 && x86_copy_from_user(&address_length, arg2, sizeof(address_length)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        const int32_t accepted = rad_socket_accept(static_cast<int32_t>(arg0), arg1 ? &address : nullptr, arg2 ? &address_length : nullptr);
        if (accepted < 0) return accepted;
        if (arg1 && x86_copy_to_user(arg1, &address, sizeof(address)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        if (number == LinuxSysAccept && arg2) {
            const uint32_t linux_address_length = static_cast<uint32_t>(address_length);
            if (x86_copy_to_user(arg2, &linux_address_length, sizeof(linux_address_length)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        } else if (arg2 && x86_copy_to_user(arg2, &address_length, sizeof(address_length)) != RAD_STATUS_OK) {
            return RAD_STATUS_INVALID_ARGUMENT;
        }
        return accepted;
    }
    case RAD_SYSCALL_SHUTDOWN:
    case LinuxSysShutdown:
        return rad_socket_shutdown(static_cast<int32_t>(arg0), static_cast<int>(arg1));
    case RAD_SYSCALL_SENDTO:
    case LinuxSysSendto: {
        if (!user_range_ok(arg1, arg2) || arg2 > 1400u) return RAD_STATUS_INVALID_ARGUMENT;
        uint8_t payload[1400];
        if (x86_copy_from_user(payload, arg1, static_cast<size_t>(arg2)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        if (!arg4 && arg5 == 0) {
            return rad_socket_send(static_cast<int32_t>(arg0), payload, static_cast<size_t>(arg2), static_cast<uint32_t>(arg3));
        }
        if (!user_range_ok(arg4, arg5) || arg5 < sizeof(rad_sockaddr_in_t)) return RAD_STATUS_INVALID_ARGUMENT;
        rad_sockaddr_in_t address{};
        if (x86_copy_from_user(&address, arg4, sizeof(address)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        return rad_socket_sendto(static_cast<int32_t>(arg0), payload, static_cast<size_t>(arg2), static_cast<uint32_t>(arg3), &address, sizeof(address));
    }
    case RAD_SYSCALL_RECVFROM:
    case LinuxSysRecvfrom: {
        if (!user_range_ok(arg1, arg2) || arg2 > 1400u) return RAD_STATUS_INVALID_ARGUMENT;
        uint8_t payload[1400];
        if (!arg4 && !arg5) {
            const intptr_t received = rad_socket_recv(static_cast<int32_t>(arg0), payload, static_cast<size_t>(arg2), static_cast<uint32_t>(arg3));
            if (received <= 0) return static_cast<long>(received);
            return x86_copy_to_user(arg1, payload, static_cast<size_t>(received)) == RAD_STATUS_OK ? static_cast<long>(received) : static_cast<long>(RAD_STATUS_INVALID_ARGUMENT);
        }
        rad_sockaddr_in_t address{};
        size_t address_length = sizeof(address);
        if (number == LinuxSysRecvfrom && arg5) {
            if (!user_range_ok(arg5, sizeof(uint32_t))) return RAD_STATUS_INVALID_ARGUMENT;
            uint32_t linux_address_length = 0;
            if (x86_copy_from_user(&linux_address_length, arg5, sizeof(linux_address_length)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
            address_length = linux_address_length;
        } else if (arg5) {
            if (!user_range_ok(arg5, sizeof(size_t))) return RAD_STATUS_INVALID_ARGUMENT;
            if (x86_copy_from_user(&address_length, arg5, sizeof(address_length)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        }
        const intptr_t received = rad_socket_recvfrom(static_cast<int32_t>(arg0), payload, static_cast<size_t>(arg2), static_cast<uint32_t>(arg3), arg4 ? &address : nullptr, arg5 ? &address_length : nullptr);
        if (received <= 0) return static_cast<long>(received);
        if (x86_copy_to_user(arg1, payload, static_cast<size_t>(received)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        if (arg4 && address_length >= sizeof(rad_sockaddr_in_t)) {
            if (!user_range_ok(arg4, sizeof(address))) return RAD_STATUS_INVALID_ARGUMENT;
            if (x86_copy_to_user(arg4, &address, sizeof(address)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        }
        if (number == LinuxSysRecvfrom && arg5) {
            const uint32_t linux_address_length = static_cast<uint32_t>(address_length);
            if (x86_copy_to_user(arg5, &linux_address_length, sizeof(linux_address_length)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        } else if (arg5 && x86_copy_to_user(arg5, &address_length, sizeof(address_length)) != RAD_STATUS_OK) {
            return RAD_STATUS_INVALID_ARGUMENT;
        }
        return static_cast<long>(received);
    }
    case RAD_SYSCALL_SETSOCKOPT:
    case LinuxSysSetsockopt: {
        uint8_t value[64];
        const void *value_ptr = nullptr;
        if (arg4) {
            if (arg4 > sizeof(value) || !user_range_ok(arg3, arg4)) return RAD_STATUS_INVALID_ARGUMENT;
            if (x86_copy_from_user(value, arg3, static_cast<size_t>(arg4)) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
            value_ptr = value;
        }
        return rad_socket_setsockopt(static_cast<int32_t>(arg0), static_cast<int>(arg1), static_cast<int>(arg2), value_ptr, static_cast<size_t>(arg4));
    }
    case RAD_SYSCALL_GETSOCKOPT:
    case LinuxSysGetsockopt: {
        if (!arg3 || !arg4 || !user_range_ok(arg4, sizeof(size_t))) return RAD_STATUS_INVALID_ARGUMENT;
        size_t value_length = 0;
        if (x86_copy_from_user(&value_length, arg4, sizeof(value_length)) != RAD_STATUS_OK || value_length > 64u) return RAD_STATUS_INVALID_ARGUMENT;
        uint8_t value[64]{};
        const int32_t status = rad_socket_getsockopt(static_cast<int32_t>(arg0), static_cast<int>(arg1), static_cast<int>(arg2), value, &value_length);
        if (status != RAD_STATUS_OK) return status;
        if (!user_range_ok(arg3, value_length)) return RAD_STATUS_INVALID_ARGUMENT;
        if (x86_copy_to_user(arg3, value, value_length) != RAD_STATUS_OK) return RAD_STATUS_INVALID_ARGUMENT;
        return x86_copy_to_user(arg4, &value_length, sizeof(value_length));
    }
    case RAD_SYSCALL_SHM_OPEN:
        return x86_sys_shm_open(arg0, arg1, arg2);
    case RAD_SYSCALL_SHM_UNLINK: {
        char name[RAD_SHM_NAME_MAX]{};
        const rad_status_t copied = x86_copy_string_from_user(name, sizeof(name), arg0);
        if (copied != RAD_STATUS_OK) return copied;
        return rad_shm_unlink(name);
    }
    case RAD_SYSCALL_MMAP:
        return x86_sys_mmap(arg0, arg1, arg2, arg3, arg4, arg5);
    case RAD_SYSCALL_MUNMAP:
        return user_range_ok(arg0, arg1) ? RAD_STATUS_OK : RAD_STATUS_INVALID_ARGUMENT;
    default:
        return RAD_STATUS_NOT_SUPPORTED;
    }
}

extern "C" rad_status_t x86_copy_from_user(void *dst, uint64_t src, size_t size) {
    if ((!dst && size) || !x86_vm_validate_user_range(src, size, 0)) return RAD_STATUS_INVALID_ARGUMENT;
    if (size) memcpy(dst, reinterpret_cast<const void*>(static_cast<uintptr_t>(src)), size);
    return RAD_STATUS_OK;
}

extern "C" rad_status_t x86_copy_to_user(uint64_t dst, const void *src, size_t size) {
    if ((!src && size) || !x86_vm_validate_user_range(dst, size, 1)) return RAD_STATUS_INVALID_ARGUMENT;
    if (size) memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(dst)), src, size);
    return RAD_STATUS_OK;
}

extern "C" rad_status_t x86_copy_string_from_user(char *dst, size_t dst_size, uint64_t src) {
    if (!dst || dst_size == 0 || !user_range_ok(src, 1)) return RAD_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0; i < dst_size; ++i) {
        if (!user_range_ok(src + i, 1)) return RAD_STATUS_INVALID_ARGUMENT;
        const char ch = *reinterpret_cast<const char*>(static_cast<uintptr_t>(src + i));
        dst[i] = ch;
        if (!ch) return RAD_STATUS_OK;
    }
    dst[dst_size - 1] = '\0';
    return RAD_STATUS_INVALID_ARGUMENT;
}

extern "C" int x86_user_copy_self_test(void) {
    constexpr uint64_t test_addr = (UserStackTop - UserStackSize) + 256u;
    const char source[] = "rad-user-copy";
    char copied[sizeof(source)]{};
    memcpy(reinterpret_cast<void*>(static_cast<uintptr_t>(test_addr)), source, sizeof(source));
    if (x86_copy_from_user(copied, test_addr, sizeof(copied)) != RAD_STATUS_OK) return 0;
    if (memcmp(copied, source, sizeof(source)) != 0) return 0;
    const char out[] = "ok";
    if (x86_copy_to_user(test_addr + 64u, out, sizeof(out)) != RAD_STATUS_OK) return 0;
    if (memcmp(reinterpret_cast<const void*>(static_cast<uintptr_t>(test_addr + 64u)), out, sizeof(out)) != 0) return 0;
    if (x86_copy_from_user(copied, 0x1000u, 1u) != RAD_STATUS_INVALID_ARGUMENT) return 0;
    if (x86_copy_to_user(0x1000u, out, 1u) != RAD_STATUS_INVALID_ARGUMENT) return 0;
    return 1;
}

extern "C" int x86_user_run_init(const char *path) {
    ensure_process_arch_registered();
    int32_t pid = 0;
    rad_task_t task = nullptr;
    if (x86_user_spawn_process(path, rad_process_current_pid(), &pid, &task) != RAD_STATUS_OK) {
        rad_debug_marker("RADIX_USER_INIT_LOAD_FAIL");
        return 0;
    }
    if (strcmp(path, "/bin/init") == 0 || strcmp(path, "/sbin/radinit") == 0) {
        rad_debug_marker("RADIX_RADINIT_SPAWN_OK");
        (void)task;
        return 1;
    }
    return x86_user_wait_process(pid);
}

rad_status_t install_child_stdio(int32_t pid, const char *stdio_path) {
    if (!stdio_path || !*stdio_path) return RAD_STATUS_OK;
    const int32_t previous = rad_process_current_pid();
    rad_process_set_current_pid(pid);
    rad_status_t status = RAD_STATUS_OK;
    for (int32_t fd = 0; fd < 3; ++fd) {
        const int32_t opened = rad_fd_open(stdio_path, RAD_VFS_READ | RAD_VFS_WRITE);
        if (opened < 0) {
            status = static_cast<rad_status_t>(opened);
            break;
        }
        const int32_t installed = rad_fd_dup2(opened, fd);
        if (installed != fd) {
            status = installed < 0 ? static_cast<rad_status_t>(installed) : RAD_STATUS_ERROR;
            rad_fd_close(opened);
            break;
        }
        if (opened != fd) rad_fd_close(opened);
    }
    if (status == RAD_STATUS_OK) rad_debug_marker("RADIX_USER_PROCESS_FD_NAMESPACE_OK");
    rad_process_set_current_pid(previous);
    return status;
}

extern "C" rad_status_t x86_user_spawn_process_with_stdio(const char *path, int32_t parent_pid, const char *stdio_path, int32_t *pid_out, rad_task_t *task_out) {
    if (!path || !*path) return RAD_STATUS_INVALID_ARGUMENT;
    ensure_process_arch_registered();
    X86UserProcess *process = allocate_user_process();
    if (!process) return RAD_STATUS_NO_MEMORY;
    memset(process, 0, sizeof(*process));
    const int32_t pid = rad_process_create(path, parent_pid);
    if (pid < 0) return static_cast<rad_status_t>(pid);
    process->used = 1;
    process->pid = pid;
    process->parent_pid = parent_pid >= 0 ? parent_pid : rad_process_current_pid();
    process->exit_code = 0;
    process->target_core = RAD_TASK_CORE_ANY;
    set_default_process_args(process, path);
    rad_status_t status = load_user_program(process, path);
    if (status != RAD_STATUS_OK) {
        process->used = 0;
        rad_process_exit(static_cast<int32_t>(status));
        return status;
    }
    status = install_child_stdio(pid, stdio_path);
    if (status != RAD_STATUS_OK) {
        x86_vm_destroy_address_space(&process->address_space);
        process->used = 0;
        rad_process_exit(static_cast<int32_t>(status));
        return status;
    }
    status = start_user_process_task(process);
    if (status != RAD_STATUS_OK) {
        x86_vm_destroy_address_space(&process->address_space);
        process->used = 0;
        rad_process_exit(static_cast<int32_t>(status));
        return status;
    }
    process_fd_table_self_test(pid);
    if (pid_out) *pid_out = pid;
    if (task_out) *task_out = process->task;
    rad_debug_marker("RADIX_USER_PROCESS_SPAWN_OK");
    return RAD_STATUS_OK;
}

extern "C" rad_status_t x86_user_spawn_process(const char *path, int32_t parent_pid, int32_t *pid_out, rad_task_t *task_out) {
    return x86_user_spawn_process_with_stdio(path, parent_pid, nullptr, pid_out, task_out);
}

extern "C" rad_status_t radix_shell_launch_terminal_process(void) {
    int32_t pid = 0;
    rad_task_t task = nullptr;
    const rad_status_t status = x86_user_spawn_process("/bin/radsh", rad_process_current_pid(), &pid, &task);
    if (status != RAD_STATUS_OK) return status;
    if (!x86_user_wait_process(pid)) return RAD_STATUS_ERROR;
    rad_debug_marker("RADIX_USER_RADSH_EXIT_OK");
    rad_debug_marker("RADIX_SLINT_APP_LAUNCH_PROCESS_OK");
    return RAD_STATUS_OK;
}

extern "C" int x86_user_wait_process(int32_t pid) {
    int32_t status = 0;
    const int32_t waited = rad_process_waitpid(pid, &status, 0);
    if (waited == pid) {
        rad_debug_marker("RADIX_USER_PROCESS_WAIT_OK");
        rad_debug_marker("RADIX_USER_WAIT_WAKE_OK");
        rad_debug_marker("RADIX_USER_ZOMBIE_REAP_OK");
        return 1;
    }
    return 0;
}
