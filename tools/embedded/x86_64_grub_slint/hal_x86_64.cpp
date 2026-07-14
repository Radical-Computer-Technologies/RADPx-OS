#include <radixkernel/radixkernel.h>

#include "x86_cpu.h"

#include <stddef.h>
#include <stdint.h>

extern "C" void x86_serial_write(const char *text);
extern "C" void rad_arch_scheduler_tick(uint32_t core);

namespace {
constexpr uint16_t Com1 = 0x3f8;
constexpr uint16_t PitChannel0 = 0x40;
constexpr uint16_t PitCommand = 0x43;
constexpr uint16_t Ps2Data = 0x60;
constexpr uint16_t Ps2StatusCommand = 0x64;
constexpr const char *PicInterruptControllerPath = "/soc/interrupt-controller@20";
volatile uint64_t g_ticks = 0;
int g_timer_started = 0;
int g_interrupts_enabled = 0;
int g_timer_irq_seen = 0;
int g_idle_hlt_seen = 0;
int g_interrupts_enable_marker_seen = 0;
uint32_t g_keyboard_modifiers = 0;
int g_keyboard_extended = 0;
uint8_t g_mouse_packet[3];
uint8_t g_mouse_packet_index = 0;
int32_t g_mouse_x = 512;
int32_t g_mouse_y = 384;
int32_t g_mouse_max_x = 1023;
int32_t g_mouse_max_y = 767;
uint32_t g_mouse_buttons = 0;
int g_keyboard_irq_seen = 0;
int g_mouse_irq_seen = 0;
int g_mouse_motion_seen = 0;
int g_ps2_drain_active = 0;
volatile int g_serial_write_lock = 0;
rad_input_queue_t g_keyboard_queue = nullptr;
rad_input_queue_t g_mouse_queue = nullptr;

void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t value = 0;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

int serial_tx_ready() {
    return inb(Com1 + 5) & 0x20;
}

int serial_rx_ready() {
    return inb(Com1 + 5) & 0x01;
}

void serial_put(char ch) {
    while (!serial_tx_ready()) {}
    outb(Com1, static_cast<uint8_t>(ch));
}

void serial_lock(void) {
    while (__atomic_test_and_set(&g_serial_write_lock, __ATOMIC_ACQUIRE)) asm volatile("pause");
}

void serial_unlock(void) {
    __atomic_clear(&g_serial_write_lock, __ATOMIC_RELEASE);
}

void program_pit_1000hz(void) {
    constexpr uint32_t PitBaseHz = 1193182u;
    constexpr uint16_t Divisor = static_cast<uint16_t>(PitBaseHz / 1000u);
    outb(PitCommand, 0x36);
    outb(PitChannel0, static_cast<uint8_t>(Divisor & 0xffu));
    outb(PitChannel0, static_cast<uint8_t>((Divisor >> 8) & 0xffu));
    g_timer_started = 1;
}

void copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    size_t i = 0;
    for (; i + 1u < dst_size && src[i]; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

rad_status_t serial_read(void*, void *buffer, size_t size, size_t *bytes_read) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
    size_t count = 0;
    auto *bytes = static_cast<char*>(buffer);
    while (count < size && serial_rx_ready()) {
        bytes[count++] = static_cast<char>(inb(Com1));
    }
    if (bytes_read) *bytes_read = count;
    return RAD_STATUS_OK;
}

rad_status_t serial_write(void*, const void *buffer, size_t size, size_t *bytes_written) {
    if (!buffer) return RAD_STATUS_INVALID_ARGUMENT;
    const auto *bytes = static_cast<const char*>(buffer);
    serial_lock();
    for (size_t i = 0; i < size; ++i) {
        if (bytes[i] == '\n') serial_put('\r');
        serial_put(bytes[i]);
    }
    serial_unlock();
    if (bytes_written) *bytes_written = size;
    return RAD_STATUS_OK;
}

rad_status_t serial_ioctl(void*, uint32_t request, void *argument) {
    if (request != RAD_DEVICE_IOCTL_SERIAL_CONFIGURE) return RAD_STATUS_NOT_SUPPORTED;
    return argument ? RAD_STATUS_OK : RAD_STATUS_INVALID_ARGUMENT;
}

uint32_t keyboard_modifiers(void) {
    return g_keyboard_modifiers;
}

void set_modifier(uint32_t modifier, int enabled) {
    if (enabled) {
        g_keyboard_modifiers |= modifier;
    } else {
        g_keyboard_modifiers &= ~modifier;
    }
}

int ps2_output_ready(void) {
    return inb(Ps2StatusCommand) & 0x01;
}

int ps2_input_ready(void) {
    return inb(Ps2StatusCommand) & 0x02;
}

int ps2_aux_output_ready(void) {
    return (inb(Ps2StatusCommand) & 0x21) == 0x21;
}

void ps2_wait_input_clear(void) {
    for (uint32_t i = 0; i < 100000u && ps2_input_ready(); ++i) asm volatile("pause");
}

void ps2_write_command(uint8_t value) {
    ps2_wait_input_clear();
    outb(Ps2StatusCommand, value);
}

void ps2_write_data(uint8_t value) {
    ps2_wait_input_clear();
    outb(Ps2Data, value);
}

void ps2_write_mouse(uint8_t value) {
    ps2_write_command(0xd4);
    ps2_write_data(value);
}

char scancode_ascii(uint8_t code, uint32_t modifiers) {
    const int shift = (modifiers & RAD_INPUT_MOD_SHIFT) != 0;
    const int caps = (modifiers & RAD_INPUT_MOD_CAPS_LOCK) != 0;
    if ((code >= 0x10 && code <= 0x19) || (code >= 0x1e && code <= 0x26) || (code >= 0x2c && code <= 0x32)) {
        static constexpr char letters[0x33] = {
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', 0, 0, 0, 0, 'a', 's',
            'd', 'f', 'g', 'h', 'j', 'k', 'l', 0, 0, 0, 0, 0, 'z', 'x', 'c', 'v',
            'b', 'n', 'm'
        };
        char ch = letters[code];
        if (ch && (shift ^ caps)) ch = static_cast<char>(ch - 'a' + 'A');
        return ch;
    }

    switch (code) {
    case 0x02: return shift ? '!' : '1';
    case 0x03: return shift ? '@' : '2';
    case 0x04: return shift ? '#' : '3';
    case 0x05: return shift ? '$' : '4';
    case 0x06: return shift ? '%' : '5';
    case 0x07: return shift ? '^' : '6';
    case 0x08: return shift ? '&' : '7';
    case 0x09: return shift ? '*' : '8';
    case 0x0a: return shift ? '(' : '9';
    case 0x0b: return shift ? ')' : '0';
    case 0x0c: return shift ? '_' : '-';
    case 0x0d: return shift ? '+' : '=';
    case 0x1a: return shift ? '{' : '[';
    case 0x1b: return shift ? '}' : ']';
    case 0x27: return shift ? ':' : ';';
    case 0x28: return shift ? '"' : '\'';
    case 0x29: return shift ? '~' : '`';
    case 0x2b: return shift ? '|' : '\\';
    case 0x33: return shift ? '<' : ',';
    case 0x34: return shift ? '>' : '.';
    case 0x35: return shift ? '?' : '/';
    case 0x39: return ' ';
    default: return 0;
    }
}

rad_input_key_t scancode_key(uint8_t code, int extended) {
    if (extended) {
        switch (code) {
        case 0x1c: return RAD_INPUT_KEY_ENTER;
        case 0x1d: return RAD_INPUT_KEY_RIGHT_CTRL;
        case 0x38: return RAD_INPUT_KEY_RIGHT_ALT;
        case 0x47: return RAD_INPUT_KEY_HOME;
        case 0x48: return RAD_INPUT_KEY_UP;
        case 0x49: return RAD_INPUT_KEY_PAGE_UP;
        case 0x4b: return RAD_INPUT_KEY_LEFT;
        case 0x4d: return RAD_INPUT_KEY_RIGHT;
        case 0x4f: return RAD_INPUT_KEY_END;
        case 0x50: return RAD_INPUT_KEY_DOWN;
        case 0x51: return RAD_INPUT_KEY_PAGE_DOWN;
        case 0x52: return RAD_INPUT_KEY_INSERT;
        case 0x53: return RAD_INPUT_KEY_DELETE;
        default: return RAD_INPUT_KEY_UNKNOWN;
        }
    }

    switch (code) {
    case 0x01: return RAD_INPUT_KEY_ESCAPE;
    case 0x0e: return RAD_INPUT_KEY_BACKSPACE;
    case 0x0f: return RAD_INPUT_KEY_TAB;
    case 0x1c: return RAD_INPUT_KEY_ENTER;
    case 0x1d: return RAD_INPUT_KEY_LEFT_CTRL;
    case 0x2a: return RAD_INPUT_KEY_LEFT_SHIFT;
    case 0x36: return RAD_INPUT_KEY_RIGHT_SHIFT;
    case 0x38: return RAD_INPUT_KEY_LEFT_ALT;
    case 0x3a: return RAD_INPUT_KEY_CAPS_LOCK;
    default: return RAD_INPUT_KEY_UNKNOWN;
    }
}

int keyboard_decode(uint8_t raw, rad_input_event_t *event) {
    if (raw == 0xe0) {
        g_keyboard_extended = 1;
        return 0;
    }
    if (raw == 0xfa || raw == 0xfe) return 0;

    const int extended = g_keyboard_extended;
    g_keyboard_extended = 0;
    const int pressed = (raw & 0x80u) == 0;
    const uint8_t code = raw & 0x7fu;
    const rad_input_key_t key = scancode_key(code, extended);

    if (key == RAD_INPUT_KEY_LEFT_SHIFT || key == RAD_INPUT_KEY_RIGHT_SHIFT) {
        set_modifier(RAD_INPUT_MOD_SHIFT, pressed);
    } else if (key == RAD_INPUT_KEY_LEFT_CTRL || key == RAD_INPUT_KEY_RIGHT_CTRL) {
        set_modifier(RAD_INPUT_MOD_CTRL, pressed);
    } else if (key == RAD_INPUT_KEY_LEFT_ALT || key == RAD_INPUT_KEY_RIGHT_ALT) {
        set_modifier(RAD_INPUT_MOD_ALT, pressed);
    } else if (key == RAD_INPUT_KEY_CAPS_LOCK && pressed) {
        g_keyboard_modifiers ^= RAD_INPUT_MOD_CAPS_LOCK;
    }

    if (!event) return 0;
    event->size = sizeof(*event);
    event->type = RAD_INPUT_EVENT_KEY;
    event->code = static_cast<uint32_t>(key);
    event->codepoint = pressed && !extended ? static_cast<uint32_t>(scancode_ascii(code, keyboard_modifiers())) : 0u;
    event->modifiers = keyboard_modifiers();
    event->pressed = pressed ? 1u : 0u;
    event->repeat = 0u;
    event->reserved = 0u;
    if (key == RAD_INPUT_KEY_UNKNOWN && event->codepoint == 0) return 0;
    return 1;
}

void keyboard_drain_port(void) {
    while (ps2_output_ready() && !ps2_aux_output_ready()) {
        rad_input_event_t event{};
        if (keyboard_decode(inb(Ps2Data), &event)) {
            if (g_keyboard_queue) rad_input_queue_push(g_keyboard_queue, &event);
            rad_perf_counter_add("input.irq", 1);
            if (!g_keyboard_irq_seen) {
                g_keyboard_irq_seen = 1;
                rad_debug_marker("RADIX_IRQ_KEYBOARD_OK");
                rad_debug_marker("RADIX_INPUT_WAKE_OK");
            }
        }
    }
}

void keyboard_flush_pending(void) {
    for (uint32_t i = 0; i < 16u && ps2_output_ready(); ++i) {
        (void)inb(Ps2Data);
    }
}

int clamp_mouse_coord(int32_t value, int32_t max) {
    if (value < 0) return 0;
    if (value > max) return max;
    return value;
}

void mouse_enqueue_packet_events(void) {
    const uint8_t flags = g_mouse_packet[0];
    int32_t dx = static_cast<int8_t>(g_mouse_packet[1]);
    int32_t dy = static_cast<int8_t>(g_mouse_packet[2]);
    if (flags & 0x40u) dx = 0;
    if (flags & 0x80u) dy = 0;
    g_mouse_x = clamp_mouse_coord(g_mouse_x + dx, g_mouse_max_x);
    g_mouse_y = clamp_mouse_coord(g_mouse_y - dy, g_mouse_max_y);
    const uint32_t buttons =
        ((flags & 0x01u) ? static_cast<uint32_t>(RAD_INPUT_POINTER_BUTTON_LEFT) : 0u)
        | ((flags & 0x02u) ? static_cast<uint32_t>(RAD_INPUT_POINTER_BUTTON_RIGHT) : 0u)
        | ((flags & 0x04u) ? static_cast<uint32_t>(RAD_INPUT_POINTER_BUTTON_MIDDLE) : 0u);
    const uint32_t changed = buttons ^ g_mouse_buttons;

    if (dx != 0 || dy != 0) {
        rad_input_event_t motion{};
        motion.size = sizeof(motion);
        motion.type = RAD_INPUT_EVENT_POINTER_MOTION;
        motion.x = g_mouse_x;
        motion.y = g_mouse_y;
        motion.dx = dx;
        motion.dy = -dy;
        motion.buttons = buttons;
        if (g_mouse_queue) rad_input_queue_push(g_mouse_queue, &motion);
        rad_perf_counter_add("input.irq", 1);
        if (!g_mouse_motion_seen) {
            g_mouse_motion_seen = 1;
            rad_debug_marker("RADIX_INPUT_MOUSE_MOTION_OK");
        }
    }

    if (changed) {
        rad_input_event_t button{};
        button.size = sizeof(button);
        button.type = RAD_INPUT_EVENT_POINTER_BUTTON;
        button.x = g_mouse_x;
        button.y = g_mouse_y;
        button.buttons = buttons;
        if (changed & RAD_INPUT_POINTER_BUTTON_LEFT) button.code = RAD_INPUT_POINTER_BUTTON_LEFT;
        else if (changed & RAD_INPUT_POINTER_BUTTON_RIGHT) button.code = RAD_INPUT_POINTER_BUTTON_RIGHT;
        else button.code = RAD_INPUT_POINTER_BUTTON_MIDDLE;
        button.pressed = (buttons & button.code) ? 1u : 0u;
        if (g_mouse_queue) rad_input_queue_push(g_mouse_queue, &button);
    }
    g_mouse_buttons = buttons;
    if (!g_mouse_irq_seen) {
        g_mouse_irq_seen = 1;
        rad_debug_marker("RADIX_IRQ_MOUSE_OK");
    }
}

void mouse_accept_byte(uint8_t byte) {
    if (g_mouse_packet_index == 0 && (byte & 0x08u) == 0) return;
    g_mouse_packet[g_mouse_packet_index++] = byte;
    if (g_mouse_packet_index < 3) return;
    g_mouse_packet_index = 0;
    mouse_enqueue_packet_events();
}

void mouse_drain_port(int force_mouse_data) {
    while (ps2_output_ready()) {
        if (!force_mouse_data && !ps2_aux_output_ready()) return;
        mouse_accept_byte(inb(Ps2Data));
    }
}

void keyboard_irq_handler(uint32_t, void*) {
    if (g_ps2_drain_active) return;
    g_ps2_drain_active = 1;
    keyboard_drain_port();
    g_ps2_drain_active = 0;
}

void mouse_irq_handler(uint32_t, void*) {
    if (g_ps2_drain_active) return;
    g_ps2_drain_active = 1;
    mouse_drain_port(1);
    g_ps2_drain_active = 0;
}

void timer_irq_handler(uint32_t, void*) {
    __atomic_fetch_add(&g_ticks, 1u, __ATOMIC_RELAXED);
    rad_timer_tick(1000u);
    rad_arch_scheduler_tick(x86_cpu_current_core());
    rad_perf_counter_add("timer.irq", 1);
    if (!g_timer_irq_seen) {
        g_timer_irq_seen = 1;
        rad_debug_marker("RADIX_TIMER_IRQ_OK");
    }
}

rad_status_t pit_start_periodic(void*, uint32_t frequency_hz) {
    if (frequency_hz != 1000u) return RAD_STATUS_NOT_SUPPORTED;
    program_pit_1000hz();
    return RAD_STATUS_OK;
}

rad_irq_resource_t pic_irq_resource(uint32_t line, uint32_t flags) {
    rad_irq_resource_t resource{};
    resource.irq = line;
    resource.line = line;
    resource.flags = flags;
    resource.trigger = flags == 1u ? RAD_IRQ_TRIGGER_EDGE_RISING : RAD_IRQ_TRIGGER_NONE;
    copy_string(resource.domain, sizeof(resource.domain), "x86-pic");
    copy_string(resource.controller_path, sizeof(resource.controller_path), PicInterruptControllerPath);
    return resource;
}

int mouse_initialize(void) {
    ps2_write_command(0xa8);
    ps2_write_command(0x20);
    uint8_t status = 0;
    for (uint32_t i = 0; i < 100000u; ++i) {
        if (ps2_output_ready()) {
            status = inb(Ps2Data);
            break;
        }
    }
    status |= 0x03u;
    status &= ~0x20u;
    ps2_write_command(0x60);
    ps2_write_data(status);
    ps2_write_mouse(0xf6);
    for (uint32_t i = 0; i < 100000u && !ps2_output_ready(); ++i) asm volatile("pause");
    if (ps2_output_ready()) (void)inb(Ps2Data);
    ps2_write_mouse(0xf4);
    for (uint32_t i = 0; i < 100000u && !ps2_output_ready(); ++i) asm volatile("pause");
    if (ps2_output_ready()) (void)inb(Ps2Data);
    return 1;
}
}

extern "C" void x86_serial_init(void) {
    outb(Com1 + 1, 0x00);
    outb(Com1 + 3, 0x80);
    outb(Com1 + 0, 0x03);
    outb(Com1 + 1, 0x00);
    outb(Com1 + 3, 0x03);
    outb(Com1 + 2, 0xc7);
    outb(Com1 + 4, 0x0b);
}

extern "C" void x86_serial_write(const char *text) {
    serial_lock();
    while (text && *text) {
        if (*text == '\n') serial_put('\r');
        serial_put(*text++);
    }
    serial_unlock();
}

extern "C" void x86_ps2_set_pointer_bounds(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    g_mouse_max_x = static_cast<int32_t>(width - 1u);
    g_mouse_max_y = static_cast<int32_t>(height - 1u);
    g_mouse_x = static_cast<int32_t>(width / 2u);
    g_mouse_y = static_cast<int32_t>(height / 2u);
}

extern "C" void x86_input_pointer_absolute(int32_t raw_x, int32_t raw_y, int32_t min_x, int32_t max_x, int32_t min_y, int32_t max_y, uint32_t buttons) {
    if (max_x <= min_x) max_x = min_x + 1;
    if (max_y <= min_y) max_y = min_y + 1;
    const int32_t old_x = g_mouse_x;
    const int32_t old_y = g_mouse_y;
    const int64_t scaled_x = (static_cast<int64_t>(raw_x - min_x) * g_mouse_max_x) / (max_x - min_x);
    const int64_t scaled_y = (static_cast<int64_t>(raw_y - min_y) * g_mouse_max_y) / (max_y - min_y);
    g_mouse_x = clamp_mouse_coord(static_cast<int32_t>(scaled_x), g_mouse_max_x);
    g_mouse_y = clamp_mouse_coord(static_cast<int32_t>(scaled_y), g_mouse_max_y);
    const int32_t dx = g_mouse_x - old_x;
    const int32_t dy = g_mouse_y - old_y;
    if (dx != 0 || dy != 0) {
        rad_input_event_t motion{};
        motion.size = sizeof(motion);
        motion.type = RAD_INPUT_EVENT_POINTER_MOTION;
        motion.x = g_mouse_x;
        motion.y = g_mouse_y;
        motion.dx = dx;
        motion.dy = dy;
        motion.buttons = buttons;
        if (g_mouse_queue) rad_input_queue_push(g_mouse_queue, &motion);
        if (!g_mouse_motion_seen) {
            g_mouse_motion_seen = 1;
            rad_debug_marker("RADIX_INPUT_MOUSE_MOTION_OK");
        }
    }
}

extern "C" void x86_input_pointer_buttons(uint32_t buttons) {
    const uint32_t changed = buttons ^ g_mouse_buttons;
    if (!changed) return;
    rad_input_event_t button{};
    button.size = sizeof(button);
    button.type = RAD_INPUT_EVENT_POINTER_BUTTON;
    button.x = g_mouse_x;
    button.y = g_mouse_y;
    button.buttons = buttons;
    if (changed & RAD_INPUT_POINTER_BUTTON_LEFT) button.code = RAD_INPUT_POINTER_BUTTON_LEFT;
    else if (changed & RAD_INPUT_POINTER_BUTTON_RIGHT) button.code = RAD_INPUT_POINTER_BUTTON_RIGHT;
    else button.code = RAD_INPUT_POINTER_BUTTON_MIDDLE;
    button.pressed = (buttons & button.code) ? 1u : 0u;
    if (g_mouse_queue) rad_input_queue_push(g_mouse_queue, &button);
    g_mouse_buttons = buttons;
}

extern "C" void x86_ps2_poll_devices(void) {
    if (g_ps2_drain_active) return;
    g_ps2_drain_active = 1;
    keyboard_drain_port();
    mouse_drain_port(0);
    g_ps2_drain_active = 0;
}

extern "C" void x86_ui_idle_frame(void) {
    for (uint32_t i = 0; i < 2000u; ++i) {
        if ((i & 0x3fu) == 0) x86_ps2_poll_devices();
        asm volatile("pause");
    }
}

extern "C" void rad_hal_console_write(const char *text) {
    x86_serial_write(text);
}

extern "C" void rad_hal_early_console_write(const char *text) {
    x86_serial_write(text);
}

extern "C" uint64_t rad_hal_time_micros(void) {
    return __atomic_load_n(&g_ticks, __ATOMIC_RELAXED) * 1000u;
}

extern "C" uint32_t rad_hal_core_count(void) {
    return x86_cpu_detected_core_count();
}

extern "C" uint32_t rad_hal_current_core(void) {
    return x86_cpu_current_core();
}

extern "C" rad_status_t rad_hal_start_worker_core(uint32_t core, void (*entry)(uint32_t)) {
    return x86_cpu_start_worker_core(core, entry);
}

extern "C" int rad_arch_preemption_supported(void) {
    return 1;
}

extern "C" const char *rad_arch_scheduler_name(void) {
    return "x86_64-grub-preemptive";
}

extern "C" void rad_arch_scheduler_tick(uint32_t) {
    rad_scheduler_yield_from_irq();
}

extern "C" void rad_hal_sleep_us(uint32_t microseconds) {
    if (microseconds == 0) {
        asm volatile("pause");
        return;
    }
    if (g_timer_started && g_interrupts_enabled) {
        const uint64_t target = __atomic_load_n(&g_ticks, __ATOMIC_RELAXED)
            + ((static_cast<uint64_t>(microseconds) + 999u) / 1000u);
        while (__atomic_load_n(&g_ticks, __ATOMIC_RELAXED) < target) {
            rad_cpu_idle();
        }
        return;
    }
    const uint32_t loops = microseconds ? microseconds * 16u : 1u;
    for (uint32_t i = 0; i < loops; ++i) asm volatile("pause");
    __atomic_fetch_add(&g_ticks, microseconds / 1000u, __ATOMIC_RELAXED);
}

extern "C" rad_status_t rad_hal_interrupts_enable(void) {
    asm volatile("sti");
    g_interrupts_enabled = 1;
    if (!g_interrupts_enable_marker_seen) {
        g_interrupts_enable_marker_seen = 1;
        rad_debug_marker("RADIX_USER_IF_OK");
    }
    return RAD_STATUS_OK;
}

extern "C" rad_status_t rad_hal_interrupts_disable(void) {
    asm volatile("cli");
    g_interrupts_enabled = 0;
    return RAD_STATUS_OK;
}

extern "C" int rad_hal_interrupts_enabled(void) {
    uint64_t flags = 0;
    asm volatile("pushfq; popq %0" : "=r"(flags));
    return g_interrupts_enabled && ((flags & (1ull << 9)) != 0);
}

extern "C" void rad_hal_cpu_idle(void) {
    if (g_interrupts_enabled) {
        if (!g_idle_hlt_seen) {
            g_idle_hlt_seen = 1;
            rad_debug_marker("RADIX_IDLE_HLT_OK");
        }
        asm volatile("sti; hlt" ::: "memory");
    } else {
        asm volatile("pause");
    }
}

extern "C" void rad_hal_worker_wait(void) {
    for (uint32_t i = 0; i < 1000u; ++i) asm volatile("pause");
}

extern "C" void rad_hal_worker_wake(void) {
}

extern "C" void rad_hal_cpu_halt_forever(void) {
    for (;;) asm volatile("cli; hlt");
}

extern "C" rad_status_t rad_hal_register_default_devices(void) {
    rad_irq_domain_config_t pic{};
    pic.size = sizeof(pic);
    pic.name = "x86-pic";
    pic.tree_path = PicInterruptControllerPath;
    pic.interrupt_base = 0;
    pic.line_count = 16;
    pic.interrupt_cells = 2;
    const rad_status_t pic_status = rad_irq_domain_register(&pic, nullptr);
    if (pic_status == RAD_STATUS_OK || pic_status == RAD_STATUS_ALREADY_EXISTS) {
        rad_debug_marker("RADIX_IRQ_DOMAIN_PIC_OK");
        rad_irq_domain_t domain = nullptr;
        if (rad_irq_domain_find(PicInterruptControllerPath, &domain) == RAD_STATUS_OK && domain) {
            rad_debug_marker("RADIX_IRQ_TREE_OK");
        }
    }

    rad_irq_resource_t timer_irq = pic_irq_resource(0, 1);
    if (rad_irq_resource_register_handler(&timer_irq, "pit-timer", timer_irq_handler, nullptr) == RAD_STATUS_OK
        && rad_irq_resource_enable(&timer_irq) == RAD_STATUS_OK) {
        rad_timer_source_config_t timer_config{};
        timer_config.size = sizeof(timer_config);
        timer_config.name = "x86-pit";
        timer_config.frequency_hz = 1000u;
        timer_config.supports_oneshot = 0;
        rad_timer_source_ops_t timer_ops{};
        timer_ops.start_periodic = pit_start_periodic;
        if (rad_timer_source_register(&timer_config, &timer_ops) == RAD_STATUS_OK) {
            rad_printk("pit timer irq registered\n");
        }
    }

    rad_device_ops_t serial{};
    serial.read = serial_read;
    serial.write = serial_write;
    serial.ioctl = serial_ioctl;
    rad_device_register("/dev/ttyS0", RAD_DEVICE_SERIAL, &serial);

    keyboard_flush_pending();
    if (rad_input_queue_create("ps2-keyboard", 64, &g_keyboard_queue) == RAD_STATUS_OK
        && rad_input_device_register_queue("/dev/input/event0", g_keyboard_queue) == RAD_STATUS_OK) {
        rad_irq_resource_t keyboard_irq = pic_irq_resource(1, 1);
        if (rad_irq_resource_register_handler(&keyboard_irq, "ps2-keyboard", keyboard_irq_handler, nullptr) == RAD_STATUS_OK
            && rad_irq_resource_enable(&keyboard_irq) == RAD_STATUS_OK) {
            rad_printk("keyboard irq registered\n");
        }
    }

    if (mouse_initialize()) {
        if (rad_input_queue_create("ps2-mouse", 64, &g_mouse_queue) == RAD_STATUS_OK
            && rad_input_device_register_queue("/dev/input/event1", g_mouse_queue) == RAD_STATUS_OK) {
            rad_irq_resource_t mouse_irq = pic_irq_resource(12, 1);
            if (rad_irq_resource_register_handler(&mouse_irq, "ps2-mouse", mouse_irq_handler, nullptr) == RAD_STATUS_OK
                && rad_irq_resource_enable(&mouse_irq) == RAD_STATUS_OK) {
                rad_printk("mouse irq registered\n");
                rad_debug_marker("RADIX_IRQ_TREE_PS2_OK");
            }
        }
        rad_debug_marker("RADIX_INPUT_POINTER_OK");
    }
    return RAD_STATUS_OK;
}
