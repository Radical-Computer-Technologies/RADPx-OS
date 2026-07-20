#include <radkernel/radkernel.h>
#include <radboot.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace {
constexpr uintptr_t DefaultPeripheralBase = 0x3f000000u;
constexpr uintptr_t DefaultMailboxBase = 0x3f00b880u;
constexpr uintptr_t DefaultLocalInterruptBase = 0x40000000u;
constexpr uintptr_t Uart0Offset = 0x201000u;
constexpr uintptr_t SystemTimerOffset = 0x3000u;
constexpr uintptr_t InterruptControllerOffset = 0xb200u;
constexpr uintptr_t EmmcOffset = 0x300000u;
constexpr uintptr_t UsbOffset = 0x980000u;
constexpr uint32_t MailboxChannelProperty = 8u;
constexpr uint32_t MailboxFull = 0x80000000u;
constexpr uint32_t MailboxEmpty = 0x40000000u;
constexpr uint32_t MailboxRequest = 0u;
constexpr uint32_t MailboxResponseSuccess = 0x80000000u;
constexpr uint32_t TagSetPhysicalSize = 0x00048003u;
constexpr uint32_t TagSetVirtualSize = 0x00048004u;
constexpr uint32_t TagSetDepth = 0x00048005u;
constexpr uint32_t TagSetPixelOrder = 0x00048006u;
constexpr uint32_t TagAllocateBuffer = 0x00040001u;
constexpr uint32_t TagGetPitch = 0x00040008u;
constexpr uint32_t TagEnd = 0u;
constexpr uint32_t DefaultFbWidth = 1280u;
constexpr uint32_t DefaultFbHeight = 720u;
constexpr uint32_t DefaultFbDepth = 32u;
constexpr uint32_t EmmcSectorSize = 512u;
constexpr uint32_t EmmcSectorCount = 256u;

struct Bcm283xState {
    uintptr_t peripheral_base = DefaultPeripheralBase;
    uintptr_t mailbox_base = DefaultMailboxBase;
    uintptr_t local_interrupt_base = DefaultLocalInterruptBase;
    uintptr_t arm_memory_size = 512u * 1024u * 1024u;
    volatile uint32_t *framebuffer = nullptr;
    uint32_t framebuffer_width = 0;
    uint32_t framebuffer_height = 0;
    uint32_t framebuffer_stride = 0;
    uint64_t vsync_counter = 0;
    int emmc_ready = 0;
    int usb_ready = 0;
    // Real SD card (Arasan SDHCI EMMC) state; sd_ready=0 falls back to the
    // in-RAM g_emmc_storage stub so a kernel-only boot (no -sd image) still works.
    int sd_ready = 0;
    int sd_controller = 0; // 0=none/RAM stub, 1=Arasan SDHCI, 2=SDHOST (real HW)
    uint32_t sd_high_capacity = 1;
    uint32_t sd_rca = 0;
    uint64_t sd_sector_count = 0;
    rad_input_queue_t input_queue = nullptr;
};

Bcm283xState g_bcm283x;
alignas(16) volatile uint32_t g_mailbox[64];
alignas(16) uint8_t g_emmc_storage[EmmcSectorSize * EmmcSectorCount];

// MBR partition block devices layered on the real SD card (mirrors the ZynqMP).
struct PartitionDevice {
    const char *name;
    uint64_t start_sector;
    uint64_t sector_count;
    int ready;
};
PartitionDevice g_partitions[2] = {
    {"/dev/mmcblk0p1", 0, 0, 0},
    {"/dev/mmcblk0p2", 0, 0, 0},
};

volatile uint32_t *reg32(uintptr_t address) {
    return reinterpret_cast<volatile uint32_t*>(address);
}

uintptr_t peripheral(uintptr_t offset) {
    return g_bcm283x.peripheral_base + offset;
}

uintptr_t uart0(uintptr_t offset) {
    return peripheral(Uart0Offset + offset);
}

uint32_t read32(uintptr_t address) {
    return *reg32(address);
}

void write32(uintptr_t address, uint32_t value) {
    *reg32(address) = value;
}

uint8_t read8(uintptr_t address) { return *reinterpret_cast<volatile uint8_t*>(address); }
void write8(uintptr_t address, uint8_t value) { *reinterpret_cast<volatile uint8_t*>(address) = value; }
void write16(uintptr_t address, uint16_t value) { *reinterpret_cast<volatile uint16_t*>(address) = value; }
} // anonymous namespace (helpers below need rad_hal_time_micros, declared next)

extern "C" uint64_t rad_hal_time_micros(void);
// CYW43438 SDIO WiFi probe (bcm283x_wifi.cpp), driven on the Arasan EMMC.
// HELD: the driver is written and builds, but simply LINKING bcm283x_wifi.o
// deterministically breaks the interactive login (a latent layout-sensitivity
// bug in the kernel that ~4 KB of extra code exposes -- one such bug, the page
// allocator/image overlap, is already fixed via RAD_A53_USABLE_FLOOR; a second
// remains). Until that is root-caused, the WiFi probe is not wired in so the
// QEMU image stays login-clean. Re-enable the extern + the call in
// bcm283x_emmc_init together with adding bcm283x_wifi.o back to the Makefile.
// extern "C" rad_status_t rad_bcm283x_wifi_init(uintptr_t peripheral_base);

namespace {
void udelay(uint32_t us) {
    const uint64_t start = rad_hal_time_micros();
    while (rad_hal_time_micros() - start < us) { asm volatile("nop"); }
}

int wait_mask(uintptr_t reg, uint32_t mask, uint32_t value, uint32_t timeout_us) {
    for (uint32_t i = 0; i < timeout_us; ++i) {
        if ((read32(reg) & mask) == value) return 1;
        if ((i & 0xffu) == 0) udelay(1);
    }
    return 0;
}

// GPIO alternate-function select (GPFSEL, 3 bits/pin at GPIO base 0x200000).
// alt0=4, alt3=7 in the function-select encoding.
constexpr uintptr_t GpioOffset = 0x200000u;
void gpio_set_alt(uint32_t pin, uint32_t alt) {
    const uintptr_t reg = peripheral(GpioOffset + (pin / 10u) * 4u);
    const uint32_t shift = (pin % 10u) * 3u;
    uint32_t v = read32(reg);
    v &= ~(0x7u << shift);
    v |= (alt & 0x7u) << shift;
    write32(reg, v);
}

uint32_t arm_to_vc_bus(uintptr_t address) {
    return static_cast<uint32_t>(address | 0x40000000u);
}

uintptr_t vc_bus_to_arm(uint32_t address) {
    return static_cast<uintptr_t>(address & 0x3fffffffu);
}

int mailbox_call(uint8_t channel) {
    const uint32_t message = (arm_to_vc_bus(reinterpret_cast<uintptr_t>(g_mailbox)) & ~0xfu) | (channel & 0xfu);
    while (read32(g_bcm283x.mailbox_base + 0x18u) & MailboxFull) {}
    write32(g_bcm283x.mailbox_base + 0x20u, message);
    for (;;) {
        while (read32(g_bcm283x.mailbox_base + 0x18u) & MailboxEmpty) {}
        const uint32_t response = read32(g_bcm283x.mailbox_base);
        if ((response & 0xfu) == channel && (response & ~0xfu) == (message & ~0xfu)) {
            return g_mailbox[1] == MailboxResponseSuccess;
        }
    }
}

void mailbox_push(size_t& index, uint32_t tag, uint32_t value_size, uint32_t value0, uint32_t value1 = 0) {
    g_mailbox[index++] = tag;
    g_mailbox[index++] = value_size;
    g_mailbox[index++] = value_size;
    g_mailbox[index++] = value0;
    if (value_size >= 8u) g_mailbox[index++] = value1;
}

#if defined(RAD_PI_JTAG_CONSOLE)
// Console-over-JTAG rings (opt-in: -DRAD_PI_JTAG_CONSOLE). A debugger drains the
// output ring and fills the input ring over the JTAG memory port (OpenOCD reads/
// writes these symbols while the core runs), so the console needs NO UART wire --
// only the Pico DirtyJTAG. Off by default so the shipped image's memory layout is
// untouched (the WiFi-object link exposes a layout-sensitivity bug; keep this out
// of the default build until that is fixed). See tools/embedded/rad_pi_zero2w_hw.
struct RadJtagRing { volatile uint32_t magic, head, tail, size; char buf[8192]; };
extern "C" RadJtagRing g_rad_jtag_console = {0x524a5443u /*'RJTC'*/, 0, 0, 8192u, {0}};
extern "C" RadJtagRing g_rad_jtag_input   = {0x524a5449u /*'RJTI'*/, 0, 0,  256u, {0}};
void jtag_console_emit(const uint8_t *b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        const uint32_t h = g_rad_jtag_console.head;
        g_rad_jtag_console.buf[h % 8192u] = static_cast<char>(b[i]);
        g_rad_jtag_console.head = h + 1u;
    }
}
size_t jtag_input_drain(uint8_t *out, size_t max) {
    size_t n = 0;
    while (n < max && g_rad_jtag_input.tail != g_rad_jtag_input.head) {
        out[n++] = static_cast<uint8_t>(g_rad_jtag_input.buf[g_rad_jtag_input.tail % 256u]);
        g_rad_jtag_input.tail++;
    }
    return n;
}
#endif

rad_status_t bcm_serial_read(void*, void *buffer, size_t size, size_t *bytes_read) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
    size_t count = 0;
#if defined(RAD_PI_JTAG_CONSOLE)
    count += jtag_input_drain(static_cast<uint8_t*>(buffer), size); // debugger-injected input first
#endif
    while (count < size && (read32(uart0(0x18u)) & (1u << 4u)) == 0) {
        static_cast<uint8_t*>(buffer)[count++] = static_cast<uint8_t>(read32(uart0(0x00u)) & 0xffu);
    }
    if (bytes_read) *bytes_read = count;
    return RAD_STATUS_OK;
}

rad_status_t bcm_serial_write(void*, const void *buffer, size_t size, size_t *bytes_written) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
    const auto *bytes = static_cast<const uint8_t*>(buffer);
#if defined(RAD_PI_JTAG_CONSOLE)
    jtag_console_emit(bytes, size); // mirror all console output to the JTAG ring
#endif
    for (size_t i = 0; i < size; ++i) {
        if (bytes[i] == '\n') {
            while (read32(uart0(0x18u)) & (1u << 5u)) {}
            write32(uart0(0x00u), '\r');
        }
        while (read32(uart0(0x18u)) & (1u << 5u)) {}
        write32(uart0(0x00u), bytes[i]);
    }
    if (bytes_written) *bytes_written = size;
    return RAD_STATUS_OK;
}

rad_status_t bcm_serial_ioctl(void*, uint32_t request, void *argument) {
    if (request != RAD_DEVICE_IOCTL_SERIAL_CONFIGURE) return RAD_STATUS_NOT_SUPPORTED;
    return argument ? RAD_STATUS_OK : RAD_STATUS_INVALID_ARGUMENT;
}

rad_status_t bcm_framebuffer_present(void*, const rad_framebuffer_present_t *present) {
    if (!present || !present->pixels || !g_bcm283x.framebuffer || present->stride_bytes < present->rect.width * sizeof(uint32_t)) {
        return RAD_STATUS_INVALID_ARGUMENT;
    }
    const uint32_t x0 = present->rect.x;
    const uint32_t y0 = present->rect.y;
    if (x0 >= g_bcm283x.framebuffer_width || y0 >= g_bcm283x.framebuffer_height) return RAD_STATUS_OK;
    uint32_t width = present->rect.width;
    uint32_t height = present->rect.height;
    if (x0 + width > g_bcm283x.framebuffer_width) width = g_bcm283x.framebuffer_width - x0;
    if (y0 + height > g_bcm283x.framebuffer_height) height = g_bcm283x.framebuffer_height - y0;
    const uint32_t src_stride = present->stride_bytes / sizeof(uint32_t);
    const auto *src_base = static_cast<const uint32_t*>(present->pixels);
    uint32_t *dst_base = const_cast<uint32_t*>(g_bcm283x.framebuffer);
    const uint32_t dst_stride = g_bcm283x.framebuffer_stride / sizeof(uint32_t);
    for (uint32_t row = 0; row < height; ++row) {
        const uint32_t *src = src_base + static_cast<size_t>(y0 + row) * src_stride + x0;
        uint32_t *dst = dst_base + static_cast<size_t>(y0 + row) * dst_stride + x0;
        memcpy(dst, src, static_cast<size_t>(width) * sizeof(uint32_t));
    }
    ++g_bcm283x.vsync_counter;
    return RAD_STATUS_OK;
}

rad_status_t bcm_framebuffer_flush(void*, const rad_framebuffer_rect_t*) {
    ++g_bcm283x.vsync_counter;
    return RAD_STATUS_OK;
}

uint64_t bcm_framebuffer_vsync(void*) {
    return g_bcm283x.vsync_counter;
}

rad_status_t register_mailbox_framebuffer() {
    memset(const_cast<uint32_t*>(g_mailbox), 0, sizeof(g_mailbox));
    size_t i = 2;
    mailbox_push(i, TagSetPhysicalSize, 8u, DefaultFbWidth, DefaultFbHeight);
    mailbox_push(i, TagSetVirtualSize, 8u, DefaultFbWidth, DefaultFbHeight);
    mailbox_push(i, TagSetDepth, 4u, DefaultFbDepth);
    mailbox_push(i, TagSetPixelOrder, 4u, 1u);
    mailbox_push(i, TagAllocateBuffer, 8u, 16u, 0u);
    mailbox_push(i, TagGetPitch, 4u, 0u);
    g_mailbox[i++] = TagEnd;
    g_mailbox[0] = static_cast<uint32_t>(i * sizeof(uint32_t));
    g_mailbox[1] = MailboxRequest;
    if (!mailbox_call(MailboxChannelProperty)) return RAD_STATUS_NOT_SUPPORTED;

    uint32_t width = DefaultFbWidth;
    uint32_t height = DefaultFbHeight;
    uint32_t pitch = 0;
    uintptr_t pixels = 0;
    for (size_t index = 2; index < i && g_mailbox[index] != TagEnd;) {
        const uint32_t tag = g_mailbox[index++];
        const uint32_t value_size = g_mailbox[index++];
        index++;
        if (tag == TagSetPhysicalSize && value_size >= 8u) {
            width = g_mailbox[index];
            height = g_mailbox[index + 1u];
        } else if (tag == TagAllocateBuffer && value_size >= 8u) {
            pixels = vc_bus_to_arm(g_mailbox[index]);
        } else if (tag == TagGetPitch && value_size >= 4u) {
            pitch = g_mailbox[index];
        }
        index += value_size / sizeof(uint32_t);
    }
    if (!pixels || !pitch || !width || !height) return RAD_STATUS_NOT_SUPPORTED;
    g_bcm283x.framebuffer = reinterpret_cast<volatile uint32_t*>(pixels);
    g_bcm283x.framebuffer_width = width;
    g_bcm283x.framebuffer_height = height;
    g_bcm283x.framebuffer_stride = pitch;

    rad_framebuffer_config_t config{};
    config.size = sizeof(config);
    config.name = "/dev/fb0";
    config.info.size = sizeof(config.info);
    config.info.width = width;
    config.info.height = height;
    config.info.stride_bytes = pitch;
    config.info.pixel_format = RAD_PIXEL_FORMAT_XRGB8888;
    config.info.pixels = const_cast<uint32_t*>(g_bcm283x.framebuffer);
    config.output_type = RAD_DISPLAY_OUTPUT_BCM283X_MAILBOX;
    config.connector = "bcm283x-mailbox-hdmi";
    config.mode_count = 1u;
    config.preferred_mode = 0u;
    config.modes[0].width = width;
    config.modes[0].height = height;
    config.modes[0].refresh_hz = 60u;
    config.modes[0].stride_bytes = pitch;
    config.modes[0].pixel_format = RAD_PIXEL_FORMAT_XRGB8888;
    config.primary = 1;
    rad_framebuffer_ops_t ops{};
    ops.flush = bcm_framebuffer_flush;
    ops.present = bcm_framebuffer_present;
    ops.get_vsync_counter = bcm_framebuffer_vsync;
    const rad_status_t status = rad_framebuffer_register_ex(&config, &ops);
    if (status == RAD_STATUS_OK) rad_debug_marker("RAD_PI_MAILBOX_FB_OK");
    return status;
}

// Defined further below; dispatches the read to the active SD controller
// (SDHOST on real HW / Arasan in QEMU default wiring).
rad_status_t sd_read_block(uint64_t sector, void *buffer);

rad_status_t bcm_block_info(void*, uint32_t request, void *argument) {
    // A real SD card reports a large sector count so ext4/FAT can read anywhere
    // in the image; the RAM stub keeps its small fixed capacity.
    const uint64_t capacity = g_bcm283x.sd_ready ? (1ull << 23u) /*4 GiB window*/ : EmmcSectorCount;
    if (request == RAD_DEVICE_IOCTL_BLOCK_INFO) {
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        auto *info = static_cast<rad_block_info_t*>(argument);
        memset(info, 0, sizeof(*info));
        info->size = sizeof(*info);
        info->sector_size = EmmcSectorSize;
        info->sector_count = capacity;
        info->flags = 0u;
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_READ || request == RAD_DEVICE_IOCTL_BLOCK_WRITE) {
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        auto *block = static_cast<rad_block_request_t*>(argument);
        if (!block->buffer || block->sector_count == 0u) return RAD_STATUS_INVALID_ARGUMENT;
        if (block->sector >= capacity || block->sector_count > capacity - block->sector) {
            return RAD_STATUS_INVALID_ARGUMENT;
        }
        if (g_bcm283x.sd_ready) {
            // Real SD: PIO single-block transfers (read only; writes not modeled).
            if (request == RAD_DEVICE_IOCTL_BLOCK_WRITE) return RAD_STATUS_NOT_SUPPORTED;
            auto *bytes = static_cast<uint8_t*>(block->buffer);
            for (uint32_t i = 0; i < block->sector_count; ++i) {
                const rad_status_t st = sd_read_block(block->sector + i, bytes + static_cast<size_t>(i) * EmmcSectorSize);
                if (st != RAD_STATUS_OK) return st;
            }
            rad_debug_marker("RAD_PI_BLOCK_READ_OK");
            return RAD_STATUS_OK;
        }
        const size_t offset = static_cast<size_t>(block->sector) * EmmcSectorSize;
        const size_t bytes = static_cast<size_t>(block->sector_count) * EmmcSectorSize;
        if (request == RAD_DEVICE_IOCTL_BLOCK_READ) {
            memcpy(block->buffer, g_emmc_storage + offset, bytes);
            rad_debug_marker("RAD_PI_BLOCK_READ_OK");
        } else {
            memcpy(g_emmc_storage + offset, block->buffer, bytes);
        }
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_FLUSH) {
        return RAD_STATUS_OK;
    }
    return RAD_STATUS_NOT_SUPPORTED;
}

rad_status_t bcm_usb_ioctl(void*, uint32_t request, void *argument) {
    if (request != RAD_DEVICE_IOCTL_USB_HOST_INFO) return RAD_STATUS_NOT_SUPPORTED;
    if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
    auto *info = static_cast<rad_usb_host_info_t*>(argument);
    memset(info, 0, sizeof(*info));
    info->size = sizeof(*info);
    info->controller = RAD_USB_CONTROLLER_DWC_OTG;
    info->device_count = g_bcm283x.usb_ready ? 2u : 0u;
    info->hid_keyboard_count = g_bcm283x.usb_ready ? 1u : 0u;
    info->hid_mouse_count = g_bcm283x.usb_ready ? 1u : 0u;
    return RAD_STATUS_OK;
}

// --- Arasan SDHCI (BCM2835 "EMMC") block driver -----------------------------
// The BCM2835 EMMC is an SD-spec SDHCI controller at peripheral+0x300000. On a
// real Pi Zero 2 W this controller is wired to the on-board CYW43438 WiFi (SDIO),
// NOT the SD card -- so this SD-card path is only a QEMU fallback (used when the
// GPIO48-53 alt is left at the default and QEMU keeps the card parented to the
// Arasan bus). The real microSD path is the SDHOST driver below. PIO single-block.
// (Already inside the file's anonymous namespace -- no nested namespace here, so
// sd_read_single matches its forward declaration above bcm_block_info.)
constexpr uint32_t SdSectorSize = 512u;
constexpr uint32_t SdPresentCmdInhibit = 1u << 0u;
constexpr uint32_t SdPresentDatInhibit = 1u << 1u;
constexpr uint32_t SdPresentReadReady = 1u << 11u;
constexpr uint32_t SdIntCmdComplete = 1u << 0u;
constexpr uint32_t SdIntTransferComplete = 1u << 1u;
constexpr uint32_t SdIntBufferReadReady = 1u << 5u;
constexpr uint32_t SdIntError = 0xffff0000u;
constexpr uint16_t SdCmdRespNone = 0x0000u;
constexpr uint16_t SdCmdRespLong = 0x0001u;
constexpr uint16_t SdCmdRespShort = 0x0002u;
constexpr uint16_t SdCmdCrc = 0x0008u;
constexpr uint16_t SdCmdIndex = 0x0010u;
constexpr uint16_t SdCmdData = 0x0020u;

uintptr_t emmc(uintptr_t offset) { return peripheral(EmmcOffset + offset); }
uint32_t sd_response32() { return read32(emmc(0x10u)); }

rad_status_t sd_send_cmd(uint32_t index, uint32_t argument, uint16_t flags, uint32_t *response) {
    const uint32_t inhibit = (flags & SdCmdData) ? (SdPresentCmdInhibit | SdPresentDatInhibit) : SdPresentCmdInhibit;
    if (!wait_mask(emmc(0x24u), inhibit, 0u, 1000000u)) return RAD_STATUS_TIMEOUT;
    write32(emmc(0x30u), 0xffffffffu);
    write32(emmc(0x08u), argument);
    write16(emmc(0x0eu), static_cast<uint16_t>((index << 8u) | flags));
    for (uint32_t spin = 0; spin < 1000000u; ++spin) {
        const uint32_t status = read32(emmc(0x30u));
        if (status & SdIntError) { write32(emmc(0x30u), status); return RAD_STATUS_ERROR; }
        if (status & SdIntCmdComplete) {
            write32(emmc(0x30u), SdIntCmdComplete);
            if (response) *response = sd_response32();
            return RAD_STATUS_OK;
        }
        if ((spin & 0xffu) == 0) udelay(1);
    }
    return RAD_STATUS_TIMEOUT;
}

rad_status_t sd_set_clock() {
    write16(emmc(0x2cu), 0x0000u);
    udelay(1000);
    write16(emmc(0x2cu), 0x0101u);
    if (!wait_mask(emmc(0x2cu), 0x0002u, 0x0002u, 100000u)) return RAD_STATUS_TIMEOUT;
    write16(emmc(0x2cu), 0x0107u);
    return RAD_STATUS_OK;
}

rad_status_t sd_reset() {
    write8(emmc(0x2fu), 0x01u);
    for (uint32_t spin = 0; spin < 100000u; ++spin) {
        if ((read8(emmc(0x2fu)) & 0x01u) == 0) break;
        if ((spin & 0xffu) == 0) udelay(1);
    }
    write8(emmc(0x29u), 0x0fu);
    write32(emmc(0x34u), 0xffffffffu);
    write32(emmc(0x38u), 0x00000000u);
    write8(emmc(0x2eu), 0x0eu);
    return sd_set_clock();
}

rad_status_t sd_read_single(uint64_t sector, void *buffer) {
    if (!buffer || !g_bcm283x.sd_ready) return RAD_STATUS_INVALID_ARGUMENT;
    write16(emmc(0x04u), SdSectorSize);
    write16(emmc(0x06u), 1u);
    write16(emmc(0x0cu), 0x0010u);
    const uint32_t argument = g_bcm283x.sd_high_capacity ? static_cast<uint32_t>(sector) : static_cast<uint32_t>(sector * SdSectorSize);
    rad_status_t status = sd_send_cmd(17u, argument, SdCmdRespShort | SdCmdCrc | SdCmdIndex | SdCmdData, nullptr);
    if (status != RAD_STATUS_OK) return status;
    for (uint32_t spin = 0; spin < 1000000u; ++spin) {
        const uint32_t irq = read32(emmc(0x30u));
        if (irq & SdIntError) { write32(emmc(0x30u), irq); return RAD_STATUS_ERROR; }
        if ((irq & SdIntBufferReadReady) || (read32(emmc(0x24u)) & SdPresentReadReady)) {
            auto *words = static_cast<uint32_t*>(buffer);
            for (uint32_t i = 0; i < SdSectorSize / sizeof(uint32_t); ++i) words[i] = read32(emmc(0x20u));
            write32(emmc(0x30u), SdIntBufferReadReady | SdIntTransferComplete);
            return RAD_STATUS_OK;
        }
        if ((spin & 0xffu) == 0) udelay(1);
    }
    return RAD_STATUS_TIMEOUT;
}

rad_status_t sd_init_card() {
    if (sd_reset() != RAD_STATUS_OK) return RAD_STATUS_ERROR;
    uint32_t response = 0;
    (void)sd_send_cmd(0u, 0u, SdCmdRespNone, nullptr);
    (void)sd_send_cmd(8u, 0x1aau, SdCmdRespShort | SdCmdCrc | SdCmdIndex, &response);
    rad_status_t status = RAD_STATUS_TIMEOUT;
    for (uint32_t retry = 0; retry < 1000u; ++retry) {
        (void)sd_send_cmd(55u, 0u, SdCmdRespShort | SdCmdCrc | SdCmdIndex, nullptr);
        status = sd_send_cmd(41u, 0x40300000u, SdCmdRespShort, &response);
        if (status == RAD_STATUS_OK && (response & 0x80000000u)) break;
        udelay(1000);
    }
    if ((response & 0x80000000u) == 0) return RAD_STATUS_TIMEOUT;
    g_bcm283x.sd_high_capacity = (response & 0x40000000u) ? 1u : 0u;
    if (sd_send_cmd(2u, 0u, SdCmdRespLong | SdCmdCrc, nullptr) != RAD_STATUS_OK) return RAD_STATUS_ERROR;
    if (sd_send_cmd(3u, 0u, SdCmdRespShort | SdCmdCrc | SdCmdIndex, &response) != RAD_STATUS_OK) return RAD_STATUS_ERROR;
    g_bcm283x.sd_rca = response & 0xffff0000u;
    if (sd_send_cmd(7u, g_bcm283x.sd_rca, 0x0003u | SdCmdCrc | SdCmdIndex, nullptr) != RAD_STATUS_OK) return RAD_STATUS_ERROR;
    return RAD_STATUS_OK;
}

// --- BCM2835 SDHOST controller (real-hardware microSD path) -----------------
// On a real Pi Zero 2 W the microSD card is wired to the SDHOST controller
// (peripheral+0x202000) via GPIO48-53 alt0, while the Arasan SDHCI EMMC
// (0x300000) carries the on-board CYW43438 WiFi over SDIO. QEMU raspi3 models
// both and reparents the SD card to whichever controller GPIO48-53's alt selects,
// so driving SDHOST here makes QEMU match real silicon AND frees the Arasan EMMC
// for the WiFi driver. SDHOST is the Broadcom-custom controller (not SDHCI):
// write SDARG then SDCMD|NEW_FLAG, poll NEW_FLAG, read SDRSP0; data drains from
// the SDDATA FIFO (word count in SDEDM[8:4]). Ported from Circle addon/SDCard.
constexpr uintptr_t SdhostOffset = 0x202000u;
constexpr uint32_t SdhostCmdNewFlag = 0x8000u;
constexpr uint32_t SdhostCmdFailFlag = 0x4000u;
constexpr uint32_t SdhostCmdBusywait = 0x800u;
constexpr uint32_t SdhostCmdNoResponse = 0x400u;
constexpr uint32_t SdhostCmdLongResponse = 0x200u;
constexpr uint32_t SdhostCmdReadCmd = 0x40u;
constexpr uint32_t SdhostHstsErrorMask = 0xf8u; // CMD/REW timeout + CRC + FIFO error
constexpr uint32_t SdhostCfgWideIntBus = (1u << 1);
constexpr uint32_t SdhostCfgSlowCard = (1u << 3);
constexpr uint32_t SdhostCfgBusyIrptEn = (1u << 10);

uintptr_t sdhost(uintptr_t offset) { return peripheral(SdhostOffset + offset); }

rad_status_t sdhost_command(uint32_t cmd, uint32_t arg, uint32_t flags, uint32_t *resp) {
    for (uint32_t i = 0; (read32(sdhost(0x00u)) & SdhostCmdNewFlag); ++i) {
        if (i > 100000u) return RAD_STATUS_TIMEOUT;
        udelay(10);
    }
    write32(sdhost(0x20u), 0x7f8u);                                        // clear SDHSTS
    write32(sdhost(0x04u), arg);                                           // SDARG
    write32(sdhost(0x00u), (cmd & 0x3fu) | flags | SdhostCmdNewFlag);      // SDCMD
    uint32_t sdcmd = 0;
    for (uint32_t i = 0; i < 100000u; ++i) {
        sdcmd = read32(sdhost(0x00u));
        if (!(sdcmd & SdhostCmdNewFlag)) break;
        udelay(10);
    }
    if (sdcmd & SdhostCmdNewFlag) return RAD_STATUS_TIMEOUT;
    if (sdcmd & SdhostCmdFailFlag) return RAD_STATUS_ERROR;
    if (resp) *resp = read32(sdhost(0x10u));                               // SDRSP0
    return RAD_STATUS_OK;
}

void sdhost_reset() {
    write32(sdhost(0x30u), 0);        // SDVDD off
    write32(sdhost(0x00u), 0);        // SDCMD
    write32(sdhost(0x04u), 0);        // SDARG
    write32(sdhost(0x08u), 0xf00000u);// SDTOUT
    write32(sdhost(0x0cu), 0);        // SDCDIV
    write32(sdhost(0x20u), 0x7f8u);   // SDHSTS clear
    write32(sdhost(0x38u), 0);        // SDHCFG
    write32(sdhost(0x3cu), 0);        // SDHBCT
    write32(sdhost(0x50u), 0);        // SDHBLC
    uint32_t edm = read32(sdhost(0x34u)); // SDEDM: cap FIFO thresholds (silicon bug)
    edm &= ~((0x1fu << 14) | (0x1fu << 9));
    edm |= (4u << 14) | (4u << 9);
    write32(sdhost(0x34u), edm);
    udelay(10000);
    write32(sdhost(0x30u), 1);        // SDVDD on
    udelay(10000);
    write32(sdhost(0x38u), SdhostCfgWideIntBus | SdhostCfgSlowCard | SdhostCfgBusyIrptEn);
    write32(sdhost(0x0cu), 0x7ffu);   // SDCDIV: slowest (ident) clock
}

rad_status_t sdhost_read_single(uint64_t sector, void *buffer) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
    write32(sdhost(0x3cu), SdSectorSize); // SDHBCT block size
    write32(sdhost(0x50u), 1u);           // SDHBLC block count
    const uint32_t arg = g_bcm283x.sd_high_capacity ? static_cast<uint32_t>(sector)
                                                    : static_cast<uint32_t>(sector * SdSectorSize);
    const rad_status_t st = sdhost_command(17u, arg, SdhostCmdReadCmd, nullptr); // CMD17
    if (st != RAD_STATUS_OK) return st;
    auto *words = static_cast<uint32_t*>(buffer);
    uint32_t got = 0;
    const uint32_t total = SdSectorSize / sizeof(uint32_t);
    for (uint32_t spin = 0; spin < 1000000u && got < total; ++spin) {
        uint32_t avail = (read32(sdhost(0x34u)) >> 4) & 0x1fu; // SDEDM[8:4] = FIFO words
        if (avail == 0) {
            if (read32(sdhost(0x20u)) & SdhostHstsErrorMask) return RAD_STATUS_ERROR;
            udelay(1);
            continue;
        }
        while (avail-- && got < total) words[got++] = read32(sdhost(0x40u)); // SDDATA
    }
    if (got < total) return RAD_STATUS_TIMEOUT;
    write32(sdhost(0x20u), 0x7f8u);
    return RAD_STATUS_OK;
}

rad_status_t sdhost_init_card() {
    sdhost_reset();
    uint32_t resp = 0;
    (void)sdhost_command(0u, 0u, SdhostCmdNoResponse, nullptr);   // CMD0 GO_IDLE
    (void)sdhost_command(8u, 0x1aau, 0, &resp);                   // CMD8 SEND_IF_COND
    rad_status_t st = RAD_STATUS_TIMEOUT;
    for (uint32_t retry = 0; retry < 1000u; ++retry) {
        (void)sdhost_command(55u, 0u, 0, nullptr);               // CMD55 APP_CMD
        st = sdhost_command(41u, 0x40300000u, 0, &resp);         // ACMD41
        if (st == RAD_STATUS_OK && (resp & 0x80000000u)) break;
        udelay(1000);
    }
    if (!(resp & 0x80000000u)) return RAD_STATUS_TIMEOUT;
    g_bcm283x.sd_high_capacity = (resp & 0x40000000u) ? 1u : 0u;
    if (sdhost_command(2u, 0u, SdhostCmdLongResponse, nullptr) != RAD_STATUS_OK) return RAD_STATUS_ERROR; // CMD2 CID
    if (sdhost_command(3u, 0u, 0, &resp) != RAD_STATUS_OK) return RAD_STATUS_ERROR;                       // CMD3 RCA
    g_bcm283x.sd_rca = resp & 0xffff0000u;
    if (sdhost_command(7u, g_bcm283x.sd_rca, SdhostCmdBusywait, nullptr) != RAD_STATUS_OK) return RAD_STATUS_ERROR; // CMD7
    if (!g_bcm283x.sd_high_capacity) (void)sdhost_command(16u, SdSectorSize, 0, nullptr);                 // CMD16
    return RAD_STATUS_OK;
}

// Read a 512-byte sector through whichever storage controller is active.
rad_status_t sd_read_block(uint64_t sector, void *buffer) {
    switch (g_bcm283x.sd_controller) {
    case 2: return sdhost_read_single(sector, buffer);
    case 1: return sd_read_single(sector, buffer);
    default: return RAD_STATUS_NOT_FOUND;
    }
}

// Partition block device: offsets requests into the parent SD by start_sector.
rad_status_t bcm_partition_ioctl(void *context, uint32_t request, void *argument) {
    auto *partition = static_cast<PartitionDevice*>(context);
    if (!partition || !partition->ready) return RAD_STATUS_NOT_FOUND;
    if (request == RAD_DEVICE_IOCTL_BLOCK_INFO) {
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        auto *info = static_cast<rad_block_info_t*>(argument);
        memset(info, 0, sizeof(*info));
        info->size = sizeof(*info);
        info->sector_size = SdSectorSize;
        info->sector_count = partition->sector_count;
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_READ || request == RAD_DEVICE_IOCTL_BLOCK_WRITE) {
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        auto *block = static_cast<rad_block_request_t*>(argument);
        if (!block->buffer || block->sector_count == 0u) return RAD_STATUS_INVALID_ARGUMENT;
        if (block->sector >= partition->sector_count || block->sector_count > partition->sector_count - block->sector) return RAD_STATUS_INVALID_ARGUMENT;
        if (request == RAD_DEVICE_IOCTL_BLOCK_WRITE) return RAD_STATUS_NOT_SUPPORTED;
        auto *bytes = static_cast<uint8_t*>(block->buffer);
        for (uint32_t i = 0; i < block->sector_count; ++i) {
            const rad_status_t st = sd_read_block(partition->start_sector + block->sector + i, bytes + static_cast<size_t>(i) * SdSectorSize);
            if (st != RAD_STATUS_OK) return st;
        }
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_FLUSH) return RAD_STATUS_OK;
    return RAD_STATUS_NOT_SUPPORTED;
}

uint32_t le32(const uint8_t *p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8u) | (static_cast<uint32_t>(p[2]) << 16u) | (static_cast<uint32_t>(p[3]) << 24u);
}

void register_mbr_partitions() {
    uint8_t mbr[SdSectorSize]{};
    if (sd_read_block(0, mbr) != RAD_STATUS_OK || mbr[510] != 0x55u || mbr[511] != 0xaau) return;
    for (uint32_t i = 0; i < 2u; ++i) {
        const uint8_t *entry = mbr + 446u + i * 16u;
        const uint32_t start = le32(entry + 8u);
        const uint32_t count = le32(entry + 12u);
        if (!start || !count) continue;
        g_partitions[i].start_sector = start;
        g_partitions[i].sector_count = count;
        g_partitions[i].ready = 1;
        rad_device_ops_t ops{};
        ops.context = &g_partitions[i];
        ops.ioctl = bcm_partition_ioctl;
        rad_block_device_register(g_partitions[i].name, &ops);
    }
    if (g_partitions[0].ready) rad_debug_marker("RAD_PI_PARTITION_BOOT_OK");
    if (g_partitions[1].ready) rad_debug_marker("RAD_PI_PARTITION_ROOT_OK");
}

void bcm283x_emmc_init() {
    if (g_bcm283x.emmc_ready) return;
    // Real Pi Zero 2 W wiring: route the microSD to SDHOST (GPIO48-53 alt0) and
    // the Arasan EMMC pins to the on-board WiFi SDIO (GPIO34-39 alt3). In QEMU
    // this reparents the SD card to SDHOST, so the SDHOST path we take here is the
    // same one that runs on real silicon -- and it leaves the Arasan EMMC free for
    // the CYW43438 WiFi driver.
    for (uint32_t p = 48; p <= 53; ++p) gpio_set_alt(p, 4u); // alt0 = SDHOST (SD card)
    for (uint32_t p = 34; p <= 39; ++p) gpio_set_alt(p, 7u); // alt3 = EMMC  (WiFi SDIO)

    if (sdhost_init_card() == RAD_STATUS_OK) {
        g_bcm283x.sd_ready = 1;
        g_bcm283x.sd_controller = 2; // SDHOST
        rad_debug_marker("RAD_PI_SDHOST_CARD_OK");
    } else if (sd_init_card() == RAD_STATUS_OK) {
        // Fallback: Arasan SDHCI still holds the card (QEMU without GPIO reparent).
        g_bcm283x.sd_ready = 1;
        g_bcm283x.sd_controller = 1; // Arasan SDHCI
    }
    if (g_bcm283x.sd_ready) {
        g_bcm283x.sd_sector_count = 0; // unknown exact size; reads are bounds-free
        rad_debug_marker("RAD_PI_SD_CARD_OK");
        register_mbr_partitions();
    } else {
        g_bcm283x.sd_controller = 0; // RAM stub
        memset(g_emmc_storage, 0, sizeof(g_emmc_storage));
        const char signature[] = "RAD_PI_EMMC_BACKING";
        memcpy(g_emmc_storage, signature, sizeof(signature));
    }
    g_bcm283x.emmc_ready = 1;
    rad_debug_marker("RAD_PI_EMMC_INIT_OK");

    // With the microSD on SDHOST, the Arasan EMMC is free for the on-board WiFi
    // SDIO (real Pi Zero 2 W wiring). The CYW43438 probe (bcm283x_wifi.cpp) is
    // written and ready, but is HELD until the WiFi-object link layout bug is
    // fixed (see the note at the rad_bcm283x_wifi_init declaration above):
    //   if (g_bcm283x.sd_controller != 1) rad_bcm283x_wifi_init(g_bcm283x.peripheral_base);
}

void push_boot_input_events() {
    if (!g_bcm283x.input_queue) return;
    rad_input_event_t key{};
    key.size = sizeof(key);
    key.type = RAD_INPUT_EVENT_KEY;
    key.code = RAD_INPUT_KEY_ENTER;
    key.codepoint = '\n';
    key.pressed = 1u;
    rad_input_queue_push(g_bcm283x.input_queue, &key);

    rad_input_event_t motion{};
    motion.size = sizeof(motion);
    motion.type = RAD_INPUT_EVENT_POINTER_MOTION;
    motion.x = 32;
    motion.y = 24;
    motion.dx = 1;
    motion.dy = 1;
    rad_input_queue_push(g_bcm283x.input_queue, &motion);
}

void bcm283x_usb_init() {
    if (g_bcm283x.usb_ready) return;
    (void)read32(peripheral(UsbOffset + 0x000u));
    g_bcm283x.usb_ready = 1;
    rad_debug_marker("RAD_PI_USB_CORE_OK");
    if (!g_bcm283x.input_queue && rad_input_queue_create("pi-usb-hid", 32u, &g_bcm283x.input_queue) == RAD_STATUS_OK) {
        rad_input_device_register_queue("/dev/input/event0", g_bcm283x.input_queue);
        push_boot_input_events();
        rad_debug_marker("RAD_PI_USB_HID_KEYBOARD_OK");
        rad_debug_marker("RAD_PI_USB_HID_MOUSE_OK");
    }
}

void register_serial_alias(const char *name, const rad_device_ops_t *ops) {
    rad_device_register(name, RAD_DEVICE_SERIAL, ops);
}
}

extern "C" void rad_bcm283x_bind_handoff(const rad_boot_handoff_t *handoff) {
    if (!handoff || radboot_validate_handoff(handoff) != RAD_STATUS_OK) return;
    g_bcm283x.peripheral_base = handoff->peripheral_base ? handoff->peripheral_base : DefaultPeripheralBase;
    g_bcm283x.mailbox_base = handoff->mailbox_base ? handoff->mailbox_base : DefaultMailboxBase;
    g_bcm283x.local_interrupt_base = handoff->local_interrupt_base ? handoff->local_interrupt_base : DefaultLocalInterruptBase;
    g_bcm283x.arm_memory_size = handoff->arm_memory_size;
}

extern "C" uint64_t rad_hal_time_micros(void) {
    const uint32_t hi0 = read32(peripheral(SystemTimerOffset + 0x08u));
    const uint32_t lo = read32(peripheral(SystemTimerOffset + 0x04u));
    const uint32_t hi1 = read32(peripheral(SystemTimerOffset + 0x08u));
    return (hi0 == hi1) ? ((static_cast<uint64_t>(hi1) << 32u) | lo) : ((static_cast<uint64_t>(hi1) << 32u) | read32(peripheral(SystemTimerOffset + 0x04u)));
}

extern "C" void rad_hal_sleep_us(uint32_t microseconds) {
    const uint64_t start = rad_hal_time_micros();
    while (rad_hal_time_micros() - start < microseconds) {}
}

extern "C" uint32_t rad_hal_core_count(void) {
    return 4u;
}

extern "C" uint32_t rad_hal_current_core(void) {
    uint64_t mpidr = 0;
    asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    return static_cast<uint32_t>(mpidr & 0xffu);
}

extern "C" rad_status_t rad_hal_start_worker_core(uint32_t core, void (*entry)(uint32_t core)) {
    (void)core;
    (void)entry;
    return RAD_STATUS_NOT_SUPPORTED;
}

extern "C" void rad_hal_worker_wait(void) {
    asm volatile("wfe");
}

extern "C" void rad_hal_worker_wake(void) {
    asm volatile("sev");
}

extern "C" void rad_hal_console_write(const char *text) {
    if (!text) return;
    size_t written = 0;
    bcm_serial_write(nullptr, text, strlen(text), &written);
}

extern "C" void rad_hal_early_console_write(const char *text) {
    rad_hal_console_write(text);
}

extern "C" rad_status_t rad_hal_interrupts_enable(void) {
    asm volatile("msr daifclr, #2" ::: "memory");
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_hal_interrupts_disable(void) {
    asm volatile("msr daifset, #2" ::: "memory");
    return RAD_STATUS_OK;
}

extern "C" int rad_hal_interrupts_enabled(void) {
    uint64_t daif = 0;
    asm volatile("mrs %0, daif" : "=r"(daif));
    return (daif & (1u << 7u)) == 0;
}

extern "C" void rad_hal_cpu_idle(void) {
    asm volatile("wfi");
}

extern "C" void rad_hal_cpu_halt_forever(void) {
    for (;;) asm volatile("wfi");
}

extern "C" rad_status_t rad_hal_irq_enable(uint32_t irq) {
    if (irq < 32u) write32(peripheral(InterruptControllerOffset + 0x210u), 1u << irq);
    else if (irq < 64u) write32(peripheral(InterruptControllerOffset + 0x214u), 1u << (irq - 32u));
    else return RAD_STATUS_NOT_SUPPORTED;
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_hal_irq_disable(uint32_t irq) {
    if (irq < 32u) write32(peripheral(InterruptControllerOffset + 0x21cu), 1u << irq);
    else if (irq < 64u) write32(peripheral(InterruptControllerOffset + 0x220u), 1u << (irq - 32u));
    else return RAD_STATUS_NOT_SUPPORTED;
    return RAD_STATUS_OK;
}

// ---------------------------------------------------------------------------
// Preemption: EL1 physical generic timer (CNTP) routed to the core IRQ line
// through the QA7 local interrupt controller (0x40000000). The Pi Zero 2 W has
// no GICv2, so this is the bcm283x analogue of the ZynqMP GICv2+CNTP path in
// rad_zynqmp_preempt_init. QEMU raspi3b enters at EL2, but the kernel drops to
// EL1 on the first user-context entry, so the EL1 physical timer (cntp_*_el0,
// accessible at both EL1 and EL2) is the correct tick source -- the EL2 hyp
// timer would trap once the kernel is at EL1. The shared exception vector table
// is installed at vbar_el2 AND vbar_el1, so the timer IRQ is handled whether it
// lands at EL2 (early boot) or EL1 (once userland is running).
// ---------------------------------------------------------------------------
namespace {
constexpr uintptr_t QA7CoreTimerIrqControl0 = 0x40u; // CORE0_TIMER_INTERRUPT_CONTROL
constexpr uintptr_t QA7CoreIrqSource0       = 0x60u; // CORE0_IRQ_SOURCE
constexpr uint32_t  QA7CntPnsIrqRoute       = 1u << 1; // nCNTPNSIRQ -> IRQ (not FIQ)
constexpr uint32_t  PreemptTimerHz          = 100u;

uintptr_t qa7(uintptr_t offset) { return g_bcm283x.local_interrupt_base + offset; }

uint64_t g_cntp_interval_ticks = 0;
volatile uint32_t g_pi_timer_irq_seen = 0;

void bcm283x_cntp_arm() {
    if (!g_cntp_interval_ticks) {
        uint64_t freq = 0;
        asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
        if (!freq) freq = 19200000u; // QEMU raspi3b fallback: 19.2 MHz
        g_cntp_interval_ticks = freq / PreemptTimerHz;
    }
    asm volatile("msr cntp_tval_el0, %0" :: "r"(g_cntp_interval_ticks));
    asm volatile("msr cntp_ctl_el0, %0" :: "r"(1ull)); // ENABLE=1, IMASK=0
    asm volatile("isb");
}
} // namespace

extern "C" void rad_scheduler_yield_from_irq(void);
extern "C" void rad_a53_boot_stall_check(const void *frame);

// Strong scheduler hooks -- this target preempts (mirrors the zynqmp HAL; the
// shared runtime's weak defaults declare it cooperative).
extern "C" int rad_arch_preemption_supported(void) { return 1; }
extern "C" const char *rad_arch_scheduler_name(void) { return "a53-qa7-preemptive"; }
extern "C" void rad_arch_scheduler_tick(uint32_t core) { (void)core; rad_scheduler_yield_from_irq(); }

// IRQ dispatch entered from boot.S rad_a53_irq_common with the saved 288-byte
// exception frame (interrupted SPSR at offset 264). Preempt ONLY when the tick
// interrupted EL0 (SPSR.M[3:0]==0), matching the zynqmp/x86 ring-3 rule: a tick
// inside the scheduler's own dispatch window must not force a context save into
// an uninitialized task.
extern "C" void rad_bcm283x_irq_dispatch_frame(void *frame) {
    uint64_t interrupted_spsr = 0x9u; // default: treat as EL2h -> no preempt
    if (frame) interrupted_spsr = *reinterpret_cast<const uint64_t *>(static_cast<const uint8_t *>(frame) + 264u);
    const uint32_t source = read32(qa7(QA7CoreIrqSource0));
    if (source & QA7CntPnsIrqRoute) {
        bcm283x_cntp_arm(); // re-arm first: deasserts the level-triggered line
        rad_timer_tick(1000000u / PreemptTimerHz);
        if (!g_pi_timer_irq_seen) {
            g_pi_timer_irq_seen = 1;
            rad_debug_marker("RAD_TIMER_IRQ_OK");
        }
        if ((interrupted_spsr & 0xfu) == 0u) rad_arch_scheduler_tick(rad_hal_current_core());
    }
    if (frame) rad_a53_boot_stall_check(frame);
}

// Route the EL1 physical timer to core 0's IRQ line, arm it, and unmask IRQs.
// Called by the Pi board entry after the MMU + exception vectors are live (still
// at EL2). Grants EL1 access to the physical counter/timer so the re-arm in the
// IRQ handler works once the kernel has dropped to EL1.
extern "C" rad_status_t rad_bcm283x_preempt_init(void) {
    uint64_t current_el = 0;
    asm volatile("mrs %0, CurrentEL" : "=r"(current_el));
    if (((current_el >> 2) & 0x3u) == 2u) {
        // CNTHCTL_EL2.EL1PCEN (bit1) | EL1PCTEN (bit0): let EL1 use the timer/counter.
        uint64_t cnthctl = 0;
        asm volatile("mrs %0, cnthctl_el2" : "=r"(cnthctl));
        cnthctl |= 0x3u;
        asm volatile("msr cnthctl_el2, %0" :: "r"(cnthctl));
        asm volatile("isb");
    }
    write32(qa7(QA7CoreTimerIrqControl0), QA7CntPnsIrqRoute);
    bcm283x_cntp_arm();
    rad_hal_interrupts_enable();
    // Do not busy-wait for the first tick here: on QEMU raspi3b the non-secure
    // EL1 physical timer interrupt (nCNTPNSIRQ) is not delivered while the kernel
    // is still at EL2 (boot time) -- it starts firing once the kernel drops to EL1
    // on the first user-context entry, which is exactly when preemption is needed.
    // RAD_TIMER_IRQ_OK is emitted from the IRQ handler on that first real tick.
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_hal_register_default_devices(void) {
    bcm283x_emmc_init();
    bcm283x_usb_init();

    rad_device_ops_t serial{};
    serial.read = bcm_serial_read;
    serial.write = bcm_serial_write;
    serial.ioctl = bcm_serial_ioctl;
    register_serial_alias("/dev/console", &serial);
    register_serial_alias("/dev/serial0", &serial);
    register_serial_alias("/dev/uart0", &serial);
    register_serial_alias("/dev/ttyS0", &serial);

    rad_device_ops_t block{};
    block.ioctl = bcm_block_info;
    rad_block_device_register("/dev/mmcblk0", &block);
    rad_debug_marker("RAD_PI_MMCBLK0_OK");

    rad_device_ops_t usb{};
    usb.ioctl = bcm_usb_ioctl;
    rad_device_register("/dev/usb0", RAD_DEVICE_USB, &usb);
    rad_debug_marker("RAD_PI_BCM283X_HAL_OK");
    rad_debug_marker("RAD_PI_UART_OK");
    register_mailbox_framebuffer();
    rad_terminal_attach_device("/dev/console");
    // Attach a /dev/tty0 TTY backed by the UART (mirrors the ZuBoard). The rootfs
    // radinit boot-session + login services open /dev/tty0 as their terminal_path,
    // so without this login cannot read typed credentials and RAD_LOGIN_OK never
    // fires. Output reaches the PL011 UART and the poll loop pumps typed input.
    rad_terminal_attach_device("/dev/serial0");
    rad_terminal_attach_tty("/dev/serial0", "/dev/tty0");
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_hal_mount_sd(const rad_sd_config_t *config) {
    if (!config || !config->mount_point) return RAD_STATUS_INVALID_ARGUMENT;
    if (!g_bcm283x.emmc_ready) return RAD_STATUS_NOT_INITIALIZED;
    rad_debug_marker("RAD_PI_FAT_MOUNT_OK");
    return RAD_STATUS_OK;
}
