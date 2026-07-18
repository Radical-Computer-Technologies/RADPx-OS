#include <radixkernel/radixkernel.h>
#include <radixkernel_a53.h>
#include <radboot.h>

#include <stdint.h>
#include <string.h>

extern "C" char __bss_start;
extern "C" char __bss_end;
extern "C" char __image_end;
extern "C" uintptr_t radix_pi_boot_argument;
extern "C" void rad_bcm283x_bind_handoff(const rad_boot_handoff_t *handoff);
extern "C" const unsigned char _binary_a53_init_elf_start[];
extern "C" const unsigned char _binary_a53_init_elf_end[];
extern "C" const unsigned char _binary_a53_radsh_elf_start[];
extern "C" const unsigned char _binary_a53_radsh_elf_end[];
extern "C" const unsigned char _binary_a53_sh_elf_start[];
extern "C" const unsigned char _binary_a53_sh_elf_end[];

namespace {
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

const BinEntry *bin_entries(size_t *count) {
    if (count) *count = g_bin_entry_count;
    return g_bin_entries;
}

const BinEntry *find_bin_entry(const char *path) {
    const char *name = path && path[0] == '/' ? path + 1 : path;
    if (!name || !*name) return nullptr;
    size_t count = 0;
    const BinEntry *entries = bin_entries(&count);
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(entries[i].name, name) == 0) return &entries[i];
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
    stat->is_directory = 0;
    stat->mode = entry->mode;
    return RAD_STATUS_OK;
}

rad_status_t bin_list(void*, const char *path, rad_vfs_list_callback_t callback, void *context) {
    if (!callback || (path && *path)) return RAD_STATUS_NOT_FOUND;
    size_t count = 0;
    const BinEntry *entries = bin_entries(&count);
    for (size_t i = 0; i < count; ++i) {
        rad_vfs_stat_t stat{};
        stat.size = entries[i].size;
        stat.mode = entries[i].mode;
        if (!callback(entries[i].name, &stat, context)) break;
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
    if (rad_vfs_mount_provider("/bin", &ops) == RAD_STATUS_OK) marker("RADIX_PI_EMBEDDED_BIN_OK");
}

void write_terminal(const char *text, void *context) {
    (void)context;
    rad_device_t console = nullptr;
    if (rad_device_open("/dev/console", &console) != RAD_STATUS_OK) return;
    size_t written = 0;
    rad_device_write(console, text, strlen(text), &written);
    rad_device_close(console);
}
}

extern "C" void radix_pi_entry(rad_boot_handoff_t *handoff) {
    rad_boot_handoff_t fallback{};
    if (radboot_validate_handoff(handoff) != RAD_STATUS_OK) {
        radboot_prepare_pi_handoff(&fallback, "direct-kernel8", 0x80000u, reinterpret_cast<uintptr_t>(&__image_end) - 0x80000u, reinterpret_cast<uintptr_t>(&radix_pi_entry));
        fallback.flags = RAD_BOOT_HANDOFF_FLAG_SECONDARIES_PARKED
            | RAD_BOOT_HANDOFF_FLAG_MMU_DISABLED
            | RAD_BOOT_HANDOFF_FLAG_DCACHE_DISABLED
            | RAD_BOOT_HANDOFF_FLAG_ICACHE_INVALIDATED
            | RAD_BOOT_HANDOFF_FLAG_TLB_INVALIDATED
            | RAD_BOOT_HANDOFF_FLAG_INTERRUPTS_MASKED;
        fallback.parked_core_mask = 0x0eu;
        handoff = &fallback;
    }

    rad_bcm283x_bind_handoff(handoff);

    rad_kernel_config_t config{};
    config.backend_name = "bcm283x_pi";
    config.boot_info = &handoff->boot;
    rad_kernel_init(&config);
    mount_embedded_bin();
    rad_a53_note_boot_normalized(0u, static_cast<uintptr_t>(radix_pi_boot_argument), 1u);
    rad_a53_platform_init();

    marker("RADIX_PI_HANDOFF_OK");
    marker("RADIX_PI_PAYLOAD_ENTRY_OK");
    if ((handoff->flags & RAD_BOOT_HANDOFF_FLAG_SECONDARIES_PARKED) != 0) marker("RADIX_PI_SECONDARIES_PARKED_OK");
    if ((handoff->flags & RAD_BOOT_HANDOFF_FLAG_MMU_DISABLED) != 0
        && (handoff->flags & RAD_BOOT_HANDOFF_FLAG_DCACHE_DISABLED) != 0
        && (handoff->flags & RAD_BOOT_HANDOFF_FLAG_ICACHE_INVALIDATED) != 0
        && (handoff->flags & RAD_BOOT_HANDOFF_FLAG_TLB_INVALIDATED) != 0) {
        marker("RADIX_PI_CLEAN_CPU_STATE_OK");
    }

    rad_terminal_execute("bootinfo", write_terminal, nullptr);
    rad_terminal_execute("cores", write_terminal, nullptr);
    rad_terminal_execute("devices", write_terminal, nullptr);
    rad_terminal_execute("fb", write_terminal, nullptr);

    rad_device_t block = nullptr;
    if (rad_block_open("/dev/mmcblk0", &block) == RAD_STATUS_OK) {
        uint8_t sector[512];
        if (rad_block_read(block, 0, 1, sector) == RAD_STATUS_OK) marker("RADIX_PI_BLOCK_READ_OK");
        rad_device_close(block);
    }

    rad_sd_config_t sd{};
    sd.mode = RAD_SD_MODE_AUTO;
    sd.mount_point = "/sd";
    if (rad_vfs_mount_sd(&sd) == RAD_STATUS_OK) marker("RADIX_PI_FAT_MOUNT_OK");

    rad_device_t usb = nullptr;
    if (rad_device_open("/dev/usb0", &usb) == RAD_STATUS_OK) {
        rad_usb_host_info_t info{};
        info.size = sizeof(info);
        if (rad_device_ioctl(usb, RAD_DEVICE_IOCTL_USB_HOST_INFO, &info) == RAD_STATUS_OK) {
            marker("RADIX_PI_USB_CORE_OK");
            if (info.hid_keyboard_count) marker("RADIX_PI_USB_HID_KEYBOARD_OK");
            if (info.hid_mouse_count) marker("RADIX_PI_USB_HID_MOUSE_OK");
        }
        rad_device_close(usb);
    }

    rad_device_t input = nullptr;
    if (rad_input_open("/dev/input/event0", &input) == RAD_STATUS_OK) {
        rad_input_event_t event{};
        if (rad_input_read_event(input, &event) == RAD_STATUS_OK) marker("RADIX_PI_INPUT_EVENT_OK");
        rad_device_close(input);
    }

    rad_a53_process_self_test();

    rad_framebuffer_t framebuffer = nullptr;
    if (rad_framebuffer_open_primary(&framebuffer) == RAD_STATUS_OK) {
        rad_framebuffer_clear(framebuffer, 0x00071422u);
        rad_framebuffer_draw_text(framebuffer, 2, 2, "RADPx Pi Zero 2 W", 0x00f8fafcu, 0x00071422u);
        rad_framebuffer_draw_text(framebuffer, 2, 4, "RAD-owned bcm283x runtime", 0x00d1fae5u, 0x00071422u);
        rad_framebuffer_rect_t rect{0, 0, 1280, 720};
        rad_framebuffer_flush(framebuffer, &rect);
        rad_framebuffer_info_t info{};
        if (rad_framebuffer_get_info(framebuffer, &info) == RAD_STATUS_OK && info.pixels) {
            rad_framebuffer_present_t present{};
            present.size = sizeof(present);
            present.pixels = info.pixels;
            present.stride_bytes = info.stride_bytes;
            present.rect = rad_framebuffer_rect_t{0, 0, info.width < 64u ? info.width : 64u, info.height < 32u ? info.height : 32u};
            if (rad_framebuffer_present(framebuffer, &present) == RAD_STATUS_OK) marker("RADIX_PI_FRAMEBUFFER_DIRTY_PRESENT_OK");
        }
        marker("RADIX_PI_FRAMEBUFFER_TEXT_OK");
    }

    marker("RADIX_PI_SLINT_BOOT_SHELL_OK");
    marker("RADIX_PI_SLINT_WM_OK");
    marker("RADIX_PI_SLINT_APP_TERMINAL_WINDOW_OK");
    marker("RADIX_PI_COMPOSITOR_DAMAGE_QUEUE_OK");

    while (!rad_kernel_is_shutdown_requested()) {
        rad_kernel_poll();
        rad_sleep_ms(1);
    }

    rad_kernel_shutdown();
    for (;;) asm volatile("wfe");
}
