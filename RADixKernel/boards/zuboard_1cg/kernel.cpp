#include <radixkernel/radixkernel.h>
#include <radixkernel_a53.h>
#include <radboot.h>

#include <stdint.h>
#include <string.h>

extern "C" char __image_end;
extern "C" uintptr_t radix_zuboard_boot_argument;
extern "C" void radix_zuboard_entry(rad_boot_handoff_t *incoming_handoff);
extern "C" void rad_zynqmp_bind_handoff(const rad_boot_handoff_t *handoff);
extern "C" rad_status_t rad_zynqmp_preempt_init(void);
extern "C" rad_status_t rad_zynqmp_gem_rx_irq_selftest(void);
extern "C" rad_status_t rad_zynqmp_smp_release(void);
extern "C" uint64_t rad_hal_time_micros(void);
extern "C" void rad_hal_worker_wake(void);
extern "C" rad_status_t x86_ext4_mount_root(const char *block_device);
extern "C" int x86_ext4_self_test(void);
extern "C" rad_status_t rad_a53_user_spawn_process_with_stdio(const char *path, int32_t parent_pid, const char *stdio_path, int32_t *pid_out, rad_task_t *task_out);
extern "C" const unsigned char _binary_a53_init_elf_start[];
extern "C" const unsigned char _binary_a53_init_elf_end[];
extern "C" const unsigned char _binary_a53_radsh_elf_start[];
extern "C" const unsigned char _binary_a53_radsh_elf_end[];
extern "C" const unsigned char _binary_a53_sh_elf_start[];
extern "C" const unsigned char _binary_a53_sh_elf_end[];

namespace {
constexpr uintptr_t KernelBase = 0x00200000u;
constexpr uintptr_t ZynqmpUart0Base = 0xff000000u;
constexpr uintptr_t ZynqmpGicDistributorBase = 0xf9010000u;
constexpr uintptr_t ZynqmpGicCpuBase = 0xf9020000u;
constexpr uintptr_t ZuboardDdrBase = 0x00000000u;
constexpr uintptr_t ZuboardDdrSize = 1024u * 1024u * 1024u;

struct BinEntry {
    const char *name;
    const uint8_t *data;
    size_t size;
    uint32_t mode;
};

struct BinHandle {
    const BinEntry *entry;
    size_t position;
    int used;
};

constexpr char TestScript[] = "#!/bin/sh\nradsh-exec-smoke\n";
BinEntry g_bin_entries[4];
size_t g_bin_entry_count = 0;
BinHandle g_bin_handles[8];

void marker(const char *text) {
    rad_debug_marker(text);
}

const BinEntry *find_bin_entry(const char *path) {
    const char *name = path && path[0] == '/' ? path + 1 : path;
    if (!name || !*name) return nullptr;
    for (size_t i = 0; i < g_bin_entry_count; ++i) {
        if (strcmp(g_bin_entries[i].name, name) == 0) return &g_bin_entries[i];
    }
    return nullptr;
}

rad_status_t bin_open(void*, const char *path, uint32_t flags, void **file) {
    if (!file || (flags & RAD_VFS_WRITE)) return RAD_STATUS_INVALID_ARGUMENT;
    const BinEntry *entry = find_bin_entry(path);
    if (!entry) return RAD_STATUS_NOT_FOUND;
    for (auto& handle : g_bin_handles) {
        if (handle.used) continue;
        handle.used = 1;
        handle.entry = entry;
        handle.position = 0;
        *file = &handle;
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NO_MEMORY;
}

rad_status_t bin_read(void *file, void *buffer, size_t size, size_t *bytes_read) {
    auto *handle = static_cast<BinHandle *>(file);
    if (!handle || !handle->used || !handle->entry || !buffer) return RAD_STATUS_INVALID_ARGUMENT;
    const size_t available = handle->position < handle->entry->size ? handle->entry->size - handle->position : 0;
    const size_t count = available < size ? available : size;
    if (count) memcpy(buffer, handle->entry->data + handle->position, count);
    handle->position += count;
    if (bytes_read) *bytes_read = count;
    return RAD_STATUS_OK;
}

rad_status_t bin_write(void*, const void*, size_t, size_t *bytes_written) {
    if (bytes_written) *bytes_written = 0;
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t bin_seek(void *file, int64_t offset, rad_seek_origin_t origin) {
    auto *handle = static_cast<BinHandle *>(file);
    if (!handle || !handle->used || !handle->entry) return RAD_STATUS_INVALID_ARGUMENT;
    int64_t base = 0;
    if (origin == RAD_SEEK_CUR) base = static_cast<int64_t>(handle->position);
    else if (origin == RAD_SEEK_END) base = static_cast<int64_t>(handle->entry->size);
    const int64_t next = base + offset;
    if (next < 0 || static_cast<uint64_t>(next) > handle->entry->size) return RAD_STATUS_INVALID_ARGUMENT;
    handle->position = static_cast<size_t>(next);
    return RAD_STATUS_OK;
}

uint64_t bin_tell(void *file) {
    auto *handle = static_cast<BinHandle *>(file);
    return handle && handle->used ? handle->position : 0;
}

void bin_close(void *file) {
    auto *handle = static_cast<BinHandle *>(file);
    if (handle) memset(handle, 0, sizeof(*handle));
}

rad_status_t bin_stat(void*, const char *path, rad_vfs_stat_t *stat) {
    if (!stat) return RAD_STATUS_INVALID_ARGUMENT;
    memset(stat, 0, sizeof(*stat));
    const char *name = path && path[0] == '/' ? path + 1 : path;
    if (!name || !*name) {
        stat->is_directory = 1;
        stat->mode = 0040755u;
        return RAD_STATUS_OK;
    }
    const BinEntry *entry = find_bin_entry(path);
    if (!entry) return RAD_STATUS_NOT_FOUND;
    stat->size = entry->size;
    stat->mode = entry->mode;
    return RAD_STATUS_OK;
}

rad_status_t bin_list(void*, const char *path, rad_vfs_list_callback_t callback, void *context) {
    if (!callback || (path && *path)) return RAD_STATUS_NOT_FOUND;
    for (size_t i = 0; i < g_bin_entry_count; ++i) {
        rad_vfs_stat_t stat{};
        stat.size = g_bin_entries[i].size;
        stat.mode = g_bin_entries[i].mode;
        if (!callback(g_bin_entries[i].name, &stat, context)) break;
    }
    return RAD_STATUS_OK;
}

void mount_embedded_bin(void) {
    g_bin_entries[0] = BinEntry{"init", _binary_a53_init_elf_start, static_cast<size_t>(_binary_a53_init_elf_end - _binary_a53_init_elf_start), 0100755u};
    g_bin_entries[1] = BinEntry{"radsh", _binary_a53_radsh_elf_start, static_cast<size_t>(_binary_a53_radsh_elf_end - _binary_a53_radsh_elf_start), 0100755u};
    g_bin_entries[2] = BinEntry{"sh", _binary_a53_sh_elf_start, static_cast<size_t>(_binary_a53_sh_elf_end - _binary_a53_sh_elf_start), 0100755u};
    g_bin_entries[3] = BinEntry{"test.sh", reinterpret_cast<const uint8_t *>(TestScript), sizeof(TestScript) - 1u, 0100644u};
    g_bin_entry_count = 4;
    rad_vfs_backend_ops_t ops{};
    ops.open = bin_open;
    ops.read = bin_read;
    ops.write = bin_write;
    ops.seek = bin_seek;
    ops.tell = bin_tell;
    ops.close = bin_close;
    ops.stat = bin_stat;
    ops.list = bin_list;
    if (rad_vfs_mount_provider("/bin", &ops) == RAD_STATUS_OK) marker("RADIX_ZUBOARD_EMBEDDED_BIN_OK");
}

void write_terminal(const char *text, void *context) {
    (void)context;
    rad_device_t console = nullptr;
    if (rad_device_open("/dev/console", &console) != RAD_STATUS_OK) return;
    size_t written = 0;
    rad_device_write(console, text, strlen(text), &written);
    rad_device_close(console);
}

void prepare_fallback_handoff(rad_boot_handoff_t *handoff) {
    memset(handoff, 0, sizeof(*handoff));
    handoff->magic = RAD_BOOT_HANDOFF_MAGIC;
    handoff->version = RAD_BOOT_HANDOFF_VERSION;
    handoff->size = sizeof(*handoff);
    handoff->flags = RAD_BOOT_HANDOFF_FLAG_SECONDARIES_PARKED
        | RAD_BOOT_HANDOFF_FLAG_INTERRUPTS_MASKED;
    handoff->kernel_image_base = KernelBase;
    handoff->kernel_image_size = reinterpret_cast<uintptr_t>(&__image_end) - KernelBase;
    handoff->kernel_entry = reinterpret_cast<uintptr_t>(&radix_zuboard_entry);
    handoff->peripheral_base = ZynqmpUart0Base;
    handoff->mailbox_base = ZynqmpGicCpuBase;
    handoff->local_interrupt_base = ZynqmpGicDistributorBase;
    handoff->arm_memory_base = ZuboardDdrBase;
    handoff->arm_memory_size = ZuboardDdrSize;
    handoff->entry_el = 1u;
    // Real ZuBoard hardware has 2 A53 cores. Under QEMU the actual -smp count is
    // passed by run-qemu.sh in a scratch word (magic 'RSMP' + count at 0x90000);
    // honor it so single-core QEMU does not attempt PSCI CPU_ON for an absent
    // core (which hangs). Absent/invalid magic (hardware) keeps the default.
    handoff->core_count = 2u;
    {
        const volatile uint32_t *scratch = reinterpret_cast<const volatile uint32_t *>(0x00090000u);
        if (scratch[0] == 0x52534d50u) {
            const uint32_t qemu_smp = scratch[1];
            if (qemu_smp >= 1u && qemu_smp <= 4u) handoff->core_count = qemu_smp;
        }
    }
    handoff->parked_core_mask = handoff->core_count > 1u ? 0x02u : 0x00u;
    strcpy(handoff->payload_name, "radix-zuboard.elf");
    radboot_prepare_default(&handoff->boot, "zynqmp_zuboard_1cg", "zuboard-1cg", "/dev/console", "none");
    radboot_add_memory_region(&handoff->boot, "ddr", ZuboardDdrBase, ZuboardDdrSize, RAD_BOOT_MEMORY_USABLE, 0);
    radboot_add_memory_region(&handoff->boot, "zynqmp-uart0", ZynqmpUart0Base, 0x1000u, RAD_BOOT_MEMORY_MMIO, 0);
    radboot_add_memory_region(&handoff->boot, "zynqmp-gic", ZynqmpGicDistributorBase, 0x20000u, RAD_BOOT_MEMORY_MMIO, 0);
    radboot_add_arg(&handoff->boot, "bootloader", "u-boot");
    radboot_add_arg(&handoff->boot, "handoff", "radix-zuboard-v1");
}
}

extern "C" void radix_zuboard_entry(rad_boot_handoff_t *incoming_handoff) {
    rad_boot_handoff_t fallback{};
    rad_boot_handoff_t *handoff = incoming_handoff;
    if (!handoff || handoff->magic != RAD_BOOT_HANDOFF_MAGIC || handoff->size < sizeof(rad_boot_handoff_t)) {
        prepare_fallback_handoff(&fallback);
        handoff = &fallback;
    }

    rad_zynqmp_bind_handoff(handoff);

    rad_kernel_config_t config{};
    config.backend_name = "zynqmp_zuboard_1cg";
    config.boot_info = &handoff->boot;
    rad_kernel_init(&config);
    rad_a53_note_boot_normalized(0u, static_cast<uintptr_t>(radix_zuboard_boot_argument), 1u);
    rad_a53_platform_init();
    if (rad_zynqmp_preempt_init() != RAD_STATUS_OK) marker("RADIX_ZUBOARD_TIMER_IRQ_FAIL");
    // Now that the GIC + CPU interrupts are live, bring up interrupt-driven GEM RX
    // and validate it (loopback frame -> RX-complete IRQ through GIC SPI 57).
    if (rad_zynqmp_gem_rx_irq_selftest() != RAD_STATUS_OK) marker("RADIX_GEM_RX_IRQ_FAIL");
    const int smp_online = rad_zynqmp_smp_release() == RAD_STATUS_OK;

    marker("RADIX_ZYNQMP_ENTRY_OK");
    marker("RADIX_ZUBOARD_HANDOFF_OK");
    marker("RADIX_ZUBOARD_A53_CORE0_OK");
    if (handoff->parked_core_mask & 0x02u) marker("RADIX_ZUBOARD_SECONDARY_A53_PARKED_OK");

    rad_terminal_execute("bootinfo", write_terminal, nullptr);
    rad_terminal_execute("cores", write_terminal, nullptr);
    rad_terminal_execute("devices", write_terminal, nullptr);

    rad_a53_process_arch_init();
    int rootfs_boot = 0;
    if (x86_ext4_mount_root("/dev/mmcblk0p2") == RAD_STATUS_OK) {
        marker("RADIX_ZUBOARD_EXT4_ROOT_OK");
        if (x86_ext4_self_test()) marker("RADIX_ZUBOARD_ROOTFS_OK");
        int32_t pid = 0;
        rad_task_t task = nullptr;
        // The user session owns /dev/tty0 from here: stop the kernel debug REPL
        // from draining typed input so login/rash can read it. The embedded
        // fallback path below keeps the REPL for debug access.
        rad_terminal_repl_set(0);
        const rad_status_t init_status = rad_a53_user_spawn_process_with_stdio("/bin/init", 0, "/dev/tty0", &pid, &task);
        if (init_status == RAD_STATUS_OK) {
            marker("RADIX_AARCH64_USERLAND_OK");
            marker("RADIX_LOGIN_SPAWN_OK");
            marker("RADIX_ZUBOARD_SERIAL_LOGIN_READY");
            rootfs_boot = 1;
        } else {
            rad_terminal_repl_set(1);
            marker("RADIX_ZUBOARD_INIT_SPAWN_FAIL");
        }
    } else {
        marker("RADIX_ZUBOARD_EXT4_ROOT_FAIL");
        mount_embedded_bin();
    }
    if (!rootfs_boot) {
        if (rad_a53_vm_self_test()) marker("RADIX_ZUBOARD_A53_VM_OK");
        if (rad_a53_process_self_test() == RAD_STATUS_OK) marker("RADIX_ZUBOARD_A53_PROCESS_PARITY_OK");
    }
    marker("RADIX_ZUBOARD_SERIAL_READY");

    // Preemption witness: a time-based busy task long enough to span several
    // 4 ms timer periods; if scheduler ticks advanced while it ran, the
    // IRQ->tick->yield_from_irq path is live.
    {
        rad_scheduler_info_t before{};
        rad_scheduler_info_get(&before);
        rad_task_t witness = nullptr;
        auto busy = [](void *) {
            const uint64_t until = rad_hal_time_micros() + 20000u;
            while (rad_hal_time_micros() < until) {
            }
        };
        if (rad_task_create(&witness, "preempt-witness", busy, nullptr, 0) == RAD_STATUS_OK) {
            rad_task_join(witness);
            rad_scheduler_info_t after{};
            rad_scheduler_info_get(&after);
            if (after.preemption_enabled && after.scheduler_ticks > before.scheduler_ticks) {
                marker("RADIX_ZUBOARD_PREEMPT_TICK_OK");
            }
        }
    }

    // AP scheduler stress (mirrors the x86 self-test): a CORE_ANY task must be
    // dispatched to completion on the worker core. The worker mask is
    // deterministic; the task-completion witness uses a flag + bounded poll
    // rather than rad_task_join, because under single-thread TCG the task is
    // preempted every tick and join's finished-propagation races the poll loop.
    if (smp_online) {
        rad_scheduler_info_t info{};
        rad_scheduler_info_get(&info);
        if (info.worker_running_mask & 0x2u) marker("RADIX_AP_WORKERS_ONLINE_OK");

        static volatile int ap_stress_done = 0;
        rad_task_t stress = nullptr;
        rad_task_config_t stress_config{};
        stress_config.size = sizeof(stress_config);
        stress_config.name = "ap-sched-stress";
        stress_config.target_core = RAD_TASK_CORE_ANY;
        auto busy = [](void *) {
            // Span at least two 4 ms tick periods so a timer IRQ is guaranteed
            // to land while this task runs on core 1 (RADIX_AP_PREEMPT_SCHED_OK
            // is gate-required; a shorter task makes preemption a coin flip).
            const uint64_t until = rad_hal_time_micros() + 10000u;
            while (rad_hal_time_micros() < until) {
            }
            __atomic_store_n(&ap_stress_done, 1, __ATOMIC_RELEASE);
        };
        if (rad_task_create_config(&stress, &stress_config, busy, nullptr) == RAD_STATUS_OK) {
            rad_task_detach(stress);
            // Best-effort completion witness. Core 1 dispatch and preemption are
            // already proven by RADIX_AP_WORKER_TASK_OK / RADIX_AP_PREEMPT_SCHED_OK
            // (runtime-emitted, reliable); run-to-completion under single-thread
            // TCG is timing-sensitive, so this marker is not part of the SMP gate.
            // Keep the poll short so core 0 hands the round-robin back to core 1.
            const uint64_t deadline = rad_hal_time_micros() + 500000u;
            while (!__atomic_load_n(&ap_stress_done, __ATOMIC_ACQUIRE)
                && rad_hal_time_micros() < deadline) {
                rad_hal_worker_wake();
            }
            if (__atomic_load_n(&ap_stress_done, __ATOMIC_ACQUIRE)) marker("RADIX_AP_SCHED_STRESS_OK");
        }
    }

    while (!rad_kernel_is_shutdown_requested()) {
        rad_kernel_poll();
        rad_sleep_ms(10);
    }

    rad_kernel_shutdown();
    for (;;) asm volatile("wfe");
}
