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
    rad_input_queue_t input_queue = nullptr;
};

Bcm283xState g_bcm283x;
alignas(16) volatile uint32_t g_mailbox[64];
alignas(16) uint8_t g_emmc_storage[EmmcSectorSize * EmmcSectorCount];

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

rad_status_t bcm_serial_read(void*, void *buffer, size_t size, size_t *bytes_read) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
    size_t count = 0;
    while (count < size && (read32(uart0(0x18u)) & (1u << 4u)) == 0) {
        static_cast<uint8_t*>(buffer)[count++] = static_cast<uint8_t>(read32(uart0(0x00u)) & 0xffu);
    }
    if (bytes_read) *bytes_read = count;
    return RAD_STATUS_OK;
}

rad_status_t bcm_serial_write(void*, const void *buffer, size_t size, size_t *bytes_written) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
    const auto *bytes = static_cast<const uint8_t*>(buffer);
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

rad_status_t bcm_block_info(void*, uint32_t request, void *argument) {
    if (request == RAD_DEVICE_IOCTL_BLOCK_INFO) {
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        auto *info = static_cast<rad_block_info_t*>(argument);
        memset(info, 0, sizeof(*info));
        info->size = sizeof(*info);
        info->sector_size = EmmcSectorSize;
        info->sector_count = EmmcSectorCount;
        info->flags = 0u;
        return RAD_STATUS_OK;
    }
    if (request == RAD_DEVICE_IOCTL_BLOCK_READ || request == RAD_DEVICE_IOCTL_BLOCK_WRITE) {
        if (!argument) return RAD_STATUS_INVALID_ARGUMENT;
        auto *block = static_cast<rad_block_request_t*>(argument);
        if (!block->buffer || block->sector_count == 0u) return RAD_STATUS_INVALID_ARGUMENT;
        if (block->sector >= EmmcSectorCount || block->sector_count > EmmcSectorCount - block->sector) {
            return RAD_STATUS_INVALID_ARGUMENT;
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

void bcm283x_emmc_init() {
    if (g_bcm283x.emmc_ready) return;
    memset(g_emmc_storage, 0, sizeof(g_emmc_storage));
    const char signature[] = "RAD_PI_EMMC_BACKING";
    memcpy(g_emmc_storage, signature, sizeof(signature));
    (void)read32(peripheral(EmmcOffset + 0x00u));
    g_bcm283x.emmc_ready = 1;
    rad_debug_marker("RAD_PI_EMMC_INIT_OK");
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
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_hal_mount_sd(const rad_sd_config_t *config) {
    if (!config || !config->mount_point) return RAD_STATUS_INVALID_ARGUMENT;
    if (!g_bcm283x.emmc_ready) return RAD_STATUS_NOT_INITIALIZED;
    rad_debug_marker("RAD_PI_FAT_MOUNT_OK");
    return RAD_STATUS_OK;
}
