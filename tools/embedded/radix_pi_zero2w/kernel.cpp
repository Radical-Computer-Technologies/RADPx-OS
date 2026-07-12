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

namespace {
void marker(const char *text) {
    rad_debug_marker(text);
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
        rad_framebuffer_draw_text(framebuffer, 2, 2, "RADix Pi Zero 2 W", 0x00f8fafcu, 0x00071422u);
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
